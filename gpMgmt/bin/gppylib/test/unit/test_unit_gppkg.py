from mock import *
from gp_unittest import *
from gppylib.programs.gppkg import GpPkgProgram

import shutil
import sys

class GpPkgProgramTestCase(GpTestCase):
    def setUp(self):
        self.mock_cmd = Mock()
        self.mock_list_files_by_pattern = Mock()
        self.mock_gppkg = Mock()
        self.mock_uninstall_package = Mock()

        self.apply_patches([
            patch('gppylib.operations.package.logger', return_value=Mock(spec=['log', 'info', 'debug', 'error'])),
            patch('gppylib.programs.gppkg.Command', return_value=self.mock_cmd),
            patch('gppylib.programs.gppkg.ListFilesByPattern', return_value=self.mock_list_files_by_pattern),
            patch('gppylib.programs.gppkg.Gppkg', return_value=self.mock_gppkg),
            patch('gppylib.programs.gppkg.UninstallPackage', return_value=self.mock_uninstall_package),
        ])
        self.mock_logger = self.get_mock_from_apply_patch('logger')

    def tearDown(self):
        super(GpPkgProgramTestCase, self).tearDown()

    def test__remove_raises_when_gppkg_was_not_installed(self):
        sys.argv = ["gppkg", "--remove", "sample"]
        get_result_mock = Mock()
        get_result_mock.stdout.strip.return_value = "RPM version 4.8.0"

        self.mock_list_files_by_pattern.run.return_value = []
        self.mock_cmd.get_results.return_value = get_result_mock

        parser = GpPkgProgram.create_parser()
        options, args = parser.parse_args()
        with self.assertRaisesRegexp(Exception, "Package sample has not been installed"):
            self.subject = GpPkgProgram(options, args)
            self.subject.run()

    def test__remove_succeeds_when_gppkg_had_been_installed(self):
        sys.argv = ["gppkg", "--remove", "sample"]
        get_result_mock = Mock()
        get_result_mock.stdout.strip.return_value = "RPM version 4.8.0"

        self.mock_cmd.get_results.return_value = get_result_mock
        self.mock_list_files_by_pattern.run.return_value = ['sample.gppkg']
        self.mock_gppkg.from_package_path.return_value = []
        self.mock_uninstall_package.run.return_value = None

        parser = GpPkgProgram.create_parser()
        options, args = parser.parse_args()
        self.subject = GpPkgProgram(options, args)
        self.subject.run()

        self.mock_list_files_by_pattern.run.assert_called_once()
        self.mock_uninstall_package.run.assert_called_once()
