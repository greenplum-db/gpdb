# Line too long - pylint: disable=C0301
# Copyright (c) Greenplum Inc 2011. All Rights Reserved.

import os
import errno
import shutil
import hashlib
import signal

from gppylib.db import dbconn, catalog
from gppylib.db.dbconn import UnexpectedRowsError
from gppylib import gplog
from gppylib.commands.gp import GpStop, GpStart, get_local_db_mode
from gppylib.commands.unix import Ping
from gppylib.operations import Operation
from gppylib.operations.utils import RemoteOperation, ParallelOperation
from gppylib.operations.unix import CheckFile, CheckDir, ListFiles
from gppylib.gparray import GpArray

logger = gplog.get_default_logger()

GP_TRANSACTION_FILES_FILESPACE = 'gp_transaction_files_filespace'
GP_TEMPORARY_FILES_FILESPACE = 'gp_temporary_files_filespace'
PG_SYSTEM_FILESPACE = 'pg_system'
MASTER_DATA_DIR = os.environ.get('MASTER_DATA_DIRECTORY')
NUM_WORKERS = 16  # Number of simultaneous parallel process that are started by ParallelOperation


def is_filespace_configured():
    if MASTER_DATA_DIR is None:
        raise Exception('MASTER_DATA_DIRECTORY is not set')

    if os.path.exists(os.path.join(MASTER_DATA_DIR, GP_TRANSACTION_FILES_FILESPACE)) or \
            os.path.exists(os.path.join(MASTER_DATA_DIR, GP_TEMPORARY_FILES_FILESPACE)):
        return True

    return False


class FileType:
    """
        This class is to emulate an enum
    """

    TRANSACTION_FILES, TEMPORARY_FILES = range(2)
    lookup = ['TRANSACTION_FILES', 'TEMPORARY_FILES']


class MoveFilespaceError(Exception):
    def __init__(self, msg):
        Exception.__init__(self, msg)


class CheckFilespaceIsSame(Operation):
    """
        Checks that the given filespace is different
        from the one currently being used.

        @return: True if Filespace is different False otherwise
    """

    def __init__(self, gparray, filespace_name, file_type):
        self.gparray = gparray
        self.filespace_name = filespace_name
        self.file_type = file_type

    def execute(self):
        logger.info('Checking if filespace is same as current filespace')

        try:
            if self.file_type == FileType.TRANSACTION_FILES:
                filename = GP_TRANSACTION_FILES_FILESPACE
            else:
                filename = GP_TEMPORARY_FILES_FILESPACE

            pg_system_filespace_entries = GetFilespaceEntries(self.gparray, PG_SYSTEM_FILESPACE).run()

            file_path = os.path.join(pg_system_filespace_entries[0][2], filename)

            if self.filespace_name == 'pg_system' and not CheckFile(file_path).run():
                return True

            if CheckFile(file_path).run():
                with open(file_path) as file:
                    oid = int(file.readline().strip())
                    if self.gparray.getFileSpaceName(oid) == self.filespace_name:
                        return True
        except Exception, e:
            raise MoveFilespaceError('Failed to check if filespace is same.')

        return False


class CheckFilespace(Operation):
    """
        Checks that the given filespace exists.
        
        @return: True if Filespace exists False otherwise
    """
    SELECT_FILESPACENAME_QUERY = """
        SELECT * FROM pg_filespace WHERE fsname = \'%s\'
    """

    def __init__(self, filespace):
        self.filespace = filespace

    def execute(self):
        logger.info('Checking if filespace %s exists' % self.filespace)

        dburl = dbconn.DbURL()
        query = self.SELECT_FILESPACENAME_QUERY % self.filespace

        logger.debug('Connecting to database')
        with dbconn.connect(dburl, utility=True) as conn:
            try:
                logger.debug('Executing query %s' % query)
                tuple = dbconn.execSQLForSingletonRow(conn, query)
            except UnexpectedRowsError, e:
                if e.actual == 0:
                    return False
                raise

        return True


class CheckSuperUser(Operation):
    """
        Checks if the user running the gpfilespace utility
        is a super user or not.
    """

    def __init__(self, dburl):
        self.dburl = dburl

    def execute(self):
        is_super = None
        conn = dbconn.connect(self.dburl, utility=True)
        try:
            q = "SELECT usesuper FROM pg_user WHERE usename = user"
            rows = catalog.basicSQLExec(conn, q)
            is_super = rows[0][0]
        except Exception, e:
            raise MoveFilespaceError(str(e))
        finally:
            conn.close()

        if not is_super:
            return False

        return True


class CheckConnectivity(Operation):
    """
        Check if all hosts are reachable 
        in the cluster.
    """

    def __init__(self, gparray):
        self.gparray = gparray

    def execute(self):
        logger.info('Checking for connectivity')

        host_set = self.gparray.getHostList()

        for host in host_set:
            try:
                Ping.local("pinging host %s" % host, host)
            except Exception, e:
                logger.error('Cannot connect to host %s: %s' % (host, e))
                return False

        return True


