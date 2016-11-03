import imp
import os
from mock import *
from gp_unittest import *
from gparray import GpDB, GpArray
from gppylib.db.dbconn import UnexpectedRowsError
from pygresql import pgdb


class GpTransfer(GpTestCase):
    def setUp(self):
        # because gptransfer does not have a .py extension,
        # we have to use imp to import it
        # if we had a gptransfer.py, this is equivalent to:
        #   import gptransfer
        #   self.subject = gptransfer
        gptransfer_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gptransfer")
        self.subject = imp.load_source('gptransfer', gptransfer_file)
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning'])
        self.gparray = self.createGpArrayWith2Primary2Mirrors()

        self.db_connection = MagicMock(spec=["__exit__", "close", "__enter__"])
        self.cursor = MagicMock(spec=pgdb.pgdbCursor)
        self.db_singleton = Mock()

        self.apply_patches([
            patch('os.environ', new={}),
            patch('gppylib.operations.dump.GpArray.initFromCatalog', return_value=self.gparray),
            patch('gptransfer.connect', return_value=self.db_connection),
            patch('gptransfer.getUserDatabaseList', return_value=[["my_first_database"],["my_second_database"]]),
            patch('gppylib.db.dbconn.connect', return_value=self.db_connection),
            patch('gptransfer.WorkerPool', return_value=Mock()),
            patch('gptransfer.doesSchemaExist', return_value=True),
            patch('gptransfer.dropSchemaIfExist'),
            patch('gptransfer.execSQL', new=self.cursor),
            patch('gptransfer.execSQLForSingletonRow', new=self.db_singleton),
            patch("gppylib.commands.unix.FileDirExists.remote", return_value=True)
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
            interactive=True,
            last_port=-1,
            logfileDirectory=None,
            max_gpfdist_instances=1,
            max_line_length=10485760,
            no_final_count_validation=False,
            partition_transfer=False,
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
            wait_time=3,
            work_base_dir='/home/gpadmin/',
        )

    @patch('gptransfer.TableValidatorFactory', return_value=Mock())
    @patch('gptransfer.execSQLForSingletonRow', side_effect=[['MYDATE'],
                                                             ['MY"DATE'],
                                                             ['''MY'DATE'''],
                                                             ['MY""DATE']])
    def test__get_distributed_by_with_special_characters_on_column(self, mock1,
                                                                   mock2):
        gptransfer = self.subject
        cmd_args = self.GpTransferCommand_args

        src_args = ('src', 'public', 'foo', False)
        dest_args = ('dest', 'public', 'foo', False)
        source_table = gptransfer.GpTransferTable(*src_args)
        dest_table = gptransfer.GpTransferTable(*dest_args)
        cmd_args['table_pair'] = gptransfer.GpTransferTablePair(source_table,
                                                                dest_table)
        table_validator = gptransfer.GpTransferCommand(**cmd_args)

        expected_results = ['MYDATE',
                            'MY""DATE',
                            'MY\'DATE',
                            'MY""""DATE']
        for res in expected_results:
            expected_distribution = '''DISTRIBUTED BY ("%s")''' % res
            result_distribution = table_validator._get_distributed_by()
            self.assertEqual(expected_distribution, result_distribution)

    @patch('gptransfer.TableValidatorFactory', return_value=Mock())
    @patch('gptransfer.execSQLForSingletonRow',
           side_effect=[UnexpectedRowsError(1, 0, "sql foo"),
                        ""])
    def test__get_distributed_randomly(self, mock1, mock2):
        gptransfer = self.subject
        cmd_args = self.GpTransferCommand_args

        src_args = ('src', 'public', 'foo', False)
        dest_args = ('dest', 'public', 'foo', False)
        source_table = gptransfer.GpTransferTable(*src_args)
        dest_table = gptransfer.GpTransferTable(*dest_args)
        cmd_args['table_pair'] = gptransfer.GpTransferTablePair(source_table,
                                                                dest_table)
        table_validator = gptransfer.GpTransferCommand(**cmd_args)

        expected_distribution = '''DISTRIBUTED RANDOMLY'''
        result_distribution = table_validator._get_distributed_by()
        self.assertEqual(0, len(self.subject.logger.method_calls))
        self.assertEqual(expected_distribution, result_distribution)

        result_distribution = table_validator._get_distributed_by()
        self.assertEqual(1, len(self.subject.logger.method_calls))
        self.assertEqual(expected_distribution, result_distribution)

    def test__validates_good_partition(self):
        options = self.setup_partition_validation()
        self.cursor.side_effect = CursorSideEffect().cursor_side_effect

        self.subject.GpTransfer(Mock(**options), [])

    def test__validate_bad_partition_not_leaf(self):
        options = self.setup_partition_validation()

        additional = {
            "select relname from pg_class r": ["many", "relations"],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "Destination table "):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_bad_partition_different_number_columns(self):
        options = self.setup_partition_validation()

        additional = {
            "select ordinal_position, is_nullable, data_type, character_maximum_length,": [[1, "t", "my_data_type", 255, 16, 1024, 1024, 1, 1024, "my_interval_type", "my_udt_name"],
                                                                                           [2, "t", "my_data_type", 255, 16, 1024, 1024, 1, 1024, "my_interval_type", "my_udt_name"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different column layout or types"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_bad_partition_different_column_type(self):
        options = self.setup_partition_validation()

        additional = {
            "select ordinal_position, is_nullable, data_type, character_maximum_length,": [[1, "t", "my_new_data_type", 255, 16, 1024, 1024, 1, 1024, "my_interval_type", "my_udt_name"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different column layout or types"):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_bad_partition_different_max_levels(self):
        options = self.setup_partition_validation()

        additional = {
            "select max(p1.partitionlevel)": [2],
        }
        self.db_singleton.side_effect = SingletonSideEffect(additional).singleton_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = [args[0][0] for args in self.subject.logger.error.call_args_list]
        self.assertIn("Max level of partition is not same between", log_messages[0])

    def test__validate_bad_partition_different_values_of_attributes(self):
        options = self.setup_partition_validation()

        additional = {
            "select parkind, parlevel, parnatts, paratts": [["my_parkind", 1, "my_parnatts", "3 4"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = [args[0][0] for args in self.subject.logger.error.call_args_list]
        self.assertIn("Partition type or key is different between", log_messages[1])
        self.assertIn("Partition column attributes are different at level", log_messages[0])

    def test__validate_bad_partition_different_parent_kind(self):
        options = self.setup_partition_validation()

        additional = {
            "select parkind, parlevel, parnatts, paratts": [["different_parkind", 1, "my_parnatts", "my_paratts"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = [args[0][0] for args in self.subject.logger.error.call_args_list]
        self.assertIn("Partition type or key is different between", log_messages[1])
        self.assertIn("Partition type is different at level", log_messages[0])

    def test__validate_bad_partition_different_number_of_attributes(self):
        options = self.setup_partition_validation()

        additional = {
            "select parkind, parlevel, parnatts, paratts": [["my_parkind", 1, 2, "my_paratts"]],
        }
        self.cursor.side_effect = CursorSideEffect(additional).cursor_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = [args[0][0] for args in self.subject.logger.error.call_args_list]
        self.assertIn("Partition type or key is different between", log_messages[1])
        self.assertIn("Number of partition columns is different at level ", log_messages[0])

    def test__validate_bad_partition_different_partition_values(self):
        options = self.setup_partition_validation()

        additional = {
            "select n.nspname, c.relname": [["not_public", "not_my_table", ""],["public", "my_table", ""]],
            "select parisdefault, parruleord, parrangestartincl,": ["t", "1", "t", "t", 100, 10, "", ""],
        }
        self.db_singleton.side_effect = SingletonSideEffect(additional).singleton_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = [args[0][0] for args in self.subject.logger.error.call_args_list]
        self.assertIn("One of the subpartition table is a default partition", log_messages[0])
        self.assertIn("Partition value is different in the partition hierarchy between", log_messages[1])

    def test__validate_bad_partition_unknown_type(self):
        options = self.setup_partition_validation()
        my_singleton = SingletonSideEffect()
        my_singleton.values["select partitiontype"] = ["unknown"]
        self.db_singleton.side_effect = my_singleton.singleton_side_effect

        with self.assertRaisesRegexp(Exception, "Unknown partitioning type "):
            self.subject.GpTransfer(Mock(**options), [])

    def test__validate_bad_partition_different_list_values(self):
        options = self.setup_partition_validation()

        additional = {
            "select parisdefault, parruleord, parrangestartincl,": ["f", "1", "t", "t", 100, 10, "", "different"],
        }

        my_singleton = SingletonSideEffect(additional)
        my_singleton.values["select partitiontype"] = [["list"]]
        self.db_singleton.side_effect = my_singleton.singleton_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = [args[0][0] for args in self.subject.logger.error.call_args_list]
        self.assertIn("List partition value is different between", log_messages[0])
        self.assertIn("Partition value is different in the partition hierarchy between", log_messages[1])

    def test__validate_bad_partition_different_range_values(self):
        self.run_range_partition_value({"select parisdefault, parruleord, parrangestartincl,": ["f", "1", "f", "t", 100, 10, "", "different"]})
        self.run_range_partition_value({"select parisdefault, parruleord, parrangestartincl,": ["f", "1", "t", "f", 999, 10, "", "different"]})
        self.run_range_partition_value({"select parisdefault, parruleord, parrangestartincl,": ["f", "1", "t", "t", 100, 999, "", "different"]})
        self.run_range_partition_value({"select parisdefault, parruleord, parrangestartincl,": ["f", "1", "t", "t", 100, 10, 999, "different"]})

    def test__validate_bad_partition_different_parent_partition(self):
        options = self.setup_partition_validation()

        multi = {
            "select parisdefault, parruleord, parrangestartincl,": [["f", "1", "t", "t", 100, 10, "", ""], ["f", "1", "t", "t", 100, 10, "", ""], ["f", "1", "t", "t", 999, 10, "", ""]],
        }
        singleton_side_effect = SingletonSideEffect(multi_list=multi)
        self.db_singleton.side_effect = singleton_side_effect.singleton_side_effect

        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])

        log_messages = [args[0][0] for args in self.subject.logger.error.call_args_list]
        self.assertIn("Range partition value is different between source partition table", log_messages[0])
        self.assertIn("Partitions have different parents at level", log_messages[1])


####################################################################################################################
        # End of tests, start of private methods/objects
####################################################################################################################

    def run_range_partition_value(self, additional):
        options = self.setup_partition_validation()
        self.db_singleton.side_effect = SingletonSideEffect(additional).singleton_side_effect
        with self.assertRaisesRegexp(Exception, "has different partition criteria from destination table"):
            self.subject.GpTransfer(Mock(**options), [])
        log_messages = [args[0][0] for args in self.subject.logger.error.call_args_list]
        self.assertIn("Range partition value is different between", log_messages[0])
        self.assertIn("Partition value is different in the partition hierarchy between", log_messages[1])

    def createGpArrayWith2Primary2Mirrors(self):
        master = GpDB.initFromString("1|-1|p|p|s|u|mdw|mdw|5432|None|/data/master||/data/master/base/10899,/data/master/base/1,/data/master/base/10898,/data/master/base/25780,/data/master/base/34782")
        primary0 = GpDB.initFromString("2|0|p|p|s|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        primary1 = GpDB.initFromString("3|1|p|p|s|u|sdw2|sdw2|40001|41001|/data/primary1||/data/primary1/base/10899,/data/primary1/base/1,/data/primary1/base/10898,/data/primary1/base/25780,/data/primary1/base/34782")
        mirror0 = GpDB.initFromString("4|0|m|m|s|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
        mirror1 = GpDB.initFromString("5|1|m|m|s|u|sdw1|sdw1|50001|51001|/data/mirror1||/data/mirror1/base/10899,/data/mirror1/base/1,/data/mirror1/base/10898,/data/mirror1/base/25780,/data/mirror1/base/34782")
        return GpArray([master, primary0, primary1, mirror0, mirror1])

    def setup_partition_validation(self):
        os.environ["GPHOME"] = "my_gp_home"
        SOURCE_MAP_FILENAME = "/tmp/gptransfer_test_source_map"
        with open(SOURCE_MAP_FILENAME, "w") as src_map_file:
            src_map_file.write("sdw1,12700\nsdw2,12700")
        INPUT_FILENAME = "/tmp/gptransfer_test"
        with open(INPUT_FILENAME, "w") as src_map_file:
            src_map_file.write("my_first_database.public.my_table")
        self.cursor.side_effect = CursorSideEffect().cursor_side_effect
        self.db_singleton.side_effect = SingletonSideEffect().singleton_side_effect
        options = {}
        options.update(self.GpTransfer_options_defaults)
        options.update(
            partition_transfer=True,
            input_file=INPUT_FILENAME,
            source_map_file=SOURCE_MAP_FILENAME,
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
            "n.nspname, c.relname, c.relstorage": [["public", "my_table", ""]],
            "select relname from pg_class r": ["my_relname"],
            "select ordinal_position, is_nullable, data_type, character_maximum_length,": [[1, "t", "my_data_type", 255, 16, 1024, 1024, 1, 1024, "my_interval_type", "my_udt_name"]],
            "select parkind, parlevel, parnatts, paratts": [["my_parkind", 1, "my_parnatts", "my_paratts"]],
            "SELECT fsname FROM pg_catalog.pg_filespace": ["public"],
        }
        self.counters = dict((key, 0) for key in self.first_values.keys())
        self.second_values = self.first_values.copy()
        if additional:
            self.second_values.update(additional)

    def cursor_side_effect(self, *args):
        for key in self.first_values.keys():
            for arg in args[1:]:
                if key in arg:
                    if self.has_called(key):
                        return FakeCursor(self.second_values[key])
                    return FakeCursor(self.first_values[key])
        return None

    def has_called(self, key):
        self.counters[key] += 1
        return self.counters[key] > 1


class FakeCursor:
    def __init__(self, my_list):
        self.list = list([[""]])
        if my_list:
            self.list = my_list
        self.rowcount = len(self.list)

    def __iter__(self):
        return iter(self.list)

    def close(self):
        pass

class SingletonSideEffect:
    def __init__(self, additional=None, multi_list=None):
        self.values = {
                "select partitiontype": ["range"],
                "select max(p1.partitionlevel)": [1],
                "select schemaname, tablename from pg_catalog.pg_partitions": ["public", "my_table"],
                "select c.oid": ["oid1", "oid1"],
                "select parisdefault, parruleord, parrangestartincl,": ["f", "1", "t", "t", 100, 10, "", ""],
                "select n.nspname, c.relname": ["public", "my_table"]
        }

        self.counters = dict((key, 0) for key in self.values.keys())
        # make values into list to accommodate multiple sequential values
        self.values = dict((key, [value]) for (key, value) in self.values.iteritems())
        for key in self.values.keys():
            if additional:
                if key in additional:
                    value = self.values[key]
                    value.append(additional[key])
            if multi_list:
                if key in multi_list:
                    value = self.values[key]
                    value.extend(multi_list[key])

    def singleton_side_effect(self, *args):
        for key in self.values.keys():
            for arg in args:
                if key in arg:
                    value_list = self.values[key]
                    result = value_list[self.counters[key] % len(value_list)]
                    self.counters[key] += 1
                    return result
        return None


if __name__ == '__main__':
    run_tests()
