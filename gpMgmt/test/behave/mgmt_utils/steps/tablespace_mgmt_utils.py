import shutil
import tempfile

from behave import given, when, then
from pygresql import pg

from gppylib.db import dbconn


class Tablespace:
    def __init__(self, name):
        self.name = name
        self.path = tempfile.mkdtemp()
        self.dbname = 'tablespace_db_%s' % name
        self.table_counter = 0
        self.initial_data = None

        with dbconn.connect(dbconn.DbURL()) as conn:
            db = pg.DB(conn)
            db.query("CREATE TABLESPACE %s LOCATION '%s'" % (self.name, self.path))
            db.query("CREATE DATABASE %s TABLESPACE %s" % (self.dbname, self.name))

        with dbconn.connect(dbconn.DbURL(dbname=self.dbname)) as conn:
            db = pg.DB(conn)
            db.query("CREATE TABLE tbl (i int) DISTRIBUTED RANDOMLY")
            db.query("INSERT INTO tbl VALUES (GENERATE_SERIES(0, 25))")
            # save the distributed data for later verification
            self.initial_data = db.query("SELECT gp_segment_id, i FROM tbl").getresult()

    def cleanup(self):
        with dbconn.connect(dbconn.DbURL(dbname="postgres")) as conn:
            db = pg.DB(conn)
            db.query("DROP DATABASE IF EXISTS %s" % self.dbname)
            db.query("DROP TABLESPACE IF EXISTS %s" % self.name)

            # Without synchronous_commit = 'remote_apply' introduced in 9.6, there
            # is no guarantee that the mirrors have removed their tablespace
            # directories by the time the DROP TABLESPACE command returns.
            # We need those directories to no longer be in use by the mirrors
            # before removing them below.
            _checkpoint_and_wait_for_replication_replay(db)

        shutil.rmtree(self.path)

    def verify(self):
        """
        Verify tablespace functionality by ensuring the tablespace can be
        written to, read from, and the initial data is still correctly
        distributed.
        """
        with dbconn.connect(dbconn.DbURL(dbname=self.dbname)) as conn:
            db = pg.DB(conn)
            data = db.query("SELECT gp_segment_id, i FROM tbl").getresult()

            # verify that we can still write to the tablespace
            self.table_counter += 1
            db.query("CREATE TABLE tbl_%s (i int) DISTRIBUTED RANDOMLY" % self.table_counter)
            db.query("INSERT INTO tbl_%s VALUES (GENERATE_SERIES(0, 25))" % self.table_counter)

        if sorted(data) != sorted(self.initial_data):
            raise Exception("Tablespace data is not identically distributed. Expected:\n%r\n but found:\n%r" % (
                sorted(self.initial_data), sorted(data)))


def _checkpoint_and_wait_for_replication_replay(db):
    """
    Taken from src/test/walrep/sql/missing_xlog.sql
    """
    db.query("""
-- checkpoint to ensure clean xlog replication before bring down mirror
create or replace function checkpoint_and_wait_for_replication_replay (retries int) returns bool as
$$
declare
	i int;
	checkpoint_locs pg_lsn[];
	replay_locs pg_lsn[];
	failed_for_segment text[];
	r record;
	all_caught_up bool;
begin
	i := 0;

	-- Issue a checkpoint.
	checkpoint;

	-- Get the WAL positions after the checkpoint records on every segment.
	for r in select gp_segment_id, pg_current_xlog_location() as loc from gp_dist_random('gp_id') loop
		checkpoint_locs[r.gp_segment_id] = r.loc;
	end loop;
	-- and the QD, too.
	checkpoint_locs[-1] = pg_current_xlog_location();

	-- Force some WAL activity, to nudge the mirrors to replay past the
	-- checkpoint location. There are some cases where a non-transactional
	-- WAL record is created right after the checkpoint record, which
	-- doesn't get replayed on the mirror until something else forces it
	-- out.
	drop table if exists dummy;
	create temp table dummy (id int4) distributed randomly;

	-- Wait until all mirrors have replayed up to the location we
	-- memorized above.
	loop
		all_caught_up = true;
		for r in select gp_segment_id, replay_location as loc from gp_stat_replication loop
			replay_locs[r.gp_segment_id] = r.loc;
			if r.loc < checkpoint_locs[r.gp_segment_id] then
				all_caught_up = false;
				failed_for_segment[r.gp_segment_id] = 1;
			else
				failed_for_segment[r.gp_segment_id] = 0;
			end if;
		end loop;

		if all_caught_up then
			return true;
		end if;

		if i >= retries then
			RAISE INFO 'checkpoint_locs:    %', checkpoint_locs;
			RAISE INFO 'replay_locs:        %', replay_locs;
			RAISE INFO 'failed_for_segment: %', failed_for_segment;
			return false;
		end if;
		perform pg_sleep(0.1);
		i := i + 1;
	end loop;
end;
$$ language plpgsql;

SELECT checkpoint_and_wait_for_replication_replay(0);
DROP FUNCTION checkpoint_and_wait_for_replication_replay(int);
    """)


@given('a tablespace is created with data')
def impl(context):
    _create_tablespace_with_data(context, "outerspace")


@given('another tablespace is created with data')
def impl(context):
    _create_tablespace_with_data(context, "myspace")


def _create_tablespace_with_data(context, name):
    if 'tablespaces' not in context:
        context.tablespaces = {}
    context.tablespaces[name] = Tablespace(name)


@then('the tablespace is valid')
def impl(context):
    context.tablespaces["outerspace"].verify()


@then('the other tablespace is valid')
def impl(context):
    context.tablespaces["myspace"].verify()
