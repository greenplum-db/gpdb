#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#

"""
TODO: docs!
"""
import os, pickle, base64, time
import os.path
import pipes
try:
    import subprocess32 as subprocess
except:
    import subprocess

import re, socket

from gppylib.gplog import *
from gppylib.db import dbconn
from gppylib import gparray
from gppylib.commands.base import *
from unix import *
import pg
from gppylib import pgconf
from gppylib.utils import writeLinesToFile, createFromSingleHostFile, shellEscape


logger = get_default_logger()

#TODO:  need a better way of managing environment variables.
GPHOME=os.environ.get('GPHOME')

#Default timeout for segment start
SEGMENT_TIMEOUT_DEFAULT=600
SEGMENT_STOP_TIMEOUT_DEFAULT=120

#"Command not found" return code in bash
COMMAND_NOT_FOUND=127

#Default size of thread pool for gpstart and gpsegstart
DEFAULT_GPSTART_NUM_WORKERS=64

# Application name used by the pg_rewind instance that gprecoverseg starts
# during incremental recovery. gpstate uses this to figure out when incremental
# recovery is active.
RECOVERY_REWIND_APPNAME = '__gprecoverseg_pg_rewind__'

def get_postmaster_pid_locally(datadir):
    cmdStr = "ps -ef | grep postgres | grep -v grep | awk '{print $2}' | grep `cat %s/postmaster.pid | head -1` || echo -1" % (datadir)
    name = "get postmaster"
    cmd = Command(name, cmdStr)
    try:
        cmd.run(validateAfter=True)
        sout = cmd.get_results().stdout.lstrip(' ')
        return int(sout.split()[0])
    except:
        return -1

def getPostmasterPID(hostname, datadir):
    cmdStr="ps -ef | grep postgres | grep -v grep | awk '{print $2}' | grep \\`cat %s/postmaster.pid | head -1\\` || echo -1" % (datadir)
    name="get postmaster pid"
    cmd=Command(name,cmdStr,ctxt=REMOTE,remoteHost=hostname)
    try:
        cmd.run(validateAfter=True)
        sout=cmd.get_results().stdout.lstrip(' ')
        return int(sout.split()[1])
    except:
        return -1

#-----------------------------------------------

class CmdArgs(list):
    """
    Conceptually this is a list of an executable path and executable options
    built in a structured manner with a canonical string representation suitable
    for execution via a shell.

    Examples
    --------

    >>> str(CmdArgs(['foo']).set_verbose(True))
    'foo -v'
    >>> str(CmdArgs(['foo']).set_verbose(False).set_wait_timeout(True,600))
    'foo -w -t 600'

    """

    def __init__(self, l):
        list.__init__(self, l)

    def __str__(self):
        return " ".join(self)

    def set_verbose(self, verbose):
        """
        @param verbose - true if verbose output desired
        """
        if verbose: self.append("-v")
        return self

    def set_wrapper(self, wrapper, args):
        """
        @param wrapper - wrapper executable ultimately passed to pg_ctl
        @param args - wrapper arguments ultimately passed to pg_ctl
        """
        if wrapper:
            self.append("--wrapper=\"%s\"" % wrapper)
            if args:
                self.append("--wrapper-args=\"%s\"" % args)
        return self

    def set_wait_timeout(self, wait, timeout):
        """
        @param wait: true if should wait until operation completes
        @param timeout: number of seconds to wait before giving up
        """
        if wait:
            self.append("-w")
        if timeout:
            self.append("-t")
            self.append(str(timeout))
        return self

    def set_segments(self, segments):
        """
        @param segments - segments (from GpArray.getSegmentsByHostName)
        """
        for seg in segments:
            cfg_array = repr(seg)
            self.append("-D '%s'" % (cfg_array))
        return self



class PgCtlBackendOptions(CmdArgs):
    """
    List of options suitable for use with the -o option of pg_ctl.
    Used by MasterStart, SegmentStart to format the backend options
    string passed via pg_ctl -o

    Examples
    --------

    >>> str(PgCtlBackendOptions(5432, 1, 2))
    '-p 5432 --silent-mode=true'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_master())
    '-p 5432 --silent-mode=true -i -c gp_role=dispatch'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_execute())
    '-p 5432 --silent-mode=true -i -c gp_role=execute'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_special('upgrade'))
    '-p 5432 --silent-mode=true -U'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_special('maintenance'))
    '-p 5432 --silent-mode=true -m'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_utility())
    '-p 5432 --silent-mode=true -c gp_role=utility'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_restricted(True,1))
    '-p 5432 --silent-mode=true -c superuser_reserved_connections=1'
    >>>

    """

    def __init__(self, port):
        """
        @param port: backend port
        """
        CmdArgs.__init__(self, [
            "-p", str(port),
        ])

    #
    # master/segment-specific options
    #

    def set_master(self):
        """
        @param is_utility_mode: start with is_utility_mode?
        """
        self.append("-c gp_role=dispatch")
        return self

    #
    # startup mode options
    #

    def set_special(self, special):
        """
        @param special: special mode (none, 'upgrade', 'maintenance', 'convertMasterDataDirToSegment')
        """
        opt = {None:None, 'upgrade':'-U', 'maintenance':'-m', 'convertMasterDataDirToSegment':'-M'}[special]
        if opt: self.append(opt)
        return self

    def set_utility(self):
        self.append("-c gp_role=utility")
        return self

    def set_execute(self):
        self.append("-c gp_role=execute")
        return self

    def set_restricted(self, restricted, max_connections):
        """
        @param restricted: true if restricting connections
        @param max_connections: connection limit
        """
        if restricted:  self.append("-c superuser_reserved_connections=%s" % max_connections)
        return self


class PgCtlStartArgs(CmdArgs):
    """
    Used by MasterStart, SegmentStart to format the pg_ctl command
    to start a backend postmaster.

    Examples
    --------

    >>> a = PgCtlStartArgs("/data1/master/gpseg-1", str(PgCtlBackendOptions(5432, 1, 2)), 123, None, None, True, 600)
    >>> str(a).split(' ') #doctest: +NORMALIZE_WHITESPACE
    ['env', GPERA=123', '$GPHOME/bin/pg_ctl', '-D', '/data1/master/gpseg-1', '-l',
     '/data1/master/gpseg-1/pg_log/startup.log', '-w', '-t', '600',
     '-o', '"', '-p', '5432', '--silent-mode=true', '"', 'start']
    """

    def __init__(self, datadir, backend, era, wrapper, args, wait, timeout=None):
        """
        @param datadir: database data directory
        @param backend: backend options string from PgCtlBackendOptions
        @param era: gpdb master execution era
        @param wrapper: wrapper executable for pg_ctl
        @param args: wrapper arguments for pg_ctl
        @param wait: true if pg_ctl should wait until backend starts completely
        @param timeout: number of seconds to wait before giving up
        """

        CmdArgs.__init__(self, [
            "env",
            "GPSESSID=0000000000", 	# <- overwritten with gp_session_id to help identify orphans
            "GPERA=%s" % str(era),	# <- master era used to help identify orphans
            "$GPHOME/bin/pg_ctl",
            "-D", str(datadir),
            "-l", "%s/pg_log/startup.log" % datadir,
        ])
        self.set_wrapper(wrapper, args)
        self.set_wait_timeout(wait, timeout)
        self.extend([
            "-o", "\"", str(backend), "\"",
            "start"
        ])


