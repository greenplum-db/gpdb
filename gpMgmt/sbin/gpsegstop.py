#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
#
# Internal Use Function.
#
#
#
# THIS IMPORT MUST COME FIRST
#
# import mainUtils FIRST to get python version check
from gppylib.mainUtils import *

import os, sys, time, signal

from optparse import Option, OptionGroup, OptionParser, OptionValueError, SUPPRESS_USAGE

from gppylib.gpparseopts import OptParser, OptChecker
from gppylib import gplog
from gppylib.commands import base
from gppylib.commands import unix
from gppylib.commands import gp
from gppylib.commands.gp import SEGMENT_STOP_TIMEOUT_DEFAULT, DEFAULT_SEGHOST_NUM_WORKERS
from gppylib.commands import pg
from gppylib.db import dbconn
from gppylib import pgconf
from gppylib.commands.gp import is_pid_postmaster

description = ("""
This utility is NOT SUPPORTED and is for internal-use only.

stops a set of one or more segment databases.
""")

logger = gplog.get_default_logger()


# -------------------------------------------------------------------------
class SegStopStatus:
    def __init__(self, datadir, stopped, reason):
        self.datadir = datadir
        self.stopped = stopped
        self.reason = reason

    def __str__(self):
        return "STATUS--DIR:%s--STOPPED:%s--REASON:%s" % (self.datadir, self.stopped, self.reason)


class SegStop(base.Command):
    def __init__(self, name, db, mode, timeout):
        self.name = name
        self.db = db
        self.mode = mode
        self.timeout = timeout
        self.result = None
        self.port = None
        self.datadir = None
        self.logger = logger
        base.Command.__init__(self, name=self.name, cmdStr='Stop an individual segment on the host', ctxt=None,
                              remoteHost=None)

    def get_datadir_and_port(self):
        return self.db.split(':')[0:2]

    def get_results(self):
        return self.result

    def run(self):

        try:
            self.datadir, self.port = self.get_datadir_and_port()

            # Get a list of postgres processes running before stopping the server
            # Use localhost since gpsegstop is run locally on segment hosts
            postgres_pids = gp.get_postgres_segment_processes(self.datadir, 'localhost')
            logger.debug("Postgres processes running for segment %s: %s", self.datadir, postgres_pids)

            cmd = gp.SegmentStop('segment shutdown', self.datadir, mode=self.mode, timeout=self.timeout)
            cmd.run()

            results = cmd.get_results()
            is_shutdown = False

            if results.rc == 0:
                cmd = gp.SegmentIsShutDown('check if shutdown', self.datadir)
                cmd.run()

                if cmd.is_shutdown():
                    status = SegStopStatus(self.datadir, True, "Shutdown Succeeded")
                    self.result = status
                    is_shutdown = True
                # MPP-16171
                #
                elif self.mode == 'immediate':
                    status = SegStopStatus(self.datadir, True, "Shutdown Immediate")
                    self.result = status
                    is_shutdown = True

            # read pid and datadir from /tmp/.s.PGSQL.<port>.lock file
            name = "failed segment '%s'" % self.db
            (succeeded, mypid, file_datadir) = pg.ReadPostmasterTempFile.local(name, self.port).getResults()

            if not is_shutdown:
                if succeeded and file_datadir == self.datadir:

                    # now try to terminate the process, first trying with
                    # SIGTERM and working our way up to SIGABRT sleeping
                    # in between to give the process a moment to exit
                    #
                    unix.kill_sequence(mypid)

                    if not unix.check_pid(mypid):
                        lockfile = "/tmp/.s.PGSQL.%s" % self.port
                        if os.path.exists(lockfile):
                            self.logger.info("Clearing segment instance lock files")
                            os.remove(lockfile)

                status = SegStopStatus(self.datadir, True,
                                       "Forceful termination success: rc: %d stdout: %s stderr: %s." % (
                                       results.rc, results.stdout, results.stderr))

            try:
                # Use the process list and make sure that all the processes are killed at the end
                # Use localhost since gpsegstop is run locally on segment hosts
                unix.kill_9_segment_processes(self.datadir, postgres_pids, 'localhost')

                if unix.check_pid(mypid) and mypid != -1:
                    status = SegStopStatus(self.datadir, False,
                                           "Failed forceful termnation: rc: %d stdout: %s stderr: %s." % (
                                           results.rc, results.stdout, results.stderr))

                self.result = status
            except Exception as e:
                logger.error('Failed forceful termination of segment %s: (%s)' % (self.datadir, str(e)))
                self.result = SegStopStatus(self.datadir, False,
                                            'Failed forceful termination of segment! (%s)' % str(e))

            return self.result
        except Exception as e:
            logger.exception(e)
            self.result = SegStopStatus(self.datadir, False, 'Shutdown failed! %s' % str(e))
            return self.result


