import datetime
import time

from gppylib.operations.segment_reconfigurer import SegmentReconfigurer

from gppylib.test.unit.gp_unittest import GpTestCase
from pygresql import pgdb
import mock
from mock import Mock, patch, call, MagicMock


class SegmentReconfiguerTestCase(GpTestCase):
    def setUp(self):
        self.conn = Mock(name='conn')
        self.logger = Mock()
        self.worker_pool = Mock()
        self.db_url = 'dbUrl'

        self.db_url_mock = MagicMock(return_value=self.db_url)
        cm = patch('gppylib.db.dbconn.DbURL', new=(MagicMock(return_value=self.db_url)))
        cm.__enter__()
        self.cm = cm

    def tearDown(self):
        self.cm.__exit__()

    @patch('gppylib.db.dbconn.connect')
    def test_it_retries_the_connection(self, connect):
        connect.configure_mock(side_effect=[pgdb.DatabaseError, pgdb.DatabaseError, self.conn])

        reconfigurer = SegmentReconfigurer(self.logger, self.worker_pool)
        reconfigurer.reconfigure()

        connect.assert_has_calls([call(self.db_url), call(self.db_url), call(self.db_url), ])
        self.conn.close.assert_any_call()

    @patch('time.time')
    @patch('gppylib.db.dbconn.connect')
    def test_it_gives_up_after_30_seconds(self, connect, now_mock):
        start_datetime = datetime.datetime(2018, 5, 9, 16, 0, 0)
        start_time = time.mktime(start_datetime.timetuple())
        now_mock.configure_mock(return_value=start_time)

        def fail_for_half_a_minute():
            for i in xrange(1, 3):
                # leap forward 15 seconds
                new_time = start_time + 15 * i
                now_mock.configure_mock(return_value=new_time)
                yield pgdb.DatabaseError


        connect.configure_mock(side_effect=fail_for_half_a_minute())

        reconfigurer = SegmentReconfigurer(self.logger, self.worker_pool)
        with self.assertRaises(pgdb.DatabaseError):
            reconfigurer.reconfigure()

        connect.assert_has_calls([call(self.db_url), call(self.db_url), ])
        self.conn.close.assert_has_calls([])
