#!/usr/bin/env python3

import os
import shutil
import unittest
import tempfile
import platform
import getpass
import tarfile

from gppylib.db import dbconn
from gppylib.gparray import GpArray
from contextlib import closing
from gppylib.commands import gp
from gppylib.commands.unix import Rsync
from gppylib.commands.base import Command, ExecutionError
from gppylib.operations import Operation
from gppylib.operations.unix import CheckFile, RemoveRemoteFile
from gppylib.operations.package import dereference_symlink

OS = platform.system()
ARCH = platform.machine()

# AK: use dereference_symlink when mucking with RPM database for the same reason 
# it's used in the gppylib.operations.package. For more info, see the function definition.
GPHOME = dereference_symlink(gp.get_gphome())

ARCHIVE_PATH = os.path.join(GPHOME, 'share/packages/archive')
RPM_DATABASE = os.path.join(GPHOME, 'share/packages/database')
GPPKG_EXTENSION = ".gppkg"
SCRATCH_SPACE = os.path.join(tempfile.gettempdir(), getpass.getuser())
GPDB_VERSION = "4.2"
COORDINATOR_PORT = os.getenv("PGPORT", 15432)


def skipIfNoStandby():
    standby = get_host_list()[0]
    if standby is None:
        return unittest.skip("requires standby")
    return lambda o: o


def get_host_list():
    """
        Returns a tuple which consists of the standby
        and segment hosts
    """
    gparr = GpArray.initFromCatalog(dbconn.DbURL(port=COORDINATOR_PORT), utility=True)
    segs = gparr.getDbList()

    standby_host = None
    segment_host_list = []

    for seg in segs:
        if seg.isSegmentStandby(current_role=True):
            standby_host = seg.getSegmentHostName()
        elif not seg.isSegmentCoordinator(current_role=True):
            segment_host_list.append(seg.getSegmentHostName())

    # Deduplicate the hosts so that we
    # dont install multiple times on the same host
    segment_host_list = list(set(segment_host_list))

    return (standby_host, segment_host_list)


def remove_timestamp(result):
    result_without_timestamp = []
    results = result.split('\n')[1:]

    for res in results:
        res = res.split(':-')[1].strip()
        res = ''.join(res.split(' '))
        result_without_timestamp.append(res)

    return result_without_timestamp


def run_command(cmd_str):
    cmd = Command("Local Command", cmd_str)
    cmd.run(validateAfter=True)
    results = cmd.get_results()

    if results.rc != 0:
        return results.stderr.strip()
    else:
        return results.stdout.strip()


class RPMSpec:
    def __init__(self, name, version, release, depends=[]):
        self.name = name
        self.version = version
        self.release = release
        self.depends = depends

    def get_package_name(self):
        return self.name + '-' + self.version + '-' + self.release

    def get_filename(self):
        return self.get_package_name() + '.' + ARCH + ".rpm"

    def __str__(self):
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
BuildArch:      ''' + ARCH + ''' 
Provides:       temp_test = ''' + self.version + ''', /bin/sh
BuildRoot:      %{_topdir}/BUILD '''

        if self.depends != []:
            '''Requires:       ''' + ' '.join(self.depends)

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

            shutil.copy(os.path.join(SCRATCH_SPACE, "RPMS", ARCH, self.spec.get_filename()), os.getcwd())
        finally:
            shutil.rmtree(build_dir)
            shutil.rmtree(rpms_dir)

        return self.spec.get_filename()


class GppkgSpec:
    def __init__(self, name, version, gpdbversion=GPDB_VERSION, os=OS, arch=ARCH):
        self.name = name
        self.version = version
        self.gpdbversion = gpdbversion
        self.os = os
        self.arch = arch

    def get_package_name(self):
        return self.name + '-' + self.version

    def get_filename(self):
        return self.get_package_name() + '-' + self.os + '-' + self.arch + GPPKG_EXTENSION

    def __str__(self):
        gppkg_spec_file = '''
