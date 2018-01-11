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

# Brief blurb on XidlimitsTests:
#
# As a consequence of disabling autovacuum, age of template0 database
# (or any other database having datallowconn='f') keeps on increasing.
# It eventually crosses xid_warn_limit, xid_stop_limit and finally
# wrap limit.  A new database created using template0 as template
# inherits this age.  These tests are intended to exercise procedures
# to rectify the age of such a newly created database when it has
# crossed the above mentioned limits (hence the name XidlimitsTests).
# The tests also ensure that autovacuum daemon is indeed disabled on
# master and segments.

from os.path import abspath, dirname, isdir
import sys
import unittest2 as unittest
from tinctest import logger
from tinctest.lib import Gpdiff, local_path
from mpp.lib.PSQL import PSQL, PSQLException
from mpp.gpdb.lib.models.sql.template import SQLTemplateTestCase
from mpp.models import MPPTestCase

import tinctest.lib.gpplatform as gpplatform
from gppylib.commands.base import Command, ExecutionError
from gppylib.db import dbconn
from gppylib.gparray import GpArray

FirstNormalTransactionId = 3
def xid_sum(x, y):
    """
    XID arithmetic is tricky. See GetNewTransactionId for more information.
    """
    ret = (x + y) % (2 ** 32)
    if ret <= FirstNormalTransactionId:
        ret += FirstNormalTransactionId
    return ret

def preceding_xid(x, y):
    """
    Returns the xid that precedes the other.  Analogous to the standard min().
    The logic here is equivalent to the step "(int) (x - y)" where x and y are
    32-bit unsigned int variables in TransactionIdPrecedes() in transam.c.
    """
    diff = x - y
    if diff >= 2**31:
        ret = x
    elif diff > 0:
        ret = y
    elif diff > -2**31:
        ret = x
    else: # diff <= -2**31
        ret = y
    return ret