class CheckFilespaceOidLocally(Operation):
    """
        Returns the oid present in the flat files
    """

    def __init__(self, flat_file_location):
        self.flat_file_location = flat_file_location

    def execute(self):
        logger.info('flat file location = %s' % self.flat_file_location)
        if not CheckFile(self.flat_file_location).run():
            return -1

        with open(self.flat_file_location, 'r') as file:
            oid = file.readline().strip()
        return oid


class CheckFilespaceEntriesLocally(Operation):
    """
        Check if filespace entries are valid
    """

    def __init__(self, cur_filespace_entry, peer_filespace_entry, filespace_location, file_type):
        self.cur_filespace_entry = cur_filespace_entry
        self.peer_filespace_entry = peer_filespace_entry
        self.filespace_location = filespace_location
        self.file_type = file_type

    def execute(self):
        if self.file_type == FileType.TRANSACTION_FILES:
            flat_file = GP_TRANSACTION_FILES_FILESPACE
        elif self.file_type == FileType.TEMPORARY_FILES:
            flat_file = GP_TEMPORARY_FILES_FILESPACE

        is_consistent = True
        flat_file_location = os.path.join(self.filespace_location, flat_file)

        # Default filespace pg_system
        if not os.path.exists(flat_file_location):
            return is_consistent

        if not os.path.exists(self.cur_filespace_entry[2]):
            logger.info('path %s does not exist' % self.cur_filespace_entry[2])
            is_consistent = False

        # Read flat file entries and check if the directories exist
        entries_set = set()
        with open(flat_file_location) as file:
            for line in file:
                entries = line.split()
                if len(entries) != 2:
                    continue
                else:
                    logger.info('path = %s' % entries[1].strip())
                    logger.info('dbid = %s' % entries[0].strip())
                    entries_set.add(entries[0].strip() + ' ' + entries[1].strip())

        required_set = set()
        required_set.add(str(self.cur_filespace_entry[1]) + ' ' + self.cur_filespace_entry[2])

        if self.peer_filespace_entry is not None:
            required_set.add(str(self.peer_filespace_entry[1]) + ' ' + self.peer_filespace_entry[2])

        if required_set != entries_set:
            is_consistent = False

        return is_consistent


class CheckFilespacePermissions(Operation):
    """
        Checks if the filespace has the correct permissions
    """

    def __init__(self, location):
        self.location = location

    def execute(self):

        path = os.path.dirname(self.location)

        # In case of recoverseg, the segment directories will be
        # created by the backend code.
        if not os.path.exists(path):
            path = os.path.dirname(path)

        self.location = os.path.join(path, os.path.basename(self.location))

        try:
            with open(self.location, 'w') as file:
                pass
            os.remove(self.location)
        except IOError, e:
            if e.errno == errno.EACCES:
                return False
            else:
                raise
        return True


class CheckFilespaceConsistency(Operation):
    """
        Checks that the filespaces for transaction and temporary
        files are consistent on all segments
    """

    def __init__(self, gparray, file_type):
        self.gparray = gparray
        self.file_type = file_type

    def execute(self):
        logger.info('Checking for filespace consistency')
        if self.file_type == FileType.TRANSACTION_FILES:
            flat_file = GP_TRANSACTION_FILES_FILESPACE
        elif self.file_type == FileType.TEMPORARY_FILES:
            flat_file = GP_TEMPORARY_FILES_FILESPACE

        operations = []
        pg_system_fs_entries = GetFilespaceEntriesDict(GetFilespaceEntries(self.gparray,
                                                                           PG_SYSTEM_FILESPACE).run()).run()
        cur_filespace_entries = GetFilespaceEntriesDict(GetCurrentFilespaceEntries(self.gparray,
                                                                                   self.file_type).run()).run()
        for seg in self.gparray.getDbList():
            flat_file_location = os.path.join(pg_system_fs_entries[seg.getSegmentDbId()][2],
                                              flat_file)
            logger.debug('flat file location = %s' % flat_file_location)
            operations.append(RemoteOperation(CheckFilespaceOidLocally(flat_file_location),
                                              seg.getSegmentHostName()
                                              )
                              )
        ParallelOperation(operations, NUM_WORKERS).run()

        try:
            oid_set = set([int(op.get_ret()) for op in operations])
        except Exception, e:
            logger.error('Invalid OID in flat file on host %s' % op.host)
            return False

        logger.debug('%s oid set = %s' % (FileType.lookup[self.file_type], oid_set))

        if len(oid_set) != 1:
            logger.error('Multiple OIDs found in flat files')
            return False

        # Verify that we have a valid oid and that
        # the filespace exists in the catalog.
        try:
            oid = int(oid_set.pop())
            if oid == -1:
                fs_name = 'pg_system'
            else:
                fs_name = self.gparray.getFileSpaceName(oid)
        except Exception, e:
            logger.error('%s OIDs are inconsistent for %s filespace' % (FileType.lookup[self.file_type], fs_name))
            return False
        logger.info('%s OIDs are consistent for %s filespace' % (FileType.lookup[self.file_type], fs_name))

        # Now check for the filespace entries
        operation_list = []
        for seg in self.gparray.getDbList():
            cur_filespace_entry = cur_filespace_entries[seg.getSegmentDbId()]
            peer_filespace_entry = get_peer_filespace_entry(cur_filespace_entries, seg.getSegmentDbId(),
                                                            seg.getSegmentContentId(), self.gparray.getDbList())
            logger.debug('current_filespace_entry = %s' % str(cur_filespace_entry))
            logger.debug('peer_filespace_entry = %s' % str(peer_filespace_entry))
            operation_list.append(RemoteOperation(
                CheckFilespaceEntriesLocally(cur_filespace_entry, peer_filespace_entry,
                                             pg_system_fs_entries[seg.getSegmentDbId()][2], self.file_type),
                seg.getSegmentHostName()
                )
                                  )

        ParallelOperation(operation_list, NUM_WORKERS).run()

        for operation in operation_list:
            try:
                if not operation.get_ret():
                    logger.error('%s entries are inconsistent for %s filespace on host %s' % (
                    FileType.lookup[self.file_type], fs_name, operation.host))
                    return False
            except Exception, e:
                logger.error('%s entries are inconsistent for %s filespace on host %s' % (
                FileType.lookup[self.file_type], fs_name, operation.host))
                return False

        logger.info('%s entries are consistent for %s filespace' % (FileType.lookup[self.file_type], fs_name))

        return True


