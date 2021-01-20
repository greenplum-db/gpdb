#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
from mock import patch, MagicMock, Mock

from gppylib.gparray import Segment, GpArray
from gppylib.operations.update_pg_hba_conf import config_primaries_for_replication
from test.unit.gp_unittest import GpTestCase, run_tests


class UpdatePgHBAConfTests(GpTestCase):
    def setUp(self):
        def _setup_gparray():
            coordinator = Segment.initFromString(
                "1|-1|p|p|s|u|cdw|cdw|5432|/data/coordinator")
            standby = Segment.initFromString(
                "6|-1|m|m|s|u|sdw3|sdw3|5433|/data/standby")
            primary0 = Segment.initFromString(
                "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
            primary1 = Segment.initFromString(
                "3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
            mirror0 = Segment.initFromString(
                "4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
            mirror1 = Segment.initFromString(
                "5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")
            return GpArray([coordinator, standby, primary0, primary1, mirror0, mirror1])

        self.gparray = _setup_gparray()
        self.apply_patches([
            patch('gppylib.operations.update_pg_hba_conf.logger', return_value=Mock(spec=['log', 'info', 'debug', 'error'])),
        ])
        self.logger = self.get_mock_from_apply_patch('logger')

    @patch('gppylib.operations.update_pg_hba_conf.Command')
    @patch('gppylib.operations.update_pg_hba_conf.gp.IfAddrs.list_addrs', return_value=['192.168.2.1', '192.168.1.1'])
    def test_pghbaconf_updated_successfully(self, mock_cmd, mock_list_addrs):
        config_primaries_for_replication(self.gparray, False)
        self.logger.info.assert_any_call("Starting to modify pg_hba.conf on primary segments to allow replication connections")
        self.logger.info.assert_any_call("Successfully modified pg_hba.conf on primary segments to allow replication connections")

    @patch('gppylib.operations.update_pg_hba_conf.Command', side_effect=Exception("boom"))
    @patch('gppylib.operations.update_pg_hba_conf.gp.IfAddrs.list_addrs', return_value=['192.168.2.1', '192.168.1.1'])
    def test_pghbaconf_updated_fails(self, mock1, mock2):
        with self.assertRaisesRegex(Exception, "boom"):
            config_primaries_for_replication(self.gparray, False)
        self.logger.info.assert_any_call("Starting to modify pg_hba.conf on primary segments to allow replication connections")
        self.logger.error.assert_any_call("Failed while modifying pg_hba.conf on primary segments to allow replication connections: boom")

