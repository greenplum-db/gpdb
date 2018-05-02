#!/usr/bin/env python
#
# Copyright (c) 2017, Pivotal Software Inc.
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
    cgroup_mntpnt = None
    cgroup_default_mntpnt = gpcheckresgroupimpl.detectCgroupMountPoint()

    def setUp(self):
        self.cgroup_mntpnt = tempfile.mkdtemp(prefix='fake-cgroup-mnt-')

        os.mkdir(os.path.join(self.cgroup_mntpnt, "cpu"), 0755)
        os.mkdir(os.path.join(self.cgroup_mntpnt, "cpuacct"), 0755)
        os.mkdir(os.path.join(self.cgroup_mntpnt, "memory"), 0755)

        self.cgroup = gpcheckresgroupimpl.cgroup()
        self.cgroup.mount_point = self.cgroup_mntpnt
        self.cgroup.die = self.mock_cgroup_die

        os.mkdir(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"), 0700)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cgroup.procs"), 0600)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_period_us"), 0600)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_quota_us"), 0600)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.shares"), 0600)

        os.mkdir(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"), 0700)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cgroup.procs"), 0600)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.usage"), 0400)
        self.touch(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.stat"), 0400)

        self.touch(os.path.join(self.cgroup_mntpnt, "memory", "memory.limit_in_bytes"), 0400)
        self.touch(os.path.join(self.cgroup_mntpnt, "memory", "memory.memsw.limit_in_bytes"), 0400)

        os.mkdir(os.path.join(self.cgroup_mntpnt, "memory", "gpdb"), 0700)
        self.touch(os.path.join(self.cgroup_mntpnt, "memory", "gpdb", "memory.limit_in_bytes"), 0600)
        self.touch(os.path.join(self.cgroup_mntpnt, "memory", "gpdb", "memory.usage_in_bytes"), 0400)

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

    def test_proper_setup(self):
        self.cgroup.validate_all()

    def test_when_cpu_gpdb_dir_missing(self):
        shutil.rmtree(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"))
        with self.assertRaisesRegexp(AssertionError, "directory '.*/cpu/gpdb/' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_dir_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"), 0500)
        with self.assertRaisesRegexp(AssertionError, "directory '.*/cpu/gpdb/' permission denied: require permission 'rwx'"):
            self.cgroup.validate_all()
        # restore permission for the dir to be removed in tearDown()
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb"), 0700)

    def test_when_cpu_gpdb_cgroup_procs_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cgroup.procs"))
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpu/gpdb/cgroup.procs' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cgroup_procs_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cgroup.procs"), 0100)
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpu/gpdb/cgroup.procs' permission denied: require permission 'rw'"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_cfs_period_us_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_period_us"))
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpu/gpdb/cpu.cfs_period_us' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_cfs_period_us_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_period_us"), 0100)
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpu/gpdb/cpu.cfs_period_us' permission denied: require permission 'rw'"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_cfs_quota_us_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_quota_us"))
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpu/gpdb/cpu.cfs_quota_us' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_cfs_quota_us_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.cfs_quota_us"), 0100)
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpu/gpdb/cpu.cfs_quota_us' permission denied: require permission 'rw'"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_shares_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.shares"))
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpu/gpdb/cpu.shares' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpu_gpdb_cpu_shares_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpu", "gpdb", "cpu.shares"), 0100)
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpu/gpdb/cpu.shares' permission denied: require permission 'rw'"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_dir_missing(self):
        shutil.rmtree(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"))
        with self.assertRaisesRegexp(AssertionError, "directory '.*/cpuacct/gpdb/' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_dir_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"), 0500)
        with self.assertRaisesRegexp(AssertionError, "directory '.*/cpuacct/gpdb/' permission denied: require permission 'rwx'"):
            self.cgroup.validate_all()
        # restore permission for the dir to be removed in tearDown()
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb"), 0700)

    def test_when_cpuacct_gpdb_cgroup_procs_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cgroup.procs"))
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpuacct/gpdb/cgroup.procs' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_cgroup_procs_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cgroup.procs"), 0100)
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpuacct/gpdb/cgroup.procs' permission denied: require permission 'rw'"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_cpuacct_usage_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.usage"))
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpuacct/gpdb/cpuacct.usage' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_cpuacct_usage_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.usage"), 0100)
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpuacct/gpdb/cpuacct.usage' permission denied: require permission 'r'"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_cpuacct_stat_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.stat"))
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpuacct/gpdb/cpuacct.stat' does not exist"):
            self.cgroup.validate_all()

    def test_when_cpuacct_gpdb_cpuacct_stat_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "cpuacct", "gpdb", "cpuacct.stat"), 0100)
        with self.assertRaisesRegexp(AssertionError, "file '.*/cpuacct/gpdb/cpuacct.stat' permission denied: require permission 'r'"):
            self.cgroup.validate_all()

    def test_when_memory_limit_in_bytes_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "memory", "memory.limit_in_bytes"))
        with self.assertRaisesRegexp(AssertionError, "file '.*/memory/memory.limit_in_bytes' does not exist"):
            self.cgroup.validate_all()

    def test_when_memory_limit_in_bytes_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "memory", "memory.limit_in_bytes"), 0100)
        with self.assertRaisesRegexp(AssertionError, "file '.*/memory/memory.limit_in_bytes' permission denied: require permission 'r'"):
            self.cgroup.validate_all()

    def test_when_memory_gpdb_dir_missing(self):
        shutil.rmtree(os.path.join(self.cgroup_mntpnt, "memory", "gpdb"))
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegexp(AssertionError, "directory '.*/memory/gpdb/' does not exist"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()

    def test_when_memory_gpdb_dir_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "memory", "gpdb"), 0500)
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegexp(AssertionError, "directory '.*/memory/gpdb/' permission denied: require permission 'rwx'"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()
        # restore permission for the dir to be removed in tearDown()
        os.chmod(os.path.join(self.cgroup_mntpnt, "memory", "gpdb"), 0700)

    def test_when_memory_gpdb_limit_in_bytes_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "memory", "gpdb", "memory.limit_in_bytes"))
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegexp(AssertionError, "file '.*/memory/gpdb/memory.limit_in_bytes' does not exist"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()

    def test_when_memory_gpdb_limit_in_bytes_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "memory", "gpdb", "memory.limit_in_bytes"), 0100)
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegexp(AssertionError, "file '.*/memory/gpdb/memory.limit_in_bytes' permission denied: require permission 'rw'"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()

    def test_when_memory_gpdb_usage_in_bytes_missing(self):
        os.unlink(os.path.join(self.cgroup_mntpnt, "memory", "gpdb", "memory.usage_in_bytes"))
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegexp(AssertionError, "file '.*/memory/gpdb/memory.usage_in_bytes' does not exist"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()

    def test_when_memory_gpdb_usage_in_bytes_bad_permission(self):
        os.chmod(os.path.join(self.cgroup_mntpnt, "memory", "gpdb", "memory.usage_in_bytes"), 0100)
        if gpver.version >= [6, 0, 0]:
            with self.assertRaisesRegexp(AssertionError, "file '.*/memory/gpdb/memory.usage_in_bytes' permission denied: require permission 'r'"):
                self.cgroup.validate_all()
        else:
            self.cgroup.validate_all()


if __name__ == '__main__':
    unittest.main()
