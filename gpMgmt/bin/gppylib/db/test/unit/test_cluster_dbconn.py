import unittest

from pygresql import pg

from gppylib.db import dbconn


class ConnectTestCase(unittest.TestCase):
    """A test case for dbconn.connect()."""

    @classmethod
    def setUpClass(cls):
        # Connect to the database pointed to by PGHOST et al.
        with dbconn.connect(dbconn.DbURL()) as conn:
            # using the pg.DB connection so that each SQL is done as a single
            # transaction
            db = pg.DB(conn)
            test_database_name = "gpdb_test_database"
            db.query("DROP DATABASE IF EXISTS %s" % test_database_name)
            db.query("CREATE DATABASE %s" % test_database_name)
            cls.url = dbconn.DbURL(dbname=test_database_name)

    def _raise_warning(self, db, msg):
        """Raises a WARNING message on the given pg.DB."""
        db.query("""
            DO LANGUAGE plpgsql $$
            BEGIN
                RAISE WARNING '{msg}';
            END
            $$
        """.format(msg=msg))

    def test_warnings_are_normally_suppressed(self):
        warning = "this is my warning message"

        with dbconn.connect(self.url) as conn:
            # Wrap our connection in pg.DB() so we can get at the underlying
            # notices. (This isn't available in the standard DB-API.)
            db = pg.DB(conn)
            self._raise_warning(db, warning)
            notices = db.notices()

        self.assertEqual(notices, []) # we expect no notices

    def test_verbose_mode_allows_warnings_to_be_sent_to_the_client(self):
        warning = "this is my warning message"

        with dbconn.connect(self.url, verbose=True) as conn:
            db = pg.DB(conn)
            self._raise_warning(db, warning)
            notices = db.notices()

        for notice in notices:
            if warning in notice:
                return # found it!

        self.fail("Didn't find expected notice '{}' in {!r}".format(
            warning, notices
        ))

    def test_client_encoding_can_be_set(self):
        encoding = 'SQL_ASCII'

        with dbconn.connect(self.url, encoding=encoding) as conn:
            actual = dbconn.execSQLForSingleton(conn, 'SHOW client_encoding')

        self.assertEqual(actual, encoding)

    def test_secure_search_path_set(self):

        with dbconn.connect(self.url) as conn:
            result = dbconn.execSQLForSingleton(conn, "SELECT setting FROM pg_settings WHERE name='search_path'")

        self.assertEqual(result, '')

    def test_secure_search_path_not_set(self):

        with dbconn.connect(self.url, unsetSearchPath=False) as conn:
            result = dbconn.execSQLForSingleton(conn, "SELECT setting FROM pg_settings WHERE name='search_path'")

        self.assertEqual(result, '"$user", public')

    def test_search_path_cve_2018_1058(self):
        with dbconn.connect(self.url) as conn:
            dbconn.execSQL(conn, "CREATE TABLE public.Names (name VARCHAR(255))")
            dbconn.execSQL(conn, "INSERT INTO public.Names VALUES ('AAA')")

            dbconn.execSQL(conn, "CREATE FUNCTION public.lower(VARCHAR) RETURNS text AS $$ "
                                 "  SELECT 'Alice was here: ' || $1;"
                                 "$$ LANGUAGE SQL IMMUTABLE;")

            name = dbconn.execSQLForSingleton(conn, "SELECT lower(name) FROM public.Names")
            self.assertEqual(name, 'aaa')

    def test_no_transaction_after_connect(self):
        with dbconn.connect(self.url) as conn:
            db = pg.DB(conn)
            # this would fail if we were in a transaction DROP DATABASE cannot
            # run inside a transaction block
            db.query("DROP DATABASE IF EXISTS some_nonexistent_database")


if __name__ == '__main__':
    unittest.main()
