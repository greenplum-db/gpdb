# Line too long - pylint: disable=C0301
# Copyright (c) Greenplum Inc 2011. All Rights Reserved.

from contextlib import closing
import os
import platform
import shutil
import sys
import tarfile

try:
    from gppylib import gplog
    from gppylib.commands import gp
    from gppylib.commands.base import Command, REMOTE, WorkerPool, ExecutionError
    from gppylib.commands.unix import Scp
    from gppylib.gpversion import GpVersion
    from gppylib.mainUtils import ExceptionNoStackTraceNeeded
    from gppylib.operations import Operation
    from gppylib.operations.utils import RemoteOperation, ParallelOperation
    from gppylib.operations.unix import CheckFile, CheckDir, MakeDir, RemoveFile, RemoveRemoteTree, RemoveRemoteFile, \
        CheckRemoteDir, MakeRemoteDir, CheckRemoteFile, ListRemoteFilesByPattern, ListFiles, ListFilesByPattern
    from gppylib.utils import TableLogger

    import yaml
    from yaml.scanner import ScannerError
except ImportError, ex:
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

GPPKG_EXTENSION = '.gppkg'
SPECFILE_NAME = 'gppkg_spec.yml'
SPECFILE_REQUIRED_TAGS = ['pkgname', 'version', 'architecture', 'os', 'description', 'gpdbversion']
SPECFILE_OPTIONAL_TAGS = ['preinstall', 'postinstall', 'preuninstall', 'postuninstall', 'postupdate']

# TODO: AK: Our interactions with the internal RPM database could benefit from an abstraction layer
# that hides the underlying commands used for installation, uninstallation, queries, etc.
RPM_DATABASE_PATH = 'share/packages/database'
RPM_DATABASE = os.path.join(GPHOME, RPM_DATABASE_PATH)
RPM_INSTALLATION_PATH = GPHOME

# An Update for deb package
DEB_DATABASE_DIR = GPHOME + '/share/packages/database/deb/{info,updates,triggers}'
DEB_DATABASE = os.path.join(GPHOME + '/share/packages/database/deb')
DEB_STATUSFILE = GPHOME + '/share/packages/database/deb/status'

# TODO: AK: Our interactions with the archive could benefit from an abstraction layer
# that hides the implementations of archival, unarchival, queries, etc.
# That is, consider the query "is this package already archived?" Currently, this is implemented
# with a CheckFile. Rather, it should be a call to Archive.contains(package), where package
# is instanceof Gppkg.
ARCHIVE_PATH = 'share/packages/archive'
GPPKG_ARCHIVE_PATH = os.path.join(GPHOME, ARCHIVE_PATH)

# TODO: AK: Shouldn't this be "$GPHOME/.tmp"?
# i.e. what if remote host has its $GPHOME elsewhere?
TEMP_EXTRACTION_PATH = GPHOME + '/.tmp'
DEPS_DIR = 'deps'


class GpdbVersionError(Exception):
    """
        Exception to notify that the gpdb version
        does not match
    """
    pass


class AlreadyInstalledError(Exception):
    def __init__(self, package_name):
        Exception.__init__(self, '%s is already installed.' % package_name)


class NotInstalledError(Exception):
    def __init__(self, package_name):
        Exception.__init__(self, '%s is not installed.' % package_name)


class BuildPkgError(Exception):
    """
        Exception to notify that there was an error during
        the building of a gppkg
    """
    pass


class MissingDependencyError(Exception):
    """
        Exception to catch missing dependency
    """

    def __init__(self, value):
        Exception.__init__(self, 'Dependency %s is missing' % value)


class OSCompatibilityError(Exception):
    """
        Exception to notify that OS does not meet the
        requirement
    """

    def __init__(self, requiredos, foundos):
        Exception.__init__(self, '%s OS required. %s OS found' % (requiredos, foundos))


class ArchCompatibilityError(Exception):
    """
        Exception to notify that architecture does not meet
        the requirement
    """

    def __init__(self, requiredarch, foundarch):
        Exception.__init__(self, '%s Arch required. %s Arch found' % (requiredarch, foundarch))


class RequiredDependencyError(Exception):
    """
        Exception to notify that the package being uninstalled
        is a dependency for another package
    """
    pass


class Gppkg:
    """
        This class stores all the information about a gppkg
    """

    def __init__(self, pkg, pkgname, main_rpm, version, architecture, os, gpdbversion, description, abspath, preinstall,
                 postinstall, preuninstall, postuninstall, postupdate, dependencies, file_list):
        """
            The constructor takes the following arguments
            pkg             The complete package name e.g pgcrypto-1.0-Darwin-i386.gppkg        TODO: AK: This is an awful variable name. Change to "package_filename".
            pkgname         The name of the package as specified in the spec file
            main_rpm        The name of the main rpm. e.g PL/R, PostGIS etc
            version         The version of the gppkg
            architecture    The architecture for which the package is built
            os              The operating system for which the package is built
            gpdbversion     The Greenplum Database version for which package is built
            description     A short description for the package
            abspath         This is the absolute path where the package sits on the host
            preinstall      The cluster level preinstallation hooks
            postinstall     The cluster level postinstallation hooks
            preuninstall    The cluster level preuninstallation hooks
            postuninstall   The cluster level postuninstallation hooks
            postupdate      The cluster level postupdate hooks
            dependencies    The dependencies of the package. e.g Geos, Proj in case of PostGIS
            file_list       The list of files present in the package
        """

        logger.debug('Gppkg Constructor')

        self.pkg = pkg
        self.pkgname = pkgname
        self.main_rpm = main_rpm
        self.version = version
        self.architecture = architecture
        self.os = os
        self.gpdbversion = gpdbversion
        self.description = description
        self.abspath = abspath
        self.preinstall = preinstall
        self.postinstall = postinstall
        self.preuninstall = preuninstall
        self.postuninstall = postuninstall
        self.postupdate = postupdate
        self.dependencies = dependencies
        self.file_list = file_list

    @staticmethod
    def from_package_path(pkg_path):
        '''
             This method takes a package as the argument and
             obtains all the information about the package
             Details include name, arch, OS, version, description, dependencies,
             list of files present in the package and returns a gppkg object
        '''

        logger.debug('from_package_path')

        if not os.path.exists(pkg_path):
            logger.error('Cannot find package %s' % pkg_path)
            raise IOError

        # We check for a directory first because
        # is_tarfile does not accept directories as path names
        if os.path.isdir(pkg_path):
            logger.error('%s is a directory !' % pkg_path)
            raise IOError

        if not tarfile.is_tarfile(pkg_path) or not pkg_path.endswith(GPPKG_EXTENSION):
            logger.error('%s is Not a valid package' % pkg_path)
            raise IOError

        if os.path.getsize(pkg_path) == 0:
            logger.error('Package is empty')
            raise IOError

        pkg = {}

        # XXX: AK: It's purely coincidence that the optional tags are lists.
        for tag in SPECFILE_REQUIRED_TAGS:
            pkg[tag] = ''
        for tag in SPECFILE_OPTIONAL_TAGS:
            pkg[tag] = []

        pkg['file_list'] = []
        pkg['dependencies'] = []

        with closing(tarfile.open(pkg_path, 'r:gz')) as tarinfo:
            # store the list of all files present in the archive
            archive_list = tarinfo.getnames()
            pkg["file_list"] = archive_list
            keys = []
            yamlfile = {}

            # The spec file has to be called gppkg_spec
            # so there will only be one such file,
            for cur_file in archive_list:
                if cur_file.endswith(SPECFILE_NAME):
                    specfile = tarinfo.extractfile(cur_file)
                    yamlfile = yaml.load(specfile)
                    keys = yamlfile.keys()
                    break

        # store all the tags
        for key in keys:
            pkg[key.lower()] = yamlfile[key]

        # update the pkgpath
        pkg['pkg'] = os.path.split(pkg_path)[-1]

        # make the version as string
        pkg['version'] = str(pkg['version'])

        # Convert the required version to a GpVersion
        pkg['gpdbversion'] = GpVersion(str(pkg['gpdbversion']))

        # update the absolute path
        pkg['abspath'] = pkg_path

        # store all the dependencies of the gppkg
        if platform.linux_distribution()[0] == 'Ubuntu':
            for cur_file in archive_list:
                if cur_file.find('deps/') != -1 and cur_file.endswith('.deb'):
                    pkg['dependencies'].append(cur_file[cur_file.rfind('/') + 1:])

            # store the main deb, remain the attr name main_rpm to keep compatibility
            for cur_file in archive_list:
                if cur_file.find('deps/') == -1 and cur_file.endswith('.deb'):
                    pkg['main_rpm'] = cur_file

        else:
            for cur_file in archive_list:
                if cur_file.find('deps/') != -1 and cur_file.endswith('.rpm'):
                    pkg['dependencies'].append(cur_file[cur_file.rfind('/') + 1:])

            # store the main rpm
            for cur_file in archive_list:
                if cur_file.find('deps/') == -1 and cur_file.endswith('.rpm'):
                    pkg['main_rpm'] = cur_file

        gppkg = Gppkg(**pkg)

        return gppkg


