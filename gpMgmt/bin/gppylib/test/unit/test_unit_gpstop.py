import imp
import os
import sys

from gparray import GpDB, GpArray, Segment
from mock import Mock, patch
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests


class GpStop(GpTestCase):
    def setUp(self):
        # because gpstop does not have a .py extension,
        # we have to use imp to import it
        # if we had a gpstop.py, this is equivalent to:
        #   import gpstop
        #   self.subject = gpstop
        gpstop_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpstop")
        self.subject = imp.load_source('gpstop', gpstop_file)
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.mock_gp = Mock()
        self.mock_pgconf = Mock()
        self.mock_os = Mock()

        self.mock_conn = Mock()
        self.mock_catalog = Mock()
        self.mock_gperafile = Mock()
        self.mock_unix = Mock()
        self.gparray = self.createGpArrayWith2Primary2Mirrors()

        self.apply_patches([
            patch('gpstop.gp', return_value=self.mock_gp),
            patch('gpstop.pgconf', return_value=self.mock_pgconf),
            patch('gpstop.os', return_value=self.mock_os),
            patch('gpstop.dbconn.connect', return_value=self.mock_conn),
            patch('gpstop.catalog', return_value=self.mock_catalog),
            patch('gpstop.unix', return_value=self.mock_unix),
            patch('gpstop.GpEraFile', return_value=self.mock_gperafile),
            patch('gpstop.GpArray.initFromCatalog', return_value=self.gparray),
        ])
        sys.argv = ["gpstop"]  # reset to relatively empty args list

    def tearDown(self):
        super(GpStop, self).tearDown()

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

    @patch('gpstop.userinput', return_value=Mock(spec=['ask_yesno']))
    def test_option_master_success_without_auto_accept(self, mock_userinput):
        sys.argv = ["gpstop", "-m"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        mock_userinput.ask_yesno.return_value = True
        gpstop = self.subject.GpStop.createProgram(options, args)
        gpstop.run()
        self.assertEqual(mock_userinput.ask_yesno.call_count, 1)
        mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with master-only shutdown', 'N')

    @patch('gpstop.userinput', return_value=Mock(spec=['ask_yesno']))
    def test_option_master_success_with_auto_accept(self, mock_userinput):
        sys.argv = ["gpstop", "-m", "-a"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        mock_userinput.ask_yesno.return_value = True
        gpstop = self.subject.GpStop.createProgram(options, args)
        gpstop.run()
        self.assertEqual(mock_userinput.ask_yesno.call_count, 0)

if __name__ == '__main__':
    run_tests()
