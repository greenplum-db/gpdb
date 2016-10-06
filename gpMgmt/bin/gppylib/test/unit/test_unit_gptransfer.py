import imp
import os
from mock import *
from gp_unittest import *
from gppylib.db.dbconn import UnexpectedRowsError


class GpTransfer(GpTestCase):
    def setUp(self):
        # because gptransfer does not have a .py extension,
        # we have to use imp to import it
        # if we had a gptransfer.py, this is equivalent to:
        #   import gptransfer
        #   self.subject = gptransfer
        gptransfer_file = os.path.abspath(os.path.dirname(__file__) +
                                          "/../../../gptransfer")
        self.subject = imp.load_source('gptransfer',
                                       gptransfer_file)
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error'])

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

if __name__ == '__main__':
    run_tests()
