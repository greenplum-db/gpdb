#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2013. All Rights Reserved. 
#
# Program for DD Boost Managed File Replication related operations.
# The program does not need GPDB running.

from datetime import datetime
import os
import re
import signal
import subprocess
import sys
from threading import Event, Thread, Timer
from decimal import Decimal
import time
try:
    from gppylib import gplog, gpsubprocess
    from gppylib.commands.base import Command
    from gppylib.db import dbconn
    from gppylib.gparray import GpArray
    from gppylib.gpparseopts import OptChecker, OptionGroup, OptParser
    from gppylib.mainUtils import (ProgramArgumentValidationException,
                                   simple_main,
                                   addStandardLoggingAndHelpOptions)
    from gppylib.operations import Operation
except ImportError, e:
    sys.exit("Cannot import modules. " +
             "Please check that you have sourced greenplum_path.sh. " +
             "Detail: " + str(e))

GPMFR_EXEC = "gpmfr"
DDBOOST_EXEC = os.environ["GPHOME"] + "/bin/gpddboost"
# Existence of the these config files implies local/remote Data Domain is
# configured correctly.
LOCAL_DDBOOST_CONFIG = os.environ["HOME"] + "/DDBOOST_CONFIG"
REMOTE_DDBOOST_CONFIG = os.environ["HOME"] + "/DDBOOST_MFR_CONFIG"

logger = gplog.get_default_logger()

yyyymmdd = "[12][0-9]{3}[01][0-9][0-3][0-9]"
hhmmss = "[012][0-9][0-5][0-9][0-5][0-9]"

def mfr_parser():
    parser = OptParser(option_class=OptChecker,
                       version="%prog version $Revision: #5 $",
                       description="Managed File Replication using DD Boost.")
    parser.set_usage("%prog OPERATION [--remote] [--master-port=NUMBER] " +
                     "[--skip-ping]")
    addStandardLoggingAndHelpOptions(parser, includeNonInteractiveOption=True)
    ddOpt = OptionGroup(parser, "Operations", description="Valid formats for " +
                        "TIMESTAMP argument are 'YYYY-Month-DD HH:MM:SS' and " +
                        "YYYYMMDDHHMMSS.  Keywords 'OLDEST' and 'LATEST' may " +
                        "also be used.")
    ddOpt.add_option("--list", dest="ls", action="store_true", default=False,
                     help="List backup sets available on a Data Domain.")
    ddOpt.add_option("--list-files", dest="lsfiles", action="store",
                     metavar="TIMESTAMP", default=None,
                     help="List files in a backup set on a Data Domain.")
    ddOpt.add_option("--replicate", dest="replicate", action="store",
                     metavar="TIMESTAMP", default=None,
                     help="Replicate backup set TIMESTAMP from local to " +
                     "remote Data Domain.")
    ddOpt.add_option("--recover", dest="recover", action="store", default=None,
                     metavar="TIMESTAMP", help="Recover backup set identified" +
                     "by TIMESTAMP from remote to local Data Domain.")
    ddOpt.add_option("--max-streams", dest="maxStreams", action="store",
                     metavar="NUMBER", default=None,
                     help="Maximum number of Data Domain I/O streams to be " +
                     "used for replication/recovery.")
    ddOpt.add_option("--delete", dest="delete", action="store", default=None,
                     metavar="TIMESTAMP", help="Delete a backup set on " +
                     "Data Domain.")
    ddOpt.add_option("--show-streams", dest="showStreams", action="store_true",
                     default=False, help="Show I/O streams used for DD Boost" +
                     " MFR on local and remote Data Domains.")
    ddOpt.add_option("--ddboost-storage-unit", dest="ddboost_storage_unit", action="store", default=None,
                     help="Storage unit name in Data Domain server.")
    parser.add_option_group(ddOpt)
    optOpt = OptionGroup(parser, "Optional arguments used by operations")
    optOpt.add_option("--remote", dest="remote", action="store_true",
                      default=False, help="Perform the operation on remote " +
                      "Data Domain.  Applicable to --list, --delete and " +
                      "--list-files operations only.  If omitted, the " +
                      "operation is performed on local Data Domain.")
    optOpt.add_option("--skip-ping", dest="ping", action="store_false",
                      default=True, help="Do not try to ping Data Domain " +
                      "before performing an operation.")
    optOpt.add_option("--master-port", dest="master_port", action="store",
                      metavar="NUMBER", default=None,
                      help="GPDB master port number.  Backup sets will be " +
                      "validated against this GPDB instance.  Not applicable" +
                      " to --show-streams operation.")
    parser.add_option_group(optOpt)
    parser.setHelp([])
    return parser


class BackupFile(object):
    """
    A file backed up on a DD system.
    """
    def __init__(self, name, mode, size):
        self.name = name
        self.mode = mode
        self.size = size

    def __str__(self):
        return "%-50s\t%s\t%ld" % (self.name, self.mode, self.size)

    def __eq__(self, other):
        return self.name == other.name and self.size == other.size

    def __lt__(self, other):
        return self.name < other.name or \
               (self.name == other.name and self.size < other.size)

    def __gt__(self, other):
        return self.name > other.name or \
               (self.name == other.name and self.size > other.size)

    def isDumpFile(self):
        """
        Returns True if the file is a database dump.  Config files, cdatabase
        files are not dump files.
        """
        metadata_keywords = ['rpt', 'ao_state_file', 'co_state_file', 'schema', \
                        'last_operation', 'dirty_list', 'table_list', 'increments']

        for i in metadata_keywords:
            if self.name.startswith("gp_dump") and self.name.endswith(i):
                return False

        return self.name.startswith("gp_dump") and "post_data" not in self.name

    def isConfigFile(self):
        """
        Returns True if the file is a master/segment config archive.
        """
        return self.name.startswith("gp_master_config_files") or \
            self.name.startswith("gp_segment_config_files")


class BackupTimestamp(object):
    """
    Users refer to a backup set using timestamp.  Three formats of timestamp are
    supported - 
        YYYYMMDDHHMMSS
        'YYYY-Month-DD HH:MM:SS'
        keywords: LATEST/OLDEST
    """
    prettyFormat = "%Y-%B-%d %H:%M:%S"
    ddFormat = "%Y%m%d%H%M%S"
    keywords = ("latest", "oldest")

    def __init__(self, tstring):
        if tstring.lower() in BackupTimestamp.keywords:
            self.keyword = tstring
            self.ddStr = None
            self.prettyStr = None
        else:
            self.keyword = None
            try:
                dt = datetime.strptime(tstring, BackupTimestamp.prettyFormat)
                self.prettyStr = tstring
                self.ddStr = dt.strftime(BackupTimestamp.ddFormat)
            except ValueError:
                dt = datetime.strptime(tstring, BackupTimestamp.ddFormat)
                self.ddStr = tstring
                self.prettyStr = dt.strftime(BackupTimestamp.prettyFormat)
            if re.match(yyyymmdd+hhmmss, self.ddStr) is None:
                msg = "Timestamp '%s' does not appear valid." % self.ddStr
                logger.error(msg)
                raise ValueError(msg)

    def __str__(self):
        if self.keyword:
            return self.keyword.lower()
        return "%s (%s)" % (self.prettyStr, self.ddStr)

    def isKeyword(self):
        return self.keyword is not None

    def isLatest(self):
        return self.keyword.lower() == BackupTimestamp.keywords[0]

    def isOldest(self):
        return self.keyword.lower() == BackupTimestamp.keywords[1]

