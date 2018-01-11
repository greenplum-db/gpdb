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
"""

import os
import sys
from time import sleep
import tinctest
from tinctest.lib import local_path, Gpdiff, run_shell_command
from mpp.lib.PSQL import PSQL
from mpp.lib.config import GPDBConfig
from gppylib.commands.base import Command


def get_items_list(test_file):
    """ Get file contents to a list """
    with open(test_file, "r") as f:
        test_list = [line.strip() for line in f]
    return test_list

def checkDBUp():
    """
	Connect to psql many times to see if the recovery is complete
    before proceeding with actual test
	"""
    for i in range(1, 100):
        out = PSQL.run_sql_command("drop table if exists dbup_test; "
                                   "create table dbup_test(i int)")
        if out.lower().find("error") == -1 or out.lower().find("fatal") == -1:
            return
        sleep(10)
    raise Exception("The cluster is not coming up after 100 psql attempts")

def validate_sql(filename):
    """ Compare the out and ans files """
    out_file = local_path(filename.replace(".sql", ".out"))
    ans_file = local_path(filename.replace(".sql" , ".ans"))
    assert Gpdiff.are_files_equal(out_file, ans_file)

def run_sql(filename, verify=True):
    """ Run the provided sql and validate it """
    out_file = local_path(filename.replace(".sql", ".out"))
    PSQL.run_sql_file(sql_file = filename, out_file = out_file)
    if verify == True:
        validate_sql(filename)

def search_log(cmd, key, segdir, logname, start=""):
    """
    Search using gplogfilter. Returns True if match has no '0 lines' -
    meaning some match found
    """
    logfilename = local_path(logname)
    GPHOME =  os.getenv("GPHOME")
    if len(start)>0:
        start = "-b %s" %start
    cmd_str = ("gpssh %s -e \"source %s/greenplum_path.sh; "
               "gplogfilter %s -m %s %s\" > %s 2>&1" %
               (cmd, GPHOME, start, key, segdir, logfilename))
    cmd = Command("search_log", cmdStr=cmd_str)
    cmd.run(validateAfter=False)
    f = open(logfilename, "r")
    for line in f.readlines():
        line = line.strip()
        if line.find("match:") > 0:
            if line.find("0 lines") < 0:
                return True
    return False

def search_string(hostname, search_string_list, dir, start=""):
    """
    Search for each keyword in the search_string_list per hostname
    Returns True if the keyword is found
    """
    for key in search_string_list:
        tinctest.logger.info("Checking for %s in %s " % (key, dir))
        if search_log("-h %s" % hostname, key, dir,
                      "segment-%s.out" % hostname, start):
            return (True, "Found %s in %s" % (key, dir))
    return (False, "No Issues Found")

def check_logs(search_string_list):
    """
    Check all the segment logs(master/primary/mirror) for keywords in the
    search_string_list
    """
    dbid_list = PSQL.run_sql_command(
        "select dbid from gp_segment_configuration;", flags="-q -t",
        dbname="postgres")
    dbid_list = dbid_list.split()
    config = GPDBConfig()
    for dbid in dbid_list:
        (host, data_dir) = config.get_host_and_datadir_of_segment(dbid.strip())
        (rc , msg) = search_string(host, search_string_list, data_dir)
        if rc:
            return (False, msg)
    return (True, "No Issues found")


def copy_files_to_segments(filename, location):
    config = GPDBConfig()
    hosts = config.get_hosts(segments=True)
    for host in hosts:
        cmd = 'gpssh -h %s -e "scp %s %s:%s/" ' % (host, filename, host, location)
        tinctest.logger.debug(cmd)
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command (cmd, 'run scp', res) 
        if res['rc']>0:
            raise Exception('Copying to host %s failed' % host)

def copy_files_to_master(filename, location):
    config = GPDBConfig()
    host = config.get_masterhost()
    cmd = 'gpssh -h %s -e "scp %s %s:%s/" ' % (host, filename, host, location)
    tinctest.logger.debug(cmd)
    res = {'rc':0, 'stderr':'', 'stdout':''}
    run_shell_command (cmd, 'run scp', res)
    if res['rc']>0:
        raise Exception('Copying to host %s failed' % host)    


class Gpprimarymirror:


    def run_gpprimarymirror(self):
        gp_primarymirror_infile = local_path("gpprimarymirror_in")
        gp_primarymirror_outfile = local_path("gpprimarymirror_out")
        sql = "select hostname, port from gp_segment_configuration where mode='r' and role='p' limit 1;"
        out = PSQL.run_sql_command(sql, flags="-q -t").strip()
        host = out.split('|')[0].strip()
        port = out.split('|')[1].strip()
        cmd = "gp_primarymirror -h %s -p %d < %s > %s 2>&1" % (host, int(port), gp_primarymirror_infile, gp_primarymirror_outfile)
        ok  = run_shell_command(cmd)
        return ok

class Gpstate:

    def run_gpstate_shell_cmd(self, options):
        gpstate_outfile = local_path("gpstate_out")
        cmd = "gpstate %s > %s 2>&1" % (options, gpstate_outfile)
        ok  = run_shell_command(cmd)
        return ok

    def get_gpprimarymirror_output(self):
        gpprimarymirror_outfile = local_path("gpprimarymirror_out")
        total_resync_count = 0
        current_resync_count = 0
        with open(gpprimarymirror_outfile) as fp:
            lines = fp.readlines()
            for line in lines:
                if 'totalResyncObjectCount' in line:
                    total_resync_count = line.split(':')[-1]
                if 'curResyncObjectCount' in line:
                    current_resync_count = line.split(':')[-1]
        return (int(total_resync_count), int(current_resync_count))

    def verify_gpstate_output(self):
        gpstate_outfile = local_path("gpstate_out")
        total_resync_count = 0
        current_resync_count = 0
        with open(gpstate_outfile) as fp:
            lines = fp.readlines()
            for line in lines:
                if 'Incremental' in line:
                    total_resync_count = line.split()[7]
                    current_resync_count = line.split()[8]

        mirror_total_count, mirror_cur_count = self.get_gpprimarymirror_output()
        return int(total_resync_count) == mirror_total_count and \
            int(current_resync_count) == mirror_cur_count

    def run_gpstate(self, type, phase):
        tinctest.logger.info("running gpstate")

        gphome = os.environ.get('GPHOME')
        masterdd = os.environ.get('MASTER_DATA_DIRECTORY')
        res = {'rc':0, 'stdout':'', 'stderr':''}

        cmd = "gpstate --printSampleExternalTableSql -q -d %s" % (masterdd)
        ok  = run_shell_command(cmd, results=res)
        if res['rc']>0:
            raise Exception("Failed to query gpstate --printSampleExternalTableSql");
        
        out = PSQL.run_sql_command(res['stdout'])
        if out.find('CREATE EXTERNAL TABLE') == -1 :
            raise Exception("Failed to create the external table gpstate_segment_status")

        gpstate_outfile = local_path("gpstate_out")
        cmd = "gpstate -s -a > %s 2>&1" % (gpstate_outfile)

        ok  = run_shell_command(cmd)
        self.check_gpstate(type, phase)
        return ok

    def __compare_segcounts(self, q1, q2, state):
            num1 = PSQL.run_sql_command(q1, flags="-q -t").strip()
            try:
                num1 = int(num1)
            except ValueError:
                raise Exception("Invalid int value: %s" % num1)
            num2 = PSQL.run_sql_command(q2, flags="-q -t").strip()
            try:
                num2 = int(num2)
            except ValueError:
                raise Exception("Invalid int value: %s" % num2)
            if num1 != num2:
                raise Exception("gpstate in %s state failed" % state)

    def check_gpstate(self, type, phase):
        """
        Checking cluster state during different transition stage
        """
        if phase == "sync1":
            q1 = ("select count(*) from gpstate_segment_status where "
                  "role=preferred_role and mirror_status='Synchronized' "
                  "and status_in_config='Up' and instance_status='Up';")
            q2 = ("select count(*) from gp_segment_configuration "
                  "where content <> -1;")
            self.__compare_segcounts(q1, q2, "Sync")
            tinctest.logger.info("Done Running gpstate in %s phase " %(phase))

        elif phase == "ct":
            q1 = ("select count(*) from gpstate_segment_status where "
                  "role=preferred_role and mirror_status='Change Tracking'"
                  "and role='Primary' and status_in_config='Up' "
                  "and instance_status='Up';")
            q2 = ("select count(*) from gpstate_segment_status where "
                  "role=preferred_role and mirror_status='Out of Sync' "
                  "and role='Mirror' and status_in_config='Down' "
                  "and instance_status='Down in configuration';")
            self.__compare_segcounts(q1, q2, "CT")
            tinctest.logger.info("Done Running gpstate in %s phase " %(phase))

        elif phase == "resync_incr":
            if type == "primary":
                q1 = ("select count(*) from gpstate_segment_status where "
                      "role=preferred_role and mirror_status="
                      "'Resynchronizing' and status_in_config='Up' and "
                      "instance_status='Up' and resync_mode="
                      "'Incremental';")
            else:
                q1 = ("select count(*) from gpstate_segment_status where "
                      "mirror_status='Resynchronizing' and "
                      "status_in_config='Up' and instance_status='Up' "
                      "and resync_mode='Incremental';")
            q2 = ("select count(*) from gp_segment_configuration where "
                  "content <> -1;")
            self.__compare_segcounts(q1, q2, "Resync Incremental")
            tinctest.logger.info("Done Running gpstate in %s phase " %(phase))

        elif phase == "resync_full":
            q1 = ("select count(*) from gp_segment_configuration where "
                  "content <> -1;")
            if type == "primary":
                q2 = ("select count(*) from gpstate_segment_status where "
                      "role=preferred_role and "
                      "mirror_status='Resynchronizing' and "
                      "status_in_config='Up' and instance_status='Up' "
                      "and resync_mode='Full';")
            else:
                q2 = ("select count(*) from gpstate_segment_status where "
                      "mirror_status='Resynchronizing' and "
                      "status_in_config='Up' and instance_status='Up' "
                      "and resync_mode='Full';")
            self.__compare_segcounts(q1, q2, "Resync Full")
            tinctest.logger.info("Done Running gpstate in %s phase " %(phase))
        return True


    

