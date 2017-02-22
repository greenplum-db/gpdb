import imp
import os
import sys
import tempfile

from StringIO import StringIO

from gparray import GpDB, GpArray, Segment
import shutil
from mock import *
from gp_unittest import *
from gphostcache import GpHost


class GpConfig(GpTestCase):
    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.postgressql_conf = self.temp_dir + "/postgresql.conf"
        with open(self.postgressql_conf, "w") as postgresql:
            postgresql.close()

        # because gpconfig does not have a .py extension,
        # we have to use imp to import it
        # if we had a gpconfig.py, this is equivalent to:
        #   import gpconfig
        #   self.subject = gpconfig
        gpconfig_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpconfig")
        self.subject = imp.load_source('gpconfig', gpconfig_file)
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning'])

        self.conn = Mock()
        self.rows = []
        self.conn.execSql.return_value = self.rows

        self.os_env = dict(USER="my_user")
        self.os_env["MASTER_DATA_DIRECTORY"] = self.temp_dir
        self.gparray = self.createGpArrayWith2Primary2Mirrors()
        self.host_cache = Mock()

        host = GpHost('localhost')
        seg = Segment()
        db = self.gparray.master
        seg.addPrimary(db)
        host.addDB(seg)
        self.host_cache.get_hosts.return_value = [host]
        self.host_cache.ping_hosts.return_value = []

        self.master_read_config = Mock()
        self.master_read_config.get_guc_value.return_value = "foo"
        self.master_read_config.get_seg_id.return_value = -1
        self.segment_read_config = Mock()
        self.segment_read_config.get_guc_value.return_value = "foo"
        self.segment_read_config.get_seg_id.return_value = 0

        self.pool = Mock()
        self.pool.getCompletedItems.return_value = [self.master_read_config, self.segment_read_config]

        self.apply_patches([
            patch('os.environ', new=self.os_env),
            patch('gpconfig.dbconn.connect', return_value=self.conn),
            patch('gpconfig.dbconn.execSQL', return_value=self.rows),
            patch('gpconfig.GpHostCache', return_value=self.host_cache),
            patch('gpconfig.GpArray.initFromCatalog', return_value=self.gparray),
            patch('gpconfig.GpReadConfig', return_value=self.master_read_config),
            patch('gpconfig.WorkerPool', return_value=self.pool)
        ])
        sys.argv = ["gpconfig"] # reset to relatively empty args list

    def tearDown(self):
        shutil.rmtree(self.temp_dir)
        super(GpConfig, self).tearDown()

    def createGpArrayWith2Primary2Mirrors(self):
        master = GpDB.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|None|/data/master||/data/master/base/10899,/data/master/base/1,/data/master/base/10898,/data/master/base/25780,/data/master/base/34782")
        primary0 = GpDB.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        primary1 = GpDB.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|41001|/data/primary1||/data/primary1/base/10899,/data/primary1/base/1,/data/primary1/base/10898,/data/primary1/base/25780,/data/primary1/base/34782")
        mirror0 = GpDB.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
        mirror1 = GpDB.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|51001|/data/mirror1||/data/mirror1/base/10899,/data/mirror1/base/1,/data/mirror1/base/10898,/data/mirror1/base/25780,/data/mirror1/base/34782")
        return GpArray([master, primary0, primary1, mirror0, mirror1])

    def test_option_f_parses(self):
        sys.argv = ["gpconfig", "--file", "--show", "statement_mem"]
        options = self.subject.parseargs()

        self.assertEquals(options.show, "statement_mem")
        self.assertEquals(options.file, True)

    def test_option_list_parses(self):
        sys.argv = ["gpconfig", "--list"]
        options = self.subject.parseargs()

        self.assertEquals(options.list, True)

    def test_when_no_options_prints_and_throws(self):
        with self.assertRaisesRegexp(Exception, "No action specified.  See the --help info."):
            self.subject.do_main()
        self.subject.logger.error.assert_called_once_with("No action specified.  See the --help info.")

    def test_option_value_must_accompany_option_change(self):
        sys.argv = ["gpconfig", "--change", "statement_mem"]
        with self.assertRaisesRegexp(Exception, "change requested but value not specified"):
            self.subject.parseargs()
        self.subject.logger.error.assert_called_once_with("change requested but value not specified")

    def test_option_file_with_option_change_will_raise(self):
        sys.argv = ["gpconfig", "--file", "--change", "statement_mem"]
        with self.assertRaisesRegexp(Exception, "'--file' option must accompany '--show' option"):
            self.subject.parseargs()
        self.subject.logger.error.assert_called_once_with("'--file' option must accompany '--show' option")

    def test_option_file_with_option_list_will_raise(self):
        sys.argv = ["gpconfig", "--file", "--list", "statement_mem"]
        with self.assertRaisesRegexp(Exception, "'--file' option must accompany '--show' option"):
            self.subject.parseargs()
        self.subject.logger.error.assert_called_once_with("'--file' option must accompany '--show' option")

    def test_option_file_without_MASTER_DATA_DIR_will_raise(self):
        sys.argv = ["gpconfig", "--file", "--show", "statement_mem"]
        del self.os_env["MASTER_DATA_DIRECTORY"]
        with self.assertRaisesRegexp(Exception, "--file option requires that MASTER_DATA_DIRECTORY be set"):
            self.subject.parseargs()
        self.subject.logger.error.assert_called_once_with("--file option requires that MASTER_DATA_DIRECTORY be set")

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_f_will_report_presence_of_setting(self, mock_stdout):
        sys.argv = ["gpconfig", "--show", "my_property_name", "--file"]

        self.subject.do_main()

        self.assertEqual(self.subject.logger.error.call_count, 0)
        self.assertIn("foo", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_f_will_report_absence_of_setting(self, mock_stdout):
        sys.argv = ["gpconfig", "--show", "my_property_name", "--file"]
        self.master_read_config.get_guc_value.return_value = None
        self.segment_read_config.get_guc_value.return_value = None

        self.subject.do_main()

        self.assertEqual(self.subject.logger.error.call_count, 0)
        self.assertIn("not set in postgresql.conf", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_f_will_report_difference_segments_out_of_sync(self, mock_stdout):
        sys.argv = ["gpconfig", "--show", "my_property_name", "--file"]
        self.master_read_config.get_guc_value.return_value = 'foo'
        self.segment_read_config.get_guc_value.return_value = 'bar'
        another_segment_read_config = Mock()
        another_segment_read_config.get_guc_value.return_value = "baz"
        another_segment_read_config.get_seg_id.return_value = 1
        self.pool.getCompletedItems.return_value.append(another_segment_read_config)

        self.subject.do_main()

        self.assertEqual(self.subject.logger.error.call_count, 0)
        self.assertIn("WARNING: GUCS ARE OUT OF SYNC", mock_stdout.getvalue())
        self.assertIn("bar", mock_stdout.getvalue())
        self.assertIn("baz", mock_stdout.getvalue())


if __name__ == '__main__':
    run_tests()
