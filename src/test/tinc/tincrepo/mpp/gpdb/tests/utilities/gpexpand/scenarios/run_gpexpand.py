"""
Copyright (c) 2004-Present Pivotal Software, Inc.

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

Expansion utility methods
"""
import os
import socket
import pexpect
import time
import datetime
import shutil

import tinctest

from tinctest.lib import run_shell_command
from tinctest.lib import local_path
from tinctest.lib import Gpdiff

from mpp.models import MPPTestCase
from mpp.models import MPPDUT
from mpp.lib.PgHba import PgHba

from gppylib.commands.base import Command, REMOTE
from gppylib.db import dbconn
from gppylib.db.dbconn import UnexpectedRowsError
from gppylib.gparray import GpArray
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.lib.gpfilespace import Gpfilespace

@tinctest.skipLoading('scenario')
class GpExpandTests(MPPTestCase):

    def log_and_test_gp_segment_config(self, message="logging gpsc"):
        tinctest.logger.info( "[gpsc_out] :" + message)
        seg_config = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid", flags ='-q -t')
        tinctest.logger.info( seg_config)
        if not self.test_gpsc():
            self.fail("primary and mirror are on the same host")

    def test_gpsc(self):
        """
        check if primary and mirror are on the same host
        """
        max_segs = self.get_value_from_query("select max(content) from gp_segment_configuration ;")
        tinctest.logger.info( max_segs)
        ret_flag = True
        for i in (range(int(max_segs)+1)):
            prim_host = self.get_value_from_query("select hostname from gp_segment_configuration where role='p' and content ='%s';" %(i))
            mirr_host = self.get_value_from_query("select hostname from gp_segment_configuration where role='m' and content ='%s';" %(i))
            tinctest.logger.info( prim_host)
            tinctest.logger.info( mirr_host)
            if prim_host == mirr_host:
                ret_flag = False
                tinctest.logger.info( "mirror and primary are on the same host %s for content id %s" %(prim_host, i))
        return ret_flag

    def run_expansion(self, mapfile, dbname, mdd=os.environ.get("MASTER_DATA_DIRECTORY"), output_dir=os.environ.get("MASTER_DATA_DIRECTORY"), interview = False, validate=True, validate_gpseg_conf=True,
                      validate_pghba=True):
        """
        Run an expansion test based on the mapping file
        """
        outfile = output_dir + "/run_expansion.out"
        errfile = output_dir + "/run_expansion.err"

        self.log_and_test_gp_segment_config(message="before running epxnasion")

        cmd = Command(name='run gpexpand', cmdStr="export MASTER_DATA_DIRECTORY=%s; echo -e \"y\\n\" | gpexpand -i %s -D %s" % (mdd, mapfile, dbname))
        tinctest.logger.info("Running expansion setup: %s" %cmd)
        try:
            cmd.run(validateAfter=validate)
        except Exception, e:
            self.fail("gpexpand failed. \n %s" %e)

        if validate_gpseg_conf:
            self._validate_gpseg_conf(mapfile)

        if validate_pghba:
            # Validate the new entries that will be added after expansion in pg_hba
            self._validate_pghba_entries()
            
        results = cmd.get_results()
        str_uni_idx ="Tables with unique indexes exist.  Until these tables are successfully"
        found_uni_idx_msg = False

        for line in results.stdout.splitlines():
            if line.find(str_uni_idx) != -1:
                found_uni_idx_msg = True

        if found_uni_idx_msg == False:
            tinctest.logger.error("stdout from failed index in expand command: %s" % results.stdout)
            tinctest.logger.error("stderr from failed index in expand command: %s" % results.stderr)
            self.fail("Message for unique indexes not printed during gpexpand")

        with open(outfile, 'w') as output_file:
            output_file.write(results.stdout)
        with open(errfile, 'w') as output_file:
            output_file.write(results.stderr)

        self.log_and_test_gp_segment_config(message="after running expansion")

        return results

    def _validate_gpseg_conf(self, mapfile):
        """
        Validate if the new hosts are added to gp_segment_configuration table
        Parse the expansion map and populate the datafields into a list
        """
        tinctest.logger.info("Verifying expanded segments in gp_segment_configuration table ...")

        with open(mapfile) as fp:
            for line in fp:
                tinctest.logger.info("Checking for segment: %s" %line)
                fields = line.split(':')
                if len(fields) == 8:
                    cmd = """select count(*)
                    from gp_segment_configuration
                    where hostname = '%s'
                    and address = '%s'
                    and port = %s
                    and content = %s
                    and role = '%s'
                    and replication_port = %s""" % (fields[0], fields[1], fields[2],fields[5], fields[6], fields[7])
                else:
                    cmd = """select count(*)
                    from gp_segment_configuration
                    where hostname = '%s'
                    and address = '%s'
                    and port = %s
                    and content = %s
                    and role = '%s'""" % (fields[0], fields[1], fields[2], fields[5], fields[6])
                # CHECK FOR DBID ONCE MPP-24082 is RESOLVED    
                with dbconn.connect(dbconn.DbURL()) as conn:
                    row = dbconn.execSQLForSingleton(conn, cmd)
                    
                    if row != 1:
                        self.log_and_test_gp_segment_config(message="failed gpexpand validation")
                        self.fail("Expected segment not found in gp_segment_configuration: %s" %line)

    def _validate_pghba_entries(self):
        """
        Validate if new entries for all the hosts are added to pg_hba.conf files in all the segments
        """
        tinctest.logger.info("Verifying pg_hba entries for all segment hosts")
        segment_dirs_sql = """select distinct hostname, fselocation, content from gp_segment_configuration, pg_filespace_entry
                              where content > -1 and fsedbid = dbid"""

        dburl = dbconn.DbURL()
        pg_hba_files = []
        hosts = set()
        with dbconn.connect(dburl) as conn:
            cursor = dbconn.execSQL(conn, segment_dirs_sql)
            try:
                for row in cursor.fetchall():
                    host = row[0]
                    segment_dir = row[1]
                    segment_no = row[2]
                    pg_hba = os.path.join(segment_dir, 'pg_hba.conf')
                    pg_hba_temp = '/tmp/pg_hba_%s_%s' %(host, segment_no)
                    if not "cdbfast_fs" in segment_dir and not "filespace" in segment_dir:
                    # We dont want to do this for filespace entries in pg_filepsace_entry. 
                    # The file space names have prefix cdbfast_fs. 
                    # So if keyword cdbfast_fs appears in the dirname, we skip it"""
                        if os.path.exists(pg_hba_temp):
                            os.remove(pg_hba_temp)
                        cmdstr = 'scp %s:%s %s' %(host, pg_hba, pg_hba_temp)
                        if not run_shell_command(cmdstr=cmdstr, cmdname='copy over pg_hba'):
                            raise Exception("Failure while executing command: %s" %cmdstr)
                        self.assertTrue(os.path.exists(pg_hba_temp))
                        pg_hba_files.append(pg_hba_temp)
                        hosts.add(host)
            finally:
                cursor.close()

        for f in pg_hba_files:
            tinctest.logger.info("Verifying pg_hba entries for file: %s" %f)
            self._verify_host_entries_in_pg_hba(f, hosts)

        tinctest.logger.info("Completed verifying pg_hba entries for all segment hosts successfully")


    def _verify_host_entries_in_pg_hba(self, filename, hosts):
        """
        Verify that a valid trust entry is there in pg_hba for every host in hosts

        @param filename: Complete path to the pg_hba file
        @type filename: string

        @param hosts: A list of hostnames whose entries are expected to be present in pg_hba
        @type hosts: set
        """
        pg_hba = PgHba(filename)
        for host in hosts:
            matches = pg_hba.search(type='host', user='all', database='all', address=socket.gethostbyname(host),
                                    authmethod='trust')
            self.assertTrue(len(matches) >= 1)

    def run_redistribution(self, dbname, output_dir=os.environ.get("MASTER_DATA_DIRECTORY"),mdd=os.environ.get("MASTER_DATA_DIRECTORY"), use_parallel_expansion=False, number_of_parallel_table_redistributed=4, validate=True, validate_redistribution=True):
        """
        Run data redistribution
        """
        outfile = output_dir +"/run_redistribution.out"
        errfile = output_dir +"/run_redistribution.err"

        self.log_and_test_gp_segment_config(message="beforer running redistribution")

        if use_parallel_expansion:
            cmd = Command(name='run gpexpand redistribute', cmdStr="export MASTER_DATA_DIRECTORY=%s; gpexpand -n %s -D %s" %(mdd, number_of_parallel_table_redistributed, dbname))
            tinctest.logger.info("Running data redistribution with parallel expansion: %s" %cmd)
        else:
            cmd = Command(name='run gpexpand redistribute', cmdStr="export MASTER_DATA_DIRECTORY=%s; gpexpand -D %s" %(mdd, dbname))
            tinctest.logger.info("Running data redistribution: %s" %cmd)

        cmd.run(validateAfter=validate)
        if validate_redistribution:
            self._validate_redistribution()
        results = cmd.get_results()

        with open(outfile, 'w') as output_file:
            output_file.write(results.stdout)
        with open(errfile, 'w') as output_file:
            output_file.write(results.stderr)

        self.log_and_test_gp_segment_config(message="after running redistribution")
        return results

    def interview(self, mdd, primary_data_dir, mirror_data_dir, new_hosts, use_host_file, num_new_segs=0, filespace_data_dir="", mapfile="/tmp/gpexpand_input"):
        '''
        @param new_hosts comma separated list of hostnames
        NOTE: The current interview process uses pexpect. It assumes that number_of_expansion_segments is exactly 1 and we are using filespaces
        '''
        self.log_and_test_gp_segment_config(message="Before interview")
        tinctest.logger.info("Expansion host list for interview: %s" %new_hosts)

        if use_host_file:
            segment_host_file = local_path("new_host_file")
            with open(segment_host_file, 'w') as f:
                f.write(new_hosts.replace(",", "\n"))
            shell_cmd = 'rm -fv gpexpand_inputfile*;export MASTER_DATA_DIRECTORY=%s;gpexpand -f %s -D %s' %(mdd, segment_host_file, os.environ.get('PGDATABASE'))
        else:  
            shell_cmd = 'rm -fv gpexpand_inputfile*;export MASTER_DATA_DIRECTORY=%s;gpexpand -D %s' %(mdd, os.environ.get('PGDATABASE'))


        child = pexpect.spawn('/bin/bash', ['-c', shell_cmd])
        child.expect ('Would you like to initiate a new System Expansion Yy|Nn (default=N):')
        tinctest.logger.info("pexpect 1: %s" %child.before)
        child.sendline ('y')

        if not use_host_file:
            res = child.expect(['Are you sure you want to continue with this gpexpand session',
                'Enter a blank line to only add segments to existing hosts'])
            # In case some of the cluster is configured with 'localhost',
            # the script finds it is not the "standard" configuration.
            # We don't care this case because the test is only to verify the
            # generated file.
            if res == 0:
                child.sendline('y')
                child.expect('Enter a blank line to only add segments to existing hosts')
                tinctest.logger.info("pexpect 2: %s" %child.before)
        
            child.sendline (new_hosts)
            child.expect ('What type of mirroring strategy would you like?')
        else:
            res = child.expect(['Are you sure you want to continue with this gpexpand session',
                'What type of mirroring strategy would you like?'])
            # In case some of the cluster is configured with 'localhost',
            # the script finds it is not the "standard" configuration.
            # We don't care this case because the test is only to verify the
            # generated file.
            if res == 0:
                child.sendline('y')
                child.expect('What type of mirroring strategy would you like?')

        tinctest.logger.info("pexpect 3: %s" %child.before)
        child.sendline ('grouped')
        
        child.expect ('How many new primary segments per host do you want to add')
        tinctest.logger.info("pexpect 4: %s" %child.before)
        child.sendline (str(num_new_segs))

        count_filespaces = self.get_value_from_query("select distinct count (*) from gp_persistent_filespace_node;");


        for i in range(int(num_new_segs)):
            child.expect('Enter new primary data directory')
            tinctest.logger.info("pexpect 5: %s" %child.before)
            child.sendline (primary_data_dir)
            for j in range(int(count_filespaces)):
                child.expect('Enter new file space location for file space name')
                tinctest.logger.info("pexpect 5: %s" %child.before)
                child.sendline (filespace_data_dir+"/filespace_pri_"+str(j))


        for i in range(int(num_new_segs)):
            child.expect('Enter new mirror data directory')
            tinctest.logger.info("pexpect 6: %s" %child.before)
            child.sendline (mirror_data_dir)
            for j in range(int(count_filespaces)):
                child.expect('Enter new file space location for file space name')
                tinctest.logger.info("pexpect 5: %s" %child.before)
                child.sendline (filespace_data_dir+"/filesapce_mir_"+str(j))


        child.expect('Please review the file')

        mapfile_interview = ""
        mapfile_interview_fs = ""
        cur_dir=os.getcwd()
        for f in os.listdir(cur_dir):
            if f.startswith('gpexpand_inputfile'):
                if not f.endswith('.fs'):
                    mapfile_interview=os.getcwd()+"/"+f
                if f.endswith('.fs'):
                    mapfile_interview_fs=os.getcwd()+"/"+f

        tinctest.logger.info("Mapfile generated by interview: %s" %mapfile_interview)
        shutil.copyfile(mapfile_interview, mapfile)
        if mapfile_interview_fs != "":
            shutil.copyfile(mapfile_interview_fs, mapfile+".fs")

    def create_filespace_dirs(self, primary_data_dir, mirror_data_dir, filespace_data_dir):
        """ 
        cretes necessary directories for expansion
        """

        cmd = "select fsname from pg_filespace where fsname!='pg_system';"
        list_of_filespaces = PSQL.run_sql_command(cmd, flags ='-q -t').strip().split("\n ")
        segment_host_file = '/tmp/segment_hosts'
        for filespaces in list_of_filespaces:
           fs_path_pri= os.path.join(primary_data_dir, filespaces ,"primary/")
           fs_path_mir= os.path.join(mirror_data_dir, filespaces , "mirror/")
           self.create_segment_dirs(fs_path_pri, fs_path_mir,"make filespace prim and mirr dirs" )

        count_filespaces = self.get_value_from_query("select distinct count (*) from gp_persistent_filespace_node;");
        for j in range(int(count_filespaces)):
           fs_path_pri= filespace_data_dir+"/filespace_pri_"+str(j)
           fs_path_mir= filespace_data_dir+"/filesapce_mir_"+str(j)
           self.create_segment_dirs(fs_path_pri, fs_path_mir,"make filespace dirs" )

    def create_segment_dirs(self, fs_path_pri, fs_path_mir, cmd_name):
        """ helper method to create dirs """

        #The test creates the hostfile in the tmp directory during gpintisytem, assuming that this still exists
        segment_host_file = '/tmp/segment_hosts'
        res = {'rc': 0, 'stdout' : '', 'stderr': ''} 
        run_shell_command("gpssh -f %s -e 'mkdir -p %s'" %(segment_host_file, fs_path_pri), cmd_name, res)
        if res['rc'] > 0:
            raise Exception("Failed to create directories on segments")
        run_shell_command("gpssh -f %s -e 'mkdir -p %s'" %(segment_host_file, fs_path_mir), cmd_name, res)
        if res['rc'] > 0:
            raise Exception("Failed to create directories on segments")


    def mirror_and_catalog_validation(self):
        '''
        @summary :gpcheckcat and gpcheckmirrorintegrity
        '''
        
        ###psql.run_shell_command("CHECKPOINT; CHECKPOINT; CHECKPOINT;CHECKPOINT; CHECKPOINT;")
        ###sleep(30) # sleep for some time for the segments to be in sync before validation

        self.dbstate = DbStateClass('run_validation')
        tinctest.logger.info("running gpcheckcat and gpcheckmirrorintegrity")
        outfile = local_path("gpcheckcat_"+datetime.datetime.fromtimestamp(time.time()).strftime('%Y%m%d%H%M%S')+".out")

        self.dbstate.check_catalog(outputFile=outfile)
        self.dbstate.check_mirrorintegrity()


    def get_value_from_query(self, sql_cmd):
        res = PSQL.run_sql_command(sql_cmd, flags ='-q -t')
        res = res.replace("\n", "")
        res = res.rstrip()
        res = res.lstrip()
        #cannot use res.strip because we are using this to get timestamp also and we dont want to remove the space between date and time
        return res

    def run_redistribution_with_duration(self, dbname, output_dir=os.environ.get("MASTER_DATA_DIRECTORY"), mdd=os.environ.get("MASTER_DATA_DIRECTORY"), use_end_time = False,validate=True, validate_redistribution=True):
        """
        Run data redistribution with duration

        There are two aspects
        1. The utility redistributes tables until the last table in the schema is successfully marked completed, 
        2. or until the specified duration or end time is reached. 
        There are two gpexpand commands for redistribution first command redistributes a part thus validating the second point 
        and the second command redistributes the remaining with max hours i.e. 60, thus validating the first point
        """
         
        bytes_left = " " 
        outfile = output_dir +"/run_redistribution_with_duration_1.out"
        errfile = output_dir +"/run_redistribution_with_duration_1.err"

        if use_end_time:
            end_time = self.get_value_from_query("select LOCALTIMESTAMP(0) + interval '4 seconds';"); 
            cmd = Command(name='run gpexpand redistribute', cmdStr="export MASTER_DATA_DIRECTORY=%s; gpexpand -e '%s' -D %s" %(mdd, end_time, dbname))
            tinctest.logger.info("Running data redistribution with end_time: %s" %cmd)
        else:
            cmd = Command(name='run gpexpand redistribute', cmdStr="export MASTER_DATA_DIRECTORY=%s; gpexpand -d 00:00:03 -D %s" %(mdd, dbname))
            tinctest.logger.info("Running data redistribution with duration: %s" %cmd)

        cmd.run(validateAfter=validate)
        results = cmd.get_results()
        with open(outfile, 'w') as output_file:
            output_file.write(results.stdout)
        with open(errfile, 'w') as output_file:
            output_file.write(results.stderr)
 
        sql_cmd ="select value from gpexpand.expansion_progress where name='Bytes Left';"
        bytes_left = PSQL.run_sql_command(sql_cmd, flags ='-q -t')
        tables_compteted = PSQL.run_sql_command("select count(*) from gpexpand.status_detail where status='COMPLETED';", flags ='-q -t')
        
        if  bytes_left == " "  or tables_compteted == 0:           
            self.fail("Either all or none of the tables were redistributed in the first stage of redistribution_with_duration. Bytes left: %s, tables compteted: %s" %(bytes_left, tables_compteted))

        #Make sure that the work load does not have large files to avoid the redistribution starting with a bigger table that cannot complete within 3 seconds.
        #need to add check here to count the rows in all tables

        outfile = output_dir +"/run_redistribution_with_duration_2.out"
        errfile = output_dir +"/run_redistribution_with_duration_2.err"

        cmd = Command(name='run gpexpand redistribute', cmdStr="export MASTER_DATA_DIRECTORY=%s; gpexpand -d 60:00:00 -D %s" %(mdd, dbname))
        tinctest.logger.info("Running data redistribution with duration 60:00:00 to redistrubute remainign data: %s" %cmd)
        cmd.run(validateAfter=validate)

        if validate_redistribution:
            self._validate_redistribution()
        results = cmd.get_results()

        with open(outfile, 'w') as output_file:
            output_file.write(results.stdout)
        with open(errfile, 'w') as output_file:
            output_file.write(results.stderr)
        self.log_and_test_gp_segment_config(message="after running redistribution")

        return results

    def check_number_of_parallel_tables_expanded(self, number_of_parallel_table_redistributed):
        tinctest.logger.info("in check_number_of_parallel_tables_expanded")
        tables_completed = self.get_value_from_query("select count(*) from gpexpand.status_detail where status='COMPLETED';")
        max_parallel = -1

        # For parallelism, we expect tables_in_progress to be more than one.
        minimum_acceptable_evidence_of_parallelism = 2

        # If the data being distributed is smaller than the number_of_parallel_table_redistributed,
        # then this will go into an infinite loop. However, we know that our current data is larger or equal to the
        # requested value number_of_parallel_table_redistributed. So, we won't consider that case here.
        while int(tables_completed) < int(number_of_parallel_table_redistributed):
            tables_in_progress = self.get_value_from_query("select count(*) from gpexpand.status_detail where status='IN PROGRESS';")
            tables_completed = self.get_value_from_query("select count(*) from gpexpand.status_detail where status='COMPLETED';")
            tinctest.logger.info("waiting to reach desired number of parallel redistributions \ntables_completed : " + tables_completed)
            tinctest.logger.info("tables_in_progress :"+ tables_in_progress)

            if int(tables_in_progress) > max_parallel:
                max_parallel = int(tables_in_progress)

        if max_parallel < minimum_acceptable_evidence_of_parallelism:
            self.fail("The minimum parallelism %d was never reached. "
                      "Maximum number of parallel found for IN_PROGRESS is only %d. "
                      "The table redistributed is %d. "
                      % (minimum_acceptable_evidence_of_parallelism, max_parallel,
                         int(number_of_parallel_table_redistributed)))

        sql_cmd = "select * from gpexpand.status_detail"
        PSQL.run_sql_command(sql_cmd, out_file="/data/gpexpand_psql.out", flags='-q -t')

        # It is possible that gpexpand continues to run if it has more data, and there could be more tables IN_PROGRESS.
        # However, we have observed parallelism if tables_in_progress is more than minimum_acceptable_evidence_of
        # parallelism.

    def _validate_redistribution(self):
        """
        This validates whether or not all the tables in all the databases are distributed across all the segments.
        Assumption: Assumes that there are at least enough rows in each table to get them distributed across all the segments.
        """
        return 

    def cleanup_expansion(self, dbname, output_dir=os.environ.get("MASTER_DATA_DIRECTORY"),  mdd=os.environ.get("MASTER_DATA_DIRECTORY")):
        """
        Run gpexpand to cleanup the expansion catalog
        """
        outfile = output_dir +"/cleanup_expansion.out"
        errfile = output_dir +"/cleanup_expansion.err"
        self.log_and_test_gp_segment_config(message="after running redistribution")

        cmd = Command(name='run gpexpand cleanup',
                      cmdStr='export MASTER_DATA_DIRECTORY=%s; echo -e \"y\\n\"  | gpexpand -c -D %s' % (mdd, dbname))
        tinctest.logger.info("Running expansion cleanup ...: %s" %cmd)
        cmd.run(validateAfter=True)
        results = cmd.get_results()
        with open(outfile, 'w') as output_file:
            output_file.write(results.stdout)
        with open(errfile, 'w') as output_file:
            output_file.write(results.stderr)
        
        with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
            try:
                query = "SELECT count(*) FROM information_schema.schemata where schema_name='gpexpand';"
                tinctest.logger.info("Executing query %s" %query)
                row = dbconn.execSQLForSingleton(conn, query)
            except UnexpectedRowsError, e:
                tinctest.logger.exception(e)
                raise Exception("Exception while executing query: %s" %query)

            self.assertEquals(row, 0, "Cleanup of expansion failed")

        self.log_and_test_gp_segment_config(message="after cleanup")

