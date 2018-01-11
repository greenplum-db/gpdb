#!/usr/bin/env python
# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103

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

Capture the information regarding gpdb
"""

try:
    import sys, time
    import traceback
    #from generalUtil import GeneralUtil
    from gppylib.commands.base import Command
    from PSQL import PSQL
    #from cdbfastUtil import PrettyPrint
    from datetime import datetime
except Exception, e:
    sys.exit('Cannot import modules GpdbSegmentConfig.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(e))

#####
class GpdbSegmentConfig:
    """
    Capture the information regarding Gpdb Segment Configuration
    @class GpdbSegmentConfig
    
    @organization: DCD Partner Engineering
    @contact: Kenneth Wong
    """

    ###
    def __init__(self):
        """
        Constructor for GpdbSegmentConfig
        """
        #self.generalUtil = GeneralUtil()
        self.psql = PSQL
    ###
    def GetSqlData(self, sql):
        """
        Execute the sql statement and returns the data
        @param sql: The sql command to execute
        @return: data
        """
        data = []
        # use run_sql_command in SQL instead of run
        #(rc, out) = psql.run(dbname='gptest', cmd='%s' % (sql), ofile='-', flag='-q -t')
        out = self.psql.run_sql_command(sql_cmd='%s' % (sql), dbname='gptest', out_file='-', flags='-q -t')
        for line in out:
            line = line.strip()
            if line:
                data.append(line)
        return 0, data

    def hasStandbyMaster( self ):
        '''
        check if a greenplum database has standby master configured
        @return: True or False which indicates whether or not the gpdb has standby master configured
        '''
        cmd = 'SELECT CASE WHEN count(*) > 0 THEN \'True\' ELSE \'False\' END AS hasstandby FROM pg_catalog.gp_segment_configuration WHERE content = -1 AND role = \'m\';'
        # use run_sql_command in SQL instead of runcmd
        #( ok, out ) = psql.runcmd( cmd )
        out = self.psql.run_sql_command(sql_cmd='%s' % (cmd), out_file='-', flags='-t -q')

        #if not ok:

        if out and out[0].strip() == 'True':
            return True
        else:
            return False

    def hasMirrors( self ):
        '''
        check if a greenplum database has mirrors configured
        @return: True or False which indicates whether or not the gpdb has mirrors configured
        '''
        cmd = 'SELECT CASE WHEN count(*) > 0 THEN \'True\' ELSE \'False\' END AS hasstandby FROM pg_catalog.gp_segment_configuration WHERE content > -1 AND role = \'m\';'
        # use run_sql_command in SQL instead of runcmd
        #( ok, out ) = psql.runcmd( cmd )
        out = self.psql.run_sql_command(sql_cmd='%s' % (cmd), out_file='-', flags='-t -q')

        if out.strip() == 'True':
            return True
        else:
            return False

    ###
    def GetSegmentInvalidCount(self):
        """
        @return: Number of invalid segment servers
        """
        """
        """
        cmd = "psql gptest -c 'SET search_path To public,gp_toolkit; SELECT COUNT(*) as invalid FROM gp_pgdatabase_invalid' | sed -n '3,3 s/^[     ]*//p'"
        # use Command instead of ShellCommand
        #rc, data = self.generalUtil.ShellCommand("psql gptest -c 'SET search_path To public,gp_toolkit; SELECT COUNT(*) as invalid FROM gp_pgdatabase_invalid' | sed -n '3,3 s/^[     ]*//p'")
        generalUtil = Command(name='psql gptest -c',cmdStr=cmd)
        generalUtil.run()
        rc = generalUtil.get_results().rc
        if rc != 0:
            raise Exception("psql gptest -c failed with rc:  (%d)" % (rc))
        data = generalUtil.get_results().stdout
        #segmentInvalidCount = data[0].strip()
        segmentInvalidCount = data.strip()
        return rc, segmentInvalidCount

    ###
    def GetSegmentInSync(self, sleepTime=60, repeatCnt=120, greenplum_path=""):
        """
        @param sleepTime: Number of seconds to sleep before retry
        @param repeatCnt: Number of times to repeat retry. Default is 2 hours
        @return: Return True when the number of segment servers that are in resync is 0 rows
        """
        inSync = ""
        for cnt in range(repeatCnt):
            data = ""
            try:
                cmd = "psql gptest -c \"SELECT dbid, content, role, preferred_role, status, mode, address, fselocation, port, replication_port FROM gp_segment_configuration, pg_filespace_entry where dbid = fsedbid and mode = 'r'\""
                if greenplum_path:
                    cmd = "%s %s" % (greenplum_path, cmd)
                # use Command instead of ShellCommand
                #rc, data = self.generalUtil.ShellCommand(cmd)
                generalUtil = Command(name='psql gptest -c',cmdStr=cmd)
                generalUtil.run()
                rc = generalUtil.get_results().rc
                data = generalUtil.get_results().stdout
                if rc == 0:
		            if True in ['(0 rows)' in x for x in data]:
			            return rc, True
                time.sleep(sleepTime)
            except Exception, e:
                traceback.print_exc()
                print "ERRORFOUND GetSegmentInSync %s" % (str(e))
                #PrettyPrint('ERRORFOUND GetSegmentInSync', data) TODO
                print 'ERRORFOUND GetSegmentInSync', data
        return 0, False

    ###
    def GetServerList(self, role='p', status='u'):
        """
        @param role: Supported role are 'p' and 'm'
        @param status: Supported status are 'u' and 'd'
        @return: list of hostname
        """
        cmd = "SELECT hostname FROM gp_segment_configuration WHERE content != -1 AND role = '%s' AND status = '%s'" % (role, status)
        (rc, data) = self.GetSqlData(cmd)
        return rc, data

    ###
    def GetMasterHost(self, role='p', status='u'):
        """
        @param role: Supported role are 'p' and 'm'
        @param status: Supported status are 'u' and 'd'
        @return: master hostname
        """
        cmd = "SELECT hostname FROM gp_segment_configuration WHERE content = -1 AND role = '%s' AND status = '%s'" % (role, status)
        (rc, data) = self.GetSqlData(cmd)
        if data:
            data = data[0]
            data = data.strip()
        else:
            data = None
        return rc, data

    ###
    def GetMasterStandbyHost(self, status='u'):
        """
        @param status: Supported status are 'u' and 'd'
        @return: Master-standby hostname
        """
        rc, data = self.GetMasterHost('m', status)
        return rc, data

    ###
    def GetUpServerList(self, role='p'):
        """
        @param role: Supported role are 'p' and 'm'
        @return: list of segment server that are up
        """
        serverList = self.GetServerList(role, "u")
        return serverList

    ###
    def GetDownServerList(self, role='m'):
        """
        @param role: Supported role are 'p' and 'm'
        @return: list of segment server that are down
        """
        rc, serverList = self.GetServerList(role, "d")
        return rc, serverList

    ###
    def GetSegmentServerCount(self, role='p', status='u'):
        """
        @param role: Supported role are 'p' and 'm'
        @param status: Supported status are 'u' and 'd'
        @return: Segment server count
        """
        rc, serverList = self.GetServerList(role, status)
        return rc, len(serverList)

    ###
    def GetHostAndPort(self, content, role='p', status='u'):
        """
        Returns the list of segment server that are up or down depending on mode
        @param role: Supported role are 'p' and 'm'
        @param status: Supported status are 'u' and 'd'
        @return: hostname and port
        """
        cmd = "SELECT hostname, port FROM gp_segment_configuration WHERE content = %s AND role = '%s' AND status = '%s'" % (content, role, status)
        (rc, data) = self.GetSqlData(cmd)
        if data:
            host, port = data[0].split('|')
            return rc, host.strip(), port.strip()
        else:
            return rc, "", ""

    ###
    def GetContentIdList(self, role='p', status='u'):
        """
        Returns the list of segment server content ID  for the primary or mirror depending on role in content assending order
        @param role: Supported role are 'p' and 'm'
        @return: content list
        """
        contentList = []
        cmd = "SELECT content FROM gp_segment_configuration WHERE content != -1 AND role = '%s' AND status = '%s' ORDER BY content" % (role, status)
        out = self.psql.run_sql_command(sql_cmd='%s' % (cmd), dbname='gptest',out_file='-', flags='-q -t')
        #TODO
        for line in out:
            line = line.strip()
            if line:
                contentList.append(line)
        return contentList

    ###
    def GetPrimaryContentIdList(self, status):
        """
        Returns the list of primary segment server content ID that are up or down depending on mode in assending order
        @param status: Supported mode are 'u' and 'd'
        @return: content list of primary segment server
        """
        rc, contentList = self.GetContentIdList("p", status)
        return rc, contentList

    ###
    def GetMirrorContentIdList(self, status):
        """
        Returns the list of mirror segment server content ID that are up or down depending on mode in assending order
        @param status: Supported status are 'u' and 'd'
        @return: content list of mirror segment server
        """
        rc, contentList = self.GetContentIdList("m", status)
        return rc, contentList

    ###
    def GetMasterDataDirectory(self):
        """
        @return: master data directory
        """
        cmd = "SELECT fselocation as datadir FROM gp_segment_configuration, pg_filespace_entry, pg_catalog.pg_filespace fs WHERE fsefsoid = fs.oid and fsname='pg_system' and gp_segment_configuration.dbid=pg_filespace_entry.fsedbid AND content = -1 AND role = 'p' ORDER BY content, preferred_role"
        out = self.psql.run_sql_command(sql_cmd='%s' % (cmd),dbname='gptest', out_file='-', flags='-q -t')

        datadir = out
        datadir = datadir.strip()
        return datadir

    ###
    def GetSegmentData(self, myContentId, myRole='p', myStatus='u'):
        """
        Returns the list of segment information that matches the content id, role and status
        @param myContentId:
        @param myRole: Either 'p' or 'm'
        @param myStatus: Either 'u' or 'd'
        @return: hostname, port, datadir address and status
        """
        segmentData = []
        cmd = "SELECT dbid, content, role, preferred_role, mode, status, hostname, address, port, fselocation as datadir, replication_port FROM gp_segment_configuration, pg_filespace_entry, pg_catalog.pg_filespace fs WHERE fsefsoid = fs.oid and fsname='pg_system' and gp_segment_configuration.dbid=pg_filespace_entry.fsedbid ORDER BY content, preferred_role"
        #(rc, out) = psql.run(dbname='gptest', cmd='%s' % (cmd), ofile='-', flag='-q -t')
        out = self.psql.run_sql_command(sql_cmd='%s' % (cmd),dbname='gptest',out_file='-', flags='-t -q')
        for line in out:
            if line:
                data = {}
                # Check for valid data
                if len(line) > 10:
                    (dbid, content, role, preferred_role, mode, status, hostname, address, port, datadir, replication_port) = line.split('|')
                    content = content.strip()
                    role = role.strip()
                    status = status.strip()
                    if int(content) == int(myContentId) and role == myRole and status == myStatus:
                        data['content'] = content
                        data['hostname'] = hostname.strip()
                        data['port'] = port.strip()
                        data['datadir'] = datadir.strip()
                        data['status'] = status.strip()
                        data['role'] = role.strip()
                        data['preferred_role'] = preferred_role.strip()
                        data['address'] = address.strip()
                        segmentData.append(data)
        return segmentData

if __name__ == '__main__':
    gpdbSegmentConfig = GpdbSegmentConfig()
    gpdbSegmentConfig.GetSegmentInvalidCount()
    x, y = gpdbSegmentConfig.GetSegmentInSync()
    print "x %s y %s" % (x, y)
