from mock import *
from .gp_unittest import *
from gppylib.programs.gppkg import GpPkgProgram

import sys


class GpPkgProgramTestCase(GpTestCase):
    def setUp(self):
        self.mock_cmd = Mock()
        self.mock_gppkg = Mock()
        self.mock_uninstall_package = Mock()

        self.apply_patches([
            patch('gppylib.operations.package.logger', return_value=Mock(spec=['log', 'info', 'debug', 'error'])),
            patch('gppylib.programs.gppkg.Command', return_value=self.mock_cmd),
            patch('gppylib.programs.gppkg.Gppkg', return_value=self.mock_gppkg),
            patch('gppylib.programs.gppkg.UninstallPackage', return_value=self.mock_uninstall_package),
            patch('os.listdir')
        ])
        self.mock_logger = self.get_mock_from_apply_patch('logger')
        self.mock_listdir = self.get_mock_from_apply_patch('listdir')

    def test__remove_raises_when_gppkg_was_not_installed(self):
        sys.argv = ["gppkg", "--remove", "sample"]
        get_result_mock = Mock()
        get_result_mock.stdout.strip.return_value = "RPM version 4.8.0"

        self.mock_listdir.return_value = ['another.gppkg']
        self.mock_cmd.get_results.return_value = get_result_mock

        parser = GpPkgProgram.create_parser()
        options, args = parser.parse_args()
        with self.assertRaisesRegex(Exception, "Package sample has not been installed"):
            self.subject = GpPkgProgram(options, args)
            self.subject.run()

    def test__remove_succeeds_when_gppkg_had_been_installed(self):
        sys.argv = ["gppkg", "--remove", "sample"]
        get_result_mock = Mock()
        get_result_mock.stdout.strip.return_value = "RPM version 4.8.0"

        self.mock_cmd.get_results.return_value = get_result_mock
        self.mock_listdir.return_value = ['sample.gppkg', 'another.gppkg', 'sample2.gppkg']

        self.mock_gppkg.from_package_path.return_value = []
        self.mock_uninstall_package.run.return_value = None

        parser = GpPkgProgram.create_parser()
        options, args = parser.parse_args()
        self.subject = GpPkgProgram(options, args)
        self.subject.run()

        self.mock_listdir.assert_called()
        self.mock_uninstall_package.run.assert_called_once()

    def test__input_matches_multiple_packages(self):
        sys.argv = ["gppkg", "--remove", "sampl"]
        get_result_mock = Mock()
        get_result_mock.stdout.strip.return_value = "RPM version 4.8.0"

        self.mock_cmd.get_results.return_value = get_result_mock
        self.mock_listdir.return_value = ['sample.gppkg', 'sample2.gppkg', 'another.gppkg']

        self.mock_gppkg.from_package_path.return_value = []
        self.mock_uninstall_package.run.return_value = None

        parser = GpPkgProgram.create_parser()
        options, args = parser.parse_args()
        self.subject = GpPkgProgram(options, args)
        with self.assertRaisesRegex(Exception, "Remove request 'sampl' too broad. "
                                                "Multiple packages match remove request: \( sample.gppkg, sample2.gppkg \)."):
            self.subject.run()

        self.assertFalse(self.mock_uninstall_package.run.called)

    def test__input_exact_match_when_wildcard_would_have_more(self):
        sys.argv = ["gppkg", "--remove", "sample"]
        get_result_mock = Mock()
        get_result_mock.stdout.strip.return_value = "RPM version 4.8.0"

        self.mock_cmd.get_results.return_value = get_result_mock
        self.mock_listdir.return_value = ['sample.gppkg', 'sample2.gppkg', 'another.gppkg']

        self.mock_gppkg.from_package_path.return_value = []
        self.mock_uninstall_package.run.return_value = None

        parser = GpPkgProgram.create_parser()
        options, args = parser.parse_args()
        self.subject = GpPkgProgram(options, args)

        self.subject.run()

        self.mock_listdir.assert_called()
        self.mock_uninstall_package.run.assert_called_once()
