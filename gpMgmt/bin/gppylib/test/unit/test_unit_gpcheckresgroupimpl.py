#!/usr/bin/env python3
#
# Copyright (c) 2017, VMware, Inc. or its affiliates.
#

import unittest
import os
import sys
import shutil
import tempfile

import imp
gpcheckresgroupimpl_path = os.path.abspath('gpcheckresgroupimpl')
gpcheckresgroupimpl = imp.load_source('gpcheckresgroupimpl', gpcheckresgroupimpl_path)
import gpcheckresgroupimpl

from gppylib.commands import gp
from gppylib import gpversion

gpverstr = gp.GpVersion.local("", os.getenv("GPHOME"))
gpver = gpversion.GpVersion(gpverstr)

@unittest.skipUnless(sys.platform.startswith("linux"), "requires linux")
class GpCheckResGroupImplCGroup(unittest.TestCase):

    def setUp(self):
        self.cgroup_mntpnt = tempfile.mkdtemp(prefix='fake-cgroup-mnt-')

        os.mkdir(os.path.join(self.cgroup_mntpnt, "cpu"), 0o755)
        os.mkdir(os.path.join(self.cgroup_mntpnt, "cpuacct"), 0o755)
        os.mkdir(os.path.join(self.cgroup_mntpnt, "cpuset"), 0o755)

        self.cgroup = gpcheckresgroupimpl.CgroupValidationVersionOne()
        self.cgroup.mount_point = self.cgroup_mntpnt
        self.cgroup.die = self.mock_cgroup_die
        self.cgroup.compdirs = self.cgroup.fallback_comp_dirs()

        self.cgroup_default_mntpnt = self.cgroup.detect_cgroup_mount_point()

        os.mkdir(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"), 0o700)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cgroup.procs"), 0o600)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_period_us"), 0o600)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_quota_us"), 0o600)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.shares"), 0o600)

        os.mkdir(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"), 0o700)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cgroup.procs"), 0o600)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.usage"), 0o400)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.stat"), 0o400)

        os.mkdir(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb"), 0o700)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb", "cgroup.procs"), 0o600)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb", "cpuset.cpus"), 0o600)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb", "cpuset.mems"), 0o600)

    def tearDown(self):
        shutil.rmtree(self.cgroup_mntpnt)
        self.cgroup = None

    def mock_cgroup_die(self, msg):
        output = self.cgroup.impl + self.cgroup.error_prefix + msg
        output = output.replace(self.cgroup_mntpnt, self.cgroup_default_mntpnt)
        raise AssertionError(output)

    def touch(self, path, mode):
        with open(path, "w"):
            pass
        os.chmod(path, mode)

    def test_comp_lists(self):
        # this looks like redundant as it's just a copy of required_comps(),
        # however it is necessary to verify this unit test is up-to-date.
        comps = ['cpu', 'cpuacct']
        if gpver.version >= [6, 0, 0]:
            comps.extend(['cpuset'])
        self.assertEqual(self.cgroup.required_comps(), comps)

    def test_comp_dirs_validation(self):
        self.assertTrue(self.cgroup.validate_comp_dirs())

    def test_comp_dirs_validation_when_cpu_gpdb_dir_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"), 0o100)
        self.assertFalse(self.cgroup.validate_comp_dirs())
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"), 0o700)

    def test_comp_dirs_validation_when_cpu_gpdb_dir_missing(self):
        shutil.rmtree(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"))
        self.assertFalse(self.cgroup.validate_comp_dirs())

    def test_comp_dirs_validation_when_cpuacct_gpdb_dir_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"), 0o100)
        self.assertFalse(self.cgroup.validate_comp_dirs())
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"), 0o700)

    def test_comp_dirs_validation_when_cpuacct_gpdb_dir_missing(self):
        shutil.rmtree(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"))
        self.assertFalse(self.cgroup.validate_comp_dirs())

    def test_comp_dirs_validation_when_cpuset_gpdb_dir_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb"), 0o100)
        if gpver.version >= [6, 0, 0]:
            self.assertFalse(self.cgroup.validate_comp_dirs())
        else:
            self.assertTrue(self.cgroup.validate_comp_dirs())
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb"), 0o700)

    def test_comp_dirs_validation_when_cpuset_gpdb_dir_missing(self):
        shutil.rmtree(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb"))
        if gpver.version >= [6, 0, 0]:
            self.assertFalse(self.cgroup.validate_comp_dirs())
        else:
            self.assertTrue(self.cgroup.validate_comp_dirs())

    def test_proper_setup(self):
        self.cgroup.validate_all()

    def test_proper_setup_with_non_default_cgroup_comp_dirs(self):
        # set comp dir to comp.dir
        component_dirs = self.cgroup.component_dirs
        self.cgroup.component_dirs = {}
        for comp in list(component_dirs.keys()):
            self.cgroup.component_dirs[comp] = comp + '.dir'
        # move /sys/fs/cgroup/comp to /sys/fs/cgroup/comp/comp.dir
        for comp in list(self.cgroup.component_dirs.keys()):
            compdir = self.cgroup.component_dirs[comp]
            olddir = os.path.join(self.cgroup_mntpnt, comp)
            tmpdir = os.path.join(self.cgroup_mntpnt, compdir)
            shutil.move(olddir, tmpdir)
            os.mkdir(olddir, 0o700)
            shutil.move(tmpdir, olddir)
        self.cgroup.validate_all()

    def test_when_cpu_gpdb_dir_missing(self):
        shutil.rmtree(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"))
        with self.assertRaisesRegex(AssertionError, "directory '.*/cpu/gpdb/' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_dir_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"), 0o500)
        with self.assertRaisesRegex(AssertionError, "directory '.*/cpu/gpdb/' permission denied: require permission 'rwx'"):
            self.cgroup.validate_all()
        # restore permission for the dir to be removed in tearDown()
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"), 0o700)

    def test_when_cpu_gpdb_cgroup_procs_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cgroup.procs"))
        with self.assertRaisesRegex(AssertionError, "file '.*/cpu/gpdb/cgroup.procs' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cgroup_procs_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cgroup.procs"), 0o100)
        with self.assertRaisesRegex(AssertionError, "file '.*/cpu/gpdb/cgroup.procs' permission denied: require permission 'rw'"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_cfs_period_us_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_period_us"))
        with self.assertRaisesRegex(AssertionError, "file '.*/cpu/gpdb/cpu.cfs_period_us' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_cfs_period_us_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_period_us"), 0o100)
        with self.assertRaisesRegex(AssertionError, "file '.*/cpu/gpdb/cpu.cfs_period_us' permission denied: require permission 'rw'"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_cfs_quota_us_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_quota_us"))
        with self.assertRaisesRegex(AssertionError, "file '.*/cpu/gpdb/cpu.cfs_quota_us' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_cfs_quota_us_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_quota_us"), 0o100)
        with self.assertRaisesRegex(AssertionError, "file '.*/cpu/gpdb/cpu.cfs_quota_us' permission denied: require permission 'rw'"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_shares_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.shares"))
        with self.assertRaisesRegex(AssertionError, "file '.*/cpu/gpdb/cpu.shares' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_shares_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.shares"), 0o100)
        with self.assertRaisesRegex(AssertionError, "file '.*/cpu/gpdb/cpu.shares' permission denied: require permission 'rw'"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_dir_missing(self):
        shutil.rmtree(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"))
        with self.assertRaisesRegex(AssertionError, "directory '.*/cpuacct/gpdb/' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_dir_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"), 0o500)
        with self.assertRaisesRegex(AssertionError, "directory '.*/cpuacct/gpdb/' permission denied: require permission 'rwx'"):
            self.cgroup.validate_all()
        # restore permission for the dir to be removed in tearDown()
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"), 0o700)

    def test_when_cpuacct_gpdb_cgroup_procs_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cgroup.procs"))
        with self.assertRaisesRegex(AssertionError, "file '.*/cpuacct/gpdb/cgroup.procs' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_cgroup_procs_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cgroup.procs"), 0o100)
        with self.assertRaisesRegex(AssertionError, "file '.*/cpuacct/gpdb/cgroup.procs' permission denied: require permission 'rw'"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_cpuacct_usage_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.usage"))
        with self.assertRaisesRegex(AssertionError, "file '.*/cpuacct/gpdb/cpuacct.usage' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_cpuacct_usage_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.usage"), 0o100)
        with self.assertRaisesRegex(AssertionError, "file '.*/cpuacct/gpdb/cpuacct.usage' permission denied: require permission 'r'"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_cpuacct_stat_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.stat"))
        with self.assertRaisesRegex(AssertionError, "file '.*/cpuacct/gpdb/cpuacct.stat' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_cpuacct_stat_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.stat"), 0o100)
        with self.assertRaisesRegex(AssertionError, "file '.*/cpuacct/gpdb/cpuacct.stat' permission denied: require permission 'r'"):
            self.cgroup.validate_all()

    def test_when_cpuset_gpdb_dir_missing(self):
        shutil.rmtree(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb"))
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegex(AssertionError, "directory '.*/cpuset/gpdb/' does not exist"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()

    def test_when_cpuset_gpdb_dir_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb"), 0o500)
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegex(AssertionError, "directory '.*/cpuset/gpdb/' permission denied: require permission 'rwx'"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()
        # restore permission for the dir to be removed in tearDown()
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb"), 0o700)

    def test_when_cpuset_gpdb_cgroup_procs_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb", "cgroup.procs"))
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegex(AssertionError, "file '.*/cpuset/gpdb/cgroup.procs' does not exist"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()

    def test_when_cpuset_gpdb_cgroup_procs_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb", "cgroup.procs"), 0o100)
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegex(AssertionError, "file '.*/cpuset/gpdb/cgroup.procs' permission denied: require permission 'rw'"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()

    def test_when_cpuset_gpdb_cpuset_cpus_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb", "cpuset.cpus"))
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegex(AssertionError, "file '.*/cpuset/gpdb/cpuset.cpus' does not exist"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()

    def test_when_cpuset_gpdb_cpuset_cpus_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb", "cpuset.cpus"), 0o100)
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegex(AssertionError, "file '.*/cpuset/gpdb/cpuset.cpus' permission denied: require permission 'rw'"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()

    def test_when_cpuset_gpdb_cpuset_mems_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb", "cpuset.mems"))
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegex(AssertionError, "file '.*/cpuset/gpdb/cpuset.mems' does not exist"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()

    def test_when_cpuset_gpdb_cpuset_mems_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuset", "gpdb", "cpuset.mems"), 0o100)
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegex(AssertionError, "file '.*/cpuset/gpdb/cpuset.mems' permission denied: require permission 'rw'"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()


if __name__ == '__main__':
    unittest.main()
