import os
import shutil
import unittest
import tempfile
import getpass
import tarfile

from gppylib.db import dbconn
from gppylib.gparray import GpArray
from gppylib.gpversion import MAIN_VERSION
from contextlib import closing
from gppylib.commands import gp
from gppylib.commands.unix import Rsync
from gppylib.commands.base import Command, ExecutionError, REMOTE
from gppylib.operations import Operation
from gppylib.operations.unix import CheckFile, CheckRemoteFile, RemoveRemoteFile
from gppylib.operations.package import dereference_symlink, GpRsync, linux_distribution_id, linux_distribution_version
from gppylib.commands.base import Command, REMOTE

def get_os():
    dist, release = linux_distribution_id(), linux_distribution_version()
    major_release = release.partition('.')[0]

    os_string = ''
    if dist.lower().startswith('redhat'):
        os_string += 'rhel'
    elif dist.lower().startswith('suse'):
        os_string += 'suse'

    os_string += major_release

    return os_string

OS = get_os()

# AK: use dereference_symlink when mucking with RPM database for the same reason
# it's used in the gppylib.operations.package. For more info, see the function definition.
GPHOME = dereference_symlink(gp.get_gphome())

ARCHIVE_PATH = os.path.join(GPHOME, 'share/packages/archive')
RPM_DATABASE = os.path.join(GPHOME, 'share/packages/database')
GPPKG_EXTENSION = ".gppkg"
SCRATCH_SPACE = os.path.join(tempfile.gettempdir(), getpass.getuser())
GPDB_VERSION = '.'.join([str(n) for n in MAIN_VERSION[:2]])
COORDINATOR_PORT = os.getenv("PGPORT")

def skipIfNoStandby():
    """
    A decorator which skips a unit test if a standby
    is not already present in the cluster.
    """
    standby = get_host_list()[0]
    if standby is None:
        return unittest.skip('requires standby')
    return lambda o: o

def skipIfSingleNode():
    """
    Skip a test if its a single node install.
    """
    if len(get_host_list()[1]) == 0:
        return unittest.skip('requires multiple nodes')
    return lambda o: o

def get_host_list():
    """
    Returns a tuple which consists of the standby
    and segment hosts comprising the cluster.

    @return: tuple containing the standby and segment hosts
             tuple[0] contains standby
             tuple[1] contains segment hosts
    """
    gparr = GpArray.initFromCatalog(dbconn.DbURL(port = COORDINATOR_PORT), utility = True)
    segs = gparr.getDbList()

    coordinator = None
    standby_host = None
    segment_host_list = []

    for seg in segs:
        if seg.isSegmentStandby(current_role=True):
            standby_host = seg.getSegmentHostName()
        elif not seg.isSegmentCoordinator(current_role=True):
            segment_host_list.append(seg.getSegmentHostName())
        elif seg.isSegmentCoordinator(current_role=True):
            coordinator = seg.getSegmentHostName()

    #Deduplicate the hosts so that we
    #dont install multiple times on the same host
    segment_host_list = list(set(segment_host_list))
    if coordinator in segment_host_list:
        segment_host_list.remove(coordinator)

    return (standby_host, segment_host_list)

def run_command(cmd_str):
    """
    Runs a command on the localhost

    @param cmd_str: The command string to be executed
    @return: stdout/stderr output as a string
    """
    cmd = Command("Local Command", cmd_str)
    cmd.run(validateAfter = True)
    results = cmd.get_results()

    if results.rc != 0:
        return results.stderr.strip()
    else:
        return results.stdout.strip()

def run_remote_command(cmd_str, host):
    """
    Runs a command on a remote host

    @param cmd_str: The command string to be executed
    @return: stdout/stderr output as a string
    """
    cmd = Command(name = "Remote Command",
                  cmdStr = cmd_str,
                  ctxt = REMOTE,
                  remoteHost = host)
    cmd.run(validateAfter = True)

    results = cmd.get_results()

    if results.rc != 0:
        return results.stderr.strip()
    else:
        return results.stdout.strip()

class GppkgSpec:
    """Represents the gppkg spec file"""
    def __init__(self, name, version, gpdbversion = GPDB_VERSION, os = OS):
        """
        All the parameters require arguments of type string.
        """
        self.name = name
        self.version = version
        self.gpdbversion = gpdbversion
        self.os = os

    def get_package_name(self):
        """Returns the package name of the form <name>-<version>"""
        return self.name + '-' + self.version

    def get_filename(self):
        """Returns the complete filename of the gppkg"""
        return self.get_package_name() + '-' + self.os + '-x86_64' + GPPKG_EXTENSION

    def __str__(self):
        """Returns the GppkgSpec in the form of a string"""
        gppkg_spec_file = '''
PkgName: ''' + self.name + '''
Version: ''' + self.version + '''
GPDBVersion: ''' + self.gpdbversion + '''
Description: Temporary Test Package
OS: ''' + self.os + '''
Architecture: x86_64'''

        return gppkg_spec_file