class PgCtlStopArgs(CmdArgs):
    """
    Used by MasterStop, SegmentStop to format the pg_ctl command
    to stop a backend postmaster

    >>> str(PgCtlStopArgs("/data1/master/gpseg-1", "smart", True, 600))
    '$GPHOME/bin/pg_ctl -D /data1/master/gpseg-1 -m smart -w -t 600 stop'

    """

    def __init__(self, datadir, mode, wait, timeout):
        """
        @param datadir: database data directory
        @param mode: shutdown mode (smart, fast, immediate)
        @param wait: true if pg_ctlshould wait for backend to stop
        @param timeout: number of seconds to wait before giving up
        """
        CmdArgs.__init__(self, [
            "$GPHOME/bin/pg_ctl",
            "-D", str(datadir),
            "-m", str(mode),
        ])
        self.set_wait_timeout(wait, timeout)
        self.append("stop")


class MasterStart(Command):
    def __init__(self, name, dataDir, port, era,
                 wrapper, wrapper_args, specialMode=None, restrictedMode=False, timeout=SEGMENT_TIMEOUT_DEFAULT,
                 max_connections=1, utilityMode=False, ctxt=LOCAL, remoteHost=None,
                 wait=True
                 ):
        self.dataDir=dataDir
        self.port=port
        self.utilityMode=utilityMode
        self.wrapper=wrapper
        self.wrapper_args=wrapper_args

        # build backend options
        b = PgCtlBackendOptions(port)
        if utilityMode:
            b.set_utility()
        else:
            b.set_master()
        b.set_special(specialMode)
        b.set_restricted(restrictedMode, max_connections)

        # build pg_ctl command
        c = PgCtlStartArgs(dataDir, b, era, wrapper, wrapper_args, wait, timeout)
        logger.info("MasterStart pg_ctl cmd is %s", c);
        self.cmdStr = str(c)

        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

    @staticmethod
    def local(name, dataDir, port, era,
              wrapper, wrapper_args, specialMode=None, restrictedMode=False, timeout=SEGMENT_TIMEOUT_DEFAULT,
              max_connections=1, utilityMode=False):
        cmd=MasterStart(name, dataDir, port, era,
                        wrapper, wrapper_args, specialMode, restrictedMode, timeout,
                        max_connections, utilityMode)
        cmd.run(validateAfter=True)

#-----------------------------------------------
class MasterStop(Command):
    def __init__(self,name,dataDir,mode='smart',timeout=SEGMENT_STOP_TIMEOUT_DEFAULT, ctxt=LOCAL,remoteHost=None):
        self.dataDir = dataDir
        self.cmdStr = str( PgCtlStopArgs(dataDir, mode, True, timeout) )
        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

    @staticmethod
    def local(name,dataDir):
        cmd=MasterStop(name,dataDir)
        cmd.run(validateAfter=True)

#-----------------------------------------------
class SegmentStart(Command):
    """
    SegmentStart is used to start a single segment.

    Note: Most code should probably use GpSegStartCmd instead which starts up
    all of the segments on a specified GpHost.
    """

    def __init__(self, name, gpdb, numContentsInCluster, era, mirrormode,
                 utilityMode=False, ctxt=LOCAL, remoteHost=None,
                 pg_ctl_wait=True, timeout=SEGMENT_TIMEOUT_DEFAULT,
                 specialMode=None, wrapper=None, wrapper_args=None):

        # This is referenced from calling code
        self.segment = gpdb

        # Interesting data from our input segment
        port    = gpdb.getSegmentPort()
        datadir = gpdb.getSegmentDataDirectory()

        # build backend options
        b = PgCtlBackendOptions(port)
        if utilityMode:
            b.set_utility()
        else:
            b.set_execute()
        b.set_special(specialMode)

        # build pg_ctl command
        c = PgCtlStartArgs(datadir, b, era, wrapper, wrapper_args, pg_ctl_wait, timeout)
        logger.info("SegmentStart pg_ctl cmd is %s", c);
        self.cmdStr = str(c) + ' 2>&1'

        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

    @staticmethod
    def local(name, gpdb, numContentsInCluster, era, mirrormode, utilityMode=False):
        cmd=SegmentStart(name, gpdb, numContentsInCluster, era, mirrormode, utilityMode)
        cmd.run(validateAfter=True)

    @staticmethod
    def remote(name, remoteHost, gpdb, numContentsInCluster, era, mirrormode, utilityMode=False):
        cmd=SegmentStart(name, gpdb, numContentsInCluster, era, mirrormode, utilityMode, ctxt=REMOTE, remoteHost=remoteHost)
        cmd.run(validateAfter=True)

#-----------------------------------------------
class SegmentStop(Command):
    def __init__(self, name, dataDir,mode='smart', nowait=False, ctxt=LOCAL,
                 remoteHost=None, timeout=SEGMENT_STOP_TIMEOUT_DEFAULT):

        self.cmdStr = str( PgCtlStopArgs(dataDir, mode, not nowait, timeout) )
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)

    @staticmethod
    def local(name, dataDir,mode='smart'):
        cmd=SegmentStop(name, dataDir,mode)
        cmd.run(validateAfter=True)
        return cmd

    @staticmethod
    def remote(name, hostname, dataDir, mode='smart'):
        cmd=SegmentStop(name, dataDir, mode, ctxt=REMOTE, remoteHost=hostname)
        cmd.run(validateAfter=True)
        return cmd

#-----------------------------------------------
class SegmentIsShutDown(Command):
    """
    Get the pg_controldata status, and check that it says 'shut down'
    """
    def __init__(self,name,directory,ctxt=LOCAL,remoteHost=None):
        cmdStr = "$GPHOME/bin/pg_controldata %s" % directory
        Command.__init__(self,name,cmdStr,ctxt,remoteHost)

    def is_shutdown(self):
        for key, value in self.results.split_stdout():
            if key == 'Database cluster state':
                return value.strip() == 'shut down'
        return False

    @staticmethod
    def local(name,directory):
        cmd=SegmentIsShutDown(name,directory)
        cmd.run(validateAfter=True)