class BackupSet(object):
    """
    A set backup files matching the same timestamp, present under the same
    directory on a DD system.
    """
    def __init__(self, bdate, btime):
        if re.match(yyyymmdd, bdate) is None or \
           re.match(hhmmss, btime) is None:
            raise Exception(
                "Invalid backup set identifier: '%s' '%s'" % (bdate, btime))
        self.backupDate = bdate
        self.backupTime = btime
        self.backupFiles = []
        self.isValid = None
        # Only for pretty printing purpose.
        self.bt = BackupTimestamp(bdate + btime)

    def __eq__(self, other):
        # TODO: It's super inefficient to copy and then sort the two lists.
        # Think of a better solution.
        lval = [f for f in self.backupFiles]
        rval = [f for f in other.backupFiles]
        lval.sort()
        rval.sort()
        return self.backupDate == other.backupDate and \
               self.backupTime == other.backupTime and \
               lval == rval

    def __lt__(self, other):
        return int(self.backupDate + self.backupTime) < \
               int(other.backupDate + other.backupTime)

    def __gt__(self, other):
        return int(self.backupDate + self.backupTime) > \
               int(other.backupDate + other.backupTime)

    def addFile(self, bfile):
        self.backupFiles.append(bfile)

    def prettyPrint(self, printFiles=False):
        bkpstr = "%s" % self.bt
        if self.isValid is not None and not self.isValid:
            bkpstr = bkpstr + " !" # Invalid backup indicator.
        if not printFiles:
            print "  ", bkpstr
        else:
            self.backupFiles.sort()
            print "  Backup Set:", bkpstr
            print "\t%-50s\t%s\t%s" % ("NAME", "MODE", "SIZE")
            print "  "+"-"*90
            for f in self.backupFiles:
                print "\t", f

    def validateWithGpdb(self, gparray):
        """
        Compare the number of database dump files and configuration files in the
        BackupSet with the number of dbs in GPDB.  gparray must be a valid
        GpArray instance.  Returns True if the numbers match.
        """
        dumpCount = 0
        configCount = 0
        for f in self.backupFiles:
            if f.isDumpFile():
                dumpCount = dumpCount + 1
            elif f.isConfigFile():
                configCount = configCount + 1
        # Backup sets that don't contain any dump and config files are
        # considered valid.  Only if the backup set contains non-zero dump
        # files, we correlate the number with the number of GPDB segments.  Same
        # applies to config files.
        dumpValid = (dumpCount <= 0 or
                     dumpCount == gparray.numPrimarySegments + 1)
        configValid = (configCount <= 0 or
                       configCount == gparray.numPrimarySegments + 1)
        self.isValid = (dumpValid and configValid)
        return self.isValid