def get_peer_filespace_entry(filespace_entries, dbid, content_id, segments):
    """
        Returns the filespace entry for a given 
        DbId corresponding to its peer. 
        i.e given primary, it returns the filespace 
        entry of the mirror, and given mirror, it 
        returns the filespace entry of the primary. 
    
        @param:  DbId of the primary segment    
                 content id of the segment
        @return: A tuple of the form 
                 filespace oid, dbid, filespace directory
    """
    logger.debug('Getting peer filespace entry for %s' % dbid)
    for seg in segments:
        if seg.getSegmentContentId() == content_id and seg.getSegmentDbId() != dbid:
            return filespace_entries[seg.getSegmentDbId()]


class UpdateFlatFilesLocally(Operation):
    """
        Update the flat files on primaries/mirrors locally
    """

    def __init__(self, flat_file, filespace_oid, cur_filespace_entry, peer_filespace_entry):
        self.flat_file = flat_file
        self.filespace_oid = filespace_oid
        self.cur_filespace_entry = cur_filespace_entry
        self.peer_filespace_entry = peer_filespace_entry

    def execute(self):
        logger.info('Updating flat files')

        # If flat file directory does not exist, we create it
        if not os.path.exists(os.path.dirname(self.flat_file)):
            os.mkdir(os.path.dirname(self.flat_file))

        temp_flat_file = self.flat_file + '.tmp'
        lines_to_write = str(self.filespace_oid) + '\n' + \
                         str(self.cur_filespace_entry[1]) + ' ' + self.cur_filespace_entry[2] + '\n'
        if self.peer_filespace_entry is not None:
            lines_to_write += str(self.peer_filespace_entry[1]) + ' ' + self.peer_filespace_entry[2] + '\n'

        with open(temp_flat_file, 'w') as file:
            file.write(lines_to_write)

        logger.debug('Wrote contents = %s' % lines_to_write)

        # Read back and check if what we've written is the same
        with open(temp_flat_file) as file:
            contents = file.read()
            if contents != lines_to_write:
                raise MoveFilespaceError('Failed to write contents to flat file %s' % lines_to_write)

        shutil.move(temp_flat_file, self.flat_file)

        if not os.path.exists(self.cur_filespace_entry[2]):
            os.makedirs(self.cur_filespace_entry[2])

        return self.flat_file + ' ' + contents


