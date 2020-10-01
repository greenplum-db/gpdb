import unittest
from contextlib import closing

from gppylib.db import dbconn


class ConnectTestCase(unittest.TestCase):
    """A test case for dbconn.connect()."""

    @classmethod
    def setUpClass(cls):
        # Connect to the database pointed to by PGHOST et al.
        conn = dbconn.connect(dbconn.DbURL())
        # don't use "with" block so that each exec is a commited transaction
        test_database_name = "gpdb_test_database"
        dbconn.execSQL(conn, "DROP DATABASE IF EXISTS %s" % test_database_name)
        dbconn.execSQL(conn, "CREATE DATABASE %s" % test_database_name)
        conn.close()
        cls.url = dbconn.DbURL(dbname=test_database_name)

    def _raise_warning(self, conn, msg):
        """Raises a WARNING message on the given pg.DB."""
        dbconn.execSQL(conn, """
            DO LANGUAGE plpgsql $$
            BEGIN
                RAISE WARNING '{msg}';
            END
            $$
        """.format(msg=msg))

    def test_warnings_are_normally_suppressed(self):
        warning = "this is my warning message"

        with closing(dbconn.connect(self.url)) as conn:
            self._raise_warning(conn, warning)
            notices = conn.notices()

        self.assertEqual(notices, []) # we expect no notices

    def test_verbose_mode_allows_warnings_to_be_sent_to_the_client(self):
        warning = "this is my warning message"

        with closing(dbconn.connect(self.url, verbose=True)) as conn:
            self._raise_warning(conn, warning)
            notices = conn.notices()


        for notice in notices:
            if warning in notice.message:
                return # found it!

        self.fail("Didn't find expected notice '{}' in {!r}".format(
            warning, notices
        ))

    def test_client_encoding_can_be_set(self):
        encoding = 'SQL_ASCII'

        with closing(dbconn.connect(self.url, encoding=encoding)) as conn:
            actual = dbconn.querySingleton(conn, 'SHOW client_encoding')

        self.assertEqual(actual, encoding)

    def test_secure_search_path_set(self):

        with dbconn.connect(self.url) as conn:
            result = dbconn.querySingleton(conn, "SELECT setting FROM pg_settings WHERE name='search_path'")

        self.assertEqual(result, '')

    def test_secure_search_path_not_set(self):

        with dbconn.connect(self.url, unsetSearchPath=False) as conn:
            result = dbconn.querySingleton(conn, "SELECT setting FROM pg_settings WHERE name='search_path'")

        self.assertEqual(result, '"$user", public')

    def test_search_path_cve_2018_1058(self):
        with dbconn.connect(self.url) as conn:
            dbconn.execSQL(conn, "CREATE TABLE public.Names (name VARCHAR(255))")
            dbconn.execSQL(conn, "INSERT INTO public.Names VALUES ('AAA')")

            dbconn.execSQL(conn, "CREATE FUNCTION public.lower(VARCHAR) RETURNS text AS $$ "
                                 "  SELECT 'Alice was here: ' || $1;"
                                 "$$ LANGUAGE SQL IMMUTABLE;")

            name = dbconn.querySingleton(conn, "SELECT lower(name) FROM public.Names")
            self.assertEqual(name, 'aaa')

    def test_no_transaction_after_connect(self):
        conn = dbconn.connect(self.url)
        # this would fail if we were in a transaction DROP DATABASE cannot
        # run inside a transaction block
        dbconn.execSQL(conn,"DROP DATABASE IF EXISTS some_nonexistent_database")
        conn.close()



if __name__ == '__main__':
    unittest.main()
