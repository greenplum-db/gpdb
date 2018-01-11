import os
import shutil
import tempfile

from gp_unittest import *
from mock import *

from gppylib.gp_era import GpEraFile

class GpEraTestCase(GpTestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        os.mkdir(os.path.join(self.tmpdir, 'pg_log'))

        self.apply_patches([
            patch('os.path.exists'),
        ])
        self.mock_path_exists = self.get_mock_from_apply_patch('exists')

        self.subject = GpEraFile(self.tmpdir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)
        super(GpEraTestCase, self).tearDown()

    def test_creates_new_era_file_successfully(self):
        self.mock_path_exists.return_value = False
        self.subject.new_era(host='host', port='port', time='time')
        gp_era_path = os.path.join(self.tmpdir, 'pg_log', 'gp_era')
        self.assertTrue(os.path.isfile(gp_era_path))


if __name__ == '__main__':
    run_tests()