class UpdateFlatFiles(Operation):
    """
        Update the flat files on primaries separately
        in case of addmirrors.
        Update the flat files on the mirrors separately
        in case of gpexpand.
    """

    def __init__(self, gparray, primaries=True, expansion=False):
        self.gparray = gparray
        self.primaries = primaries
        self.expansion = expansion

    def execute(self):

        # Obtain list of segments from gparray
        if self.expansion:
            db_list = self.gparray.getExpansionSegDbList()
        else:
            db_list = self.gparray.getDbList()

        if self.primaries:
            segments = [seg for seg in db_list if seg.isSegmentPrimary()]
        else:
            segments = [seg for seg in db_list if seg.isSegmentMirror()]

        logger.debug('segment_list = %s' % self.gparray.getDbList())
        logger.debug('segments on which flat files will be updated = %s' % segments)
        pg_system_filespace_entries = GetFilespaceEntriesDict(GetFilespaceEntries(self.gparray,
                                                                                  PG_SYSTEM_FILESPACE).run()
                                                              ).run()
        transaction_flat_file = os.path.join(pg_system_filespace_entries[1][2], GP_TRANSACTION_FILES_FILESPACE)
        if os.path.exists(transaction_flat_file):
            logger.debug('Updating transaction flat files')
            cur_filespace_entries = GetFilespaceEntriesDict(
                GetCurrentFilespaceEntries(self.gparray, FileType.TRANSACTION_FILES
                                           ).run()
                ).run()
            operation_list = []
            for seg in segments:
                filespace_oid = cur_filespace_entries[seg.getSegmentDbId()][0]
                cur_filespace_entry = cur_filespace_entries[seg.getSegmentDbId()]
                peer_filespace_entry = get_peer_filespace_entry(cur_filespace_entries, seg.getSegmentDbId(),
                                                                seg.getSegmentContentId(), db_list)
                logger.debug('cur_filespace_entry = %s' % str(cur_filespace_entry))
                logger.debug('peer_filespace_entry = %s' % str(peer_filespace_entry))
                flat_file = os.path.join(pg_system_filespace_entries[seg.getSegmentDbId()][2],
                                         GP_TRANSACTION_FILES_FILESPACE)
                operation_list.append(RemoteOperation(UpdateFlatFilesLocally(flat_file,
                                                                             filespace_oid,
                                                                             cur_filespace_entry,
                                                                             peer_filespace_entry
                                                                             ),
                                                      seg.getSegmentHostName())
                                      )

            ParallelOperation(operation_list, NUM_WORKERS).run()

            try:
                for operation in operation_list:
                    operation.get_ret()
            except Exception, e:
                raise MoveFilespaceError('Failed to update transaction flat file.')

        temporary_flat_file = os.path.join(pg_system_filespace_entries[1][2], GP_TEMPORARY_FILES_FILESPACE)
        if os.path.exists(temporary_flat_file):
            logger.debug('Updating temporary flat files')
            cur_filespace_entries = GetFilespaceEntriesDict(
                GetCurrentFilespaceEntries(self.gparray, FileType.TEMPORARY_FILES
                                           ).run()
                ).run()
            operation_list = []
            for seg in segments:
                filespace_oid = cur_filespace_entries[seg.getSegmentDbId()][0]
                cur_filespace_entry = cur_filespace_entries[seg.getSegmentDbId()]
                peer_filespace_entry = get_peer_filespace_entry(cur_filespace_entries, seg.getSegmentDbId(),
                                                                seg.getSegmentContentId(), db_list)
                logger.debug('cur_filespace_entry = %s' % str(cur_filespace_entry))
                logger.debug('peer_filespace_entry = %s' % str(peer_filespace_entry))
                flat_file = os.path.join(pg_system_filespace_entries[seg.getSegmentDbId()][2],
                                         GP_TEMPORARY_FILES_FILESPACE)
                operation_list.append(RemoteOperation(UpdateFlatFilesLocally(flat_file,
                                                                             filespace_oid,
                                                                             cur_filespace_entry,
                                                                             peer_filespace_entry
                                                                             ),
                                                      seg.getSegmentHostName())
                                      )

            ParallelOperation(operation_list, NUM_WORKERS).run()

            try:
                for operation in operation_list:
                    operation.get_ret()
            except Exception, e:
                raise MoveFilespaceError('Failed to update temporary flat file.')


