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
import pexpect as pexpect
import re
import sys
import time, string
import calendar
import unittest2 as unittest
from datetime import date, timedelta
from collections import defaultdict
import threading


import tinctest
from gppylib.db import dbconn
from gppylib.commands.base import Command, REMOTE
from tinctest.case import TINCTestCase
from tinctest.lib import local_path, Gpdiff
from tinctest.runner import TINCTextTestResult
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.utilities.backup_restore import read_config_yaml

try:
    from gppylib.db import dbconn
    from gppylib.gplog import setup_tool_logging, quiet_stdout_logging
    from gppylib.commands.unix import getLocalHostname, getUserName

except ImportError, e:
    sys.exit( 'Error: Cannot import modules. Please check that you have sourced greenplum_path.sh. Detail: '+str(e))

#Fix the search path for modules so it finds the utility lib dir
MYD = os.path.abspath(os.path.dirname(__file__))
mkpath = lambda *x: os.path.join(MYD, *x)
UPD = os.path.abspath(mkpath('..'))
port = os.environ.get('PGPORT')

if UPD not in sys.path:
    sys.path.append(UPD)

if MYD in sys.path:
    sys.path.remove( MYD )
    sys.path.append( MYD )

try:
    import yaml
except ImportError:
    sys.stderr.write("MFR_Testing requires pyyaml.  You can get it from http://pyyaml.org.\n")
    sys.exit(2)


DATABASENAME = None
gpc = 'gpcrondump -x '
gpcrf = '-a'
gpdbr = 'gpdbrestore -a -e -t '
DBINFO = {}