#-----------------------------------------------
class SegmentRewind(Command):
    """
    SegmentRewind is used to run pg_rewind using source server.
    """

    def __init__(self, name, target_host, target_datadir,
                 source_host, source_port,
                 verbose=False, ctxt=REMOTE):

        # Construct the source server libpq connection string
        # We set application_name here so gpstate can identify whether an
        # incremental recovery is occurring.
        source_server = "host={} port={} dbname=template1 application_name={}".format(
            source_host, source_port, RECOVERY_REWIND_APPNAME
        )

        # Build the pg_rewind command. Do not run pg_rewind if recovery.conf
        # file exists in target data directory because the target instance can
        # be started up normally as a mirror for WAL replication catch up.
        rewind_cmd = '[ -f %s/recovery.conf ] || PGOPTIONS="-c gp_role=utility" $GPHOME/bin/pg_rewind --write-recovery-conf --slot="internal_wal_replication_slot" --source-server="%s" --target-pgdata=%s' % (target_datadir, source_server, target_datadir)

        if verbose:
            rewind_cmd = rewind_cmd + ' --progress'

        self.cmdStr = rewind_cmd + ' 2>&1'

        Command.__init__(self, name, self.cmdStr, ctxt, target_host)

#
# list of valid segment statuses that can be requested
#

SEGMENT_STATUS_GET_STATUS = "getStatus"

#
# corresponds to a postmaster string value; result is a string object, or None if version could not be fetched
#
SEGMENT_STATUS__GET_VERSION = "getVersion"

#
# corresponds to a postmaster string value; result is a dictionary object, or None if data could not be fetched
#
# dictionary will contain:
#    mode -> string
#    segmentState -> string
#    dataState -> string
#    resyncNumCompleted -> large integer
#    resyncTotalToComplete -> large integer
#    elapsedTimeSeconds -> large integer
#
SEGMENT_STATUS__GET_MIRROR_STATUS = "getMirrorStatus"

#
# fetch the active PID of this segment; result is a dict with "pid" and "error" values
#
# see comments on getPidStatus in GpSegStatusProgram class
#
SEGMENT_STATUS__GET_PID = "__getPid"

#
# fetch True or False depending on whether the /tmp/.s.PSQL.<port>.lock file is there
#
SEGMENT_STATUS__HAS_LOCKFILE = "__hasLockFile"

#
# fetch True or False depending on whether the postmaster pid file is there
#
SEGMENT_STATUS__HAS_POSTMASTER_PID_FILE = "__hasPostmasterPidFile"

class GpGetStatusUsingTransitionArgs(CmdArgs):
    """
    Examples
    --------

    >>> str(GpGetStatusUsingTransitionArgs([],'request'))
    '$GPHOME/sbin/gpgetstatususingtransition.py -s request'
    """

    def __init__(self, segments, status_request):
        """
        @param status_request
        """
        CmdArgs.__init__(self, [
            "$GPHOME/sbin/gpgetstatususingtransition.py",
            "-s", str(status_request)
        ])
        self.set_segments(segments)


class GpGetSegmentStatusValues(Command):
    """
    Fetch status values for segments on a host

    Results will be a bin-hexed/pickled value that, when unpacked, will give a
    two-level map:

    outer-map maps from SEGMENT_STATUS__* value to inner-map
    inner-map maps from dbid to result (which is usually a string, but may be different)

    @param statusRequestArr an array of SEGMENT_STATUS__ constants
    """
    def __init__(self, name, segments, statusRequestArr, verbose=False, ctxt=LOCAL, remoteHost=None):

        # clone the list
        self.dblist = [x for x in segments]

        # build gpgetstatususingtransition commadn
        status_request = ":".join(statusRequestArr)
        c = GpGetStatusUsingTransitionArgs(segments, status_request)
        c.set_verbose(verbose)
        cmdStr = str(c)

        Command.__init__(self, name, cmdStr, ctxt, remoteHost)

    def decodeResults(self):
        """
        return (warning,outputFromCmd) tuple, where if warning is None then
           results were returned and outputFromCmd should be read.  Otherwise, the warning should
           be logged and outputFromCmd ignored
        """
        if self.get_results().rc != 0:
            return ("Error getting status from host %s" % self.remoteHost, None)

        outputFromCmd = None
        for line in self.get_results().stdout.split('\n'):
            if line.startswith("STATUS_RESULTS:"):
                toDecode = line[len("STATUS_RESULTS:"):]
                outputFromCmd = pickle.loads(base64.urlsafe_b64decode(toDecode))
                break
        if outputFromCmd is None:
            return ("No status output provided from host %s" % self.remoteHost, None)
        return (None, outputFromCmd)


SEGSTART_ERROR_UNKNOWN_ERROR = -1
SEGSTART_SUCCESS = 0
SEGSTART_ERROR_STOP_RUNNING_SEGMENT_FAILED = 5
SEGSTART_ERROR_DATA_DIRECTORY_DOES_NOT_EXIST = 6
SEGSTART_ERROR_PG_CTL_FAILED = 8
SEGSTART_ERROR_PING_FAILED = 10 # not actually done inside GpSegStartCmd, done instead by caller
SEGSTART_ERROR_PG_CONTROLDATA_FAILED = 11
SEGSTART_ERROR_CHECKSUM_MISMATCH = 12


class GpSegStartArgs(CmdArgs):
    """
    Examples
    --------

    >>> str(GpSegStartArgs('en_US.utf-8:en_US.utf-8:en_US.utf-8', 'mirrorless', 'gpversion', 1, 123, 600))
    "$GPHOME/sbin/gpsegstart.py -M mirrorless -V 'gpversion' -n 1 --era 123 -t 600"
    """

    def __init__(self, mirrormode, gpversion, num_cids, era, master_checksum_value, timeout):
        """
        @param mirrormode - mirror start mode (START_AS_PRIMARY_OR_MIRROR or START_AS_MIRRORLESS)
        @param gpversion - version (from postgres --gp-version)
        @param num_cids - number content ids
        @param era - master era
        @param timeout - seconds to wait before giving up
        """
        default_args = [
            "$GPHOME/sbin/gpsegstart.py",
            "-M", str(mirrormode),
            "-V '%s'" % gpversion,
            "-n", str(num_cids),
            "--era", str(era),
            "-t", str(timeout)
        ]
        if master_checksum_value != None:
            default_args.append("--master-checksum-version")
            default_args.append(str(master_checksum_value))

        CmdArgs.__init__(self, default_args)

    def set_special(self, special):
        """
        @param special - special mode
        """
        assert(special in [None, 'upgrade', 'maintenance'])
        if special:
            self.append("-U")
            self.append(special)
        return self

    def set_transition(self, data):
        """
        @param data - pickled transition data
        """
        if data is not None:
            self.append("-p")
            self.append(data)
        return self

    def set_parallel(self, parallel):
        """
        @param parallel - maximum size of a thread pool to start segments
        """
        if parallel is not None:
            self.append("-B")
            self.append(str(parallel))
        return self