class MoveTransFilespaceLocally(Operation):
    """
        Move the Filespace to the new location for
        transaction files.
        This requires the pg_system filespace entry for a particular dbid 
        because that information is needed to write the gp_transaction_files_filespace 
        flat file.

        @param: filespace entries from the pg_filespace_entry table.
        @return: None
    """
    TRANSACTION_FILES_DIRS = ['pg_xlog', 'pg_multixact', 'pg_subtrans', 'pg_clog',
                              'pg_distributedlog', 'pg_distributedxidmap']

    def __init__(self, current_filespace_entry, new_filespace_name, new_filespace_entry, peer_filespace_entry,
                 pg_system_filespace_entry, rollback=False):
        self.new_filespace_name = new_filespace_name
        self.new_filespace_entry = new_filespace_entry
        self.peer_filespace_entry = peer_filespace_entry
        self.current_filespace_entry = current_filespace_entry
        self.pg_system_filespace_entry = pg_system_filespace_entry
        self.rollback = rollback

    def get_sha256(self, directory):
        m = hashlib.sha256()

        files_to_hash = []
        for root, dirs, filenames in os.walk(directory):
            for filename in filenames:
                file = os.path.join(root, filename)
                files_to_hash.append(file)

        files_to_hash.sort()

        for file in files_to_hash:
            with open(file, 'rb') as f:
                while True:
                    data = f.read(128)
                    if not data:
                        break
                    m.update(data)
        return m.hexdigest()

    def sha256_check(self, src_dir, dst_dir):
        src_sha256 = self.get_sha256(src_dir)
        dst_sha256 = self.get_sha256(dst_dir)

        logger.info('sha256 checksum for %s = %s' % (src_dir, src_sha256))
        logger.info('sha256 checksum for %s = %s' % (dst_dir, dst_sha256))

        if src_sha256 != dst_sha256:
            raise MoveFilespaceError('sha256 Checksum failed for %s' % src_dir)

    def execute(self):

        # if the filespace directory is not present on the segment,
        # We simply return.
        if not CheckDir(self.current_filespace_entry[2]).run():
            return

        tmp_file = os.path.join(self.new_filespace_entry[2], 'tmp_file')
        if not CheckFilespacePermissions(tmp_file).run():
            raise MoveFilespaceError('Invalid permissions for %s' % tmp_file)

        stats = os.statvfs(os.path.dirname(self.new_filespace_entry[2]))
        free_bytes = stats.f_bfree * stats.f_frsize
        logger.info('free_bytes for %s = %s' % (self.new_filespace_entry[2], free_bytes))
        if not free_bytes:
            raise MoveFilespaceError('Insufficient diskspace for %s' % self.new_filespace_entry[2])

        # 1. Copy the contents of the filespace dirs from old filespace to new filespace
        for directory in self.TRANSACTION_FILES_DIRS:
            src_dir = os.path.join(self.current_filespace_entry[2], directory)
            dst_dir = os.path.join(self.new_filespace_entry[2], directory)

            logger.info('Copying directories from %s to %s' % (src_dir, dst_dir))
            try:
                # In case of a rollback, we need to retain the directories that
                # already exist on the source segment. If it doesn't then we do a
                # copy.
                if self.rollback:
                    if CheckDir(src_dir).run() and not CheckDir(dst_dir).run():
                        logger.info('copying %s' % src_dir)
                        shutil.copytree(src_dir, dst_dir)
                        try:
                            self.sha256_check(src_dir, dst_dir)
                        except (IOError, OSError), e:
                            raise MoveFilespaceError('Failed to calculate sha256 checksums !')
                elif CheckDir(src_dir).run():
                    # We use the src segment as the source of truth and copy everything
                    if CheckDir(dst_dir).run():
                        shutil.rmtree(dst_dir)

                    logger.info('copying %s' % src_dir)
                    shutil.copytree(src_dir, dst_dir)

                    try:
                        self.sha256_check(src_dir, dst_dir)
                    except (IOError, OSError), e:
                        raise MoveFilespaceError('Failed to calculate sha256 checksums !')

            except (IOError, OSError), e:
                logger.error('Failed to copy transaction files to new Filespace location.')
                raise

        # 2. Drop the directories in current filespace
        for directory in self.TRANSACTION_FILES_DIRS:
            src_dir = os.path.join(self.current_filespace_entry[2], directory)
            try:
                if CheckDir(src_dir).run():
                    logger.info('Dropping dir %s' % src_dir)
                    shutil.rmtree(src_dir)
            except (IOError, OSError), e:
                logger.error('Failed to drop transaction files directories from current filespace.')
                raise

        gp_transaction_files_filespace_path = os.path.join(self.pg_system_filespace_entry[2],
                                                           GP_TRANSACTION_FILES_FILESPACE)
        # If we are moving to default Filespace, then we need to delete the flat file
        if self.new_filespace_name == PG_SYSTEM_FILESPACE:
            if CheckFile(gp_transaction_files_filespace_path).run():
                os.remove(gp_transaction_files_filespace_path)
            return

        # 3. Write the dbid and directories into tmp directory
        temp_gp_transaction_files_filespace_path = gp_transaction_files_filespace_path + '.tmp'

        logger.info('Writing filespace information into flat file')
        with open(temp_gp_transaction_files_filespace_path, 'w') as file:
            lines_to_write = str(self.new_filespace_entry[0]) + '\n' + \
                             str(self.new_filespace_entry[1]) + ' ' + self.new_filespace_entry[2] + '\n'
            # In case of system without mirrors
            if self.peer_filespace_entry is not None:
                lines_to_write += str(self.peer_filespace_entry[1]) + ' ' + self.peer_filespace_entry[2] + '\n'

            file.write(lines_to_write)

        # Read back and check the file contents
        with open(temp_gp_transaction_files_filespace_path) as file:
            contents = file.read()
            if contents != lines_to_write:
                raise MoveFilespaceError('Failed to write contents to flat file %s' % lines_to_write)

        # 4. After the file copy has been completed successfully,
        #   copy over the tmp directory file 
        shutil.move(temp_gp_transaction_files_filespace_path, gp_transaction_files_filespace_path)


