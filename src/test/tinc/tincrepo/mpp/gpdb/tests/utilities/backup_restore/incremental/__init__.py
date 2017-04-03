"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import fileinput
import fnmatch
import os
import glob
import shutil
import pexpect as pexpect
import re
import sys
import time
import unittest2 as unittest
from datetime import datetime, date, timedelta


import tinctest
from gppylib.commands.base import Command, REMOTE, ExecutionError
from gppylib.db import dbconn
from tinctest.case import TINCTestCase
from tinctest.lib import local_path, Gpdiff
from tinctest.runner import TINCTextTestResult

from mpp.lib.PSQL import PSQL
from mpp.lib.gpstop import GpStop
from mpp.lib.PgHba import PgHba, Entry

DDBOOST_BACKUP_DIR = 'backup/DCA-35'
DEFAULT_DUMP_LOC = 'db_dumps'

class BackupTestCase(TINCTestCase):
    """
        Base class for backup restore test case
    """

    def __init__(self, methodName):
        self.test_method = methodName
        self.gphome = os.environ.get('GPHOME')
        self.pgport = os.environ.get('PGPORT')
        self.master_dd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.backup_timestamp = ''
        self.full_backup_timestamp = ''
        self.incre_timestamp_1 = ''
        self.incre_timestamp_2 = ''
        self.incre_timestamp_3 = ''
        self.incre_timestamp_4 = ''
        self.dump_file_prefix = 'gp_dump'
        self.restore_file_prefix = 'gp_restore'
        super(BackupTestCase,self).__init__(methodName)

    def run_full_backup(self, dbname = None, option = None, value = None, location=None, func_name=None):
        tinctest.logger.info("Running Full Backup ...")
        self.run_gpcrondump(dbname, option, value, func_name=func_name)

    def run_incr_backup(self, dirty_dir, dbname = None, option = None, value = None, expected_rc = 0, incr_no = 1, location=None, func_name=None):
        tinctest.logger.info("Running Incremental Backup...")
        result = self.run_gpcrondump(dbname, option, value, expected_rc, incr_no, incremental = 'yes', func_name=func_name)
        if expected_rc == 0:
            if option == '-u':
                dump_dir = value
            else:
                dump_dir = None

            if option and option.startswith('--prefix'):
                prefix = option[9:].strip().split()[0].strip()
            else:
                prefix = None

            ddboost = False
            if option is not None:
                if 'ddboost' in option:
                    ddboost = True

            self.compare_dirty_table_list(dirty_dir, dump_dir, prefix, ddboost, location)

            if option is not None:
                option_list = option.split(',')
                for opt in option_list :
                    self.validateOption(dbname, result, opt.strip(), ddboost, location)

    def run_restore(self, dbname = None, option = None, location = None, type = 'incremental', expected_rc = 0, incr_no = None):
        tinctest.logger.info("Running Restore ...")
        self.run_gpdbrestore(dbname, option, location, type, expected_rc, incr_no)

    def run_gpcrondump(self, dbname = None, option = None, value = None, expected_rc = 0, incr_no = 1, incremental = None, sleeping=0, func_name=None):
        '''Note : options that dont need a value can be given as a list,
                  option that need a value must be given as an individual element in the list with a valid value '''

        if sleeping > 0:
            time.sleep(sleeping)

        if dbname is None:
            dbname = os.environ.get('PGDATABASE')
            if dbname is None:
                raise Exception('Database name is required to do the backup')

        op_value_list = ['-s', '-u', '-t', '-T', '-B', '-y', '-d', '-f', '-l', '--exclude-table-file', '--table-file']
        option_str = " "
        cmd  = ' -x %s -a ' % dbname
        if option is not None:
            if "--prefix" in option:
                option_str = option.strip()
            else:
                option_list = option.split(',')
                for option in option_list:
                    if option in op_value_list:
                        if value is None:
                            raise Exception('Value for the option %s is missing : Specify a valid value' % option)
                        else:
                            if option.strip() == '--exclude-table-file' or option.strip() == '--table-file' :
                                 cmd = cmd + '  %s="%s" ' % (option.strip(), value)
                            else:
                                cmd = cmd + '%s  %s '% (option.strip(), value)
                    else:
                        option_str = option_str + " " + option.strip()

            cmd = cmd + option_str

        if incremental is not None:
            cmd = cmd + ' --incremental'

        cmd_str = '%s/bin/gpcrondump %s' % (self.gphome, cmd)
        result = self.run_gpcommand(cmd_str, expected_rc)
        if expected_rc == 0:
            self.backup_timestamp = self.get_timestamp(result)
            if incremental is None:
                self.full_backup_timestamp = self.backup_timestamp
            elif incr_no == 2:
                self.incre_timestamp_2 = self.backup_timestamp
            elif incr_no == 3:
                self.incre_timestamp_3 = self.backup_timestamp
            elif incr_no == 4:
                self.incre_timestamp_4 = self.backup_timestamp
            else:
                self.incre_timestamp_1 = self.backup_timestamp
        if func_name:
            test_old_to_new_incr_ddboost.global_timestamps[func_name].append(self.backup_timestamp)
        return result

    def run_gpdbrestore(self, dbname = None, option = None, location = None, type = 'incremental', expected_rc = 0, incr_no = None):
        if type == 'full':
            backup_timestamp = self.full_backup_timestamp
        else:
            if incr_no is None:
                backup_timestamp = self.backup_timestamp
            elif incr_no == 1:
                backup_timestamp = self.incre_timestamp_1
            elif incr_no == 2:
                backup_timestamp = self.incre_timestamp_2
            elif incr_no == 3:
                backup_timestamp = self.incre_timestamp_3
            elif incr_no == 4:
                backup_timestamp = self.incre_timestamp_4
            else:
                backup_timestamp = self.backup_timestamp

        ddboost = False
        if option is None:
            cmd = ' -e -t %s -a ' % backup_timestamp
        elif ' -b ' in option:
            cmd = '%s %s' % (option, backup_timestamp[0:8])
            if 'ddboost' in option:
                self.restore_with_option_b(location, cmd, type, ddboost=True)
            else:
                self.restore_with_option_b(location, cmd, type)
        elif '-s ' in option:
            cmd =  '%s -a ' % option
        elif ' -R ' in option:
            cmd = '-e -a -R localhost:%s/db_dumps/%s ' % (location, backup_timestamp[0:8])
        elif 'ddboost' in option:
            ddboost = True
            cmd = '-t %s -a %s -e ' % (backup_timestamp, option)
        else:
            cmd = '-t %s -a %s ' % (backup_timestamp, option)

        cmd_str = '%s/bin/gpdbrestore %s' % (self.gphome, cmd)
        if option is None or ' -b ' not in option:
            result = self.run_gpcommand(cmd_str, expected_rc)
            return
        if option.strip() == '-L':
            self.validateOption(dbname, result, option.strip(), ddboost, location)

    def restore_with_option_b(self, location, cmd, type = 'incremental', ddboost=False):

        date_folder = self.backup_timestamp[0:8]
        if location is None:
            location = self.master_dd
        if ddboost:
            dump_folder = '%s/%s' % (location, date_folder)
            #dump_folder = '%s/%s/%s' % (location, DDBOOST_BACKUP_DIR, date_folder)
        else:
            dump_folder = '%s/db_dumps/%s' % (location, date_folder)
        results = []
        results += [each for each in os.listdir(dump_folder) if fnmatch.fnmatch(each, 'gp_dump*.rpt')]
        timestamp_number = len(results) - 1

        gp_cmd =  "/bin/bash -c 'source %s/greenplum_path.sh;%s/bin/gpdbrestore %s'" % (self.gphome, self.gphome, cmd)
        logfile = open(local_path('install.log'),'w')

        child = pexpect.spawn(gp_cmd, timeout=400)
        child.logfile = logfile
        time.sleep(5)
        check = child.expect(['.* Enter timestamp number to restore >.*', ' '])
        if check != 0:
            raise Exception('gpdbrestore not asking for timestamp number')
        child.sendline(str(timestamp_number))

        time.sleep(150)
        try:
            check = child.expect(['.*completed without error.*'])
            if check != 0:
                raise Exception('gpdbrestore with option -b failed')
            child.close()
        except:
            raise

    def run_gpcommand(self, command, expected_rc = 0):
        cmd = Command(name='run gp_command', cmdStr = 'source %s/greenplum_path.sh;%s' % (self.gphome, command))
        try:
            cmd.run(validateAfter=False)
        except Exception:
            tinctest.logger.error("Error running command")

        result = cmd.get_results()
        if result.rc != expected_rc:
            raise Exception(" %s failed with error %s " % (cmd.cmdStr , result.stderr))
        return result.stdout

    # execute commands to create, check, or remove one-year old dump directories
    def execDumpDirCommand(self, ifile, dumpDirOpLog):
        file = open(dumpDirOpLog,'a')
        for line in ifile:
            if (line.find('ssh ')>=0 and line.find('SELECT')<0):
                (ok, output) = self.run_command(line)
                if ok:
                    raise Exception('Dump directory operation error:' + output)
                else:
                    file.write(output)
        file.close()

    def validateOption(self, dbname, result, option, ddboost=False, location=None):
        if option == '-k':
            search_str = "Commencing post-dump vacuum..."
        elif option == '-j':
            search_str = "Commencing pre-dump vacuum"
        elif option == '-G':
            search_str = "Commencing pg_catalog dump"
        elif option == '-g':
            search_str = "Dumping master config files"
        elif option == '-h':
            search_str = "Inserted dump record into public.gpcrondump_history"
        elif option == '-L':
            search_str = "List of database tables for dump file with time stamp %s" % self.backup_timestamp
        else:
            search_str = "Incremental"

        if not self.search_optionString(result, search_str) and option != '--ddboost':
            raise Exception('The search string for the option is not found in the stdout')

        if option == '-g':
            self.check_config_file()
        if option == '-h':
            if not self.history_table_check(dbname):
                raise Exception('History table is not updated with the --incremental gpcrondump call')
        if option in ('--use-set-session-authorization', '--no-owner', '--no-privileges'):
            self.verifyOption(option, ddboost, location)

    def verifyOption(self, option, ddboost=False, location=None):
        zcat_file = '/tmp/foo'
        if ddboost:
            dump_file = '%s/%s/gp_dump_-1_1_%s.gz' % (location, self.backup_timestamp[0:8], self.backup_timestamp)
            cmd = Command('DDBoost copy of master dump file', 'gpddboost --readFile --from-file=%s | gunzip' % (dump_file))

            try:
                cmd.run(validateAfter = True)
            except ExecutionError: # Dump may be using old-format files, check for those next
                dump_file = '%s/%s/gp_dump_1_1_%s.gz' % (location, self.backup_timestamp[0:8], self.backup_timestamp)
                cmd = Command('DDBoost copy of master dump file', 'gpddboost --readFile --from-file=%s | gunzip' % (dump_file))
                cmd.run(validateAfter = True)
            dump_file_contents = cmd.get_results().stdout.splitlines()
            with open(zcat_file, 'w') as fd:
                dump_contents = " ".join(dump_file_contents)
                fd.write(dump_contents)
        else:
            dump_file = '%s/db_dumps/%s/gp_dump_-1_1_%s.gz' % (self.master_dd, self.backup_timestamp[0:8], self.backup_timestamp)
            cmd = Command('Copy of master dump file', 'zcat %s > %s' % (dump_file, zcat_file))
            try:
                cmd.run(validateAfter = True)
            except ExecutionError:
                dump_file = '%s/db_dumps/%s/gp_dump_1_1_%s.gz' % (self.master_dd, self.backup_timestamp[0:8], self.backup_timestamp)
                cmd = Command('Copy of master dump file', 'zcat %s > %s' % (dump_file, zcat_file))
                cmd.run(validateAfter = True)
        content = ''
        with open(zcat_file) as fd:
            content = fd.read()
        if option == '--use-set-session-authorization':
            if not self.search_optionString(content, 'SET SESSION AUTHORIZATION') :
                raise Exception ('SET SESSION AUTHORIZATION is not used for option --use-set-session-authorization')
        elif option == '--no-owner':
            if self.search_optionString(content, 'OWNER TO'):
                raise Exception('OWNER is still present with option --no-owner')
        elif option == '--no-privileges':
            if self.search_optionString(content, 'GRANT'):
                raise Exception('GRANT and REVOKE are still present when --no-privileges is used')

    def history_table_check(self, dbname):
        history_sql = 'select dump_key,options from public.gpcrondump_history;'
        result = self.run_SQLQuery(history_sql, dbname = 'template1')
        for (dump_key, options) in result:
            if self.backup_timestamp == dump_key.strip() and dbname in options:
                    return True
        return False

    def search_optionString(self, result, search_str):
        for line in result.splitlines():
            if search_str in line:
                return True
        return False

    def check_config_file(self):
        dump_config_file = '%s/db_dumps/%s/gp_master_config_files_%s.tar' % (self.master_dd, self.backup_timestamp[0:8], self.backup_timestamp)
        if not os.path.exists(dump_config_file):
            raise Exception('Config tar file not created by -g option with --incremental')

    def run_command(self, command):
        cmd = Command(name='run %s' % command, cmdStr='%s' % (command))
        try:
            cmd.run(validateAfter=True)
        except Exception, e:
            tinctest.logger.error("Error running command %s" % e)
        result = cmd.get_results()
        return result.rc, result.stdout

    def run_SQLCommand(self, sql_cmd = None, dbname = None, username = None, password = None,
                        PGOPTIONS = None, host = None, port = None):
        cmd = PSQL(None, sql_cmd = sql_cmd, dbname = dbname, username = username, password = password, PGOPTIONS = PGOPTIONS, host = host, port = port)
        cmd.run(validateAfter = False)
        result = cmd.get_results()
        return result.rc, result.stdout

    def run_SQLQuery(self, exec_sql, dbname):
        with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
            curs = dbconn.execSQL(conn, exec_sql)
            results = curs.fetchall()
        return results

    def validate_timestamp(self, ts):
        try:
            int_ts = int(ts)
        except Exception as e:
            raise Exception('Timestamp is not valid %s' % ts)

        if len(ts) != 14:
            raise Exception('Timestamp is invalid %s' % ts)

    def get_timestamp(self, result):
        for line in result.splitlines():
            if 'Timestamp key = ' in line:
                log_msg, delim, timestamp = line.partition('=')
                ts = timestamp.strip()
                self.validate_timestamp(ts)
                return ts
        raise Exception('Timestamp key not found')

    def run_sqlfiles(self, load_path, dbname):
        version = self.get_version()
        for file in os.listdir(load_path):
            if file.endswith(".sql"):
                out_file = file.replace(".sql", ".out")
                assert PSQL.run_sql_file(sql_file = load_path + file, dbname = dbname, port = self.pgport, out_file = load_path + out_file)

    def run_workload(self, dir, dbname, verify = False):
        tinctest.logger.info("Running workload ...")
        if dir.find('dirty') < 0 :
            self.drop_create_database(dbname)
        load_path = local_path(dir) + os.sep
        self.run_sqlfiles(load_path, dbname)
        if verify == True:
            self.validate_sql_files(load_path)

    def validate_restore(self, dir, dbname):
        tinctest.logger.info("Validating workload after restore...")
        load_path = local_path(dir) + os.sep
        self.run_sqlfiles(load_path, dbname)
        self.validate_sql_files(load_path)

    def validate_sql_files(self, load_path):
        for file in os.listdir(load_path):
            if file.endswith(".out"):
                out_file = file
                ans_file = file.replace('.out' , '.ans')
                if os.path.exists(load_path + ans_file):
                    assert Gpdiff.are_files_equal(load_path + out_file, load_path + ans_file)
                else:
                    raise Exception("No .ans file exists for %s " % out_file)

    def validate_sql_file(self, file):
        out_file = file.replace('.sql', '.out')
        ans_file = file.replace('.sql', '.ans')
        if os.path.exists(ans_file):
            assert Gpdiff.are_files_equal(out_file, ans_file)
        else:
            raise Exception("No .ans file exists for %s " % out_file)

    def isFileEqual(self, src_file, dest_file):
        if os.path.exists(src_file) and os.path.exists(dest_file):
            return Gpdiff.are_files_equal(src_file, dest_file)

    def check_db_exists(self, dbname):
        db_sql = "select datname from pg_database where datname='%s';" % dbname
        result = self.run_SQLQuery(db_sql, dbname = 'template1')
        if result:
            if result[0][0] == dbname:
                    return True
        return False

    def create_database(self, dbname):

        command = "create database %s;" % dbname
        for i in range(10):
            (rc, result) = self.run_SQLCommand(sql_cmd = command, dbname = 'template1')
            if self.check_db_exists(dbname):
                tinctest.logger.info("Database %s created" % dbname)
                return
            time.sleep(1)

    def drop_database(self, dbname):

        command = "drop database %s;" % dbname
        for i in range(10):
            (rc, result) = self.run_SQLCommand(sql_cmd = command, dbname = 'template1')
            if not self.check_db_exists(dbname):
                tinctest.logger.info("Database %s dropped" % dbname)
                return
            time.sleep(1)

    def drop_create_database(self, dbname):
        if self.check_db_exists(dbname):
            self.drop_database(dbname)
        self.create_database(dbname)

    def get_table_list(self,dbname):
        table_sql = "SELECT n.nspname AS schemaname, c.relname AS tablename \
                    FROM pg_class c  LEFT JOIN pg_namespace n ON n.oid = c.relnamespace \
                    LEFT JOIN pg_tablespace t ON t.oid = c.reltablespace \
                    WHERE c.relkind = 'r'::\"char\" AND c.oid > 16384 AND (c.relnamespace > 16384 or n.nspname = 'public');"
        table_list = self.run_SQLQuery(table_sql, dbname = dbname)
        return table_list

    def get_data_to_file(self, db_name, operation):
        tables = self.get_table_list(db_name)
        for table in tables:
            tablename = "%s.%s" % (table[0].strip(), table[1].strip())
            data_file = "%s_%s" % (tablename, operation)
            filename  = local_path('./backup_data/%s'% data_file)
            data_sql = "COPY (select gp_segment_id, * from gp_dist_random('%s') order by 1, 2) TO '%s'" %(tablename, filename)
            (rc, data_out)  = self.run_SQLCommand(data_sql, dbname=db_name)
            if rc !=0 :
                raise Exception(" COPY command for table %s failed" % tablename)

    def check_filter_file_exists(self, expected=False, prefix=None):
        if prefix is None:
            prefix = ""
        else:
            prefix = prefix + "_"

        filter_file = '%s/db_dumps/%s/%sgp_dump_%s_filter' % (self.master_dd, self.backup_timestamp[0:8], prefix, self.backup_timestamp)
        if (not os.path.exists(filter_file)) and expected:
            raise Exception('Expected filter file %s to be present, but was not found.' % (filter_file))
        elif os.path.exists(filter_file) and (not expected):
            raise Exception('Found filter file %s when it should not exist.' % (filter_file))

        return

    def compare_table_data(self, dbname, restore_no = 1, base_file_name='backup'):
        self.get_data_to_file(dbname, 'restore%s' % restore_no) #Get all the table data to files for validation
        tables = self.get_table_list(dbname)
        for table in tables:
            tablename = "%s.%s" % (table[0].strip(), table[1].strip())
            if tablename == "public.gpcrondump_history":
                continue
            backup_file_path = './backup_data/%s_%s' % (tablename, base_file_name)
            if base_file_name == 'backup':
                backup_file_path = './backup_data/%s_%s%s' % (tablename, base_file_name, restore_no)
            backup_file = local_path(backup_file_path)
            restore_file = local_path('./backup_data/%s_restore%s' % (tablename, restore_no))
            diff_cmd = 'diff %s %s' % (backup_file, restore_file)
            (rc, out) = self.run_command(diff_cmd)
            if rc !=0:
                shutil.copy(backup_file, backup_file + self.backup_timestamp)
                shutil.copy(restore_file, restore_file + self.backup_timestamp)
                tinctest.logger.error("Restore Validation Failed,  output: %s, look into the backup file %s and restore file %s, timestamp %s" %
                                     (out, backup_file, restore_file, self.backup_timestamp))
                raise Exception("Restore Validation Failed for table %s" % tablename)
        tinctest.logger.info("Compared table data  after restore")


    def get_partition_list(self, partition_type, dbname):

        GET_APPENDONLY_DATA_TABLE_INFO_SQL ="""SELECT ALL_DATA_TABLES.oid, ALL_DATA_TABLES.schemaname, ALL_DATA_TABLES.tablename, OUTER_PG_CLASS.relname as tupletable FROM(
        SELECT ALLTABLES.oid, ALLTABLES.schemaname, ALLTABLES.tablename FROM
        (SELECT c.oid, n.nspname AS schemaname, c.relname AS tablename FROM pg_class c, pg_namespace n
        WHERE n.oid = c.relnamespace) as ALLTABLES,
        (SELECT n.nspname AS schemaname, c.relname AS tablename
        FROM pg_class c LEFT JOIN pg_namespace n ON n.oid = c.relnamespace
        LEFT JOIN pg_tablespace t ON t.oid = c.reltablespace
        WHERE c.relkind = 'r'::"char" AND c.oid > 16384 AND (c.relnamespace > 16384 or n.nspname = 'public')
        EXCEPT
        ((SELECT x.schemaname, x.partitiontablename FROM
        (SELECT distinct schemaname, tablename, partitiontablename, partitionlevel FROM pg_partitions) as X,
        (SELECT schemaname, tablename maxtable, max(partitionlevel) maxlevel FROM pg_partitions group by (tablename, schemaname))   as Y
        WHERE x.schemaname = y.schemaname and x.tablename = Y.maxtable and x.partitionlevel != Y.maxlevel)
        UNION (SELECT distinct schemaname, tablename FROM pg_partitions))) as DATATABLES
        WHERE ALLTABLES.schemaname = DATATABLES.schemaname and ALLTABLES.tablename = DATATABLES.tablename AND ALLTABLES.oid not in (select reloid from pg_exttable)
        ) as ALL_DATA_TABLES, pg_appendonly, pg_class OUTER_PG_CLASS
        WHERE ALL_DATA_TABLES.oid = pg_appendonly.relid
        AND OUTER_PG_CLASS.oid = pg_appendonly.segrelid
        """

        GET_ALL_AO_DATATABLES_SQL = """
            %s AND pg_appendonly.columnstore = 'f'
        """ % GET_APPENDONLY_DATA_TABLE_INFO_SQL

        GET_ALL_CO_DATATABLES_SQL = """
            %s AND pg_appendonly.columnstore = 't'
        """ % GET_APPENDONLY_DATA_TABLE_INFO_SQL
        if partition_type == 'ao':
            sql = GET_ALL_AO_DATATABLES_SQL
        elif partition_type == 'co':
            sql = GET_ALL_CO_DATATABLES_SQL

        partition_list = self.run_SQLQuery(sql, dbname)
        for line in partition_list:
            if len(line) != 4:
                raise Exception('Invalid results from query to get all AO tables: [%s]' % (','.join(line)))
        return partition_list

    def verify_stats(self, dbname, partition_info):

        for (oid, schemaname, partition_name, tupletable) in partition_info:
            if partition_name.strip() in ('ao_table_parent' , 'co_table_parent', 'ao_table_child', 'co_table_child'):
                continue
            tuple_count_sql = "select to_char(sum(tupcount::bigint), '999999999999999999999') from pg_aoseg.%s" % tupletable
            tuple_count = self.run_SQLQuery(tuple_count_sql, dbname)[0][0]
            if tuple_count is not None:
                tuple_count = tuple_count.strip()
            else:
                tuple_count = '0'
            self.validate_tuple_count(dbname, schemaname, partition_name, tuple_count)

    def validate_tuple_count(self, dbname, schemaname, partition_name, tuple_count):
        sql = 'select count(*) from %s.%s' % (schemaname, partition_name)
        row_count = self.run_SQLQuery(sql, dbname)[0][0]
        if int(row_count) != int(tuple_count):
            raise Exception('Stats for the table %s.%s does not match. Stat count "%s" does not match the actual tuple count "%s"' % (schemaname, partition_name, tuple_count, row_count))

    def compare_ao_tuple_count(self, dbname):
        partition_info_ao = self.get_partition_list('ao', dbname)
        self.verify_stats(dbname, partition_info_ao)

        partition_info_co = self.get_partition_list('co', dbname)
        self.verify_stats(dbname, partition_info_co)

    def sort_file_contents(self, filename):
        '''Sort the contents of a file and return as a list '''
        with open(filename) as f:
            lines = f.read()
        f_list = lines.splitlines()
        sorted_list = sorted(filter(bool, f_list), key=lambda x: str(x), reverse=False)
        return sorted_list

    def get_version(self):
        result = PSQL.run_sql_command('select version();', flags='-t -q', dbname='postgres')
        return result.split(" ").pop(5)

    def compare_dirty_table_list(self, dirty_dir, dump_dir = None, prefix = None, ddboost=False, location=None):
        if dump_dir is None:
            dump_dir = self.master_dd
        if prefix is None:
            prefix = ""
        else:
            prefix = prefix + "_"

        # Check version and use a different dirty_table_list for 4.2 and others
        version = self.get_version()
        table_list = local_path('%s/dirty_table_list'% (dirty_dir))
        tinctest.logger.info("Got the table_list for answering file: %s" % table_list)
        dirty_list = self.sort_file_contents(table_list)
        if ddboost:
            dump_dirty_list = self.sort_file_contents(os.path.join(dump_dir, location, '%s' % self.backup_timestamp[0:8] ,'%sgp_dump_%s_dirty_list' % (prefix, self.backup_timestamp)))
            #dump_dirty_list = self.sort_file_contents(os.path.join(dump_dir, DDBOOST_BACKUP_DIR, '%s' % self.backup_timestamp[0:8] ,'%sgp_dump_%s_dirty_list' % (prefix, self.backup_timestamp)))
            tinctest.logger.info("output dump_dirty_list_file: %s" % os.path.join(dump_dir, location, '%s' % self.backup_timestamp[0:8] ,'%sgp_dump_%s_dirty_list' % (prefix, self.backup_timestamp)))
        else:
            dump_dirty_list = self.sort_file_contents(os.path.join(dump_dir, 'db_dumps', '%s' % self.backup_timestamp[0:8] ,'%sgp_dump_%s_dirty_list' % (prefix, self.backup_timestamp)))
            tinctest.logger.info("output dump_dirty_list_file: %s" % (os.path.join(dump_dir, 'db_dumps', '%s' % self.backup_timestamp[0:8] ,'%sgp_dump_%s_dirty_list' % (prefix, self.backup_timestamp))))
        if dirty_list != dump_dirty_list :
            ans_set = set(dirty_list)
            out_set = set(dump_dirty_list)
            if ans_set > out_set:
                msg = "The following tables are present in the answer file but not the output: " + ','.join(ans_set - out_set)
            else:
                msg = "The following tables are present in the output but not the answer file: " + ','.join(out_set - ans_set)
            tinctest.logger.info("dirty_list:\n%s" % dirty_list)
            tinctest.logger.info("dump_dirty_list:\n%s" % dump_dirty_list)
            raise Exception("Incremental backup validation failed with diff: %s" % msg)
        else:
            tinctest.logger.info("The tablelist for the incremental backup is validated")

    def get_hosts(self):
        get_hosts_sql = "select distinct hostname from gp_segment_configuration where role='p';"
        return self.run_SQLQuery(get_hosts_sql, dbname = 'template1')

    def get_segments(self):
        get_seg_sql = "select distinct(hostname) from gp_segment_configuration where content != -1;"
        return self.run_SQLQuery(get_seg_sql, dbname = 'template1')

    def cleanup_backup_files(self, location=None):
        config = self.get_config()
        for record in config:
            if location is None:
                cleanup_cmd = "gpssh -h %s -e 'rm -rf %s/db_dumps'" % (record[2], record[3])
            else:
                cleanup_cmd = "gpssh -h %s -e 'rm -rf %s/db_dumps'" % (record[2], location)
            (rc, result) = self.run_command(cleanup_cmd)
            if rc != 0:
                raise Exception('Unable to remove the dump directories')

    def get_config_with_fs(self):
        config_sql = "select dbid, role, hostname, fselocation from gp_segment_configuration, pg_filespace_entry where fsedbid = dbid; "
        return self.run_SQLQuery(config_sql, dbname = 'template1')

    def get_config(self):
        config_sql = "select dbid, role, hostname, fselocation from gp_segment_configuration gp,pg_filespace_entry, pg_filespace fs where fsefsoid = fs.oid and fsname='pg_system' and gp.dbid=pg_filespace_entry.fsedbid;"
        return self.run_SQLQuery(config_sql, dbname = 'template1')

    def filespace_exists(self, fsname = 'filespace_a'):
        fs_sql = "select * from pg_filespace where fsname='%s'" % fsname.strip()
        fs_list = self.run_SQLQuery(fs_sql, dbname = 'template1')
        if len(fs_list) == 1 :
            return True
        return False

    def create_filespace(self, fsname = 'filespace_a'):

        if self.filespace_exists(fsname) is False:
            config = self.get_config_with_fs()
            file_config = local_path('%s_config' % fsname)
            f1 = open(file_config , "w")
            f1.write('filespace:%s\n' % fsname)
            for record in config:
                if record[1] == 'p':
                    fileloc = '%s/%s/primary' % (os.path.split(record[3])[0], fsname)
                else:
                    fileloc = '%s/%s/mirror' % (os.path.split(record[3])[0], fsname)
                cmd = "gpssh -h %s -e 'rm -rf %s; mkdir -p %s'"  % (record[2], fileloc, fileloc)
                self.run_command(cmd)
                f1.write("%s:%s:%s/%s\n" % (record[2], record[0], fileloc, os.path.split(record[3])[1]))
            f1.close()
            fs_cmd = '%s/bin/gpfilespace -c %s' % (self.gphome, file_config)
            self.run_gpcommand(fs_cmd)

    def copy_full_backup_to_prev_day(self):
        today = self.backup_timestamp[0:8]
        yesterday = (date.today() - timedelta(2)).strftime("%Y%m%d")
        self.copy_backup_to_prev_day(today, yesterday)

    def copy_incr_backup_to_prev_day(self):
        today = self.backup_timestamp[0:8]
        yesterday = (date.today() - timedelta(1)).strftime("%Y%m%d")
        self.copy_backup_to_prev_day(today, yesterday)
        full_bkday = (date.today() - timedelta(2)).strftime("%Y%m%d")
        self.change_increments_file(today, yesterday, full_bkday)

    def copy_backup_to_prev_day(self, today, yesterday):

        config = self.get_config()
        for record in config:
            if record[1] == 'p':
                cmd = "gpssh -h %s -e 'rm -rf %s/db_dumps/%s ;mkdir -p %s/db_dumps/%s' " % (record[2], record[3], yesterday, record[3], yesterday)
                self.run_command(cmd)
                cmd = "gpssh -h %s -e 'mv %s/db_dumps/%s/*%s* %s/db_dumps/%s/' " % (record[2], record[3], today, self.backup_timestamp, record[3], yesterday)
                self.run_command(cmd)
                cmd = "gpssh -h %s -e 'cd %s/db_dumps/%s/ ;rename %s %s *' " % (record[2], record[3], yesterday,today, yesterday)
                self.run_command(cmd)
        # Change the report file to have yesterday's date
        rpt_file = "%s/db_dumps/%s/gp_dump_%s%s.rpt" % (self.master_dd, yesterday, yesterday, self.backup_timestamp[8:14])
        for line in fileinput.input(rpt_file, inplace=1):
            line = re.sub(today, yesterday, line)
            print str(re.sub('\n','',line))

    def change_increments_file(self, today, yesterday, full_bkday):
        incr_file = "%s/db_dumps/%s/gp_dump_%s%s_increments" % (self.master_dd, full_bkday, full_bkday, self.full_backup_timestamp[8:14])
        for line in fileinput.input(incr_file, inplace=1):
            line = re.sub(today, yesterday, line)
            print str(re.sub('\n','',line))

    def copy_seg_files_to_master(self, location = None):

        hosts = self.get_segments()
        dump_dir = '%s/db_dumps/%s'% (location, self.backup_timestamp[0:8])
        for host in hosts:
            cp_cmd = 'scp %s:%s/*%s* %s/ ' % (host[0].strip(), dump_dir, self.backup_timestamp, dump_dir)
            self.run_command(cp_cmd)

    def remove_full_dumps(self, location = None):
        dump_dir = '%s/db_dumps/%s' % (location, self.backup_timestamp[0:8])
        cmd = 'rm %s/*%s* '%(dump_dir, self.full_backup_timestamp)
        self.run_command(cmd)

    def add_user(self, role, mode):
        """
        @description: Add an entry for each user in pg_hba.conf
        """
        pghba = PgHba()
        new_entry = Entry(entry_type='local',
                            database = 'all',
                            user = role,
                            authmethod = mode)
        pghba.add_entry(new_entry)
        pghba.write()
        # Check if the roles are added correctly
        res = pghba.search(type='local',
                        database='all',
                        user = role,
                        authmethod= mode)
        if not res:
            raise Exception('The entry is not added to pg_hba.conf correctly')

        gpstop = GpStop()
        gpstop.run_gpstop_cmd(reload=True) # reload to activate the users


    def remove_user(self, role):
        """
        @description: Remove an entry for user in pg_hba.conf
        """
        pghba = PgHba()
        entries = pghba.search( user = role )
        for ent in entries:
            ent.delete()
        pghba.write()
        gpstop = GpStop()
        gpstop.run_gpstop_cmd(reload=True) # reload to activate the users


    def get_dump_proc_pid(self, path, proc_name='gp_dump_agent', wait=True):
        cmdStr = "ps -ef | grep %s | grep %s | grep -v grep | grep -v \"sh \-c\"| awk \'{print $2}\'" % (path, proc_name)
        tinctest.logger.info('Command to get the segment dump pid = %s' % cmdStr)
        cmd = Command('get the pid of the dump agent', cmdStr=cmdStr)
        cmd.run(validateAfter=True)
        results = cmd.get_results()
        out = results.stdout.strip()
        max_wait = 50
        time_waited = 1
        while len(out) == 0 and time_waited < max_wait and wait:
            cmd = Command('get the pid of the dump agent', cmdStr=cmdStr)
            cmd.run(validateAfter=True)
            results = cmd.get_results()
            out = results.stdout.strip()
            time_waited += 1
            time.sleep(0.1)
        if time_waited == max_wait:
            raise Exception('Failed to get the dump pid')
        return out

    def wait_gpcrondump_exit(self):
        gpcrondump_path = '%s/bin/gpcrondump' % self.gphome
        pid = self.get_dump_proc_pid(gpcrondump_path, 'python', False)
        max_wait = 660
        waited = 0
        while len(pid) != 0 and waited < max_wait:
            pid = self.get_dump_proc_pid(gpcrondump_path, 'python', False)
            time.sleep(5)
            waited += 5
        if waited >= max_wait:
            raise Exception("Gpcrondump expected to complete within 10 min, 11 min has passed!")

    def get_dump_dir(self, datadir):
        """
        dump path format: datadir/db_dumps/20151016
        """
        dump_folder_name = self.get_cur_date_as_dump_folder()
        return os.path.join(datadir, DEFAULT_DUMP_LOC, dump_folder_name)

    def get_status_file_prefix(self, dbid, master=False, content_id_format=False):

        ismaster = '-1_' if content_id_format else '1_'
        if not master:
            ismaster = '0_'
        # If not master, need to use content ID instead.
        return self.dump_file_prefix + '_status_' + ismaster + str(dbid) + '_'

    def isStringInFile(self, file_path, string):
        found = False
        with open(file_path, "r") as fr:
            for line in fr:
                if string in line:
                    found = True
                    break
        return found

    def get_restore_report_file_path(self, datadir, dumpKey):
        return os.path.join(datadir, self.restore_file_prefix + '_' + dumpKey +'.rpt')

    def get_dump_report_file_path(self, datadir, dump_folder_name):
        return os.path.join(datadir, DEFAULT_DUMP_LOC, dump_folder_name,
                            self.dump_file_prefix + '_' +self.backup_timestamp +'.rpt')

    def get_dump_status_file(self, datadir, timestamp, content_id_format=False):
        status_file_prefix = self.get_status_file_prefix(master =True, dbid = 1,
                                                         content_id_format=content_id_format)
        cur_dump_date_folder = self.get_cur_date_as_dump_folder()
        log_dir = os.path.join(datadir, DEFAULT_DUMP_LOC, cur_dump_date_folder)
        log_file_path = os.path.join(log_dir, status_file_prefix + timestamp)
        return log_file_path

    def write_dirty_string(self, file_path, msg):
        if os.path.exists(file_path):
            with open(file_path, 'a') as fw:
                fw.write(msg)
        else:
            raise Exception("File path: %s not exist!" % file_path)

    def wait_kill_and_verify_dump_agent_on_master(self, datadir, wait_log_msg, verify_log_msg):
        """
        """
        status_file_prefix = self.dump_file_prefix + '_status_*_1_'

        cur_dump_date_folder = self.get_cur_date_as_dump_folder()
        log_dir = os.path.join(datadir, DEFAULT_DUMP_LOC, cur_dump_date_folder)

        last_timestamp = self.get_latest_matching_file(log_dir, status_file_prefix)
        if last_timestamp:
            tinctest.logger.info('The latest timestamp matched for file: %s is %s, wait for new status file' % (status_file_prefix, last_timestamp))
        else:
            tinctest.logger.info('Found no existing file matching %s, wait for new status file' % status_file_prefix)
        dump_agent_pid = self.get_dump_proc_pid(datadir)
        tinctest.logger.info("Obtained segment dump agent process id %s" % dump_agent_pid)

        self.backup_timestamp = self.get_latest_log_timestamp(log_dir, status_file_prefix, last_timestamp)
        log_file_path = self.get_latest_matching_file_path(log_dir, status_file_prefix)

        self.wait_for_log_msg(log_file_path, wait_log_msg)
        tinctest.logger.info("Crash segment dump agent with kill -9 %s" % dump_agent_pid)
        kill_cmd = Command(name = 'kill dump_agent', cmdStr='kill -9 %s' % dump_agent_pid)
        kill_cmd.run(validateAfter = True)

        self.wait_gpcrondump_exit()
        self.verify_dump_crash_detected(datadir, cur_dump_date_folder, verify_log_msg)

    def verify_dump_crash_detected(self, datadir, cur_dump_date_folder, verify_log_msg):
        if verify_log_msg is None or len(verify_log_msg) == 0:
            raise Exception('Invalid log message to verify')
        tinctest.logger.info('Start verifying if crash detected and error message generated.')
        report_file = self.get_dump_report_file_path(datadir=datadir, dump_folder_name=cur_dump_date_folder)
        tinctest.logger.info('report file path %s' % report_file)
        with open(report_file, 'r') as fr:
            lines = fr.readlines()
            for line in lines:
                if verify_log_msg in line:
                    tinctest.logger.info('Expected message %s found in report file %s' % (verify_log_msg, report_file))
                    return
        raise Exception('Expected message %s not found in report file %s' % (verify_log_msg, report_file))

    def get_cur_date_as_dump_folder(self):
        """
        agressive way to get the current date as the folder which contains the dump files.
        in case the dump happens around 12:00 am, it may not be consistent with real dump path
        """
        cur_date = datetime.now().strftime("%Y%m%d")
        tinctest.logger.info('Current date as parent dump folder is %s' % cur_date)
        return cur_date

    def get_latest_matching_file_path(self, log_dir, filename_prefix):
        """
        get the latest matched file in the directory
        """
        matched_status_files= glob.glob(os.path.join(log_dir, filename_prefix + '*'))
        if matched_status_files:
            return max(matched_status_files, key = os.path.getctime)
        return None

    def get_latest_matching_file(self, log_dir, filename_prefix):
        """
        get the latest matched file in the directory
        """
        last_timestamp = None
        matched_status_files= glob.glob(os.path.join(log_dir, filename_prefix + '*'))
        if matched_status_files:
            latest_found = max(matched_status_files, key = os.path.getctime)
            filename_prefix_regex_format = filename_prefix.replace('*', '.*')
            last_timestamp = re.split(filename_prefix_regex_format, latest_found)[1]
        return last_timestamp

    def get_latest_log_timestamp(self, log_dir, filename_prefix, last_timestamp=None):
        tinctest.logger.info('Wait latest timestamp for filename_prefix %s under directory %s' % (filename_prefix, log_dir))
        max_wait = 60
        start_time = datetime.now()
        cur_timestamp = last_timestamp
        while cur_timestamp is None or cur_timestamp == last_timestamp:
            cur_timestamp = self.get_latest_matching_file(log_dir, filename_prefix)
            if self.get_total_seconds((datetime.now() - start_time)) > max_wait:
                raise Exception('Exceeded max waiting time for status file on segment %s' % log_dir)

        tinctest.logger.info('Obtained the latest timestamp %s' % cur_timestamp)
        return cur_timestamp

    def get_total_seconds(self, td):
            return (td.microseconds + (td.seconds + td.days * 24 * 3600) * 1e6) / 1e6

    def wait_for_log_msg(self, log_file, log_msg):
        """
        wait for a specific log message from a log file, wait for 60 sec maximum,
        and this can be tuned dependent on env, normally 60 sec is enough
        """

        tinctest.logger.info('Start waiting for log message %s in log file %s' % (log_msg, log_file))
        start_time = datetime.now()
        max_wait = 300
        waiting = True
        fo = open(log_file, 'r')
        fo.seek(0, 2)
        pos_last = fo.tell()
        contents = ''
        tinctest.logger.info('Waiting for log msg: %s from file %s' % (log_msg, log_file))
        while waiting:
            fo.seek(0,2)
            pos_cur = fo.tell()
            if pos_cur != pos_last:
                bytes =  pos_cur - pos_last
                #set back to last position
                fo.seek(bytes * (-1), 2)
                contents = fo.read(bytes)
                pos_last = pos_cur

            if contents and log_msg in contents:
                waiting = False
                break
            if self.get_total_seconds((datetime.now() - start_time)) > max_wait:
                raise Exception('Exceeded max waiting time for msg %s in status file on segment %s' % (log_msg, log_dir))
        tinctest.logger.info('Log message found.')
        fo.close()

    def populate_tables(self, db_name, num):
        """
        Create enough number of tables for dump to put lock onto, so we have enough time
        window to do test
        """
        tinctest.logger.info('create %s tables in database %s' % (num, db_name))
        create_db = 'DROP DATABASE IF EXISTS %s; CREATE DATABASE %s' % (db_name, db_name)
        self.run_SQLCommand(create_db, dbname='template1')
        for i in range(num):
            self.run_SQLCommand('create table tbl_%s (i int);' % i, dbname=db_name)


    def get_list_of_ddboost_files(self, content, timestamp):
        dump_files = []
        for line in content.split('\n'):
            dump_file = line.split(' ')[0]
            if timestamp in dump_file:
                dump_files.append(dump_file)
        return sorted(dump_files)

    def delete_ddboost_files(self, backup_dir, files, storage_unit=None, id=None):
        for f in files:
            file_path = os.path.join(backup_dir, f)
            cmdStr = "gpddboost --del-file='%s'" % file_path
            if storage_unit:
                cmdStr += " --ddboost-storage-unit=%s" % storage_unit
            if id == 'remote':
                cmdStr += ' --remote'
            Command('run command %s' % cmdStr, cmdStr).run(validateAfter=True)

    def populate_simple_tables(self, dbname):
        create_tables = '''create table ao(i int) with(appendonly=true, orientation=row);
                           create table co(i int) with(appendonly=true, orientation=column);
                           create table heap(i int);'''

        PSQL.run_sql_command(create_tables, dbname = dbname)

    def populate_simple_tables_data(self, dbname, tuple_count=1):
        insert_query = '''insert into ao (select generate_series(1,%s)i);
                          insert into co (select generate_series(1,%s)i);
                          insert into heap (select generate_series(1,%s)i);''' % (tuple_count, tuple_count, tuple_count)

        PSQL.run_sql_command(insert_query, dbname = dbname)

    def cleanup_simple_tables(self, dbname):
        drop_tables = '''drop table if exists heap;
                         drop table if exists ao;
                         drop table if exists co;'''
        PSQL.run_sql_command(drop_tables, dbname = dbname)

    def verifying_simple_tables(self, dbname, tuple_count=None):
        for table in ['ao', 'co', 'heap']:
            query = 'select count(*) from %s' % table
            count = self.run_SQLQuery(query, dbname)[0][0]
            self.assertEquals(count, tuple_count[table])

# Pass in two A.B.C.D version strings, return true if the first one is an earlier version than the second one
def is_earlier_than(current, baseline):
    current = [int(i) if i.isdigit() else 999 for i in current.split('.')]
    baseline = [int(i) if i.isdigit() else 999 for i in baseline.split('.')]
    # e.g. 4.3.9.MS1 is not considered earlier than 4.3.9.0
    for i in range(4):
        if i < len(current) and i < len(baseline):
            if current[i] > baseline[i]:
                return False
        elif i < len(current):
            return False
        elif i < len(baseline): # e.g. 4.3.9.0 is not considered earlier than 4.3.9
            return baseline[i] > 0
    return True

