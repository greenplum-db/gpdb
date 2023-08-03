# Line too long - pylint: disable=C0301
# Copyright (c) Greenplum Inc 2011. All Rights Reserved.

from contextlib import closing
import os
import shutil
import sys
import tarfile
import re
import random
import string

try:
    from gppylib import gplog
    from gppylib.commands import gp
    from gppylib.commands.base import Command
    from gppylib.commands.unix import RsyncFromFileList, Rsync
    from gppylib.operations import Operation
    from gppylib.operations.utils import RemoteOperation
    from gppylib.operations.unix import CheckFile, CheckDir, MakeDir, RemoveFile, RemoveRemoteTree, RemoveRemoteFile, \
        CheckRemoteDir, MakeRemoteDir, CheckRemoteFile, ListRemoteFilesByPattern, ListFiles, ListFilesByPattern
except ImportError as ex:
    sys.exit(
        'Operation: Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(ex))

logger = gplog.get_default_logger()


def dereference_symlink(path):
    """
    MPP-15429: rpm is funky with symlinks...
    During an rpm -e invocation, rpm mucks with the /usr/local/greenplum-db symlink.
    From strace output, it appears that rpm tries to rmdir any directories it may have created during
    package installation. And, in the case of our GPHOME symlink, rpm will actually try to unlink it.
    To avoid this scenario, we perform all rpm actions against the "symlink dereferenced" $GPHOME.
    """
    path = os.path.normpath(path)
    if not os.path.islink(path):
        return path
    link = os.path.normpath(os.readlink(path))
    if os.path.isabs(link):
        return link
    return os.path.join(os.path.dirname(path), link)


GPHOME = dereference_symlink(gp.get_gphome())
GPPKG_METADATA_DIR = os.path.join(GPHOME, "usr/local/gppkg")
GPPKG_METADATA_EXTENSION = ".ini"
GPPKG_METADATA_FILELINE_PATTERM = re.compile(r""".*permission.*type.*size.*sha1.*md5""")

def get_package_file_list(gppkg_package_name):
    filelist = []
    gppkg_metadata_file = "{}/{}{}".format(GPPKG_METADATA_DIR, gppkg_package_name, GPPKG_METADATA_EXTENSION)
    with open(gppkg_metadata_file, "rb") as f:
        for linenum, line in enumerate(f):
            if linenum == 0 and line != b'version=v1\n':
                raise Exception("Unexpect version of package's metadata of gppkg: '{}'".format(gppkg_package_name))
            line = line.decode('utf-8')
            if GPPKG_METADATA_FILELINE_PATTERM.match(line):
                filelist.append(line.split("=")[0])
    filelist.append("usr/local/gppkg/" + gppkg_package_name + GPPKG_METADATA_EXTENSION)
    return filelist

def random_str(N):
    return ''.join([random.choice(string.ascii_letters) for _ in range(N)])

class SyncPackages(Operation):
    """
    Synchronizes packages from coordinator to a remote host
    TODO: AK: MPP-15568
    """
    def __init__(self, host):
        self.host = host

    def execute(self):
        if not CheckDir(GPPKG_METADATA_DIR).run():
            MakeDir(GPPKG_METADATA_DIR).run()
        if not CheckRemoteDir(GPPKG_METADATA_DIR, self.host).run():
            MakeRemoteDir(GPPKG_METADATA_DIR, self.host).run()

        # set of packages on the coordinator
        coordinator_package_set = set(
            map(lambda x : x[:-4], ListFilesByPattern(GPPKG_METADATA_DIR, '*' + GPPKG_METADATA_EXTENSION).run()))
        # set of packages on the remote host
        remote_package_set = set(
            map(lambda x : x[:-4], ListRemoteFilesByPattern(GPPKG_METADATA_DIR, '*' + GPPKG_METADATA_EXTENSION, self.host).run()))
        # packages to be uninstalled on the remote host
        uninstall_package_set = remote_package_set - coordinator_package_set
        # packages to be installed on the remote host
        install_package_set = coordinator_package_set - remote_package_set

        if not install_package_set and not uninstall_package_set:
            logger.info('The packages on {} are consistent.'.format(self.host))
            return

        if install_package_set:
            logger.info(
                'The following packages will be installed on {}: {}'.format(self.host, ', '.join(sorted(install_package_set))))
            for package in install_package_set:
                logger.debug('Copying {} to {}'.format(package, self.host))
                file_list = get_package_file_list(package)
                sync_list_file = os.path.join(GPHOME, "/tmp/gpdb.{}.{}.synclist".format(package, random_str(4)))
                with open(sync_list_file, 'wt') as f:
                    for file_ in file_list:
                        f.write(file_ + "\n")
                RsyncFromFileList(name='Copying {} to {}'.format(package, self.host),
                                  sync_list_file=sync_list_file,
                                  local_base_dir=GPHOME,
                                  remote_basedir=GPHOME,
                                  dstHost=self.host).run(validateAfter=True)
                RemoveFile(sync_list_file).run()
                logger.info("Package '{}' has been successfully synced to host: {}".format(package, self.host))

        logger.info("{}".format(uninstall_package_set))
        if uninstall_package_set:
            logger.info(
                'The following packages will be uninstalled on {}: {}'.format(self.host, ', '.join(sorted(uninstall_package_set))))
            for package in uninstall_package_set:
                remote_package_metadata = "{}/{}.ini".format(GPPKG_METADATA_DIR, package)
                remove_package_metadata = os.path.join(GPHOME, "/tmp/gpdb.{}.{}.remove.ini".format(package, random_str(4)))
                Rsync(name='Syncing {} to localhost'.format(package),
                      srcFile=remote_package_metadata,
                      dstFile=remove_package_metadata,
                      srcHost=self.host, ignore_times=True, whole_file=True).run(validateAfter=True)
                remove_list = get_package_file_list(package)
                for file_ in remove_list:
                    RemoveRemoteFile(os.path.join(GPHOME, file_), self.host).run()
                RemoveFile(remove_package_metadata).run()
                logger.info("Package '{}' has been successfully removed from host: {}".format(package, self.host))