class MoveTempFilespaceLocally(Operation):
    """
        Move the Filespace to the new location for
        temporary files.

        @return: None     
    """
    TEMPORARY_FILES_DIR = 'pgsql_tmp'

    def __init__(self, current_filespace_entry, new_filespace_name, new_filespace_entry, peer_filespace_entry,
                 pg_system_filespace_entry, rollback=None):
        self.new_filespace_name = new_filespace_name
        self.new_filespace_entry = new_filespace_entry
        self.peer_filespace_entry = peer_filespace_entry
        self.current_filespace_entry = current_filespace_entry
        self.pg_system_filespace_entry = pg_system_filespace_entry
        self.rollback = rollback

    def execute(self):

        if not CheckDir(self.current_filespace_entry[2]).run():
            return

        tmp_file = os.path.join(self.new_filespace_entry[2], 'tmp_file')
        if not CheckFilespacePermissions(tmp_file).run():
            raise MoveFilespaceError('Invalid permissions for %s' % tmp_file)

        stats = os.statvfs(os.path.dirname(self.new_filespace_entry[2]))
        free_bytes = stats.f_bfree * stats.f_frsize
        logger.info('free_bytes for %s = %s' % (self.new_filespace_entry[2], free_bytes))
        if not free_bytes:
            raise MoveFilespaceError('Insufficient diskspace for %s' % self.new_filespace_entry[2])

        # 1. Drop the directories from old filespace location
        # If the filespace is being moved from pg_system filespace
        # for temp files will be slightly different as they will be stored under
        # <filespace_dir>/base/<database_oid>/
        gp_temporary_files_filespace_path = os.path.join(self.pg_system_filespace_entry[2],
                                                         GP_TEMPORARY_FILES_FILESPACE)

        if not CheckFile(gp_temporary_files_filespace_path).run():
            base_dir = os.path.join(self.current_filespace_entry[2], 'base')

            directories = []
            try:
                directories = ListFiles(base_dir).run()
            except Exception, e:
                if self.rollback:
                    pass
                else:
                    raise

            for directory in directories:
                try:
                    dst_dir = os.path.join(base_dir, directory, self.TEMPORARY_FILES_DIR)
                    if CheckDir(dst_dir).run():
                        logger.info('Dropping dir %s' % dst_dir)
                        shutil.rmtree(dst_dir)
                except (IOError, OSError), e:
                    logger.error('Failed to delete temporary files')
                    raise
        else:
            # else if filespace has been moved before, the temp files directory will always be under
            # <filespace_dir>/pgsql_tmp
            dst_dir = os.path.join(self.current_filespace_entry[2], self.TEMPORARY_FILES_DIR)
            try:
                if CheckDir(dst_dir).run():
                    logger.info('Dropping dir %s' % dst_dir)
                    shutil.rmtree(dst_dir)
            except (IOError, OSError), e:
                logger.error('Failed to delete temporary filespace directories')
                raise

        # If we are moving to default Filespace, then we need to delete the flat file
        if self.new_filespace_name == PG_SYSTEM_FILESPACE:
            if CheckFile(gp_temporary_files_filespace_path).run():
                os.remove(gp_temporary_files_filespace_path)
            return

        # 2. Write the dbid and directories into tmp directory
        temp_gp_temporary_files_filespace_path = gp_temporary_files_filespace_path + '.tmp'

        logger.info('Writing filespace information into flat file')
        with open(temp_gp_temporary_files_filespace_path, 'w') as file:
            lines_to_write = str(self.new_filespace_entry[0]) + '\n' + \
                             str(self.new_filespace_entry[1]) + ' ' + self.new_filespace_entry[2] + '\n'
            # In case of master, when there is no standby, peer will be None
            if self.peer_filespace_entry is not None:
                lines_to_write += str(self.peer_filespace_entry[1]) + ' ' + self.peer_filespace_entry[2] + '\n'

            file.write(lines_to_write)

        # Read back and check the contents
        with open(temp_gp_temporary_files_filespace_path) as file:
            contents = file.read()
            if contents != lines_to_write:
                raise MoveFilespaceError('Failed to write contents to flat file %s' % lines_to_write)

                # 3. After the file copy has been completed successfully,
        # copy over the tmp directory file
        shutil.move(temp_gp_temporary_files_filespace_path, gp_temporary_files_filespace_path)


class GetFilespaceEntries(Operation):
    """
        Returns the pg_filespace_entry table contents for a given filespace.
        Each entry will be of the form oid, dbid, filespace_directory
 
        @return: List containing the directories used by the filespace
    """

    def __init__(self, gparray, filespace_name):
        self.filespace_name = filespace_name
        self.gparray = gparray

    def execute(self):
        logger.debug('Obtaining filespace information for filespace %s' % self.filespace_name)
        filespace_entries = []

        filespaces = self.gparray.getFilespaces(includeSystemFilespace=True)
        filespace_oid = None
        for fs in filespaces:
            if fs.getName() == self.filespace_name:
                filespace_oid = fs.getOid()

        if filespace_oid is None:
            raise MoveFilespaceError('Invalid filespace name.')

        for seg in self.gparray.getDbList(includeExpansionSegs=True):
            filespace_entries.append((filespace_oid, seg.getSegmentDbId(), seg.getSegmentFilespaces()[filespace_oid]))

        return filespace_entries


class GetCurrentFilespaceEntries(Operation):
    """
        The results returned will contain the entries from 
        pg_filespace_entry table for the current filespace used
        by transaction/temporary files.
    """

    def __init__(self, gparray, file_type):
        self.file_type = file_type
        self.gparray = gparray

    def execute(self):
        logger.info('Obtaining current filespace entries used by %s' % FileType.lookup[self.file_type])
        filespace_entries = []
        default_filespace_entries = GetFilespaceEntries(self.gparray, PG_SYSTEM_FILESPACE).run()
        flat_files_dir = default_filespace_entries[0][2]
        flat_file = None

        if self.file_type == FileType.TRANSACTION_FILES:
            flat_file = GP_TRANSACTION_FILES_FILESPACE
        elif self.file_type == FileType.TEMPORARY_FILES:
            flat_file = GP_TEMPORARY_FILES_FILESPACE

        gp_filespace_path = os.path.join(flat_files_dir, flat_file)

        try:
            if CheckFile(gp_filespace_path).run():
                with open(gp_filespace_path) as file:
                    fs_oid = file.readline().strip()
                filespace_name = self.gparray.getFileSpaceName(int(fs_oid))
                filespace_entries = GetFilespaceEntries(self.gparray, filespace_name).run()
        except TypeError, e:
            raise MoveFilespaceError('Invalid oid in flat file. %s' % str(e))
        except (IOError, OSError), e:
            raise MoveFilespaceError('Failed to read flat file.%s' % str(e))

        if len(filespace_entries) == 0:
            filespace_entries = default_filespace_entries

        return filespace_entries