class DDSystem(object):
    """
    A Data Domain appliance.  This class assumes that at the most two Data
    Domains need to be dealt with for MFR - local and remote, remote being
    optional.  Product Management confirmed that at least for first MFR release
    (Q1 2013), this assumption will hold.
    """
    # The error codes come from DD Boost SDK v2.4.
    errorCodes = {
        5004: "Backup object not found on DD system.",
        5005: "Not enough I/O streams available for file transfer.",
        5075: "Failed to connect to DD System, " + \
              "check DD Boost username/password.",
        5028: "Failed to connect to DD System, check hostname/IP address."
        }

    def __init__(self, id, backup_dir=None, dd_storage_unit=None):
        self.id = id
        if id == "local":
            self.argSuffix = ""
        elif id == "remote":
            # This will be appended to every gpddboost command line.
            self.argSuffix = " --remote"
        else:
            raise NotImplemented(
                "Only local and remote DD systems are supported currently.")
        self._checkConfigExists(id)

        self.DDBackupDir = backup_dir
        self.DDStorageUnit = dd_storage_unit

        self.hostname, default_backup_dir, default_storage_unit = self._readConfig()

        if not self.DDStorageUnit:
            self.DDStorageUnit = default_storage_unit

        if not self.DDBackupDir:
            self.DDBackupDir = default_backup_dir

        self._replSourceLimit = None
        self._replDestLimit = None

    def _checkConfigExists(self, id):
        """
        Raise exception if DD Boost config file does not exist.  Assume that
        gpddboost creates LockBox configuration files with particular names for
        storing local and remote Data Domain configuration.
        """
        testcmd = "test -e %s" % LOCAL_DDBOOST_CONFIG
        logger.debug("Checking config exists: %s" % testcmd)
        cmd = Command("Checking config exists", testcmd)
        cmd.run()
        if not cmd.was_successful():
            raise Exception("Local Data Domain configuration not found.")
        if id == "remote":
            testcmd = "test -e %s" % REMOTE_DDBOOST_CONFIG
            logger.debug("Checking config exists: %s" % testcmd)
            cmd = Command("Checking config exists", testcmd)
            cmd.run()
            if not cmd.was_successful():
                raise Exception("Remote Data Domain configuration not found.")

    def __str__(self):
        return "%s(%s)" % (self.id, self.hostname)

    def _parseError(self, lines):
        """
        Parses given list of lines from gpddboost console output and returns
        appropriate error code.
        """
        perr = re.compile(".*[eE]rr[^0-9]*([0-9]*)")
        for l in lines:
            m = perr.match(l)
            if m is not None:
                return int(m.group(1))
        return -1

    def printErrorAndAbort(self, code, msg=None):
        if code != -1:
            msg = "DD Boost error %d" % code
            if code in self.errorCodes:
                msg += ": " + self.errorCodes[code]
        elif msg is None:
            msg = "DD Boost: Unknown Error"
        # Simple main framwork prints the exception message on stdout.
        raise Exception(msg)

    def _runDDBoost(self, args):
        """
        Runs DDBoost command with specified arguments.  Returns a list of lines
        on console output.
        """
        cmdStr = DDBOOST_EXEC + " " + args.strip() + self.argSuffix
        ddCmd = Command("DD Boost on master", cmdStr)
        ddCmd.run()
        r = ddCmd.get_results()
        if r is not None and r.completed:
            lines = r.stdout.splitlines() + r.stderr.splitlines()
            if r.rc != 0:
                # This should be a warning but to mask it from stdout, it's
                # info.
                logger.info("DDBoost command %s returned error: %s" % \
                             (cmdStr, str(r)))
            return (r.rc, lines)
        # r.was_successful() is not used because gpddboost may complete with
        # non-zero exit code.  In this case, the caller may be interested in
        # error code printed on stderr/stdout.
        msg = "Failed to execute %s:\n" % cmdStr
        logger.error(msg + str(r))
        raise Exception(msg)

    def findItem(self, pattern, line):
        m = pattern.match(line)
        if m:
            return m.group(1)
        else:
            return None

    def _readConfig(self):
        """
        Run gpddboost to get default backup dir and DD hostname.
        """

        default_hostname = None
        default_backup_dir = None
        default_ddboost_storage_unit = None

        phost = re.compile("Data Domain Hostname:([^ \t]*)")
        pdir = re.compile("Default Backup Directory:([^ \t]*)")
        pstu = re.compile("Data Domain Storage Unit:([^ \t]*)")

        rc, lines = self._runDDBoost("--show-config")

        if rc != 0:
            code = self._parseError(lines)
            self.printErrorAndAbort(code)

        for l in lines:
            if not default_hostname:
                default_hostname = self.findItem(phost, l)
            if not default_backup_dir:
                default_backup_dir = self.findItem(pdir, l)
            if not default_ddboost_storage_unit:
                default_ddboost_storage_unit = self.findItem(pstu, l)

        if default_hostname is None or default_hostname == "":
            raise Exception("Failed to obtain Data Domain configuration.")

        if self.id == "remote":
            # For remote Data Domain, default backup dir is not shown by
            # gpddboost.  Hence we employ the following ugly trick to get
            # default directory from local Data Domain config.  TODO: Change
            # gpddboost to record and print default dir for remote.
            argSuffix = self.argSuffix
            self.argSuffix = ""
            rc, lines = self._runDDBoost("--show-config")
            self.argSuffix = argSuffix
            if rc != 0:
                code = self._parseError(lines)
                self.printErrorAndAbort(code)
            for l in lines:
                if not default_backup_dir:
                    default_backup_dir = self.findItem(pdir, l)
                if not default_ddboost_storage_unit:
                    default_ddboost_storage_unit = self.findItem(pstu, l)

        if default_backup_dir is None or default_backup_dir == "":
            raise Exception("Failed to obtain backup directory from Data Domain configuration.")

        if default_ddboost_storage_unit is None or default_ddboost_storage_unit == "":
            raise Exception("Failed to obtain storage unit from Data Domain configuration.")

        return default_hostname, default_backup_dir, default_ddboost_storage_unit

    def _ddBoostStreamCounts(self, regex):
        """
        The regex must have at least one group of digits.  E.g. "([0-9]*)".
        """
        args = "--get_stream_counts"
        rc, lines = self._runDDBoost(args)
        if rc != 0:
            code = self._parseError(lines)
            self.printErrorAndAbort(code)
        p = re.compile(regex)
        for line in lines:
            m = p.match(line)
            if m is not None:
                return int(m.group(1))
        raise Exception("gpddboost output did not match expected regex.\n" +
                        "Output:\n%s" % "\n".join(lines) + "\nRegex: " + regex)

    def listDir(self, path):
        """
        Returns None if path not found on DD system.  Otherwise returns listing
        of files/directories, one per line.
        """
        args = "--ls " + path + " --ddboost-storage-unit=" + self.DDStorageUnit
        rc, lines = self._runDDBoost(args)
        if rc != 0:
            code = self._parseError(lines)
            if 5004 == code:
                return None
            self.printErrorAndAbort(code)
        return lines

    def deleteFile(self, path):
        args = "--del-file " + path + " --ddboost-storage-unit=" + self.DDStorageUnit
        rc, lines = self._runDDBoost(args)
        if rc != 0:
            code = self._parseError(lines)
            self.printErrorAndAbort(code)
        return lines

    def replSourceSoftLimit(self):
        """
        The maximum number of replication source (read) streams this Data Domain
        supports.

        Note: One file transfer from local to remote Data Domain consumes one
        source stream on local and one destination stream on remote.  Data
        Domain calls this number 'soft limit'.  There is a much larger 'hard
        limit'.  But Data Domain recommends that soft limit should not be
        exceeded (ref: day-long meeting between Data Domain and Greenplum folks
        at Santa Clara on Feb 26, 2013).
        """
        if self._replSourceLimit:
            return self._replSourceLimit
        regex = "Replication Source Streams[ \t]*:[ \t]*([0-9]*)"
        self._replSourceLimit = self._ddBoostStreamCounts(regex)
        return self._replSourceLimit

    def replDestSoftLimit(self):
        """
        The maximum number of replication destination (write) streams this Data
        Domain supports.  See also: DDSystem.replicationSourceSoftLimit().
        """
        if self._replDestLimit is not None:
            return self._replDestLimit
        regex = "Replication Destination Streams[ \t]*:[ \t]*([0-9]*)"
        self._replDestLimit = self._ddBoostStreamCounts(regex)
        return self._replDestLimit

    def inUseMfrStreams(self):
        """
        Returns the number of I/O streams currently in use on this Data Domain
        by managed file replication applications.
        """
        regex = "Used Filecopy Streams[ \t]*:[ \t]*([0-9]*)"
        return self._ddBoostStreamCounts(regex)

    def pingTest(self):
        """
        This method is to cover for incorrect Data Domain hostname configured by
        user.  DD Boost connect API apparently takes 20 minutes to determine DD
        system is not reachable.  Hence we check that ourselves: MPP-19210 and
        MPP-19211.  We use ping and not ssh because we don't have access to
        password, which is stored by gpddboost in encrypted LockBox format.
        """
        pktcount = 5
        args = ["ping", "-c", str(pktcount), self.hostname]
        self.proc = gpsubprocess.Popen(args, stderr=subprocess.STDOUT,
                                       stdout=subprocess.PIPE)
        # Regex for percentage loss in ping output.
        pping = \
              re.compile("%d packets.*received[^0-9]*([0-9]*)[%%] packet loss" %
                         pktcount)
        output = self.proc.stdout.readline()
        loss = -1
        while output != "" and loss == -1:
            logger.debug("pingTest: %s" % output)
            m = pping.match(output)
            if m is not None:
                loss = int(m.group(1))
            output = self.proc.stdout.readline()
        self.proc.communicate()
        if loss == -1:
            logger.error("pingTest: unexpected output, returning 100% loss.")
            loss = 100
        return loss

    def verifyLogin(self):
        """
        "gpddboost --verify" connects to DD system using configured username and
        password.  gpddboost also creates storage unit on the DD system if one
        doesn't exist.
        """
        rc, lines = self._runDDBoost("--verify --ddboost-storage-unit %s" % self.DDStorageUnit)
        if rc != 0:
            logger.info("gpddboost --verify --ddboost-storage-unit %s: %s" % (self.DDStorageUnit, "\n".join(lines)))
            code = self._parseError(lines)
            self.printErrorAndAbort(code)


class MfrException(Exception):
    pass

