import unittest
import mock

from pygresql import pgdb

from gppylib import gparray
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

    def mock_pg_stat_replication(self, mock_execSQL, rows):
        cursor = mock.MagicMock()
        mock_execSQL.return_value = cursor

        cursor.rowcount = len(rows)
        cursor.fetchall.return_value = rows

    def stub_replication_entry(self, **kwargs):
        # The row returned here must match the order and contents expected by
        # the pg_stat_replication query performed in _add_replication_info().
        # It's put here so there is just a single place to fix the tests if that
        # query changes.
        return (
            kwargs.get('application_name', 'gp_walreceiver'),
            kwargs.get('state', 'streaming'),
            kwargs.get('sent_location', '0/0'),
            kwargs.get('flush_location', '0/0'),
            kwargs.get('flush_left', 0),
            kwargs.get('replay_location', '0/0'),
            kwargs.get('replay_left', 0),
        )

    def test_add_replication_info_adds_unknowns_if_primary_is_down(self):
        self.primary.status = gparray.STATUS_DOWN
        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LOCATION))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LOCATION))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LOCATION))

    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_adds_unknowns_if_connection_cannot_be_made(self, mock_connect):
        # Simulate a connection failure in dbconn.connect().
        mock_connect.side_effect = pgdb.InternalError('connection failure forced by unit test')
        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LOCATION))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LOCATION))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LOCATION))

    @mock.patch('gppylib.db.dbconn.execSQL', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_adds_unknowns_if_pg_stat_replication_has_no_entries(self, mock_connect, mock_execSQL):
        self.mock_pg_stat_replication(mock_execSQL, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LOCATION))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LOCATION))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LOCATION))

    @mock.patch('gppylib.db.dbconn.execSQL', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_adds_unknowns_if_pg_stat_replication_has_too_many_mirrors(self, mock_connect, mock_execSQL):
        self.mock_pg_stat_replication(mock_execSQL, [
            self.stub_replication_entry(application_name='gp_walreceiver'),
            self.stub_replication_entry(application_name='gp_walreceiver'),
        ])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LOCATION))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LOCATION))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LOCATION))

    @mock.patch('gppylib.db.dbconn.execSQL', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_populates_correctly_from_pg_stat_replication(self, mock_connect, mock_execSQL):
        # Set up the row definition.
        self.mock_pg_stat_replication(mock_execSQL, [
            self.stub_replication_entry(
                sent_location='0/1000',
                flush_location='0/0800',
                flush_left=2048,
                replay_location='0/0000',
                replay_left=4096,
            )
        ])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Streaming', self.data.getStrValue(self.mirror, VALUE__MIRROR_STATUS))
        self.assertEqual('0/1000', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LOCATION))
        self.assertEqual('0/0800 (2048 bytes left)', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LOCATION))
        self.assertEqual('0/0000 (4096 bytes left)', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LOCATION))

    @mock.patch('gppylib.db.dbconn.execSQL', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_omits_lag_info_if_WAL_locations_are_identical(self, mock_connect, mock_execSQL):
        # Set up the row definition.
        self.mock_pg_stat_replication(mock_execSQL, [
            self.stub_replication_entry(
                sent_location='0/1000',
                flush_location='0/1000',
                flush_left=0,
                replay_location='0/1000',
                replay_left=0,
            )
        ])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('0/1000', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LOCATION))
        self.assertEqual('0/1000', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LOCATION))
        self.assertEqual('0/1000', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LOCATION))

    @mock.patch('gppylib.db.dbconn.execSQL', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_adds_unknowns_if_pg_stat_replication_is_incomplete(self, mock_connect, mock_execSQL):
        # Set up the row definition.
        self.mock_pg_stat_replication(mock_execSQL, [
            self.stub_replication_entry(
                sent_location=None,
                flush_location=None,
                flush_left=None,
                replay_location=None,
                replay_left=None,
            )
        ])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_SENT_LOCATION))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_FLUSH_LOCATION))
        self.assertEqual('Unknown', self.data.getStrValue(self.mirror, VALUE__REPL_REPLAY_LOCATION))

    @mock.patch('gppylib.db.dbconn.execSQL', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_closes_connections_and_cursors(self, mock_connect, mock_execSQL):
        self.mock_pg_stat_replication(mock_execSQL, [])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        assert mock_connect.return_value.close.called
        assert mock_execSQL.return_value.close.called

    @mock.patch('gppylib.db.dbconn.execSQL', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_displays_full_backup_state_on_primary(self, mock_connect, mock_execSQL):
        self.mock_pg_stat_replication(mock_execSQL, [
            self.stub_replication_entry(
                application_name='some_backup_utility',
                state='backup',
                sent_location='0/0', # this matches the real-world behavior but is unimportant to the test
                flush_location=None,
                flush_left=None,
                replay_location=None,
                replay_left=None,
            )
        ])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Copying files from primary', self.data.getStrValue(self.primary, VALUE__MIRROR_STATUS))

    @mock.patch('gppylib.db.dbconn.execSQL', autospec=True)
    @mock.patch('gppylib.db.dbconn.connect', autospec=True)
    def test_add_replication_info_displays_simultaneous_backup_and_replication(self, mock_connect, mock_execSQL):
        self.mock_pg_stat_replication(mock_execSQL, [
            self.stub_replication_entry(
                application_name='some_backup_utility',
                state='backup',
                sent_location='0/0', # this matches the real-world behavior but is unimportant to the test
                flush_location=None,
                flush_left=None,
                replay_location=None,
                replay_left=None,
            ),
            self.stub_replication_entry(
                state='streaming',
            ),
        ])

        GpSystemStateProgram._add_replication_info(self.data, self.primary, self.mirror)

        self.assertEqual('Copying files from primary', self.data.getStrValue(self.primary, VALUE__MIRROR_STATUS))
        self.assertEqual('Streaming', self.data.getStrValue(self.mirror, VALUE__MIRROR_STATUS))

    def test_set_mirror_replication_values_complains_about_incorrect_kwargs(self):
        with self.assertRaises(TypeError):
            GpSystemStateProgram._set_mirror_replication_values(self.data, self.mirror, badarg=1)

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

        self.assertEqual('', data.getStrValue(mirror, VALUE__HOSTNAME))
        self.assertEqual('', data.getStrValue(primary, VALUE__DATADIR))
