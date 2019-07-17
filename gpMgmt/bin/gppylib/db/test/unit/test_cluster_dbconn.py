import unittest

from pygresql import pg

from gppylib.db import dbconn


class ConnectTestCase(unittest.TestCase):
    """A test case for dbconn.connect()."""

    def setUp(self):
        # Connect to the database pointed to by PGHOST et al.
        self.url = dbconn.DbURL()

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

        self.assertEqual(result, '"$user",public')

if __name__ == '__main__':
    unittest.main()
