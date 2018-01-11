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

import tinctest
import re
import os
import time

from tinctest.lib import local_path
from gppylib.commands.base import Command, REMOTE
from gppylib.db import dbconn
from gppylib.gparray import GpArray
from mpp.lib.PSQL import PSQL
from mpp.lib.config import GPDBConfig
from mpp.lib.gprecoverseg import GpRecover
from mpp.lib.gpstop import GpStop
from mpp.lib.gpstart import GpStart

MYD = os.path.abspath(os.path.dirname(__file__))
GPHOME = os.environ.get('GPHOME')

class Fault(Command):
    """
    
    @description Contains utility functions to create faults for different scenarios
    @created 2009-01-27 14:00:00
    @modified 2013-09-12 17:10:15
    @tags storage schema_topology 
    @product_version gpdb:4.2.x,gpdb:main
    """

    def __init__(self, cmd = None):
        Command.__init__(self, 'Running fault command', cmd)

    def _run_sys_cmd(self, cmd_str, validate = False):
        """
        @summary: helper function to run a sys cmd
        
        @param cmd_str: the command to run
        @param validate: decides whether to validate the exit status or not
        @return: results of the executed command
        """

        tinctest.logger.info("execute:" +cmd_str)
        cmd = Fault(cmd_str)
        cmd.run(validateAfter = validate)
        tinctest.logger.info(cmd.get_results())
        return cmd.get_results()

    def get_segment_host(self, preferred_role='p', content=0):
        """
        @summary: Gets host name for the required segment for require content
        
        @param preferred_role: required role of the segment
        @param content: required content no of the segment
        @return: hostname containing the required data segments
        """

        sqlCmd = "SELECT hostname from gp_segment_configuration where content = %s and preferred_role = '%s';"%(content,preferred_role)
        hosts = PSQL.run_sql_command(sqlCmd).split('\n')[3].strip()
        return hosts
        
    def get_segment_host_fileLoc(self):
        """
        @summary: Gets the file location information for primary seg
        
        @return: hostname of the node having primary segment with content 0
                 location of directory where pg_ directories are present
        """

        sqlCmd = "SELECT hostname, fselocation from pg_filespace_entry, gp_segment_configuration where content = 0 and preferred_role = 'p' and dbid = fsedbid ;"
        result = PSQL.run_sql_command(sqlCmd).split('\n')[3].strip()
        (host, fileLoc) = result.split('|')
        return (host, fileLoc)

    def get_seginfo_for_primaries(self):
        seglist = []
        gparray = GpArray.initFromCatalog(dbconn.DbURL())
        for seg in gparray.getDbList():
            if seg.isSegmentPrimary() and not seg.isSegmentMaster():
                seglist.append(seg)
        return seglist

    def get_seginfo(self, preferred_role='p', content=0):
        gparray = GpArray.initFromCatalog(dbconn.DbURL())
        for seg in gparray.getDbList():
            if seg.getSegmentContentId() == content and seg.getSegmentPreferredRole() == preferred_role:
                return seg

    def inject_using_gpfaultinjector(self, fault_name, fault_mode, fault_type, segdbid):
        sysCmd = 'gpfaultinjector -f %s -m %s -y %s -s %s' % (fault_name, fault_mode, fault_type, segdbid)
        self._run_sys_cmd(sysCmd, validate=True)

    def create_remote_symlink(self, host, datadir):
        datadir_root = os.path.dirname(datadir)
        segdir = os.path.basename(datadir)
        sysCmd = 'mkdir -p {datadir_root}/link; mv {datadir} {datadir_root}/link/{segdir}; ln -s {datadir_root}/link/{segdir} {datadir}'\
               .format(datadir_root=datadir_root, datadir=datadir, segdir=segdir)
        cmd = Command('create remote symlinks', cmdStr=sysCmd, ctxt=REMOTE, remoteHost=host)
        cmd.run(validateAfter=True)

    def remove_remote_symlink(self, host, datadir):
        datadir_root = os.path.dirname(datadir)
        segdir = os.path.basename(datadir)
        sysCmd = 'rm -f {datadir}; mv {datadir_root}/link/{segdir} {datadir_root}/{segdir}; rmdir {datadir_root}/link;'\
                .format(datadir_root=datadir_root, datadir=datadir, segdir=segdir)
        cmd = Command('remove symlinks and restore the data directory', cmdStr=sysCmd, ctxt=REMOTE, remoteHost=host)
        cmd.run(validateAfter=True)

    def drop_pg_dirs_on_primary(self, host, fileLoc):
        """
        @summary: Drops all the pg_ directories from the primary segment directory
        
        @param host: hostname of the node where primary segment with content 0 resides
        @param fileLoc: directory location where pg_ directories are present
        @return: None
        """

        sysCmd = "ssh %s rm -rf %s/pg_*" %(host, fileLoc)
        self._run_sys_cmd(sysCmd)

    def kill_primary_gp0(self, hosts):
        """
        @summary: Kills the primary process of segment with content 0
        
        @param hosts: hostname of the node where primary segment with content 0 resides
        @return: Boolean value representing whether the process is killed or not
        """

        sqlCmd = "SELECT port FROM gp_segment_configuration WHERE content = 0 AND preferred_role = 'p'"
        primary_seg_port_no = PSQL.run_sql_command(sqlCmd).split('\n')[3].strip()
        sys_cmd = "gpssh -h %s  -e ' ps aux | egrep '\\\''postgres -D.*-p %s'\\\'' | awk '\\\''{print \"kill -9 \"$2}'\\\'' | sh ' "%(hosts,primary_seg_port_no)
        self._run_sys_cmd(sys_cmd)

        r = 0
        retries = 200
        while r < retries: 
            sys_cmd = """gpssh -h %s -e 'ps aux | grep "[p]ostgres: port.*%s"'""" %(hosts,primary_seg_port_no)
            result = self._run_sys_cmd(sys_cmd).stdout
            if result.find('postgres') > 0:
                continue
            else:
                tinctest.logger.info("Primary process with content 0 killed.")
                return True
            time.sleep(1)
            r += 1

        if result.find('postgres') > 0:
            tinctest.logger.warn("Couldn't kill the primary process..")
            return False

    def kill_primary(self, host, datadir, port):
        """
        @summary: Kills the primary process of segment 
        
        @param host: hostname of the node where primary segment
        @param datadir: data directory of the primary segment 
        @param port: port of the primary segment 
        @return: Boolean value representing whether the process is killed or not
        """
    
        sys_cmd = "gpssh -h %s  -e ' ps aux | egrep '\\\''[p]ostgres -D.*-p %s'\\\'' | awk '\\\''{print \"kill -9 \"$2}'\\\'' | sh ' "%(host, port)
        self._run_sys_cmd(sys_cmd)

        r = 0
        retries = 200
        while r < retries: 
            sys_cmd = """gpssh -h %s -e 'ps aux | grep "[p]ostgres: port.*%s"'""" %(host, port)
            result = self._run_sys_cmd(sys_cmd).stdout
            if result.find('postgres') > 0:
                continue
            else:
                tinctest.logger.info("Primary process with content 0 killed.")
                return True
            time.sleep(1)
            r += 1

        if result.find('postgres') > 0:
            tinctest.logger.warn("Couldn't kill the primary process..")
            return False

    def kill_mirror_gp0(self, hosts):
        """
        @summary: Kills the mirror process of segment with content 0
        
        @param hosts: hostname of the node where mirror segment with content 0 resides
        @return: Boolean value representing whether the process is killed or not
        """

        sqlCmd = "SELECT port FROM gp_segment_configuration WHERE content = 0 AND preferred_role = 'm'"
        mirror_seg_port_no = PSQL.run_sql_command(sqlCmd).split('\n')[3].strip()
        sys_cmd = "gpssh -h %s  -e ' ps aux | egrep '\\\''postgres -D.*-p %s'\\\'' | awk '\\\''{print \"kill -9 \"$2}'\\\'' | sh ' "%(hosts,mirror_seg_port_no)
        self._run_sys_cmd(sys_cmd)
        sys_cmd = "gpssh -h %s -e 'netstat -antu | grep %s'" %(hosts,mirror_seg_port_no)
        result = self._run_sys_cmd(sys_cmd).stdout
        if not result.find('tcp') < 0:
            tinctest.logger.warn("Couldn't kill the mirror process..")
            return False
        else:
            tinctest.logger.info("Mirror process with content 0 killed.")
            return True

    def is_changetracking(self):
        """
        @summary: return true if system is in change tracking mode
        
        @return: Boolean value representing the whether the cluster is insync or not
        """

        config = GPDBConfig()
        return not config.is_not_insync_segments()

    def replace_new_dirPath(self, orig_filename='recovery.conf', new_filename='recovery_new.conf'):
        """
        @summary: Modifies the template config file with new filespace location
        
        @param orig_filename: name of the template config file
        @param new_filename: name of the new config file
        @return: None
        """

        new_file = open(local_path(new_filename),'w')
        old_file = open(local_path(orig_filename))
        # Finds the gp prefix string from the host entry of the segments
        (host, fileLoc) = self.get_segment_host_fileLoc()
        slashIndex = fileLoc.rfind('/')
        # Extract the gp prefix
        gp_prefix = fileLoc[(slashIndex+1):]
        lineNo = 0
        for line in old_file:
            if lineNo == 1 :
                # If you checkout the template out file from the command gprecoverseg -o, you'll see that,
                # it has failed_host:port:data_dir <SPACE> recovery_host:port:data_dir.
                # we intend to change only the data_dir of the recovery_host and not that of the failed host
                # so we first divide the lines & modify only the second part 
                # and then concatenate them again
                prefix_index = line.find(gp_prefix)
                first_part_of_line = line[:(prefix_index + len(gp_prefix))]
                remaining_part_of_line = line[(prefix_index + len(gp_prefix)):]
                new_gp_prefix = gp_prefix+'new'
                remaining_part_of_line = re.sub(gp_prefix,new_gp_prefix,remaining_part_of_line)
                line = first_part_of_line + remaining_part_of_line
            lineNo = lineNo + 1
            new_file.write(line)
        new_file.close()
        old_file.close()

    def create_new_loc_config(self, hosts, orig_filename='recovery.conf', new_filename='recovery_new.conf'):
        """
        @summary: Runs recovery with -o option and creates a new config out of template
                    having new filespace location
        
        @param orig_filename: name of the template config file
        @param new_filename: name of the new config file
        @return: None
        """

        gpcmd = 'source %s/greenplum_path.sh; gprecoverseg -p %s -o %s' %(GPHOME, hosts, local_path(orig_filename))
        self._run_sys_cmd(gpcmd)
        self.replace_new_dirPath(orig_filename,new_filename)

      
    def run_recovery_with_config(self, filename='recovery_new.conf'):
        """
        @summary: Runs incremental recovery using config file.
        
        @param filename: name of the modified recoverseg config file
        @return: Boolean value representing the status of recovery process
        """

        rcvr_cmd = 'gprecoverseg -a  -i %s' % local_path(filename)
        cmd = Command(name='Run gprecoverseg', cmdStr='source %s/greenplum_path.sh;%s' % (GPHOME, rcvr_cmd))
        tinctest.logger.info("Running gprecoverseg : %s" % cmd)
        cmd.run(validateAfter=True)
        result = cmd.get_results()
        if result.rc != 0 or result.stderr:
            return False
        return True

    def stop_db(self):
        """
        @summary: Stops the greenplum DB based on the options provided
        
        @param option: represents different gpstop command options
        @return: output of the executed command as a string
        """        
        gpstop = GpStop()
        gpstop.run_gpstop_cmd()

    def start_db(self):
        """
        @summary: Start the greenplum DB based on the options provided 
        
        @param option: represents different gpstart command options
        @return: output of the executed command as a string
        """

        gpstart = GpStart()
        gpstart.run_gpstart_cmd()

    def check_if_not_in_preferred_role(self):
        """
        @summary: Checks if the segments are in preferred roles or not.
        
        @return: True if the segments are not in preferred roles
        """

        cmd_str = "select 'segment_not_In_PreferredRole' from gp_segment_configuration where role <> preferred_role and not content = -1"
        out = PSQL.run_sql_command(cmd_str).count('segment_not_In_PreferredRole') - 1
        if out > 0:
            return True
        return False

