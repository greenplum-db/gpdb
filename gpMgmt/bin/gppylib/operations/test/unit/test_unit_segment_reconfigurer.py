from gppylib.operations.segment_reconfigurer import SegmentReconfigurer

from gppylib.test.unit.gp_unittest import GpTestCase
from pygresql import pgdb
import mock
from mock import Mock, patch, call


class SegmentReconfiguerTestCase(GpTestCase):
    @patch('gppylib.db.dbconn.connect')
    @patch('gppylib.db.dbconn.DbURL')
    def test_it_retries_the_connection(self, db_url_mock, connect):
        nonlocal = {'counter': 0}

        def fail_twice(*args):
            if nonlocal['counter'] < 2:
                nonlocal['counter'] += 1
                raise pgdb.DatabaseError
            else:
                return mock.DEFAULT

        db_url = 'dbUrl'
        db_url_mock.return_value = db_url
        conn = Mock(name='conn')
        connect.configure_mock(return_value=conn, side_effect=fail_twice)
        logger = Mock()
        worker_pool = Mock()

        reconfigurer = SegmentReconfigurer(logger, worker_pool)
        reconfigurer.reconfigure()

        connect.assert_has_calls([call(db_url), call(db_url), call(db_url), ])
        conn.close.assert_any_call()
