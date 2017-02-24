#!/usr/bin/env python
#
# Copyright (c) Pivotal Inc 2014. All Rights Reserved. 
#

import os
import unittest2 as unittest
from gppylib.gpversion import GpVersion
from gppylib.operations.gpMigratorUtil import is_supported_version
from mock import patch, MagicMock, Mock 

class IsSupportedVersionTestCase(unittest.TestCase):
    def test_is_supported_version_major_version_upgrade(self):
        v = GpVersion('4.3.0.0')
        self.assertTrue(is_supported_version(v))

    def test_is_supported_minor_version_upgrade(self):
        v = GpVersion('4.3.8.0')
        self.assertTrue(is_supported_version(v))

    def test_is_supported_dev_build_version_upgrade(self):
        v = GpVersion('4.3 build dev')
        self.assertTrue(is_supported_version(v))

    def test_is_supported_version_hotfix_upgrade(self):
        v = GpVersion('4.3.7.3EC7')
        self.assertTrue(is_supported_version(v))

    def test_is_supported_version_major_version_unsupported_upgrade(self):
        v = GpVersion('4.0.0.0')
        with self.assertRaisesRegexp(Exception, "Greenplum Version '4.0.0.0 build dev' is not supported for upgrade"):
            is_supported_version(v)

    def test_is_supported_version_awesome_build_upgrade(self):
        v = GpVersion('4.3.3.4__AWESOME_BUILD__')
        self.assertTrue(is_supported_version(v))

    def test_is_supported_version_space_in_build_upgrade(self):
        v = GpVersion('4.3.3.4 EC7')
        self.assertTrue(is_supported_version(v))

    def test_is_supported_version_major_version_downgrade(self):
        v = GpVersion('4.3.0.0')
        self.assertTrue(is_supported_version(v, False))

    def test_is_supported_minor_version_downgrade(self):
        v = GpVersion('4.3.8.0')
        self.assertTrue(is_supported_version(v, False))

    def test_is_supported_dev_build_version_downgrade(self):
        v = GpVersion('4.3 build dev')
        self.assertTrue(is_supported_version(v, False))

    def test_is_supported_version_hotfix_downgrade(self):
        v = GpVersion('4.3.7.3EC7')
        self.assertTrue(is_supported_version(v, False))

    def test_is_supported_version_major_version_unsupported_downgrade(self):
        v = GpVersion('4.0.0.0')
        with self.assertRaisesRegexp(Exception, "Greenplum Version '4.0.0.0 build dev' is not supported for downgrade"):
            is_supported_version(v, False)

    def test_is_supported_version_awesome_build_downgrade(self):
        v = GpVersion('4.3.3.4__AWESOME_BUILD__')
        self.assertTrue(is_supported_version(v, False))

    def test_is_supported_version_space_in_build_downgrade(self):
        v = GpVersion('4.3.3.4 EC7')
        self.assertTrue(is_supported_version(v))

