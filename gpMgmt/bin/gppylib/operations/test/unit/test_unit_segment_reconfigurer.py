from gppylib.operations.segment_reconfigurer import SegmentReconfigurer

from gppylib.test.unit.gp_unittest import GpTestCase
from mock import Mock, patch


class SegmentReconfiguerTestCase(GpTestCase):
    @patch('gppylib.db.dbconn.connect')
    @patch('gppylib.db.dbconn.DbURL')
    def test_it_closes_the_connection(self, db_url_mock, connect):
        db_url = 'dbUrl'
        db_url_mock.return_value = db_url
        conn = Mock()
        connect.return_value = conn
        logger = Mock()
        worker_pool = Mock()

        reconfigurer = SegmentReconfigurer(logger, worker_pool)
        reconfigurer.reconfigure()

        db_url_mock.assert_called_once_with()
        connect.assert_called_once_with(db_url)
        conn.close.assert_called_once_with()