class LocalCommand(Operation):
    """
    DEPRECATED

    TODO: AK: Eliminate this. Replace invocations with Command(...).run(validateAfter = True)
    """

    def __init__(self, cmd_str, echo=False):
        self.cmd_str = cmd_str
        self.echo = echo

    def execute(self):

        logger.debug(self.cmd_str)
        cmd = Command(name='LocalCommand', cmdStr=self.cmd_str)
        cmd.run(validateAfter=True)
        if self.echo:
            echo_str = cmd.get_results().stdout.strip()
            if echo_str:
                logger.info(echo_str)
        return cmd.get_results()


class RemoteCommand(Operation):
    """
    DEPRECATED

    TODO: AK: Rename as GpSsh, like GpScp below.
    """

    def __init__(self, cmd_str, host_list):
        self.cmd_str = cmd_str
        self.host_list = host_list
        self.pool = None

    def execute(self):
        logger.debug(self.cmd_str)

        # Create Worker pool
        # and add commands to it
        self.pool = WorkerPool()

        for host in self.host_list:
            cmd = Command(name='Remote Command', cmdStr=self.cmd_str, ctxt=REMOTE, remoteHost=host)
            self.pool.addCommand(cmd)
        self.pool.join()

        # This will raise ExecutionError exception if even a single command fails
        self.pool.check_results()


class ListPackages(Operation):
    """
        Lists all the packages present in
        $GPHOME/share/packages/archive
    """

    def __init__(self):
        pass

    def execute(self):
        # Ensure archive path exists
        # TODO: AK: In hindsight, this should've been named MakeDirP,
        # to reflect that it won't blow up if the path already exists.
        MakeDir(GPPKG_ARCHIVE_PATH).run()

        package_list = ListFilesByPattern(GPPKG_ARCHIVE_PATH, '*' + GPPKG_EXTENSION).run()

        package_name_list = []

        for pkg in package_list:
            pkg_name = pkg.split('/')[-1]
            if len(pkg_name.split('-')) >= 2:
                package_name_list.append(pkg_name[:pkg_name.index('-', pkg_name.index('-') + 1)])
            else:
                try:
                    package_name_list.append(pkg_name[:pkg_name.index('.')])
                except ValueError:
                    raise Exception('unable to parse %s as a gppkg' % pkg_name)

        return package_name_list


class CleanupDir(Operation):
    """
        Cleans up the given dir
        Returns True if either the dir is already removed
        or if we were able to remove the dir successfully
        False for other errors
    """

    def __init__(self, dir_path):
        self.dir_path = dir_path

    def execute(self):

        dir_path = self.dir_path

        logger.debug('Cleaning up %s' % dir_path)

        # If file does not exist, nothing to remove
        # So we return true
        if not os.path.exists(dir_path):
            return True

        if os.path.isdir(dir_path):
            shutil.rmtree(dir_path)
        else:
            return False

        return True


class IsVersionCompatible(Operation):
    """
        Returns True if the gppkg is compatible
        with the gpdb version that has been installed
    """

    def __init__(self, gppkg):
        super(IsVersionCompatible, self).__init__()
        self.gppkg = gppkg

    def execute(self):

        gppkg = self.gppkg

        gpdb_version = self._get_gpdb_version()
        required_gpdb_version = gppkg.gpdbversion

        logger.debug('Greenplum Database Version = %s' % gpdb_version)
        logger.debug('Required Greenplum Database version = %s' % required_gpdb_version)

        if gpdb_version is None:
            logger.error('Could not determine Greenplum Database version')
            return False

        if not required_gpdb_version.isVersionRelease(gpdb_version):
            logger.error('%s requires Greenplum Database version %s' % (gppkg.pkgname, required_gpdb_version))
            return False

        return True

    def _get_gpdb_version(self):
        """
            Get the version of the current GPDB
            Returns a string consisting of the major
            release version
        """
        logger.debug('_get_gpdb_version')
        self.gphome = gp.get_gphome()
        version = gp.GpVersion.local('local GP software version check', self.gphome)
        gpdb_version = GpVersion(version.strip())
        return gpdb_version

    def _convert_to_magic_number_version(self, gpversion_obj):
        """
            Converts GPDB version to the GPDB magic number
            Returns an int consisting of the major and minor release version
        """
        logger.debug('_convert_to_magic_number_version')

        ver_list = gpversion_obj.version

        # The generation of the magic version number (GP_VERSION_NUM) is
        # retrieved from our configure.in file
        magic_num = "%d%02d%02d" % (ver_list[0], ver_list[1],
                                    ver_list[2] if len(ver_list) > 2 else 0)
        return int(magic_num)


