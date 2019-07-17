import unittest

from pygresql import pg

from gppylib.db import dbconn


class ConnectTestCase(unittest.TestCase):
    """A test case for dbconn.connect()."""

    def setUp(self):
        # Connect to the database pointed to by PGHOST et al.
        self.url = dbconn.DbURL()

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
