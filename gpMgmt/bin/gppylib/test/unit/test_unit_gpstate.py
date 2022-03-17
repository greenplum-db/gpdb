import unittest
import mock
import pgdb
import tempfile

from gppylib import gparray
from .gp_unittest import GpTestCase
from gppylib.programs.clsSystemState import *

#
# Creation helpers for gparray.Segment.
#

def create_segment(**kwargs):
    """
    Takes the same arguments as gparray.Segment's constructor, but with friendly
    defaults to minimize typing.
    """
    content        = kwargs.get('content', 0)
    role           = kwargs.get('role', gparray.ROLE_PRIMARY)
    preferred_role = kwargs.get('preferred_role', role)
    dbid           = kwargs.get('dbid', 0)
    mode           = kwargs.get('mode', gparray.MODE_SYNCHRONIZED)
    status         = kwargs.get('status', gparray.STATUS_UP)
    hostname       = kwargs.get('hostname', 'localhost')
    address        = kwargs.get('address', 'localhost')
    port           = kwargs.get('port', 15432)
    datadir        = kwargs.get('datadir', '/tmp/')

    return gparray.Segment(content, preferred_role, dbid, role, mode, status,
                           hostname, address, port, datadir)

def create_primary(**kwargs):
    """Like create_segment() but with the role overridden to ROLE_PRIMARY."""
    kwargs['role'] = gparray.ROLE_PRIMARY
    return create_segment(**kwargs)

def create_mirror(**kwargs):
    """Like create_segment() but with the role overridden to ROLE_MIRROR."""
    kwargs['role'] = gparray.ROLE_MIRROR
    return create_segment(**kwargs)