class ValidateInstallDebPackage(Operation):
    """
    A replicate class of ValidateInstallPackage that used for test deb package for Ubuntu,
    it will use dpkg --verify that is instead of rpm --test

    NOTE: In newest GPDB, the dependencies will use system's dependencies, we still leave the gppkg
    to install dependencies, but did not check it anymore.

    """

    def __init__(self, gppkg, is_update=False):
        self.gppkg = gppkg
        self.is_update = is_update

    def execute(self):
        # Check the GPDB requirements
        if not IsVersionCompatible(self.gppkg).run():
            raise GpdbVersionError

        self.prepare_deb_env()

        # No need to check the architecture here as there is no i386 arch in GPDB 5 or newer version
        deb_set = set([self.gppkg.main_rpm] + self.gppkg.dependencies)
        deb_packages_path = ' '.join([os.path.join(TEMP_EXTRACTION_PATH, deb_path) for deb_path in deb_set])

        if self.is_update:
            deb_test_command = 'fakeroot dpkg --dry-run --force-not-root --force-depends --log=/dev/null --admindir=%s ' \
                               '--instdir=%s -i %s' % (DEB_DATABASE, GPHOME, deb_packages_path)
        else:
            deb_test_command = 'fakeroot dpkg --dry-run --force-not-root --log=/dev/null --admindir=%s ' \
                               '--instdir=%s -i %s' % (DEB_DATABASE, GPHOME, deb_packages_path)

        cmd = Command('Validating deb installation', deb_test_command)
        logger.info(cmd)

        try:
            cmd.run(validateAfter=True)
        except ExecutionError, e:
            lines = e.cmd.get_results().stderr.splitlines()

            if len(lines) == 0:
                raise

            # TODO: Add dependency and already installed check

        archive_package_exists = CheckFile(os.path.join(GPPKG_ARCHIVE_PATH, self.gppkg.pkg)).run()
        package_already_installed = (not deb_set) and archive_package_exists
        if package_already_installed:
            raise AlreadyInstalledError(self.gppkg.pkg)

        already_installed = self.check_existence()
        if self.is_update:
            if not already_installed:
                raise NotInstalledError(self.gppkg.pkg)
        else:
            if already_installed:
                raise AlreadyInstalledError(self.gppkg.pkg)

        return deb_set

    def check_existence(self):
        command = 'dpkg --force-not-root --log=/dev/null --admindir=%s --instdir=%s -l %s' % (DEB_DATABASE, GPHOME, self.gppkg.pkgname)
        cmd = Command('Check installation of package', command)
        logger.info(cmd)

        cmd.run(validateAfter=False)
        return cmd.results.rc == 0

    def prepare_deb_env(self):
        prepare_env_dir = 'mkdir -p ' + DEB_DATABASE_DIR
        prepare_env_file = 'touch ' + DEB_STATUSFILE
        cmd_dir = Command('Prepare deb package DIY directories', prepare_env_dir)
        cmd_file = Command('Prepare deb package Status file', prepare_env_file)

        try:
            cmd_dir.run(validateAfter=True)
            cmd_file.run(validateAfter=True)
        except ExecutionError, e:
            lines = e.cmd.get_results().stderr.splitlines()

            raise ExecutionError('Can not setup deb package env', lines)


class ValidateInstallPackage(Operation):
    """
    Ensure that the given rpms can be installed safely. This is accomplished mainly
    through use of rpm --test, which will have one of a few outcomes:
    1) A return code of 0, indicating the installation should proceed smoothly
    2) A non-zero return code, and stderr indicating some of the rpms are already installed.
       We simply omit such rpms from the returned list of rpms, indicating to the caller
       that to be successful, installation should only be attempted on the filtered list of rpms.
    3) A non-zero return code, and stderr indicating that a failed dependency issue will arise.
       This scenario must result in a MissingDependencyError.

    Note: install and update share this code, because there is extensive commonality in regards
    to the version, os, arch. checking, in addition to the 3 code paths enumerated just above.

    Lastly, for an edge case, if we determine that all of the relevant rpms are currently installed
    *and* the archive package already exists we declare the package is already installed.

    TODO: This is depending on ExtractPackage having put the dependencies in this same directory.
    TODO: Use regexes for more reliable string matching. CR-2865#c20112
    """

    def __init__(self, gppkg, is_update=False):
        self.gppkg = gppkg
        self.is_update = is_update

    def execute(self):
        # Check the GPDB requirements
        if not IsVersionCompatible(self.gppkg).run():
            raise GpdbVersionError

        # TODO: AK: I've changed our use of the OS tag from 'Linux' to 'rhel5' or 'suse10'.
        # So, the two lines below will not work properly.
        # if self.gppkg.os.lower() != platform.system().lower():
        #    raise OSCompatibilityError(self.gppkg.os, platform.system().lower())

        # architecture compatibility
        if self.gppkg.architecture.lower() != platform.machine().lower():
            raise ArchCompatibilityError(self.gppkg.architecture, platform.machine().lower())

        rpm_set = set([self.gppkg.main_rpm] + self.gppkg.dependencies)
        rpm_install_string = ' '.join([os.path.join(TEMP_EXTRACTION_PATH, rpm) for rpm in rpm_set])
        if self.is_update:
            rpm_install_command = 'rpm --test -U --force %s --dbpath %s --prefix %s' % (
            rpm_install_string, RPM_DATABASE, RPM_INSTALLATION_PATH)
        else:
            rpm_install_command = 'rpm --test -i %s --dbpath %s --prefix %s' % (
            rpm_install_string, RPM_DATABASE, RPM_INSTALLATION_PATH)
        cmd = Command('Validating rpm installation', rpm_install_command)
        logger.info(cmd)  # TODO: AK: This should be debug(), but RMI cannot propagate a log level.

        try:
            cmd.run(validateAfter=True)
        except ExecutionError, e:
            lines = e.cmd.get_results().stderr.splitlines()

            # Forking between code paths 2 and 3 depends on some meaningful stderr
            # Without such stderr, we must bubble up the ExecutionError.
            if len(lines) == 0:
                raise

            if 'failed dependencies' in lines[0].lower():
                # Code path 3 (see docstring)
                # example stderr:
                # error: Failed dependencies:
                #    geos-3.2.2-1.x86_64.rpm is needed by postgis-1.0-1.x86_64

                # TODO: AK: Dependencies should be parsed out here and used to initialize
                # this MissingDependencyError. However, this exception does not support
                # multiple missing dependencies. Some refactoring work is needed in both places.
                logger.error(e.cmd.get_results().stderr)
                raise MissingDependencyError('')

            # Code path 2, possibly (see docstring)
            # example stderr:
            #    package geos-3.2.2-1.x86_64 is already installed
            #    package proj-4.7.0-1.x86_64 is already installed
            #    package postgis-1.0-1.x86_64 is already installed
            for line in lines:
                if 'already installed' in line.lower():
                    package_name = line.split()[1]
                    rpm_name = "%s.rpm" % package_name
                    rpm_set.remove(rpm_name)
                else:
                    # This is unexpected, so bubble up the ExecutionError.
                    raise

        # MPP-14359 - installation and uninstallation prechecks must also consider
        # the archive. That is, if a partial installation had added all rpms
        # but failed to add the archive package, then for our purposes, we consider
        # the package not yet installed and still in need of InstallPackageLocally.
        archive_package_exists = CheckFile(os.path.join(GPPKG_ARCHIVE_PATH, self.gppkg.pkg)).run()
        package_already_installed = (not rpm_set) and archive_package_exists
        if package_already_installed:
            raise AlreadyInstalledError(self.gppkg.pkg)

        # Code path 1 (See docstring)
        return rpm_set


