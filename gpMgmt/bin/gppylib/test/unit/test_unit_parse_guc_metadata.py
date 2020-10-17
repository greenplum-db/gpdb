import os

import sys

from .gp_unittest import *
from gpconfig_modules.parse_guc_metadata import ParseGuc


class TestParseGucMetadata(GpTestCase):
    def setUp(self):
        self.subject = ParseGuc()
        self.DEST_DIR = "/tmp/test_parseguc/bar"

    def test_main_writes_file(self):
        sys.argv = ["parse_guc_metadata", self.DEST_DIR]

        self.subject.main()

        dest_file = os.path.join(self.DEST_DIR, "share/greenplum", self.subject.DESTINATION_FILENAME)
        with open(dest_file, 'r') as f:
            lines = f.readlines()
        self.assertIn('is_superuser\n', lines)
        self.assertIn('gp_session_id\n', lines)

    def test_main_when_no_prefix_will_fail(self):
        sys.argv = ["parse_guc_metadata"]

        self.assertRaises(SystemExit, self.subject.main)


if __name__ == '__main__':
    run_tests()
