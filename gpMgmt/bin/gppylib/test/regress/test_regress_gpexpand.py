#!/usr/bin/env python

import unittest
import os, socket

from gppylib.commands.base import Command, ExecutionError
from gppylib.commands.gp import GpStart
from gppylib.db import dbconn

class GpExpandTestCase(unittest.TestCase):

    EXPANSION_INPUT_FILE = 'test_expand.input'
    GP_COMMAND_FAULT_POINT = 'GP_COMMAND_FAULT_POINT'
    GPMGMT_FAULT_POINT = 'GPMGMT_FAULT_POINT'
    MASTER_DATA_DIRECTORY = os.environ['MASTER_DATA_DIRECTORY']
    SEGMENTS = 1
    TEST_DB = 'postgres'
    NUM_TABLES = 10

    primary_host_name = None
    mirror_host_name = None
    primary_host_address = None
    mirror_host_address = None

    def setUp(self):
        self._create_expansion_input_file()

    def tearDown(self):
        os.remove(self.EXPANSION_INPUT_FILE)
        if self.GP_COMMAND_FAULT_POINT in os.environ:
            del os.environ[self.GP_COMMAND_FAULT_POINT]

    def _create_expansion_input_file(self):
        """This code has been taken from system_management utilities
           test suite.
           creates a expansion input file"""

        with dbconn.connect(dbconn.DbURL(), unsetSearchPath=False) as conn:
            next_dbid = dbconn.execSQLForSingletonRow(conn,
                                                      "select max(dbid)+1 \
                                                       from pg_catalog.gp_segment_configuration")[0]
            next_content = dbconn.execSQL(conn,
                                          "select max(content)+1 \
                                           from pg_catalog.gp_segment_configuration").fetchall()[0][0]
            next_pri_port = dbconn.execSQL(conn,
                                           "select max(port)+1 \
                                            from pg_catalog.gp_segment_configuration \
                                            where role='p'").fetchall()[0][0]
            self.primary_host_name = dbconn.execSQL(conn,
                                                    "select distinct hostname \
                                                     from gp_segment_configuration \
                                                     where content >= 0 and preferred_role = 'p'").fetchall()[0][0]
            next_mir_port = dbconn.execSQL(conn,
                                           "select max(port)+1 \
                                            from pg_catalog.gp_segment_configuration \
                                            where role='m'").fetchall()[0][0]

            if next_mir_port is None or next_mir_port == ' ' or next_mir_port == 0:
                mirroring_on = False
            else:
                mirroring_on = True
                next_pri_replication_port = dbconn.execSQL(conn,
                                                           "select max(replication_port)+1 \
                                                            from pg_catalog.gp_segment_configuration \
                                                            where role='p'").fetchall()[0][0]
                next_mir_replication_port = dbconn.execSQL(conn,
                                                           "select max(replication_port)+1 \
                                                            from pg_catalog.gp_segment_configuration \
                                                            where role='m'").fetchall()[0][0]
                select_mirror = "select distinct hostname \
                                 from gp_segment_configuration \
                                 where content >= 0 and preferred_role = 'm' and hostname != '%s'" % self.primary_host_name
                mirror_name_row = dbconn.execSQL(conn, select_mirror).fetchall()
                if mirror_name_row is None or len(mirror_name_row) == 0:
                    self.mirror_host_name = self.primary_host_name
                else:
                    self.mirror_host_name = mirror_name_row[0][0]

                self.primary_host_address = socket.getaddrinfo(self.primary_host_name, None)[0][4][0]
                self.mirror_host_address = socket.getaddrinfo(self.mirror_host_name, None)[0][4][0]

            with open(self.EXPANSION_INPUT_FILE, 'w') as outfile:
                for i in range(self.SEGMENTS):
                    pri_datadir = os.path.join(os.getcwd(), 'new_pri_seg%d' % i)
                    mir_datadir = os.path.join(os.getcwd(), 'new_mir_seg%d' % i)

                    temp_str = "%s:%s:%d:%s:%d:%d:%s" % (self.primary_host_name, self.primary_host_address, next_pri_port, pri_datadir, next_dbid, next_content, 'p')
                    if mirroring_on:
                        temp_str = temp_str + ":" + str(next_pri_replication_port)
                    temp_str = temp_str + "\n"
                    outfile.write(temp_str)

                    if mirroring_on: # The content number for mirror is same as the primary segment's content number
                        next_dbid += 1
                        outfile.write("%s:%s:%d:%s:%d:%d:%s:%s\n" % (self.mirror_host_name, self.mirror_host_address, next_mir_port, mir_datadir, next_dbid, next_content, 'm', str(next_mir_replication_port)))
                        next_mir_port += 1
                        next_pri_replication_port += 1
                        next_mir_replication_port += 1

                    next_pri_port += 1
                    next_dbid += 1
                    next_content += 1

    def _create_tables(self):
        with dbconn.connect(dbconn.DbURL(), unsetSearchPath=False) as conn:
            for i in range(self.NUM_TABLES):
                dbconn.execSQL(conn, 'create table tab%d(i integer)' % i)
            conn.commit()

    def _drop_tables(self):
        with dbconn.connect(dbconn.DbURL(), unsetSearchPath=False) as conn:
            for i in range(self.NUM_TABLES):
                dbconn.execSQL(conn, 'drop table tab%d' % i)
            conn.commit()

    def _get_dist_policies(self):
        policies = []
        with dbconn.connect(dbconn.DbURL(), unsetSearchPath=False) as conn:
            cursor = dbconn.execSQL(conn, 'select * from gp_distribution_policy;').fetchall()
            for row in cursor:
                policies.append(row)

        return policies

    def test00_pg_hba_conf_file(self):
        os.environ[self.GP_COMMAND_FAULT_POINT] = 'gpexpand tar segment template'

        cmd = Command(name='run gpexpand', cmdStr='gpexpand -i %s' % (self.EXPANSION_INPUT_FILE))
        with self.assertRaisesRegexp(ExecutionError, 'Fault Injection'):
            cmd.run(validateAfter=True)

        #Read from the pg_hba.conf file and ensure that
        #The address of the new hosts is present.
        cmd = Command(name='get the temp pg_hba.conf file',
                      cmdStr="ls %s" % os.path.join(os.path.dirname(self.MASTER_DATA_DIRECTORY),
                                                    'gpexpand*',
                                                    'pg_hba.conf'))
        cmd.run(validateAfter=True)
        results = cmd.get_results()
        temp_pg_hba_conf = results.stdout.strip()

        actual_values = set()
        expected_values = set([self.primary_host_address, self.mirror_host_address])
        with open(temp_pg_hba_conf) as f:
            for line in f:
                if line.strip() == '# %s' % self.primary_host_name or\
                   line.strip() == '# %s' % self.mirror_host_name:
                    address = f.next().strip().split()[3]
                    address = address[:address.rfind('/')]
                    actual_values.add(address)

        self.assertEqual(actual_values, expected_values)

        GpStart(name='start the database in master only mode', masterOnly=True).run(validateAfter=True)
        Command(name='rollback the expansion', cmdStr='gpexpand -r').run(validateAfter=True)
        GpStart(name='start the database').run(validateAfter=True)

    def test01_distribution_policy(self):

        self._create_tables()

        try:
            os.environ[self.GPMGMT_FAULT_POINT] = 'gpexpand MPP-14620 fault injection'
            original_dist_policies = self._get_dist_policies()
            cmd = Command(name='run gpexpand', cmdStr='gpexpand -i %s' % (self.EXPANSION_INPUT_FILE))
            with self.assertRaisesRegexp(ExecutionError, 'Fault Injection'):
                cmd.run(validateAfter=True)

            rollback = Command(name='rollback expansion', cmdStr='gpexpand -r')
            rollback.run(validateAfter=True)

            dist_policies = self._get_dist_policies()

            self.assertEqual(original_dist_policies, dist_policies)
        finally:
            self._drop_tables()
