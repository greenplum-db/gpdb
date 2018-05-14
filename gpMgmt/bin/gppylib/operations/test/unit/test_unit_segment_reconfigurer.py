import datetime
import time

from gppylib.operations.segment_reconfigurer import SegmentReconfigurer, FTS_PROBE_QUERY

from gppylib.test.unit.gp_unittest import GpTestCase
from pygresql import pgdb
import mock
from mock import Mock, patch, call, MagicMock
import contextlib
import pygresql.pg


class MyDbUrl:
    pass


class SegmentReconfiguerTestCase(GpTestCase):
    db = 'database'
    host = 'mdw'
    port = 15432
    user = 'postgres'
    passwd = 'passwd'

    def setUp(self):
        self.conn = Mock(name='conn')
        self.logger = Mock()
        self.worker_pool = Mock()
        self.db_url = db_url = MyDbUrl()
        db_url.pgdb = self.db
        db_url.pghost = self.host
        db_url.pgport = self.port
        db_url.pguser = self.user
        db_url.pgpass = self.passwd

        self.connect = MagicMock()
        cm = contextlib.nested(
                patch('gppylib.db.dbconn.connect', new=self.connect),
                patch('gppylib.db.dbconn.DbURL', return_value=self.db_url),
                patch('pygresql.pg.connect'),
                )
        cm.__enter__()
        self.cm = cm

    def tearDown(self):
        self.cm.__exit__(None, None, None)

    def test_it_triggers_fts_probe(self):
        reconfigurer = SegmentReconfigurer(self.logger, self.worker_pool)
        reconfigurer.reconfigure()
        pygresql.pg.connect.assert_has_calls([
            call(self.db, self.host, self.port, None, self.user, self.passwd),
            call().query(FTS_PROBE_QUERY),
            call().close(),
            ]
            )

    def test_it_retries_the_connection(self):
        self.connect.configure_mock(side_effect=[pgdb.DatabaseError, pgdb.DatabaseError, self.conn])

        reconfigurer = SegmentReconfigurer(self.logger, self.worker_pool)
        reconfigurer.reconfigure()

        self.connect.assert_has_calls([call(self.db_url), call(self.db_url), call(self.db_url), ])
        self.conn.close.assert_any_call()

    @patch('time.time')
    def test_it_gives_up_after_30_seconds(self, now_mock):
        start_datetime = datetime.datetime(2018, 5, 9, 16, 0, 0)
        start_time = time.mktime(start_datetime.timetuple())
        now_mock.configure_mock(return_value=start_time)

        def fail_for_half_a_minute():
            for i in xrange(1, 3):
                # leap forward 15 seconds
                new_time = start_time + 15 * i
                now_mock.configure_mock(return_value=new_time)
                yield pgdb.DatabaseError


        self.connect.configure_mock(side_effect=fail_for_half_a_minute())

        reconfigurer = SegmentReconfigurer(self.logger, self.worker_pool)
        with self.assertRaises(pgdb.DatabaseError):
            reconfigurer.reconfigure()

        self.connect.assert_has_calls([call(self.db_url), call(self.db_url), ])
        self.conn.close.assert_has_calls([])