class ValidateUninstallDebPackage(Operation):
    """
     A replicate class of ValidateUninstallPackage that used for pre-uninstall deb package for Ubuntu.
     We do not need to test dependencies in GPDB 6 as there is no GPDB's dependencies anymore.

    """

    def __init__(self, gppkg):
        self.gppkg = gppkg

    def execute(self):
        deb_list = [self.gppkg.main_rpm] + self.gppkg.dependencies

        def strip_extension_and_arch(filename):
            # expecting filename of form %{name}-%{version}-%{release}.%{arch}.rpm
            rest = str.split(filename, '-')
            return rest[-3]

        # We check the installed list only
        deb_set = set([strip_extension_and_arch(deb_package) for deb_package in deb_list])
        archive_package_exists = CheckFile(os.path.join(GPPKG_ARCHIVE_PATH, self.gppkg.pkg)).run()
        package_not_installed = (not deb_set) and (not archive_package_exists)
        if package_not_installed:
            raise NotInstalledError(self.gppkg.pkg)

        return deb_set


class ValidateUninstallPackage(Operation):
    """
    Ensure that the given rpms can be uninstalled safely. This is accomplished mainly
    through use of rpm --test, which will have one of a few outcomes:
    1) A return code of 0, indicating the uninstallation should proceed smoothly
    2) A non-zero return code, and stderr indicating some of the rpms are already uninstalled.
       We simply omit such rpms from the returned list of rpms, indicating to the caller
       that to be successful, uninstallation should only be attempted on the filtered list of rpms.
    3) A non-zero return code, and stderr indicating that dependencies remain.

    Lastly, for an edge case, if we determine that none of the relevant rpms are currently installed
    *and* the archive package does not exist, we declare the package is not installed.

    TODO: Use regexes for more reliable string matching.
    """

    def __init__(self, gppkg):
        self.gppkg = gppkg

    def execute(self):
        rpm_list = [self.gppkg.main_rpm] + self.gppkg.dependencies

        def strip_extension_and_arch(filename):
            # expecting filename of form %{name}-%{version}-%{release}.%{arch}.rpm
            rest, ext = os.path.splitext(filename)
            rest, arch = os.path.splitext(rest)
            return rest

        rpm_set = set([strip_extension_and_arch(rpm) for rpm in rpm_list])
        rpm_uninstall_string = ' '.join(rpm_set)
        rpm_uninstall_command = 'rpm --test -e %s --dbpath %s' % (rpm_uninstall_string, RPM_DATABASE)
        cmd = Command('Validating rpm uninstallation', rpm_uninstall_command)
        logger.info(cmd)  # TODO: AK: This should be debug(), but RMI cannot propagate a log level.

        try:
            cmd.run(validateAfter=True)
        except ExecutionError, e:
            lines = e.cmd.get_results().stderr.splitlines()

            # Forking between code paths 2 and 3 depends on some meaningful stderr
            # Without such stderr, we must bubble up the ExecutionError.
            if len(lines) == 0:
                raise

            if 'failed dependencies' in lines[0].lower():
                # Code path 3 (see docstring)
                # example stderr:
                # error: Failed dependencies:
                #    jre = 1.6.0_26 is needed by (installed) gphdfs-1.1-1.x86_64
                self.resolve_shared_dependencies(rpm_set, lines[1:])
            else:
                # Code path 2, possibly (see docstring)
                # example stderr:
                #   error: package postgis-1.0-1.x86_64 is not installed
                #   error: package proj-4.7.0-1.x86_64 is not installed
                #   error: package geos-3.2.2-1.x86_64 is not installed
                for line in lines:
                    if 'not installed' in line.lower():
                        package_name = line.split()[2]
                        rpm_set.remove(package_name)
                    else:
                        # This is unexpected, so bubble up the ExecutionError.
                        raise

        # MPP-14359 - installation and uninstallation prechecks must also consider
        # the archive. That is, if a partial uninstallation had removed all rpms
        # but failed to remove the archive package, then for our purposes, we consider
        # the package installed and still in need of UninstallPackageLocally.
        archive_package_exists = CheckFile(os.path.join(GPPKG_ARCHIVE_PATH, self.gppkg.pkg)).run()
        package_not_installed = (not rpm_set) and (not archive_package_exists)
        if package_not_installed:
            raise NotInstalledError(self.gppkg.pkg)

        # Code path 1 (See docstring)
        return rpm_set

    def resolve_shared_dependencies(self, rpm_set, dependency_lines):
        """
        This is a very naive resolution to shared dependencies. (See code path #3 in ValidateUninstallPackage.execute)

        Among the rpms we attempt to remove from the system, a subset cannot be
        removed during this particular gppkg uninstallation, because their removal would violate
        the dependency constraints of other rpms that remain in the system; we simply leave these culprit rpm(s) behind.
        More specifically, the preceding rpm --test -e command has given us the violated *capabilities*. For each *capability*,
        we query the rpm database with --whatprovides to discern the culprit rpm(s).

        In simpler terms, consider this example:
            pljava depends on jre, which its gppkg contains
            install the gppkgs for pljava
            uninstall pljava gppkg
            we determine that the jre rpm is responsible for *providing* "jre = 1.6"
            so, we ultimately omit the jre rpm from our "rpm -e" and move on

        TODO: AK: A more robust version of this function would ensure that the remaining
        rpms are, in fact, bound by a remaining gppkg.  We defer this responsibility for now because gppkgs
        should not have external dependencies. That is, no package should have requirements on rpms
        not contained in its own gppkg distro. So, it's safe to assume that if foo is a culprit rpm, there exists
        some gppkg bar that internally contains foo. (I realize that, with time, this will not be a scalable requirement
        for gppkgs... hence the TODO.)

        @type  rpm_set: set
        @param rpm_set: rpms being uninstalled, among which there exists an rpm
                        whose removal violates the dependencies of remaining rpms
        @type  dependency_lines: list
        @param dependency_lines: lines produced from the stderr in
                                 code path #3 in ValidateUninstallPackage.execute
                                 ex: ["     jre >= 1.6.0_26 is needed by (installed) gphdfs-1.1-1.x86_64"]
        """
        for dependency_line in dependency_lines:
            violated_capability = dependency_line.split()[0]  # e.g. "jre"
            cmd = Command('Discerning culprit rpms for %s' % violated_capability,
                          'rpm -q --whatprovides %s --dbpath %s' % (violated_capability, RPM_DATABASE))
            cmd.run(validateAfter=True)
            culprit_rpms = set(cmd.get_results().stdout.splitlines())
            rpm_set -= culprit_rpms


class ExtractPackage(Operation):
    """
    Extract the contents of the package into the temp folder

    TODO: AK: Extraction should be implemented as a context manager.
    """

    def __init__(self, gppkg):
        self.gppkg = gppkg

    def execute(self):
        # clean up tmp extraction folder
        if os.path.exists(TEMP_EXTRACTION_PATH) and not CleanupDir(TEMP_EXTRACTION_PATH).run():
            logger.error('Could not clean temp folder')
            raise IOError

        # untar the package into tmp folder
        with closing(tarfile.open(self.gppkg.abspath)) as tarinfo:
            tarinfo.extractall(TEMP_EXTRACTION_PATH)

        # move all the deps into same folder as the main rpm
        path = os.path.join(TEMP_EXTRACTION_PATH, DEPS_DIR)
        if os.path.exists(path):
            for cur_file in os.listdir(path):
                shutil.move(os.path.join(TEMP_EXTRACTION_PATH, DEPS_DIR, cur_file), TEMP_EXTRACTION_PATH)