class GpSegStartCmd(Command):
    def __init__(self, name, gphome, segments, gpversion,
                 mirrormode, numContentsInCluster, era, master_checksum_value=None,
                 timeout=SEGMENT_TIMEOUT_DEFAULT, verbose=False,
                 ctxt=LOCAL, remoteHost=None, pickledTransitionData=None,
                 specialMode=None, wrapper=None, wrapper_args=None,
                 parallel=None, logfileDirectory=False):

        # Referenced by calling code (in operations/startSegments.py), create a clone
        self.dblist = [x for x in segments]

        # build gpsegstart command string
        c = GpSegStartArgs(mirrormode, gpversion, numContentsInCluster, era, master_checksum_value,timeout)
        c.set_verbose(verbose)
        c.set_special(specialMode)
        c.set_transition(pickledTransitionData)
        c.set_wrapper(wrapper, wrapper_args)
        c.set_segments(segments)
        c.set_parallel(parallel)

        cmdStr = str(c)
        logger.debug(cmdStr)

        if (logfileDirectory):
            cmdStr = cmdStr + " -l '" + logfileDirectory + "'"
        Command.__init__(self,name,cmdStr,ctxt,remoteHost)


#-----------------------------------------------
class GpSegStopCmd(Command):
    def __init__(self, name, gphome, version,mode,dbs,timeout=SEGMENT_STOP_TIMEOUT_DEFAULT,
                 verbose=False, ctxt=LOCAL, remoteHost=None, logfileDirectory=False):
        self.gphome=gphome
        self.dblist=dbs
        self.dirportlist=[]
        self.mode=mode
        self.version=version
        for db in dbs:
            datadir = db.getSegmentDataDirectory()
            port = db.getSegmentPort()
            self.dirportlist.append(datadir + ':' + str(port))
        self.timeout=timeout
        dirstr=" -D ".join(self.dirportlist)
        if verbose:
            setverbose=" -v "
        else:
            setverbose=""

        self.cmdStr="$GPHOME/sbin/gpsegstop.py %s -D %s -m %s -t %s -V '%s'"  %\
                        (setverbose,dirstr,mode,timeout,version)

        if (logfileDirectory):
            self.cmdStr = self.cmdStr + " -l '" + logfileDirectory + "'"
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)


#-----------------------------------------------
class GpStandbyStart(MasterStart, object):
    """
    Start up the master standby.  The options to postgres in standby
    are almost same as primary master, with a few exceptions.
    The standby will be up as dispatch mode, and could be in remote.
    """

    def __init__(self, name, datadir, port, ctxt=LOCAL,
                 remoteHost=None, era=None,
                 wrapper=None, wrapper_args=None):
        super(GpStandbyStart, self).__init__(
                name=name,
                dataDir=datadir,
                port=port,
                era=era,
                wrapper=wrapper,
                wrapper_args=wrapper_args,
                ctxt=ctxt,
                remoteHost=remoteHost,
                wait=False
                )

    @staticmethod
    def local(name, datadir, port, era=None,
              wrapper=None, wrapper_args=None):
        cmd = GpStandbyStart(name, datadir, port,
                             era=era,
                             wrapper=wrapper, wrapper_args=wrapper_args)
        cmd.run(validateAfter=True)
        return cmd

    @staticmethod
    def remote(name, host, datadir, port, era=None,
               wrapper=None, wrapper_args=None):
        cmd = GpStandbyStart(name, datadir, port, ctxt=REMOTE,
                             remoteHost=host, era=era,
                             wrapper=wrapper, wrapper_args=wrapper_args)
        cmd.run(validateAfter=True)
        return cmd

#-----------------------------------------------
class GpStart(Command):
    def __init__(self, name, masterOnly=False, restricted=False, verbose=False,ctxt=LOCAL, remoteHost=None):
        self.cmdStr="$GPHOME/bin/gpstart -a"
        if masterOnly:
            self.cmdStr += " -m"
            self.propagate_env_map['GPSTART_INTERNAL_MASTER_ONLY'] = 1
        if restricted:
            self.cmdStr += " -R"
        if verbose or logging_is_verbose():
            self.cmdStr += " -v"
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)

    @staticmethod
    def local(name,masterOnly=False,restricted=False):
        cmd=GpStart(name,masterOnly,restricted)
        cmd.run(validateAfter=True)

#-----------------------------------------------
class NewGpStart(Command):
    def __init__(self, name, masterOnly=False, restricted=False, verbose=False,nostandby=False,ctxt=LOCAL, remoteHost=None, masterDirectory=None):
        self.cmdStr="$GPHOME/bin/gpstart -a"
        if masterOnly:
            self.cmdStr += " -m"
            self.propagate_env_map['GPSTART_INTERNAL_MASTER_ONLY'] = 1
        if restricted:
            self.cmdStr += " -R"
        if verbose or logging_is_verbose():
            self.cmdStr += " -v"
        if nostandby:
            self.cmdStr += " -y"
        if masterDirectory:
            self.cmdStr += " -d " + masterDirectory

        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)

    @staticmethod
    def local(name,masterOnly=False,restricted=False,verbose=False,nostandby=False,
              masterDirectory=None):
        cmd=NewGpStart(name,masterOnly,restricted,verbose,nostandby,
                       masterDirectory=masterDirectory)
        cmd.run(validateAfter=True)

#-----------------------------------------------
class NewGpStop(Command):
    def __init__(self, name, masterOnly=False, restart=False, fast=False, force=False, verbose=False, ctxt=LOCAL, remoteHost=None):
        self.cmdStr="$GPHOME/bin/gpstop -a"
        if masterOnly:
            self.cmdStr += " -m"
        if verbose or logging_is_verbose():
            self.cmdStr += " -v"
        if fast:
            self.cmdStr += " -f"
        if restart:
            self.cmdStr += " -r"
        if force:
            self.cmdStr += " -M immediate"
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)

    @staticmethod
    def local(name,masterOnly=False, restart=False, fast=False, force=False, verbose=False):
        cmd=NewGpStop(name,masterOnly,restart, fast, force, verbose)
        cmd.run(validateAfter=True)

#-----------------------------------------------
class GpStop(Command):
    def __init__(self, name, masterOnly=False, verbose=False, quiet=False, restart=False, fast=False, force=False, datadir=None, parallel=None, reload=False, ctxt=LOCAL, remoteHost=None, logfileDirectory=False):
        self.cmdStr="$GPHOME/bin/gpstop -a"
        if masterOnly:
            self.cmdStr += " -m"
        if restart:
            self.cmdStr += " -r"
        if fast:
            self.cmdStr += " -f"
        if force:
            self.cmdStr += " -M immediate"
        if datadir:
            self.cmdStr += " -d %s" % datadir
        if verbose or logging_is_verbose():
            self.cmdStr += " -v"
        if quiet:
            self.cmdStr += " -q"
        if parallel:
            self.cmdStr += " -B %s" % parallel
        if logfileDirectory:
            self.cmdStr += " -l '" + logfileDirectory + "'"
        if reload:
            self.cmdStr += " -u"
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)

    @staticmethod
    def local(name,masterOnly=False, verbose=False, quiet=False,restart=False, fast=False, force=False, datadir=None, parallel=None, reload=False):
        cmd=GpStop(name,masterOnly,verbose,quiet,restart,fast,force,datadir,parallel,reload)
        cmd.run(validateAfter=True)
        return cmd

