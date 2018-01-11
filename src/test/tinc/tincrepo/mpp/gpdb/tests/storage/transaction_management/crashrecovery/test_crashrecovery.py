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

import time

from gppylib.commands.base import Command
from gppylib.db import dbconn
from gppylib.gparray import GpArray

import tinctest
from tinctest.lib import local_path
from tinctest.case import TINCTestCase
from tinctest.models.scenario import ScenarioTestCase

from mpp.lib.gpdbverify import GpdbVerify
from mpp.lib.PSQL import PSQL
from mpp.lib.filerep_util import Filerepe2e_Util

class CrashRecovery_2PC(TINCTestCase):
    """Recovery scenarios for distributed transactions.
    """

    def __init__(self, methodName):
        super(CrashRecovery_2PC, self).__init__(methodName)
        self.skipRestart = False
        self.proc = None
        self.filereputil = Filerepe2e_Util()

    def setUp(self):
        tinctest.logger.info("setUp: resetting fault")
        self.faultcmd = ("gpfaultinjector -f dtm_xlog_distributed_commit -y %s"
                         " -o 0 --seg_dbid 1")
        cmd = Command("Reset fault if one is injected already",
                      self.faultcmd % "reset")
        cmd.run(validateAfter=True)
        tinctest.logger.info(cmd.get_results().printResult())

    def tearDown(self):
        # Wait for the process we spawned.
        if self.proc is not None:
            tinctest.logger.info("tearDown: waiting for proc")
            (rc, out, err) = self.proc.communicate2()
            tinctest.logger.info("runNoWait rc: %d, output: %s, err: %s" %
                                  (rc, out, err))
        # Restarting the stopped segment(s) should be enough but it
        # will require constructing the right pg_ctl command, which is
        # a hassle.
        if self.skipRestart:
            tinctest.logger.info("tearDown: cleaning up fault")
            cmd = Command("Reset fault if one is injected already",
                          self.faultcmd % "reset")
            cmd.run(validateAfter=True)
            tinctest.logger.info(cmd.get_results().printResult())
        else:
            cmd = Command("restarting GPDB", "gpstop -air")
            cmd.run(validateAfter=True)
            tinctest.logger.debug(cmd.get_results().printResult())

    def test_master_panic_after_phase1(self):
        """PANIC master after recording distributed commit.

        Trigger PANIC in master after completing phase 1 of 2PC,
        right after recording distributed commit in xlog but before
        broadcasting COMMIT PREPARED to segments.  Master's recovery
        cycle should correctly broadcast COMMIT PREPARED because
        master should find distributed commit record in its xlog
        during recovery.  Verify that the transaction is committed
        after recovery.

        JIRA: MPP-19044

        """
        tinctest.logger.info("running test: test_crash_master_after_phase1")
        gparray = GpArray.initFromCatalog(dbconn.DbURL(), utility=True)
        assert len(gparray.getHostList()) == 1, "cannot run on multi-node"
        host = gparray.getHostList()[0]

        # Must have at least one in-sync and up segment.
        primaries = [
            p for p in gparray.get_list_of_primary_segments_on_host(host)
            if p.getSegmentMode() == "s" and p.getSegmentStatus() == "u"]
        assert len(primaries) > 0, "in-sync and up primary not found"
        primary = primaries[0]
        tinctest.logger.info("chose primary: %s" % primary.datadir)

        # Inject suspend fault after recording distributed commit on master.
        cmd = Command("Suspend master post distributed commit",
                      self.faultcmd % "suspend")
        cmd.run(validateAfter=True)
        tinctest.logger.info(cmd.get_results().printResult())

        # Trigger the fault.
        cmd = Command("run DDL", "psql -f %s" %
                      local_path('sql/ao_create.sql'))
        self.proc = cmd.runNoWait()
        tinctest.logger.info("runNoWait: %s, pid: %d" % (cmd.cmdStr, self.proc.pid))

        commitBlocked = self.filereputil.check_fault_status(fault_name='dtm_xlog_distributed_commit', status="triggered", seg_id='1', num_times_hit=1);

        # Shutdown of primary (and mirror) should happen only after
        # the commit is blocked due to suspend fault.
        assert commitBlocked, "timeout waiting for commit to be blocked"
        tinctest.logger.info("commit is blocked due to suspend fault")
        # At this point, segments have already recorded the
        # transaction as prepared by writing PREPARE record in xlog.
        # Crash one primary (and its mirror).
        mirror = None
        mirrors = [m for m in gparray.get_list_of_mirror_segments_on_host(host)
                   if m.getSegmentMode() == "s" and m.getSegmentStatus() == "u"
                   and primary.getSegmentContentId() == m.getSegmentContentId()]
        if len(mirrors) > 0:
            mirror = mirrors[0]
            tinctest.logger.info("chose mirror: %s" % mirror.datadir)
            # Pause FTS probes to avoid a failover while we bring down
            # segments.  Note that we bring down both primary and its
            # mirror, thereby causing double failure.  This prevents
            # FTS from making changes to segment configuration, even
            # if FTS probes are unpaused.  It is necessary to unpause
            # FTS probes to prevent gang creation from being blocked.
            PSQL.run_sql_command_utility_mode("SET gp_fts_probe_pause = on")
            tinctest.logger.info("FTS probes paused")
            cmdstr = 'pg_ctl -D %s stop -m immediate' % mirror.datadir
            tinctest.logger.info("bringing down primary: %s" % cmdstr)
            cmd = Command("Shutdown a primary segment", cmdstr)
            cmd.run(validateAfter=True)

        cmdstr = 'pg_ctl -D %s stop -m immediate' % primary.datadir
        tinctest.logger.info("bringing down primary: %s" % cmdstr)
        cmd = Command("Shutdown a primary segment", cmdstr)
        cmd.run(validateAfter=True)

        if mirror is not None:
            PSQL.run_sql_command_utility_mode("SET gp_fts_probe_pause = off")
            tinctest.logger.info("FTS probes unpaused")

        # Resume master.  Master should PANIC and go through crash recovery.
        cmd = Command("resume master", self.faultcmd % "resume")
        cmd.run(validateAfter=True)
        tinctest.logger.info(cmd.get_results().printResult())

        (rc, out, err) = self.proc.communicate2()
        self.proc = None
        tinctest.logger.info("runNoWait rc: %d, output: %s, err: %s" %
                              (rc, out, err))
        # Fail if QD did not PANIC.
        assert (out.find("commit succeeded") == -1 and
                err.find("commit succeeded") == -1 and
                err.find("PANIC") != -1)

        # Wait for a few seconds to ensure that postmaster reset has started
        time.sleep(5)

        # Wait for recovery to complete, timeout after ~ 5 mins.
        attempts = 1
        recoveryComplete = False
        while attempts < 600 and not recoveryComplete:
            recoveryComplete = "aaa150" in PSQL.run_sql_command_utility_mode(
                "select 'aaa' || (100+50)")
            time.sleep(0.5)
            attempts = attempts + 1
        assert recoveryComplete, "timeout waiting for master to recover"
        cmdstr = "gpstop -ar"
        cmd = Command("restart", cmdstr)
        tinctest.logger.info("restarting the cluster with '%s'" % cmdstr)
        cmd.run(validateAfter=True)
        tinctest.logger.info("restart complete")
        # Verify table got created (commit was successful).
        assert PSQL.run_sql_file(local_path('sql/ao_select.sql'))

        gpverify = GpdbVerify()
        (errorCode, hasError, gpcheckcat_output, repairScript) = gpverify.gpcheckcat()
        assert errorCode == 0, ("gpcheckcat failed: %s" % gpcheckcat_output[0])

        # No need to restart GPDB again in tearDown()
        self.skipRestart = True