class GetFilespaceEntriesDict(Operation):
    """
        @param:  A list of filespace entries 
        @return: A dict containing the directories for each
                 corresponding database id.
    """

    def __init__(self, filespace_entries):
        self.filespace_entries = filespace_entries

    def execute(self):
        logger.debug('Converting filespace entries into dict')
        db_directories_dict = {}

        for entry in self.filespace_entries:
            dbid = entry[1]
            db_directories_dict[dbid] = entry

        return db_directories_dict


class RollBackFilespaceChanges(Operation):
    """
        This does the reverse operation of Move().
        Since the Move from cur -> new Filespace failed, 
        we do the reverse and move files back from new -> cur.
    """

    def __init__(self, segments, file_type, cur_filespace_name, cur_filespace_entries, new_filespace_entries,
                 pg_system_filespace_entries):
        self.segments = segments
        self.file_type = file_type
        self.cur_filespace_name = cur_filespace_name
        self.cur_filespace_entries = cur_filespace_entries
        self.new_filespace_entries = new_filespace_entries
        self.pg_system_filespace_entries = pg_system_filespace_entries

    def execute(self):
        logger.info('Rolling back filespace changes ...')
        operations = []
        for seg in self.segments:
            logger.debug('Creating RemoteOperation for segment %s' % seg)
            peer_filespace_entry = get_peer_filespace_entry(self.cur_filespace_entries, seg.getSegmentDbId(),
                                                            seg.getSegmentContentId(), self.segments)
            if self.file_type == FileType.TRANSACTION_FILES:
                # Move from new -> cur
                operations.append(
                    RemoteOperation(MoveTransFilespaceLocally(self.new_filespace_entries[seg.getSegmentDbId()],
                                                              self.cur_filespace_name,
                                                              self.cur_filespace_entries[seg.getSegmentDbId()],
                                                              peer_filespace_entry,
                                                              self.pg_system_filespace_entries[seg.getSegmentDbId()],
                                                              rollback=True
                                                              ),
                                    seg.getSegmentHostName()
                                    ),
                    )
            elif self.file_type == FileType.TEMPORARY_FILES:
                operations.append(
                    RemoteOperation(MoveTempFilespaceLocally(self.new_filespace_entries[seg.getSegmentDbId()],
                                                             self.cur_filespace_name,
                                                             self.cur_filespace_entries[seg.getSegmentDbId()],
                                                             peer_filespace_entry,
                                                             self.pg_system_filespace_entries[seg.getSegmentDbId()],
                                                             rollback=True
                                                             ),
                                    seg.getSegmentHostName()
                                    ),
                    )

        logger.debug('Running remote operations in parallel')
        ParallelOperation(operations, NUM_WORKERS).run()

        logger.debug('Checking results of parallel operations')
        for operation in operations:
            operation.get_ret()


class GetMoveOperationList(Operation):
    """
        @return: A list containing a set of RemoteOperations
                 that are executed on remote hosts via ParallelOperation.
    """

    def __init__(self, segments, file_type, new_filespace_name, new_filespace_entries, cur_filespace_entries,
                 pg_system_filespace_entries):
        self.segments = segments
        self.file_type = file_type
        self.new_filespace_name = new_filespace_name
        self.new_filespace_entries = new_filespace_entries
        self.cur_filespace_entries = cur_filespace_entries
        self.pg_system_filespace_entries = pg_system_filespace_entries

    def execute(self):
        logger.info('Creating RemoteOperations list')
        operations = []
        for seg in self.segments:
            logger.debug('segment_dbid = %s' % seg.getSegmentDbId())
            logger.debug('segmenthostname = %s' % seg.getSegmentHostName())
            logger.debug(self.new_filespace_entries[seg.getSegmentDbId()])
            logger.debug(self.cur_filespace_entries[seg.getSegmentDbId()])
            peer_filespace_entry = get_peer_filespace_entry(self.new_filespace_entries, seg.getSegmentDbId(),
                                                            seg.getSegmentContentId(), self.segments)
            if self.file_type == FileType.TRANSACTION_FILES:
                operations.append(
                    RemoteOperation(MoveTransFilespaceLocally(self.cur_filespace_entries[seg.getSegmentDbId()],
                                                              self.new_filespace_name,
                                                              self.new_filespace_entries[seg.getSegmentDbId()],
                                                              peer_filespace_entry,
                                                              self.pg_system_filespace_entries[seg.getSegmentDbId()]
                                                              ),
                                    seg.getSegmentHostName()))
            elif self.file_type == FileType.TEMPORARY_FILES:
                operations.append(
                    RemoteOperation(MoveTempFilespaceLocally(self.cur_filespace_entries[seg.getSegmentDbId()],
                                                             self.new_filespace_name,
                                                             self.new_filespace_entries[seg.getSegmentDbId()],
                                                             peer_filespace_entry,
                                                             self.pg_system_filespace_entries[seg.getSegmentDbId()]
                                                             ),
                                    seg.getSegmentHostName()))
        return operations


