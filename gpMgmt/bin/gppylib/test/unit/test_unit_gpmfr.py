import imp
import os
import io
from threading import Event

from mock import *
from gp_unittest import *
from gppylib.commands.base import CommandResult

DDBOOST_CONFIG_REMOTE_INFO= """
Data Domain Hostname:ddremote
Data Domain Boost Username:metro
Data Domain default log level:WARNING
Data Domain default log size:50
"""

DDBOOST_CONFIG_INFO= """
Data Domain Hostname:ddlocal
Data Domain Boost Username:metro
Default Backup Directory:MY_BACKUP_DIR
Data Domain Storage Unit:MY_STORAGE_UNIT_NAME
Data Domain default log level:WARNING
Data Domain default log size:50
"""

class GpMfrTestCase(GpTestCase):
    def setUp(self):

        # load subject after setting env vars
        gpmfr_path = os.path.abspath(os.path.dirname(__file__) + "/../../../gpmfr.py")
        # GOOD_MOCK_EXAMPLE of environment variables
        with patch.dict('os.environ', values = {'GPHOME': 'foo'}):
            self.subject = imp.load_source('gpmfr', gpmfr_path)

        self.subject.logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn'])

        self.popen = Mock()
        self.popen.pid = 3
        self.popen.stdout.return_value = Mock()
        # self.popen.pid.return_value =
        self.popen.stdout.readline.return_value = "5 packets received 0% packet loss"
        self.popen.communicate.return_value = ('foo', "")
        self.popen_class = Mock(return_value=self.popen)

        # commands return CommandResults
        self.mock_init = Mock(return_value=None)

        self.apply_patches([
            patch("gpmfr.gpsubprocess.Popen", self.popen_class),
            patch('gpmfr.Command.__init__', self.mock_init),
            patch('gpmfr.Command.was_successful', return_value=True),
            patch('gpmfr.Command.run', return_value=None),
        ])

    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(0, DDBOOST_CONFIG_INFO, "", True, False))
    def test_listBackups_withDDBoost_should_issue_correct_command(self, mock_results):
        p = self.subject.mfr_parser()

        mfropt, mfrargs = p.parse_args(['--list'], None)
        mfr = self.subject.GpMfr(mfropt, mfrargs)

        mfr.run()

        self.mock_init.assert_any_call('DD Boost on master', 'foo/bin/gpddboost --ls MY_BACKUP_DIR --ddboost-storage-unit=MY_STORAGE_UNIT_NAME' )

    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(0, DDBOOST_CONFIG_INFO, "", True, False))
    def test_verify_login_local_ddsystem_issue_correct_command(self, mock_results):
        ddsystem = self.subject.DDSystem('local', dd_storage_unit='MY_STORAGE_UNIT_NAME')

        ddsystem.verifyLogin()

        self.mock_init.assert_any_call('DD Boost on master', 'foo/bin/gpddboost --verify --ddboost-storage-unit MY_STORAGE_UNIT_NAME' )

    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(0, DDBOOST_CONFIG_INFO, "", True, False))
    def test_verify_login_remote_ddsystem_issue_correct_command(self, mock_results):
        ddsystem = self.subject.DDSystem('remote', dd_storage_unit='MY_STORAGE_UNIT_NAME')

        ddsystem.verifyLogin()

        self.mock_init.assert_any_call('DD Boost on master', 'foo/bin/gpddboost --verify --ddboost-storage-unit MY_STORAGE_UNIT_NAME --remote' )

    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(0, DDBOOST_CONFIG_INFO, "", True, False))
    def test_delete_file_on_ddsystem_issue_correct_command(self, mock_results):
        ddsystem = self.subject.DDSystem('local', dd_storage_unit='MY_STORAGE_UNIT_NAME')
        path = 'foo/20160101/gp_dump_1_1_20160101122346.gz'

        ddsystem.deleteFile(path)

        self.mock_init.assert_any_call('DD Boost on master', 'foo/bin/gpddboost --del-file foo/20160101/gp_dump_1_1_20160101122346.gz --ddboost-storage-unit=MY_STORAGE_UNIT_NAME' )

    @patch('gppylib.commands.base.Command.get_results', side_effect=[
        CommandResult(0, DDBOOST_CONFIG_INFO, "", True, False),
        CommandResult(0, DDBOOST_CONFIG_REMOTE_INFO, "", True, False),
        CommandResult(0, DDBOOST_CONFIG_INFO, "", True, False),
        CommandResult(0, "gp_dump_1_1_20160101122346.gz	600	691", "", True, False),
        CommandResult(0, DDBOOST_CONFIG_REMOTE_INFO, "", True, False),
        CommandResult(0, "Replication Source Streams  : 75", "", True, False),
        CommandResult(0, "Replication Source Streams  : 70", "", True, False),
        CommandResult(0, "Replication Destination Streams  : 80", "", True, False),
        CommandResult(0, "Used Filecopy Streams   : 0", "", True, False),
        CommandResult(0, "Used Filecopy Streams   : 0", "", True, False),
        CommandResult(0, "Replication gp_dump_1_1_20160101122346.gz completed 100 percent 691 bytes", "", True, False),
        CommandResult(0, "Used Filecopy Streams   : 0", "", True, False),
    ])
    def test_replicate_on_ddsystem_issues_correct_message(self, mock_results):

        with patch('gpmfr.ReplicateWorker') as mock_rep_class:
            mock_worker = Mock()
            mock_worker.isFailed.return_value = False
            mock_worker.isFinished.return_value = True
            mock_worker.bytesSent = 691
            mock_rep_class.return_value = mock_worker

            p = self.subject.mfr_parser()
            mfropt, mfrargs = p.parse_args(['--replicate', '20160101122346', '--max-streams', 60, '--master-port', '5432'], None)
            mfr = self.subject.GpMfr(mfropt, mfrargs)

            expected_message1 = 'Initiating transfer for 1 files from local(ddlocal) to remote(ddremote) Data Domain.'
            expected_message2 = 'Backup \'2016-January-01 12:23:46 (20160101122346)\' transferred from local(ddlocal) to remote(ddremote) Data Domain.'

            # GOOD_MOCK_EXAMPLE of stdout
            with patch('sys.stdout', new=io.BytesIO()) as mock_stdout:
                mfr.run()
                self.assertIn(expected_message1, mock_stdout.getvalue())
                self.assertIn(expected_message2, mock_stdout.getvalue())

    @patch('gpmfr.Thread')
    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(0, DDBOOST_CONFIG_INFO, "", True, False))
    def test_replicate_worker_issues_correct_command_for_replicate(self, mock_results, mock_thread_class):
        bfile, remoteDD, sourceDD = self.__setup_DDBoost_info()
        self.popen.stdout.readline.side_effect = [""]
        worker = self.subject.ReplicateWorker(bfile, sourceDD, remoteDD, Event(), Event())

        worker.run()

        self.popen_class.assert_any_call(['foo/bin/gpddboost', '--replicate', '--from-file', 'localBackupDir/20160421/foo.txt', '--to-file', 'localBackupDir/20160421/foo.txt', '--ddboost-storage-unit', 'localStorageUnit'], stderr=-2, stdout=-1)

    @patch('gpmfr.Thread')
    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(0, DDBOOST_CONFIG_INFO, "", True, False))
    def test_replicate_worker_issues_correct_command_for_recover(self, mock_results, mock_thread_class):
        bfile, remoteDD, sourceDD = self.__setup_DDBoost_info()
        self.popen.stdout.readline.side_effect = [""]
        worker = self.subject.ReplicateWorker(bfile, remoteDD, sourceDD, Event(), Event())

        worker.run()

        self.popen_class.assert_any_call(['foo/bin/gpddboost', '--recover', '--from-file', 'localBackupDir/20160421/foo.txt', '--to-file', 'localBackupDir/20160421/foo.txt', '--ddboost-storage-unit', 'localStorageUnit'], stderr=-2, stdout=-1)

    def __setup_DDBoost_info(self):
        sourceDD = self.subject.DDSystem('local', 'localBackupDir', 'localStorageUnit')
        remoteDD = self.subject.DDSystem('remote', 'remoteBackupDir', 'remoteStorageUnit')
        bfile = self.subject.BackupFile("foo.txt", 755, 1234)
        bfile.fullPath = "/".join([sourceDD.DDBackupDir, '20160421', bfile.name])
        return bfile, remoteDD, sourceDD

if __name__ == '__main__':
    run_tests()