#-----------------------------------------------
class ModifyConfSetting(Command):
    def __init__(self, name, file, optName, optVal, optType='string', ctxt=LOCAL, remoteHost=None):
        cmdStr = None
        if optType == 'number':
            cmdStr = "perl -p -i.bak -e 's/^%s[ ]*=[ ]*\\d+/%s=%d/' %s" % (optName, optName, optVal, file)
        elif optType == 'string':
            cmdStr = "perl -i -p -e \"s/^%s[ ]*=[ ]*'[^']*'/%s='%s'/\" %s" % (optName, optName, optVal, file)
        else:
            raise Exception, "Invalid optType for ModifyConfSetting"
        self.cmdStr = cmdStr
        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

class ModifyPgHbaConfSetting(Command):
    def __init__(self, name, file, ctxt, remoteHost, addresses, is_hba_hostnames):
        username = getUserName()
        # Add a samehost replication entry to support single-host development.
        hba_content = "\nhost replication {username} samehost trust".format(username=username)

        for address in addresses:
            if is_hba_hostnames:
                hba_content += "\nhost all {username} {hostname} trust".format(username=username, hostname=address)
                hba_content += "\nhost replication {username} {hostname} trust".format(username=username, hostname=address)
            else:
                ips = InterfaceAddrs.remote('get mirror ips', address)
                for ip in ips:
                    cidr_suffix = '/128' if ':' in ip else '/32'
                    cidr = ip + cidr_suffix
                    hba_content += "\nhost all {username} {cidr} trust".format(username=username, cidr=cidr)
                if_addrs = IfAddrs.list_addrs(address)
                for ip in if_addrs:
                    cidr_suffix = '/128' if ':' in ip else '/32'
                    cidr = ip + cidr_suffix
                    hba_content += "\nhost replication {username} {cidr} trust".format(username=username, cidr=cidr)

        # You might think you can substitute the primary and mirror addresses
        # with the new primary and mirror addresses, but what if they were the
        # same? Then you could end up with only the new primary or new mirror
        # address.
        cmdStr = "echo '{0}' >> {1}".format(hba_content, file)
        self.cmdStr = cmdStr
        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

#-----------------------------------------------
class GpCleanSegmentDirectories(Command):
    """
    Clean all of the directories for a set of segments on the host.

    Does NOT delete all files in the data directories -- tries to preserve logs and any non-database
       files the user has placed there
    """
    def __init__(self, name, segmentsToClean, ctxt, remoteHost):
        pickledSegmentsStr = base64.urlsafe_b64encode(pickle.dumps(segmentsToClean))
        cmdStr = "$GPHOME/sbin/gpcleansegmentdir.py -p %s" % pickledSegmentsStr
        Command.__init__(self, name, cmdStr, ctxt, remoteHost)

#-----------------------------------------------
class GpDirsExist(Command):
    """
    Checks if gp_dump* directories exist in the given directory
    """
    def __init__(self, name, baseDir, dirName, ctxt=LOCAL, remoteHost=None):
        cmdStr = "find %s -name %s -type d -print" % (baseDir, dirName)
        Command.__init__(self, name, cmdStr, ctxt, remoteHost)

    @staticmethod
    def local(name, baseDir, dirName):
        cmd = GpDirsExist(name, baseDir=baseDir, dirName=dirName)
        cmd.run(validateAfter=True)
        dirCount = len(cmd.get_results().stdout.split('\n'))
        # This is > 1 because the command output will terminate with \n
        return dirCount > 1


#-----------------------------------------------
class ConfigureNewSegment(Command):
    """
    Configure a new segment, usually from a template, as is done during gpexpand, gpaddmirrors, gprecoverseg (full),
      etc.
    """

    def __init__(self, name, confinfo, logdir, newSegments=False, tarFile=None,
                 batchSize=None, verbose=False,ctxt=LOCAL, remoteHost=None, validationOnly=False, writeGpIdFileOnly=False,
                 forceoverwrite=False):

        cmdStr = '$GPHOME/bin/lib/gpconfigurenewsegment -c \"%s\" -l %s' % (confinfo, pipes.quote(logdir))

        if newSegments:
            cmdStr += ' -n'
        if tarFile:
            cmdStr += ' -t %s' % tarFile
        if verbose:
            cmdStr += ' -v '
        if batchSize:
            cmdStr += ' -B %s' % batchSize
        if validationOnly:
            cmdStr += " --validation-only"
        if writeGpIdFileOnly:
            cmdStr += " --write-gpid-file-only"
        if forceoverwrite:
            cmdStr += " --force-overwrite"

        Command.__init__(self, name, cmdStr, ctxt, remoteHost)

    #-----------------------------------------------
    @staticmethod
    def buildSegmentInfoForNewSegment(segments, isTargetReusedLocationArr = None, primaryMirror = 'both'):
        """
        Build the new segment info that can be used to get the confinfo argument to pass to ConfigureNewSegment

        @param segments list of segments

        @param isTargetReusedLocationArr if not None, then is an array of boolean values in parallel with segments
                                      True values indicate that the directory has been cleaned by gpcleansegmentdir.py
                                      and we should have lighter restrictions on how to check it for emptiness
                                      Passing None is the same as passing an array of all False values

        @param primaryMirror Process 'primary' or 'mirror' or 'both'

        @return A dictionary with the following format:

                Name  =   <host name>
                Value =   <system data directory>
                        : <port>
                        : if primary then 'true' else 'false'
                        : if target is reused location then 'true' else 'false'
                        : <segment dbid>
        """
        result = {}
        for segIndex, seg in enumerate(segments):
            if primaryMirror == 'primary' and seg.isSegmentPrimary() == False:
               continue
            elif primaryMirror == 'mirror' and seg.isSegmentPrimary() == True:
               continue
            hostname = seg.getSegmentHostName()
            if result.has_key(hostname):
                result[hostname] += ','
            else:
                result[hostname] = ''

            isTargetReusedLocation = isTargetReusedLocationArr and isTargetReusedLocationArr[segIndex]
            # only a mirror segment has these two attributes
            # added on the fly, by callers
            primaryHostname = getattr(seg, 'primaryHostname', "")
            primarySegmentPort = getattr(seg, 'primarySegmentPort', "-1")
            if primaryHostname == "":
                isPrimarySegment =  "true" if seg.isSegmentPrimary(current_role=True) else "false"
                isTargetReusedLocationString = "true" if isTargetReusedLocation else "false"
            else:
                isPrimarySegment = "false"
                isTargetReusedLocationString = "false"

            progressFile = getattr(seg, 'progressFile', "")

            result[hostname] += '%s:%d:%s:%s:%d:%d:%s:%s:%s' % (seg.getSegmentDataDirectory(), seg.getSegmentPort(),
                                                          isPrimarySegment,
                                                          isTargetReusedLocationString,
                                                          seg.getSegmentDbId(),
                                                          seg.getSegmentContentId(),
                                                          primaryHostname,
                                                          primarySegmentPort,
                                                          progressFile
            )
        return result

