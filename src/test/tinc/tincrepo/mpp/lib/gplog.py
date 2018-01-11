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
import time

import tinctest

from mpp.lib.PSQL import PSQL

from gppylib.commands.base import Command
from gppylib.commands.base import REMOTE
from gppylib.commands.gp import GpLogFilter
from gppylib.gparray import GpArray
from gppylib.db.dbconn import DbURL, connect

_DEFAULT_OUT_FILE = '/tmp/cluster.logs'
_DEFAULT_USER = os.environ.get('USER')
_DEFAULT_PORT = int(os.environ.get('PGPORT', 5432))

class GpLogException(Exception):
    pass

class GpLog(object):
    """
    This class lets users perform operations on logs from the cluster
    """
    
    @staticmethod
    def gather_log(start_time, end_time=None, out_file=_DEFAULT_OUT_FILE,
                   dbname=_DEFAULT_USER, host='localhost', port=_DEFAULT_PORT,
                   user=_DEFAULT_USER,
                   errors_only=False, master_only=False):
        """
        @type start_time: date
        @param start_time: Start time of the duration for which logs should be gathered.

        @type end_time: date
        @param end_time: End time of the duration for which logs should be gathered.

        @type out_file: string
        @param out_file: File to which the gathered logs should be written to.Defaults to /tmp/cluster.logs

        @type host: string
        @param host: Host name for the connection.Defaults to localhost

        @type port: integer
        @param port: Port number for the connection to the cluster. Defaults to environment variable PGPORT.

        @type user: string
        @param user: Username for the connection to the cluster. Defaults to the current user.

        @type dbname: string
        @param dbname: Database name to use for the connection to the cluster. Defaults to the current user.

        @type errors_only: boolean
        @param errors_only: When set to true, gathers only errors from logs.Defaults to False.

        @type master_only: boolean
        @param master_only: When set to true, gathers logs only from the master host. 
        """
        try:
            # TODO - When the cluster is down or this fails,
            # no exception is thrown from run_sql_command
            GpLog._gather_log_from_gp_toolkit(start_time=start_time,
                                              end_time=end_time,
                                              out_file=out_file,
                                              host=host,
                                              port=port,
                                              user=user,
                                              dbname=dbname,
                                              errors_only=errors_only,
                                              master_only=master_only)
        except Exception, e:
            tinctest.logger.exception("Gather log failed: %s" %e)
            raise GpLogException("Gathering log failed. Make sure you can connect to the cluster.")
            # TODO - use this as a backup if gp toolkit fails
            """
            GpLog._gather_log_from_gp_log_filter(start_time=start_time,
                                                 end_time=end_time,
                                                 out_file=out_file,
                                                 host=host,
                                                 port=port,
                                                 user=user,
                                                 dbname=dbname,
                                                 errors_only=errors_only,
                                                 master_only=master_only)
            """

    @staticmethod
    def check_log_for_errors(start_time, end_time=None, host='localhost',
                             user=_DEFAULT_USER, port=_DEFAULT_PORT,
                             dbname=_DEFAULT_USER):
        """
        Check logs in the given duration for any error messages.
        Returns True / False based on whether errors were found in the logs.

        @type start_time: date
        @param start_time: Start time of the duration for which logs should be gathered.

        @type end_time: date
        @param end_time: End time of the duration for which logs should be gathered.

        @type host: string
        @param host: Host name for the connection.Defaults to localhost

        @type port: integer
        @param port: Port number for the connection to the cluster. Defaults to environment variable PGPORT.

        @type user: string
        @param user: Username for the connection to the cluster. Defaults to the current user.

        @type dbname: string
        @param dbname: Database name to use for the connection to the cluster. Defaults to the current user.

        @rtype: boolean
        @return: Returns True if there are errors found in the log in the given duration, False otherwise. 
        
        """
        format_start_time = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(start_time))
        if end_time:
            format_end_time = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(end_time))
        else:
            format_end_time = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime())

        tinctest.logger.info("checking log from %s to %s" % (format_start_time, format_end_time))

        sql_cmd = "select logseverity,count(logseverity) from gp_toolkit.gp_log_database " + \
                  "where (logseverity=\'FATAL\' or logseverity=\'ERROR\' or logseverity='PANIC') " + \
                  "and (logtime >=\'%s\' and logtime <= \'%s\') group by logseverity;" % \
                  (format_start_time, format_end_time)
        try:
            result = PSQL.run_sql_command(sql_cmd, dbname=dbname, host=host, port=port, username=user, flags='-a -x')
            if "RECORD" in result:
                return True
        except Exception, e:
            tinctest.logger.exception("Failed while checking logs - %s" %e)
            raise GpLogException("Failed while checking logs. Make sure you can connect to the cluster")
        return False
               
    @staticmethod
    def _test_connection(host='localhost',port=_DEFAULT_PORT, user=_DEFAULT_USER,
                         dbname=_DEFAULT_USER):
        try:
            connect(DbURL(hostname=host,
                          port=port,
                          dbname=dbname,
                          username=user))
        except Exception, expt:
            tinctest.logger.error("Failed to connect to hostname %s, port %s, database %s, as user %s"
                                  % (host, port, dbname, user))
            tinctest.logger.exception(expt)
            return False
        return True

    @staticmethod
    def _gather_log_from_gp_log_filter(start_time, end_time=None, out_file=_DEFAULT_OUT_FILE, host='localhost',
                                   port=_DEFAULT_PORT, user=_DEFAULT_USER, dbname=_DEFAULT_USER, errors_only=False, master_only=False):
        """
        This retrieves log messages from all segments that happened within the last
        'duration' seconds. The format of start_time and end_time is YYYY-MM-DD [hh:mm[:ss]]
        The tuples returned are (dbid, hostname, datadir, logdata). sorted by dbid.
        Returns True/False based on whether matching log entries were found.
        """
        format_start_time = time.strftime("%Y-%m-%dT%H:%M:%S",time.localtime(start_time))
        if end_time:
            format_end_time = time.strftime("%Y-%m-%dT%H:%M:%S",time.localtime(end_time))
        else:
            format_end_time = time.strftime("%Y-%m-%dT%H:%M:%S",time.localtime())

        tinctest.logger.info("Collecting log from %s to %s into the file -%s" % (format_start_time,format_end_time, out_file))

        array = GpArray.initFromCatalog(DbURL(hostname=host, port=port, username=user, dbname=dbname), True)

        log_chunks = []

        for seg in array.getDbList():
            tinctest.logger.info("Collecting log for segment - %s : %s" %(seg.getSegmentHostName(), seg.getSegmentContentId()))
            if master_only and seg.getSegmentContentId() != -1:
                continue

            cmd = GpLogFilter('collect log chunk',
                              '\\`ls -rt %s | tail -1\\`' % os.path.join(seg.getSegmentDataDirectory(), 'pg_log', '*.csv'),
                              start=format_start_time, end=format_end_time,
                              trouble=errors_only,
                              ctxt=REMOTE,
                              remoteHost=seg.getSegmentHostName())
            cmd.run()
            rc = cmd.get_results().rc
            if rc:
                tinctest.logger.warning("Failed command execution %s : %s" %(cmd, cmd.get_results().stderr))
                continue

            log_data = cmd.get_results().stdout

            if not log_data:
                tinctest.logger.warning("No log data returned for the given time frame.")
            else:
                log_chunks.append((seg.getSegmentContentId(),
                                   seg.getSegmentHostName(),
                                   seg.getSegmentDataDirectory(),
                                   log_data))

        if log_chunks:
            tinctest.logger.info("Writing log data to file - %s" %(out_file))
            with open(out_file, 'w') as f:
                for part in log_chunks:
                    f.write("-"*70)
                    f.write("\n  DBID %s (%s:%s)\n" % (part[0], part[1], part[2]))
                    f.write("-"*70)
                    f.write("\n%s" % part[3])
                    f.write("\n\n")
    
    @staticmethod
    def _gather_log_from_gp_toolkit(start_time, end_time=None, out_file=_DEFAULT_OUT_FILE, host='localhost',
                                   port=_DEFAULT_PORT, user=_DEFAULT_USER, dbname=_DEFAULT_USER, errors_only=False, master_only=False):
        """
        This retrieves log messages from all segments that happened within the last
        'duration' seconds. The format of start_time and end_time is YYYY-MM-DD [hh:mm[:ss]]

        This function gathers logs by querying external tables in gptoolkit. If the cluster is not up and running,
        use _gather_log_from_gp_log_filter which uses the utility gplogfilter to gather logs. 
        """
        format_start_time = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(start_time))
        if end_time:
            format_end_time = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(end_time))
        else:
            format_end_time = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime())

        tinctest.logger.info("Collecting log from %s to %s" % (format_start_time,format_end_time))

        sql_cmd = "select * from gp_toolkit.gp_log_database where logtime >=\'%s\' and logtime <= \'%s\';" % \
                  (format_start_time, format_end_time)
        PSQL.run_sql_command(sql_cmd, out_file=out_file, dbname=dbname,
                             host=host, port=port, username=user, flags='-a -x')