class BuildGppkg(Operation):
    def __init__(self, gppkg_spec, main_rpm_spec, dependent_rpm_specs = []):
        """
        @param gppkg_spec: The spec file required to build the Gppkg
        @type gppkg_spec: GppkgSpec
        @param main_rpm_spec: The spec file required to build the main rpm
        @type main_rpm_spec: RPMSpec
        @param dependent_rpm_specs: The list of all dependent rpm specs
        @type dependent_rpm_specs: List of RPMSpec
        """
        self.gppkg_spec = gppkg_spec
        self.main_rpm_spec = main_rpm_spec
        self.dependent_rpm_specs = dependent_rpm_specs

    def execute(self):

        if not os.path.exists(SCRATCH_SPACE):
            os.mkdir(SCRATCH_SPACE)

        gppkg_spec_file = str(self.gppkg_spec)
        #create gppkg_dir
        gppkg_dir = os.path.join(SCRATCH_SPACE, "package")
        if not os.path.exists(gppkg_dir):
            os.mkdir(gppkg_dir)

        dependent_rpms = [rpm.get_package_name() for rpm in self.dependent_rpm_specs]
        rpm_file = BuildRPM(self.main_rpm_spec).run()
        shutil.move(rpm_file, gppkg_dir)

        try:
            if self.dependent_rpm_specs != []:
                deps_dir = os.path.join(gppkg_dir, "deps")
                os.mkdir(deps_dir)

                for spec in self.dependent_rpm_specs:
                    rpm_file = BuildRPM(spec).run()
                    shutil.move(rpm_file, deps_dir)

            #create gppkg
            with open(os.path.join(gppkg_dir, "gppkg_spec.yml"), "w") as f:
                f.write(gppkg_spec_file)

            run_command("gppkg --build " + gppkg_dir)
        finally:
            shutil.rmtree(gppkg_dir)
            shutil.rmtree(SCRATCH_SPACE)

        return self.gppkg_spec.get_filename()

class RPMSpec:
    """Represents an RPM spec file used for creating an RPM"""
    def __init__(self, name, version, release, depends = []):
        """
        @param depends: List of dependecies for the rpm
        @type depends: List of strings
        """
        self.name = name
        self.version = version
        self.release = release
        self.depends = depends

    def get_package_name(self):
        """Returns the package name of the form <name>-<version>-<release>"""
        return self.name + '-' + self.version + '-' + self.release

    def get_filename(self):
        """Returns the complete filename of the rpm"""
        return self.get_package_name() + '.x86_64.rpm'

    def __str__(self):
        """Returns the rpm spec file as a string"""
        rpm_spec_file = '''

%define _topdir %(pwd)
%define __os_install_post %{nil}

Summary:        Temporary test package
License:        GPLv2
Name:           ''' + self.name + '''
Version:        ''' + self.version + '''
Release:        ''' + self.release + '''
Group:          Development/Tools
Prefix:         /temp
AutoReq:        no
AutoProv:       no
BuildArch:      x86_64
Provides:       ''' + self.name + ''' = '''+ self.version +''', /bin/sh
BuildRoot:      %{_topdir}/BUILD '''

        if self.depends != []:
            rpm_spec_file += '''
Requires:       ''' + ','.join(self.depends)

        rpm_spec_file += '''
%description
Temporary test package for gppkg.

%install
mkdir -p %{buildroot}/temp
touch %{buildroot}/temp/temp_file

%post

%postun

%files
/temp
'''
        return rpm_spec_file

class BuildRPM(Operation):
    def __init__(self, spec):
        """
        @param spec: The spec file required to build the rpm
        @type spec: RPMSpec
        """
        self.spec = spec

    def execute(self):
        rpm_spec_file = str(self.spec)
        build_dir = os.path.join(SCRATCH_SPACE, "BUILD")
        os.mkdir(build_dir)
        rpms_dir = os.path.join(SCRATCH_SPACE, "RPMS")
        os.mkdir(rpms_dir)

        try:
            with tempfile.NamedTemporaryFile() as f:
                f.write(rpm_spec_file)
                f.flush()

                os.system("cd " + SCRATCH_SPACE + "; rpmbuild --quiet -bb " + f.name)

            shutil.copy(os.path.join(SCRATCH_SPACE, "RPMS", "x86_64", self.spec.get_filename()), os.getcwd())
        finally:
            shutil.rmtree(build_dir)
            shutil.rmtree(rpms_dir)

        return self.spec.get_filename()