#-----------------------------------------------
class GpVersion(Command):
    def __init__(self,name,gphome,ctxt=LOCAL,remoteHost=None):
        # XXX this should make use of the gphome that was passed
        # in, but this causes problems in some environments and
        # requires further investigation.

        self.gphome=gphome
        #self.cmdStr="%s/bin/postgres --gp-version" % gphome
        self.cmdStr="$GPHOME/bin/postgres --gp-version"
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)

    def get_version(self):
        return self.results.stdout.strip()

    @staticmethod
    def local(name,gphome):
        cmd=GpVersion(name,gphome)
        cmd.run(validateAfter=True)
        return cmd.get_version()

#-----------------------------------------------
class GpCatVersion(Command):
    """
    Get the catalog version of the binaries in a given GPHOME
    """
    def __init__(self,name,gphome,ctxt=LOCAL,remoteHost=None):
        # XXX this should make use of the gphome that was passed
        # in, but this causes problems in some environments and
        # requires further investigation.
        self.gphome=gphome
        #cmdStr="%s/bin/postgres --catalog-version" % gphome
        cmdStr="$GPHOME/bin/postgres --catalog-version"
        Command.__init__(self,name,cmdStr,ctxt,remoteHost)

    def get_version(self):
        # Version comes out like this:
        #   "Catalog version number:               201002021"
        # We only want the number
        return self.results.stdout.split(':')[1].strip()

    @staticmethod
    def local(name,gphome):
        cmd=GpCatVersion(name,gphome)
        cmd.run(validateAfter=True)
        return cmd.get_version()

#-----------------------------------------------
class GpCatVersionDirectory(Command):
    """
    Get the catalog version of a given database directory
    """
    def __init__(self,name,directory,ctxt=LOCAL,remoteHost=None):
        cmdStr = "$GPHOME/bin/pg_controldata %s" % directory
        Command.__init__(self,name,cmdStr,ctxt,remoteHost)

    def get_version(self):
        "sift through pg_controldata looking for the catalog version number"
        for key, value in self.results.split_stdout():
            if key == 'Catalog version number':
                return value.strip()

    @staticmethod
    def local(name,directory):
        cmd=GpCatVersionDirectory(name,directory)
        cmd.run(validateAfter=True)
        return cmd.get_version()


#-----------------------------------------------
class GpConfigHelper(Command):
    def __init__(self, command_name, postgresconf_dir, name, value=None, segInfo=None, removeParameter=False, getParameter=False, ctxt=LOCAL, remoteHost=None):
        self.segInfo = segInfo

        addParameter = (not getParameter) and (not removeParameter)
        if addParameter:
            args = '--add-parameter %s --value %s ' % (name, base64.urlsafe_b64encode(pickle.dumps(value)))
        if getParameter:
            args = '--get-parameter %s' % name
        if removeParameter:
            args = '--remove-parameter %s' % name

        cmdStr = "$GPHOME/sbin/gpconfig_helper.py --file %s %s" % (
            os.path.join(postgresconf_dir, 'postgresql.conf'),
            args)

        Command.__init__(self, command_name, cmdStr, ctxt, remoteHost)

    # FIXME: figure out how callers of this can handle exceptions here
    def get_value(self):
        raw_value = self.get_results().stdout
        return pickle.loads(base64.urlsafe_b64decode(raw_value))


#-----------------------------------------------
class GpLogFilter(Command):
    def __init__(self, name, filename, start=None, end=None, duration=None,
                 case=None, count=None, search_string=None,
                 exclude_string=None, search_regex=None, exclude_regex=None,
                 trouble=None, ctxt=LOCAL,remoteHost=None):
        cmdfrags = []
        if start:
            cmdfrags.append('--begin=%s' % start)
        if end:
            cmdfrags.append('--end=%s' % end)
        if duration:
            cmdfrags.append('--duration=%s' % duration)
        if case:
            cmdfrags.append('--case=%s' % case)
        if search_string:
            cmdfrags.append('--find=\'%s\'' % search_string)
        if exclude_string:
            cmdfrags.append('--nofind=\'%s\'' % exclude_string)
        if search_regex:
            cmdfrags.append('--match=\'%s\'' % search_regex)
        if count:
            cmdfrags.append('-n %s' % count)
        if exclude_regex:
            cmdfrags.append('--nomatch=\'%s\'' % exclude_regex)
        if trouble:
            cmdfrags.append('-t')
        cmdfrags.append(filename)
        self.cmdStr = "$GPHOME/bin/gplogfilter %s" % ' '.join(cmdfrags)
        Command.__init__(self, name, self.cmdStr, ctxt,remoteHost)

    @staticmethod
    def local(name, filename, start=None, end=None, duration=None,
               case=None, count=None, search_string=None,
               exclude_string=None, search_regex=None, exclude_regex=None,
               trouble=None):
        cmd = GpLogFilter(name, filename, start, end, duration, case, count, search_string,
                          exclude_string, search_regex, exclude_regex, trouble)
        cmd.run(validateAfter=True)
        return "".join(cmd.get_results().stdout).split("\r\n")

#-----------------------------------------------
def distribute_tarball(queue,list,tarball):
        logger.debug("distributeTarBall start")
        for db in list:
            hostname = db.getSegmentHostName()
            datadir = db.getSegmentDataDirectory()
            (head,tail)=os.path.split(datadir)
            scp_cmd=Scp(name="copy master",srcFile=tarball,dstHost=hostname,dstFile=head)
            queue.addCommand(scp_cmd)
        queue.join()
        queue.check_results()
        logger.debug("distributeTarBall finished")




class GpError(Exception): pass

######
def get_gphome():
    gphome=os.getenv('GPHOME',None)
    if not gphome:
        raise GpError('Environment Variable GPHOME not set')
    return gphome


######
def get_masterdatadir():
    master_datadir = os.environ.get('MASTER_DATA_DIRECTORY')
    if not master_datadir:
        raise GpError("Environment Variable MASTER_DATA_DIRECTORY not set!")
    return master_datadir

######
def get_masterport(datadir):
    return pgconf.readfile(os.path.join(datadir, 'postgresql.conf')).int('port')


######
def check_permissions(username):
    logger.debug("--Checking that current user can use GP binaries")
    chk_gpdb_id(username)