PkgName: ''' + self.name + '''
Version: ''' + self.version + '''
GPDBVersion: ''' + self.gpdbversion + '''
Description: Temporary Test Package
OS: ''' + self.os + '''
Architecture: ''' + self.arch

        return gppkg_spec_file


class BuildGppkg(Operation):
    def __init__(self, gppkg_spec, main_rpm_spec, dependent_rpm_specs=[]):
        self.gppkg_spec = gppkg_spec
        self.main_rpm_spec = main_rpm_spec
        self.dependent_rpm_specs = dependent_rpm_specs

    def execute(self):

        if not os.path.exists(SCRATCH_SPACE):
            os.mkdir(SCRATCH_SPACE)

        gppkg_spec_file = str(self.gppkg_spec)
        # create gppkg_dir
        gppkg_dir = os.path.join(SCRATCH_SPACE, "package")
        if not os.path.exists(gppkg_dir):
            os.mkdir(gppkg_dir)

        rpm_file = BuildRPM(self.main_rpm_spec).run()
        shutil.move(rpm_file, gppkg_dir)

        try:
            if self.dependent_rpm_specs != []:
                deps_dir = os.path.join(gppkg_dir, "deps")
                os.mkdir(deps_dir)

                for spec in self.dependent_rpm_specs:
                    rpm_file = BuildRPM(spec).run()
                    shutil.move(rpm_file, deps_dir)

            # create gppkg
            with open(os.path.join(gppkg_dir, "gppkg_spec.yml"), "w") as f:
                f.write(gppkg_spec_file)

            run_command("gppkg --build " + gppkg_dir)
        finally:
            shutil.rmtree(gppkg_dir)
            shutil.rmtree(SCRATCH_SPACE)

        return self.gppkg_spec.get_filename()


class GppkgTestCase(unittest.TestCase):
    def setUp(self):
        self.cleanup()
        self.rpm_spec = RPMSpec("test", "1", "1")
        self.gppkg_spec = GppkgSpec("test", "1.0")

    def tearDown(self):
        self.cleanup()

    @classmethod
    def setUpClass(self):
        self.built_packages = set()

    @classmethod
    def tearDownClass(self):
        for gppkg in self.built_packages:
            os.remove(gppkg)

    def cleanup(self):
        results = run_command("gppkg -q --all")
        gppkgs = results.split('\n')[1:]  # The first line is 'Starting gppkg with args', which we want to ignore.

        for gppkg in gppkgs:
            run_command("gppkg --remove " + gppkg)

    def build(self, gppkg_spec, rpm_spec, dep_rpm_specs=[]):
        gppkg_file = BuildGppkg(gppkg_spec, rpm_spec, dep_rpm_specs).run()
        self.assertTrue(self._check_build(gppkg_file, gppkg_spec))
        self.built_packages.add(gppkg_file)
        return gppkg_file

    def _check_build(self, gppkg_file, gppkg_spec):
        return (gppkg_file == (gppkg_spec.get_filename()))

    def install(self, gppkg_filename):
        run_command("gppkg --install %s" % gppkg_filename)
        self.assertTrue(self.check_install(gppkg_filename))

    def check_install(self, gppkg_filename):
        cmd = "gppkg -q %s" % gppkg_filename
        results = run_command(cmd)
        results = results[1:]
        test_str = ''.join(gppkg_filename.split('-')[:1]) + " is installed"
        is_installed = test_str in results
        return is_installed and CheckFile(os.path.join(ARCHIVE_PATH, gppkg_filename)).run()

    def remove(self, gppkg_filename):
        gppkg_package_name = gppkg_filename.split('-')[0] + '-' + gppkg_filename.split('-')[1]
        run_command("gppkg --remove %s" % gppkg_package_name)
        self.assertFalse(self.check_install(gppkg_filename))

    def update(self, old_gppkg_filename, new_gppkg_filename):
        run_command("gppkg --update %s" % new_gppkg_filename)
        self.assertTrue(self.check_install(new_gppkg_filename))


class SimpleGppkgTestCase(GppkgTestCase):
    def test00_simple_build(self):
        self.build(self.gppkg_spec, self.rpm_spec)

    def test01_simple_install(self):
        gppkg_file = self.gppkg_spec.get_filename()
        self.install(gppkg_file)

        # Check RPM database
        results = run_command("rpm -q %s --dbpath %s" % (self.rpm_spec.get_package_name(), RPM_DATABASE))
        self.assertEqual(results, self.rpm_spec.get_package_name())

    def test02_simple_update(self):
        gppkg_file = self.gppkg_spec.get_filename()
        self.install(gppkg_file)

        update_rpm_spec = RPMSpec("test", "1", "2")
        update_gppkg_spec = GppkgSpec("test", "1.1")
        update_gppkg_file = self.build(update_gppkg_spec, update_rpm_spec)

        self.update(gppkg_file, update_gppkg_file)

        # Check for the packages
        results = run_command("rpm -q %s --dbpath %s" % (update_rpm_spec.get_package_name(), RPM_DATABASE))
        self.assertEqual(results, update_rpm_spec.get_package_name())

    def test03_simple_uninstall(self):
        gppkg_file = self.gppkg_spec.get_filename()

        self.install(gppkg_file)
        self.remove(gppkg_file)

        results = run_command("gppkg -q --all")
        results = results.split('\n')[1:]

        self.assertEqual(results, [])

    def test04_help(self):
        help_options = ["--help", "-h", "-?"]

        for opt in help_options:
            results = run_command("gppkg " + opt)
            self.assertNotEqual(results, "")

    def test05_version(self):
        results = run_command("gppkg --version")
        self.assertNotEqual(results, "")


class QueryTestCases(GppkgTestCase):
    def setUp(self):
        self.rpm_spec1 = RPMSpec("test", "1", "1")
        self.gppkg_spec1 = GppkgSpec("test", "1.0")

        self.rpm_spec2 = RPMSpec("test1", "1", "1")
        self.gppkg_spec2 = GppkgSpec("test1", "1.0")

    def tearDown(self):
        '''
            Overriding the teardown here as successive tests
            in the suite make use of gppkg installed in the
            previous test.
        '''
        pass

    def test00_install(self):
        gppkg_file1 = self.build(self.gppkg_spec1, self.rpm_spec1)
        gppkg_file2 = self.build(self.gppkg_spec2, self.rpm_spec2)

        self.install(gppkg_file1)
        self.install(gppkg_file2)

    def test01_query_all(self):
        results = run_command("gppkg -q --all")
        self.assertEqual(results.split('\n')[1:],
                          [self.gppkg_spec1.get_package_name(), self.gppkg_spec2.get_package_name()])

        results = run_command("gppkg --all -q")
        self.assertEqual(results.split('\n')[1:],
                          [self.gppkg_spec1.get_package_name(), self.gppkg_spec2.get_package_name()])

        self.assertRaises(ExecutionError, run_command, "gppkg -qall")

    def test02_query_info(self):
        expected_info_result = ['Nametest', 'Version1.0', 'Architecturex86_64', 'OSLinux', 'GPDBVersionmainbuilddev',
                                'DescriptionTemporaryTestPackage']
        # Normal order of the options
        results = run_command("gppkg -q --info %s" % self.gppkg_spec1.get_filename())
        self.assertTrue(len(results.split('\n')) > 1)
        results = remove_timestamp(results)
        self.assertEqual(results, expected_info_result)

        # Reverse order of the options
        results = run_command("gppkg --info -q %s" % self.gppkg_spec1.get_filename())
        self.assertTrue(len(results.split('\n')) > 1)
        results = remove_timestamp(results)
        self.assertEqual(results, expected_info_result)

    def test03_query_list(self):
        expected_list_result = []
        with closing(tarfile.open(self.gppkg_spec1.get_filename(), 'r:gz')) as tarinfo:
            expected_list_result = tarinfo.getnames()

        results = run_command("gppkg -q --list %s" % self.gppkg_spec1.get_filename())
        self.assertTrue(len(results.split('\n')) > 1)
        results = results.split('\n')[1:]
        self.assertEqual(results, expected_list_result)

        results = run_command("gppkg --list -q %s" % self.gppkg_spec1.get_filename())
        self.assertTrue(len(results.split('\n')) > 1)
        results = results.split('\n')[1:]
        self.assertEqual(results, expected_list_result)


class MiscTestCases(GppkgTestCase):
    @unittest.expectedFailure
    def test00_MPP_14671(self):
        rpm_spec = RPMSpec("test", "1", "1")
        gppkg_spec = GppkgSpec("test", "1.0")
        gppkg_file = self.build(self.gppkg_spec, self.rpm_spec)

        dummy_file = "dummy-x.y.z-abcd-efgh.gppkg"
        shutil.copy(gppkg_file, dummy_file)
        self.built_packages.add(dummy_file)

        self.install(dummy_file)
        self.assertTrue(os.path.exists(os.path.join(GPHOME, 'share', 'packages', 'archive', dummy_file)))

        results = run_command("rpm -q %s --dbpath %s" % (rpm_spec.get_package_name(), RPM_DATABASE))
        self.assertEqual(results, rpm_spec.get_package_name())

        self.assertTrue(self.check_install(gppkg_file))

        results = run_command("gppkg -q --all")
        self.assertTrue(''.join(gppkg_file.split('-')[:1]) in results)


class SimpleNegativeTestCases(GppkgTestCase):
    @unittest.expectedFailure
    def test00_wrong_os(self):
        os = "abcde"
        rpm_spec = self.rpm_spec
        gppkg_spec = GppkgSpec("test", "1.0", GPDB_VERSION, os)
        gppkg_file = self.build(gppkg_spec, rpm_spec)

        with self.assertRaisesRegex(ExecutionError, "%s os required. %s os found" % (os, OS)):
            self.install(gppkg_file)

    def test02_wrong_gpdbversion(self):
        gpdb_version = "4.6"
        rpm_spec = self.rpm_spec
        gppkg_spec = GppkgSpec("test", "1.0", gpdb_version)
        gppkg_file = self.build(gppkg_spec, rpm_spec)

        with self.assertRaisesRegex(ExecutionError, "requires Greenplum Database version %s" % gpdb_version):
            self.install(gppkg_file)

    def test03_install_twice(self):
        gppkg_file = self.build(self.gppkg_spec, self.rpm_spec)

        self.install(self.gppkg_spec.get_filename())

        with self.assertRaisesRegex(ExecutionError, "%s is already installed" % gppkg_file):
            self.install(gppkg_file)

    @unittest.expectedFailure
    def test04_update_gppkg_lower(self):
        # Use gppkg from previous test
        self.install(self.gppkg_spec.get_filename())

        # Use gppkg which has a lower version, but the rpm is > the one installed
        update_rpm_spec = RPMSpec("test", "1", "2")
        update_gppkg_spec = GppkgSpec("test", "0.1")
        update_gppkg_file = self.build(update_gppkg_spec, update_rpm_spec)

        with self.assertRaisesRegex(ExecutionError,
                                     "Newer version of %s already installed" % update_gppkg_spec.get_package_name()):
            self.update(self.gppkg_spec.get_filename(), update_gppkg_file)
        # Check that the original package is still installed and not updated
        self.assertTrue(self.check_install(self.gppkg_spec.get_filename()))

    def test05_update_rpm_lower(self):
        # Use gppkg from previous test
        self.install(self.gppkg_spec.get_filename())

        # Use gppkg with a lower RPM version but gppkg version is > the one installed
        update_rpm_spec = RPMSpec("test", "1", "0")
        update_gppkg_spec = GppkgSpec("test", "1.1")
        update_gppkg_file = self.build(update_gppkg_spec, update_rpm_spec)

        with self.assertRaisesRegex(ExecutionError, self.rpm_spec.get_filename()):
            self.update(self.gppkg_spec.get_filename(), update_gppkg_file)
        # Check that the original package is still installed and not updated
        self.assertTrue(self.check_install(self.gppkg_spec.get_filename()))

    def test06_uninstall_twice(self):
        # Uses the gppkg from previous test
        self.install(self.gppkg_spec.get_filename())

        # Uninstall gppkg
        self.remove(self.gppkg_spec.get_filename())

        with self.assertRaisesRegex(ExecutionError, "%s has not been installed" % self.gppkg_spec.get_package_name()):
            self.remove(self.gppkg_spec.get_filename())


class SingleDependenciesTestCases(GppkgTestCase):
    def setUp(self):
        super(SingleDependenciesTestCases, self).cleanup()
        self.main_rpm_spec = RPMSpec("test", "1", "1")
        self.dep_rpm_spec = RPMSpec("dep", "1", "1")
        self.gppkg_spec = GppkgSpec("test", "1.0")

    def test00_build(self):
        self.build(self.gppkg_spec, self.main_rpm_spec, [self.dep_rpm_spec])

    def test01_install(self):
        gppkg_file = self.build(self.gppkg_spec, self.main_rpm_spec, [self.dep_rpm_spec])
        self.install(gppkg_file)

    def test02_update_gppkg_higher(self):
        # Use gppkg from previous test
        self.install(self.gppkg_spec.get_filename())

        # New gppkg with higher gppkg, main and deps version
        update_main_rpm_spec = RPMSpec("test", "1", "2")
        update_dep_rpm_spec = RPMSpec("dep", "1", "2")
        update_gppkg_spec = GppkgSpec("test", "1.1")
        update_gppkg_file = self.build(update_gppkg_spec, update_main_rpm_spec, [update_dep_rpm_spec])

        self.update(self.gppkg_spec.get_filename(), update_gppkg_file)

    @unittest.expectedFailure
    def test03_update_gppkg_lower(self):
        # Use the gppkg from previous test
        update_main_rpm_spec = RPMSpec("test", "1", "2")
        update_dep_rpm_spec = RPMSpec("dep", "1", "2")
        update_gppkg_spec = GppkgSpec("test", "1.1")
        self.install(update_gppkg_spec.get_filename())

        # Original gppkg with a lower gppkg, main and deps version
        with self.assertRaisesRegex(ExecutionError,
                                     "A newer version of %s is already installed" % self.gppkg_spec.get_filename()):
            self.update(update_gppkg_spec.get_filename(), self.gppkg_spec.get_filename())

    def test04_uninstall(self):
        self.install(self.gppkg_spec.get_filename())
        self.remove(self.gppkg_spec.get_filename())


class MuckWithInternalsTestCases(GppkgTestCase):
    '''
        Each test method in this suite should clean up
        whatever it "mucks". Otherwise it will leave the 
        system in an inconsistent state and the generic
        cleanup method will not be able to cleanup 
        the packages. 
    '''

    def test00_delete_package_from_archive_and_install(self):
        gppkg_file = self.build(self.gppkg_spec, self.rpm_spec)
        self.install(gppkg_file)

        # Remove the package from archive
        os.remove(os.path.join(ARCHIVE_PATH, gppkg_file))

        self.install(gppkg_file)

    @unittest.expectedFailure
    def test01_delete_package_from_archive_and_uninstall(self):
        # Building off of last test case's state
        # Use gppkg from previous test
        gppkg_file = self.gppkg_spec.get_filename()
        self.install(gppkg_file)

        # Remove the package from archive
        os.remove(os.path.join(ARCHIVE_PATH, gppkg_file))

        try:
            self.remove(self.gppkg_spec.get_filename())
        except ExecutionError as e:
            shutil.copy(gppkg_file, os.path.join(ARCHIVE_PATH, gppkg_file))
            self.fail("Execution Error %s" % e)

    def test02_delete_rpm_and_install(self):
        # Use gppkg from previous test
        gppkg_file = self.gppkg_spec.get_filename()
        self.install(gppkg_file)

        # Uninstall the RPM
        run_command("rpm -e %s --dbpath %s" % (self.rpm_spec.get_package_name(), RPM_DATABASE))

        with self.assertRaisesRegex(ExecutionError, "%s is not installed" % self.rpm_spec.get_package_name()):
            run_command("rpm -q %s --dbpath %s" % (self.rpm_spec.get_package_name(), RPM_DATABASE))

        try:
            self.install(gppkg_file)
        except ExecutionError as e:
            # Install the rpm
            with closing(tarfile.open(self.gppkg_spec.get_filename())) as tf:
                tf.extract(self.rpm_spec.get_filename())
            run_command("rpm -i %s --dbpath %s --prefix=%s" % (self.rpm_spec.get_filename(), RPM_DATABASE, GPHOME))
            os.remove(self.rpm_spec.get_filename())
            self.fail("ExecutionError %s" % e)

    def test03_install_rpm_and_install(self):
        # Install the rpm
        with closing(tarfile.open(self.gppkg_spec.get_filename())) as tf:
            tf.extract(self.rpm_spec.get_filename())

        # Install rpm
        run_command("rpm --install %s --dbpath %s --prefix=%s" % (self.rpm_spec.get_filename(), RPM_DATABASE, GPHOME))

        results = run_command("rpm -q %s --dbpath %s" % (self.rpm_spec.get_package_name(), RPM_DATABASE))
        self.assertRegex(results, self.rpm_spec.get_package_name())

        # Use gppkg from previous test
        gppkg_file = self.gppkg_spec.get_filename()

        try:
            self.install(gppkg_file)
        except ExecutionError as e:
            run_command("rpm -e %s --dbpath %s" % (self.rpm_spec.get_package_name(), RPM_DATABASE))
            os.remove(self.rpm_spec.get_filename())
            self.fail("ExecutionError %s" % e)

    @unittest.expectedFailure
    def test04_delete_package_from_archive_on_segment(self):
        '''
            Known issue: MPP-15737
        '''
        gppkg_file = self.gppkg_spec.get_filename()
        archive_file = os.path.join(ARCHIVE_PATH, gppkg_file)
        self.install(gppkg_file)

        # Remove package from archive
        segment_host_list = get_host_list()[1]
        self.assertTrue(len(segment_host_list) > 0)
        RemoveRemoteFile(archive_file, segment_host_list[0])

        try:
            self.install(gppkg_file)
        except ExecutionError as e:
            Rsync(name="copy to segment", srcFile=gppkg_file, dstFile=archive_file, srcHost=None,
                dstHost=segment_host_list[0]).run(validateAfter=True)
            self.fail("ExecutionError %s" % e)

    @skipIfNoStandby()
    @unittest.expectedFailure
    def test05_delete_package_from_archive_on_standby(self):
        '''
            Known issue: MPP-15737
        '''
        gppkg_file = self.gppkg_spec.get_filename()
        archive_file = os.path.join(ARCHIVE_PATH, gppkg_file)
        self.install(gppkg_file)

        # Remove package from standby
        standby = get_host_list()[0]
        RemoveRemoteFile(os.path.join(ARCHIVE_PATH, gppkg_file), standby).run()

        try:
            self.install(gppkg_file)
        except ExecutionError as e:
            Rsync(name="copy to segment", srcFile=gppkg_file, dstFile=archive_file, srcHost=None, dstHost=standby).run(
                validateAfter=True)
            self.fail("ExecutionError %s" % e)


if __name__ == "__main__":
    unittest.main()