class BackupTestCase(TINCTestCase):
    TSTINFO = None
    do_setup = True
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
        super(BackupTestCase,self).__init__(methodName)
        if BackupTestCase.do_setup:
            self.setup()
        BackupTestCase.do_setup = False

    def setup(self):
        file = os.path.join(os.getenv('MASTER_DATA_DIRECTORY'),
                                      'ddboost_config.yml')
        tinctest.logger.info("==========================================================================")
        tinctest.logger.info("STARTING TEST SUITE")
        tinctest.logger.info("==========================================================================")
        BackupTestCase.TSTINFO = read_config_yaml(file)

        #If a DDBOOST Directory wasn't specify create one
        if 'DDBOOST_DIR' not in BackupTestCase.TSTINFO:
            dir = os.getenv('PULSE_PROJECT') + '_DIR'
        BackupTestCase.TSTINFO['DDBOOST_DIR'] = dir

        tinctest.logger.info("Using %s as the DDBoost directory to store backups" % BackupTestCase.TSTINFO['DDBOOST_DIR'])


        if 'CREATEDB' in BackupTestCase.TSTINFO.keys() and BackupTestCase.TSTINFO['CREATEDB'] == 1:
            self.check_build_dbs()

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

    def filespace_exists(self, fsname = 'filespace_a'):
        fs_sql = "select * from pg_filespace where fsname='%s'" % fsname.strip()
        fs_list = self.run_SQLQuery(fs_sql, dbname = 'template1')
        if len(fs_list) == 1 :
            return True
        return False

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

    def get_ddboost_dump_dirs(self, root_dir, content):
        pattern = re.compile('^[\d]{8}$')
        dir_list = []
        for line in content.split('\n'):
            dump_dir = line.split(' ')[0]
            matched = re.match(pattern, dump_dir)
            if matched:
                dir_list.append(os.path.join(root_dir, matched.group(0)))
        return dir_list

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

    def delete_ddboost_dir(self, backup_dirs, storage_unit=None, id=None):
        for backup_dir in backup_dirs:
            cmdStr = "gpddboost --del-dir='%s'" % backup_dir
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

    def get_config_with_fs(self):
        config_sql = "select dbid, role, hostname, fselocation from gp_segment_configuration, pg_filespace_entry where fsedbid = dbid; "
        return self.run_SQLQuery(config_sql, dbname = 'template1')

    def run_workload(self, dir, dbname, verify = False):
        tinctest.logger.info("Running workload ...")
        if dir.find('dirty') < 0 :
            self.drop_create_database(dbname)
        load_path = local_path(dir) + os.sep
        self.run_sqlfiles(load_path, dbname)
        if verify == True:
            self.validate_sql_files(load_path)

    def drop_create_database(self, dbname):
        if self.check_db_exists(dbname):
            self.drop_database(dbname)
        self.create_database(dbname)

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

    def check_db_exists(self, dbname):
        db_sql = "select datname from pg_database where datname='%s';" % dbname
        result = self.run_SQLQuery(db_sql, dbname = 'template1')
        if result:
            if result[0][0] == dbname:
                    return True
        return False

    def run_SQLQuery(self, exec_sql, dbname):
        with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
            curs = dbconn.execSQL(conn, exec_sql)
            results = curs.fetchall()
        return results

    def run_SQLCommand(self, sql_cmd = None, dbname = None, username = None, password = None,
                        PGOPTIONS = None, host = None, port = None, flags='-a'):
        cmd = PSQL(None, sql_cmd = sql_cmd, dbname = dbname, username = username, password = password, PGOPTIONS = PGOPTIONS, host = host, port = port, flags = flags)
        cmd.run(validateAfter = False)
        result = cmd.get_results()
        return result.rc, result.stdout

    def run_sqlfiles(self, load_path, dbname):
        version = self.get_version()
        for file in os.listdir(load_path):
            if file.endswith(".sql"):
                out_file = file.replace(".sql", ".out")
                assert PSQL.run_sql_file(sql_file = load_path + file, dbname = dbname, port = self.pgport, out_file = load_path + out_file)

    def get_version(self):
        result = PSQL.run_sql_command('select version();', flags='-t -q', dbname='postgres')
        return result.split(" ").pop(5)

    def validate_sql_files(self, load_path):
        for file in os.listdir(load_path):
            if file.endswith(".out"):
                out_file = file
                ans_file = file.replace('.out' , '.ans')
                if os.path.exists(load_path + ans_file):
                    assert Gpdiff.are_files_equal(load_path + out_file, load_path + ans_file)
                else:
                    raise Exception("No .ans file exists for %s " % out_file)

    def run_full_backup(self, dbname = None, option = None, value = None):
        tinctest.logger.info("Running Full Backup ...")
        self.run_gpcrondump(dbname, option, value)

    def run_gpcrondump(self, dbname = None, option = None, value = None, expected_rc = 0, incr_no = 1, incremental = None):
        '''Note : options that dont need a value can be given as a list,
                  option that need a value must be given as an individual element in the list with a valid value '''

        if dbname is None:
            dbname = os.environ.get('PGDATABASE')
            if dbname is None:
                raise Exception('Database name is required to do the backup')

        op_value_list = ['-s', '-u', '-t', '-T', '-B', '-y', '-d', '-f', '-l', '--exclude-table-file', '--table-file']
        option_str = " "
        db_option = ' -x '.join(map(str, dbname.split(',')))
        cmd  = ' -x %s -a ' % db_option
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
        return result

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

    def get_table_list(self,dbname):
        table_sql = "SELECT n.nspname AS schemaname, c.relname AS tablename \
                    FROM pg_class c  LEFT JOIN pg_namespace n ON n.oid = c.relnamespace \
                    LEFT JOIN pg_tablespace t ON t.oid = c.reltablespace \
                    WHERE c.relkind = 'r'::\"char\" AND c.oid > 16384 AND (c.relnamespace > 16384 or n.nspname = 'public');"
        table_list = self.run_SQLQuery(table_sql, dbname = dbname)
        return table_list

    def get_latest_timestamp(self):
        backup_timestamp = self.full_backup_timestamp
        return backup_timestamp

    def get_restore_status_file_on_master(self):
        master_status_file_prefix_new = 'gp_restore_status_-1_1_'
        master_status_file_prefix_old = 'gp_restore_status_1_1_'
        status_file_new = os.path.join(self.master_dd, master_status_file_prefix_new + self.full_backup_timestamp)
        status_file_old = os.path.join(self.master_dd, master_status_file_prefix_old + self.full_backup_timestamp)
        try: # Look for file in new filename format
            fd = open(status_file_new, 'r')
            return status_file_new
        except: # Look for file in old filename format
            fd = open(status_file_old, 'r')
            return status_file_old

    def run_restore(self, dbname = None, option = None, location = None, type = 'full', expected_rc = 0, incr_no = None):
        tinctest.logger.info("Running Restore ...")
        self.run_gpdbrestore(dbname, option, location, type, expected_rc, incr_no)

    def run_gpdbrestore(self, dbname = None, option = None, location = None, type = 'full', expected_rc = 0, incr_no = None):
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
        if option is None:
            cmd = ' -e -t %s -a ' % backup_timestamp
        elif ' -b ' in option:
            cmd = '%s %s' % (option, backup_timestamp[0:8])
            if ' --ddboost ' in option:
                self.restore_with_option_b(location, cmd, type, ddboost=True)
            else:
                self.restore_with_option_b(location, cmd, type)
        elif '-s ' in option:
            cmd =  '%s -a ' % option
        elif ' -R ' in option:
            cmd = '-e -a -R localhost:%s/db_dumps/%s ' % (location, backup_timestamp[0:8])
        else:
            cmd = '-t %s -a %s ' % (backup_timestamp, option)

        cmd_str = '%s/bin/gpdbrestore %s' % (self.gphome, cmd)
        if option is None or ' -b ' not in option:
            result = self.run_gpcommand(cmd_str, expected_rc)
            return
        if option.strip() == '-L':
            self.validateOption(dbname, result, option.strip())

    def restore_with_option_b(self, location, cmd, type = 'full', ddboost=False):
        date_folder = self.backup_timestamp[:8]
        if location is None:
            location = self.master_dd
        if ddboost:
            dump_folder = '%s/%s/%s' % (self.master_dd, location, date_folder)
        else:
            dump_folder = '%s/db_dumps/%s' % (location, date_folder)

        # I just want the options output so I can run the real deal next
        gp_cmd =  "/bin/bash -c 'source {gphome}/greenplum_path.sh; echo '-1' \
                   | {gphome}/bin/gpdbrestore {cmd}'".format(gphome=self.gphome, cmd=cmd)
        _, b_option_results = self.run_command(gp_cmd)
        timestamp_number = -1
        for tentative_input in b_option_results.split('\n'):
            if date_folder in tentative_input and self.backup_timestamp[8:] in tentative_input:
                # we will assume at this point that this will have a [\d+] portion
                # for example: [17] ...... 20160915 143643
                regex_result = re.match(r'^\s+\[(\d+)\]', tentative_input)
                if regex_result:
                    timestamp_number = regex_result.group(1)
                    break

        gp_cmd =  "/bin/bash -c 'source %s/greenplum_path.sh; echo '%s' | %s/bin/gpdbrestore %s'" % (self.gphome, timestamp_number, self.gphome, cmd)
        tinctest.logger.info("restoring with -b and other options: %s" % gp_cmd)
        rc, output = self.run_command(gp_cmd)

        if rc != 0 or 'Enter timestamp number to restore' not in output:
            log_file = local_path('install.log')
            with open(log_file, 'w') as fw:
                fw.write(output)
            if 'Enter timestamp number to restore' not in output:
                raise Exception('gpdbrestore not asking for timestamp number')
            else:
                raise Exception('gpdbrestore with option -b failed')


    def validateOption(self, dbname, result, option):
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
            search_str = "incremental backup"

        if not self.search_optionString(result, search_str):
            raise Exception('The search string for the option is not found in the stdout')

        if option == '-g':
            self.check_config_file()
        if option == '-h':
            if not self.history_table_check(dbname):
                raise Exception('History table is not updated with the --incremental gpcrondump call')
        if option in ('--use-set-session-authorization', '--no-owner', '--no-privileges'):
            self.verifyOption(option)

    def verifyOption(self, option):
        # Check for files in new filename format first
        dump_file = '%s/db_dumps/%s/gp_dump_-1_1_%s.gz' % (self.master_dd, self.backup_timestamp[0:8], self.backup_timestamp)
        zcat_file = '/tmp/foo'
        (rc, out) = self.run_command('zcat %s > %s' % (dump_file, zcat_file))
        if rc !=0: # Dump may be using old-format files, check for those next
            dump_file = '%s/db_dumps/%s/gp_dump_1_1_%s.gz' % (self.master_dd, self.backup_timestamp[0:8], self.backup_timestamp)
            (rc, out) = self.run_command('zcat %s > %s' % (dump_file, zcat_file))
            if rc != 0:
                raise Exception('zcat of the dump file failed')
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

    def search_optionString(self, result, search_str):
        for line in result.splitlines():
            if search_str in line:
                return True
        return False

    def check_config_file(self):
        dump_config_file = '%s/db_dumps/%s/gp_master_config_files_%s.tar' % (self.master_dd, self.backup_timestamp[0:8], self.backup_timestamp)
        if not os.path.exists(dump_config_file):
            raise Exception('Config tar file not created by -g option with --incremental')

    def history_table_check(self, dbname):
        history_sql = 'select dump_key,options from public.gpcrondump_history;'
        result = self.run_SQLQuery(history_sql, dbname = 'template1')
        for (dump_key, options) in result:
            if self.backup_timestamp == dump_key.strip() and dbname in options:
                    return True
        return False

    def history_table_exists(self, dbname):
        sql = "SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE  table_schema = 'public' AND table_name = 'gpcrondump_history');"
        res = self.run_SQLQuery(sql, dbname = dbname)
        # We assume that we get one value
        return res[0][0]

    def get_segment_count(self):
        """
        PURPOSE: Pulls the GP Cluster configuration from the database to determine the segment db count
        inorder to determine the correct backup file count that should be contained in the backup set
        """

        user = "gpadmin"
        dburl = dbconn.DbURL(dbname='template1', username=user)

        conn = None

        #Attempt to establish a connection to the database
        try:
            conn = dbconn.connect(dburl, utility=False)
        except:
            tinctest.logger.error("Database is not responding. Is it up and running?")
            raise Exception("Database is not responding. Is it up and running?")

        mysql = ''' SELECT count(*) FROM gp_segment_configuration WHERE role='p' '''

        #Execute the sql command to generate the configuration of the cluster
        seginf = dbconn.execSQL(conn, mysql)

        segcnt = None
        #Process the query output
        for i in seginf:
            (segcnt) = i

        conn.close()

        return int(segcnt[0])

        ### End function get_segment_count

    def check_build_dbs(self):
        """
        PURPOSE: Verify all databases exist that are listed in yaml if not create them accordingly
        If they do exist verify all tables exist and contain rows if not drop and recreate the database
        """
        errcnt = 0

        tbl_names = [ 'customer', 'nation', 'region', 'orders', 'part', 'partsupp', 'supplier', 'lineitem' ]

        tinctest.logger.info("Executing setup. Verifying selected databases exist. If not will create them.")

        for dbn in BackupTestCase.TSTINFO['DATABASES'].keys():
            DBNAME = dbn

            do_loadDB = False
            tinctest.logger.info("Checking to see if database %s exists" % DBNAME)
            tinctest.logger.info("Database %s exists. Dropping and recreating it." % DBNAME)
            if drop_create_db(DBNAME):
                do_loadDB = True
            else:
                tinctest.logger.error("Failed to drop and create database %s" % DBNAME)
                errcnt += 1

            if do_loadDB:
                tinctest.logger.info("Building database stucture and generating data.")
                ok = build_database(DBNAME)
                if not ok:
                    tinctest.logger.error("DBBuild ERROR: Unable to load database %s with data" % DBNAME)
                    errcnt += 1
                    continue
                else:
                    tinctest.logger.info("Successfully loaded database %s with data" % DBNAME)

        ### End function check_build_dbs()

    def run_command(self, command):
        cmd = Command(name='run %s' % command, cmdStr='%s' % (command))
        try:
            cmd.run(validateAfter=True)
        except Exception, e:
            tinctest.logger.error("Error running command %s" % e)
        result = cmd.get_results()
        return result.rc, result.stdout

    def validate_restore(self, dir, dbname):
        tinctest.logger.info("Validating workload after restore...")
        load_path = local_path(dir) + os.sep
        self.run_sqlfiles(load_path, dbname)
        self.validate_sql_files(load_path)

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
                raise Exception("Restore Validation Failed for table %s" % tablename)
        tinctest.logger.info("Compared table data after restore")

    def compare_ao_co_tuple_count(self, dbname):
        partition_info_ao = self.get_partition_list('ao', dbname)
        self.verify_stats(dbname, partition_info_ao)

        partition_info_co = self.get_partition_list('co', dbname)
        self.verify_stats(dbname, partition_info_co)

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
            tuple_count_sql = "select to_char(sum(tupcount::bigint), '999999999999999999999') from gp_dist_random('pg_aoseg.%s')" % tupletable
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

    def get_config(self):
        config_sql = "select dbid, role, hostname, fselocation from gp_segment_configuration gp,pg_filespace_entry, pg_filespace fs where fsefsoid = fs.oid and fsname='pg_system' and gp.dbid=pg_filespace_entry.fsedbid;"
        return self.run_SQLQuery(config_sql, dbname = 'template1')

    def check_filter_file_exists(self, expected=False, prefix=None, location=None):
        if prefix is None:
            prefix = ""
        else:
            prefix = prefix + "_"

        if location is None:
            filter_file = '%s/db_dumps/%s/%sgp_dump_%s_filter' % (self.master_dd, self.backup_timestamp[0:8], prefix, self.backup_timestamp)
        else:
            filter_file = '%s/db_dumps/%s/%sgp_dump_%s_filter' % (location, self.backup_timestamp[0:8], prefix, self.backup_timestamp)
        if (not os.path.exists(filter_file)) and expected:
            raise Exception('Expected filter file %s to be present, but was not found.' % (filter_file))
        elif os.path.exists(filter_file) and (not expected):
            raise Exception('Found filter file %s when it should not exist.' % (filter_file))

        return


    def check_ddboost_cfg_file():
        """
        PURPOSE: Verify the ddboost config file .ddconfig exists in gpadmin's home dir if not create it and
        distribute it to all segment servers
        """
        tinctest.logger.info("Verifying if DDBoost is configured to run backups and restores")
        cmdOut = mkpath('ddboost_config_check.out')
        cmdDD = "gpddboost --verify"
        (rc, out) = self.run_command(cmdDD)
        if rc !=0:
            tinctest.logger.error("Unable to verify DDBoost user configuration/setup.")
            tinctest.logger.error("DDBoost backups and restores will most likely faile.")
            tinctest.logger.error("Output from command %s" % cmdDD)
            #for line in out:
            for line in out.splitlines():
                tinctest.logger.error(line)

    ### End function check_ddboost_cfg_file()

    def scan_out(self, out):
        """
        PURPOSE: Get the dumpkey for the gpcrondump so that a restore can be issued against it
        also pull any warnings or errors from the output to verify exit status of gpcrondump MPP-14553
        """
        DmpKey = None
        MsgS = []

        for line in out.splitlines():
            if string.find(line,'Dump key') >= 0:
                DmpKey = line.split()[-1]
            elif string.find(line,'[WARN]') >= 0 or string.find(line,'[WARNING]') >= 0:
                MsgS.append(line.strip('\n'))
            elif string.find(line,'[ERROR]') >= 0:
                MsgS.append(line.strip('\n'))
            elif string.find(line,'[CRITICAL]') >= 0:
                MsgS.append(line.strip('\n'))
            elif string.find(line,'failed') >= 0:
                MsgS.append(line.strip('\n'))

        return DmpKey, MsgS

    ### End function scan_out()

    def end_proc(proc):
        '''
        end_proc executes pkill against a process name.
        '''

        killCmd = "pkill %s" % proc
        cmdOutFile = 'pkill_'+proc+'_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'

        cmdOut = mkpath(cmdOutFile)
        (rc, out) = self.run_command(killCmd)

    ### End function end_proc

    def get_db_info(self, DBNAME):

        dbtblinfo = defaultdict(dict)
        mysql1 = '''
        select table_schema, table_name, pg_total_relation_size(table_schema || '.' || table_name)
        as fulltblsize from information_schema.tables where table_catalog=\'''' + DBNAME + '''\'
        and table_schema='public' and table_name NOT LIKE 'e_%';;
        '''

        dburl = dbconn.DbURL(dbname=DBNAME, username='gpadmin')

        try:
            conn = dbconn.connect(dburl)
        except:
            tinctest.logger.info('Database is not responding. Is it up and running?')
            return dbtblinfo

        sizeout = dbconn.execSQL(conn,mysql1)

        for i in sizeout:
            sch, tbl, bytes = i
            if tbl == 'gpcrondump_history':
                continue
            else:
                l = sch +"."+ tbl
                dbtblinfo[l]['Size'] = bytes

        for tble in dbtblinfo.keys():
            mysql2 = ''' select count(*) from ''' + tble

            cntout = dbconn.execSQL(conn,mysql2)
            for i in cntout:
                dbtblinfo[tble]['RowCount'] = i[0]

        conn.close()

        return dbtblinfo

    ### End function get_db_info()

    def get_ext_ip(self):
        """
        PURPOSE: Pulls the external IP Address from the ifconfig output so it can be used as a
        unique directory on the DD for storing backups
        """

        cmdOutFile = 'ifconfig_output_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'

        cmd = '/sbin/ifconfig'

        cmdOut = mkpath(cmdOutFile)
        (rc, out) = self.run_command(cmd)

        iname = None
        INTF = defaultdict(dict)
        int_cnt = 0
        for i in out:
            ip = None
            if i.startswith('eth'):
                iname = i.split(' ')[0]
            if 'inet addr:' in i:
                ip = i.lstrip(' ').split(':')[1].rstrip('  Bcast')
                if '127.0.0' not in ip and '172.28.' not in ip:
                    INTF[int_cnt][iname] = ip
                    int_cnt += 1

        for cnt in sorted(INTF.keys()):
            for int, ip in INTF[cnt].iteritems():
                return ip.replace('.','_')

    ### End function get_ext_ip()

    def convert_dmpkey_to_date(self, dumpkey):
        """
        PURPOSE: Takes a dumpkey in the format of yyyymmddhhmmss and returns yyyy-MONTH-dd hh:mm:ss
        to be used for listing the ddboost backup contents with the gpmfr.py utility
        """
        #Pull out the date information
        year = dumpkey[:4]
        month = dumpkey[4:6]
        day = dumpkey[6:8]
        hour = dumpkey[8:10]
        min = dumpkey[10:-2]
        sec = dumpkey[-2:]

        if month[0] == 0:
            month = calendar.month_name[int(month[1])]
        else:
            month = calendar.month_name[int(month)]

        mydate = "%s-%s-%s %s:%s:%s" % (year,month,day,hour,min,sec)

        return mydate

    ### End function convert_dmpkey_to_date

    def mfr_list_sets(self, name, dumpk1, dumpk2, remote=False, return_list=False):
        """
        PURPOSE: Lists backup sets seen on either the local or remote DD and verifies the dump keys provided
        exist in the backup set(s) listed
        """

        list_objs = []

        if dumpk1 != None:
            list_objs.append(self.convert_dmpkey_to_date(dumpk1))

        if dumpk2 != None:
            list_objs.append(self.convert_dmpkey_to_date(dumpk2))

        cmdOutFile = name+'_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'

        cmd = 'gpmfr.py --list --master-port=%s' % port

        if remote:
            cmd += ' --remote'

        cmdOut = mkpath(cmdOutFile)
        #(ok, out) = shell.run(cmd, cmdOut, 'w')
        (rc, out) = self.run_command(cmd)

        bu_set_list = {}
        num = 0
        found = 0
        status = True
        msg = None
        for bu_date in list_objs:
            for line in out.splitlines():
                if bu_date in line:
                    found += 1
                if line.startswith('   20'):
                    bu_set_list[num] = line.lstrip('   ')
                    num += 1

        if return_list:
            return bu_set_list

        if found == len(list_objs):
            return status, msg
        else:
            status = False
            msg = "Unable to find both backup sets in the list output\n"
            for i in out:
                msg += i

            return status, msg

    ### End function mfr_list_sets

    def mfr_list_files(self, name, dumpk1, dumpk2, op, remote=False):
        """
        PURPOSE: Lists backup set files seen on either the local or remote DD and verifies the dump keys provided
        exist in the backup set(s) and the correct number of files are listed based on the cluster configuration
        """

        correct_file_cnt = self.get_segment_count() + 7

        list_objs = {}

        if op == 'file':
            if dumpk1 != None:
                list_objs[self.convert_dmpkey_to_date(dumpk1)] = dumpk1

            if dumpk2 != None:
                list_objs[self.convert_dmpkey_to_date(dumpk2)] = dumpk2

        else:
            #Dumpk2 = Latest and Dumpk1 is the oldest always in this utility based on when it was gathered
            if op == 'latest':
                bu_sets = self.mfr_list_sets("determin_latest",dumpk1,dumpk2,remote=remote,return_list=True)
                #list_objs[op.upper()] = dumpk2
                list_objs[op.upper()] = bu_sets[len(bu_sets.keys()) - 1].split(' (')[1].split(')')[0]
            else:
                bu_sets = self.mfr_list_sets("determin_oldest",dumpk1,dumpk2,remote=remote,return_list=True)
                #list_objs[op.upper()] = dumpk1
                list_objs[op.upper()] = bu_sets[0].split(' (')[1].split(')')[0]

            tinctest.logger.info("Using Dump Key %s as %s backup" % (list_objs[op.upper()],op.upper()))

        status = True
        errmsgs = []
        results = 0
        for bu_date in list_objs.keys():

            cmd = "gpmfr.py --list-files=\"%s\" --master-port=%s" % (bu_date, port)

            if remote:
                cmd += " --remote"

            cmdOutFile = name
            if remote:
                cmdOutFile += '_remote'
            else:
                cmdOutFile += '_local'

            cmdOutFile += '_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
            cmdOut = mkpath(cmdOutFile)
            #(ok, out) = shell.run(cmd, cmdOut, 'a')
            (rc, out) = self.run_command(cmd)

            found = 0
            tofind = "_%s" % list_objs[bu_date]
            #for line in out:
            for line in out.splitlines():
                if tofind in line:
                    found += 1

            """if found == correct_file_cnt:
                results += 1
            else:
                txt = "Backup set %s [%s] missing files\n" % (bu_date,list_objs[bu_date])
                errmsgs.append(txt)
                txt = "Expected %s files found %s\n" % (correct_file_cnt,found)
                errmsgs.append(txt)

                #for line in out:
                for line in out.splitlines():
                    errmsgs.append(line)

        if results != len(list_objs.keys()):
            status = False
        """

        return status, ''.join(errmsgs)

    ### End function self.mfr_list_files

    def do_restr_test(self, name,DBNAME,dumpkey):
        """
        PURPOSE: Conducts the restore tests requested
        """
        global DBINFO

        #Drop a linefeed onto the screen so gptest output shows up better
        sys.stdout.write("\n")

        cmdOutFile = name+'_restore_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'

        cmdRestore = None
        testtype = "Executing"

        cmdRestore = gpdbr + dumpkey +' --ddboost'

        tinctest.logger.info("[%s]" % name)
        tinctest.logger.info("%s gpdbrestore of database %s to DD via ddboost using command" % (testtype, DBNAME))

        tinctest.logger.info(cmdRestore)

        cmdOut = mkpath(cmdOutFile)
        tinctest.logger.info("Output of command located in file %s" % cmdOutFile)
        #(ok, out) = shell.run(cmdRestore, cmdOut, 'w')
        (rc, out) = self.run_command(cmdRestore)
        if rc==0:
            ok = True
        else:
            ok = False

        #Don't need dump key but, don't need another function so it will always come back None
        (GPDmpKey, ErrMsgs) = self.scan_out(out)

        if len(ErrMsgs) > 0:
            for i in ErrMsgs:
                tinctest.logger.error(i)

        #Validate DB size and table info before saying PASSED
        atr_info = self.get_db_info(DBNAME)

        badcompare = False
        if len(DBINFO[DBNAME].keys()) != 0:
            tinctest.logger.info("Comparing database table row counts from the backup to what they are now.")
            for tbl in DBINFO[DBNAME].keys():
                if tbl in atr_info.keys() and DBINFO[DBNAME][tbl]['RowCount'] != atr_info[tbl]['RowCount']:
                    tinctest.logger.error("Table %s different before and after restore" % tbl)

                    tinctest.logger.error("Prior To Restore:  Size=%d  RowCount=%d"
                    % (DBINFO[DBNAME][tbl]['Size'],DBINFO[DBNAME][tbl]['RowCount']))

                    tinctest.logger.error("After The Restore: Size=%d  RowCount=%d" %
                    (atr_info[tbl]['Size'],atr_info[tbl]['RowCount']))

                    badcompare = True
                    ok = False
                elif tbl not in atr_info.keys():
                    tinctest.logger.error("WARNING: %s existed in the database prior to the " % tbl)
                    tinctest.logger.error("restore but, doesn't exist after")
                    badcompare = True
                    ok = False
                else:
                    tinctest.logger.info("Table %s is the same before and after the restore" % tbl)
        else:
            tinctest.logger.error("No Database Information was collected prior to restore.")
            tinctest.logger.error("Unable to validate if appropriate data was restored.")
            tinctest.logger.info("Please manually verify database for completeness.")

        msg = None
        if not ok and badcompare:
            if badcompare:
                tinctest.logger.error("Marking Restore as FAILED due to bad row count comparison or tables missing.")
                msg = "Marking Restore as FAILED due to bad row count comparison or tables missing.";
            if not ok:
                tinctest.logger.error("Restore failed with gpdbrestore errors")
                msg = "Restore failed with gpdbrestore errors"
                #tinctest.logger.error("GPDBRESTORE ERROR on command: %s" % shell.lastcmd)
                #msg += "GPDBRESTORE ERROR on command: %s" % shell.lastcmd

            msg += "Backup of %s via ddboost failed to run successfully please review %s for more details.\n" \
            % (DBNAME, cmdOutFile)
            tinctest.logger.error(msg)
            for i in ErrMsgs:
                if 'WARNING' in i:
                    continue
                else:
                    msg += "%s\n" % i
            tststatus = 'FAILED'
        else:
            tststatus = 'PASSED'

        tinctest.logger.info("[%s] %s" % (name, tststatus))

        return tststatus, msg

    ### End function do_restr_test()

    def do_bkup_test(self, name, DBNAME, bu_options):
        """
        PURPOSE: Conducts the backup tests requested
        """
        global DBINFO

        #Drop a linefeed onto the screen so gptest output shows up better
        sys.stdout.write("\n")

        ErrMsgs = None
        tststatus = None

        cmdOutFile = name+'_backup_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
        cmdOut = mkpath(cmdOutFile)
        if bu_options == None:
            cmdBackup = gpc + DBNAME +' -a -G --ddboost'
        else:
            cmdBackup = gpc + DBNAME +' -a -G --ddboost --max-streams=15 ' + bu_options

        tinctest.logger.info("[%s]" % name)
        tinctest.logger.info("Executing gpcrondump of database %s to DD via ddboost using command" % (DBNAME))

        tinctest.logger.info(cmdBackup)

        tinctest.logger.info("Output of command located in file %s" % cmdOutFile)
        #(ok, out) = shell.run(cmdBackup, cmdOut, 'w')
        (rc, out) = self.run_command(cmdBackup)

        (GPDmpKey, ErrMsgs) = self.scan_out(out)
        msg = None

        if rc!=0:
            tststatus = 'FAILED'
            msg = "Backup of %s via ddboost failed to run successfully please review %s for more details.\n" \
            % (DBNAME, cmdOutFile)
            tinctest.logger.error(msg)
            if len(ErrMsgs) > 0:
                for i in ErrMsgs:
                    if 'WARNING' in i:
                        continue
                    else:
                        msg += "%s\n" % i
                        tinctest.logger.error(i)
        else:
            tststatus = 'PASSED'

        tinctest.logger.info(tststatus)

        tinctest.logger.info("[%s] %s" % (name, tststatus))

        #Get the database info
        DBINFO[DBNAME] = self.get_db_info(DBNAME)

        return GPDmpKey, msg

    def do_incr_bkup_test(self, name, DBNAME, bu_options):
        """
        PURPOSE: Conducts the backup tests requested
        """
        global DBINFO

        #Drop a linefeed onto the screen so gptest output shows up better
        sys.stdout.write("\n")

        ErrMsgs = None
        tststatus = None

        cmdOutFile = name+'_backup_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
        cmdOut = mkpath(cmdOutFile)
        if bu_options == None:
            cmdBackup = gpc + DBNAME +' -a -G --ddboost --incremental'
        else:
            cmdBackup = gpc + DBNAME +' -a -G --ddboost --max-streams=15 --incremental' + bu_options

        tinctest.logger.info("[%s]" % name)
        tinctest.logger.info("Executing gpcrondump of database %s to DD via ddboost using command" % (DBNAME))

        tinctest.logger.info(cmdBackup)

        tinctest.logger.info("Output of command located in file %s" % cmdOutFile)
        #(ok, out) = shell.run(cmdBackup, cmdOut, 'w')
        (rc, out) = self.run_command(cmdBackup)

        (GPDmpKey, ErrMsgs) = self.scan_out(out)
        msg = None

        if rc!=0:
            tststatus = 'FAILED'
            msg = "Backup of %s via ddboost failed to run successfully please review %s for more details.\n" \
            % (DBNAME, cmdOutFile)
            tinctest.logger.error(msg)
            if len(ErrMsgs) > 0:
                for i in ErrMsgs:
                    if 'WARNING' in i:
                        continue
                    else:
                        msg += "%s\n" % i
                        tinctest.logger.error(i)
        else:
            tststatus = 'PASSED'

        tinctest.logger.info(tststatus)

        tinctest.logger.info("[%s] %s" % (name, tststatus))

        #Get the database info
        DBINFO[DBNAME] = self.get_db_info(DBNAME)

        return GPDmpKey, msg

