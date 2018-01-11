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

This utility allows to verify GPDB with the following:
- gpverify: online verification with database activity
- gpcheckmirrorseg.pl: check primary mirror integrity with no activity
- gpcheckcat: check the database catalog

Usage:
Run GPDB Verify in a loop every 10 mins. This will stop when GPDB is not running
python gpdbverify.py -l True|T|t

Run GPDB Verify once
python gpdbverify.py

Run GPDB Verify with no activity. 
python gpdbverify.py -t noactivity

"""

import os
import sys
import glob
import time
import socket
import logging

from optparse import OptionParser

import tinctest
from tinctest.lib import local_path, run_shell_command
from gppylib.commands.base import Command, REMOTE
from mpp.lib.PSQL import PSQL

from mpp.lib.config import GPDBConfig

#####
class GpdbVerify:
    
    SLEEPTIME = 600 # Default sleep time, 10 mins
    LOGPATH = os.path.dirname(os.path.realpath(__file__)) 
    
    GPVERIFY_STATE = { "success": "SUCCESS",
                      "running": "RUNNING",
                      "fail": "FAIL"
    }
    GPCHECK_STATE = { "fail": "[FAIL]",
                     "warning": "[WARNING]",
                     "ok": "[OK]",
                     "repair": "repair script(s) generated in dir"
    }

    def __init__(self, **kwargs):
        """
        Constructor for GpdbVerify
        """
        self.tinchome = os.environ["TINCHOME"]
        self.gphome = os.environ["GPHOME"]
        self.mdd = os.environ["MASTER_DATA_DIRECTORY"]
        self.pgport = int(os.environ["PGPORT"]) or 5432
        self.dbname = os.environ["PGDATABASE"] or 'gptest'
        if 'config' in kwargs:
            self.config = kwargs['config']
        else:
            self.config = GPDBConfig()

        if not os.path.exists(self.LOGPATH):
            os.mkdir(self.LOGPATH)

        try: # Initialize the arguments and set default values if does not exist
            self.set_commandtype(kwargs["cmdtype"])
            self.set_sleeptime(kwargs["sleeptime"])
        except KeyError, e:
            self.set_commandtype("cmd")
            self.set_sleeptime(self.SLEEPTIME)


    def set_commandtype(self, value):
        """
        Set the Command Type
        @param value: command type 
        """
        self.cmdtype = value

    def set_sleeptime(self, value):
        """
        Set the sleept time
        @param value: sleeptime in seconds
        """
        self.sleeptime = value
        
    def gpverify(self, wait=False):
        """
        gpverify utility
        @param wait
        """
        tinctest.logger.info("Running gpverify ...")
        if self.check_integrityresults() == 0: # Check first whether gpverify is already running
            cmd = Command(name=' gpverify --full ', cmdStr = 'gpverify --full')
            cmd.run(validateAfter=False)
            result = cmd.get_results()
            
            if result.rc and wait:
                while self.check_integrityresults()==-1:
                    time.sleep(SLEEPTIME)
                    tinctest.logger.info("gpverify is still Running ...")
                
    def cleanup_day_old_out_files(self, cleanup_dir=None):
        '''Cleanup a day old out files if present to avoid OOD scenarios '''
        cur_time = time.strftime("%Y%m%d%H%M%S")
        if cleanup_dir is None:
            cleanup_dir = self.LOGPATH
        file_types = ('fixfile_','checkmastermirrorsegoutput_', 'checkmirrorsegoutput_')
        for ftype in file_types:
            for file in glob.glob('%s/%s*' % (cleanup_dir ,ftype)):
                filedate = file.split('_')[-1]
                if filedate.isdigit():
                    time_diff = int(cur_time) - int(filedate)
                    if int(time_diff)  > 1000000:
                        os.remove(file)
        
    def gpcheckmirrorseg(self, options=None, master=False):
        """
        gpcheckmirrorseg utility
        @param options
        @param master
        @return True or False and Fix File
        """
        # Cleanup a day old out files before proceeding with a new checkmirrorseg
        self.cleanup_day_old_out_files()
        cur_time = time.strftime("%Y%m%d%H%M%S")
        if master:
            integrity_outfile = self.LOGPATH + "/checkmastermirrorsegoutput_" + cur_time
        else:   
            integrity_outfile = self.LOGPATH + "/checkmirrorsegoutput_" + cur_time
        fix_outfile = self.LOGPATH + "/fixfile_" + cur_time
        if options is None:
            if master:
                options = " -mastermirror=true  -exclude recovery.conf -ignore 'gp_restore' -ignore 'gp_cdatabase' -ignore 'gp_dump' -ignore '_data' -ignore 'wet_execute.tbl' -ignore 'pg_xlog' -ignore 'pg_changetracking' -ignore 'pg_verify' -ignore 'backup_label.old' -dirignore 'pg_xlog' -ignore 'pg_subtrans' -ignore '/errlog/' -dirignore 'gpperfmon/data' "
            else:
                options = " -ignore '_data' -ignore 'wet_execute.tbl' -ignore 'gp_dump' -ignore 'core' -ignore pg_changetracking -ignore 'pg_xlog' -ignore 'pg_verify' -ignore '/errlog/' -parallel=true"

        command = "gpcheckmirrorseg.pl -connect '-p %d -d %s' -fixfile %s" \
                % (self.pgport, self.dbname, fix_outfile)
        command += options
        command += "> %s 2>&1" % (integrity_outfile)
        tinctest.logger.info("Running gpcheckmirroseg.pl ...")
        rc = run_shell_command(command)
        if not rc:
            tinctest.logger.info('Checkmirrorintegrity shows difference... We will check first if its extra_m before failing')
        if not os.path.exists(integrity_outfile):
            raise Exception('Integrity output file not present')
        ifile = open(integrity_outfile, 'r')
        lines = ifile.readlines()
        checkmirror = False
        if lines[-1].find('no differences found')>=0:
            checkmirror = True
        else:
            # Issue with mirror cleanup from Storage when we abort transaction.
            # For now, ignore the extra_m and rewrites
            checkmirror = self.ignore_extra_m(fix_outfile)

        if master:
            if checkmirror:
                tinctest.logger.info("-- MasterMirror Integrity passed")
            else:
                tinctest.logger.error("-- MasterMirror Integrity failed")
        else:
            if checkmirror:
                tinctest.logger.info("-- PrimaryMirror Integrity check passed")
            else:
                tinctest.logger.info("-- PrimaryMirror Integrity check failed")
            
        return (checkmirror, fix_outfile)
    
    def ignore_extra_m(self, fix_file):
        """
        Ignore extra_m and rewrites to new fix_file_ignoreextram
        """
        if not os.path.exists(fix_file):
            raise Exception('%s does not exist' % fix_file)
        os.rename(fix_file, fix_file+".bak")
        f = open(fix_file, "w")
        contents = open(fix_file+".bak").readlines()
        tinctest.logger.info('contents of fixfile %s' % contents)
        checkmirror = True
        for line in contents:
            if line.find('extra_m') == -1:
                f.write(line)
                checkmirror = False
        f.close()
        return checkmirror
                
    
    def gpcheckcat(self, dbname=None, alldb=False, online=False, testname=None, outputFile=None, host=None, port=None):
        """
        gpcheckcat wrapper
        @param dbname: Database name (default gptest)
        @param alldb: Check all database
        @param online: Activity (True) vs No Activity (False)
        @return: errorCode, hasError, gpcheckcat output, repairScriptDir
        
        # errorCode from gpcheckcat
        # SUCCESS=0
        # ERROR_REMOVE=1
        # ERROR_RESYNC=2
        # ERROR_NOREPAIR=3       
        """
        if dbname is None:
            dbname = self.dbname

        if port is None:
            port = "-p %s" % self.pgport
        else:
            port = "-p %s" % port

        if alldb is False:
            alldb = ""
        else:
            alldb = "-A"
            
        if online:
            online = "-O"
        else:
            online = ""
            
        if testname is None:
            testname = ""
        else:
            testname = "-R %s" % testname
        if outputFile is None:
            outputFile = self.LOGPATH + '/checkcatoutput_' + time.strftime("%Y%m%d%H%M%S")

        tinctest.logger.info("Running gpcheckcat ...")
        checkcat_cmd = "%s/bin/lib/gpcheckcat %s %s %s %s %s > %s 2>&1;" \
                              % (self.gphome, port, alldb, online, testname, dbname, outputFile)
        if host and host not in (socket.gethostname(), 'localhost'):
            cmd = Command(name=' Running Gpcheckcat.. ', cmdStr = checkcat_cmd, ctxt=REMOTE, remoteHost=host)
        else:
            cmd = Command(name=' Running Gpcheckcat.. ', cmdStr = checkcat_cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        # Get Error Code from gpcheckcat
        errorCode = result.rc
        if host and host not in (socket.gethostname(), 'localhost'):
            parent_dir =  os.path.dirname(outputFile)
            cmd = Command(name='scp remote gpcheckcat output file to local', cmdStr = 'scp %s:%s %s'%(host, outputFile, parent_dir))
            cmd.run(validateAfter=False)
        gpcheckcat_output = open(outputFile).readlines()
        (hasError, repairScriptDir) = self.check_catalogresults(outputFile)
        return (errorCode, hasError, gpcheckcat_output, repairScriptDir)

    def run_repair_script(self, repair_script_dir, dbname=None, alldb=True, online=False, testname=None, outputFile=None, host=None, port=None):
        '''
        @summary : Run the gpcehckcat repair script generated by gpcehckcat
        '''

        if not os.path.exists(repair_script_dir):
            repair_script_dir = '%s/%s' % (self.tinchome, repair_script_dir)

        tinctest.logger.debug('Using repair script dir ... %s' % repair_script_dir)
        repair_scripts = glob.glob(repair_script_dir + '/*.sh')

        ok = 0
        for repair_script in repair_scripts:
            repair_cmd = "/bin/bash %s" % str(repair_script).strip()
            tinctest.logger.info('Running repair script ... %s' % repair_cmd)
            if host and host not in (socket.gethostname(), 'localhost'):
                cmd = Command(name=' Running Gpcheckcat.. ', cmdStr = repair_cmd, ctxt=REMOTE, remoteHost=host)
            else:
                cmd = Command(name=' Running Gpcheckcat.. ', cmdStr = repair_cmd)
            cmd.run(validateAfter=False)
            result = cmd.get_results()
            # Get Error Code from running repair script
            if result.rc != 0:
                ok = result.rc

        if ok != 0:
            return False

        return True

    def check_integrityresults(self):
        """
        Check gpverify results from the last token
        @return: True or False, -1 is still running
        """
        sql = "select vertoken from gp_verification_history order by 1 desc limit 1"
        out= PSQL.run_sql_command(sql, flags='-q -t', dbname='postgres')
        last_token = out.strip()

        if not last_token:
            return 0 # No records of gpverify

        cmd = Command(name='gpverify', cmdStr="gpverify --results --token %s" % (last_token))
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        state = result.stdout[len(result.stdout)-2]
        if state.find(self.GPVERIFY_STATE["success"])>0:
            tinctest.logger.info("gpverify at %s: Successful" % last_token)
            return True
        elif state.find(self.GPVERIFY_STATE["running"])>0:
            tinctest.logger.info("gpverify at %s: Running" % last_token)
            return -1
        elif state.find(self.GPVERIFY_STATE["fail"])>0:
            tinctest.logger.info("gpverify at %s: Failed" % last_token)
            return False
        else:
            tinctest.logger.info("gpverify has not start")
            return 0
            
    def check_catalogresults(self, output):
        """
        Verify the gpcheckcat output for ERROR
        @param output: gpcheckcat output
        @return: True or False
        """
        
        dbname = ""
        error_lines = ""
        repair_script_dir = ""
        mark = False
        
        f = open(output)
        for line in f:
            if line.find("Performing check for database")>0:
                dbname = line[line.find("'")+1:line.rfind("'")]

            if line.find(self.GPCHECK_STATE["fail"])>0:
                mark = True
            elif line.find(self.GPCHECK_STATE["warning"])>0:
                error_lines += "%s: %s\n" % (dbname, line)
            elif line.find(self.GPCHECK_STATE["ok"])>0:
                mark = False
            elif line.find(self.GPCHECK_STATE["repair"])>0:
                repair_script_dir = line
                
            if mark:
                error_lines += "%s: %s" % (dbname, line)

        repair_line = repair_script_dir.split(" ")
        repair_script_dir = repair_line[-1].rstrip('\n') # Get the gpcheckcat repair script directory

        if error_lines == "":
            tinctest.logger.info("Catalog Check: Successful")
            return (True, repair_script_dir)
        else:
            tinctest.logger.info("*** Catalog Check: Failed")
            tinctest.logger.info(error_lines)
            return (False, repair_script_dir)
        
    def check_mastermirrorintegrity(self, options=None):
        """
        Check the integrity of master mirror
        @param options: options for gpcheckmirrorseg.pl
        """
        return self.gpcheckmirrorseg(options=options, master=True)
        
    def online_verification_loop(self):
        """
        Run the online verification in a loop, every SLEEPTIME (10 mins)
        """
        while self.check_db_is_running(): # Need to check DB is running
            self.verify_online()
            tinctest.logger.info("Running online verification in ... %d s" % 
                             (self.SLEEPTIME))
            time.sleep(self.SLEEPTIME)
            
    def verify_online(self, wait=True):
        """ 
        Verify GPDB even when there is activity with gpverify and gpcheckcat
        @param wait: Wait until gpverify is complete
        """
        self.gpverify(wait=wait)
        self.gpcheckcat(alldb=True, online=True)

    def verify_online_noactivity(self, options=None):
        """ 
        Verify GPDB when there is no activity with gpverify and gpcheckcat
        @param options: options for gpcheckmirrorseg.pl
        """
        if self.config.has_master_mirror():
            self.gpcheckmirrorseg(master=True) # Check Master/Standby
        self.gpcheckmirrorseg(master=False) # Check Primary/Mirror
        self.gpcheckcat(alldb=True, online=False)

    def check_db_is_running(self):
        """
        Check whether DB is running
        TODO: Need to put in gpdbSystem
        """
        cmd = Command(name='pg_ctl call',cmdStr="%s/bin/pg_ctl -D %s status" % (self.gphome, self.mdd))
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        if result.rc == 0:
            if result.stdout.splitlines()[0].find("server is running")>0:
                tinctest.logger.info('Server is Running')
                return True
        return False
        
#
# Input argument
#
def ProcessOption():
    """
    Process Options
    """
    parser = OptionParser()
    parser.add_option("-t", "--type", dest="verify", 
                      default="activity", 
                      help="Verify with: activity (default) or no_activity")
    parser.add_option("-l", "--loop", dest="loop", 
                      default="False", 
                      help="Loop online verification")
    parser.add_option("-s", "--sleep", dest="sleep", 
                      default=None, type="int", 
                      help="gpdb verify sleeps every 600s (10 mins) by default")
    (options, args) = parser.parse_args()
    return options

##########################
if __name__ == '__main__':
    options = ProcessOption()
    gpv = GpdbVerify()

    if options.verify == "activity":
        if options.loop == "True" or options.loop == "t" or options.loop == "T":
            if options.sleep:
                gpv.set_sleeptime(options.sleep)
            gpv.set_commandtype("background")
            gpv.online_verification_loop()
        else:
            gpv.verify_online()
    else:
        gpv.verify_online_noactivity()