class RecoveryProgressTestCase(unittest.TestCase):
    """
        A test case for GpSystemStateProgram.parseRecoveryProgressData().
    """

    def setUp(self):

        self.primary1 = create_primary(dbid=1)
        self.primary2 = create_primary(dbid=2)
        self.primary3 = create_primary(dbid=3)

        self.data = GpStateData()
        self.data.beginSegment(self.primary1)
        self.data.beginSegment(self.primary2)
        self.data.beginSegment(self.primary3)

        self.gpArrayMock = mock.MagicMock(spec=gparray.GpArray)
        self.gpArrayMock.getSegDbList.return_value = [self.primary1, self.primary2, self.primary3]

    def check_recovery_fields(self, segment, type, completed, total, percentage):
        self.assertEqual(type, self.data.getStrValue(segment, VALUE_RECOVERY_TYPE))
        self.assertEqual(completed, self.data.getStrValue(segment, VALUE_RECOVERY_COMPLETED_BYTES))
        self.assertEqual(total, self.data.getStrValue(segment, VALUE_RECOVERY_TOTAL_BYTES))
        self.assertEqual(percentage, self.data.getStrValue(segment, VALUE_RECOVERY_PERCENTAGE))

    def test_parse_recovery_progress_data_returns_empty_when_file_does_not_exist(self):
        self.assertEqual([], GpSystemStateProgram._parse_recovery_progress_data(self.data, '/file/does/not/exist', self.gpArrayMock))

        self.check_recovery_fields(self.primary1, '', '', '', '')
        self.check_recovery_fields(self.primary2, '', '', '', '')
        self.check_recovery_fields(self.primary3, '', '', '', '')

    def test_parse_recovery_progress_data_adds_recovery_progress_data_during_recovery(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write("full:1: 1164848/1371715 kB (84%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)".encode("utf-8"))
            f.flush()
            self.assertEqual([self.primary1], GpSystemStateProgram._parse_recovery_progress_data(self.data, f.name, self.gpArrayMock))

            self.check_recovery_fields(self.primary1, 'full', '1164848', '1371715', '84%')
            self.check_recovery_fields(self.primary2, '', '', '', '')
            self.check_recovery_fields(self.primary3, '', '', '', '')

    def test_parse_recovery_progress_data_adds_recovery_progress_data_during_multiple_recoveries(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write("full:1: 1164848/1371715 kB (0%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("incremental:2: 1171384/1371875 kB (85%)anything can appear here".encode('utf-8'))
            f.flush()
            self.assertEqual([self.primary1, self.primary2], GpSystemStateProgram._parse_recovery_progress_data(self.data, f.name, self.gpArrayMock))

            self.check_recovery_fields(self.primary1,'full', '1164848', '1371715', '0%')
            self.check_recovery_fields(self.primary2, 'incremental', '1171384', '1371875', '85%')
            self.check_recovery_fields(self.primary3, '', '', '', '')

    def test_parse_recovery_progress_data_doesnt_adds_recovery_progress_data_only_for_completed_recoveries(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write("full:1: 1164848/1371715 kB (84%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:2: 1164848/1371715 kB (100%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1164848/1371715 kB (100#), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1164848/1371715 kB 84%, 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1164848/1371715 kB (100), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: /1371715 kB (100%, 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1/1371715 KB (100%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1/1371715 kB 100%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1/1371715 kB (100%, 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1/1371715 MB (100%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1/1371715 B (100%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1164848/1371715 kB (84%%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1164848/1371715 kB (84%a), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1164848/1371715 kB (a84%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("full:3: 1164848/1371715 kB ((84%), 0/1 tablespace (...t1/demoDataDir0/base/16384/40962)\n".encode("utf-8"))
            f.write("incremental:2: pg_rewind: done.\n".encode('utf-8'))
            f.write("incremental:3: pg_rewind: Error \n".encode('utf-8'))
            f.write("incremental:3: 1171384/1371875 kB (a8ab5%)\n".encode('utf-8'))
            f.write("incremental:3: 1171384/1371875 kB ((85%)\n".encode('utf-8'))
            f.write("incremental:3: 1171384/1371875 kB (85%%1))\n".encode('utf-8'))
            f.write("incremental:3: 1171384/1371875 kB (foo%))\n".encode('utf-8'))
            f.flush()
            self.assertEqual([self.primary1, self.primary2], GpSystemStateProgram._parse_recovery_progress_data(self.data, f.name, self.gpArrayMock))

            self.check_recovery_fields(self.primary1,'full', '1164848', '1371715', '84%')
            self.check_recovery_fields(self.primary2,'full', '1164848', '1371715', '100%')
            self.check_recovery_fields(self.primary3, '', '', '', '')


class ReplicationInfoTestCase(unittest.TestCase):
    """
    A test case for GpSystemStateProgram._add_replication_info().
    """

    def setUp(self):
        """
        Every test starts with a primary, a mirror, and a GpStateData instance
        that is ready to record information for the mirror segment. Feel free to
        ignore these if they are not helpful.
        """
        self.primary = create_primary(dbid=1)
        self.mirror = create_mirror(dbid=2)

        self.data = GpStateData()
        self.data.beginSegment(self.primary)
        self.data.beginSegment(self.mirror)

        # Implementation detail for the mock_pg_[table] functions. _pg_rows maps
        # a query fragment to the set of rows that should be returned from
        # dbconn.query() for a matching query. Reset this setup for every
        # test.
        self._pg_rows = {}

    def _get_rows_for_query(self, *args):
        """
        Mock implementation of dbconn.query() for these unit tests. Don't use
        this directly; use one of the mock_pg_xxx() helpers.
        """
        query = args[1]
        rows = None

        # Try to match the query() query against one of our stored fragments.
        # If there is an overlap in fragments, we can get wrong results.
        # Make sure none of the fragments in a test is a subset of another fragment.
        for fragment in self._pg_rows:
            if fragment in query:
                rows = self._pg_rows[fragment]
                break

        if rows is None:
            self.fail(
                'Expected one of the query fragments {!r} to be in the query {!r}.'.format(
                    list(self._pg_rows.keys()), query
                )
            )

        # Mock out the cursor's rowcount, fetchall(), and fetchone().
        # fetchone.side_effect conveniently lets us return one row from the list
        # at a time.
        cursor = mock.MagicMock()
        cursor.rowcount = len(rows)
        cursor.fetchall.return_value = rows
        cursor.fetchone.side_effect = rows
        return cursor

    def mock_pg_stat_replication(self, mock_query, rows):
        self._pg_rows['pg_stat_replication'] = rows
        mock_query.side_effect = self._get_rows_for_query

    def mock_pg_current_wal_lsn(self, mock_query, rows):
        self._pg_rows['SELECT pg_current_wal_lsn'] = rows
        mock_query.side_effect = self._get_rows_for_query

    def stub_replication_entry(self, **kwargs):
        # The row returned here must match the order and contents expected by
        # the pg_stat_replication query performed in _add_replication_info().
        # It's put here so there is just a single place to fix the tests if that
        # query changes.
        return (
            kwargs.get('application_name', 'gp_walreceiver'),
            kwargs.get('state', 'streaming'),
            kwargs.get('sent_lsn', '0/0'),
            kwargs.get('flush_lsn', '0/0'),
            kwargs.get('flush_left', 0),
            kwargs.get('replay_lsn', '0/0'),
            kwargs.get('replay_left', 0),
            kwargs.get('backend_start', None),
            kwargs.get('sent_left', '0')
        )

    def mock_pg_stat_activity(self, mock_query, rows):
        self._pg_rows['pg_stat_activity'] = rows
        mock_query.side_effect = self._get_rows_for_query

    def stub_activity_entry(self, **kwargs):
        # The row returned here must match the order and contents expected by
        # the pg_stat_activity query performed in _add_replication_info(); see
        # stub_replication_entry() above.
        return (
            kwargs.get('backend_start', None),
        )

    def test_add_replication_info_adds_unknowns_if_primary_is_down(self):
        self.primary.status = gparray.STATUS_DOWN
        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LSN))

    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_adds_unknowns_if_connection_cannot_be_made(self, mock_connect):
        # Simulate a connection failure in dbconn.connect().
        mock_connect.side_effect = pgdb.InternalError('connection failure forced by unit test')
        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LSN))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_adds_unknowns_if_pg_stat_replication_has_no_entries(self, mock_connect, mock_query):
        self.mock_pg_stat_replication(mock_query, [])
        self.mock_pg_current_wal_lsn(mock_query, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LSN))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_adds_unknowns_if_pg_stat_replication_has_too_many_mirrors(self, mock_connect, mock_query):
        self.mock_pg_stat_replication(mock_query, [
            self.stub_replication_entry(application_name='gp_walreceiver'),
            self.stub_replication_entry(application_name='gp_walreceiver'),
        ])
        self.mock_pg_current_wal_lsn(mock_query, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LSN))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_populates_correctly_from_pg_stat_replication(self, mock_connect, mock_query):
        # Set up the row definition.
        self.mock_pg_stat_replication(mock_query, [
            self.stub_replication_entry(
                sent_lsn='0/1000',
                flush_lsn='0/0800',
                flush_left=2048,
                replay_lsn='0/0000',
                replay_left=4096,
                sent_left=1024,
            )
        ])
        self.mock_pg_current_wal_lsn(mock_query, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('1024', self.data.getStrValue(self.primary, VALUE__REPL_SENT_LEFT))

        self.assertEqual('Streaming', self.data.getStrValue(self.mirror, VALUE__MIRROR_STATUS))
        self.assertEqual('0/1000', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LSN))
        self.assertEqual('0/0800', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LSN))
        self.assertEqual('0/0000', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LSN))
        self.assertEqual('2048', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LEFT))
        self.assertEqual('4096', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LEFT))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_omits_lag_info_if_WAL_locations_are_identical(self, mock_connect, mock_query):
        # Set up the row definition.
        self.mock_pg_stat_replication(mock_query, [
            self.stub_replication_entry(
                sent_lsn='0/1000',
                flush_lsn='0/1000',
                flush_left=0,
                replay_lsn='0/1000',
                replay_left=0,
                sent_left=0,
            )
        ])
        self.mock_pg_current_wal_lsn(mock_query, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('0', self.data.getStrValue(self.primary, VALUE__REPL_SENT_LEFT))

        self.assertEqual('0/1000', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LSN))
        self.assertEqual('0/1000', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LSN))
        self.assertEqual('0/1000', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LSN))
        self.assertEqual('0', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LEFT))
        self.assertEqual('0', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LEFT))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_adds_unknowns_if_pg_stat_replication_is_incomplete(self, mock_connect, mock_query):
        # Set up the row definition.
        self.mock_pg_stat_replication(mock_query, [
            self.stub_replication_entry(
                sent_lsn=None,
                flush_lsn=None,
                flush_left=None,
                replay_lsn=None,
                replay_left=None,
                sent_left=None,
            )
        ])
        self.mock_pg_current_wal_lsn(mock_query, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Unknown', self.data.getStrValue(self.primary, VALUE__REPL_SENT_LEFT))

        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LSN))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LEFT))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LEFT))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_closes_connections(self, mock_connect, mock_query):
        self.mock_pg_stat_replication(mock_query, [])
        self.mock_pg_current_wal_lsn(mock_query, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        assert mock_connect.return_value.close.called

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_displays_full_backup_state_on_primary(self, mock_connect, mock_query):
        self.mock_pg_stat_replication(mock_query, [
            self.stub_replication_entry(
                application_name='some_backup_utility',
                state='backup',
                sent_lsn='0/0', # this matches the real-world behavior but is unimportant to the test
                flush_lsn=None,
                flush_left=None,
                replay_lsn=None,
                replay_left=None,
            )
        ])
        self.mock_pg_current_wal_lsn(mock_query, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Copying files from primary', self.data.getStrValue(self.primary, VALUE__MIRROR_STATUS))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_displays_full_backup_start_timestamp_on_primary(self, mock_connect, mock_query):
        self.mock_pg_stat_replication(mock_query, [
            self.stub_replication_entry(
                application_name='some_backup_utility',
                state='backup',
                sent_lsn='0/0', # this matches the real-world behavior but is unimportant to the test
                flush_lsn=None,
                flush_left=None,
                replay_lsn=None,
                replay_left=None,
                backend_start='1970-01-01 00:00:00.000000-00'
            )
        ])
        self.mock_pg_current_wal_lsn(mock_query, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('1970-01-01 00:00:00.000000-00', self.data.getStrValue(self.primary, VALUE__MIRROR_RECOVERY_START))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_displays_simultaneous_backup_and_replication(self, mock_connect, mock_query):
        self.mock_pg_stat_replication(mock_query, [
            self.stub_replication_entry(
                application_name='some_backup_utility',
                state='backup',
                sent_lsn='0/0', # this matches the real-world behavior but is unimportant to the test
                flush_lsn=None,
                flush_left=None,
                replay_lsn=None,
                replay_left=None,
            ),
            self.stub_replication_entry(
                state='streaming',
            ),
        ])
        self.mock_pg_current_wal_lsn(mock_query, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Copying files from primary', self.data.getStrValue(self.primary, VALUE__MIRROR_STATUS))
        self.assertEqual('Streaming', self.data.getStrValue(self.mirror, VALUE__MIRROR_STATUS))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_displays_status_when_pg_rewind_is_active_and_mirror_is_down(self, mock_connect, mock_query):
        self.mock_pg_stat_replication(mock_query, [])
        self.mock_pg_current_wal_lsn(mock_query, [])
        self.mirror.status = gparray.STATUS_DOWN
        self.mock_pg_stat_activity(mock_query, [self.stub_activity_entry()])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Rewinding history to match primary timeline', self.data.getStrValue(self.primary, VALUE__MIRROR_STATUS))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_does_not_update_mirror_status_when_mirror_is_down_and_there_is_no_recovery_underway(self, mock_connect, mock_query):
        self.mock_pg_stat_replication(mock_query, [])
        self.mock_pg_current_wal_lsn(mock_query, [])
        self.mirror.status = gparray.STATUS_DOWN
        self.mock_pg_stat_activity(mock_query, [])

        self.data.switchSegment(self.primary)
        self.data.addValue(VALUE__MIRROR_STATUS, 'previous value')
        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        # The mirror status should not have been touched in this case.
        self.assertEqual('previous value', self.data.getStrValue(self.primary, VALUE__MIRROR_STATUS))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_displays_start_time_when_pg_rewind_is_active_and_mirror_is_down(self, mock_connect, mock_query):
        mock_date = '1970-01-01 00:00:00.000000-00'
        self.mock_pg_stat_replication(mock_query, [self.stub_replication_entry()])
        self.mock_pg_current_wal_lsn(mock_query, [])
        self.mock_pg_stat_activity(mock_query, [self.stub_activity_entry(backend_start=mock_date)])
        self.mirror.status = gparray.STATUS_DOWN

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual(mock_date, self.data.getStrValue(self.primary, VALUE__MIRROR_RECOVERY_START))

    @mock.patch('gppylib.db.dbconn.query', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_does_not_query_pg_stat_activity_when_mirror_is_up(self, mock_connect, mock_query):
        self.mock_pg_stat_replication(mock_query, [])
        self.mock_pg_current_wal_lsn(mock_query, [])
        self.mock_pg_stat_activity(mock_query, [self.stub_activity_entry()])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        for call in mock_query.mock_calls:
            args = call[1]  # positional args are the second item in the tuple
            query = args[1] # query is the second argument to query()
            self.assertFalse('pg_stat_activity' in query)

    def test_set_mirror_replication_values_complains_about_incorrect_kwargs(self):
        with self.assertRaises(TypeError):
            GpSystemStateProgram._set_mirror_replication_values(self.data, self.mirror, badarg=1)

class SegmentsReqAttentionTestCase(GpTestCase):

    def setUp(self):
        coordinator = create_segment(content=-1, dbid=0)
        self.primary1 = create_primary(content=0, dbid=1)
        self.primary2 = create_primary(content=1, dbid=2)
        mirror1 = create_mirror(content=0, dbid=3)
        mirror2 = create_mirror(content=1, dbid=4)
        self.gpArray = gparray.GpArray([coordinator, self.primary1, self.primary2, mirror1, mirror2])

        self.data = GpStateData()
        self.data.beginSegment(self.primary1)
        self.data.beginSegment(self.primary2)

        self.apply_patches([mock.patch('gppylib.db.dbconn.connect', autospec=True)])

    def test_WalSyncRemainingBytes_multiple_segments_both_catchup(self):
        m = mock.Mock()
        m.fetchall.side_effect = [[[100, 'async']], [[10, 'async']]]
        with mock.patch('gppylib.db.dbconn.query', return_value=m) as mock_query:
            GpSystemStateProgram._get_unsync_segs_add_wal_remaining_bytes(self.data, self.gpArray)
            self.assertEqual(mock_query.call_count, 2)
            self.assertEqual('100', self.data.getStrValue(self.primary1, VALUE__REPL_SYNC_REMAINING_BYTES))
            self.assertEqual('10', self.data.getStrValue(self.primary2, VALUE__REPL_SYNC_REMAINING_BYTES))

    def test_WalSyncRemainingBytes_multiple_segments_one_sync(self):
        m = mock.Mock()
        m.fetchall.side_effect = [[[100, 'async']], [[10, 'sync']]]
        with mock.patch('gppylib.db.dbconn.query', return_value=m) as mock_query:
            GpSystemStateProgram._get_unsync_segs_add_wal_remaining_bytes(self.data, self.gpArray)
            self.assertEqual(mock_query.call_count, 2)
            self.assertEqual('100', self.data.getStrValue(self.primary1, VALUE__REPL_SYNC_REMAINING_BYTES))
            self.assertEqual('', self.data.getStrValue(self.primary2, VALUE__REPL_SYNC_REMAINING_BYTES))

    def test_WalSyncRemainingBytes_multiple_segments_one_down(self):
        m = mock.Mock()
        m.fetchall.side_effect = [[[100, 'async']], []]
        with mock.patch('gppylib.db.dbconn.query', return_value=m) as mock_query:
            GpSystemStateProgram._get_unsync_segs_add_wal_remaining_bytes(self.data, self.gpArray)
            self.assertEqual(mock_query.call_count, 2)
            self.assertEqual('100', self.data.getStrValue(self.primary1, VALUE__REPL_SYNC_REMAINING_BYTES))
            self.assertEqual('Unknown', self.data.getStrValue(self.primary2, VALUE__REPL_SYNC_REMAINING_BYTES))

class GpStateDataTestCase(unittest.TestCase):
    def test_switchSegment_sets_current_segment_correctly(self):
        data = GpStateData()
        primary = create_primary(dbid=1)
        mirror = create_mirror(dbid=2)

        data.beginSegment(primary)
        data.beginSegment(mirror)

        data.switchSegment(primary)
        data.addValue(VALUE__HOSTNAME, 'foo')
        data.addValue(VALUE__ADDRESS, 'bar')

        data.switchSegment(mirror)
        data.addValue(VALUE__DATADIR, 'baz')
        data.addValue(VALUE__PORT, 'abc')

        self.assertEqual('foo', data.getStrValue(primary, VALUE__HOSTNAME))
        self.assertEqual('bar', data.getStrValue(primary, VALUE__ADDRESS))
        self.assertEqual('baz', data.getStrValue(mirror, VALUE__DATADIR))
        self.assertEqual('abc', data.getStrValue(mirror, VALUE__PORT))

        # Make sure that neither the mirror nor the primary were accidentally
        # updated in lieu of the other.
        self.assertEqual('', data.getStrValue(mirror, VALUE__HOSTNAME))
        self.assertEqual('', data.getStrValue(primary, VALUE__DATADIR))
