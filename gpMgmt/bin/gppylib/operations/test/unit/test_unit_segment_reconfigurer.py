import datetime
import time

from gppylib.operations.segment_reconfigurer import SegmentReconfigurer, FTS_PROBE_QUERY

from gppylib.test.unit.gp_unittest import GpTestCase
import pg
import pgdb
import mock
from mock import Mock, patch, call, MagicMock
import contextlib


class MyDbUrl:
    pass


class SegmentReconfiguerTestCase(GpTestCase):
    db = 'database'
    host = 'cdw'
    port = 15432
    user = 'postgres'
    passwd = 'passwd'
    timeout = 600

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
        self.apply_patches([
            patch('gppylib.db.dbconn.connect', new=self.connect),
            patch('gppylib.db.dbconn.DbURL', return_value=self.db_url),
            patch('pg.connect'),
        ])

    def test_it_triggers_fts_probe(self):
        reconfigurer = SegmentReconfigurer(logger=self.logger,
                worker_pool=self.worker_pool, timeout=self.timeout)
        reconfigurer.reconfigure()
        pg.connect.assert_has_calls([
            call(dbname=self.db, host=self.host, port=self.port, opt=None, user=self.user, passwd=self.passwd),
            call().query(FTS_PROBE_QUERY),
            call().close(),
            ]
            )

    def test_it_retries_the_connection(self):
        self.connect.configure_mock(side_effect=[pgdb.DatabaseError, pgdb.DatabaseError, self.conn])

        reconfigurer = SegmentReconfigurer(logger=self.logger,
                worker_pool=self.worker_pool, timeout=self.timeout)
        reconfigurer.reconfigure()

        self.connect.assert_has_calls([call(self.db_url), call(self.db_url), call(self.db_url), ])
        self.conn.close.assert_any_call()

    @patch('time.time')
    def test_it_gives_up_after_600_seconds(self, now_mock):
        start_datetime = datetime.datetime(2018, 5, 9, 16, 0, 0)
        start_time = time.mktime(start_datetime.timetuple())
        now_mock.configure_mock(return_value=start_time)

        def fail_for_five_minutes():
            new_time = start_time
            for i in range(2):
                # leap forward 300 seconds
                new_time += self.timeout / 2
                now_mock.configure_mock(return_value=new_time)
                yield pgdb.DatabaseError


        self.connect.configure_mock(side_effect=fail_for_five_minutes())

        reconfigurer = SegmentReconfigurer(logger=self.logger,
                worker_pool=self.worker_pool, timeout=self.timeout)
        with self.assertRaises(RuntimeError) as context:
            reconfigurer.reconfigure()
            self.assertEqual("Mirror promotion did not complete in {0} seconds.".format(self.timeout), context.exception.message)

        self.connect.assert_has_calls([call(self.db_url), call(self.db_url), ])
        self.conn.close.assert_has_calls([])