class CancelReplicateThreader(threading.Thread):
    '''
    Creates a thread that conducts a replication of a backup set so a cancel test can be conducted
    '''
    def __init__(self,name,dumpkey):
        threading.Thread.__init__(self)
        self.name = name
        self.dumpkey = dumpkey
        self.bu_set = convert_dmpkey_to_date(dumpkey)
        self.cmd = None


    def stop(self):
        pid = self.cmd.pid
        self.cmd.send_signal(signal.SIGINT)
        return pid

    def run(self):
        '''
        cmdOutFile = self.name+'_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
        cmdOut = mkpath(cmdOutFile)
        tinctest.logger.info("Executing replication of backup set %s" % self.bu_set)
        (ok,out) = shell.run(cmdRepl,cmdOut,'w')

        if not ok:
            msg = "Replication of backup set %s was terminated as expected\n" % self.bu_set
            tinctest.logger.info(msg)
        '''
        cmdRepl = "gpmfr.py --replicate=\"%s\" --max-streams=15 --master-port=%s" % (self.bu_set, port)
        #self.cmd = subprocess.Popen(cmdRepl,shell=True)
        self.cmd = subprocess.Popen(cmdRepl)


### End function do_bkup_test()


def my_reload_table(pTableName, pTableDefinition,
    withClause = "", preExecuteCmd = "set search_path to public", createExternalTable = False):
    '''
    Drop a table; re-create it; and re-load the data into it.
    For heap table, there is no prefix added to the table.
    For AO table, "ao_" is added to the prefix.
    For CO table, "co_" is added to the prefix.
    @param pTableName: The name of the table.
        pTableDefinition: The list of column names and data types,
        enclosed in parentheses, for example:
        "(x int, y float)"
    @param withClause: This is an optional string that may contain a "WITH"
        clause such as "WITH (APPENDONLY=True, ORIENTATION='column')".
        It could also contain other clauses, like "DISTRIBUTED BY...".
    @param preExecuteCmd: This is an optional string that will be executed
        prior to the CREATE TABLE statement.  For example, this string
        might contain "SET SEARCH_PATH TO AO_SCHEMA;" to set the
        search path to include the schema named "AO_SCHEMA".
    @param createExternalTable: set this to False if you have already created
    the external table and don't want to create it again.
    '''

    global DATABASENAME

    ok = True

    if withClause == None:
        withClause = ""
        tablePrefix = ""
    else:
        if withClause.find("column")>0:
            tablePrefix = "co_"
        elif withClause == "":
            tablePrefix = ""
        else:
            tablePrefix = "ao_"

    preExecuteCmd = "set search_path to public;"

    ok = True
    if ok:
        cmd = 'drop table if exists ' + tablePrefix + pTableName + ' cascade'
        (rc, data_out)  = run_SQLCommand(preExecuteCmd + cmd, dbname=DATABASENAME)
        if rc!=0:
            ok = False
    if ok:
        cmd = 'CREATE TABLE ' + tablePrefix + pTableName + pTableDefinition
        cmd = "".join(cmd.split('\n'))
        cmd = cmd + withClause
        (rc, data_out)  = run_SQLCommand(preExecuteCmd + cmd, dbname=DATABASENAME)
        if rc!=0:
            ok = False
    if ok:
        cmd = 'insert into ' + tablePrefix + pTableName + ' values (1, 100);'
        (rc, data_out)  = run_SQLCommand(preExecuteCmd + cmd, dbname=DATABASENAME)
        if rc!=0:
            ok = False
    if not ok:
        tinctest.logger.error("Error detected.  This message may give a clue:")
        raise Exception('Unable to load ' + tablePrefix + pTableName)