@unittest.skipIf(gpplatform.get_info()=="SOL", "Skipping Solaris, cannot compile xidhelper.so")
class XidlimitsTests(MPPTestCase):
    """
    
    @description Xidlimits Tests
    @created 2013-02-12 00:00:00
    @modified 2013-02-12 00:00:00
    @tags vacuum xidlimits echo
    @gucs gp_create_table_random_default_distribution=off
    """
    # Constants identifying the limit to exceed.
    WARN_LIMIT = 0
    STOP_LIMIT = 1
    WRAP_LIMIT = 2

    def setUp(self):
        super(XidlimitsTests, self).setUp()
        Command('re-build regress.so',
                'make -C %s xidhelper.so' % local_path('.')).run(validateAfter=True)
        SQLTemplateTestCase.perform_transformation_on_sqlfile(local_path('load_xidhelper.sql'),
                                                              local_path('load_xidhelper.sql.t'),
                                                              {'@source@' : local_path('xidhelper.so')})
        PSQL.run_sql_file(sql_file=local_path('load_xidhelper.sql.t'),
                          out_file=local_path('load_xidhelper.out.t'))
        self.gparray = GpArray.initFromCatalog(dbconn.DbURL(), utility=True)

    def _basic_sanity_check(self, suffix, kwargs=None):
        """
        Runs sanity.sql which contains a CREATE, INSERT, SELECT, and DROP.

        @param kwargs: dictionary having keys are PSQL.run_sql_file argument
        names and values as their values.
        """
        out_file = local_path('sanity_%s.out' % suffix)
        ans_file = local_path('sanity_%s.ans' % suffix)
        if kwargs is None:
            kwargs = {}
        kwargs["sql_file"] = local_path('sanity_%s.sql' % suffix)
        kwargs["out_file"] = out_file
        try:
            PSQL.run_sql_file(**kwargs)
        except:
            pass
        assert Gpdiff.are_files_equal(out_file,
                                      ans_file,
                                      match_sub=[local_path('init_file')])

    def _get_primary_mirror_pair(self):
        """
        Return the (primary, mirror) having content id = 0.
        """
        primary, mirror = None, None
        for seg in self.gparray.getSegDbList(True):
            if seg.content == 0:
                if seg.role == 'p':
                    primary = seg
                else:
                    mirror = seg
        return (primary, mirror)

    def _set_allowconn_template0(self, flag):
        """
        Enable connections to template0 on master and all segments.
        """
        if flag:
            logger.info("Enabling connections to template0")
        else:
            logger.info("Disabling connections to template0")
        for seg in self.gparray.getDbList(True):
            if seg.role == 'p':
                seg_url = dbconn.DbURL(hostname=seg.hostname, port=seg.port)
                with dbconn.connect(seg_url,
                                    utility=True,
                                    allowSystemTableMods='dml') as conn:
                    dbconn.execSQL(
                        conn, "update pg_database set datallowconn=%s "
                        "where datname='template0'" % flag)
                    conn.commit()

    def _restore_stop_limit_guc(self, datadir):
        """
        Reset xid_stop_limit GUC to default value, by removing the setting from
        postgresql.conf.

        @param datadir: PGDATA directory containing postgresql.conf that needs
        to be restored.
        """
        logger.info("Undo the stop limit GUC change")
        cmd = "source $GPHOME/greenplum_path.sh && gpstop -a"
        Command("stop system", cmd).run(validateAfter=True)
        cmd = ('sed -i".bk" "s|xid_stop_limit=.*||g" %s/postgresql.conf' %
               datadir)
        Command("undo xid_stop_limit change", cmd).run(validateAfter=True)
        cmd = "source $GPHOME/greenplum_path.sh && gpstart -a"
        Command("start system", cmd).run(validateAfter=True)

    def _reduce_stop_limit_guc(self, segdb, new_slimit):
        """
        Reduce the xid_stop_limit GUC by the specified value.

        @param datadir: PGDATA directory containing postgresql.conf that needs
        to be modified.

        @param new_slimit: New value of xid_stop_limit GUC, less than the
        default value of 10**9.
        """
        for seg in self.gparray.getDbList(True):
           logger.info("Stopping segment %d at %s" % (seg.dbid, seg.datadir))
           cmd = "pg_ctl -D %s stop" % seg.datadir
           Command("stop segment", cmd).run(validateAfter=True)
        logger.info("New xid_stop_limit: %s" % new_slimit)
        cmd = ('echo "xid_stop_limit=%d" >> %s/postgresql.conf' %
               (new_slimit, segdb.datadir))
        Command("revise xid_stop_limit", cmd).run(validateAfter=True)
        logger.info("Starting the cluster")
        cmd = "source $GPHOME/greenplum_path.sh && gpstart -a"
        Command("start cluster", cmd).run(validateAfter=True)
        dburl = dbconn.DbURL(hostname=segdb.hostname, port=segdb.port)
        with dbconn.connect(dburl, utility=True) as conn:
            stop_limit = int(
                dbconn.execSQLForSingleton(conn, "SHOW xid_stop_limit"))
        self.assertEqual(stop_limit, new_slimit, "Failed to set xid_stop_limit")

    def _raise_template0_age(self, limit, segdb):
        """
        Increase age of template0 beyond the specified limit on the specified
        segment.  When a new database is created off template0, the limit will
        be exceeded.  Assumption: template0 age =~ 0 or at least not already
        crossing any of the xid limits.  Because this function can only raise
        the age, cannot decrease it.

        @param limit: one of WARN_LIMIT, STOP_LIMIT and WRAP_LIMIT.

        @param segdb: an instance of Segment class representing the segment on
        which the limit will be exceeded.
        """
        dburl = dbconn.DbURL(hostname=segdb.hostname, port=segdb.port)
        databases = []
        with dbconn.connect(dburl, utility=True) as conn:
            sql = "SELECT datname FROM pg_database WHERE datallowconn='t'"
            for row in dbconn.execSQL(conn, sql):
                databases.append(row[0])
            sql = "SHOW xid_stop_limit"
            stop_limit_guc = int(dbconn.execSQLForSingleton(conn, sql))
            sql = "SHOW xid_warn_limit"
            warn_limit_guc = int(dbconn.execSQLForSingleton(conn, sql))
            sql = ("SELECT datfrozenxid, age(datfrozenxid) FROM pg_database "
                   "WHERE datname='template0'")
            row = dbconn.execSQL(conn, sql).fetchone()
            datfxid, age = int(row[0]), int(row[1])
            sql = "SELECT get_next_xid()"
            current_xid = int(dbconn.execSQLForSingleton(conn, sql))
        # Estimate of XIDs consumed by vacuum freeze operaiton on all databases.
        vacuum_xids = len(databases) * 500
        logger.info("Estimated xids for vacuume freeze: %d" % vacuum_xids)
        if limit == self.WARN_LIMIT:
            target_age = (2**31) - stop_limit_guc - warn_limit_guc
            target_xid = xid_sum(datfxid, target_age)
            keep_raising = lambda x: x < target_age
        elif limit == self.STOP_LIMIT:
            target_age = (2**31) - stop_limit_guc
            target_xid = xid_sum(datfxid, target_age)
            keep_raising = lambda x: x < target_age
        elif limit == self.WRAP_LIMIT:
            target_xid = xid_sum(datfxid, 2**31)
            keep_raising = lambda x: x > 0
        logger.info("Target xid = %d, limit = %d" % (target_xid, limit))
        self.assertEqual(preceding_xid(target_xid, current_xid), current_xid,
                         "Target xid (%d) precedes current xid (%d)" %
                         (target_xid, current_xid))
        while keep_raising(age):
            with dbconn.connect(dburl, utility=True) as conn:
                sql = "SELECT get_stop_limit()"
                stop_limit = int(dbconn.execSQLForSingleton(conn, sql))
                # GPDB may stop accepting connections if we spoof nextXid beyond
                # max_xid.
                max_xid = xid_sum(stop_limit, -vacuum_xids)
                new_xid = preceding_xid(target_xid, max_xid)
                logger.info("Spoofing next xid to %d, current stop limit = %d" %
                            (new_xid, stop_limit))
                sql = "SELECT spoof_next_xid('%d'::xid)"
                dbconn.execSQL(conn, sql % new_xid)
                conn.commit()
                sql = ("SELECT age(datfrozenxid) FROM pg_database "
                       "WHERE datname='template0'")
                age = int(dbconn.execSQLForSingleton(conn, sql))
            logger.info("template0 age raised to %d" % age)
            # The vacuum freeze of all databases advances stop_limit further,
            # necessary for iterating the while loop.  And template0 becomes the
            # oldest database aka the only culprit to violate the specified
            # limit.
            PSQL(sql_cmd='VACUUM FREEZE', dbname='postgres', out_file='vacuum_postgres.out').run(validateAfter=True)
            for datname in databases:
                logger.info('vacuum freeze %s' % datname)
                PSQL(sql_cmd='VACUUM FREEZE',
                     dbname=datname,
                     out_file='vacuum_%s.out' % datname).run(validateAfter=True)

    def _reset_age(self, dbname, segdb=None):
        """
        Resets datfrozenxid and relfrozenxid's in pg_class of the
        specified dbname to a value close to the current xid.  This is
        a recommended way of resetting age of dbname or a database
        that is created off template0.

        @param segdb: identifies the segment on which to operate.  It is an
        instance of Segment class.

        Note that the database dbname must have all tuples frozen (xmin=2).
        This holds true of template0 and of a database created off template0,
        only if there are no modifications done to the database.

        """
        if segdb is None:
            segdb = self.gparray.master
        dburl = dbconn.DbURL(hostname=segdb.hostname, port=segdb.port)
        dburl_dbname = dbconn.DbURL(hostname=segdb.hostname,
                                    port=segdb.port,
                                    dbname=dbname)
        with dbconn.connect(dburl,
                            utility=True,
                            allowSystemTableMods="dml") as conn:
            sql = "SELECT get_next_xid()"
            next_xid = int(dbconn.execSQLForSingleton(conn, sql))
            sql = "UPDATE pg_database SET datfrozenxid='%d'::xid WHERE datname='%s'"
            dbconn.execSQL(conn, sql % (next_xid, dbname))
            conn.commit()
        if dbname == "template0":
            self._set_allowconn_template0(True)
        with dbconn.connect(dburl_dbname,
                            utility=True,
                            allowSystemTableMods="dml") as conn:
            sql = ("UPDATE pg_class SET relfrozenxid='%d'::xid WHERE "
                   "int8in(xidout(relfrozenxid)) > 0")
            dbconn.execSQL(conn, sql % next_xid)
            conn.commit()
        PSQL(sql_cmd="VACUUM FREEZE pg_class",
             dbname=dbname,
             PGOPTIONS="-c 'gp_session_role=utility'",
             host=segdb.hostname,
             port=segdb.port,
             out_file="vacuum_%s.out" % dbname).run(validateAfter=True)
        with dbconn.connect(dburl_dbname,
                            utility=True,
                            allowSystemTableMods="dml") as conn:
            dbconn.execSQL(conn, "DELETE FROM pg_stat_last_operation")
            conn.commit()
        PSQL(sql_cmd="VACUUM FREEZE pg_stat_last_operation",
             dbname=dbname,
             PGOPTIONS="-c 'gp_session_role=utility'",
             host=segdb.hostname,
             port=segdb.port,
             out_file="vacuum_%s.out" % dbname).run(validateAfter=True)
        if dbname == "template0":
            self._set_allowconn_template0(False)
        with dbconn.connect(dburl, utility=True) as conn:
            sql = "SELECT age(datfrozenxid) FROM pg_database WHERE datname='%s'"
            age_dbname = dbconn.execSQLForSingleton(conn, sql % dbname)
            age_dbname = int(age_dbname)
        logger.info("Age of %s reset to %d" % (dbname, age_dbname))
        # We are OK as long as dbname age is less than xid_warn_limit.  The
        # 10000 is just a number assumed to be less than xid_warn_limit.
        self.assertTrue(age_dbname > 0 and age_dbname < 10000,
                        "age(%s) = %d, next xid = %d" %
                        (dbname, age_dbname, next_xid))

    def test_autovacuum_signaling(self):
        """
        Raise the nextXid to oldest_frozenxid + autovacuum_freeze_max_age.
        Run a transaction.
        Ensure that no autovacuum daemon is started.
        """
        dburl = dbconn.DbURL()
        with dbconn.connect(dburl) as conn:
            oldest_xid = int(dbconn.execSQLForSingleton(conn, 'select get_oldest_xid()'))
            autovacuum_freeze_max_age = int(dbconn.execSQLForSingleton(conn, 'show autovacuum_freeze_max_age'))
            autovacuum_xid_limit = xid_sum(oldest_xid, autovacuum_freeze_max_age)
            logger.info('Raising master xid to autovacuum_xid_limit %d' % autovacuum_xid_limit)
            dbconn.execSQLForSingleton(conn, "select spoof_next_xid('%d'::xid)" % autovacuum_xid_limit)

        # A new connection to the postmaster, at this point, will ensure that we roll through
        # the ServerLoop and potentially fork an autovacuum process... if enabled.
        # Burn a transaction to trigger any undesirable behavior that we're disabling.
        with dbconn.connect(dburl) as conn:
            self.assertEqual(1, int(dbconn.execSQLForSingleton(conn, 'select 1')))

        cmd = Command('check for autovacuum',
                      'ps -ef | grep -v grep | grep postgres | grep autovacuum')
        cmd.run()
        self.assertEqual(cmd.get_results().stdout, "", "Seriously? Found a postgres autovacuum process!")

        self._basic_sanity_check('clean')

    def test_autovacuum_signaling_on_segment(self):
        """
        Same as above, but on a segment.
        """
        # connect to the master to build gparray
        primary, _ = self._get_primary_mirror_pair()
        logger.info('Isolated segment %d at %s:%d' % (primary.dbid, primary.hostname, primary.port))
        dburl = dbconn.DbURL(hostname=primary.hostname, port=primary.port)

        with dbconn.connect(dburl, utility=True) as conn:
            oldest_xid = int(dbconn.execSQLForSingleton(conn, 'select get_oldest_xid()'))
            autovacuum_freeze_max_age = int(dbconn.execSQLForSingleton(conn, 'show autovacuum_freeze_max_age'))
            autovacuum_xid_limit = xid_sum(oldest_xid, autovacuum_freeze_max_age)
            logger.info('Raising segment xid to autovacuum_xid_limit %d' % autovacuum_xid_limit)
            dbconn.execSQLForSingleton(conn, "select spoof_next_xid('%d'::xid)" % autovacuum_xid_limit)

        # A new connection to the postmaster, at this point, will ensure that we roll through
        # the ServerLoop and potentially fork an autovacuum process... if enabled.
        with dbconn.connect(dburl, utility=True) as conn:
            self.assertEqual(1, int(dbconn.execSQLForSingleton(conn, 'select 1')))

        cmd = Command('check for autovacuum',
                      'ssh %s ps -ef | grep -v grep | grep postgres | grep autovacuum' % primary.hostname)
        cmd.run()
        self.assertEqual(cmd.get_results().stdout, "", "Seriously? Found a postgres autovacuum process!")

        self._basic_sanity_check('clean')

    def test_template0_age_limits_master(self):
        """
        Increase template0 age on master in steps:
           1. Cross warn limit on segment
           2. Cross stop limit on segment
           3. Cause wrap around on segment
        """
        self.template0_warn_limit()
        self.template0_stop_limit()
        self.template0_wrap_around()
        # Clean up.
        self._reset_age("template0")

    def template0_warn_limit(self):
        """
        Raise next xid so that age(template0) grows beyond warn limit.
        Create a new database from template0, which will inherit age
        of template0.  Ensure that warnings stop when vacuum freeze is
        run on the new database.

        """
        # Bump up age of template0 to cause warn limit violation.
        self._raise_template0_age(self.WARN_LIMIT, self.gparray.master)
        # All is well until we create a new db off template0.
        self._basic_sanity_check("clean")
        # Create database newdb off template0.
        PSQL(sql_cmd="CREATE DATABASE newdb TEMPLATE template0").run(
            validateAfter=True)
        # newdb is now the oldest database, older than warn limit.
        self._basic_sanity_check("warn")
        # Ensure that vacuum freeze on newdb stops the warnings.
        PSQL(sql_cmd="VACUUM FREEZE", dbname="newdb",
             out_file="vacuum_newdb_wl_master.out").run(validateAfter=True)
        self._basic_sanity_check("clean")
        PSQL.drop_database(dbname="newdb")

    def template0_stop_limit(self):
        """
        Raise next xid so that age(template0) grows beyond stop limit.
        Create a new database off template0, let GPDB stop accepting
        commands.  Recover GPDB using the documented proceudure.
        Ensure that the new database is sane.

        """
        dburl = dbconn.DbURL()
        with dbconn.connect(dburl, utility=True) as conn:
            sql = "SHOW xid_stop_limit"
            slimit_guc = int(dbconn.execSQLForSingleton(conn, sql))
        new_limit = xid_sum(slimit_guc, -(10**6))
        # Raise nextXid so that template0 age would cross stop limit.
        self._raise_template0_age(self.STOP_LIMIT, self.gparray.master)
        # newdb's age crosses stop limit and GPDB stops accepting commands.
        PSQL(sql_cmd="CREATE DATABASE newdb TEMPLATE template0").run(
            validateAfter=True)
        self._basic_sanity_check("error")
        # Reduce xid_stop_limit as per the standard procedure.
        self._reduce_stop_limit_guc(self.gparray.master, new_limit)
        # Vacuum freezing newdb should be suffice to recover.
        PSQL(sql_cmd="VACUUM FREEZE",
             dbname="newdb",
             out_file="vacuum_newdb_stop_master.out").run(validateAfter=True)
        self._basic_sanity_check("clean")
        PSQL.drop_database(dbname="newdb")
        self._restore_stop_limit_guc(self.gparray.master.datadir)

    def template0_wrap_around(self):
        """
        Raise next xid so that age(template0) suffers a wrap around and
        becomes negative.  Create a new database off template0, which
        also suffers wrap around.  Reset the new db's age.  Sanity
        must succeed on the new db.

        """
        self._raise_template0_age(self.WRAP_LIMIT, self.gparray.master)
        PSQL(sql_cmd="CREATE DATABASE newdb TEMPLATE template0").run(
            validateAfter=True)
        sql = "SELECT age(datfrozenxid) FROM pg_database WHERE datname='newdb'"
        dburl = dbconn.DbURL()
        with dbconn.connect(dburl, utility=True) as conn:
            age_newdb = int(dbconn.execSQLForSingleton(conn, sql))
        # Xid wrap-around should cause template0 and newdb's age to be negative.
        self.assertTrue(age_newdb < 0)
        # All xids in newdb are frozen at this point.  Therefore, we
        # can reset its age so that it is not negative.
        self._reset_age("newdb")
        with dbconn.connect(dburl, utility=True) as conn:
            age_newdb = int(dbconn.execSQLForSingleton(conn, sql))
        self.assertTrue(age_newdb > 0)
        # Verify that normal operations can be performed on newdb post recovery
        # from wraparound.
        self._basic_sanity_check("clean", {"dbname":"newdb"})
        logger.info("Sanity succeeded on newdb, dropping it.")
        PSQL.drop_database(dbname="newdb")

    def test_template0_age_limits_segment(self):
        """
        Increase template0 age on a segment in steps:
           1. Cross warn limit on segment
           2. Cross stop limit on segment
           3. Cause wrap around on segment
        """
        primary, _ = self._get_primary_mirror_pair()
        self.template0_warn_limit_on_segment(primary)
        # self.template0_stop_limit_on_segment(primary)
        # self.template0_wrap_around_on_segment(primary)
        # Clean up.
        # self._reset_age("template0", primary)

    def template0_warn_limit_on_segment(self, primary):
        """
        Same as template0_warn_limit, but on a segment.
        """
        logger.info("template0_warn_limit_on_segment: dbid(%d) %s:%d'" %
                    (primary.dbid, primary.hostname, primary.port))
        # Bump up age of template0 to cause warn limit violation.
        self._raise_template0_age(self.WARN_LIMIT, primary)
        # All is well until we create a new db off template0.
        self._basic_sanity_check("clean")
        # Create database newdb off template0.
        PSQL(sql_cmd="CREATE DATABASE newdb TEMPLATE template0").run(
            validateAfter=True)
        logger.info("newdb created off template0")
        # newdb is now the oldest database, older than warn limit.
        self._basic_sanity_check("warn_segment")
        # Ensure that vacuum freeze on newdb stops the warnings.
        PSQL(sql_cmd="VACUUM FREEZE", dbname="newdb",
             out_file="vacuum_newdb_warn_seg.out").run(validateAfter=True)
        self._basic_sanity_check("clean")
        PSQL.drop_database(dbname="newdb")

    def template0_stop_limit_on_segment(self, primary):
        """
        Same as template0_stop_limit, but on segment.
        """
        logger.info("template0_stop_limit_on_segment: dbid(%d) %s:%d'" %
                    (primary.dbid, primary.hostname, primary.port))
        dburl = dbconn.DbURL(hostname=primary.hostname, port=primary.port)
        with dbconn.connect(dburl, utility=True) as conn:
            sql = "SHOW xid_stop_limit"
            slimit_guc = int(dbconn.execSQLForSingleton(conn, sql))
        new_limit = xid_sum(slimit_guc, -(10**6))
        # Raise nextXid so that template0 age would cross stop limit.
        self._raise_template0_age(self.STOP_LIMIT, primary)
        # newdb's age crosses stop limit and GPDB stops accepting commands.
        PSQL(sql_cmd="CREATE DATABASE newdb TEMPLATE template0").run(
            validateAfter=True)
        logger.info("newdb created off template0")
        # Ensure that utility connections to the segment fail with error.
        psql_args = {"PGOPTIONS":"-c 'gp_session_role=utility'",
                     "host":primary.hostname,
                     "port":primary.port}
        self._basic_sanity_check("error", psql_args)
        logger.info("Utility connection to dbid(%d) reported stop limit "
                    "error, as expected." % primary.dbid)
        try:
            # Verify that SQL commands from master fail.
            PSQL(sql_cmd="CREATE TABLE test (a int, b int)").run(
                validateAfter=True)
            self.fail("CREATE TABLE succeeded from master, when expecting "
                      "stop limit error on segment.")
        except ExecutionError:
            logger.info("CREATE TABLE failed from master, as expected.")
        # Reduce xid_stop_limit as per the standard procedure.
        self._reduce_stop_limit_guc(primary, new_limit)
        # Vacuum freezing newdb should be suffice to recover.
        PSQL(sql_cmd="VACUUM FREEZE", dbname="newdb",
             out_file="vacuum_newdb_wl.out").run(validateAfter=True)
        # Ensure that utility connections to the segment are successful.
        sql = "SELECT age(datfrozenxid) FROM pg_database WHERE datname='newdb'"
        with dbconn.connect(dburl, utility=True) as conn:
            age_newdb = int(dbconn.execSQLForSingleton(conn, sql))
        self.assertTrue(age_newdb > 0)
        # Verify SQL commands from master are successful.
        self._basic_sanity_check("clean")
        self._restore_stop_limit_guc(primary.datadir)
        # Verify SQL commands after restoring xid_stop_limit GUC.
        self._basic_sanity_check("clean")
        PSQL.drop_database(dbname="newdb")

    def template0_wrap_around_on_segment(self, primary):
        """
        Same as template0_wrap_around, but on segment.
        """
        logger.info("template0_wrap_around_on_segment: dbid(%d) %s:%d'" %
                    (primary.dbid, primary.hostname, primary.port))
        self._raise_template0_age(self.WRAP_LIMIT, primary)
        PSQL(sql_cmd="CREATE DATABASE newdb TEMPLATE template0").run(
            validateAfter=True)
        sql = "SELECT age(datfrozenxid) FROM pg_database WHERE datname='newdb'"
        # Verify that age of newdb on the segment is negative.
        dburl = dbconn.DbURL(hostname=primary.hostname, port=primary.port)
        with dbconn.connect(dburl, utility=True) as conn:
            age_newdb = int(dbconn.execSQLForSingleton(conn, sql))
        self.assertTrue(age_newdb < 0)
        # Reset newdb age so as to recover from wrap around.
        self._reset_age("newdb", primary)
        # Verify that normal operations can be performed on newdb whose age was
        # reset to a correct value.
        self._basic_sanity_check("clean", {"dbname":"newdb"})
        # Verify that age of newdb on the segment is valid.
        with dbconn.connect(dburl, utility=True) as conn:
            age_newdb = int(dbconn.execSQLForSingleton(conn, sql))
        self.assertTrue(age_newdb > 0)
        PSQL.drop_database(dbname="newdb")