class _GpExpandStatus(object):
    '''
    Internal class to store gpexpand status, note it's different with the
    GpExpandStatus class inside the gpexpand command.
    '''

    dbname = 'postgres'

    # status information
    phase = 0
    status = 'NO EXPANSION DETECTED'

    # progress information, arrays of (dbname, relname, status)
    uncompleted = []
    completed = []
    inprogress = []

    def _get_phase1_status(self):
        # first assume no expansion is in progress
        self.phase = 0
        self.status = 'NO EXPANSION DETECTED'

        datadir = get_masterdatadir()
        filename = os.path.join(datadir, 'gpexpand.status')
        try:
            with open(filename, 'r') as f:
                # it is phase1 as long as the status file exists
                self.phase = 1
                self.status = 'UNKNOWN PHASE1 STATUS'

                lines = f.readlines()
        except IOError:
            return self.phase > 0

        if lines and lines[-1].split(':')[0]:
            self.status = lines[-1].split(':')[0]

        return True

    def _get_phase2_status(self):
        # first assume no expansion is in progress
        self.phase = 0
        self.status = 'NO EXPANSION DETECTED'

        sql = '''
            SELECT status FROM gpexpand.status ORDER BY updated DESC LIMIT 1
        '''

        try:
            dburl = dbconn.DbURL(dbname=self.dbname)
            with dbconn.connect(dburl, encoding='UTF8') as conn:
                status = dbconn.execSQLForSingleton(conn, sql)
        except Exception:
            # schema table not found
            return False

        # it is phase2 as long as the schema table exists
        self.phase = 2
        if status:
            self.status = status
        else:
            self.status = 'UNKNOWN PHASE2 STATUS'
        return True

    def _get_phase2_progress(self):
        self.uncompleted = []
        self.completed = []
        self.inprogress = []

        sql = '''
            SELECT quote_ident(dbname) AS fq_dbname, fq_name, status
              FROM gpexpand.status_detail
        '''

        try:
            dburl = dbconn.DbURL(dbname=self.dbname)
            with dbconn.connect(dburl, encoding='UTF8') as conn:
                cursor = dbconn.execSQL(conn, sql)
                rows = cursor.fetchall()
        except Exception:
            return False

        for row in rows:
            _, _, status = row
            if status == 'COMPLETED':
                self.completed.append(row)
            elif status == 'IN PROGRESS':
                self.inprogress.append(row)
            else:
                self.uncompleted.append(row)

        return True

    def get_status(self):
        '''
        Get gpexpand status, such as whether an expansion is in progress,
        which expansion phase it is.
        '''

        if self._get_phase1_status():
            pass
        elif self._get_phase2_status():
            pass

        return self

    def get_progress(self):
        '''
        Get data redistribution progress of phase 2.
        '''

        self._get_phase2_progress()

        return self

def get_gpexpand_status():
    '''
    Get gpexpand status
    '''
    status = _GpExpandStatus()
    status.get_status()
    return status

def conflict_with_gpexpand(utility, refuse_phase1=True, refuse_phase2=False):
    '''
    Generate error message when gpexpand is running in specified phases.
    Some utilities can not run in parallel with gpexpand, this function can be
    used to simplify the checks on gpexpand status.
    '''
    status = get_gpexpand_status()

    if status.phase == 1 and refuse_phase1:
        err_msg = ("ERROR: Usage of {utility} is not supported while the "
                   "cluster is in a reconfiguration state, "
                   "exit {utility}")
        return (False, err_msg.format(utility=utility))

    if status.phase == 2 and refuse_phase2:
        err_msg = ("ERROR: Usage of {utility} is not supported while the "
                   "cluster has tables waiting for expansion, "
                   "exit {utility}")
        return (False, err_msg.format(utility=utility))

    return (True, "")

#=-=-=-=-=-=-=-=-=-= Bash Migration Helper Functions =-=-=-=-=-=-=-=-

def start_standbymaster(host, datadir, port, era=None,
                        wrapper=None, wrapper_args=None):
    logger.info("Starting standby master")

    logger.info("Checking if standby master is running on host: %s  in directory: %s" % (host,datadir))
    cmd = Command("recovery_startup",
                  ("python -c "
                   "'from gppylib.commands.gp import recovery_startup; "
                   """recovery_startup("{0}", "{1}")'""").format(
                       datadir, port),
                  ctxt=REMOTE, remoteHost=host)
    cmd.run()
    res = cmd.get_results().stderr

    if res:
        logger.warning("Unable to cleanup previously started standby: '%s'" % res)

    cmd = GpStandbyStart.remote('start standby master',
                                host, datadir, port, era=era,
                                wrapper=wrapper, wrapper_args=wrapper_args)
    logger.debug("Starting standby: %s" % cmd )

    logger.debug("Starting standby master results: %s" % cmd.get_results() )

    if cmd.get_results().rc != 0:
        logger.warning("Could not start standby master: %s" % cmd)
        return False

    # Wait for the standby to start recovery.  Ideally this means the
    # standby connection is recognized by the primary, but locally in this
    # function it is better to work with only standby.  If recovery has
    # started, this means now postmaster is responsive to signals, which
    # allows shutdown etc.  If we exit earlier, there is a big chance
    # a shutdown message from other process is missed.
    for i in xrange(60):
        # Fetch it every time, as postmaster might not have been up yet for
        # the first few cycles, which we have seen when trying wrapper
        # shell script.
        pid = getPostmasterPID(host, datadir)
        cmd = Command("get pids",
                      ("python -c "
                       "'from gppylib.commands import unix; "
                       "print unix.getDescendentProcesses({0})'".format(pid)),
                      ctxt=REMOTE, remoteHost=host)
        cmd.run()
        logger.debug(str(cmd))
        result = cmd.get_results()
        logger.debug(result)
        # We want more than postmaster and logger processes.
        if result.rc == 0 and len(result.stdout.split(',')) > 2:
            return True
        time.sleep(1)

    logger.warning("Could not start standby master")
    return False

def get_pid_from_remotehost(host, datadir):
    cmd = Command(name = 'get the pid from postmaster file',
                  cmdStr = 'head -1 %s/postmaster.pid' % datadir,
                  ctxt=REMOTE, remoteHost = host)
    cmd.run()
    pid = None
    if cmd.get_results().rc == 0 and cmd.get_results().stdout.strip():
        pid = int(cmd.get_results().stdout.strip())
    return pid

def is_pid_postmaster(datadir, pid, remoteHost=None):
    """
    This function returns true on any uncertainty: if it cannot execute pgrep, pwdx or just connect to the standby host
    it will return true
    """
    def validate_command (commandName, datadir, ctxt, remoteHost):
        cmd = Command ('Check %s availability' % commandName, commandName, ctxt=ctxt, remoteHost=remoteHost)
        cmd.run()
        if cmd.get_results().rc == COMMAND_NOT_FOUND:
            if not remoteHost is None:
                logger.warning('command "%s" is not found on host %s. cannot check postmaster status, assuming it is running', commandName, remoteHost)
            else:
                logger.warning('command "%s" is not found. cannot check postmaster status, assuming it is running', commandName)
            return False
        return True

    if remoteHost is not None:
        ctxt = REMOTE
    else:
        ctxt = LOCAL

    is_postmaster = True
    if (validate_command ('pgrep', datadir, ctxt, remoteHost) and
            validate_command ('pwdx', datadir, ctxt, remoteHost)):
        cmdStr = 'pgrep postgres | xargs -I{} pwdx {} | grep "%s" | grep "^%s:" | cat' % (datadir, pid)
        cmd = Command("search for postmaster process", cmdStr, ctxt=ctxt, remoteHost=remoteHost)
        res = None
        try:
            cmd.run(validateAfter=True)
            res = cmd.get_results()
            if not res.stdout.strip():
                is_postmaster = False
            else:
                logger.info(res.stdout.strip())
        except Exception as e:
            if not remoteHost is None:
                logger.warning('failed to get the status of postmaster %s on %s. assuming that postmaster is running' % (datadir, remoteHost))
            else:
                logger.warning('failed to get the status of postmaster %s. assuming that postmaster is running' % (datadir))

    return is_postmaster