class MoveFilespace(Operation):
    """
        Main class which configures the database to use a custom filespace 
        for temporary and transaction files.
    """

    def __init__(self, new_filespace_name, file_type, user, pswd):
        self.user = user
        self.pswd = pswd
        self.new_filespace_name = new_filespace_name
        self.file_type = file_type

    def start_master_only(self):
        logger.info('Starting Greenplum Database in master only mode')
        cmd = GpStart('Start Greenplum Database in master only mode', masterOnly=True)
        cmd.run()
        if cmd.get_results().rc != 0:
            logger.error('Failed to start Greenplum Database in master only mode.')
            cmd.validate()

    def stop_master_only(self):
        logger.info('Stopping Greenplum Database in master only mode')
        cmd = GpStop('Stop Greenplum Database in master only mode', masterOnly=True)
        cmd.run()
        if cmd.get_results().rc != 0:
            logger.error('Failed to stop Greenplum Database in master only mode.')
            cmd.validate()

    def start_database(self):
        logger.info('Starting Greenplum Database')
        cmd = GpStart('Start Greenplum Database')
        cmd.run()
        if cmd.get_results().rc != 0:
            logger.error('Failed to start Greenplum Database.')
            cmd.validate()

    def stop_database(self):
        logger.info('Stopping Greenplum Database')
        cmd = GpStop('Stop Greenplum Databse')
        cmd.run()
        if cmd.get_results().rc != 0:
            logger.error('Failed to stop Greenplum Database.')
            cmd.validate()

    def check_database_stopped(self):
        try:
            mode = get_local_db_mode(MASTER_DATA_DIR)
            logger.info('Database was started in %s mode' % mode)
        except Exception, e:
            logger.info('Database might already be stopped.')
            return True

        return False

    def execute(self):

        try:
            # Disable Ctrl+C
            signal.signal(signal.SIGINT, signal.SIG_IGN)

            # StopGPDB - Check for GPDB connections first and fail if connections exist
            if not self.check_database_stopped():
                self.stop_database()

            # StartGPDB in Master only mode
            self.start_master_only()

            try:

                if not CheckSuperUser(dbconn.DbURL(username=self.user, password=self.pswd)).run():
                    raise MoveFilespaceError('gpfilespace requires database superuser privileges.')

                gparray = GpArray.initFromCatalog(dbconn.DbURL(), utility=True)

                # CheckFilespace - Check if new filespace exists
                if not CheckFilespace(self.new_filespace_name).run():
                    raise MoveFilespaceError('Filespace %s does not exist' % self.new_filespace_name)

                    # CheckFilespaceIsSame - Check if filespace is different from the old one
                if CheckFilespaceIsSame(gparray, self.new_filespace_name, self.file_type).run():
                    raise MoveFilespaceError('Filespace %s is same as the current filespace' % self.new_filespace_name)
            except Exception, e:
                # Bring GPDB Offline
                self.stop_master_only()
                # Restart the database
                self.start_database()
                raise

            # Bring GPDB Offline
            if 'UTILITY' == get_local_db_mode(MASTER_DATA_DIR):
                self.stop_master_only()
            else:
                raise MoveFilespaceError('Database state is invalid.')

            if not CheckConnectivity(gparray).run():
                raise MoveFilespaceError('Failed connectivity test')

            logger.info('Obtaining current filespace information')
            # Find the filespace directory used for each segment
            # query using pg_filespace, pg_filespace_entry, gp_segment_configuration
            # If gp_transaction/temporary flat files exist and is not empty, then we know
            # the filespace being used. Otherwise, we assume that it is the pg_system
            # filespace by default

            cur_filespace_entries = GetFilespaceEntriesDict(GetCurrentFilespaceEntries(gparray,
                                                                                       self.file_type).run()).run()
            new_filespace_entries = GetFilespaceEntriesDict(GetFilespaceEntries(gparray,
                                                                                self.new_filespace_name).run()).run()
            pg_system_filespace_entries = GetFilespaceEntriesDict(GetFilespaceEntries(gparray,
                                                                                      PG_SYSTEM_FILESPACE).run()).run()

            cur_filespace_name = gparray.getFileSpaceName(int(cur_filespace_entries[1][0]))

            logger.info('Obtaining segment information ...')
            segments = gparray.getDbList()

            # MoveTemp/Trans files

            try:
                operations = GetMoveOperationList(segments,
                                                  self.file_type,
                                                  self.new_filespace_name,
                                                  new_filespace_entries,
                                                  cur_filespace_entries,
                                                  pg_system_filespace_entries
                                                  ).run()
            except Exception, e:
                raise MoveFilespaceError('Failed to create operations list. %s' % str(e))

            logger.info('Moving %s filespace from %s to %s ...' % (
            FileType.lookup[self.file_type], cur_filespace_name, self.new_filespace_name))

            ParallelOperation(operations, NUM_WORKERS).run()

            try:
                for operation in operations:
                    operation.get_ret()
            except Exception, e:
                logger.error('Failed to move files on remote host. %s' % str(e))
                try:
                    RollBackFilespaceChanges(segments, self.file_type, cur_filespace_name, cur_filespace_entries,
                                             new_filespace_entries, pg_system_filespace_entries).run()
                except Exception, e:
                    raise MoveFilespaceError('Rollback Failed !')

            # Bring GPDB online in normal mode
            self.start_database()

        finally:
            # Enable Ctrl+C
            signal.signal(signal.SIGINT, signal.default_int_handler)