class InstallDebPackageLocally(Operation):
    """
    For deb package
    """
    def __init__(self, package_path, is_update=False):
        self.package_path = package_path
        self.is_update = is_update

    def execute(self):
        current_package_location = self.package_path
        package_name = os.path.basename(current_package_location)
        logger.info('Installing %s locally' % package_name)
        final_package_location = os.path.join(GPPKG_ARCHIVE_PATH, package_name)

        gppkg = Gppkg.from_package_path(current_package_location)
        ExtractPackage(gppkg).run()

        # squash AlreadyInstalledError here: the caller doesn't ever need to
        # know that we didn't have to do anything here
        try:
            deb_set = ValidateInstallDebPackage(gppkg, is_update=self.is_update).run()
        except AlreadyInstalledError, e:
            logger.info(e)
            return

        if deb_set:
            deb_packages = " ".join([os.path.join(TEMP_EXTRACTION_PATH, deb_package) for deb_package in deb_set])
            if self.is_update:
                deb_install_command = 'fakeroot dpkg --force-not-root --force-depends --log=/dev/null --admindir=%s ' \
                                      '--instdir=%s -i %s' % (DEB_DATABASE, GPHOME, deb_packages)
            else:
                deb_install_command = 'fakeroot dpkg --force-not-root --log=/dev/null --admindir=%s --instdir=%s -i %s' % \
                                      (DEB_DATABASE, GPHOME, deb_packages)

            cmd = Command('Installing debs', deb_install_command)
            logger.info(cmd)
            cmd.run(validateAfter=True)

        MakeDir(GPPKG_ARCHIVE_PATH).run()
        shutil.copy(current_package_location, final_package_location)
        logger.info("Completed local installation of %s." % package_name)


class InstallPackageLocally(Operation):
    """
    Installs a package on the local host

    This operation must take a slew of starting conditions and drive the state
    of the local machine towards the ending state, in which the given package is successfully
    installed, the rpm database is sane, and the package resides in the designated archive.
    To that end, we indiscriminately squash AlreadyInstalledErrors arising from ValidateInstallPackage,
    because in this context, it's not an exception, but rather an indication of our desired ending
    conditions.

    We must consider the following scenarios and more: package was deleted from archive,
    the main comprising rpm was uninstalled, dependent rpms were removed, the rpm database was
    corrupted, etc.

    Again, much like ValidateInstallPackages, we make cheap reuse of this code for the purposes
    of an --update as there is considerable commonality.
    """

    def __init__(self, package_path, is_update=False):
        self.package_path = package_path
        self.is_update = is_update

    def execute(self):
        current_package_location = self.package_path
        package_name = os.path.basename(current_package_location)
        logger.info('Installing %s locally' % package_name)
        final_package_location = os.path.join(GPPKG_ARCHIVE_PATH, package_name)

        gppkg = Gppkg.from_package_path(current_package_location)
        ExtractPackage(gppkg).run()

        # squash AlreadyInstalledError here: the caller doesn't ever need to
        # know that we didn't have to do anything here
        try:
            rpm_set = ValidateInstallPackage(gppkg, is_update=self.is_update).run()
        except AlreadyInstalledError, e:
            logger.info(e)
            return

        if rpm_set:
            if self.is_update:
                rpm_install_command = 'rpm -U --force %s --dbpath %s --prefix=%s'
            else:
                rpm_install_command = 'rpm -i %s --dbpath %s --prefix=%s'
            rpm_install_command = rpm_install_command % \
                                  (" ".join([os.path.join(TEMP_EXTRACTION_PATH, rpm) for rpm in rpm_set]),
                                   RPM_DATABASE,
                                   RPM_INSTALLATION_PATH)
            cmd = Command('Installing rpms', rpm_install_command)
            logger.info(cmd)
            cmd.run(validateAfter=True)

        # TODO: AK: MPP-15568
        # TODO: AK: abstraction layer for archive interactions... to hide use of shutil.copy, RemoveFile, etc.
        MakeDir(GPPKG_ARCHIVE_PATH).run()
        shutil.copy(current_package_location, final_package_location)
        logger.info("Completed local installation of %s." % package_name)


class UninstallDebPackageLocally(Operation):
    """
     A replicate class of ValidateUninstallPackage that used for uninstall deb package for Ubuntu.
     Same logic as UninstallPackageLocally

    """

    def __init__(self, package_name):
        self.package_name = package_name

    def execute(self):
        current_package_location = os.path.join(GPPKG_ARCHIVE_PATH, self.package_name)
        gppkg = Gppkg.from_package_path(current_package_location)

        # squash NotInstalledError here: the caller doesn't ever need to
        # know that we didn't have to do anything here
        try:
            deb_set = ValidateUninstallDebPackage(gppkg).run()
        except NotInstalledError, e:
            logger.info(e)
            return

        if deb_set:
            deb_uninstall_command = 'fakeroot dpkg --force-not-root --log=/dev/null --admindir=%s --instdir=%s -r %s' % (
                DEB_DATABASE, GPHOME, " ".join(deb_set))
            cmd = Command('Uninstalling debs', deb_uninstall_command)
            logger.info(cmd)
            cmd.run(validateAfter=True)

        MakeDir(GPPKG_ARCHIVE_PATH).run()
        RemoveFile(current_package_location).run()
        logger.info("Completed local uninstallation of %s." % self.package_name)


class UninstallPackageLocally(Operation):
    """
    Uninstalls a package on the local host

    This operation must take a slew of starting conditions and drive the state
    of the local machine towards the ending state, in which the given package is successfully
    uninstalled, the rpm database is sane, and the package is removed from the archive.
    To that end, we indiscriminately squash NotInstalledErrors arising from ValidateUninstallPackage,
    because in this context, it's not an exception, but rather an indication of our desired ending
    conditions.

    We must consider the following scenarios and more: package was deleted from archive,
    the main comprising rpm was uninstalled, dependent rpms were removed, the rpm database was
    corrupted, etc.
    """

    def __init__(self, package_name):
        self.package_name = package_name

    def execute(self):
        # TODO: AK: MPP-15737 - we're entirely dependent on the package residing in the archive
        current_package_location = os.path.join(GPPKG_ARCHIVE_PATH, self.package_name)
        gppkg = Gppkg.from_package_path(current_package_location)

        # squash NotInstalledError here: the caller doesn't ever need to
        # know that we didn't have to do anything here
        try:
            rpm_set = ValidateUninstallPackage(gppkg).run()
        except NotInstalledError, e:
            logger.info(e)
            return

        if rpm_set:
            rpm_uninstall_command = 'rpm -e %s --dbpath %s' % (" ".join(rpm_set), RPM_DATABASE)
            cmd = Command('Uninstalling rpms', rpm_uninstall_command)
            logger.info(cmd)
            cmd.run(validateAfter=True)

        # TODO: AK: abstraction layer for archive interactions... to hide use of shutil.copy, RemoveFile, etc.
        MakeDir(GPPKG_ARCHIVE_PATH).run()
        RemoveFile(current_package_location).run()
        logger.info("Completed local uninstallation of %s." % self.package_name)


