#!/usr/bin/env python
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

gpfilespace wrapper for executing and creating filespace/tablespace
"""

############################################################################
import os
import re

import tinctest
from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL
from mpp.lib.config import GPDBConfig
from tinctest.lib import local_path, run_shell_command
from tinctest.main import TINCException

class GPfilespaceException(TINCException): pass
class Gpfilespace(object):
    
    TRANSACTION_FILE = "TRANSACTION_FILES"
    TEMPORARY_FILE = "TEMPORARY_FILES"

    SUC_PATTERNS = [".*INFO.*Checking if filespace \w+ exists.*",
                ".*INFO.*Obtaining current filespace information.*",
                ".*INFO.*Obtaining current filespace entries used by.*",
                ".*INFO.*Moving \w+ filespace.* from \w+ to \w+.*",
                ".*INFO.*Starting Greenplum Database.*"]

    MASTER_FAIL_STARTUP_PATTERNS = [".*CRITICAL.*Failed to start Master instance in admin mode.*",
                ".*waiting for server to start......could not start server.*"]

    SEG_FAIL_STARTUP_PATTERNS = [".*INFO.*Master Started.*",
                ".*INFO.*Checking for filespace consistency.*",
                ".*ERROR.*\w+ entries are inconsistent.*",
                ".*ERROR.*Filespaces are inconsistent. Abort Greenplum Database start.*",
                ".*INFO.*Shutting down master.*"]
    
    FAIL_SHOW_PATTERNS = [".*ERROR.*Filespace is inconsistent.*"]
    
    DEFAULT_FS = "pg_system"
    DEFAULT_FS_OPTION = "default"
    TEMP_DIR = "pgsql_tmp"
    TRANS_DIR = ["pg_clog", "pg_distributedlog", "pg_distributedxidmap", "pg_multixact", "pg_subtrans", "pg_xlog"]
    
    LOCAL_TRAN_FILE = "gp_transaction_files_filespace"
    LOCAL_TEMP_FILE = "gp_temporary_files_filespace"
    TS_LIST = ['ts_a1','ts_a2','ts_a3','ts_b1','ts_b2','ts_b3','ts_c1','ts_c2','ts_c3']


    def __init__(self, config=None):
        if config is not None:
            self.config = config
        else:
            self.config = GPDBConfig()
        self.gphome = os.environ.get('GPHOME')

    
    def run(self, config=None, logdir=None, output=None,
                            host=None, port=None, username=None, password=None,
                            movetemp=None, movetrans=None, 
                            showtemp=None, showtrans=None,
                            help=None):
        '''
        @param config: Config File
        @param logdir: Log dirctory, default gpAdminLogs
        @param output: Output directory
        @param host: hostname
        @param port: port number
        @param username: username
        @param password: password
        @param movetempfilespace: move temporary filespace
        @param movetransfilespace: move transaction filespace
        @param showtempfilespace: show temporary filespace
        @param showtransfilespace: show transaction filespace
        @param help: show help
        @return result object (result.rc, result.stdout, result.stderr)
        '''
        cmd_opt = ""
        
        if config:
            cmd_opt += " -c %s " % config
            
        if logdir:
            cmd_opt += " -l %s " % logdir
            
        if output:
            cmd_opt += " -o %s " % logdir
            
        if host:
            cmd_opt += " -h %s " % host
            
        if port:
            cmd_opt += " -p %s " % port
            
        if username:
            cmd_opt += " -U %s " % username
            
        if password:
            cmd_opt += " -W %s " % password

        if movetemp:
            cmd_opt += " --movetempfilespace %s " % movetemp
            
        if movetrans:
            cmd_opt += " --movetransfilespace %s " % movetrans
            
        if showtemp:
            cmd_opt += " --showtempfilespace "
            
        if showtrans:
            cmd_opt += " --showtransfilespace "
            
        if help:
            cmd_opt = " -? "
        filespace_cmd = '%s/bin/gpfilespace %s' % (self.gphome, cmd_opt)

        # @todo: We shouldn't source greenplum_path.sh
        cmd = Command(name='Gpfilespace command', cmdStr="source %s/greenplum_path.sh;%s" % (self.gphome,filespace_cmd))
        tinctest.logger.info(" %s" % cmd)
        cmd.run(validateAfter=True)
        result = cmd.get_results()
        if result.rc != 0:
            raise GPfilespaceException('Issue with Gpfilespace Command')
        return result

    def move_tempdefault(self):
        '''
        gpfilespace --movetempfilespace default
        '''
        self.run(movetemp="default")
        if not self.get_filespace_location(self.TEMPORARY_FILE) == self.DEFAULT_FS:
            raise GPfilespaceException("Issue with gpfilespace")

    def move_transdefault(self):
        '''
        gpfilespace --movetransfilespace default
        '''
        self.run(movetrans="default")
        if not self.get_filespace_location(self.TRANSACTION_FILE) == self.DEFAULT_FS:
            raise GPfilespaceException("Issue with gpfilespace")
        
    def showtempfiles(self):
        '''
        gpfilespace --showtempfilespace
        @return output and return code
        '''
        return self.run(showtemp=True)

    def showtransfiles(self):
        '''
        gpfilespace --showtransfilespace
        @return output and return code
        '''
        return self.run(showtrans=True)
    
    def get_filespace_location(self, ftype=TEMPORARY_FILE):
        '''
        Get the filespace name for temporary location
        @return filespace location
        '''
        if ftype == "TEMPORARY_FILES":
            out = self.showtempfiles()
        else:
            out = self.showtransfiles()
        for line in out.stdout.splitlines():
            if re.search(".*Current Filespace.*%s.*" % ftype, line):
                return line.split(" ")[-1].strip()
            
    def get_filespace_directory(self, ftype=TEMPORARY_FILE):
        '''
        Get the filespace directory location
        @return dir_fs: array of filespace directories
        '''
        dir_fs = []
        if ftype == "TEMPORARY_FILES":
            out = self.showtempfiles()
        else:
            out = self.showtransfiles()
        for line in out.stdout.splitlines():
            if re.search(".*INFO.*-\d+.*", line):
                dir_fs.append(line.split(" ")[-1].strip())
        return dir_fs
    
    def get_hosts_for_filespace(self, location):
        '''
        Get the hostname for the filespace location
        @param location: location of filespace
        @return filespace: dictionary of hostname, location, content, role, preferred_role
        @note: This return multiple rows for the same filespace location
        '''
        cmd = "select hostname, fselocation, content, role, preferred_role from gp_segment_configuration, pg_filespace_entry where fselocation='%s' and dbid=fsedbid" % (location)
        fs_out = PSQL.run_sql_command(cmd, flags = '-t -q', dbname='postgres') 
        filespace = []
        if len(fs_out.strip()) > 0:
            for line in fs_out.splitlines():
                if len(line.strip())>0:
                    data = {}
                    (hostname, location, content, role, preferred_role) = line.split('|')
                    data['hostname'], data['location'], data['content'], data['role'], data['preferred_role'] = hostname.strip(), location.strip(), content.strip(), role.strip(), preferred_role.strip()
                    filespace.append(data)
            return filespace
        else:
            raise GPfilespaceException("Issue with getting host for filespace")

    def movetransfiles_localfilespace(self, filespace):
        '''
        Helper function to move Transaction Files to local filespace
        '''
        cur_filespace = self.get_filespace_location(self.TRANSACTION_FILE)
        if cur_filespace != filespace: # Do not move again
            out = self.run(movetrans=filespace)
            # Verify after moving, if it has been moved
            cur_filespace = self.get_filespace_location(self.TRANSACTION_FILE)
            if not (cur_filespace==filespace):
                raise GPfilespaceException("Issue with moving transaction filespace")


    def movetempfiles_localfilespace(self, filespace):
        '''
        Helper function to move Temporary Files to local filespace
        '''
        cur_filespace = self.get_filespace_location(self.TEMPORARY_FILE)
        if cur_filespace != filespace: # Do not move again
            out = self.run(movetemp=filespace)
            # Verify after moving, if it has been moved
            cur_filespace = self.get_filespace_location(self.TEMPORARY_FILE)
            if not (cur_filespace==filespace):
                raise GPfilespaceException("Issue with moving temporary filespace")

    def exists(self, filespace):
        '''
        Check whether filespace exist in catalog
        @param filespace: filespace name
        @return True or False
        
        '''
        fs_out = PSQL.run_sql_command("select count(*) from pg_filespace where fsname='%s'" % filespace, flags = '-t -q', dbname='postgres')
        if int(fs_out.strip()) > 0:
            return True
        return False

    def create_filespace(self, filespace):
        '''
        @param filespace: Filespace Name
        '''
        if self.exists(filespace) is True:
            tinctest.logger.info('Filespace %s exists' % filespace)
            return

        file1 = local_path(filespace)
        f1 = open(file1+".fs.config","w")
        f1.write('filespace:%s\n' % filespace)
       
        for record in self.config.record:
            if record.role:
                fileloc = '%s/%s/primary' % (os.path.split(record.datadir)[0], filespace)
            else:
                fileloc = '%s/%s/mirror' % (os.path.split(record.datadir)[0], filespace)
            # @todo: use a common utility to create/delete remotely
            cmd = "gpssh -h %s -e 'rm -rf %s; mkdir -p %s'"  % (record.hostname, fileloc, fileloc)
            run_shell_command(cmd)
            f1.write("%s:%s:%s/%s\n" % (record.hostname, record.dbid, fileloc, os.path.split(record.datadir)[1])) 
        f1.close()
        result = self.run(config=f1.name)
        if result.rc != 0:
            raise GPfilespaceException('"gpfilespace creation filespace FAILED".  Output = %s ' % out)

    def drop_filespace(self, filespace):
        '''
        @param filespace : filespace name
        @description: Drop the filespace
        @note : This assumes that all objects created in this filespace are deleted by the test
        '''
        sql_cmd = 'Drop Filespace %s;' % filespace 
        out = PSQL.run_sql_command(sql_cmd, flags = '-t -q', dbname='postgres') 
        tinctest.logger.info('out %s' % out)
        if 'ERROR' in out :
            tinctest.logger.info('The filespace could not be deleted. Could be that its not empty')
            return False
        return True

class HAWQGpfilespace(object):

    def __init__(self):
        self.gphome = os.environ.get('GPHOME')
        self.config = GPDBConfig()


    def exists(self, filespace):
        ''' Check whether filespace exist in catalog
        @param filespace: filespace name
        @return True or False
        
        '''
        fs_out = PSQL.run_sql_command("select count(*) from pg_filespace where fsname='%s'" % filespace, flags = '-t -q', dbname='postgres')
        if int(fs_out.strip()) > 0:
            return True
        return False

    def create_filespace(self, filespace):
        '''
        @param filespace: Filespace Name
        '''
        if self.exists(filespace) is True:
            tinctest.logger.info('Filespace %s exists' % filespace)
            return

        file1 = local_path(filespace)
        f1 = open(file1+".fs.config","w")
        f1.write('filespace:%s\n' % filespace)
        f1.write('fsysname:hdfs\n')
        fsrep = PSQL.run_sql_command("select fsrep from pg_filespace where fsname='dfs_system';", flags = '-t -q', dbname='postgres')
        f1.write('fsreplica:%s\n' % fsrep.strip())

        dfs_loc_cmd = "SELECT substring(fselocation from length('hdfs:// ') for (position('/' in substring(fselocation from length('hdfs:// ')))-1)::int) FROM pg_filespace pgfs, pg_filespace_entry pgfse  WHERE pgfs.fsname = 'dfs_system' AND fsedbid = 2 AND pgfse.fsefsoid=pgfs.oid ;"
        dfs_loc = PSQL.run_sql_command(dfs_loc_cmd,flags = '-t -q', dbname='postgres')
        for record in self.config.record:
            if record.content == -1:
                fileloc = '%s/hdfs_%s' % (os.path.split(record.datadir)[0], filespace)
                f1.write("%s:%s:%s/%s\n" % (record.hostname, record.dbid, fileloc, os.path.split(record.datadir)[1]))
                cmd = "gpssh -h %s -e 'rm -rf %s; mkdir -p %s'"  % (record.hostname, fileloc, fileloc)
                run_shell_command(cmd)
            else:
                f1.write("%s:%s:[%s/%s/%s]\n" % (record.hostname, record.dbid, dfs_loc.strip(), filespace, os.path.split(record.datadir)[1]))
        f1.close()
        filespace_cmd = '%s/bin/gpfilespace -c %s' % (self.gphome, f1.name)
        cmd = Command(name='Gpfilespace command', cmdStr="%s" % (filespace_cmd))
        tinctest.logger.info(" %s" % cmd)
        cmd.run(validateAfter=True)
        result = cmd.get_results()
        if result.rc != 0:
            raise GPfilespaceException('"gpfilespace creation filespace FAILED".  Output = %s ' % resutl.stdout)