class GppkgTestCase(unittest.TestCase):
    """
    Basic gppkg test case from which all the other test cases inherit.

    Provides default RPMSpec and GppkgSpecs for the inherited classes
    to work with.
    Default RPMSpec will have no dependencies, have a version and release 1
    and will be called A
    Default GppkgSpec will have no dependencies, have version 1.0 and will
    be called alpha
    """
    def setUp(self):
        self.cleanup()
        self.A_spec = RPMSpec("A", "1", "1")
        self.alpha_spec = GppkgSpec("alpha", "1.0")

    def tearDown(self):
        self.cleanup()

    @classmethod
    def setUpClass(self):
        self.extra_clean = set()
        self.start_output, self.end_output = (1, None)

    @classmethod
    def tearDownClass(self):
        for gppkg in self.extra_clean:
            os.remove(gppkg)

    def cleanup(self):
        """Cleans up gppkgs that are installed"""
        results = run_command("gppkg -q --all")
        gppkgs = results.split('\n')[self.start_output:self.end_output]   #The first line is 'Starting gppkg with args', which we want to ignore.

        for gppkg in gppkgs:
            run_command("gppkg --remove " + gppkg)

    def build(self, gppkg_spec, rpm_spec, dep_rpm_specs = []):
        """
        Builds a gppkg and checks if the build was successful.
        if the build was successful, returns the gppkg filename

        @param gppkg_spec: The spec file required to build a gppkg
        @type gppkg_spec: GppkgSpec
        @param rpm_spec: The spec file required to build the main rpm
        @type rpm_spec: RPMSpec
        @param dep_rpm_specs: List of all the dependecies of the main rpm
        @type dep_rpm_specs: List of RPMSpecs
        @return: gppkg filename
        @rtype: string
        """
        gppkg_file = BuildGppkg(gppkg_spec, rpm_spec, dep_rpm_specs).run()
        self.assertTrue(self._check_build(gppkg_file, gppkg_spec))
        self.extra_clean.add(gppkg_file)
        return gppkg_file

    def _check_build(self, gppkg_file, gppkg_spec):
        """
        Check if the gppkg build was successful

        @return: True if build was successful
                 False otherwise
        @rtype: bool
        """
        return gppkg_file == gppkg_spec.get_filename()

    def install(self, gppkg_filename):
        """
        Install a given gppkg and checks if the installation was
        successful.

        @param gppkg_filename: The name of the gppkg file
        @type gppkg_filename: str
        """
        run_command("gppkg --install %s" % gppkg_filename)
        self.assertTrue(self.check_install(gppkg_filename))

    def check_install(self, gppkg_filename):
        """
        Checks if a gppkg was installed successfully.

        @param gppkg_filename: The name of the gppkg file
        @type gppkg_filename: str
        @return: True if the gppkg was installed successfully
                 False otherwise
        @rtype: bool
        """
        cmd = "gppkg -q %s" % gppkg_filename
        results = run_command(cmd)
        test_str = ''.join(gppkg_filename.split('-')[:1]) + " is installed"
        is_installed = test_str in results
        return is_installed and CheckFile(os.path.join(ARCHIVE_PATH, gppkg_filename)).run()

    def remove(self, gppkg_filename):
        """
        Uninstalls a gppkg and checks if the uninstallation was
        successful.

        @param gppkg_filename: The name of the gppkg file to be uninstalled
        @type gppkg_filename: str
        """
        gppkg_package_name = gppkg_filename.split('-')[0] + '-' + gppkg_filename.split('-')[1]
        run_command("gppkg --remove %s" % gppkg_package_name)
        self.assertFalse(self.check_install(gppkg_filename))

    def update(self, gppkg_filename):
        """
        Updates a gppkg and checks if the update was
        successful.

        @param gppkg_filename: The name of the gppkg to be updated
        @type gppkg_filename: str
        """
        run_command("gppkg --update %s" % gppkg_filename)
        self.assertTrue(self.check_install(gppkg_filename))

    def remove_timestamp(self, result):
        """
        Removes timestamp from the output produced by gppkg.

        The output produced by gppkg will be of the form
        [timestamp] gppkg:host:user[INFO]:- Some message
        We want to strip out the timestamp and other unwanted
        text such as [INFO], gppkg, host, user etc.

        @param result: The output from stdout/stderr
        @type result: str
        @return: The output with the timestamps and unwanted text removed
        @rtype: str
        """
        result_without_timestamp = []
        results = result.split('\n')[self.start_output:self.end_output]

        for res in results:
            res = res.split(':-')[1].strip()
            res = ' '.join(res.split())
            result_without_timestamp.append(res)

        return result_without_timestamp

    def check_rpm_install(self, rpm_package_name):
        """
        Checks if an rpm has been installed or not.

        @param rpm_package_name: Name of the rpm package of the form <name>-<version>-<release>
        @type rpm_package_name: str
        """
        results = run_command("rpm -q %s --dbpath %s" % (rpm_package_name, RPM_DATABASE))
        self.assertEqual(results, rpm_package_name)

    def check_rpm_uninstall(self, rpm_package_name):
        """
        Checks if an rpm has been uninstalled successfully.

        @param rpm_package_name: Name of rpm package of the form <name>-<version>-<release>
        @type rpm_package_name: str
        """
        with self.assertRaisesRegex(ExecutionError, "%s is not installed" % rpm_package_name):
            run_command("rpm -q %s --dbpath %s" % (rpm_package_name, RPM_DATABASE))

    def check_remote_rpm_install(self, rpm_package_name, host):
        """
        Checks if an rpm has been installed on a remote host

        @param rpm_package_name: Name of rpm package of the form <name>-<version>-<release>
        @type rpm_package_name: str
        @param host: Remote host
        @type host: str
        """
        results = run_remote_command("rpm -q %s --dbpath %s" % (rpm_package_name, RPM_DATABASE), host)
        self.assertEqual(results, rpm_package_name)

    def check_remote_rpm_uninstall(self, rpm_package_name, host):
        """
        Checks if an rpm has been uninstalled on a remote host

        @param rpm_package_name: Name of rpm package of the form <name>-<version>-<release>
        @type rpm_package_name: str
        @param host: Remote host
        @type host: str
        """
        with self.assertRaisesRegex(ExecutionError, "%s is not installed" % rpm_package_name):
            results = run_remote_command("rpm -q %s --dbpath %s" % (rpm_package_name, RPM_DATABASE), host)

    def install_rpm(self, rpm_filename, rpm_database = RPM_DATABASE, installation_prefix = GPHOME):
        """
        Installs a given rpm and checks if the installation was successful.

        @param rpm_filename: Name of the rpm file to be installed
        @type rpm_filename: str
        @param rpm_database: The rpm database against which rpms will be installed
        @type rpm_database: str
        @param installation_prefix: The installation path for the rpm
        @param type installation_prefix: str
        """
        run_command("rpm -i %s --dbpath %s --prefix=%s" % (rpm_filename, rpm_database, installation_prefix))
        rpm_package_name = rpm_filename[:rpm_filename.index('.')]
        self.check_rpm_install(rpm_package_name)

    def uninstall_rpm(self, rpm_filename, rpm_database = RPM_DATABASE):
        """
        UnInstalls a given rpm and checks if the unsintallation was successful

        @param rpm_filename: Name of the rpm file to be uninstalled
        @type rpm_filename: str
        @param rpm_database: The rpm database against which rpms will be uninstalled
        @type rpm_database: str
        """
        rpm_package_name = rpm_filename[:rpm_filename.index('.')]
        run_command("rpm -e %s --dbpath %s" % (rpm_package_name, rpm_database))
        self.check_rpm_uninstall(rpm_package_name)

    def install_rpm_remotely(self, rpm_filename, host, rpm_database = RPM_DATABASE, installation_prefix = GPHOME):
        """
        Installs an rpm on a remote host and checks if the installation was successful

        @param rpm_filename: Name of the rpm file to be installed
        @type rpm_filename: str
        @param host: The remote host on which the rpm will be installed
        @type host: str
        @param rpm_database: The rpm database against which rpms will be installed
        @type rpm_database: str
        @param installation_prefix: The installation path for the rpm
        @type installation_prefix: str
        """
        run_remote_command("rpm -i %s --dbpath %s --prefix=%s" % (rpm_filename, rpm_database, installation_prefix), host)
        rpm_package_name = rpm_filename[:rpm_filename.index('.')]
        self.check_remote_rpm_install(rpm_package_name, host)

    def uninstall_rpm_remotely(self, rpm_filename, host, rpm_database = RPM_DATABASE):
        """
        Uninstalls an rpm on a remote host and checks if uninstallation was successful

        @param rpm_filename: Name of the rpm file to be uninstalled
        @type rpm_filename: str
        @param host: The remote host on which the rpm will be uninstalled
        @type host: str
        @param rpm_database: The rpm database against which rpms will be uninstalled
        @type rpm_database: str
        """
        rpm_package_name = rpm_filename[:rpm_filename.index('.')]
        run_remote_command("rpm -e %s --dbpath %s" % (rpm_package_name, rpm_database), host)
        self.check_remote_rpm_uninstall(rpm_package_name, host)