### End function my_reload_table()
def build_database(DBNAME):
    """
    PURPOSE: Verify the databases listed in the config yaml exist. If not create them based on
    the set parameters provided in the yaml
    """
    global DATABASENAME
    DATABASENAME = DBNAME
    dbstruc = []
    for type in BackupTestCase.TSTINFO['DATABASES'][DBNAME]['STRUCTURE']:
        if type == 'AppendOnly':
            dbstruc.append('appendonly=True')
        elif type == 'ColumnOriented':
            dbstruc.append('orientation=column')
        elif 'Compressed' in type:
            compinf = type.split(',')
            comptype = compinf[+1]
            complevel = compinf[-1]
            if comptype == 'quicklz' or comptype == 'zlib':
                dbstruc.append('compresstype='+comptype)
            else:
                dbstruc.append(comptype)

            dbstruc.append('compresslevel='+complevel)

    if len(dbstruc) > 0:
        wc = "WITH (" + ','.join(dbstruc) + ")"
        my_reload_table('test_table', '(x int, y float)', withClause=wc)
    else:
        my_reload_table('test_table', '(x int, y float)', withClause="")

    return True

    ### End function build_database()

def run_command(command):
    cmd = Command(name='run %s' % command, cmdStr='%s' % (command))
    try:
        cmd.run(validateAfter=True)
    except Exception, e:
        tinctest.logger.error("Error running command %s" % e)
    result = cmd.get_results()
    return result.rc, result.stdout

def run_SQLCommand(sql_cmd = None, dbname = None, username = None, password = None,
                    PGOPTIONS = None, host = None, port = None):
    cmd = 'psql -c "%s" -d %s' % (sql_cmd, dbname)
    (rc, out) = run_command(cmd)
    return rc, out

def drop_create_db(DBNAME):
    """
    PURPOSE: Drops and recreates the specified database prior to the full database restore.
    currently required when NFS full database restores are conducted
    """

    filen = 'dropDB_'+DBNAME+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
    cmdOut = mkpath(filen)
    cmdDrop = "dropdb %s" % (DBNAME)
    (rc, out) = run_command(cmdDrop)
    filen = 'createDB_'+DBNAME+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
    cmdOut = mkpath(filen)
    cmdCreate = "createdb %s" % (DBNAME)
    (rc, out) = run_command(cmdCreate)
    if rc==0:
        return True
    else:
        return False

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