class Scheduler(object):
    """
    Create, start, stop and monitor workers based on constraints such as max I/O
    streams per Data Domain.  Workers are Python threads that spawn gpddboost
    process to transfer a file from source to target Data Domain or delete a
    file from a Data Domain.
    """
    # Max number of concurrent DeleteWorkers to spawn.
    DELETE_BATCHSIZE = 50

    def __init__(self, bset, sourceDD, targetDD=None, quiet=False):
        self.backupSet = bset
        self.sourceDD = sourceDD
        self.targetDD = targetDD
        # ReplicateWorker objects are added to this list.
        self.replWorkers = []
        self.replCancelEv = Event()
        self.replAbortEv = Event()
        self.quiet = quiet

    def _printProgress(self, current, total, msg=None):
        """
        If msg is None, prints a progress bar based on values of current and
        total.  If msg is not None, instead of a progress bar, a message
        indicating progress is shown.  E.g. "10 out of 50 tasks completed..."
        """
        if current > total:
            raise Exception(
                "printProgress: invalid arguments: total < current.")
        if self.quiet:
            # No progress bars if --quiet is specified.
            return
        if msg:
            # Show a message indicating progress instead of a progress bar.
            # E.g. "10 out of 50 tasks complete ...".
            try:
                msg = msg % (current, total)
            except TypeError:
                raise MfrException(
                    "printProgress: invalid argument: msg='%s'." % msg)
            if current < total:
                msg += " ..."
            else:
                msg += ".   "
            sys.stdout.write("\r")
            sys.stdout.write(msg)
            sys.stdout.flush()
        else:
            if total == current:
                ratio = 1
            elif total > current:
                ratio = float(current)/float(total)
            percent = int(ratio * 100)
            progress = int(ratio * 80)
            sys.stdout.write("\r")
            sys.stdout.write("[%-80s] %d%%" % ("="*progress, percent))
            sys.stdout.flush()

    def _validateUserStreamLimit(self, userStreamLimit):
        logger.debug("Validating user specified stream limit: %d" %
                     userStreamLimit)
        sourceLimit = self.sourceDD.replSourceSoftLimit()
        targetLimit = self.targetDD.replDestSoftLimit()
        if userStreamLimit > sourceLimit:
            raise MfrException(
                "Stream limit specified is greater than soft limit on %s." %
                self.sourceDD)
        if userStreamLimit > targetLimit:
            raise MfrException(
                "Stream limit specified is greater than soft limit on %s." %
                self.targetDD)

    def _availableStreams(self):
        """
        Return the number of I/O streams currently available for MFR.  This is
        the minimum of I/O streams available on source and target Data Domains.
 
        Note: from the instant of querying in-use I/O streams until the instant
        at which we end up using an I/O stream for file transfer, other backup
        applications can come in and use I/O streams.  Data Domain have
        acknowledged this as a shortcoming in DD Boost API and we have to live
        with it for now.
        """
        sourceLimit = self.sourceDD.replSourceSoftLimit()
        targetLimit = self.targetDD.replDestSoftLimit()
        inUseSource = self.sourceDD.inUseMfrStreams()
        inUseTarget = self.targetDD.inUseMfrStreams()
        availableSource = sourceLimit - inUseSource
        availableTarget = targetLimit - inUseTarget
        return min(availableSource, availableTarget)

    def _startReplicators(self, count):
        """
        Start 'count' number of ReplicateWorkers, one per file in
        backupSet.backupFiles.  Already started ReplicateWorkers are held in
        self.replWorkers list.
        """
        logger.debug("Request to start %d ReplicateWorkers" % count)
        if count <= 0:
            return
        # Start index in backupFiles list.
        start = len(self.replWorkers)
        if start + count > len(self.backupSet.backupFiles):
            raise MfrException(
                "Cannot start %d workers with %d backup files." %
                (start + count, len(self.backupSet.backupFiles)))
        for bf in self.backupSet.backupFiles[start : start + count]:
            bf.fullPath = "/".join([self.sourceDD.DDBackupDir,
                                   self.backupSet.backupDate, bf.name])
            w = ReplicateWorker(bf, self.sourceDD, self.targetDD,
                                self.replCancelEv, self.replAbortEv)
            self.replWorkers.append(w)
            logger.debug("Starting ReplicateWorker: %s" % bf.name)
            w.start()

    def _logPerFileProgress(self, logAll=False):
        """
        --quiet option supresses progress bars on stdout.  In that case, the
        only option to convey replication progress is logs.
        """
        msg = ("Replication of file %s: %d out of %d bytes transferred.")
        for w in self.replWorkers:
            if logAll or not w.isFinished():
                logger.info(msg % (w.backupFile.fullPath,
                                   w.bytesSent, w.backupFile.size))

    def replicate(self, userStreamLimit):
        """
        Spawn one worker per backup file for transfer.  Periodically print
        transfer status.  Handle cancellation/SIGINT.  Adhere to the following
        limits at all times:

        1. Never exceed Data Domain soft-limit on the number of replication I/O
        streams.

        2. Never exceed user specified limit on the number of I/O streams used
        for replicating a GPDB backup set.
        """
        self._validateUserStreamLimit(userStreamLimit)
        goal = self._totalKBytes()
        workDone = 0 # Number of KBytes transferred.
        tenPercent = goal/10
        # Log progress when workDown becomes larger than this logStep.
        logStep = 0
        try:
            self._printProgress(workDone, goal)
            while workDone < goal:
                # New I/O streams needed for transferring remaining files.
                streamsNeeded = len(self.backupSet.backupFiles) - \
                    len(self.replWorkers)
                logger.debug("Streams needed: %d" % streamsNeeded)
                completedWorkers = sum(map(lambda w: int(w.isFinished()),
                                           self.replWorkers))
                streamsHeldByUs = len(self.replWorkers) - completedWorkers
                logger.debug("Streams held by us: %d" % streamsHeldByUs)
                if (streamsHeldByUs < userStreamLimit) and (streamsNeeded > 0):
                    # Checking for available streams is an expensive operation
                    # (involves forking of gpddboost).  The if condition above
                    # minimizes the checks.
                    streamsAvail = self._availableStreams()
                    logger.debug("Available I/O streams: %d" % streamsAvail)
                    if streamsAvail > 0:
                        # Number of new I/O streams that can be used by us.
                        canBeHeld = min(userStreamLimit, streamsAvail)
                        logger.debug("Streams that can be held by us: %d" %
                                     canBeHeld)
                        self._startReplicators(min(canBeHeld, streamsNeeded))
                    else:
                        logger.info("No I/O stream available for new workers.")
                if reduce(lambda x,y: x or y,
                          map(lambda w: w.isFailed(), self.replWorkers)):
                    raise MfrException("One or more file transfers failed.")
                workDone = self._transferedKBytes()
                logger.debug("workDone = %d" % workDone)
                # Log progress when --quiet is specified only when at least 10
                # percent of work is done.  Else we will end up bloating log
                # files (MPP-19539).  If --verbose is specified, we anyway log
                # the progress in ReplicateWorker.analyzeOutout().
                if self.quiet and not gplog.logging_is_verbose():
                    if workDone > logStep:
                        self._logPerFileProgress()
                        logStep = workDone + tenPercent
                self._printProgress(workDone, goal)
                time.sleep(0.5)
            if workDone != goal:
                msg = "Internal Error: workDone: %d, goal: %d workDone != goal"
                raise MfrException(msg % (workDone, goal))
            self._printProgress(workDone, goal)
            if self.quiet and not gplog.logging_is_verbose():
                self._logPerFileProgress(logAll=True)
        except KeyboardInterrupt:
            print "\nCancelling file transfers."
            self.replCancelEv.set()
        except Exception, e:
            logger.error(e.message + " Aborting all file transfers.")
            self.replAbortEv.set()
        if self.replCancelEv.isSet() or self.replAbortEv.isSet():
            self._cleanup()
        else:
            for w in self.replWorkers:
                w.join()
            print ("\nBackup '%s' transferred from %s to %s Data Domain." %
                        (self.backupSet.bt, self.sourceDD, self.targetDD))

    def delete(self):
        """
        Delete files in a backup set.  Spwan one DeleteWorker per backup file.
        """
        self._deleteFiles(self.backupSet.backupFiles, self.sourceDD)

    def _totalKBytes(self):
        totalBytes = sum(long(file.size) for file in self.backupSet.backupFiles)
        return Decimal(totalBytes) / 1000

    def _transferedKBytes(self):
        bytesSent = sum(long(worker.bytesSent) for worker in self.replWorkers)
        return Decimal(bytesSent) / 1000

    def _cleanup(self):
        """
        Delete backup files from target DD in the event of cancel/abort.
        """
        for w in self.replWorkers:
            w.join()
        print "All active file transfers terminated."
        files = [w.backupFile
                 for w in self.replWorkers if w.cleanupNeeded()]
        self._deleteFiles(files, self.targetDD)

    def _deleteFiles(self, files, ddSystem):
        """
        Delete each BackupFile in files from ddSystem concurrently.  Delete at
        the most DELETE_BATCHSIZE number of files at a time.
        """
        batchSize = Scheduler.DELETE_BATCHSIZE
        workDone = 0
        goal = len(files)
        logger.debug("Deleting %d files, %d at a time." % (goal, batchSize))
        while workDone < goal:
            batch = files[workDone : workDone + batchSize]
            deleteWorkers = []
            for f in batch:
                f.fullPath = "/".join([ddSystem.DDBackupDir,
                                      self.backupSet.backupDate, f.name])
                dw = DeleteWorker(f, ddSystem)
                dw.start()
                deleteWorkers.append(dw)
            msg = "Deleting files from %s Data Domain: " % ddSystem
            msg += "%d out of %d files deleted"
            self._printProgress(workDone, goal, msg)
            for w in deleteWorkers:
                w.join()
                if w.isFailed():
                    print
                    msg = "Failed to delete backup file: %s." % w.backupFile
                    if w.errorMessage:
                        logger.error(msg)
                        ddSystem.printErrorAndAbort(w.errorMessage)
                    else:
                        raise Exception(msg)
                workDone = workDone + 1
                self._printProgress(workDone, goal, msg)
        # So that subsequent messages don't appear on the same line as progress
        # message/bar.
        print


