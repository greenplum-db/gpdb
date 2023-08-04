import base64
import pickle
import sys
from gppylib.commands.base import REMOTE, WorkerPool, Command
from gppylib.commands.unix import DiskFree, DiskUsage, MakeDirectory
from gppylib.db import dbconn
from gppylib.gplog import get_default_logger
from gppylib.operations.segment_tablespace_locations import get_tablespace_locations
from gppylib.userinput import ask_yesno
from gppylib.mainUtils import ExceptionNoStackTraceNeeded, UserAbortedException

logger = get_default_logger()


class RelocateSegmentPair:
    """
    RelocateSegmentPair maps source and target information for ease of use in
    other functions. For example, it encapsulates the information needed to
    calculate disk usage on the source host to ensure enough free space on
    the target host.

    The parameter hostaddr is the hostname or ip address of the segment.
    """
    def __init__(self, source_hostaddr, source_data_dir, target_hostaddr, target_data_dir):
        self.source_hostaddr = source_hostaddr
        self.source_data_dir = source_data_dir
        self.target_hostaddr = target_hostaddr
        self.target_data_dir = target_data_dir


class RelocateDiskUsage:
    """
    RelocateDiskUsage validates if there is enough disk space on the target host
    for the source data directories and tablespaces.

    The parameter pairs is a list of RelocateSegmentPair().
    """
    def __init__(self, pairs, batch_size, options):
        self.pairs = pairs  # list of RelocateSegmentPair()
        self.batch_size = batch_size
        self.options = options


        for pair in self.pairs:
            pair.source_tablespace_usage = {}  # map of tablespace_location to disk usage
            pair.source_data_dir_usage = None
            pair.dirs_created = None

    # Calculates total disk usage for source directories, and checks free disk
    # space on target host filesystems.
    def validate_disk_space(self):
        self._determine_source_disk_usage()

        for hostaddr, filesystems in self._target_host_filesystems().items():
            for fs in filesystems:
                # Check for an additional 10% free disk space so that disk may not run out of space once mirror is moved
                disk_free_space_with_buff = int ((10 * fs.disk_required ) / 100)

                if fs.disk_free <= fs.disk_required:
                    logger.error("Not enough space on host {} for directories {}." .format(hostaddr, ', '.join(map(str, fs.directories))))
                    logger.error("Filesystem {} has {} kB available, but requires {} kB." .format(fs.name, fs.disk_free, fs.disk_required))
                    return False
                elif fs.disk_free < (disk_free_space_with_buff + fs.disk_required):
                    if self.options.interactive:
                        logger.info("Target Host:{} Filesystem:{} (directory {}) has {} kB free disk space and required is {} kB."
                                    .format(hostaddr, fs.name, ', '.join(map(str, fs.directories)), fs.disk_free, fs.disk_required))
                        if not ask_yesno(None, "\nLess than 10% of disk space will be available after mirror is moved."
                                                "Continue with segment move procedure", 'N'):
                            logger.info("User Aborted. Exiting...")
                            raise UserAbortedException()

        # Required Disk space is available in target host
        return True

    def _determine_source_disk_usage(self):
        """ 
        Calculates disk usage for the data directory, and all user defined
        tablespaces for the source host.
        NOTE: The user can specify a different target data directory from
        the source. However, we do not allow them to specify different tablespace
        locations from the source to target since the primary and mirror tablespace
        locations must match.
        """
        for pair in self.pairs:
            pair.source_data_dir_usage = self._disk_usage(pair.source_hostaddr, [pair.source_data_dir])[pair.source_data_dir]
            for host, content, tablespace_dir in get_tablespace_locations(False, pair.source_data_dir):
                pair.source_tablespace_usage = self._disk_usage(pair.source_hostaddr, list(tablespace_dir.split()))


    def _disk_usage(self, hostaddr, dirs):
        """
        Fetch the Disk usage for the given set of directories to the targeted host
        input: hostaddr , host from which the disk usage is fetched
        input: dirs, list of directories to fetch the details
        output: dictionary containing directories with it's disk usage stats in kb(kilo byte)
        """
        dirs_disk_usage = {}  # map of directories to disk usage

        if len(dirs) <= 0:
            return dirs_disk_usage

        pool = WorkerPool(numWorkers=min(len(dirs), self.batch_size))
        try:
            for directory in dirs:
                cmd = DiskUsage('check source segments disk space used', directory, ctxt=REMOTE, remoteHostAddr=hostaddr)
                pool.addCommand(cmd)
            pool.join()
        finally:
            pool.haltWork()
            pool.joinWorkers()

        for cmd in pool.getCompletedItems():
            if not cmd.was_successful():
                raise Exception("Unable to check disk usage on source segment: {}" .format(cmd.get_results().stderr))

            dirs_disk_usage[cmd.directory] = cmd.kbytes_used()

        return dirs_disk_usage

    # Returns a map of hostaddr to list of target filesystems containing the
    # disk space required and disk space available.
    def _target_host_filesystems(self):
        hostaddr_to_filesystems = {}  # map of hostaddr to list of FileSystem()

        for pair in self.pairs:
            target_filesystems = self._target_filesystems(pair)
            # Check if the Target Hostaddr is present in the dictionary
            if pair.target_hostaddr not in hostaddr_to_filesystems:
                hostaddr_to_filesystems[pair.target_hostaddr] = []

            host_filesystems = hostaddr_to_filesystems[pair.target_hostaddr]

            if not host_filesystems:
                host_filesystems.extend(target_filesystems)
                continue

            # Consolidate the same target filesystems across other pairs
            for target_fs in target_filesystems:
                for host_fs in host_filesystems:
                    if target_fs.name == host_fs.name:
                        host_fs.disk_required += target_fs.disk_required
                        host_fs.directories += set(target_fs.directories)

            # add any target filesystems not already in host_filesystems
            to_add = set(target_filesystems).difference(set(host_filesystems))
            host_filesystems.extend(to_add)

        return hostaddr_to_filesystems

    # For each source/target pair find the "target" filesystems and associated
    # free disk space. Returns a list of target filesystems
    def _target_filesystems(self, pair):
        filesystems = []  # list of FileSystem()

        directories = [pair.target_data_dir] + list(pair.source_tablespace_usage.keys())
        pool = WorkerPool(numWorkers=min(len(directories), self.batch_size))
        try:
            cmd = DiskFree(pair.target_hostaddr, directories)
            pool.addCommand(cmd)
            pool.join()
        finally:
            pool.haltWork()
            pool.joinWorkers()

        for cmd in pool.getCompletedItems():
            if not cmd.was_successful():
                raise Exception("Failed to check disk free on target segment: {}" .format(cmd.get_results().stderr))

            filesystems = pickle.loads(base64.urlsafe_b64decode(cmd.get_results().stdout))
            for fs in filesystems:
                fs.add_disk_usage(pair)

        return filesystems


class FileSystem:
    """
    Filesystem related information is encapsulated in this class and is used to
    filesystem information from target host to source host.
    """
    def __init__(self, name, disk_free=0, disk_required=0, directories=None):
        self.name = name
        self.disk_free = disk_free # in kB
        self.disk_required = disk_required  # in kB
        self.directories = directories  # set of directories

    def add_disk_usage(self, pair):
        for dir in self.directories:
            if dir == pair.target_data_dir:
                self.disk_required += pair.source_data_dir_usage
            else:
                self.disk_required += pair.source_tablespace_usage[dir]


class InsufficientDiskSpaceError(Exception):
    pass

