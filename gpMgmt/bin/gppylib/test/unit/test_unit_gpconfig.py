import base64
import errno
import imp
import os
import pickle
import shutil
import sys
import tempfile

from gppylib.gparray import Segment, GpArray, SegmentPair
from gpconfig_modules.parse_guc_metadata import ParseGuc
import errno
from pg import DatabaseError

from gp_unittest import *
from mock import *
from StringIO import StringIO

db_singleton_side_effect_list = []


def singleton_side_effect(unused1, unused2):
    # this function replaces dbconn.querySingleton(conn, sql), conditionally raising exception
    if len(db_singleton_side_effect_list) > 0:
        if db_singleton_side_effect_list[0] == "DatabaseError":
            raise DatabaseError("mock exception")
        return db_singleton_side_effect_list[0]
    return None


class GpConfig(GpTestCase):
    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        postgresql_conf = self.temp_dir + "/postgresql.conf"
        with open(postgresql_conf, "w") as postgresql:
            postgresql.close()

        # because gpconfig does not have a .py extension,
        # we have to use imp to import it
        # if we had a gpconfig.py, this is equivalent to:
        #   import gpconfig
        #   self.subject = gpconfig
        gpconfig_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpconfig")
        self.subject = imp.load_source('gpconfig', gpconfig_file)
        self.subject.LOGGER = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])
        self.subject.check_gpexpand = lambda : (True, "")

        self.conn = Mock()
        self.cursor = FakeCursor()

        self.os_env = dict(USER="my_user")
        self.os_env["MASTER_DATA_DIRECTORY"] = self.temp_dir
        self.os_env["GPHOME"] = self.temp_dir
        self.gparray = self._create_gparray_with_2_primary_2_mirrors()

        self.host = 'localhost'
        seg = SegmentPair()
        db = self.gparray.master
        seg.addPrimary(db)
        seg.datadir = self.gparray.master.datadir
        seg.hostname = 'localhost'


        self.master_file = Mock(name='master')
        self.master_file.get_value.return_value = 'foo'
        self.master_file.segInfo.getSegmentContentId.return_value = -1
        self.master_file.segInfo.getSegmentDbId.return_value = 0

        self.seg0_file = Mock(name='seg0')
        self.seg0_file.get_value.return_value = 'foo'
        self.seg0_file.segInfo.getSegmentContentId.return_value = 0
        self.seg0_file.segInfo.getSegmentDbId.return_value = 1

        self.pool = Mock()
        self.pool.getCompletedItems.return_value = [self.master_file, self.seg0_file]

        self.apply_patches([
            patch('os.environ', new=self.os_env),
            patch('gpconfig.dbconn.connect', return_value=self.conn),
            patch('gpconfig.dbconn.query', return_value=self.cursor),
            patch('gpconfig.dbconn.querySingleton', side_effect=singleton_side_effect),
            patch('gpconfig.GpArray.initFromCatalog', return_value=self.gparray),
            patch('gpconfig.WorkerPool', return_value=self.pool)
        ])
        sys.argv = ["gpconfig"]  # reset to relatively empty args list

        # GUC object for testing string quoting
        self.guc = Mock()
        self.guc.vartype = "string"

        shared_dir = os.path.join(self.temp_dir, ParseGuc.DESTINATION_DIR)
        _mkdir_p(shared_dir, 0755)
        self.guc_disallowed_readonly_file = os.path.abspath(os.path.join(shared_dir, ParseGuc.DESTINATION_FILENAME))
        with open(self.guc_disallowed_readonly_file, 'w') as f:
            f.writelines("x\ny\n")

    def tearDown(self):
        shutil.rmtree(self.temp_dir)
        super(GpConfig, self).tearDown()
        del db_singleton_side_effect_list[:]

    def test_when_no_options_prints_and_raises(self):
        with self.assertRaisesRegexp(Exception, "No action specified.  See the --help info."):
            self.subject.do_main()
        self.subject.LOGGER.error.assert_called_once_with("No action specified.  See the --help info.")

    def test_option_list_parses(self):
        sys.argv = ["gpconfig", "--list"]
        options = self.subject.parseargs()

        self.assertEquals(options.list, True)

    def test_option_value_must_accompany_option_change_raise(self):
        sys.argv = ["gpconfig", "--change", "statement_mem"]
        with self.assertRaisesRegexp(Exception, "change requested but value not specified"):
            self.subject.parseargs()
        self.subject.LOGGER.error.assert_called_once_with("change requested but value not specified")

    def test_option_show_without_master_data_dir_will_succeed(self):
        sys.argv = ["gpconfig", "--show", "statement_mem"]
        del self.os_env["MASTER_DATA_DIRECTORY"]
        self.subject.parseargs()

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_show_with_port_will_succeed(self, mock_stdout):
        sys.argv = ["gpconfig", "--show", "port"]

        # mocked database values
        # select * from gp_toolkit.gp_param_setting('port');                                                                                                                     ;
        # paramsegment | paramname | paramvalue
        # --------------+-----------+------------
        self.cursor.set_result_for_testing([['-1', 'port', '1234'], ['0', 'port', '3456']])

        self.subject.do_main()

        self.assertIn("GUC                 : port\nContext:    -1 Value: 1234\nContext:     0 Value: 3456\n",
                      mock_stdout.getvalue())

    def test_option_f_parses(self):
        sys.argv = ["gpconfig", "--file", "--show", "statement_mem"]
        options = self.subject.parseargs()

        self.assertEquals(options.show, "statement_mem")
        self.assertEquals(options.file, True)

    def test_option_file_with_option_change_will_raise(self):
        sys.argv = ["gpconfig", "--file", "--change", "statement_mem"]
        with self.assertRaisesRegexp(Exception, "'--file' option must accompany '--show' option"):
            self.subject.parseargs()
        self.subject.LOGGER.error.assert_called_once_with("'--file' option must accompany '--show' option")

    def test_option_file_compare_with_file_will_raise(self):
        sys.argv = ["gpconfig", "--file", "--show", "statement_mem", "--file-compare", ]
        with self.assertRaisesRegexp(Exception, "'--file' option and '--file-compare' option cannot be used together"):
            self.subject.parseargs()
        self.subject.LOGGER.error.assert_called_once_with("'--file' option and '--file-compare' option cannot be used together")

    def test_option_file_with_option_list_will_raise(self):
        sys.argv = ["gpconfig", "--file", "--list", "statement_mem"]
        with self.assertRaisesRegexp(Exception, "'--file' option must accompany '--show' option"):
            self.subject.parseargs()
        self.subject.LOGGER.error.assert_called_once_with("'--file' option must accompany '--show' option")

    def test_option_file_without_master_data_dir_will_raise(self):
        sys.argv = ["gpconfig", "--file", "--show", "statement_mem"]
        del self.os_env["MASTER_DATA_DIRECTORY"]
        with self.assertRaisesRegexp(Exception, "--file option requires that MASTER_DATA_DIRECTORY be set"):
            self.subject.parseargs()
        self.subject.LOGGER.error.assert_called_once_with("--file option requires that MASTER_DATA_DIRECTORY be set")

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_f_will_report_presence_of_setting(self, mock_stdout):
        sys.argv = ["gpconfig", "--show", "my_property_name", "--file"]

        self.subject.do_main()

        self.pool.addCommand.assert_called_once()
        self.pool.join.assert_called_once_with()
        self.pool.check_results.assert_called_once_with()
        self.pool.haltWork.assert_called_once_with()
        self.pool.joinWorkers.assert_called_once_with()
        self.assertEqual(self.subject.LOGGER.error.call_count, 0)
        self.assertIn("Master  value: foo\nSegment value: foo", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_f_will_report_absence_of_setting_on_master(self, mock_stdout):
        sys.argv = ["gpconfig", "--show", "my_property_name", "--file"]
        self.master_file.get_value.return_value = None
        self.seg0_file.get_value.return_value = "seg_value"

        self.subject.do_main()

        self.assertEqual(self.subject.LOGGER.error.call_count, 0)
        self.assertIn("No value is set on master\nSegment value: seg_value", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_f_will_report_absence_of_setting_on_segment(self, mock_stdout):
        sys.argv = ["gpconfig", "--show", "my_property_name", "--file"]
        self.master_file.get_value.return_value = "master_value"
        self.seg0_file.get_value.return_value = None

        self.subject.do_main()

        self.assertEqual(self.subject.LOGGER.error.call_count, 0)
        self.assertIn("Master  value: master_value\nNo value is set on segments", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_f_will_report_absence_of_setting_on_both(self, mock_stdout):
        sys.argv = ["gpconfig", "--show", "my_property_name", "--file"]
        self.master_file.get_value.return_value = None
        self.seg0_file.get_value.return_value = None

        self.subject.do_main()

        self.assertEqual(self.subject.LOGGER.error.call_count, 0)
        self.assertIn("No value is set on master\nNo value is set on segments", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_f_will_report_difference_segments_out_of_sync(self, mock_stdout):
        sys.argv = ["gpconfig", "--show", "my_property_name", "--file"]

        self.master_file.get_value.return_value = 'foo'
        self.seg0_file.get_value.return_value = 'bar'

        seg_1 = Mock(name='seg1')
        seg_1.segInfo.getSegmentContentId.return_value = 1
        seg_1.segInfo.getSegmentDbId.return_value = 2
        seg_1.get_value.return_value = 'baz'

        # mocked values in the files
        self.pool.getCompletedItems.return_value.append(seg_1)

        self.subject.do_main()

        self.assertEqual(self.pool.addCommand.call_count, 5)
        self.assertEqual(self.subject.LOGGER.error.call_count, 0)
        self.assertIn("WARNING: GUCS ARE OUT OF SYNC", mock_stdout.getvalue())
        self.assertIn("bar", mock_stdout.getvalue())
        self.assertIn("[name: my_property_name] [value: baz]", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_f_will_report_difference_segments_out_of_sync_when_unset(self, mock_stdout):
        sys.argv = ["gpconfig", "--show", "my_property_name", "--file"]

        self.master_file.get_value.return_value = 'foo'
        self.seg0_file.get_value.return_value = 'bar'

        seg_1 = Mock(name='seg1')
        seg_1.segInfo.getSegmentContentId.return_value = 1
        seg_1.segInfo.getSegmentDbId.return_value = 2
        seg_1.get_value.return_value = None

        # mocked values in the files
        self.pool.getCompletedItems.return_value.append(seg_1)

        self.subject.do_main()

        self.assertEqual(self.pool.addCommand.call_count, 5)
        self.assertEqual(self.subject.LOGGER.error.call_count, 0)
        self.assertIn("WARNING: GUCS ARE OUT OF SYNC", mock_stdout.getvalue())
        self.assertIn("bar", mock_stdout.getvalue())
        self.assertIn("[name: my_property_name] [not set in file]", mock_stdout.getvalue())

    def test_option_change_value_master_separate_succeed(self):
        db_singleton_side_effect_list.append("some happy result")
        entry = 'my_property_name'
        sys.argv = ["gpconfig", "-c", entry, "-v", "100", "-m", "20"]

        # mocked database values
        # 'SELECT name, setting, unit, short_desc, context, vartype, min_val, max_val FROM pg_settings'
        self.cursor.set_result_for_testing([['my_property_name', 'setting', 'unit', 'short_desc',
                                             'context', 'vartype', 'min_val', 'max_val']])

        self.subject.do_main()

        self.subject.LOGGER.info.assert_called_with("completed successfully with parameters '-c my_property_name -v 100 -m 20'")
        self.assertEqual(self.pool.addCommand.call_count, 5)
        segment_command = self.pool.addCommand.call_args_list[0][0][0]
        self.assertTrue("my_property_name" in segment_command.cmdStr)
        value = base64.urlsafe_b64encode(pickle.dumps("100"))
        self.assertTrue(value in segment_command.cmdStr)
        master_command = self.pool.addCommand.call_args_list[4][0][0]
        self.assertTrue("my_property_name" in master_command.cmdStr)
        value = base64.urlsafe_b64encode(pickle.dumps("20"))
        self.assertTrue(value in master_command.cmdStr)

    def test_option_change_value_masteronly_succeed(self):
        db_singleton_side_effect_list.append("some happy result")
        entry = 'my_property_name'
        sys.argv = ["gpconfig", "-c", entry, "-v", "100", "--masteronly"]

        # mocked database values
        # 'SELECT name, setting, unit, short_desc, context, vartype, min_val, max_val FROM pg_settings'
        self.cursor.set_result_for_testing([['my_property_name', 'setting', 'unit', 'short_desc',
                                             'context', 'vartype', 'min_val', 'max_val']])

        self.subject.do_main()

        self.subject.LOGGER.info.assert_called_with("completed successfully with parameters '-c my_property_name -v 100 --masteronly'")
        self.assertEqual(self.pool.addCommand.call_count, 1)
        master_command = self.pool.addCommand.call_args_list[0][0][0]
        self.assertTrue(("my_property_name") in master_command.cmdStr)
        value = base64.urlsafe_b64encode(pickle.dumps("100"))
        self.assertTrue(value in master_command.cmdStr)

    def test_option_change_value_master_separate_fail_not_valid_guc(self):
        db_singleton_side_effect_list.append("DatabaseError")

        with self.assertRaisesRegexp(Exception, "not a valid GUC: my_property_name"):
            sys.argv = ["gpconfig", "-c", "my_property_name", "-v", "100", "-m", "20"]
            self.subject.do_main()

        self.assertEqual(self.subject.LOGGER.fatal.call_count, 1)

    def test_option_change_value_hidden_guc_with_skipvalidation(self):
        sys.argv = ["gpconfig", "-c", "my_hidden_guc_name", "-v", "100", "--skipvalidation"]
        self.subject.do_main()

        self.subject.LOGGER.info.assert_called_with("completed successfully with parameters '-c my_hidden_guc_name -v 100 --skipvalidation'")
        self.assertEqual(self.pool.addCommand.call_count, 5)
        segment_command = self.pool.addCommand.call_args_list[0][0][0]
        self.assertTrue("my_hidden_guc_name" in segment_command.cmdStr)
        master_command = self.pool.addCommand.call_args_list[1][0][0]
        self.assertTrue("my_hidden_guc_name" in master_command.cmdStr)
        value = base64.urlsafe_b64encode(pickle.dumps("100"))
        self.assertTrue(value in master_command.cmdStr)

    def test_option_change_value_hidden_guc_without_skipvalidation(self):
        db_singleton_side_effect_list.append("my happy result")

        with self.assertRaisesRegexp(Exception, "GUC Validation Failed: my_hidden_guc_name cannot be changed under "
                                                "normal conditions. Please refer to gpconfig documentation."):
            sys.argv = ["gpconfig", "-c", "my_hidden_guc_name", "-v", "100"]
            self.subject.do_main()

        self.subject.LOGGER.fatal.assert_called_once_with("GUC Validation Failed: my_hidden_guc_name cannot be "
                                                          "changed under normal conditions. "
                                                          "Please refer to gpconfig documentation.")

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_file_compare_returns_same_value(self, mock_stdout):
        sys.argv = ["gpconfig", "-s", "my_property_name", "--file-compare"]

        seg_1 = Mock(name='seg1')
        seg_1.segInfo.getSegmentContentId.return_value = 1
        seg_1.segInfo.getSegmentDbId.return_value = 2
        seg_1.get_value.return_value = 'foo'

        # mocked values in the files
        self.pool.getCompletedItems.return_value.append(seg_1)

        # mocked database values
        self.cursor.set_result_for_testing([[-1, 'my_property_name', 'foo'],
                                            [0, 'my_property_name', 'foo'],
                                            [1, 'my_property_name', 'foo']])

        self.subject.do_main()

        self.assertIn("Master  value: foo | file: foo", mock_stdout.getvalue())
        self.assertIn("Segment value: foo | file: foo", mock_stdout.getvalue())
        self.assertIn("Values on all segments are consistent", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_file_compare_works_with_unset_values(self, mock_stdout):
        sys.argv = ["gpconfig", "-s", "my_property_name", "--file-compare"]

        self.master_file.get_value.return_value = None
        self.seg0_file.get_value.return_value = None

        seg_1 = Mock(name='seg1')
        seg_1.segInfo.getSegmentContentId.return_value = 1
        seg_1.segInfo.getSegmentDbId.return_value = 2
        seg_1.get_value.return_value = None

        # mocked values in the files
        self.pool.getCompletedItems.return_value.append(seg_1)

        # mocked database values
        self.cursor.set_result_for_testing([[-1, 'my_property_name', 'foo'],
                                            [0, 'my_property_name', 'foo'],
                                            [1, 'my_property_name', 'foo']])

        self.subject.do_main()

        self.assertIn("Master  value: foo | not set in file", mock_stdout.getvalue())
        self.assertIn("Segment value: foo | not set in file", mock_stdout.getvalue())
        self.assertIn("Values on all segments are consistent", mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_file_compare_returns_different_value(self, mock_stdout):
        sys.argv = ["gpconfig", "-s", "my_property_name", "--file-compare"]

        seg_1 = Mock(name='seg1')
        seg_1.segInfo.getSegmentContentId.return_value = 1
        seg_1.segInfo.getSegmentDbId.return_value = 2
        seg_1.get_value.return_value = 'bar'

        # mocked values in the files
        self.pool.getCompletedItems.return_value.append(seg_1)

        # mocked database values
        self.cursor.set_result_for_testing([[-1, 'my_property_name', 'foo'],
                                            [0, 'my_property_name', 'foo'],
                                            [1, 'my_property_name', 'foo']])

        self.subject.do_main()

        self.assertIn("WARNING: GUCS ARE OUT OF SYNC: ", mock_stdout.getvalue())
        self.assertIn("[context: -1] [dbid: 0] [name: my_property_name] [value: foo | file: foo]",
                      mock_stdout.getvalue())
        self.assertIn("[context: 0] [dbid: 1] [name: my_property_name] [value: foo | file: foo]",
                      mock_stdout.getvalue())
        self.assertIn("[context: 1] [dbid: 2] [name: my_property_name] [value: foo | file: bar]",
                      mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_file_compare_with_unset_values_on_some_segments(self, mock_stdout):
        sys.argv = ["gpconfig", "-s", "my_property_name", "--file-compare"]

        seg2_file = Mock(name='seg2')
        seg2_file.segInfo.getSegmentContentId.return_value = 1
        seg2_file.segInfo.getSegmentDbId.return_value = 2
        seg2_file.get_value.return_value = None

        # mocked values in the files
        self.pool.getCompletedItems.return_value.append(seg2_file)

        # mocked database values
        self.cursor.set_result_for_testing([[-1, 'my_property_name', 'foo'],
                                            [0, 'my_property_name', 'foo'],
                                            [1, 'my_property_name', 'foo']])

        self.subject.do_main()

        self.assertIn("WARNING: GUCS ARE OUT OF SYNC: ", mock_stdout.getvalue())
        self.assertIn("[context: -1] [dbid: 0] [name: my_property_name] [value: foo | file: foo]",
                      mock_stdout.getvalue())
        self.assertIn("[context: 0] [dbid: 1] [name: my_property_name] [value: foo | file: foo]",
                      mock_stdout.getvalue())
        self.assertIn("[context: 1] [dbid: 2] [name: my_property_name] [value: foo | not set in file]",
                      mock_stdout.getvalue())

    @patch('sys.stdout', new_callable=StringIO)
    def test_option_file_compare_with_standby_master_with_different_file_value_will_report_failure(self, mock_stdout):
        sys.argv = ["gpconfig", "-s", "my_property_name", "--file-compare"]

        standby_master = Mock(name='standby_master')
        standby_master.segInfo.getSegmentContentId.return_value = -1
        standby_master.segInfo.getSegmentDbId.return_value = 2
        standby_master.get_value.return_value = 'bar'

        # mocked values in the files
        self.pool.getCompletedItems.return_value.append(standby_master)

        # mocked database values
        self.cursor.set_result_for_testing([[-1, 'my_property_name', 'foo']])

        self.subject.do_main()

        self.assertIn("WARNING: GUCS ARE OUT OF SYNC: ", mock_stdout.getvalue())
        self.assertIn("[context: -1] [dbid: 0] [name: my_property_name] [value: foo | file: foo]",
                      mock_stdout.getvalue())
        self.assertIn("[context: -1] [dbid: 2] [name: my_property_name] [value: foo | file: bar]",
                      mock_stdout.getvalue())

    def test_setting_guc_when_guc_is_readonly_will_fail(self):
        self.subject.read_only_gucs.add("is_superuser")
        sys.argv = ["gpconfig", "-c", "is_superuser", "-v", "on"]
        with self.assertRaisesRegexp(Exception, "not a modifiable GUC: 'is_superuser'"):
            self.subject.do_main()

    def test_change_will_populate_read_only_gucs_set(self):
        sys.argv = ["gpconfig", "--change", "foobar", "--value", "baz"]
        try:
            self.subject.do_main()
        except Exception:
            pass
        self.assertEqual(len(self.subject.read_only_gucs), 2)

    def test_quote_string_not_already_quoted(self):
        value = "teststring"
        expected = "'teststring'"
        result = self.subject.quote_string(self.guc, value)
        self.assertEqual(result, expected)

    def test_quote_string_quoted_with_double_quotes(self):
        value = "\"teststring\""
        expected = "'\"teststring\"'"
        result = self.subject.quote_string(self.guc, value)
        self.assertEqual(result, expected)

    def test_quote_string_with_internal_single_quote(self):
        value = "test'string"
        expected = "'test''string'"
        result = self.subject.quote_string(self.guc, value)
        self.assertEqual(result, expected)

    def test_quote_string_with_internal_backslash(self):
        value = "test\\string"
        expected = "'test\\\\string'"
        result = self.subject.quote_string(self.guc, value)
        self.assertEqual(result, expected)

    def test_quote_string_with_single_quote_and_backslash(self):
        value = "test\\'string"
        expected = "'test\\\\''string'"
        result = self.subject.quote_string(self.guc, value)
        self.assertEqual(result, expected)

    def test_quote_string_with_newline(self):
        value = "test\nstring"
        expected = "'test\\nstring'"
        result = self.subject.quote_string(self.guc, value)
        self.assertEqual(result, expected)

    def setup_for_testing_quoting_string_values(self, vartype, value, additional_args=None):
        sys.argv = ["gpconfig", "--change", "my_property_name", "--value", value]
        if additional_args:
            sys.argv.extend(additional_args)

        # mocked database values
        self.cursor.set_result_for_testing([['my_property_name', 'setting', 'unit', 'short_desc',
                                             'context', vartype, 'min_val', 'max_val']])

    def validation_for_testing_quoting_string_values(self, expected_value):
        for call in self.pool.addCommand.call_args_list:
            # call_obj[0] returns all unnamed arguments -> ['arg1', 'arg2']
            # In this case, we have an object as an argument to poo.addCommand
            # call_obj[1] returns a dict for all named arguments -> {key='arg3', key2='arg4'}
            gp_add_config_script_obj = call[0][0]
            value = base64.urlsafe_b64encode(pickle.dumps(expected_value))
            try:
                self.assertTrue(value in gp_add_config_script_obj.cmdStr)
            except AssertionError as e:
                raise Exception("\nAssert failed: %s\n cmdStr:\n%s\nvs:\nvalue: %s" % (str(e),
                                                                                       gp_add_config_script_obj.cmdStr,
                                                                                       value))

    def test_change_of_unquoted_string_to_quoted_succeeds(self):
        self.setup_for_testing_quoting_string_values(vartype='string', value='baz')
        self.subject.do_main()
        self.validation_for_testing_quoting_string_values(expected_value="'baz'")

    def test_change_of_master_value_with_quotes_succeeds(self):
        already_quoted_master_value = "'ba'z'"
        vartype = 'string'
        self.setup_for_testing_quoting_string_values(vartype=vartype, value=already_quoted_master_value, additional_args=['--mastervalue', already_quoted_master_value])
        self.subject.do_main()
        self.validation_for_testing_quoting_string_values(expected_value="'''ba''z'''")

    def test_change_of_master_only_quotes_succeeds(self):
        unquoted_master_value = "baz"
        vartype = 'string'
        self.setup_for_testing_quoting_string_values(vartype=vartype, value=unquoted_master_value, additional_args=['--masteronly'])
        self.subject.do_main()
        self.validation_for_testing_quoting_string_values(expected_value="'baz'")

    def test_change_of_bool_guc_does_not_quote(self):
        unquoted_value = "baz"
        vartype = 'bool'
        self.setup_for_testing_quoting_string_values(vartype=vartype, value=unquoted_value)
        self.subject.do_main()
        self.validation_for_testing_quoting_string_values(expected_value="baz")

    def test_change_when_disallowed_gucs_file_is_missing_gives_warning(self):
        os.remove(self.guc_disallowed_readonly_file)
        db_singleton_side_effect_list.append("some happy result")
        entry = 'my_property_name'
        sys.argv = ["gpconfig", "-c", entry, "-v", "100", "--masteronly"]

        # mocked database values
        # 'SELECT name, setting, unit, short_desc, context, vartype, min_val, max_val FROM pg_settings'
        self.cursor.set_result_for_testing([['my_property_name', 'setting', 'unit', 'short_desc',
                                             'context', 'vartype', 'min_val', 'max_val']])
        self.subject.do_main()

        self.subject.LOGGER.info.assert_called_with("completed successfully with parameters '-c my_property_name -v 100 --masteronly'")
        target_warning = "disallowed GUCs file missing: '%s'" % self.guc_disallowed_readonly_file
        self.subject.LOGGER.warning.assert_called_with(target_warning)

    def test_when_gphome_env_unset_raises(self):
        self.os_env['GPHOME'] = None
        sys.argv = ["gpconfig", "-c", 'my_property_name', "-v", "100", "--masteronly"]

        with self.assertRaisesRegexp(Exception, "GPHOME environment variable must be set"):
            self.subject.do_main()

    def test_gpconfig_logs_successful_guc_change(self):
        sys.argv = ["gpconfig", "-c", 'my_property_name', "-v", "100", "--masteronly"]

        # mocked database values
        self.cursor.set_result_for_testing([['my_property_name', 'setting', 'unit', 'short_desc',
                                             'context', 'vartype', 'min_val', 'max_val']])

        self.subject.do_main()

        self.subject.LOGGER.info.assert_called_with("completed successfully with parameters '-c my_property_name -v 100 --masteronly'")

    def test_gpconfig_logs_unsuccessful_guc_change(self):
        sys.argv = ["gpconfig", "-c", 'my_property_name', "-v", "100", "--masteronly"]

        # mocked database values
        self.cursor.set_result_for_testing([['my_property_name', 'setting', 'unit', 'short_desc',
                                             'context', 'vartype', 'min_val', 'max_val']])
        self.seg0_file.was_successful.return_value = False
        self.subject.do_main()

        self.subject.LOGGER.error.assert_called_with("finished with errors, parameter string '-c my_property_name -v 100 --masteronly'")


    @staticmethod
    def _create_gparray_with_2_primary_2_mirrors():
        master = Segment.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|/data/master")
        primary0 = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
        mirror0 = Segment.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        mirror1 = Segment.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")
        return GpArray([master, primary0, primary1, mirror0, mirror1])


def _mkdir_p(path, mode):
    try:
        os.makedirs(path, mode)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise


if __name__ == '__main__':
    run_tests()