class ReplicateWorker(Thread):
    """
    Replication of a single file from source DD system to target DD system
    happens here.  A new gpddboost process is forked for the file transfer.
    User cancellation request (SIGINT) handled by looking for specific pattern
    in gpddboost output.  SIGINT is automatically propagated to child gpddboost
    process, courtesy Linux kernel.
    """
    # States of a worker:
    INIT = 0                # About to start DDBoost as a child process.
    DDSTARTED = 1           # DDBoost started, yet to get transfer status.
    TRANSFER_INPROGRESS = 2 # Transfer status is being received on stdout.
    TRANSFER_COMPLETE = 4   # All bytes transfered.
    TRANSFER_INCOMPLETE = 5 # Unexpected failure in gpddboot.
    TRANSFER_CANCELED = 6   # gpddboost received SIGINT and canceled the
                            # transfer.  Partially transferred file was deleted
                            # from target DD.

    def __init__(self, backupFile, sourceDD, targetDD, cancelEv, abortEv):
        Thread.__init__(self, group=None, target=None,
                        name=None, args=(), kwargs={})
        self.backupFile = backupFile
        self.sourceDD = sourceDD
        self.targetDD = targetDD
        self.cancelEvent = cancelEv
        self.abortEvent = abortEv
        self.state = self.INIT
        self.bytesSent = 0

    def _ddBoostCmd(self):
        # TODO: This method needs to change in Phase II.
        cmd = DDBOOST_EXEC
        ddboost_storage_unit = None
        if self.sourceDD.id == "local" and self.targetDD.id == "remote":
            cmd = cmd + " --replicate"
            ddboost_storage_unit = self.sourceDD.DDStorageUnit
        elif self.sourceDD.id == "remote" and self.targetDD.id == "local":
            cmd = cmd + " --recover"
            ddboost_storage_unit = self.targetDD.DDStorageUnit
        else:
            msg = "Invalid DD system pair: source = %s target = %s" % \
                  (self.sourceDD, self.targetDD)
            logger.error(msg)
            raise Exception(msg)
        cmd = cmd + " --from-file " + self.backupFile.fullPath + \
                  " --to-file " + self.backupFile.fullPath + \
                  " --ddboost-storage-unit " + ddboost_storage_unit

        return cmd

    def isInProgress(self):
        return self.state in (ReplicateWorker.INIT,
                              ReplicateWorker.DDSTARTED,
                              ReplicateWorker.TRANSFER_INPROGRESS)

    def isFinished(self):
        """
        Return True if the worker has completed the file transfer successfully.
        """
        return self.state == ReplicateWorker.TRANSFER_COMPLETE

    def isFailed(self):
        """
        Worker may fail if the gpddboost child process returns error of fails to
        run.
        """
        return self.state == ReplicateWorker.TRANSFER_INCOMPLETE

    def isCanceled(self):
        """
        In the event of SIGINT or a worker failing, all other workers cancel
        their file transfers.
        """
        return self.state == ReplicateWorker.TRANSFER_CANCELED

    def cleanupNeeded(self):
        """
        Return True if the file on target DD needs to be cleaned up.  Workers in
        TRANSFER_CANCELED state would have deleted file they created on target
        through gpddboost's SIGINT handling.  Workers that had not started the
        transfer when cancel/abort event was set, would also end up in
        TRANSFER_CANCELED state.  Cleanup is needed only in the following cases.

        1. gpddboost command failed (to cleanup).
        2. backup file was completely transfered before cancel/abort event was
        set.
        """
        return self.state in (ReplicateWorker.TRANSFER_INCOMPLETE,
                              ReplicateWorker.TRANSFER_COMPLETE)

    def analyzeOutput(self, output):
        """
        Look for transfer progress or error codes in output.  Change state
        accordingly.  output is the text dumped by gpddboost on console.  Each
        line in output is matched against three patterns in sequence:

        1. ptransfer - extracts bytes transferred value from a line that
        matches.

        2. pcanceled - in case gpddboost received SIGINT, it dumps cancellation
        status on console that matches this pattern.

        3. perr - if gpddboost fails, this pattern extracts the DD Boost error
        code from the console error message.
        """
        if output == "" or output is None:
            return
        logger.debug(self.backupFile.name +": "+ output)
        perr = re.compile(".*[eE]rr[^0-9]*([0-9]*)")
        reg = ".*Replication.*%s completed [0-9.]* percent[,][ ]([0-9]*) bytes"
        ptransfer = re.compile(reg % self.backupFile.name)
        pcanceled = re.compile(".*File copy on ddboost canceled.*" + \
                               self.backupFile.name)
        for line in output.splitlines():
            if "" == line or "None" == line:
                continue
            m = ptransfer.match(line)
            if m is not None:
                try:
                    bytesSent = long(m.group(1))
                    if bytesSent < self.backupFile.size:
                        newState = self.TRANSFER_INPROGRESS
                    elif bytesSent == self.backupFile.size:
                        newState = self.TRANSFER_COMPLETE
                    else:
                        raise ValueError
                    msg = "Current state: " + str(self.state) + \
                        ", new state: " + str(newState) + \
                        ", file: " + self.backupFile.name
                    logger.debug(msg)
                    self.bytesSent = bytesSent
                    self.state = newState
                except ValueError:
                    msg = "gpddboost transfer status not recognized: " + \
                          line + "\n\tFile: " + self.backupFile.name + \
                          ", state: " + str(self.state)
                    logger.error(msg)
            else:
                m = pcanceled.match(line)
                if m is not None:
                    msg = "Current state: " + str(self.state) + \
                        ", new state: " + str(self.TRANSFER_CANCELED) + \
                        ", file: " + self.backupFile.name
                    logger.debug(msg)
                    self.state = self.TRANSFER_CANCELED
                else:
                    m = perr.match(line)
                    if m is not None:
                        msg = "Error in gpddboost: " + line + "\n\tFile: " + \
                            self.backupFile.name + ", current state: " + \
                            str(self.state) + ", new state: " + \
                            str(self.TRANSFER_INCOMPLETE)
                        logger.error(msg)
                        self.state = self.TRANSFER_INCOMPLETE
                    else:
                        msg = "Unrecognized output: '" + line + "'\n\tFile: "+ \
                              self.backupFile.name + ", state: " + \
                              str(self.state)
                        # This should be a warning but we don't want gplog to
                        # dump this on stdout and clobber our progressbars!
                        logger.debug(msg)

    def run(self):
        """
        Fork gpddboost process and start analyzing each line at its output.
        This will block until gpddboost exits, even when cancelEvent is set.
        Which is OK because gpddboost would have caught the SIGINT and would be
        handling it.

        A timer is started after catching SIGINT from user or upon exception in
        another thread.  If gpddboost does not exit even after the timer
        expires, kill gpddboost by sending SIGTERM.  We have seen gpddboost
        handle SIGINT quite reliably though.
        """
        try:
            args = self._ddBoostCmd().split()
            logger.debug("Forking gpddboost as: %s" % str(args))
            if self.cancelEvent.isSet() or self.abortEvent.isSet():
                self.state = self.TRANSFER_CANCELED
                return
            self.proc = gpsubprocess.Popen(args, stderr=subprocess.STDOUT,
                                           stdout=subprocess.PIPE)
            logger.info("gpddboost pid %d started." % self.proc.pid +
                        " Backup file: %s" % self.backupFile.name)
            self.state = self.DDSTARTED
            # Consume all output that is available from gpddboost.
            eof = 0
            while not eof and not self.cancelEvent.isSet() and \
                    not self.abortEvent.isSet():
                output = self.proc.stdout.readline()
                if output == "":
                    eof = 1
                else:
                    # Transition self.state and update bytes transferred.
                    self.analyzeOutput(output)
            if self.cancelEvent.isSet() or self.abortEvent.isSet():
                # Function called by Timer after timeout occurs.  It termiates
                # gpddboost by SIGTERM if gpddboost fails to terminate on its
                # own.
                def __childKiller():
                    try:
                        self.proc.terminate()
                        logger.info("killTimer: gpddboost pid %d terminated." %
                                    self.proc.pid)
                    except OSError:
                        # If proc is already terminated, OSError is thrown.
                        pass
                # Unit tests have shown that gpddboost takes 5 to 10 seconds to
                # handle Ctrl-C/SIGINT.  Timeout of 20 seconds is thus good.
                killTimer = Timer(20, __childKiller)
                logger.info("gpddboost pid %d: starting kill timer." %
                            self.proc.pid)
                killTimer.start()
                if self.abortEvent.isSet():
                    # Notify gpddboost to cancel the file transfer as it's time
                    # to abort.
                    self.proc.send_signal(signal.SIGINT)
                    logger.info("Sent SIGINT to gpddboost pid %d." %
                                self.proc.pid)
                else:
                    logger.info("Waiting for gpddboost pid %d" % self.proc.pid +
                                " to handle SIGINT.")
            # This blocks until gpddboost terminates.
            out, err = self.proc.communicate()
            logger.debug("gpddboost pid %d finished: %s" %
                         (self.proc.pid, str(args)))
            if (self.cancelEvent.isSet() or self.abortEvent.isSet()) and \
                    killTimer.is_alive():
                logger.info("gpddboost pid %d: canceling kill timer.")
                killTimer.cancel()
            # In case last few lines of gpddboost output were not analyzed.
            self.analyzeOutput(str(out) + str(err))
            logger.info("Finishing replicate worker, backup file: " +
                        self.backupFile.name + ", state: %d" % self.state)
        except Exception, e:
            logger.error("Transfer failed for backup file " +
                         self.backupFile.name + "\nReason: " + str(e))
            self.state = self.TRANSFER_INCOMPLETE