# -------------------------------------------------------------------------
class GpSegStop:
    ######
    def __init__(self, dblist, mode, gpversion, timeout=SEGMENT_STOP_TIMEOUT_DEFAULT, logfileDirectory=False, segment_batch_size=DEFAULT_SEGHOST_NUM_WORKERS):
        self.dblist = dblist
        self.mode = mode
        self.expected_gpversion = gpversion
        self.timeout = timeout
        self.gphome = os.path.abspath(os.pardir)
        self.actual_gpversion = gp.GpVersion.local('local GP software version check', self.gphome)
        if self.actual_gpversion != self.expected_gpversion:
            raise Exception("Local Software Version does not match what is expected.\n"
                            "The local software version is: '%s'\n"
                            "But we were expecting it to be: '%s'\n"
                            "Please review and correct" % (self.actual_gpversion, self.expected_gpversion))
        self.logger = logger
        self.pool = None
        self.logfileDirectory = logfileDirectory
        self.segment_batch_size = segment_batch_size

    ######
    def run(self):
        results = []
        failures = []

        self.logger.info("Issuing shutdown commands to local segments...")
        self.pool = base.WorkerPool(min(len(self.dblist), self.segment_batch_size))
        for db in self.dblist:
            cmd = SegStop('segment shutdown', db=db, mode=self.mode, timeout=self.timeout)
            self.pool.addCommand(cmd)
        self.pool.join()

        failed = False
        for cmd in self.pool.getCompletedItems():
            result = cmd.get_results()
            if not result.stopped:
                failed = True
            results.append(result)

        # Log the results!
        status = '\nCOMMAND RESULTS\n'
        for result in results:
            status += str(result) + "\n"

        self.logger.info(status)
        return 1 if failed else 0

    ######
    def cleanup(self):
        if self.pool:
            self.pool.haltWork()

    @staticmethod
    def createParser():
        parser = OptParser(option_class=OptChecker,
                           description=' '.join(description.split()),
                           version='%prog version $Revision: #12 $')
        parser.setHelp([])

        addStandardLoggingAndHelpOptions(parser, includeNonInteractiveOption=False)

        parser.add_option("-D", "--db", dest="dblist", action="append", type="string")
        parser.add_option("-V", "--gp-version", dest="gpversion", metavar="GP_VERSION",
                          help="expected software version")
        parser.add_option("-m", "--mode", dest="mode", metavar="<MODE>",
                          help="how to shutdown. modes are smart,fast, or immediate")
        parser.add_option("-t", "--timeout", dest="timeout", type="int", default=SEGMENT_STOP_TIMEOUT_DEFAULT,
                          help="seconds to wait")
        parser.add_option("-b", "--segment-batch-size", dest="segment_batch_size", type="int", default=DEFAULT_SEGHOST_NUM_WORKERS,
                          help="Max number of segments per host to operate on in parallel.")
        return parser

    @staticmethod
    def createProgram(options, args):
        logfileDirectory = options.ensure_value("logfileDirectory", False)
        return GpSegStop(options.dblist, options.mode, options.gpversion, options.timeout,
                         logfileDirectory=logfileDirectory, segment_batch_size=options.segment_batch_size)


# -------------------------------------------------------------------------
if __name__ == '__main__':
    mainOptions = {'setNonuserOnToolLogger': True}
    simple_main(GpSegStop.createParser, GpSegStop.createProgram, mainOptions)