class SyncPackages(Operation):
    """
    Synchronizes packages from master to a remote host

    TODO: AK: MPP-15568
    """

    def __init__(self, host):
        self.host = host

    def execute(self):
        if not CheckDir(GPPKG_ARCHIVE_PATH).run():
            MakeDir(GPPKG_ARCHIVE_PATH).run()
        if not CheckRemoteDir(GPPKG_ARCHIVE_PATH, self.host).run():
            MakeRemoteDir(GPPKG_ARCHIVE_PATH, self.host).run()

        # set of packages on the master
        master_package_set = set(ListFilesByPattern(GPPKG_ARCHIVE_PATH, '*' + GPPKG_EXTENSION).run())
        # set of packages on the remote host
        remote_package_set = set(ListRemoteFilesByPattern(GPPKG_ARCHIVE_PATH, '*' + GPPKG_EXTENSION, self.host).run())
        # packages to be uninstalled on the remote host
        uninstall_package_set = remote_package_set - master_package_set
        # packages to be installed on the remote host
        install_package_set = master_package_set - remote_package_set

        if not install_package_set and not uninstall_package_set:
            logger.info('The packages on %s are consistent.' % self.host)
            return

        if install_package_set:
            logger.info(
                'The following packages will be installed on %s: %s' % (self.host, ', '.join(install_package_set)))
            for package in install_package_set:
                logger.debug('copying %s to %s' % (package, self.host))
                dstFile = os.path.join(GPHOME, package)
                Scp(name='copying %s to %s' % (package, self.host),
                    srcFile=os.path.join(GPPKG_ARCHIVE_PATH, package),
                    dstFile=dstFile,
                    dstHost=self.host).run(validateAfter=True)
                if platform.linux_distribution()[0] == 'Ubuntu':
                    RemoteOperation(InstallDebPackageLocally(dstFile), self.host).run()
                else:
                    RemoteOperation(InstallPackageLocally(dstFile), self.host).run()
                RemoveRemoteFile(dstFile, self.host).run()

        if uninstall_package_set:
            logger.info(
                'The following packages will be uninstalled on %s: %s' % (self.host, ', '.join(uninstall_package_set)))
            for package in uninstall_package_set:
                if platform.linux_distribution()[0] == 'Ubuntu':
                    RemoteOperation(UninstallDebPackageLocally(package), self.host).run()
                else:
                    RemoteOperation(UninstallPackageLocally(package), self.host).run()


class InstallPackage(Operation):
    def __init__(self, gppkg, master_host, standby_host, segment_host_list):
        self.gppkg = gppkg
        self.master_host = master_host
        if master_host != standby_host:
            self.standby_host = standby_host
        else:
            self.standby_host = []
        self.segment_host_list = segment_host_list

    def execute(self):
        logger.info('Installing package %s' % self.gppkg.pkg)

        # TODO: AK: MPP-15736 - precheck package state on master
        ExtractPackage(self.gppkg).run()
        if platform.linux_distribution()[0] == 'Ubuntu':
            ValidateInstallDebPackage(self.gppkg).run()
        else:
            ValidateInstallPackage(self.gppkg).run()

        # perform any pre-installation steps
        PerformHooks(hooks=self.gppkg.preinstall,
                     master_host=self.master_host,
                     standby_host=self.standby_host,
                     segment_host_list=self.segment_host_list).run()

        # distribute package to segments
        srcFile = self.gppkg.abspath
        dstFile = os.path.join(GPHOME, self.gppkg.pkg)

        if platform.linux_distribution()[0] == 'Ubuntu':
            # install package on segments
            if self.segment_host_list:
                GpScp(srcFile, dstFile, self.segment_host_list).run()
                HostOperation(InstallDebPackageLocally(dstFile), self.segment_host_list).run()

            # install package on standby
            if self.standby_host:
                Scp(name='copying %s to %s' % (srcFile, self.standby_host),
                    srcFile=srcFile,
                    dstFile=dstFile,
                    dstHost=self.standby_host).run(validateAfter=True)
                RemoteOperation(InstallDebPackageLocally(dstFile), self.standby_host).run()

            # install package on master
            InstallDebPackageLocally(srcFile).run()
        else:
            # install package on segments
            if self.segment_host_list:
                GpScp(srcFile, dstFile, self.segment_host_list).run()
                HostOperation(InstallPackageLocally(dstFile), self.segment_host_list).run()

            # install package on standby
            if self.standby_host:
                Scp(name='copying %s to %s' % (srcFile, self.standby_host),
                    srcFile=srcFile,
                    dstFile=dstFile,
                    dstHost=self.standby_host).run(validateAfter=True)
                RemoteOperation(InstallPackageLocally(dstFile), self.standby_host).run()

            # install package on master
            InstallPackageLocally(srcFile).run()

        # perform any post-installation steps
        PerformHooks(hooks=self.gppkg.postinstall,
                     master_host=self.master_host,
                     standby_host=self.standby_host,
                     segment_host_list=self.segment_host_list).run()

        logger.info('%s successfully installed.' % (self.gppkg.pkg))


class PerformHooks(Operation):
    def __init__(self, hooks, master_host, standby_host, segment_host_list):
        """
        Performs steps that have been specified in the yaml file for a particular
        stage of gppkg execution

        TODO: AK: A packager may have added commands to their hooks, with the
        assumption that the current working directory would be that which contains
        the spec file, rpms, and other artifacts (external scripts, perhaps.) To support
        this, these commands should be prefixed with a "cd".

        TODO: AK: I'm adding master_host for consistency.
        But, why would we ever need master_host?  We're on the master host!
        """
        self.hooks = hooks
        self.master_host = master_host
        if master_host != standby_host:
            self.standby_host = standby_host
        else:
            self.standby_host = []
        self.segment_host_list = segment_host_list

    def execute(self):
        if self.hooks is None:
            return
        for hook in self.hooks:
            key = hook.keys()
            if key is None:
                return
            key_str = key[0]
            if key_str.lower() == 'master':
                if self.standby_host:
                    RemoteCommand(hook[key_str], [self.standby_host]).run()
                LocalCommand(hook[key_str], True).run()
            elif key_str.lower() == 'segment':
                RemoteCommand(hook[key_str], self.segment_host_list).run()
            elif key_str.lower() == 'all':
                if self.standby_host:
                    RemoteCommand(hook[key_str], [self.standby_host]).run()
                # Change on Master
                LocalCommand(hook[key_str], True).run()
                # Change on Segment hosts
                RemoteCommand(hook[key_str], self.segment_host_list).run()



