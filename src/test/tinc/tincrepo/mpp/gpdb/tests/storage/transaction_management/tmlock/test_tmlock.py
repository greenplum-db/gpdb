from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from mpp.gpdb.tests.storage.lib import Database

import os
import subprocess

class test_tmlock(MPPTestCase):

    @classmethod
    def setUpClass(cls):
        db = Database()
        db.setupDatabase()
        os.environ['PGDATABASE'] = os.environ['USER']

    def test_tmlock(self):
        # Summary:
	# The problem occurs because some backend acquires a lock, does not
        # release it and continues to listen for subsequent statements from
        # the application. As a result, newer backends wanting this lock
        # get stuck at the lock acquire due to flawed DTM recovery flow.
        #
        # 1. gpstart
        # 2. pg_ctl -D $MASTER_DATA_DIRECTORY restart
        # 3. gpfaultinjector -f exec_simple_query_end_command -H ALL -r primary
        # 4. psql
        # 5. once 4. succeeded, connect anothe psql, try some DDL like create table.
        # 6. check the master log if it reports 'DTM initialization, caught exception'

        PSQL.run_sql_command('DROP TABLE IF EXISTS tmlock_1')
        PSQL.run_sql_command('DROP TABLE IF EXISTS tmlock_2')

        # Reset fault injection if any.
        gpfaultinjector = Command('fault injector',
                                  'source $GPHOME/greenplum_path.sh; '
                                  'gpfaultinjector -f exec_simple_query_end_command '
                                  '-y reset -H ALL -r primary')
        gpfaultinjector.run()

        # Restart master only.  This will cause DTM recovery.
        pg_ctl = Command('restart master only',
                         'source $GPHOME/greenplum_path.sh; '
                         'pg_ctl -D $MASTER_DATA_DIRECTORY restart')

        pg_ctl.run()
        results = pg_ctl.get_results()
        assert results.rc == 0

        # Since pg_ctl can return before the system can accepts
        # new connections, wait for that.  Note this needs to be
        # done by utility mode which doesn't run DTM recovery.
        while True:
            cmd = Command('psql ping',
                          'PGOPTIONS="-c gp_session_role=utility" '
                          'psql -c "SELECT 1"')
            cmd.run()
            if cmd.get_results().rc == 0:
                break

        # Inject a new fault at the end of exec_simple_query in segment.
        # This should not fail because we've just reset it.
        # This will make segment error out when DTM recovery dispatches
        # pg_prepared_xacts query to look for in-doubt transaction.
        gpfaultinjector = Command('fault injector',
                                  'source $GPHOME/greenplum_path.sh; '
                                  'gpfaultinjector -f exec_simple_query_end_command '
                                  '-y error -H ALL -r primary')

        gpfaultinjector.run()
        results = gpfaultinjector.get_results()
        assert results.rc == 0

        # Connect a psql client to the master, causing DTM recovery.
        # Due to the fault above, this will retry.
        psql1 = subprocess.Popen(['psql', '-c', 'SET client_min_messages TO WARNING;CREATE TABLE tmlock_1(a int)'],
                                stdout=subprocess.PIPE)

        # While the first client is still connecting after the retry,
        # another client can proceed and should not be blocked.
        psql2 = subprocess.Popen(['psql', '-c', 'SET client_min_messages TO WARNING;CREATE TABLE tmlock_2(a int)'])

        psql2.wait()
        assert psql2.returncode == 0

        # The fist client should have finished its work successfully.
        stdout, stderr = psql1.communicate()
        assert 'CREATE TABLE' in stdout

        DTM_retried = False
        # Check for the log
        MASTER_DATA_DIRECTORY = os.environ['MASTER_DATA_DIRECTORY']
        latest_csvpath = self.find_latest_logfile(MASTER_DATA_DIRECTORY)
        for line in open(latest_csvpath):
            if 'DTM initialization, caught exception' in line:
                DTM_retried = True
                break

        # If it didn't retry, this test is nonesense.
        assert DTM_retried

    def find_latest_logfile(self, dirname):
        """Finds the latest csv log filename in an absolute path"""

        pg_logdir = os.path.join(dirname, 'pg_log')
        csv_files = [fn for fn in os.listdir(pg_logdir)
                            if fn.endswith('.csv')]
        csv_files.sort()
        return os.path.join(pg_logdir, csv_files[-1])