######
def recovery_startup(datadir, port=None):
    """ investigate a db that may still be running """

    pid=read_postmaster_pidfile(datadir)
    if check_pid(pid) and is_pid_postmaster(datadir, pid):
        info_str="found postmaster with pid: %d for datadir: %s still running" % (pid,datadir)
        logger.info(info_str)

        logger.info("attempting to shutdown db with datadir: %s" % datadir )
        cmd=SegmentStop('db shutdown' , datadir,mode='fast')
        cmd.run()

        if check_pid(pid) and is_pid_postmaster(datadir, pid):
            info_str="unable to stop postmaster with pid: %d for datadir: %s still running" % (pid,datadir)
            logger.info(info_str)
            return info_str
        else:
            logger.info("shutdown of db successful with datadir: %s" % datadir)
            return None
    else:
        # If we get this far it means we don't have a pid and need to do some
        # cleanup.  Use port number if supplied.  postgresql.conf may be
        # bogus as we pass port number anyway instead of postgresql.conf
        # default value.
        if port is None:
            pgconf_dict = pgconf.readfile(datadir + "/postgresql.conf")
            port = pgconf_dict.int('port')

        lockfile="/tmp/.s.PGSQL.%s" % port
        tmpfile_exists = os.path.exists(lockfile)


        logger.info("No db instance process, entering recovery startup mode")

        if tmpfile_exists:
            logger.info("Clearing db instance lock files")
            os.remove(lockfile)

        postmaster_pid_file = "%s/postmaster.pid" % datadir
        if os.path.exists(postmaster_pid_file):
            logger.info("Clearing db instance pid file")
            os.remove("%s/postmaster.pid" % datadir)

        return None


# these match names from gp_bash_functions.sh
def chk_gpdb_id(username):
    path="%s/bin/initdb" % GPHOME
    if not os.access(path,os.X_OK):
        raise GpError("File permission mismatch.  The current user %s does not have sufficient"
                      " privileges to run the Greenplum binaries and management utilities." % username )


def chk_local_db_running(datadir, port):
    """Perform a few checks to see if the db is running.  We 1st look at:

       1) /tmp/.s.PGSQL.<PORT> and /tmp/.s.PGSQL.<PORT>.lock
       2) DATADIR/postmaster.pid
       3) netstat

       Returns tuple in format (postmaster_pid_file_exists, tmpfile_exists, lockfile_exists, port_active, postmaster_pid)

       postmaster_pid value is 0 if postmaster_pid_exists is False  Note that this is the PID from the postmaster.pid file

    """

    # determine if postmaster.pid is there, grab pid from it
    postmaster_pid_exists = True
    f = None
    try:
        f = open(datadir + "/postmaster.pid")
    except IOError:
        postmaster_pid_exists = False

    pid_value = 0
    if postmaster_pid_exists:
        try:
            for line in f:
                pid_value = int(line) # grab first line only
                break
        finally:
            f.close()

    tmpfile_exists = os.path.exists("/tmp/.s.PGSQL.%d" % port)
    lockfile_exists = os.path.exists(get_lockfile_name(port))

    netstat_port_active = PgPortIsActive.local('check netstat for postmaster port',"/tmp/.s.PGSQL.%d" % port, port)

    logger.debug("postmaster_pid_exists: %s tmpfile_exists: %s lockfile_exists: %s netstat port: %s  pid: %s" %\
                (postmaster_pid_exists, tmpfile_exists, lockfile_exists, netstat_port_active, pid_value))

    return (postmaster_pid_exists, tmpfile_exists, lockfile_exists, netstat_port_active, pid_value)

def get_lockfile_name(port):
    return "/tmp/.s.PGSQL.%d.lock" % port


def get_local_db_mode(master_data_dir):
    """ Gets the mode Greenplum is running in.
        Possible return values are:
            'NORMAL'
            'RESTRICTED'
            'UTILITY'
    """
    mode = 'NORMAL'

    if not os.path.exists(master_data_dir + '/postmaster.pid'):
        raise Exception('Greenplum database appears to be stopped')

    try:
        fp = open(master_data_dir + '/postmaster.opts', 'r')
        optline = fp.readline()
        if optline.find('superuser_reserved_connections') > 0:
            mode = 'RESTRICTED'
        elif optline.find('gp_role=utility') > 0:
            mode = 'UTILITY'
    except OSError:
        raise Exception('Failed to open %s.  Is Greenplum Database running?' % master_data_dir + '/postmaster.opts')
    except IOError:
        raise Exception('Failed to read options from %s' % master_data_dir + '/postmaster.opts')
    finally:
        if fp: fp.close()

    return mode

######
def read_postmaster_pidfile(datadir, host=None):
    if host:
        cmdStr ="""python -c 'from {module} import {func}; print {func}("{args}")'""".format(module=sys.modules[__name__].__name__,
                                                                                             func='read_postmaster_pidfile',
                                                                                             args=datadir)
        cmd = Command(name='run this method remotely', cmdStr=cmdStr, ctxt=REMOTE, remoteHost=host)
        cmd.run(validateAfter=True)
        return int(cmd.get_results().stdout.strip())
    pid=0
    f = None
    try:
        f = open(datadir + '/postmaster.pid')
        pid = int(f.readline().strip())
    except Exception:
        pass
    finally:
        if f: f.close()
    return pid


def createTempDirectoryName(masterDataDirectory, tempDirPrefix):
    return '%s/%s_%s_%d' % (os.sep.join(os.path.normpath(masterDataDirectory).split(os.sep)[:-1]),
                                tempDirPrefix,
                                datetime.datetime.now().strftime('%m%d%Y'),
                                os.getpid())

#-------------------------------------------------------------------------
class GpRecoverSeg(Command):
   """
   This command will execute the gprecoverseg utility
   """

   def __init__(self, name, options = "", ctxt = LOCAL, remoteHost = None):
       self.name = name
       self.options = options
       self.ctxt = ctxt
       self.remoteHost = remoteHost

       cmdStr = "$GPHOME/bin/gprecoverseg %s" % (options)
       Command.__init__(self,name,cmdStr,ctxt,remoteHost)

class IfAddrs:
    @staticmethod
    def list_addrs(hostname=None, include_loopback=False):
        cmd = ['%s/libexec/ifaddrs' % GPHOME]
        if not include_loopback:
            cmd.append('--no-loopback')
        if hostname:
            args = ['ssh', '-n', hostname]
            args.append(' '.join(cmd))
        else:
            args = cmd

        result = subprocess.check_output(args)
        return result.splitlines()

if __name__ == '__main__':

    import doctest
    doctest.testmod()
