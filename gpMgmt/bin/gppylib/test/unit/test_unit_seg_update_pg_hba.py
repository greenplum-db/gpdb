#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
from mock import call, patch, MagicMock, Mock
import os

import seg_update_pg_hba
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests

class UpdatePgHBAConfTests(GpTestCase):
    def setUp(self):
        self.apply_patches([
            patch('os.rename'),
            patch('os.path.abspath')
        ])

    def test_lineToCanonical(self):
        test_str = " \t qwerty  qwerty\tqwerty\n \n"
        result_str = seg_update_pg_hba.lineToCanonical(test_str)
        expected_str = "qwerty qwerty qwerty"
        self.assertEqual(result_str, expected_str)

    def test_remove_dup_add_entries(self):
        pg_hba_entries = ['test1', 'test2']
        test_entries = 'test2\ntest3'
        result_out_entries = seg_update_pg_hba.remove_dup_add_entries(pg_hba_entries, test_entries)
        expected_out_entries = ['test1', 'test2', 'test3']
        self.assertEqual(result_out_entries, expected_out_entries)

    def test_remove_dup_add_entries_all_dup(self):
        pg_hba_entries = ['test1', 'test2']
        test_entries = 'test2'
        result_out_entries = seg_update_pg_hba.remove_dup_add_entries(pg_hba_entries, test_entries)
        expected_out_entries = ['test1', 'test2']
        self.assertEqual(result_out_entries, expected_out_entries)

    def test_remove_dup_add_entries_existing_dup(self):
        pg_hba_entries = ['test1', 'test2', 'test2']
        test_entries = 'test3'
        result_out_entries = seg_update_pg_hba.remove_dup_add_entries(pg_hba_entries, test_entries)
        expected_out_entries = ['test1', 'test2', 'test3']
        self.assertEqual(result_out_entries, expected_out_entries)

    def test_remove_dup_with_spaces_add_entries(self):
        entries_block = """host replication gpadmin samehost trust
host all gpadmin {ip_mirror2}/32 trust
host    all     gpadmin     {ip_mirror1}/32     trust
host    all     gpadmin     {ip_mirror2}/32     trust
host   replication   gpadmin   {ip_mirror1}/32   trust
host   replication   gpadmin   {ip_mirror2}/32   trust
# host   replication   gpadmin   {ip_mirror1}/32   trust"""
        pg_hba_existing_entries = entries_block.format(ip_mirror1='10.0.0.1', ip_mirror2='10.0.0.2').split('\n')

        test_entries = 'host all gpadmin {ip_mirror1}/32 trust\nhost all gpadmin {ip_mirror3}/32 trust'\
            .format(ip_mirror1='10.0.0.1', ip_mirror3='10.0.0.3')

        result_out_entries = seg_update_pg_hba.remove_dup_add_entries(pg_hba_existing_entries, test_entries)

        expected_block = """host replication gpadmin samehost trust
host all gpadmin {ip_mirror2}/32 trust
host    all     gpadmin     {ip_mirror1}/32     trust
host   replication   gpadmin   {ip_mirror1}/32   trust
host   replication   gpadmin   {ip_mirror2}/32   trust
# host   replication   gpadmin   {ip_mirror1}/32   trust
host all gpadmin {ip_mirror3}/32 trust"""
        expected_out_entries = expected_block.\
            format(ip_mirror1='10.0.0.1', ip_mirror2='10.0.0.2', ip_mirror3='10.0.0.3').split('\n')
        self.assertEqual(result_out_entries, expected_out_entries)

    def test_remove_stale_replication_entries(self):
        existing_entries = """host replication gpadmin samehost trust
host all gpadmin 10.0.0.1/32 trust
host all gpadmin 10.0.0.2/32 trust
host replication gpadmin 10.0.0.1/32 trust
host replication gpadmin 10.0.0.2/32 trust""".split('\n')

        input_entries = 'host replication gpadmin samehost trust\nhost all gpadmin 10.0.0.2/32 trust' \
                        '\nhost replication gpadmin 10.0.0.2/32 trust\nhost replication gpadmin 10.0.0.3/32 trust'
        unique_entries = seg_update_pg_hba.remove_dup_add_entries(existing_entries, input_entries)
        result = seg_update_pg_hba.remove_stale_replication_entries(unique_entries, input_entries)

        expected = """host replication gpadmin samehost trust
host all gpadmin 10.0.0.1/32 trust
host all gpadmin 10.0.0.2/32 trust
host replication gpadmin 10.0.0.2/32 trust
host replication gpadmin 10.0.0.3/32 trust""".split('\n')
        self.assertEqual(result, expected)

    def test_pg_hba_with_no_replication_entries(self):
        existing_entries = """# standby coordinator host ip addresses
host all gpadmin 10.0.0.2/32 trust
host all gpadmin 10.0.0.1/32 trust""".split('\n')
        input_entries = '# standby coordinator host ip addresses\nhost all gpadmin 10.0.0.2/32 trust'

        result = seg_update_pg_hba.remove_stale_replication_entries(existing_entries, input_entries)

        self.assertEqual(result, existing_entries)

    def test_remove_stale_replication_entries_without_removing_localhost(self):
        existing_entries = """# Allow replication connections from localhost, by a user with the
# replication privilege.
local   replication     all                                     trust
host    replication     all             127.0.0.1/32            trust
host    replication     all             ::1/128                 trust
host replication gpadmin samehost trust
host all gpadmin 10.0.0.1/32 trust
host all gpadmin 10.0.0.2/32 trust
host replication gpadmin 10.0.0.1/32 trust
host replication gpadmin 10.0.0.2/32 trust""".split('\n')
        input_entries = 'host replication gpadmin samehost trust\nhost all gpadmin 10.0.0.2/32 trust\nhost replication gpadmin 10.0.0.2/32 trust'

        result = seg_update_pg_hba.remove_stale_replication_entries(existing_entries, input_entries)

        expected = """# Allow replication connections from localhost, by a user with the
# replication privilege.
local   replication     all                                     trust
host    replication     all             127.0.0.1/32            trust
host    replication     all             ::1/128                 trust
host replication gpadmin samehost trust
host all gpadmin 10.0.0.1/32 trust
host all gpadmin 10.0.0.2/32 trust
host replication gpadmin 10.0.0.2/32 trust""".split('\n')

        self.assertEqual(result, expected)

    def test_remove_stale_replication_entries_without_removing_samehost(self):
        existing_entries = """# Allow replication connections from localhost, by a user with the
# replication privilege.
local   replication     all                                     trust
host    replication     all             127.0.0.1/32            trust
host    replication     all             ::1/128                 trust
host replication gpadmin samehost trust
host all gpadmin 10.0.0.1/32 trust
host all gpadmin 10.0.0.2/32 trust
host replication gpadmin 10.0.0.1/32 trust
host replication gpadmin 10.0.0.2/32 trust""".split('\n')
        input_entries = 'host all gpadmin 10.0.0.2/32 trust\nhost replication gpadmin 10.0.0.2/32 trust'

        result = seg_update_pg_hba.remove_stale_replication_entries(existing_entries, input_entries)

        expected = """# Allow replication connections from localhost, by a user with the
# replication privilege.
local   replication     all                                     trust
host    replication     all             127.0.0.1/32            trust
host    replication     all             ::1/128                 trust
host replication gpadmin samehost trust
host all gpadmin 10.0.0.1/32 trust
host all gpadmin 10.0.0.2/32 trust
host replication gpadmin 10.0.0.2/32 trust""".split('\n')

        self.assertEqual(result, expected)