class UninstallPackage(Operation):
    def __init__(self, gppkg, master_host, standby_host, segment_host_list):
        self.gppkg = gppkg
        self.master_host = master_host
        if master_host != standby_host:
            self.standby_host = standby_host
        else:
            self.standby_host = []
        self.segment_host_list = segment_host_list

    def execute(self):
        logger.info('Uninstalling package %s' % self.gppkg.pkg)

        # TODO: AK: MPP-15736 - precheck package state on master
        ExtractPackage(self.gppkg).run()

        if platform.linux_distribution()[0] == 'Ubuntu':
            ValidateUninstallDebPackage(self.gppkg).run()
        else:
            ValidateUninstallPackage(self.gppkg).run()

        # perform any pre-uninstallation steps
        PerformHooks(hooks=self.gppkg.preuninstall,
                     master_host=self.master_host,
                     standby_host=self.standby_host,
                     segment_host_list=self.segment_host_list).run()

        # uninstall on segments
        if platform.linux_distribution()[0] == 'Ubuntu':
            HostOperation(UninstallDebPackageLocally(self.gppkg.pkg), self.segment_host_list).run()

            if self.standby_host:
                RemoteOperation(UninstallDebPackageLocally(self.gppkg.pkg), self.standby_host).run()

            UninstallDebPackageLocally(self.gppkg.pkg).run()

        else:
            HostOperation(UninstallPackageLocally(self.gppkg.pkg), self.segment_host_list).run()

            if self.standby_host:
                RemoteOperation(UninstallPackageLocally(self.gppkg.pkg), self.standby_host).run()

            UninstallPackageLocally(self.gppkg.pkg).run()

        # perform any post-installation steps
        PerformHooks(hooks=self.gppkg.postuninstall,
                     master_host=self.master_host,
                     standby_host=self.standby_host,
                     segment_host_list=self.segment_host_list).run()

        logger.info('%s successfully uninstalled.' % self.gppkg.pkg)


class QueryPackage(Operation):
    INFO, LIST, ALL = range(3)

    def __init__(self, query_type, package_path):
        self.query_type = query_type
        self.package_path = package_path

    def execute(self):
        if self.query_type == QueryPackage.INFO:
            def package_details(p):
                yield 'Name', p.pkgname
                yield 'Version', p.version
                yield 'Architecture', p.architecture
                yield 'OS', p.os
                yield 'GPDBVersion', str(p.gpdbversion)
                yield 'Description', p.description

            def print_package_info(package):
                tabLog = TableLogger()
                for name, value in package_details(package):
                    tabLog.info([name, value])
                tabLog.outputTable()

            package = Gppkg.from_package_path(self.package_path)
            print_package_info(package)

        elif self.query_type == QueryPackage.LIST:
            package = Gppkg.from_package_path(self.package_path)
            for file in package.file_list:
                print file
        elif self.query_type == QueryPackage.ALL:
            package_name_list = ListPackages().run()
            for package_name in package_name_list:
                print package_name
        else:
            package = Gppkg.from_package_path(self.package_path)
            try:
                ExtractPackage(package).run()
                ValidateInstallPackage(package).run()
            except AlreadyInstalledError:
                print '%s is installed.' % package.pkgname
            else:
                print '%s is not installed.' % package.pkgname


class BuildGppkg(Operation):
    """
        Builds a gppkg given a directory containing
        the spec file, rpms and any pre/post installation scripts
    """

    def __init__(self, directory, filename):
        self.directory = directory
        self.filename = filename;

    def execute(self):

        directory = self.directory

        logger.info('Building gppkg')

        # Check if the directory is valid
        if not os.path.exists(directory) or not os.path.isdir(directory):
            logger.error('%s is an Invalid directory' % directory)
            raise BuildPkgError

        filelist = os.listdir(directory)

        # Check for the spec file
        specfile = directory + '/' + SPECFILE_NAME

        if not os.path.exists(specfile):
            logger.error(' Spec file does not exist')
            raise BuildPkgError

        # parse the spec file and get the name, version and arch
        # this is used to name the gppkg
        pkg_path_details = self._get_package_name_details(specfile)

        if pkg_path_details is None:
            raise BuildPkgError

        # The file already exists. Rewrite the original with the new one
        if self.filename is None:
            pkg = pkg_path_details['pkgname'] + '-' + str(pkg_path_details['version']) + '-' + pkg_path_details[
                'os'] + '-' + pkg_path_details['architecture'] + GPPKG_EXTENSION
        else:
            pkg = self.filename
        if os.path.exists(pkg):
            os.remove(pkg)

        # Verify the spec file
        if not self._verify_specfile(specfile, directory):
            raise BuildPkgError

        # tar and gzip the directory
        # rename the file with .gppkg extension
        with closing(tarfile.open(pkg, 'w:gz')) as tarinfo:
            for cur_file in filelist:
                tarinfo.add(name=os.path.join(directory, cur_file),
                            arcname=cur_file)

        logger.info('Completed building gppkg')

    def _get_package_name_details(self, specfile):
        """
            Get details about the name, version, operating system, architecture
            of the package. The final gppkg which will be created
            will be named as <name>-<version>-<os>-<arch>.gppkg
        """

        logger.debug('_get_package_name_details')
        cur_file = None

        with open(specfile) as cur_file:
            yamlfile = yaml.load(cur_file)

            tags = yamlfile.keys()

            pkg_path_details = {}

            # return all the required tags as a dict
            for tag in tags:
                if tag.lower() in SPECFILE_REQUIRED_TAGS:
                    pkg_path_details[tag.lower()] = yamlfile[tag]

            return pkg_path_details

    def _verify_specfile(self, specfile, directory):
        '''
            Reads the spec file and makes sure that the tags are correct.
        '''

        logger.debug('_verify_specfile')
        cur_file = None

        try:
            with open(specfile) as cur_file:
                yamlfile = yaml.load(cur_file)

                if not self._verify_tags(yamlfile):
                    return False

                return True
        except ScannerError, ex:
            return False

    def _verify_tags(self, yamlfile):
        """
            Verify that the tags are valid.
            Returns true if all tags are valid
            False otherwise
        """

        logger.debug('_verify_tags')
        tags = yamlfile.keys()

        tags = [tag.lower() for tag in tags]

        # check required tags
        for required_tag in SPECFILE_REQUIRED_TAGS:
            if required_tag not in tags:
                logger.error(' Required tag %s missing in Spec file' % required_tag)
                return False

        # check for invalid tags
        for tag in tags:
            if tag not in SPECFILE_OPTIONAL_TAGS and tag not in SPECFILE_REQUIRED_TAGS:
                logger.error(' Invalid tag %s in Spec file' % tag)
                return False

        return True


class UpdatePackage(Operation):
    """ TODO: AK: Enforce gppkg version is higher than currently installed version """

    def __init__(self, gppkg, master_host, standby_host, segment_host_list):
        self.gppkg = gppkg
        self.master_host = master_host
        if master_host != standby_host:
            self.standby_host = standby_host
        else:
            self.standby_host = []
        self.segment_host_list = segment_host_list

    def execute(self):
        logger.info('Updating package %s' % self.gppkg.pkg)

        ExtractPackage(self.gppkg).run()
        if platform.linux_distribution()[0] == 'Ubuntu':
            ValidateInstallDebPackage(self.gppkg, is_update=True).run()
        else:
            ValidateInstallPackage(self.gppkg, is_update=True).run()

        # distribute package to segments
        srcFile = self.gppkg.abspath
        dstFile = os.path.join(GPHOME, self.gppkg.pkg)
        GpScp(srcFile, dstFile, self.segment_host_list).run()

        # update package on segments
        HostOperation(UpdatePackageLocally(dstFile), self.segment_host_list).run()

        # update package on standby
        if self.standby_host:
            Scp(name='copying %s to %s' % (srcFile, self.standby_host),
                srcFile=srcFile,
                dstFile=dstFile,
                dstHost=self.standby_host).run(validateAfter=True)
            RemoteOperation(UpdatePackageLocally(dstFile), self.standby_host).run()

        # update package on master
        UpdatePackageLocally(srcFile).run()

        # perform any post-update steps
        PerformHooks(hooks=self.gppkg.postupdate,
                     master_host=self.master_host,
                     standby_host=self.standby_host,
                     segment_host_list=self.segment_host_list).run()

        logger.info('%s successfully updated.' % (self.gppkg.pkg))


