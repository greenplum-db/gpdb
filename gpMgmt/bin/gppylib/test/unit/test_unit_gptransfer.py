
import imp
import os
import re
import tempfile

import shutil
from mock import *
from gp_unittest import *
from gparray import GpDB, GpArray
from gppylib.db.dbconn import UnexpectedRowsError
from pygresql import pgdb
from gppylib.operations.backup_utils import escapeDoubleQuoteInSQLString

cursor_keys = dict(
    normal_tables=re.compile(".*n\.nspname, c\.relname, c\.relstorage.*c\.oid NOT IN \( SELECT parchildrelid.*"),
    partition_tables=re.compile(".*n\.nspname, c\.relname, c\.relstorage(?!.*SELECT parchildrelid).*"),
    relations=re.compile(".*select relname from pg_class r.*"),
    table_info=re.compile(".*select is_nullable, data_type, character_maximum_length,.*"),
    partition_info=re.compile(".*select parkind, parlevel, parnatts, paratts.*"),
    schema_name=re.compile(".*SELECT spcname FROM pg_catalog.pg_tablespace.*"),
    create_schema=re.compile(".*CREATE SCHEMA.*"),
    ordinal_pos=re.compile(".*select ordinal_position from.*"),
    attname=re.compile(".*SELECT attname.*"),
)

