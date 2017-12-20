#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#

"""
TODO: docs!
"""
import os, pickle, base64, time

import re

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
class PySync(Command):
    def __init__(self,name,srcDir,dstHost,dstDir,ctxt=LOCAL,remoteHost=None, options=None):
        psync_executable=GPHOME + "/bin/lib/pysync.py"

        # MPP-13617
        if ':' in dstHost and not ']' in dstHost:
            dstHost = '[' + dstHost + ']'

        self.cmdStr="%s %s %s %s:%s" % (psync_executable,
                                        options if options else "",
                                        srcDir,
                                        dstHost,
                                        dstDir)
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)


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
    '-p 5432 --gp_dbid=1 --gp_num_contents_in_cluster=2 --silent-mode=true'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_master(2, False, False))
    '-p 5432 --gp_dbid=1 --gp_num_contents_in_cluster=2 --silent-mode=true -i -M master --gp_contentid=-1 -x 2'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_master(2, False, True))
    '-p 5432 --gp_dbid=1 --gp_num_contents_in_cluster=2 --silent-mode=true -i -M master --gp_contentid=-1 -x 2 -E'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_segment('mirror', 1))
    '-p 5432 --gp_dbid=1 --gp_num_contents_in_cluster=2 --silent-mode=true -i -M mirror --gp_contentid=1'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_special('upgrade'))
    '-p 5432 --gp_dbid=1 --gp_num_contents_in_cluster=2 --silent-mode=true -U'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_special('maintenance'))
    '-p 5432 --gp_dbid=1 --gp_num_contents_in_cluster=2 --silent-mode=true -m'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_utility(True))
    '-p 5432 --gp_dbid=1 --gp_num_contents_in_cluster=2 --silent-mode=true -c gp_role=utility'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_utility(False))
    '-p 5432 --gp_dbid=1 --gp_num_contents_in_cluster=2 --silent-mode=true'
    >>> str(PgCtlBackendOptions(5432, 1, 2).set_restricted(True,1))
    '-p 5432 --gp_dbid=1 --gp_num_contents_in_cluster=2 --silent-mode=true -c superuser_reserved_connections=1'
    >>>

    """

    def __init__(self, port, dbid, numcids):
        """
        @param port: backend port
        @param dbid: backed dbid
        @param numcids: total number of content ids in cluster
        """
        CmdArgs.__init__(self, [
            "-p", str(port),
            "--gp_dbid="+ str(dbid),
            "--gp_num_contents_in_cluster="+ str(numcids),
            "--silent-mode=true"
        ])

    #
    # master/segment-specific options
    #

    def set_master(self, standby_dbid, disable, seqserver):
        """
        @param standby_dbid: standby dbid
        @param disable: start without master mirroring?
        @param seqserver: start with seqserver?
        """
        self.extend(["-i", "-M", "master", "--gp_contentid=-1", "-x", str(standby_dbid)])
        if disable: self.append("-y")
        if seqserver: self.append("-E")
        return self

    def set_segment(self, mode, content):
        """
        @param mode: mirroring mode
        @param content: content id
        """
        self.extend(["-i", "-M", "mirrorless", "--gp_contentid="+str(content)])
        return self

    #
    # startup mode options
    #

    def set_special(self, special):
        """
        @param special: special mode (none, 'upgrade' or 'maintenance')
        """
        opt = {None:None, 'upgrade':'-U', 'maintenance':'-m'}[special]
        if opt: self.append(opt)
        return self

    def set_utility(self, utility):
        """
        @param utility: true if starting in utility mode
        """
        if utility: self.append("-c gp_role=utility")
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
     '-o', '"', '-p', '5432', '--gp_dbid=1', '--gp_num_contents_in_cluster=2', '--silent-mode=true', '"', 'start']
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
    def __init__(self, name, dataDir, port, dbid, standby_dbid, numContentsInCluster, era,
                 wrapper, wrapper_args, specialMode=None, restrictedMode=False, timeout=SEGMENT_TIMEOUT_DEFAULT,
                 max_connections=1, disableMasterMirror=False, utilityMode=False, ctxt=LOCAL, remoteHost=None,
                 wait=True
                 ):
        self.dataDir=dataDir
        self.port=port
        self.utilityMode=utilityMode
        self.wrapper=wrapper
        self.wrapper_args=wrapper_args

        # build backend options
        b = PgCtlBackendOptions(port, dbid, numContentsInCluster)
        b.set_master(standby_dbid, disableMasterMirror, seqserver=not utilityMode)
        b.set_utility(utilityMode)
        b.set_special(specialMode)
        b.set_restricted(restrictedMode, max_connections)

        # build pg_ctl command
        c = PgCtlStartArgs(dataDir, b, era, wrapper, wrapper_args, wait, timeout)
        self.cmdStr = str(c)

        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

    @staticmethod
    def local(name, dataDir, port, dbid, standbydbid, numContentsInCluster, era,
              wrapper, wrapper_args, specialMode=None, restrictedMode=False, timeout=SEGMENT_TIMEOUT_DEFAULT,
              max_connections=1, disableMasterMirror=False, utilityMode=False):
        cmd=MasterStart(name, dataDir, port, dbid, standbydbid, numContentsInCluster, era,
                        wrapper, wrapper_args, specialMode, restrictedMode, timeout,
                        max_connections, disableMasterMirror, utilityMode)
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
                 noWait=False, timeout=SEGMENT_TIMEOUT_DEFAULT,
                 specialMode=None, wrapper=None, wrapper_args=None):

        # This is referenced from calling code
        self.segment = gpdb

        # Interesting data from our input segment
        dbid    = gpdb.getSegmentDbId()
        content = gpdb.getSegmentContentId()
        port    = gpdb.getSegmentPort()
        datadir = gpdb.getSegmentDataDirectory()
        role    = gpdb.getSegmentRole()

        # build backend options
        b = PgCtlBackendOptions(port, dbid, numContentsInCluster)
        b.set_segment(mirrormode, content)
        b.set_utility(utilityMode)
        b.set_special(specialMode)

        # mirror will be in recovery mode so pg_ctl -w flag won't work
        if role == gparray.ROLE_MIRROR:
            noWait = True

        # build pg_ctl command
        c = PgCtlStartArgs(datadir, b, era, wrapper, wrapper_args, not noWait, timeout)
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
class SendFilerepTransitionMessage(Command):

    # see gpmirrortransition.c and primary_mirror_transition_client.h
    TRANSITION_ERRCODE_SUCCESS                             = 0
    TRANSITION_ERRCODE_ERROR_UNSPECIFIED                   = 1
    TRANSITION_ERRCODE_ERROR_SERVER_DID_NOT_RETURN_DATA    = 10
    TRANSITION_ERRCODE_ERROR_PROTOCOL_VIOLATED             = 11
    TRANSITION_ERRCODE_ERROR_HOST_LOOKUP_FAILED            = 12
    TRANSITION_ERRCODE_ERROR_INVALID_ARGUMENT              = 13
    TRANSITION_ERRCODE_ERROR_READING_INPUT                 = 14
    TRANSITION_ERRCODE_ERROR_SOCKET                        = 15

    #
    # note: this should be cleaned up -- there are two hosts involved,
    #   the host on which to run gp_primarymirror, AND the host to pass to gp_primarymirror -h
    #
    # Right now, it uses the same for both which is pretty wrong for anything but a local context.
    #
    def __init__(self, name, inputFile, port=None,ctxt=LOCAL, remoteHost=None, dataDir=None):
        if not remoteHost:
            remoteHost = "localhost"
        self.cmdStr='$GPHOME/bin/gp_primarymirror -h %s -p %s -i %s' % (remoteHost,port,inputFile)
        self.dataDir = dataDir
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)

    @staticmethod
    def local(name,inputFile,port=None,remoteHost=None):
        cmd=SendFilerepTransitionMessage(name, inputFile, port, LOCAL, remoteHost)
        cmd.run(validateAfter=True)
        return cmd

    @staticmethod
    def buildTransitionMessageCommand(transitionData, dir, port):
        dbData = transitionData["dbsByPort"][int(port)]
        targetMode = dbData["targetMode"]

        argsArr = []
        argsArr.append(targetMode)
        if targetMode == 'mirror' or targetMode == 'primary':
            mode = dbData["mode"]
            if mode == 'r' and dbData["fullResyncFlag"]:
                # full resync requested, convert 'r' to 'f'
                argsArr.append( 'f' )
            else:
                # otherwise, pass the mode through
                argsArr.append( dbData["mode"])
            argsArr.append( dbData["hostName"])
            argsArr.append( "%d" % dbData["hostPort"])
            argsArr.append( dbData["peerName"])
            argsArr.append( "%d" % dbData["peerPort"])
            argsArr.append( "%d" % dbData["peerPMPort"])

        #
        # write arguments to input file.  We will leave this file around.  It can be useful for debugging
        #
        inputFile = os.path.join( dir, "gp_pmtransition_args" )
        writeLinesToFile(inputFile, argsArr)

        return SendFilerepTransitionMessage("Changing seg at dir %s" % dir, inputFile, port=port, dataDir=dir)

class SendFilerepTransitionStatusMessage(Command):
    def __init__(self, name, msg, dataDir=None, port=None,ctxt=LOCAL, remoteHost=None):
        if not remoteHost:
            remoteHost = "localhost"
        self.cmdStr='$GPHOME/bin/gp_primarymirror -h %s -p %s' % (remoteHost,port)
        self.dataDir = dataDir

        logger.debug("Sending msg %s and cmdStr %s" % (msg, self.cmdStr))

        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost, stdin=msg)

    def unpackSuccessLine(self):
        """
        After run() has been called on this cmd, call this to find the "Success" data in the output

        That line is returned if successful, otherwise None is returned
        """
        res = self.get_results()
        if res.rc != 0:
            logger.warn("Error getting data stdout:\"%s\"  stderr:\"%s\"" % \
                        (res.stdout.replace("\n", " "), res.stderr.replace("\n", " ")))
            return None
        else:
            logger.info("Result: stdout:\"%s\"  stderr:\"%s\"" % \
                        (res.stdout.replace("\n", " "), res.stderr.replace("\n", " ")))

            line = res.stderr
            if line.startswith("Success:"):
                line = line[len("Success:"):]
            return line

#-----------------------------------------------
class SendFilerepVerifyMessage(Command):

    DEFAULT_IGNORE_FILES = [
        'pg_internal.init', 'pgstat.stat', 'pga_hba.conf',
        'pg_ident.conf', 'pg_fsm.cache', 'gp_dbid', 'gp_pmtransitions_args',
        'gp_dump', 'postgresql.conf', 'postmaster.log', 'postmaster.opts',
        'postmaser.pids', 'postgresql.conf.bak', 'core',  'wet_execute.tbl',
        'recovery.done', 'gp_temporary_files_filespace', 'gp_transaction_files_filespace']

    DEFAULT_IGNORE_DIRS = [
        'pgsql_tmp', 'pg_xlog', 'pg_log', 'pg_stat_tmp', 'pg_changetracking', 'pg_verify', 'db_dumps', 'pg_utilitymodedtmredo', 'gpperfmon'
    ]

    def __init__(self, name, host, port, token, full=None, verify_file=None, verify_dir=None,
                 abort=None, suspend=None, resume=None, ignore_dir=None, ignore_file=None,
                 results=None, results_level=None, ctxt=LOCAL, remoteHost=None):
        """
        Sends gp_verify message to backend to either start or get results of a
        mirror verification.
        """

        self.host = host
        self.port = port

        msg_contents = ['gp_verify']

        ## The ordering of the following appends is critical.  Do not rearrange without
        ## an associated change in gp_primarymirror

        # full
        msg_contents.append('true') if full else msg_contents.append('')
        # verify_file
        msg_contents.append(verify_file) if verify_file else msg_contents.append('')
        # verify_dir
        msg_contents.append(verify_dir) if verify_dir else msg_contents.append('')
        # token
        msg_contents.append(token)
        # abort
        msg_contents.append('true') if abort else msg_contents.append('')
        # suspend
        msg_contents.append('true') if suspend else msg_contents.append('')
        # resume
        msg_contents.append('true') if resume else msg_contents.append('')
        # ignore_directory
        ignore_dir_list = SendFilerepVerifyMessage.DEFAULT_IGNORE_DIRS + (ignore_dir.split(',') if ignore_dir else [])
        msg_contents.append(','.join(ignore_dir_list))
        # ignore_file
        ignore_file_list = SendFilerepVerifyMessage.DEFAULT_IGNORE_FILES + (ignore_file.split(',') if ignore_file else [])
        msg_contents.append(','.join(ignore_file_list))
        # resultslevel
        msg_contents.append(str(results_level)) if results_level else msg_contents.append('')

        logger.debug("gp_verify message sent to %s:%s:\n%s" % (host, port, "\n".join(msg_contents)))

        self.cmdStr='$GPHOME/bin/gp_primarymirror -h %s -p %s' % (host, port)
        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost, stdin="\n".join(msg_contents))


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
SEGSTART_ERROR_MIRRORING_FAILURE = 1
SEGSTART_ERROR_POSTMASTER_DIED = 2
SEGSTART_ERROR_INVALID_STATE_TRANSITION = 3
SEGSTART_ERROR_SERVER_IS_IN_SHUTDOWN = 4
SEGSTART_ERROR_STOP_RUNNING_SEGMENT_FAILED = 5
SEGSTART_ERROR_DATA_DIRECTORY_DOES_NOT_EXIST = 6
SEGSTART_ERROR_SERVER_DID_NOT_RESPOND = 7
SEGSTART_ERROR_PG_CTL_FAILED = 8
SEGSTART_ERROR_CHECKING_CONNECTION_AND_LOCALE_FAILED = 9
SEGSTART_ERROR_PING_FAILED = 10 # not actually done inside GpSegStartCmd, done instead by caller
SEGSTART_ERROR_OTHER = 1000


class GpSegStartArgs(CmdArgs):
    """
    Examples
    --------

    >>> str(GpSegStartArgs('en_US.utf-8:en_US.utf-8:en_US.utf-8', 'mirrorless', 'gpversion', 1, 123, 600))
    "$GPHOME/sbin/gpsegstart.py -M mirrorless -V 'gpversion' -n 1 --era 123 -t 600"
    """

    def __init__(self, mirrormode, gpversion, num_cids, era, timeout):
        """
        @param mirrormode - mirror start mode (START_AS_PRIMARY_OR_MIRROR or START_AS_MIRRORLESS)
        @param gpversion - version (from postgres --gp-version)
        @param num_cids - number content ids
        @param era - master era
        @param timeout - seconds to wait before giving up
        """
        CmdArgs.__init__(self, [
            "$GPHOME/sbin/gpsegstart.py",
            "-M", str(mirrormode),
            "-V '%s'" % gpversion,
            "-n", str(num_cids),
            "--era", str(era),
            "-t", str(timeout)
        ])

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



class GpSegStartCmd(Command):
    def __init__(self, name, gphome, segments, gpversion,
                 mirrormode, numContentsInCluster, era,
                 timeout=SEGMENT_TIMEOUT_DEFAULT, verbose=False,
                 ctxt=LOCAL, remoteHost=None, pickledTransitionData=None,
                 specialMode=None, wrapper=None, wrapper_args=None,
                 logfileDirectory=False):

        # Referenced by calling code (in operations/startSegments.py), create a clone
        self.dblist = [x for x in segments]

        # build gpsegstart command string
        c = GpSegStartArgs(mirrormode, gpversion, numContentsInCluster, era, timeout)
        c.set_verbose(verbose)
        c.set_special(specialMode)
        c.set_transition(pickledTransitionData)
        c.set_wrapper(wrapper, wrapper_args)
        c.set_segments(segments)

        cmdStr = str(c)
        logger.debug(cmdStr)

        if (logfileDirectory):
            cmdStr = cmdStr + " -l '" + logfileDirectory + "'"
        Command.__init__(self,name,cmdStr,ctxt,remoteHost)


class GpSegChangeMirrorModeCmd(Command):
    def __init__(self, name, gphome, gpversion, dbs, targetMode,
                 pickledParams, verbose=False, ctxt=LOCAL, remoteHost=None):
        self.gphome=gphome
        self.dblist=dbs
        self.dirlist=[]
        for db in dbs:
            datadir = db.getSegmentDataDirectory()
            port = db.getSegmentPort()
            self.dirlist.append(datadir + ':' + str(port))

        dirstr=" -D ".join(self.dirlist)
        if verbose:
            setverbose=" -v "
        else:
            setverbose=""

        cmdStr="$GPHOME/sbin/gpsegtoprimaryormirror.py %s -D %s -M %s -p %s -V '%s'" % \
                (setverbose,dirstr,targetMode,pickledParams,gpversion)

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

    def __init__(self, name, datadir, port, ncontents, ctxt=LOCAL,
                 remoteHost=None, dbid=None, era=None,
                 wrapper=None, wrapper_args=None):
        super(GpStandbyStart, self).__init__(
                name=name,
                dataDir=datadir,
                port=port,
                dbid=dbid,
                standby_dbid=0,
                numContentsInCluster=ncontents,
                era=era,
                wrapper=wrapper,
                wrapper_args=wrapper_args,
                disableMasterMirror=True,
                ctxt=ctxt,
                remoteHost=remoteHost,
                wait=False
                )

    @staticmethod
    def local(name, datadir, port, ncontents, dbid, era=None,
              wrapper=None, wrapper_args=None):
        cmd = GpStandbyStart(name, datadir, port, ncontents,
                             dbid=dbid, era=era,
                             wrapper=wrapper, wrapper_args=wrapper_args)
        cmd.run(validateAfter=True)
        return cmd

    @staticmethod
    def remote(name, host, datadir, port, ncontents, dbid, era=None,
               wrapper=None, wrapper_args=None):
        cmd = GpStandbyStart(name, datadir, port, ncontents, ctxt=REMOTE,
                             remoteHost=host, dbid=dbid, era=era,
                             wrapper=wrapper, wrapper_args=wrapper_args)
        cmd.run(validateAfter=True)
        return cmd

#-----------------------------------------------
class GpInitSystem(Command):
    def __init__(self,name,configFile,hostsFile, ctxt=LOCAL, remoteHost=None):
        self.configFile=configFile
        self.hostsFile=hostsFile
        self.cmdStr="$GPHOME/bin/gpinitsystem -a -c %s -h %s" % (configFile,hostsFile)
        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

#-----------------------------------------------
class GpDeleteSystem(Command):
    def __init__(self,name,datadir, ctxt=LOCAL, remoteHost=None):
        self.datadir=datadir
        self.input="y\ny\n"
        self.cmdStr="$GPHOME/bin/gpdeletesystem -d %s -f " % (datadir)
        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost,stdin=self.input)

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
    def __init__(self, name, masterOnly=False, verbose=False, quiet=False, restart=False, fast=False, force=False, datadir=None, ctxt=LOCAL, remoteHost=None, logfileDirectory=False):
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
        if logfileDirectory:
            self.cmdStr += " -l '" + logfileDirectory + "'"
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)

    @staticmethod
    def local(name,masterOnly=False, verbose=False, quiet=False,restart=False, fast=False, force=False, datadir=None):
        cmd=GpStop(name,masterOnly,verbose,quiet,restart,fast,force,datadir)
        cmd.run(validateAfter=True)
        return cmd

#-----------------------------------------------
class GpRecoverseg(Command):
    def __init__(self, name, ctxt=LOCAL, remoteHost=None):
        self.cmdStr = "$GPHOME/bin/gprecoverseg -a"
        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

#-----------------------------------------------
class Psql(Command):
    def __init__(self, name, query=None, filename=None, database='template1', port=None, utilityMode=False, ctxt=LOCAL, remoteHost=None):
        env = ''
        if utilityMode:
            env = 'PGOPTIONS="-c gp_session_role=utility"'
        cmdStr = '%s $GPHOME/bin/psql ' % env
        if port is not None:
            cmdStr += '-p %d ' % port
        if query is not None and filename is not None:
            raise Exception('Psql can accept only a query or a filename, not both.')
        elif query is not None:
            cmdStr += '-c "%s" ' % query
        elif filename is not None:
            cmdStr += '-f %s ' % filename
        else:
            raise Exception('Psql must be passed a query or a filename.')

        # shell escape and force double quote of database in case of any funny chars
        cmdStr += '"%s" ' % shellEscape(database)

        # Need to escape " for REMOTE or it'll interfere with ssh
        if ctxt == REMOTE:
             cmdStr = cmdStr.replace('"', '\\"')
        self.cmdStr=cmdStr
        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

#-----------------------------------------------
class ModifyPostgresqlConfSetting(Command):
    def __init__(self, name, file, optName, optVal, optType='string', ctxt=LOCAL, remoteHost=None):
        cmdStr = None
        if optType == 'number':
            cmdStr = "perl -p -i.bak -e 's/^%s[ ]*=[ ]*\\d+/%s=%d/' %s" % (optName, optName, optVal, file)
        elif optType == 'string':
            cmdStr = "perl -i -p -e \"s/^%s[ ]*=[ ]*'[^']*'/%s='%s'/\" %s" % (optName, optName, optVal, file)
        else:
            raise Exception, "Invalid optType for ModifyPostgresqlConfSetting"
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
class GpDumpDirsExist(Command):
    """
    Checks if gp_dump* directories exist in the given directory
    """
    def __init__(self, name, baseDir, ctxt=LOCAL, remoteHost=None):
        cmdStr = "find %s -name '*dump*' -print" % baseDir
        Command.__init__(self, name, cmdStr, ctxt, remoteHost)

    @staticmethod
    def local(name, baseDir):
        cmd = GpDumpDirsExist(name, baseDir)
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

    def __init__(self, name, confinfo, newSegments=False, tarFile=None,
                 batchSize=None, verbose=False,ctxt=LOCAL, remoteHost=None, validationOnly=False, writeGpIdFileOnly=False):
        cmdStr = '$GPHOME/bin/lib/gpconfigurenewsegment -c \"%s\"' % (confinfo)
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
                      [ : <filespace oid> : <file space directory> ]...

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

            filespaces = []
            for fsOid, path in seg.getSegmentFilespaces().iteritems():
                if fsOid not in [gparray.SYSTEM_FILESPACE]:
                    filespaces.append(str(fsOid) + ":" + path)

            result[hostname] += '%s:%d:%s:%s:%d%s' % (seg.getSegmentDataDirectory(), seg.getSegmentPort(),
                        "true" if seg.isSegmentPrimary(current_role=True) else "false",
                        "true" if isTargetReusedLocation else "false",
                        seg.getSegmentDbId(),
                        "" if len(filespaces) == 0 else (":" + ":".join(filespaces))
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
class GpAddConfigScript(Command):
    def __init__(self, name, directorystring, entry, value=None, removeonly=False, ctxt=LOCAL, remoteHost=None):
        cmdStr="echo '%s' | $GPHOME/sbin/gpaddconfig.py --entry %s" % (directorystring, entry)
        if value:
            # value will be encoded and unencoded in the script to protect against shell interpretation
            value = base64.urlsafe_b64encode(pickle.dumps(value))
            cmdStr = cmdStr + " --value '" + value + "'"
        if removeonly:
            cmdStr = cmdStr + " --removeonly "

        Command.__init__(self,name,cmdStr,ctxt,remoteHost)

#-----------------------------------------------
class GpAppendGucToFile(Command):

    # guc value will come in pickled and base64 encoded

    def __init__(self,name,file,guc,value,ctxt=LOCAL,remoteHost=None):
        unpickledText = pickle.loads(base64.urlsafe_b64decode(value))
        finalText = unpickledText.replace('"', '\\\"')
        cmdStr = 'echo "%s=%s" >> %s' %  (guc, finalText, file)
        Command.__init__(self,name,cmdStr,ctxt,remoteHost)


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
            scp_cmd=RemoteCopy("copy master",tarball,hostname,head)
            queue.addCommand(scp_cmd)
        queue.join()
        queue.check_results()
        logger.debug("distributeTarBall finished")




class GpError(Exception): pass

######
def get_user():
    username = os.environ.get('LOGNAME') or os.environ.get('USER')
    if not username:
        raise GpError('Environment Variable LOGNAME or USER not set')
    return username


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






#=-=-=-=-=-=-=-=-=-= Bash Migration Helper Functions =-=-=-=-=-=-=-=-

def start_standbymaster(host, datadir, port, dbid, ncontents, era=None,
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

    #create a pg_log directory if necessary
    CreateDirIfNecessary.remote('create standby logdir if needed', host, datadir + "/pg_log")


    cmd = GpStandbyStart.remote('start standby master',
                                host, datadir, port, ncontents, dbid, era=era,
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
        cmdStr = 'pgrep postgres | xargs -i pwdx {} | grep "%s" | grep "^%s:" | cat' % (datadir, pid)
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
    pass


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

    cmd=FileDirExists('check for /tmp/.s.PGSQL file file', "/tmp/.s.PGSQL.%d" % port)
    cmd.run(validateAfter=True)
    tmpfile_exists = cmd.filedir_exists()

    cmd=FileDirExists('check for lock file', get_lockfile_name(port))
    cmd.run(validateAfter=True)
    lockfile_exists = cmd.filedir_exists()

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


def pausePg(db):
    """
    This function will pause an instance of postgres which is part of a GPDB

    1) pause the postmaster (this prevents new connections from being made)
    2) get list of processes that are descendent from postmaster process
    3) pause all descendent processes and ignore any failures (a process may have died betwen getting pid list and doing the pauses)
    4) again, get list of processes that are descendent from postmaster process
    5) pause all descendent processes and ignore any failures (do not ignore errors, errors are failures, no pids can die between being paused the first time and getting the pid list)
    """

    datadir = db.getSegmentDataDirectory()
    content = db.getSegmentContentId()
    postmasterPID = read_postmaster_pidfile(datadir)
    if postmasterPID == 0:
        raise Exception, 'print "could not locate postmasterPID during pause'

    Kill.local(name="pausep "+str(content), pid=postmasterPID, signal="STOP")

    decsendentProcessPids = getDescendentProcesses(postmasterPID)

    for killpid in decsendentProcessPids:

        try:
            Kill.local(name="pausep "+str(killpid), pid=killpid, signal="STOP")
        except:
            pass

    decsendentProcessPids = getDescendentProcesses(postmasterPID)

    for killpid in decsendentProcessPids:

        Kill.local(name="pausep "+str(killpid), pid=killpid, signal="STOP")


def resumePg(db):
    """
    1) resume the processes descendent from the postmaster process
    2) resume the postmaster process
    """

    datadir = db.getSegmentDataDirectory()
    content = db.getSegmentContentId()
    postmasterPID = read_postmaster_pidfile(datadir)
    if postmasterPID == 0:
        raise Exception, 'print "could not locate postmasterPID during resume'

    decsendentProcessPids = getDescendentProcesses(postmasterPID)

    for killpid in decsendentProcessPids:

        Kill.local(name="pausep "+str(killpid), pid=killpid, signal="CONT")


    Kill.local(name="pausep "+str(content), pid=postmasterPID, signal="CONT")


def createTempDirectoryName(masterDataDirectory, tempDirPrefix):
    return '%s/%s_%s_%d' % (os.sep.join(os.path.normpath(masterDataDirectory).split(os.sep)[:-1]),
                                tempDirPrefix,
                                datetime.datetime.now().strftime('%m%d%Y'),
                                os.getpid())


#-------------------------------------------------------------------------
# gp_dbid methods moved to gp_dbid.py, but this class was left here
#

class GpCreateDBIdFile(Command):
    def __init__(self, name, directory, dbid, verbose=False, ctxt=LOCAL, remoteHost=None):
        if verbose:
            setverbose="-v"
        else:
            setverbose=""
        args = [
            "$GPHOME/sbin/gpsetdbid.py",
            "-d %s" % directory,
            "-i %s" % dbid,
            setverbose,
            ]
        cmdStr = " ".join(args)
        Command.__init__(self, name, cmdStr, ctxt, remoteHost)

    @staticmethod
    def local(name, directory, dbid):
        cmd = GpCreateDBIdFile(name, directory, dbid)
        cmd.run(validateAfter=True)

    @staticmethod
    def remote(name, remoteHost, directory, dbid):
        cmd = GpCreateDBIdFile(name, directory, dbid, ctxt=REMOTE, remoteHost=remoteHost)
        cmd.run(validateAfter=True)

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

class GpReadConfig(Command):
    def __init__(self, name, host, seg, guc_name, ctxt=LOCAL, remote_host=None):
        self.host = host
        self.seg_db_id = seg.getSegmentDbId()
        self.seg_content_id = seg.getSegmentContentId()
        self.guc_name = guc_name
        self.role = seg.getSegmentRole()
        cat_path = findCmdInPath('cat')

        cmdStr = "%s %s/postgresql.conf" % (cat_path, seg.getSegmentDataDirectory())
        Command.__init__(self, name, cmdStr, ctxt, remote_host)

    def get_guc_value(self):
        std_out = self.get_results().stdout
        std_out = std_out.split('\n')

        VALUE_PATTERN = re.compile(".*=(.*)")

        GUC_PATTERN = re.compile("^[\s]*" + self.guc_name + "[ \t]*=")

        value = None
        key_lines = [line for line in std_out if GUC_PATTERN.match(line)]
        if key_lines:
            value = VALUE_PATTERN.match(key_lines[-1]).group(1)
            value = value.split('#')[0].strip()

        return value

    def get_seg_content_id(self):
        return self.seg_content_id

    def get_seg_role(self):
        return self.role

    def get_seg_dbid(self):
        return self.seg_db_id


if __name__ == '__main__':

    import doctest
    doctest.testmod()