class DeleteWorker(Thread):
    """
    Delete a backup file from a Data Domain.
    """
    def __init__(self, backupFile, ddSystem):
        Thread.__init__(self, group=None, target=None,
                        name=None, args=(), kwargs={})
        self.backupFile = backupFile
        self.ddSystem = ddSystem
        self.errorMessage = None

    def run(self):
        try:
            self.ddSystem.deleteFile(self.backupFile.fullPath)
        except Exception, e:
            self.errorMessage = \
                "Delete failed for backup file %s on Data Domain %s. " % \
                (self.backupFile.fullPath, self.ddSystem)
            self.errorMessage += "Reason: %s" % e
            logger.error(self.errorMessage)

    def isFailed(self):
        return self.errorMessage is not None


class GpMfr(Operation):
    """
    Class conforming to simple_main framework.  Implements MFR
    specific operations such as listing of backups, triggering MFR for
    GPDB from source DD system to a remote DD system, recovering from
    remote DD system, checking status of an ongoing replication, etc.
    """
    def __init__(self, options, args):
        self.options = options
        self.timestamp = None    # Set by validateCmdline().
        self.validateCmdline(options, args)
        self.gparray = None
        self._setLogLevel()

    def _setLogLevel(self):
        """
        gplog dumps logs on stdout.  In order to show progress bars correctly,
        we don't want these logs on stdout.  If this class is used by other
        Python modules (e.g. gpcrondump), we must provide a way to restore
        log level after we are done.
        """
        self.originalStdoutLog = gplog._SOUT_HANDLER.level
        self.originalLogLevel = gplog._LOGGER.getEffectiveLevel()
        gplog.quiet_stdout_logging()

    def restoreLogLevel(self):
        """
        Restore log levels of the parent script which imported this class.
        """
        gplog._SOUT_HANDLER.setLevel(self.originalStdoutLog)
        gplog._LOGGER.setLevel(self.originalLogLevel)

    def validateCmdline(self, options, args):
        if args:
            msg = "Invalid argument(s): %s" % str(args)
            raise ProgramArgumentValidationException(msg, shouldPrintHelp=True)
        timestamp = None
        opcount = 0
        if options.ls:
            opcount = opcount + 1
        if options.lsfiles:
            opcount = opcount + 1
            timestamp = options.lsfiles
        if options.replicate:
            opcount = opcount + 1
            timestamp = options.replicate
        if options.recover:
            opcount = opcount + 1
            timestamp = options.recover
        if options.delete:
            opcount = opcount + 1
            timestamp = options.delete
        if options.showStreams:
            opcount = opcount + 1
        if opcount != 1:
            msg = "Exactly one of --list, --list-files, --replicate, " + \
                  "--recover, --delete should be specified."
            raise ProgramArgumentValidationException(msg, shouldPrintHelp=True)
        try:
            if timestamp is not None:
                self.timestamp = BackupTimestamp(timestamp)
        except:
            msg = "Invalid timestamp '%s'\nTimestamp should be one of:" + \
                "\n\tYYYY-Month-DD HH:MM:SS\n\tYYYYMMDD\n\tLATEST/OLDEST\n"
            raise ProgramArgumentValidationException(msg % timestamp)
        if options.replicate or options.recover:
            if options.maxStreams is None:
                msg = "--max-streams must be specified for " + \
                    "replication/recovery."
                raise ProgramArgumentValidationException(msg)
            try:
                if int(options.maxStreams) <= 0:
                    raise ValueError()
            except ValueError:
                msg = "--max-streams must be a number greater than zero."
                raise ProgramArgumentValidationException(msg)
        elif options.maxStreams is not None:
            msg = "--max-streams must be used with --replicate/--recover."
            raise ProgramArgumentValidationException(msg)
        if options.master_port:
            if options.showStreams:
                msg = "--master-port cannot be used with --show-streams."
                raise ProgramArgumentValidationException(msg)
            try:
                if int(options.master_port) <= 0:
                    raise ValueError()
            except ValueError:
                msg = "--master-port must be a number greater than zero."
                raise ProgramArgumentValidationException(msg)
        if options.remote:
            if options.replicate or options.recover:
                msg = "--remote cannot be used with --replicate/--recover."
                raise ProgramArgumentValidationException(msg)
            if options.showStreams:
                msg = "--remote cannot be used with --show-streams."
                raise ProgramArgumentValidationException(msg)

    def checkDDReachable(self, ddSystem):
        """
        Raise exception if ping test fails else continue.
        """
        loss = ddSystem.pingTest()
        # Non zero packet loss
        if loss > 0:
            raise Exception("%s Data Domain not reachable." % ddSystem)

    def execute(self):
        if self.options.remote:
            self.ddSystem = DDSystem("remote", dd_storage_unit=self.options.ddboost_storage_unit)
        else:
            self.ddSystem = DDSystem("local", dd_storage_unit=self.options.ddboost_storage_unit)
        self.defaultDir = self.ddSystem.DDBackupDir
        if self.options.ping:
            self.checkDDReachable(self.ddSystem)
        if not self.options.showStreams:
            port = self.options.master_port or os.environ.get("PORT", 5432)
            port = int(port)
            try:
                self.gparray = GpArray.initFromCatalog(dbconn.DbURL(port=port))
            except:
                logger.warn("Could not connect to GPDB, port %d. " % port +
                            "Skipping validation of backup sets.")
                self.gparray = None
        if self.options.ls:
            self.listBackups()
        elif self.options.lsfiles:
            self.listBackupFiles(self.timestamp)
        elif self.options.delete:
            self.deleteBackup(self.timestamp)
        elif self.options.showStreams:
            localDD = self.ddSystem
            remoteDD = DDSystem("remote", dd_storage_unit=self.options.ddboost_storage_unit)
            if self.options.ping:
                self.checkDDReachable(remoteDD)
            self.showDDBoostIOStreams(localDD, remoteDD)
        elif self.options.replicate:
            sourceDD = self.ddSystem
            targetDD = DDSystem("remote", dd_storage_unit=self.options.ddboost_storage_unit)
            if self.options.ping:
                self.checkDDReachable(targetDD)
            self.replicateBackup(self.timestamp, sourceDD, targetDD,
                                 int(self.options.maxStreams))
        elif self.options.recover:
            sourceDD = DDSystem("remote", dd_storage_unit=self.options.ddboost_storage_unit)
            if self.options.ping:
                self.checkDDReachable(sourceDD)
            targetDD = self.ddSystem
            self.replicateBackup(self.timestamp, sourceDD, targetDD,
                                 int(self.options.maxStreams))

    def showDDBoostIOStreams(self, localDD, remoteDD):
        """
        "--replicate" operation uses replication source I/O streams on local and
        replication destination I/O streams on remote Data Domain.  Likewise,
        "--recover" operation uses replication source I/O streams on remote and
        replication destination I/O streams on local Data Domain.
        """
        replSrcLocal = localDD.replSourceSoftLimit()
        replDestLocal = localDD.replDestSoftLimit()
        inUseLocal = localDD.inUseMfrStreams()
        replSrcRemote = localDD.replSourceSoftLimit()
        replDestRemote = localDD.replDestSoftLimit()
        inUseRemote = localDD.inUseMfrStreams()
        title = " "*47 + "%s %s" % (localDD, remoteDD)
        print title
        print "-"*len(title)
        # Format string to align two numbers in each row below local and remote
        # Data Domain names in the title.
        fmt = "\t%-"+str(len(str(localDD)))+"d  %d"
        row1 = "Maximum I/O streams available for replication"
        print row1, fmt % (replSrcLocal, replDestRemote)
        row2 = "Maximum I/O streams available for recovery   "
        print row2, fmt % (replDestLocal, replSrcRemote)
        row3 = "I/O streams currently in use for MFR         "
        print row3, fmt % (inUseLocal, inUseRemote)
        msg = "\nNo more than %d backup files may be replicated " + \
            "concurrently from %s to %s."
        maxRepl = min(replSrcLocal - inUseLocal, replDestRemote - inUseRemote)
        print msg % (maxRepl, localDD, remoteDD)
        msg = "No more than %d backup files may be recovered concurrently" + \
            " from %s to %s."
        maxRecov = min(replSrcRemote - inUseRemote, replDestLocal - inUseLocal)
        print msg % (maxRecov, remoteDD, localDD)

    def parseBackupListing(self, lines):
        """
        Given a list of lines from "gpddboost --ls ..." command, returns a
        list of BackupSet objects ordered by timestamps.
        """
        # Pattern for filename, mode, size as listed by gpddboost.
        regex = ".*(gp_[^ \t]*_(%s)(%s)[^ \t]*)[ \t]*([0-9]*)[ \t]*([0-9]*)" %\
                (yyyymmdd, hhmmss)
        pfile = re.compile(regex)
        bkpDict = {}
        for line in lines:
            m = pfile.match(line)
            if m is not None:
                name = m.group(1)
                mode = m.group(4)
                size = long(m.group(5))
                bfile = BackupFile(name, mode, size)
                # Backup time and backup date.
                bd, bt = (m.group(2), m.group(3))
                key = bd + bt
                if key not in bkpDict:
                    bkpDict[key] = BackupSet(bd, bt)
                bset = bkpDict[key]
                bset.addFile(bfile)
        bsetList = bkpDict.values()
        bsetList.sort()
        return bsetList

    def backupDirsOnDD(self, lines):
        """
        Filters 'gpddboost --ls' output to return a list of directory names on
        DD system whose names match the timestamp regex.
        """
        # Pattern for directories under default_backup_dir/.
        pdir = re.compile('.*(%s)[ \t]+' % yyyymmdd)
        backupDirs = []
        for line in lines:
            m = pdir.match(line)
            if m is not None:
                backupDirs.append(m.group(1))
        return backupDirs

    def listBackups(self):
        """
        Layout of backup directories on a DD system is as follows:

                Storage Unit (GPDB):
                     |
                     --- <default_backup_dir>
                            |
                            --- YYYYMMDD
                            |
                            --- YYYYMMDD
                            |
                            --- ...

        The function iterates through each directory under default_backup_dir to
        identify backup sets.  A backup set (aka backup) is the set of files
        whose names match a timestamp in YYYYMMDDHHMMSS format.  Validity of a
        backup set is determined by comparing the number of dump files in the
        backup set with the number of segments in a GPDB instance.
        """
        print "Listing backups on %s Data Domain." % self.ddSystem
        lines = self.ddSystem.listDir(self.defaultDir)
        if lines is None:
            print "Default backup directory '%s' not found." % self.defaultDir
            return
        ddDirs = self.backupDirsOnDD(lines)
        print "Default backup directory: %s" % self.defaultDir
        for bkpdir in ddDirs:
            lines = self.ddSystem.listDir("%s/%s" % (self.defaultDir, bkpdir))
            if lines is None:
                print "Path %s/%s not found on %s Data Domain." % \
                      (self.defaultDir, bkpdir, self.ddSystem)
                return
            bkpSets = self.parseBackupListing(lines)
            for bset in bkpSets:
                if self.gparray:
                    bset.validateWithGpdb(self.gparray)
                bset.prettyPrint()

    def _latestOrOldestBackupSet(self, bkptimestamp, ddSystem):
        """
        Starting from the latest (or oldest) backup directory, look for valid
        backup files.  For the first set of backup files thus found, return a
        BackupSet object.
        """
        if not bkptimestamp.isKeyword():
            raise Exception("Invalid timestamp keyword: %s" % bkptimestamp)
        lines = ddSystem.listDir(self.defaultDir)
        if lines is None:
            logger.info("Default backup directory '%s' not found." %
                        self.defaultDir)
            return None
        bkpDirs = map(int, self.backupDirsOnDD(lines))
        # Default sort order is ascending.
        bkpDirs.sort(reverse=bkptimestamp.isLatest())
        bset = None
        for d in bkpDirs:
            lines = ddSystem.listDir("%s/%d" % (self.defaultDir, d))
            bkpSets = self.parseBackupListing(lines)
            if len(bkpSets):
                if bkptimestamp.isLatest():
                    bset = bkpSets.pop()
                elif bkptimestamp.isOldest():
                    bset = bkpSets[0]
                break
            logger.info("No valid backup files in directory %s/%d." %
                         (self.defaultDir, d))
        if bset is not None and self.gparray is not None:
            bset.validateWithGpdb(self.gparray)
        return bset

    def _backupSet(self, bkptimestamp, ddSystem):
        """
        Lists files on DD system and returns a BackupSet object containing all
        files matching bkptimestamp.
        """
        if not bkptimestamp.ddStr:
            raise Exception("Invalid timestamp argument: %s" % bkptimestamp)
        pbkp = re.compile("(%s)(%s)" % (yyyymmdd, hhmmss))
        m = pbkp.match(bkptimestamp.ddStr)
        bkpDir = m.group(1)
        lines = ddSystem.listDir("%s/%s" % (self.defaultDir, bkpDir))
        if lines is None:
            # Directory not found on DD system.
            return None
        bkpSets = self.parseBackupListing(lines)
        if len(bkpSets) == 0:
            return None
        # Format for key: YYYYMMDDHHMMSS.
        key = m.group(1) + m.group(2)
        for bset in bkpSets:
            if bset.backupDate + bset.backupTime == key:
                if self.gparray:
                    bset.validateWithGpdb(self.gparray)
                return bset
        return None

    def listBackupFiles(self, bkptimestamp):
        """
        Lists files on a DD system in a specified backup set.  Timestamp
        YYYYMMDDHHMMSS identifies the backup set.
        """
        if bkptimestamp.isKeyword():
            print "Identifying the %s backup set on %s Data Domain." % \
                  (bkptimestamp, self.ddSystem)
            bset = self._latestOrOldestBackupSet(bkptimestamp, self.ddSystem)
            if not bset:
                print "No valid backup set found on %s Data Domain." % \
                    self.ddSystem
        else:
            bset = self._backupSet(bkptimestamp, self.ddSystem)
            if not bset:
                print "Backup '%s' not found on %s Data Domain." % \
                    (bkptimestamp, self.ddSystem)
        if bset:
            bset.prettyPrint(printFiles=True)

    def deleteBackup(self, bkptimestamp):
        """
        Deletes all files in a backup set identified by bkptimestamp.
        """
        if bkptimestamp.isKeyword():
            print "Identifying the %s backup set on %s Data Domain." % \
                (bkptimestamp, self.ddSystem)
            bset = self._latestOrOldestBackupSet(bkptimestamp, self.ddSystem)
            if not bset:
                print "No valid backup set found on %s Data Domain." % \
                    self.ddSystem
                return
        else:
            print "Identifying files to be deleted on %s Data Domain." % \
                self.ddSystem
            bset = self._backupSet(bkptimestamp, self.ddSystem)
            if not bset:
                print "Backup '%s' not found on %s Data Domain." % \
                    (bkptimestamp, self.ddSystem)
                return
        bset.prettyPrint(printFiles=True)
        if self.options.interactive:
            print "Delete all the above files from %s Data Domain (y/n)?" % \
                self.ddSystem
            c = sys.stdin.readline().strip()[0]
            if c != "y" and c != "Y":
                print "Nothing deleted."
                return
        scheduler = Scheduler(bset, self.ddSystem, quiet=self.options.quiet)
        scheduler.delete()
        print "Backup '%s' deleted from %s Data Domain." % \
            (bset.bt, self.ddSystem)

    def replicateBackup(self, bkptimestamp, sourceDD, targetDD, maxStreams):
        """
        Replicate all files in the backup set 'bkptimestamp'.
        """
        if bkptimestamp.isKeyword():
            print "Identifying the %s backup set on %s Data Domain." % \
                (bkptimestamp, sourceDD)
            sourceBset = self._latestOrOldestBackupSet(bkptimestamp, sourceDD)
            if not sourceBset:
                print "No valid backup set found on %s Data Domain." % sourceDD
                return
        else:
            print "Identifying backup files on %s Data Domain." % sourceDD
            sourceBset = self._backupSet(bkptimestamp, sourceDD)
            if not sourceBset:
                print "Backup '%s' not found on %s Data Domain." % \
                    (bkptimestamp, sourceDD)
                return
        if sourceBset.isValid is not None and not sourceBset.isValid:
            msg = "Backup '%s' does not appear to be a valid backup set."
            logger.error(msg % sourceBset.bt)
            return

        msg = "Initiating transfer for %d files from %s to %s Data Domain."
        print msg % (len(sourceBset.backupFiles), sourceDD, targetDD)

        targetBset = self._backupSet(sourceBset.bt, targetDD)
        if targetBset is not None:
            deleteTarget = False
            if targetBset == sourceBset:
                msg = "Backup %s already present on %s Data Domain."
            else:
                deleteTarget = True
                msg = "Backup %s with different set of files found on %s" + \
                      " Data Domain. It will be overwritten."
            if self.options.interactive:
                print (msg + " Continue (y/n)?") % (targetBset.bt, targetDD)
                c = sys.stdin.readline().strip()[0]
                if c != "y" and c != "Y":
                    return
            else:
                print msg % (targetBset.bt, targetDD)
            if deleteTarget:
                scheduler = Scheduler(targetBset, targetDD,
                                      quiet=self.options.quiet)
                scheduler.delete()
        else:
            # Create storage unit on target Data Domain if one doesn't
            # exist.
            targetDD.verifyLogin()

        print "Using at the most %d I/O streams on each Data Domain." % maxStreams

        scheduler = Scheduler(sourceBset, sourceDD, targetDD,
                              self.options.quiet)
        scheduler.replicate(maxStreams)


if __name__ == "__main__":
    sys.argv[0] = GPMFR_EXEC
    simple_main(mfr_parser, GpMfr, {"programNameOverride": GPMFR_EXEC})