class UpdatePackageLocally(Operation):
    """
    Updates a package on the local host

    We make cheap reuse of InstallPackageLocally with the propagation of is_update = True, which
    effectively changes the rpm --test command to use -U instead of -i. Beyond the invocation of
    InstallPackageLocally, here, we also clean up the archive directory to remove other (ideally, older)
    versions of the updated package.
    """

    def __init__(self, package_path):
        self.package_path = package_path

    def execute(self):
        if platform.linux_distribution()[0] == 'Ubuntu':
            InstallDebPackageLocally(self.package_path, is_update=True).run()
        else:
            InstallPackageLocally(self.package_path, is_update=True).run()

        # Remove other versions of the package from archive.
        # Note: Do not rely on filename format to discern such packages.
        # Rather, interrogate a package only through the Gppkg class interface.
        current_package = Gppkg.from_package_path(self.package_path)
        MakeDir(GPPKG_ARCHIVE_PATH).run()
        archived_package_paths = ListFiles(GPPKG_ARCHIVE_PATH).run()
        for archived_package_path in archived_package_paths:
            temp_package = Gppkg.from_package_path(os.path.join(GPPKG_ARCHIVE_PATH, archived_package_path))
            if temp_package.pkgname == current_package.pkgname and temp_package.version != current_package.version:
                RemoveFile(os.path.join(GPPKG_ARCHIVE_PATH, archived_package_path)).run()


class CleanGppkg(Operation):
    """
        Cleans up the Gppkg from the cluster in case of partial
        installation or removal. This might not be required if
        we can make the install and uninstall options idempotent.
        This operation is exactly the same as remove but we dont
        check on each host to see if the rpm is installed or not.
    """

    def __init__(self, standby_host, segment_host_list):
        self.standby_host = standby_host
        self.segment_host_list = segment_host_list

    def execute(self):
        operations = [SyncPackages(host) for host in self.segment_host_list]

        if self.standby_host:
            operations.append(SyncPackages(self.standby_host))

        ParallelOperation(operations).run()

        err_msgs = 'SyncPackages failed:'
        exceptions = ""
        for operation in operations:
            try:
                operation.get_ret()
            except Exception, e:
                exceptions += '\n'+str(e)

        if exceptions:
            raise ExceptionNoStackTraceNeeded("%s%s" % (err_msgs, exceptions))

        logger.info('Successfully cleaned the cluster')


class MigratePackages(Operation):
    """
    Migrates packages from another $GPHOME to this one

    This functionality is meant to facilitate minor version upgrade, whereby old packages
    need to be brought over from the older $GPHOME to the newer $GPHOME.

    Presumably, this could also be used to migrate packages across arbitrary choices
    of $GPHOMEs. However, the migration will only succeed if the packages being migrated
    are actually compatible with the target GPDB.
    """

    def __init__(self, from_gphome, to_gphome, standby_host, segment_host_list):
        self.from_gphome = from_gphome
        self.to_gphome = to_gphome
        self.standby_host = standby_host
        self.segment_host_list = segment_host_list

    def execute(self):
        if not os.path.samefile(self.to_gphome, GPHOME):
            raise ExceptionNoStackTraceNeeded(
                'The target GPHOME, %s, must match the current $GPHOME used to launch gppkg.' % self.to_gphome)
        if os.path.samefile(self.to_gphome, self.from_gphome):
            raise ExceptionNoStackTraceNeeded(
                'The source and target GPHOMEs, %s => %s, must differ for packages to be migrated.' % (
                self.from_gphome, self.to_gphome))

        # TODO: AK: Given an invalid from_gphome, we'll end up creating a 'share/packages' subdirectory within it.
        old_archive_path = os.path.join(self.from_gphome, ARCHIVE_PATH)
        MakeDir(old_archive_path).run()
        packages = ListFilesByPattern(old_archive_path, '*' + GPPKG_EXTENSION).run()
        if not packages:
            logger.info('There are no packages to migrate from %s.' % self.from_gphome)
            return

        logger.info('The following packages will be migrated: %s' % ', '.join(packages))
        for package in packages:
            package_path = os.path.join(old_archive_path, package)
            try:
                if platform.linux_distribution()[0] == 'Ubuntu':
                    InstallDebPackageLocally(package_path).run()
                else:
                    InstallPackageLocally(package_path).run()
            except AlreadyInstalledError:
                logger.info("%s is already installed." % package)
            except Exception:
                logger.exception("Failed to migrate %s from %s" % (old_archive_path, package))

        CleanGppkg(self.standby_host, self.segment_host_list).run()

        logger.info('The package migration has completed.')


class GpScp(Operation):
    """
    TODO: AK: This obviously does not belong here. My preference would be that it remain here until
    the following problem is solved.

    MPP-15270 - Improve performance of file transfer across large clusters

    I suggest:

        We consume an extra parameter 'fanout'. We partition the host_list into a number of buckets
        given by 'fanout'. For each bucket, we scp the artifact to the first host in the bucket, and then
        we recursively invoke GpScp on that machine for the remaining hosts in its bucket.

        GpScp := ParallelOperation([ A(i) for i in range(0, n) ])
        A := SerialOperation(B, C)
        B := scp source_path target_path @ host_i
            where host_i := the first host in the ith bucket
        C := RemoteOperation(GpScp(target_path, target_path, host_list_i))
            where host_list_i := the remaining hosts in the ith bucket
    """

    def __init__(self, source_path, target_path, host_list):
        self.source_path = source_path
        self.target_path = target_path
        self.host_list = host_list
        self.pool = None

    def execute(self):
        self.pool = WorkerPool()
        for host in self.host_list:
            self.pool.addCommand(Scp(name='copying %s to %s' % (self.source_path, host),
                                     srcFile=self.source_path,
                                     dstFile=self.target_path,
                                     dstHost=host))
        self.pool.join()


class HostOperation(Operation):
    """
    TODO: AK: This obviously does not belong here. My preference would be to move it to gppylib.operations.utils
    when another consumer becomes clear.

    TODO: AK: For generality, the underlying operation should inherit/implement NestedHostOperation so that
    it may be initialized with information about the host to which it's been bound. This is fortunately not necessary
    for our purposes here, so it's deferrable.

    TODO: AK: Build a SegHostOperation that wraps this and is driven by GpArray content.

    TODO: AK: Implement something similar for a SegmentOperation + NestedSegmentOperation.

    TODO: AK: This (as well as ParallelOperation) would benefit from an appropriate choice of return value. The likely
    choice would be: [op.get_ret() for op in self.operations]
    """

    def __init__(self, operation, host_list):
        self.operation = operation
        self.host_list = host_list

    def execute(self):
        operations = []
        for host in self.host_list:
            operations.append(RemoteOperation(self.operation, host))
        ParallelOperation(operations).run()
        for operation in operations:
            operation.get_ret()
