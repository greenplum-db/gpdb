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

GPDB Configuration

Usage:
from mpp.lib.config import GPDBConfig

"""
from collections import defaultdict

from mpp.lib.PSQL import PSQL
from tinctest.main import TINCException

import os

# ============================================================================
class GPDBConfigException(TINCException): pass

class GPDBConfig():
    '''Class with methods to get GPDB Configuration informaitons '''

    class Record:
        def __init__(self, line):
            line = line.split('|')
            line = [l.strip() for l in line]
            self.dbid = int(line[0])
            self.content = int(line[1])
            self.role = line[2] == 'p' 
            self.preferred_role = line[3] == 'p' 
            self.mode = line[4] == 's' 
            self.status = line[5] == 'u' 
            self.hostname = line[6]
            self.address = line[7]
            self.port = line[8]
            self.datadir = line[9]
            self.replication_port =line[10]

    
    def __init__(self):
        self.record = []
        self._fill()
    
    def _fill(self):
        '''Get the records and add to Record class '''
        self.record = []
        config_sql = "select dbid, content, role, preferred_role, mode, status, hostname, address, port, fselocation as datadir, replication_port from gp_segment_configuration, pg_filespace_entry, pg_catalog.pg_filespace fs where fsefsoid = fs.oid and fsname='pg_system' and gp_segment_configuration.dbid=pg_filespace_entry.fsedbid ORDER BY content, preferred_role;"
        config_out = PSQL.run_sql_command(config_sql, flags = '-t -q', dbname='postgres')
        if len(config_out.strip()) > 0:
            config_out = config_out.splitlines()
            for line in config_out:
                if line.find("NOTICE")<0:
                    line = line.strip()
                    if line:
                        self.record.append(GPDBConfig.Record(line))
        else:
           raise GPDBConfigException('Unable to select gp_segment_configuration')

    def has_mirror(self):
        ''' Checks if the configuration has mirror'''
        return reduce(lambda x, y: x or y,
                      [not r.role for r in self.record])

    def get_countprimarysegments(self):
        ''' Returns number of primary segments '''
        n = 0
        for r in self.record:
            if r.role and r.content != -1:
                n += 1
        return n

    def get_segments_count_per_host(self):
        """
        Return a dict of hostname and segments count. 
        """
        seg_count = defaultdict(int)
        for r in self.record:
            if r.role and r.content != -1:
                seg_count[r.hostname] += 1
        return seg_count

    def get_hosts(self, segments = False):
        ''' 
        @summary Returns the list of hostnames
        @param segments : True or False (True -returns only segment hosts)
        '''
        list = []
        for r in self.record:
            if segments:
                if r.content != -1:
                    list.append(r.hostname)
            else:
                list.append(r.hostname)
        return set(list)

    def get_hostandport_of_segment(self, psegmentNumber = 0, pRole = 'p'):

        '''
        @summary: Return a tuple that contains the host and port of the specified segment.
        @param pSegmentNumber : The segment number (0 - N-1, where N is the number of segments).
        @param pRole: 'p' for Primary, 'm' for Mirror
        '''

        if pRole == 'p':
            role = True
        else:
            role = False

        for seg in self.record:
            if seg.content == psegmentNumber and seg.role == role:
                return (seg.hostname, seg.port)

    def get_host_and_datadir_of_segment(self, dbid=-1):
        '''
        @description : Return hostname and data_dir for the dbid provided
        '''
        for r in self.record:
            if r.dbid == int(dbid):
                return(r.hostname, r.datadir)

    def is_segments_local(self, prole='p'):
        '''
        @summary: Check from the segment "address" column whether the GPDB configuration is localhost
                  Now checks whether "all" segments has "localhost" address
        @param pRole: 'p' for primary, 'm' for mirror
        '''
        if prole == 'p':
            role = True
        else:
            role = False
        n = 0
        for r in self.record:
            if r.content != -1:
                if r.role == role:
                    if r.address == "localhost":
                        n = n+1
        return (self.get_countprimarysegments()==n)

    def is_multinode(self):
        '''
        Check whether GPDB is multinode
        For OSX, it will always be single node. It's documented about issues with OSX and GPBD setting up
        @note: On DCA, the hostname for each segment is different, but the address is pointing to localhost
        localhost
        '''
        if os.uname()[0] == 'Darwin':
            return False

        # We check hostname, as we could have different addresses
        # on the same host.
        hostname_set = set([r.hostname for r in self.record])
        if len(hostname_set) == 1:
            return False
        else:
            return True

    def has_master_mirror(self):
        ''' Returns true if standby is configured '''
        master = 0
        for r in self.record:
            if r.content == -1:
                master += 1
        if master == 1:
            return False
        else:
            return True

    def get_count_segments(self):
        '''Returns number of segments '''
        out = PSQL.run_sql_command("select count(*) from gp_segment_configuration where content != -1 and role = 'p' and status = 'u'", flags='-q -t', dbname='template1')
        for line in out:
            return line.strip()

    def is_mastermirror_synchronized(self):
        ''' Returns True is master and standby are synchronized'''
        out = PSQL.run_sql_command('select summary_state from gp_master_mirroring',flags='-q -t', dbname='template1')
        if len(out.strip()) > 0:
            for line in out:
                line = line.strip()
                if line == 'Synchronized':
                    return True
        return False

    def get_masterdata_directory(self):
        ''' Returns the MASTER_DATA_DIRECTORY '''
        for r in self.record:
            if r.role and r.content == -1:
                return r.datadir

    def get_masterhost(self):
        ''' Returns master hostname'''
        for r in self.record:
            if r.role and r.content == -1:
                return r.hostname

    def get_master_standbyhost(self):
        ''' Return standby hostname '''
        for r in self.record:
            if (r.content == -1) and (not r.role):
                return r.hostname

    def is_not_insync_segments(self):
        '''Returns True if no down or change_tracking segments '''

        gpseg_sql = "select count(*) from gp_segment_configuration where mode <>'s' or status <> 'u';"
        not_insync_segments = PSQL.run_sql_command(gpseg_sql, flags = '-t -q') 
        if not_insync_segments.strip() != '0' :
            return False
        return True

    def is_balanced_segments(self):
        '''Returns True if primary and mirror are balanced'''

        gpseg_sql = "select count(*) from gp_segment_configuration where role != preferred_role;"
        balance_segments = PSQL.run_sql_command(gpseg_sql, flags = '-t -q') 
        if balance_segments.strip() != '0' :
            return False
        return True

    def count_of_nodes_in_mode(self, mode = 'c'):
        """
        PURPOSE:
            gives count of number of nodes in change tracking
        @return:
            count of number of nodes in change tracking
         
        """
        sqlcmd = "select count(*) from gp_segment_configuration where mode = '" + mode + "'"
        (num_cl) = PSQL.run_sql_command(sqlcmd)
        num_cl = num_cl.split('\n')[3].strip()
        return num_cl
    
    def is_down_segments(self):
        for r in self.record:
            if r.status == 'd':
                return True
        return False 

    def get_dbid(self, content, seg_role):
        ''' Returns the db_id given contentid and role '''

        for r in self.record:
            if r.content == content and r.role == (seg_role == "p"):
                return r.dbid