class GpTransfer(GpTestCase):
    TEMP_DIR =  "/tmp/test_unit_gptransfer"


    def setUp(self):
        if not os.path.exists(self.TEMP_DIR):
            os.makedirs(self.TEMP_DIR)

        # because gptransfer does not have a .py extension,
        # we have to use imp to import it
        # if we had a gptransfer.py, this is equivalent to:
        #   import gptransfer
        #   self.subject = gptransfer
        gptransfer_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gptransfer")
        self.subject = imp.load_source('gptransfer', gptransfer_file)
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning'])
        self.gparray = self.createGpArrayWith2Primary2Mirrors()

        self.db_connection = MagicMock()
        # TODO: We should be using a spec here, but I haven't been able to narrow down exactly which call
        # is causing an attribute error when using the spec.
        # The error is occuring because we don't mock out every possible SQL command, and some get swallowed
        # (which is fine so far), but to fully support specs
        # we need to go through and mock all the SQL calls
        # self.db_connection = MagicMock(spec=["__exit__", "close", "__enter__", "commit", "rollback"])
        self.cursor = MagicMock(spec=pgdb.pgdbCursor)

        self.db_singleton = Mock()
        self.workerpool = MagicMock()
        self.workerpool.work_queue.qsize.return_value = 0

        self.apply_patches([
            patch('os.environ', new={"GPHOME": "my_gp_home"}),
            patch('gppylib.operations.dump.GpArray.initFromCatalog', return_value=self.gparray),
            patch('gptransfer.connect', return_value=self.db_connection),
            patch('gptransfer.getUserDatabaseList', return_value=[["my_first_database"], ["my_second_database"]]),
            patch('gppylib.db.dbconn.connect', return_value=self.db_connection),
            patch('gptransfer.WorkerPool', return_value=self.workerpool),
            patch('gptransfer.doesSchemaExist', return_value=False),
            patch('gptransfer.dropSchemaIfExist'),
            patch('gptransfer.execSQL', new=self.cursor),
            patch('gptransfer.execSQLForSingletonRow', new=self.db_singleton),
            patch("gppylib.commands.unix.FileDirExists.remote", return_value=True),
            patch("gptransfer.wait_for_pool", return_value=([], [])),
            patch("gptransfer.escapeDoubleQuoteInSQLString"),
        ])

        # We have a GIGANTIC class that uses 31 arguments, so pre-setting this
        # here
        self.GpTransferCommand_args = dict(
            name='foo',
            src_host='foo',
            src_port='foo',
            src_user='foo',
            dest_host='foo',
            dest_port='foo',
            dest_user='foo',
            table_pair='foo',
            dest_exists='foo',
            truncate='foo',
            analyze='foo',
            drop='foo',
            fast_mode='foo',
            exclusive_lock='foo',
            schema_only='foo',
            work_dir='foo',
            host_map='foo',
            source_config='foo',
            batch_size='foo',
            gpfdist_port='foo',
            gpfdist_last_port='foo',
            gpfdist_instance_count='foo',
            gpfdist_verbosity='foo',
            max_line_length='foo',
            timeout='foo',
            wait_time='foo',
            delimiter='foo',
            validator='foo',
            format='foo',
            quote='foo',
            table_transfer_set_total='foo')

        self.GpTransfer_options_defaults = dict(
            analyze=False,
            base_port=8000,
            batch_size=2,
            databases=[],
            delimiter=',',
            dest_database=None,
            dest_host='127.0.0.1',
            dest_port=5432,
            dest_user='gpadmin',
            drop=False,
            dry_run=False,
            enable_test=False,
            exclude_input_file=None,
            exclude_tables=[],
            exclusive_lock=False,
            force_standard_mode=False,
            format='CSV',
            full=False,
            input_file=None,
            interactive=False,
            last_port=-1,
            logfileDirectory=None,
            max_gpfdist_instances=1,
            max_line_length=10485760,
            no_final_count_validation=False,
            partition_transfer=False,
            partition_transfer_non_pt_target=False,
            quiet=None,
            quote='\x01',
            schema_only=False,
            skip_existing=False,
            source_host='127.0.0.1',
            source_map_file=None,
            source_port=5432,
            source_user='gpadmin',
            sub_batch_size=25,
            tables=[],
            timeout=300,
            truncate=False,
            validator=None,
            verbose=None,
            gpfdist_verbose=False,
            gpfdist_very_verbose=False,
            wait_time=3,
            work_base_dir='/home/gpadmin/',
        )

    def tearDown(self):
        shutil.rmtree(self.TEMP_DIR)
        super(GpTransfer, self).tearDown()

    def test__GpCreateGpfdist(self):
        cmd = self.subject.GpCreateGpfdist(
            'gpfdist for table %s on %s' % ("some table",
                                            "some segment"),
            "some dirname",
            "some datafile",
            0,
            1,
            2,
            3,
            "pid_file",
            "log_file",
            ctxt=self.subject.REMOTE,
            remoteHost="address")
        self.assertEqual('nohup gpfdist -d some dirname -p 0 -P 1 -m 2 -t 3 > log_file 2>&1 < /dev/null & echo \\$! ' \
                         '> pid_file && bash -c "(sleep 1 && kill -0 \\`cat pid_file 2> /dev/null\\` && cat log_file) ' \
                         '|| (cat log_file >&2 && exit 1)"', cmd.cmdStr)


    def test__GpCreateGpfdist_with_verbose(self):
        cmd = self.subject.GpCreateGpfdist(
            'gpfdist for table %s on %s' % ("some table",
                                            "some segment"),
            "some dirname",
            "some datafile",
            0,
            1,
            2,
            3,
            "pid_file",
            "log_file",
            ctxt = self.subject.REMOTE,
            remoteHost = "address",
            verbosity = "-v ")
        self.assertEqual('nohup gpfdist -d some dirname -p 0 -P 1 -m 2 -t 3 -v > log_file 2>&1 < /dev/null & echo \\$! ' \
                         '> pid_file && bash -c "(sleep 1 && kill -0 \\`cat pid_file 2> /dev/null\\` && cat log_file) ' \
                         '|| (cat log_file >&2 && exit 1)"', cmd.cmdStr)

    def test__GpCreateGpfdist_with_very_verbose(self):
        cmd = self.subject.GpCreateGpfdist(
            'gpfdist for table %s on %s' % ("some table",
                                            "some segment"),
            "some dirname",
            "some datafile",
            0,
            1,
            2,
            3,
            "pid_file",
            "log_file",
            ctxt = self.subject.REMOTE,
            remoteHost = "address",
            verbosity = "-V ")
        self.assertEqual('nohup gpfdist -d some dirname -p 0 -P 1 -m 2 -t 3 -V > log_file 2>&1 < /dev/null & echo \\$! ' \
                         '> pid_file && bash -c "(sleep 1 && kill -0 \\`cat pid_file 2> /dev/null\\` && cat log_file) ' \
                         '|| (cat log_file >&2 && exit 1)"', cmd.cmdStr)

    @patch('os._exit')
    def test__cleanup_with_gpfdist_no_verbose_or_very_verbose_does_not_show_gpfdist_warning(self, mock1):
        options = self.setup_normal_to_normal_validation()

        self.subject.GpTransfer(Mock(**options), []).cleanup()
        warnings = self.get_warnings()
        self.assertNotIn("gpfdist logs are present in %s on all hosts in the source", warnings)

    @patch('os._exit')
    def test__cleanup_with_gpfdist_verbose_shows_gpfdist_warning(self, mock1):
        options = self.setup_normal_to_normal_validation()
        options.update(gpfdist_verbose=True)

        self.subject.GpTransfer(Mock(**options), []).cleanup()
        warnings = self.get_warnings()
        self.assertIn("gpfdist logs are present in %s on all hosts in the source", warnings)

    @patch('os._exit')
    def test__cleanup_with_gpfdist_very_verbose_shows_gpfdist_warning(self, mock1):
        options = self.setup_normal_to_normal_validation()
        options.update(gpfdist_very_verbose=True)

        self.subject.GpTransfer(Mock(**options), []).cleanup()
        warnings = self.get_warnings()
        self.assertIn("gpfdist logs are present in %s on all hosts in the source", warnings)

    @patch('gptransfer.TableValidatorFactory', return_value=Mock())
    def test__get_distributed_by_quotes_column_name(self, mock1):
        gptransfer = self.subject
        cmd_args = self.GpTransferCommand_args

        src_args = ('src', 'public', 'foo', False)
        dest_args = ('dest', 'public', 'foo', False)
        source_table = gptransfer.GpTransferTable(*src_args)
        dest_table = gptransfer.GpTransferTable(*dest_args)
        cmd_args['table_pair'] = gptransfer.GpTransferTablePair(source_table, dest_table)
        side_effect = CursorSideEffect()
        side_effect.append_regexp_key(cursor_keys['attname'], [['foo']])
        self.cursor.side_effect = side_effect.cursor_side_effect
        self.subject.escapeDoubleQuoteInSQLString.return_value='"escaped_string"'
        table_validator = gptransfer.GpTransferCommand(**cmd_args)
        expected_distribution = '''DISTRIBUTED BY ("escaped_string")'''

        self.assertEqual(expected_distribution, table_validator._get_distributed_by())

    @patch('gptransfer.TableValidatorFactory', return_value=Mock())
    def test__get_distributed_by_quotes_multiple_column_names(self, mock1):
        gptransfer = self.subject
        cmd_args = self.GpTransferCommand_args

        src_args = ('src', 'public', 'foo', False)
        dest_args = ('dest', 'public', 'foo', False)
        source_table = gptransfer.GpTransferTable(*src_args)
        dest_table = gptransfer.GpTransferTable(*dest_args)
        cmd_args['table_pair'] = gptransfer.GpTransferTablePair(source_table, dest_table)
        side_effect = CursorSideEffect()
        side_effect.append_regexp_key(cursor_keys['attname'], [['foo'], ['bar']])
        self.cursor.side_effect = side_effect.cursor_side_effect
        self.subject.escapeDoubleQuoteInSQLString.side_effect = ['"first_escaped_value"', '"second_escaped_value"']
        table_validator = gptransfer.GpTransferCommand(**cmd_args)
        expected_distribution = '''DISTRIBUTED BY ("first_escaped_value", "second_escaped_value")'''

        self.assertEqual(expected_distribution, table_validator._get_distributed_by())

    @patch('gptransfer.TableValidatorFactory', return_value=Mock())
    def test__get_distributed_randomly_when_no_distribution_keys(self, mock1):
        side_effect = CursorSideEffect()
        side_effect.append_regexp_key(cursor_keys['attname'], [])
        self.cursor.side_effect = side_effect.cursor_side_effect
        table_validator = self._get_gptransfer_command()
        expected_distribution = '''DISTRIBUTED RANDOMLY'''

        result_distribution = table_validator._get_distributed_by()

        self.assertEqual(0, len(self.subject.logger.method_calls))
        self.assertEqual(expected_distribution, result_distribution)

    @patch('gptransfer.TableValidatorFactory', return_value=Mock())
    def test_get_distributed_randomly_handles_exception(self, mock1):
        self.cursor.side_effect = ""
        table_validator = self._get_gptransfer_command()
        expected_distribution = '''DISTRIBUTED RANDOMLY'''

        result_distribution = table_validator._get_distributed_by()

        self.assertEqual(1, len(self.subject.logger.method_calls))
        self.assertEqual(expected_distribution, result_distribution)

    def test__normal_transfer_no_tables_does_nothing_but_log(self):
        options = self.setup_normal_to_normal_validation()
        with open(options["input_file"], "w") as src_map_file:
            src_map_file.write("my_first_database.public.nonexistent_table")
        with self.assertRaises(SystemExit):
            self.subject.GpTransfer(Mock(**options), [])
        log_messages = self.get_info_messages()
        self.assertIn("Found no tables to transfer.", log_messages[-1])

    def test__normal_transfer_with_tables_validates(self):
        options = self.setup_normal_to_normal_validation()

        self.subject.GpTransfer(Mock(**options), [])

        log_messages = self.get_info_messages()
        self.assertIn("Validating transfer table set...", log_messages)

    def test__normal_transfer_when_destination_table_already_exists_fails(self):
        options = self.setup_normal_to_normal_validation()
        additional = {
            cursor_keys["normal_tables"]: [["public", "my_normal_table", ""]],
        }
        self.cursor.side_effect = CursorSideEffect(additional=additional).cursor_side_effect
        with self.assertRaisesRegexp(Exception, "Table my_first_database.public.my_normal_table exists in "
                                                "database my_first_database"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__normal_transfer_when_input_file_bad_format_comma_fails(self):
        options = self.setup_normal_to_normal_validation()
        with open(options["input_file"], "w") as src_map_file:
            src_map_file.write("my_first_database.public.my_table, my_second_database.public.my_table")
        self.cursor.side_effect = CursorSideEffect().cursor_side_effect

        with self.assertRaisesRegexp(Exception, "Destination tables \(comma separated\) are only allowed for "
                                                "partition tables"):
            self.subject.GpTransfer(Mock(**options), [])

    @patch('gptransfer.CountTableValidator.accumulate', side_effect=Exception('BOOM'))
    def test__final_count_validation_when_throws_should_raises_exception(self, mock1):
        options = self.setup_normal_to_normal_validation()
        with self.assertRaisesRegexp(Exception, "Final count validation failed"):
            self.subject.GpTransfer(Mock(**options), []).run()

    def test__final_count_invalid_one_src_one_dest_table_logs_error(self):
        options = self.setup_normal_to_normal_validation()
        additional = {
            "SELECT count(*) FROM": [3]
        }
        self.db_singleton.side_effect = SingletonSideEffect(additional).singleton_side_effect

        self.subject.GpTransfer(Mock(**options), []).run()

        self.assertIn("Validation failed for %s", self.get_error_logging())

    def test__partition_to_partition_final_count_invalid_one_src_one_dest_table_logs_warning(self):
        options = self.setup_partition_validation()
        additional = {
            "SELECT count(*) FROM": [3]
        }
        self.db_singleton.side_effect = SingletonSideEffect(additional).singleton_side_effect

        self.subject.GpTransfer(Mock(**options), []).run()

        self.assertIn("Validation failed for %s", self.get_warnings())

    def test__partition_to_partition_when_invalid_final_counts_should_warn(self):
        options = self.setup_partition_validation()
        with open(options["input_file"], "w") as src_map_file:
            src_map_file.write(
                "my_first_database.public.my_table_partition1, my_first_database.public.my_table_partition1\n"
                "my_first_database.public.my_table_partition2")

        additional = {
            cursor_keys["partition_tables"]: [["public", "my_table_partition1", ""],
                                              ["public", "my_table_partition2", ""]],
        }
        cursor_side_effect = CursorSideEffect(additional=additional)
        self.cursor.side_effect = cursor_side_effect.cursor_side_effect

        multi = {
            "SELECT count(*) FROM": [[12], [10]]
        }
        self.db_singleton.side_effect = SingletonSideEffect(additional_col_list=multi).singleton_side_effect

        self.subject.GpTransfer(Mock(**options), []).run()

        self.assertIn("Validation failed for %s", self.get_warnings())

    def test__partition_to_partition_when_valid_final_counts_mult_src_same_dest_table_succeeds(self):
        options = self.setup_partition_validation()
        with open(options["input_file"], "w") as src_map_file:
            src_map_file.write(
                "my_first_database.public.my_table_partition1, my_first_database.public.my_table_partition1\n"
                "my_first_database.public.my_table_partition2")

        additional = {
            cursor_keys["partition_tables"]: [["public", "my_table_partition1", ""],
                                              ["public", "my_table_partition2", ""]],
        }
        cursor_side_effect = CursorSideEffect(additional=additional)
        self.cursor.side_effect = cursor_side_effect.cursor_side_effect

        self.subject.GpTransfer(Mock(**options), []).run()

        self.assertIn("Validation of %s successful", self.get_info_messages())

    def test__partition_to_normal_table_succeeds(self):
        options = self.setup_partition_to_normal_validation()

        # simulate that dest normal table has 0 rows to begin with and 20 when finished
        multi = {
            "SELECT count(*) FROM": [[20], [0]]
        }
        self.db_singleton.side_effect = SingletonSideEffect(additional_col_list=multi).singleton_side_effect

        self.subject.GpTransfer(Mock(**options), []).run()

        self.assertNotIn("Validation failed for %s", self.get_warnings())
        self.assertIn("Validation of %s successful", self.get_info_messages())

    def test__final_count_validation_same_counts_src_dest_passes(self):
        options = self.setup_normal_to_normal_validation()

        self.subject.GpTransfer(Mock(**options), []).run()

        self.assertIn("Validation of %s successful", self.get_info_messages())

    def test__validates_good_partition(self):
        options = self.setup_partition_validation()

        self.subject.GpTransfer(Mock(**options), [])

        self.assertIn("Validating partition table transfer set...", self.get_info_messages())

    def test__partition_to_nonexistent_partition_fails(self):
        options = self.setup_partition_validation()
        with open(options["input_file"], "w") as src_map_file:
            src_map_file.write(
                "my_first_database.public.my_table_partition1, my_first_database.public.my_table_partition2")
        self.cursor.side_effect = CursorSideEffect().cursor_side_effect

        with self.assertRaisesRegexp(Exception, "does not exist in destination database when transferring from "
                                                "partition tables .filtering for destination leaf partitions because "
                                                "of option \"--partition-transfer\"."):
            self.subject.GpTransfer(Mock(**options), [])

    def test__partition_to_nonexistent_normal_table_fails(self):
        options = self.setup_partition_to_normal_validation()
        with open(options["input_file"], "w") as src_map_file:
            src_map_file.write("my_first_database.public.my_table_partition1, my_first_database.public.does_not_exist")
        self.cursor.side_effect = CursorSideEffect().cursor_side_effect

        with self.assertRaisesRegexp(Exception, "does not exist in destination database when transferring from "
                                                "partition tables .filtering for destination non-partition tables "
                                                "because of option \"--partition-transfer-non-partition-target\"."):
            self.subject.GpTransfer(Mock(**options), [])

    def test__partition_to_multiple_same_partition_tables_fails(self):
        options = self.setup_partition_validation()
        with open(options["input_file"], "w") as src_map_file:
            src_map_file.write(
                "my_first_database.public.my_table_partition1\n"
                "my_first_database.public.my_table_partition3, "
                "my_first_database.public.my_table_partition1")

        cursor_side_effect = CursorSideEffect()
        cursor_side_effect.first_values[cursor_keys["partition_tables"]] = [["public", "my_table_partition1", ""],
                                                                            ["public", "my_table_partition3", ""]]
        self.cursor.side_effect = cursor_side_effect.cursor_side_effect
        with self.assertRaisesRegexp(Exception, "Multiple tables map to"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__partition_to_nonpartition_table_with_different_columns_fails(self):
        options = self.setup_partition_to_normal_validation()
        with open(options["input_file"], "w") as src_map_file:
            src_map_file.write("my_first_database.public.my_table_partition1, my_first_database.public.my_normal_table")

        additional = {
            cursor_keys["normal_tables"]: [["public", "my_normal_table", ""]],
            cursor_keys['table_info']: [
                [1, "t", "my_new_data_type", 255, 16, 1024, 1024, 1, 1024, "my_interval_type", "my_udt_name"]],
        }
        cursor_side_effect = CursorSideEffect(additional=additional)
        cursor_side_effect.first_values[cursor_keys["partition_tables"]] = [["public", "my_table_partition1", ""]]
        self.cursor.side_effect = cursor_side_effect.cursor_side_effect
        with self.assertRaisesRegexp(Exception, "has different column layout or types"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__multiple_partitions_to_same_normal_table_succeeds(self):
        options = self.setup_partition_to_normal_validation()
        with open(options["input_file"], "w") as src_map_file:
            src_map_file.write(
                "my_first_database.public.my_table_partition1, my_first_database.public.my_normal_table\n"
                "my_first_database.public.my_table_partition2, my_first_database.public.my_normal_table")

        additional = {
            cursor_keys["normal_tables"]: [["public", "my_normal_table", ""]],
        }
        cursor_side_effect = CursorSideEffect(additional=additional)
        cursor_side_effect.first_values[cursor_keys["partition_tables"]] = [["public", "my_table_partition1", ""],
                                                                            ["public", "my_table_partition2", ""]]
        self.cursor.side_effect = cursor_side_effect.cursor_side_effect

        # call through to unmocked version of this function because the function gets called too many times
        # to easily mock in this case
        self.subject.escapeDoubleQuoteInSQLString = escapeDoubleQuoteInSQLString

        class SingletonSideEffectWithIterativeReturns(SingletonSideEffect):
            def __init__(self):
                SingletonSideEffect.__init__(self)
                self.values['SELECT count(*) FROM "public"."my_normal_table"'] = [[[30], [15], [15]]]
                self.counters['SELECT count(*) FROM "public"."my_normal_table"'] = 0

            def singleton_side_effect(self, *args):
                for key in self.values.keys():
                    for arg in args:
                        if key in arg:
                            value_list = self.values[key]
                            result = value_list[self.counters[key] % len(value_list)]
                            if any(isinstance(item, list) for item in value_list):
                                result = result[self.counters[key] % len(value_list)]
                            self.counters[key] += 1
                            return result
                return None
        self.db_singleton.side_effect = SingletonSideEffectWithIterativeReturns().singleton_side_effect

        self.subject.GpTransfer(Mock(**options), []).run()

        self.assertNotIn("Validation failed for %s", self.get_warnings())
        self.assertIn("Validation of %s successful", self.get_info_messages())

    def test__validate_nonpartition_tables_with_truncate_fails(self):
        options = self.setup_partition_to_normal_validation()
        options.update(truncate=True)

        with self.assertRaisesRegexp(Exception, "--truncate is not allowed with option "
                                                "--partition-transfer-non-partition-target"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_bad_partition_source_not_leaf_fails(self):
        options = self.setup_partition_validation()

        cursor_side_effect = CursorSideEffect()
        cursor_side_effect.first_values[cursor_keys['relations']] = ["my_relname1", "my_relname2"]
        self.cursor.side_effect = cursor_side_effect.cursor_side_effect

        with self.assertRaisesRegexp(Exception, "Source table "):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_partition_when_source_and_dest_have_different_column_count_fails(self):
        options = self.setup_partition_validation()

        additional = {
            cursor_keys['table_info']: [
                ["t", "my_data_type", 255, 16, 1024, 1024, 1, 1024, "my_interval_type", "my_udt_name"],
                ["t", "my_data_type", 255, 16, 1024, 1024, 1, 1024, "my_interval_type", "my_udt_name"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different column layout or types"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_bad_partition_different_column_type_fails(self):
        options = self.setup_partition_validation()

        additional = {
            cursor_keys['table_info']: [
                ["t", "my_new_data_type", 255, 16, 1024, 1024, 1, 1024, "my_interval_type", "my_udt_name"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different column layout or types"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_bad_partition_different_max_levels_fails(self):
        options = self.setup_partition_validation()

        additional = {
            "select max(p1.partitionlevel)": [2],
        }
        self.db_singleton.side_effect = SingletonSideEffect(additional).singleton_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = self.get_error_logging()
        self.assertIn("Max level of partition is not same between", log_messages[0])

    def test__validate_bad_partition_different_values_of_attributes_fails(self):
        options = self.setup_partition_validation()

        additional = {
            cursor_keys['partition_info']: [["my_parkind", 1, 1, "3 4"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = self.get_error_logging()
        self.assertIn("Partition type or key is different between", log_messages[1])
        self.assertIn("Partition column attributes are different at level", log_messages[0])

    def test__validate_partition_transfer_when_different_partition_attributes_fails(self):
        options = self.setup_partition_validation()

        additional = {
            cursor_keys['partition_info']: [["my_parkind", 1, 2, "3 4"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = self.get_error_logging()
        self.assertIn("Partition type or key is different between", log_messages[1])
        self.assertIn("Number of partition columns is different at level", log_messages[0])

    def test__validate_bad_partition_different_parent_kind_fails(self):
        options = self.setup_partition_validation()

        additional = {
            cursor_keys['partition_info']: [["different_parkind", 1, "my_parnatts", "my_paratts"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = self.get_error_logging()
        self.assertIn("Partition type or key is different between", log_messages[1])
        self.assertIn("Partition type is different at level", log_messages[0])

    def test__validate_bad_partition_different_number_of_attributes_fails(self):
        options = self.setup_partition_validation()

        additional = {
            cursor_keys['partition_info']: [["my_parkind", 1, 2, "my_paratts"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = self.get_error_logging()
        self.assertIn("Partition type or key is different between", log_messages[1])
        self.assertIn("Number of partition columns is different at level ", log_messages[0])

    def test__validate_bad_partition_different_partition_values_fails(self):
        options = self.setup_partition_validation()

        additional = {
            "select n.nspname, c.relname": [["not_public", "not_my_table", ""], ["public", "my_table_partition1", ""]],
            "select parisdefault, parruleord, parrangestartincl,": ["t", "1", "t", "t", 100, 10, "", ""],
        }
        self.db_singleton.side_effect = SingletonSideEffect(additional).singleton_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = self.get_error_logging()
        self.assertIn("One of the subpartition table is a default partition", log_messages[0])
        self.assertIn("Partition value is different in the partition hierarchy between", log_messages[1])

    def test__validate_bad_partition_unknown_type_fails(self):
        options = self.setup_partition_validation()
        my_singleton = SingletonSideEffect()
        my_singleton.values["select partitiontype"] = ["unknown"]
        self.db_singleton.side_effect = my_singleton.singleton_side_effect

        with self.assertRaisesRegexp(Exception, "Unknown partitioning type "):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_bad_partition_different_list_values_fails(self):
        options = self.setup_partition_validation()

        additional = {
            "select parisdefault, parruleord, parrangestartincl,": ["f", "1", "t", "t", 100, 10, "", "different"],
        }

        my_singleton = SingletonSideEffect(additional)
        my_singleton.values["select partitiontype"] = [["list"]]
        self.db_singleton.side_effect = my_singleton.singleton_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = self.get_error_logging()
        self.assertIn("List partition value is different between", log_messages[0])
        self.assertIn("Partition value is different in the partition hierarchy between", log_messages[1])

    def test__validate_bad_partition_different_range_values_fails(self):
        self.run_range_partition_value(
            {"select parisdefault, parruleord, parrangestartincl,": ["f", "1", "f", "t", 100, 10, "", "different"]})
        self.run_range_partition_value(
            {"select parisdefault, parruleord, parrangestartincl,": ["f", "1", "t", "f", 999, 10, "", "different"]})
        self.run_range_partition_value(
            {"select parisdefault, parruleord, parrangestartincl,": ["f", "1", "t", "t", 100, 999, "", "different"]})
        self.run_range_partition_value(
            {"select parisdefault, parruleord, parrangestartincl,": ["f", "1", "t", "t", 100, 10, 999, "different"]})

    def test__validate_bad_partition_different_parent_partition_fails(self):
        options = self.setup_partition_validation()

        multi = {
            "select parisdefault, parruleord, parrangestartincl,": [["f", "1", "t", "t", 100, 10, "", ""],
                                                                    ["f", "1", "t", "t", 100, 10, "", ""],
                                                                    ["f", "1", "t", "t", 999, 10, "", ""]],
        }
        singleton_side_effect = SingletonSideEffect(additional_col_list=multi)
        self.db_singleton.side_effect = singleton_side_effect.singleton_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        error_messages = self.get_error_logging()
        self.assertIn("Range partition value is different between source partition table", error_messages[0])
        self.assertIn("Partitions have different parents at level", error_messages[1])

    def test__validate_pt_non_pt_target_with_validator__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['validator'] = "MD5"

        with self.assertRaisesRegexp(Exception, "--partition-transfer-non-partition-target option cannot "
                                                "be used with --validate option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_pt_non_pt_target_with_partition_transfer__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['partition_transfer'] = True

        with self.assertRaisesRegexp(Exception, "--partition-transfer option cannot "
                                                "be used with --partition-transfer-non-partition-target option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_pt_non_pt_target_without_input_file__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['input_file'] = None

        with self.assertRaisesRegexp(Exception, "--partition-transfer-non-partition-target option "
                                                "must be used with -f option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_pt_non_pt_target_with_databases__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['databases'] = ['db1']

        with self.assertRaisesRegexp(Exception, "--partition-transfer-non-partition-target option "
                                                "cannot be used with -d option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_pt_non_pt_target_with_dest_databases__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['dest_database'] = ['db1']

        with self.assertRaisesRegexp(Exception, "--partition-transfer-non-partition-target option "
                                                "cannot be used with --dest-database option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_pt_non_pt_target_with_drop__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['drop'] = True

        with self.assertRaisesRegexp(Exception, "--partition-transfer-non-partition-target option "
                                                "cannot be used with --drop option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_pt_non_pt_target_with_tables__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['tables'] = ['public.table1']

        with self.assertRaisesRegexp(Exception, "--partition-transfer-non-partition-target option "
                                                "cannot be used with -t option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_pt_non_pt_target_with_schema_only__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['schema_only'] = True

        with self.assertRaisesRegexp(Exception, "--partition-transfer-non-partition-target option "
                                                "cannot be used with --schema-only option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_pt_non_pt_target_with_full__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['full'] = True

        with self.assertRaisesRegexp(Exception, "--partition-transfer-non-partition-target option "
                                                "cannot be used with --full option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_pt_non_pt_target_with_exclude_input_file__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['exclude_input_file'] = tempfile.NamedTemporaryFile(dir=self.TEMP_DIR, delete=False)

        with self.assertRaisesRegexp(Exception, "--partition-transfer-non-partition-target option cannot "
                                                "be used with any exclude table option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_pt_non_pt_target_with_exclude_tables__fails(self):
        options = self.setup_partition_to_normal_validation()
        options['exclude_tables'] = ['public.table1']

        with self.assertRaisesRegexp(Exception, "--partition-transfer-non-partition-target option cannot "
                                                "be used with any exclude table option"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__partition_to_normal_multiple_same_dest_must_come_from_same_source_partition(self):
        options = self.setup_partition_to_normal_validation()
        with open(options["input_file"], "w") as src_map_file:
            src_map_file.write(
                "my_first_database.public.my_table_partition1, my_first_database.public.my_normal_table\n"
                "my_first_database.public.my_table_partition2, my_first_database.public.my_normal_table")

        additional = {
            cursor_keys["normal_tables"]: [["public", "my_normal_table", ""]],
        }
        cursor_side_effect = CursorSideEffect(additional=additional)
        cursor_side_effect.first_values[cursor_keys["partition_tables"]] = [["public", "my_table_partition1", ""],
                                                                            ["public", "my_table_partition2", ""]]
        self.cursor.side_effect = cursor_side_effect.cursor_side_effect

        class SingletonSideEffectWithIterativeReturns(SingletonSideEffect):
            def __init__(self, multi_value=None):
                SingletonSideEffect.__init__(self, additional_col_list=multi_value)
                self.values["SELECT count(*) FROM public.my_normal_table"] = [[[30, 15, 15]]]
                self.counters["SELECT count(*) FROM public.my_normal_table"] = 0

            def singleton_side_effect(self, *args):
                for key in self.values.keys():
                    for arg in args:
                        if key in arg:
                            value_list = self.values[key]
                            result = value_list[self.counters[key] % len(value_list)]
                            if any(isinstance(i, list) for i in value_list):
                                result = result[self.counters[key] % len(value_list)]
                            self.counters[key] += 1
                            return result
                return None

        multi_value = {
            "select n.nspname, c.relname": [["public", "my_table_partition1"], ["public", "other_parent"]]
        }
        self.db_singleton.side_effect = SingletonSideEffectWithIterativeReturns(multi_value=multi_value).singleton_side_effect

        with self.assertRaisesRegexp(Exception, "partition sources: public.my_table_partition1, "
                                                "public.my_table_partition2,  when transferred to "
                                                "the same destination: table public.my_normal_table , "
                                                "must share the same parent"):
            self.subject.GpTransfer(Mock(**options), []).run()

    def test__validating_transfer_with_empty_source_map_file_raises_proper_exception(self):
        options = self.setup_partition_to_normal_validation()

        source_map_filename = tempfile.NamedTemporaryFile(dir=self.TEMP_DIR, delete=False)
        source_map_filename.write("")
        source_map_filename.flush()
        options.update(
            source_map_file=source_map_filename.name
        )

        with self.assertRaisesRegexp(Exception, "No hosts in map"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__row_count_validation_escapes_schema_and_table_names(self):
        self.subject.escapeDoubleQuoteInSQLString.side_effect = ['"escapedSchema"', '"escapedTable"',
                                                                 '"escapedSchema"', '"escapedTable"']

        escaped_query = 'SELECT count(*) FROM "escapedSchema"."escapedTable"'

        table_mock = Mock(spec=['schema','table'])
        table_mock.schema = 'mySchema'
        table_mock.table = 'myTable'
        table_pair = Mock(spec=['source','dest'])
        table_pair.source = table_mock
        table_pair.dest = table_mock

        validator = self.subject.CountTableValidator('some_work_dir', table_pair, 'fake_db_connection',
                                                     'fake_db_connection')
        self.assertEqual(escaped_query, validator._src_sql)
        self.assertEqual(escaped_query, validator._dest_sql)

    def test__validate_good_range_partition_from_4_to_X(self):
        options = self.setup_partition_validation()

        singleton_side_effect = SingletonSideEffect()
        singleton_side_effect.replace("SELECT version()", [
            ["PostgreSQL 8.2.15 (Greenplum Database 4.3.11.3-rc1-2-g3725a31 build 1) on x86_64-unknown-linux-gnu, "
             "compiled by GCC gcc (GCC) 4.4.2 compiled on Jan 24 2017 14:17:39 (with assert checking)"],
            ["PostgreSQL 8.3.23 (Greenplum Database 5.0.0 build fdafasdf"],
        ])
        singleton_side_effect.replace("select parisdefault, parruleord, parrangestartincl,", [
            ["f", "1", "t", "t",
             "({CONST :consttype 1082 :constlen 4 :constbyval true :constisnull false :constvalue 4 []})",
             "({CONST :consttype 1082 :constlen 4 :constbyval true :constisnull false :constvalue 4 []})",
             "", ""],

            ["f", "1", "t", "t",
             "({CONST :consttype 1082 :consttypmod -1 :constlen 4 :constbyval true :constisnull false "
             ":constvalue 4 []})",
             "({CONST :consttype 1082 :consttypmod -1 :constlen 4 :constbyval true :constisnull false "
             ":constvalue 4 []})",
             "", ""],
        ])
        self.db_singleton.side_effect = singleton_side_effect.singleton_side_effect

        self.subject.GpTransfer(Mock(**options), [])

        self.assertIn("Validating partition table transfer set...", self.get_info_messages())


    def test__validate_good_list_partition_from_4_to_X(self):
        options = self.setup_partition_validation()

        singleton_side_effect = SingletonSideEffect()
        singleton_side_effect.replace("SELECT version()", [
            ["PostgreSQL 8.2.15 (Greenplum Database 4.3.11.3-rc1-2-g3725a31 build 1) on x86_64-unknown-linux-gnu, "
             "compiled by GCC gcc (GCC) 4.4.2 compiled on Jan 24 2017 14:17:39 (with assert checking)"],
            ["PostgreSQL 8.3.23 (Greenplum Database 5.0.0 build fdafasdf"],
        ])
        singleton_side_effect.replace("select parisdefault, parruleord, parrangestartincl,", [
            ["f", "1", "t", "t", "", "", "",
             "(({CONST :consttype 25 :constlen -1 :constbyval false :constisnull false :constvalue 8 "
             "[ 0 0 0 8 97 115 105 97 ]}))"],

            ["f", "1", "t", "t", "", "", "",
             "(({CONST :consttype 25 :consttypmod -1 :constlen -1 :constbyval false :constisnull false :constvalue 8 "
             "[ 0 0 0 8 97 115 105 97 ]}))"],
        ])
        singleton_side_effect.replace("select partitiontype", [["list"]])
        self.db_singleton.side_effect = singleton_side_effect.singleton_side_effect

        self.subject.GpTransfer(Mock(**options), [])

        self.assertIn("Validating partition table transfer set...", self.get_info_messages())

    def test__validate_good_multi_column_list_partition_from_4_to_X(self):
        options = self.setup_partition_validation()

        singleton_side_effect = SingletonSideEffect()
        singleton_side_effect.replace("SELECT version()", [
            ["PostgreSQL 8.2.15 (Greenplum Database 4.3.11.3-rc1-2-g3725a31 build 1) on x86_64-unknown-linux-gnu, "
             "compiled by GCC gcc (GCC) 4.4.2 compiled on Jan 24 2017 14:17:39 (with assert checking)"],
            ["PostgreSQL 8.3.23 (Greenplum Database 5.0.0 build fdafasdf"],
        ])
        singleton_side_effect.replace("select parisdefault, parruleord, parrangestartincl,", [
            ["f", "1", "t", "t", "", "", "",
             "(({CONST :consttype 1042 :constlen -1 :constbyval false :constisnull false :constvalue 5 [ 0 0 0 5 77 ]} "
             "{CONST :consttype 23 :constlen 4 :constbyval true :constisnull false :constvalue 4 [ 1 0 0 0 0 0 0 0 ]}))"],

            ["f", "1", "t", "t", "", "", "",
             "(({CONST :consttype 1042 :consttypmod 5 :constlen -1 :constbyval false :constisnull false :constvalue 5 "
             "[ 0 0 0 5 77 ]} {CONST :consttype 23 :consttypmod -1 :constlen 4 :constbyval true :constisnull false "
             ":constvalue 4 [ 1 0 0 0 0 0 0 0 ]}))"],
        ])
        singleton_side_effect.replace("select partitiontype", [["list"]])
        self.db_singleton.side_effect = singleton_side_effect.singleton_side_effect

        self.subject.GpTransfer(Mock(**options), [])

        self.assertIn("Validating partition table transfer set...", self.get_info_messages())

    def test__validate_good_multi_column_swapped_column_ordering_list_partition_with_same_version(self):
        options = self.setup_partition_validation()

        singleton_side_effect = SingletonSideEffect()
        singleton_side_effect.replace("SELECT version()", [
            ["PostgreSQL 8.3.23 (Greenplum Database 5.0.0 build fdafasdf"],
            ["PostgreSQL 8.3.23 (Greenplum Database 5.0.0 build fdafasdf"],
        ])
        singleton_side_effect.replace("select parisdefault, parruleord, parrangestartincl,", [
            ["f", "1", "t", "t", "", "", "",
             "(({CONST :consttype 1042 :consttypmod 5 :constlen -1 :constbyval false :constisnull false :constvalue 5 "
             "[ 0 0 0 5 77 ]} "
             "{CONST :consttype 23 :consttypmod -1 :constlen 4 :constbyval true :constisnull false :constvalue 4 "
             "[ 1 0 0 0 0 0 0 0 ]}))"],

            ["f", "1", "t", "t", "", "", "",
             "(({CONST :consttype 23 :consttypmod -1 :constlen 4 :constbyval true :constisnull false :constvalue 4 "
             "[ 1 0 0 0 0 0 0 0 ]} "
             "{CONST :consttype 1042 :consttypmod 5 :constlen -1 :constbyval false :constisnull false :constvalue 5 "
             "[ 0 0 0 5 77 ]}))"],
        ])
        singleton_side_effect.replace("select partitiontype", [["list"]])
        self.db_singleton.side_effect = singleton_side_effect.singleton_side_effect

        self.subject.GpTransfer(Mock(**options), [])

    def test__validate_good_multi_column_swapped_column_ordering_list_partition_from_4_to_X(self):
        options = self.setup_partition_validation()

        singleton_side_effect = SingletonSideEffect()
        singleton_side_effect.replace("SELECT version()", [
            ["PostgreSQL 8.2.15 (Greenplum Database 4.3.11.3-rc1-2-g3725a31 build 1) on x86_64-unknown-linux-gnu, "
             "compiled by GCC gcc (GCC) 4.4.2 compiled on Jan 24 2017 14:17:39 (with assert checking)"],
            ["PostgreSQL 8.3.23 (Greenplum Database 5.0.0 build fdafasdf"],
        ])
        singleton_side_effect.replace("select parisdefault, parruleord, parrangestartincl,", [
            ["f", "1", "t", "t", "", "", "",
             "(({CONST :consttype 1042 :constlen -1 :constbyval false :constisnull false :constvalue 5 [ 0 0 0 5 77 ]} "
             "{CONST :consttype 23 :constlen 4 :constbyval true :constisnull false :constvalue 4 [ 1 0 0 0 0 0 0 0 ]}))"],

            ["f", "1", "t", "t", "", "", "",
             "(({CONST :consttype 23 :consttypmod -1 :constlen 4 :constbyval true :constisnull false :constvalue 4 "
             "[ 1 0 0 0 0 0 0 0 ]} "
             "{CONST :consttype 1042 :consttypmod 5 :constlen -1 :constbyval false :constisnull false :constvalue 5 "
             "[ 0 0 0 5 77 ]}))"],
        ])
        singleton_side_effect.replace("select partitiontype", [["list"]])
        self.db_singleton.side_effect = singleton_side_effect.singleton_side_effect

        self.subject.GpTransfer(Mock(**options), [])

        self.assertIn("Validating partition table transfer set...", self.get_info_messages())

    def test__validate_max_line_length_below_minimum(self):
        options = self.setup_partition_to_normal_validation()
        options.update(max_line_length=1024*16)
        MIN_GPFDIST_MAX_LINE_LENGTH = 1024 * 32  # (32KB)
        MAX_GPFDIST_MAX_LINE_LENGTH = 1024 * 1024 * 256  # (256MB)
        with self.assertRaisesRegexp(Exception, "Invalid --max-line-length option.  Value must be between %d and %d" % (MIN_GPFDIST_MAX_LINE_LENGTH,
                                                                                                                        MAX_GPFDIST_MAX_LINE_LENGTH)):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_max_line_length_above_maximum(self):
        options = self.setup_partition_to_normal_validation()
        options.update(max_line_length=1024*1024*512)
        MIN_GPFDIST_MAX_LINE_LENGTH = 1024 * 32  # (32KB)
        MAX_GPFDIST_MAX_LINE_LENGTH = 1024 * 1024 * 256  # (256MB)
        with self.assertRaisesRegexp(Exception, "Invalid --max-line-length option.  Value must be between %d and %d" % (MIN_GPFDIST_MAX_LINE_LENGTH,
                                                                                                                        MAX_GPFDIST_MAX_LINE_LENGTH)):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_max_line_length_valid(self):
        options = self.setup_partition_to_normal_validation()
        options.update(max_line_length=1024*1024)
        MIN_GPFDIST_MAX_LINE_LENGTH = 1024 * 32  # (32KB)
        MAX_GPFDIST_MAX_LINE_LENGTH = 1024 * 1024 * 256  # (256MB)
        self.subject.GpTransfer(Mock(**options), [])

    ####################################################################################################################
    # End of tests, start of private methods/objects
    ####################################################################################################################

    def get_error_logging(self):
        return [args[0][0] for args in self.subject.logger.error.call_args_list]

    def get_info_messages(self):
        return [args[0][0] for args in self.subject.logger.info.call_args_list]

    def get_warnings(self):
        warnings = [args[0][0] for args in self.subject.logger.warning.call_args_list]
        warns = [args[0][0] for args in self.subject.logger.warn.call_args_list]
        return warnings + warns

    def _get_gptransfer_command(self):
        gptransfer = self.subject
        cmd_args = self.GpTransferCommand_args

        src_args = ('src', 'public', 'foo', False)
        dest_args = ('dest', 'public', 'foo', False)
        source_table = gptransfer.GpTransferTable(*src_args)
        dest_table = gptransfer.GpTransferTable(*dest_args)
        cmd_args['table_pair'] = gptransfer.GpTransferTablePair(source_table, dest_table)
        return gptransfer.GpTransferCommand(**cmd_args)


    def run_range_partition_value(self, additional):
        options = self.setup_partition_validation()
        self.db_singleton.side_effect = SingletonSideEffect(additional).singleton_side_effect
        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])
        log_messages = self.get_error_logging()
        self.assertIn("Range partition value is different between", log_messages[0])
        self.assertIn("Partition value is different in the partition hierarchy between", log_messages[1])

    def createGpArrayWith2Primary2Mirrors(self):
        master = GpDB.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|/data/master")
        primary0 = GpDB.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        primary1 = GpDB.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
        mirror0 = GpDB.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        mirror1 = GpDB.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")
        return GpArray([master, primary0, primary1, mirror0, mirror1])

    def setup_partition_validation(self):
        source_map_filename = tempfile.NamedTemporaryFile(dir=self.TEMP_DIR, delete=False)
        source_map_filename.write("sdw1,12700\nsdw2,12700")
        input_filename = tempfile.NamedTemporaryFile(dir=self.TEMP_DIR, delete=False)
        input_filename.write("my_first_database.public.my_table_partition1")
        self.cursor.side_effect = CursorSideEffect().cursor_side_effect
        self.db_singleton.side_effect = SingletonSideEffect().singleton_side_effect
        options = {}
        options.update(self.GpTransfer_options_defaults)
        options.update(
            partition_transfer=True,
            input_file=input_filename.name,
            source_map_file=source_map_filename.name,
            base_port=15432,
            max_line_length=32768,
            work_base_dir="/tmp",
            source_port=45432,
            dest_port=15432,
        )
        return options

    def setup_partition_to_normal_validation(self):
        source_map_filename = tempfile.NamedTemporaryFile(dir=self.TEMP_DIR, delete=False)
        source_map_filename.write("sdw1,12700\nsdw2,12700")
        input_filename = tempfile.NamedTemporaryFile(dir=self.TEMP_DIR, delete=False)
        input_filename.write("my_first_database.public.my_table_partition1, "
                             "my_second_database.public.my_normal_table")

        additional = {
            cursor_keys['relations']: ["my_relname", "another_rel"],
        }
        self.cursor.side_effect = CursorSideEffect(additional=additional).cursor_side_effect

        self.db_singleton.side_effect = SingletonSideEffect().singleton_side_effect
        options = {}
        options.update(self.GpTransfer_options_defaults)
        options.update(
            partition_transfer_non_pt_target=True,
            input_file=input_filename.name,
            source_map_file=source_map_filename.name,
            base_port=15432,
            max_line_length=32768,
            work_base_dir="/tmp",
            source_port=45432,
            dest_port=15432,
        )
        return options

    def setup_normal_to_normal_validation(self):
        source_map_filename = tempfile.NamedTemporaryFile(dir=self.TEMP_DIR, delete=False)
        source_map_filename.write("sdw1,12700\nsdw2,12700")
        source_map_filename.flush()
        input_filename = tempfile.NamedTemporaryFile(dir=self.TEMP_DIR, delete=False)
        input_filename.write("my_first_database.public.my_normal_table")
        input_filename.flush()
        additional = {
            cursor_keys["normal_tables"]: [["public", "my_normal1_table", ""]],
        }
        cursor_side_effect = CursorSideEffect(additional=additional)
        cursor_side_effect.second_values["normal_tables"] = [[]]
        self.cursor.side_effect = cursor_side_effect.cursor_side_effect

        self.db_singleton.side_effect = SingletonSideEffect().singleton_side_effect
        options = {}
        options.update(self.GpTransfer_options_defaults)
        options.update(
            input_file=input_filename.name,
            source_map_file=source_map_filename.name,
            base_port=15432,
            max_line_length=32768,
            work_base_dir="/tmp",
            source_port=45432,
            dest_port=15432,
        )
        return options


class CursorSideEffect:
    def __init__(self, additional=None):
        self.first_values = {
            cursor_keys["normal_tables"]: [["public", "my_normal_table", ""]],
            cursor_keys["partition_tables"]: [["public", "my_table_partition1", ""]],
            cursor_keys['relations']: ["my_relname"],
            cursor_keys['table_info']: [
                ["t", "my_data_type", 255, 16, 1024, 1024, 1, 1024, "my_interval_type", "my_udt_name"]],
            cursor_keys['partition_info']: [["my_parkind", 1, 1, "1"]],
            cursor_keys['schema_name']: ["public"],
            cursor_keys['create_schema']: ["my_schema"],
            cursor_keys['ordinal_pos']: [[1]],
        }
        self.counters = dict((key, 0) for key in self.first_values.keys())
        self.second_values = self.first_values.copy()
        if additional:
            self.second_values.update(additional)

    def cursor_side_effect(self, *args):
        for key in self.first_values.keys():
            for arg in args[1:]:
                arg_oneline = " ".join(arg.split("\n"))
                if key.search(arg_oneline):
                    if self.has_called(key):
                        return FakeCursor(self.second_values[key])
                    return FakeCursor(self.first_values[key])
        return None

    def has_called(self, key):
        self.counters[key] += 1
        return self.counters[key] > 1

    def append_regexp_key(self, key, value):
        self.first_values[key] = value
        self.second_values[key] = value
        self.counters[key] = 0


class SingletonSideEffect:
    """
    Mocks out the results of execSQLForSingletonRow.
    Any values which are provided as lists, using the "replace()" verb,
    will be returned as a "side-effect" in the order provided.
    """
    def __init__(self, additional_column=None, additional_col_list=None):
        self.values = {
            "select partitiontype": ["range"],
            "select max(p1.partitionlevel)": [1],
            "select schemaname, tablename from pg_catalog.pg_partitions": ["public", "my_table_partition1"],
            "select c.oid": ["oid1", "oid1"],
            "select parisdefault, parruleord, parrangestartincl,": ["f", "1", "t", "t", 100, 10, "", ""],
            "select n.nspname, c.relname": ["public", "my_table_partition1"],
            "SELECT count(*) FROM": [20],
            "SELECT version()": ["PostgreSQL 8.3.23 (Greenplum Database 5.0.0 build fdafasdf"],
        }

        self.counters = dict((key, 0) for key in self.values.keys())
        self.values = dict((key, [values]) for (key, values) in self.values.iteritems())

        # allow additional column value(s) to be added via parameters
        for key in self.values.keys():
            if additional_column:
                if key in additional_column:
                    values = self.values[key]
                    values.append(additional_column[key])
            if additional_col_list:
                if key in additional_col_list:
                    values = self.values[key]
                    values.extend(additional_col_list[key])

    def singleton_side_effect(self, *args):
        for key in self.values.keys():
            for arg in args:
                if key in arg:
                    value_list = self.values[key]
                    result = value_list[self.counters[key] % len(value_list)]
                    self.counters[key] += 1
                    return result
        return None

    def replace(self, key, value):
        self.counters[key] = 0
        self.values[key] = value


if __name__ == '__main__':
    run_tests()
