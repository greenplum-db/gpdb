import imp
import os
import sys

from gparray import GpDB, GpArray
from mock import Mock, patch
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests


class GpStart(GpTestCase):
    def setUp(self):
        # because gpstart does not have a .py extension,
        # we have to use imp to import it
        # if we had a gpstart.py, this is equivalent to:
        #   import gpstart
        #   self.subject = gpstart
        gpstart_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpstart")
        self.subject = imp.load_source('gpstart', gpstart_file)
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.gparray = self.createGpArrayWith2Primary2Mirrors()

        self.apply_patches([
            patch('gpstart.os.path.exists'),
            patch('gpstart.gp'),
            patch('gpstart.pgconf'),
            patch('gpstart.unix'),
            patch('gpstart.dbconn.DbURL'),
            patch('gpstart.dbconn.connect'),
            patch('gpstart.GpArray.initFromCatalog', return_value=self.gparray),
            patch('gpstart.catalog.getCollationSettings', return_value=(None, None, None)),
            patch('gpstart.GpDbidFile'),
            patch('gpstart.GpEraFile'),
            patch('gpstart.userinput'),
        ])

        self.mock_os_path_exists = self.get_mock_from_apply_patch('exists')
        self.mock_gp = self.get_mock_from_apply_patch('gp')
        self.mock_pgconf = self.get_mock_from_apply_patch('pgconf')
        self.mock_userinput = self.get_mock_from_apply_patch('userinput')

        self.mock_pgconf.readfile.return_value = Mock()

        self.mock_gp.get_masterdatadir.return_value = 'masterdatadir'
        self.mock_gp.GpCatVersion.local.return_value = 1
        self.mock_gp.GpCatVersionDirectory.local.return_value = 1
        sys.argv = ["gpstart"]  # reset to relatively empty args list

    def tearDown(self):
        super(GpStart, self).tearDown()

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

    def test_option_master_success_without_auto_accept(self):
        sys.argv = ["gpstart", "-m"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()

        gpstart = self.subject.GpStart.createProgram(options, args)
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with master-only startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Master instance in admin mode')
        self.subject.logger.info.assert_any_call('Master Started...')
        self.assertEqual(return_code, 0)

    def test_option_master_success_with_auto_accept(self):
        sys.argv = ["gpstart", "-m", "-a"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()

        gpstart = self.subject.GpStart.createProgram(options, args)
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 0)
        self.subject.logger.info.assert_any_call('Starting Master instance in admin mode')
        self.subject.logger.info.assert_any_call('Master Started...')
        self.assertEqual(return_code, 0)


def os_exists_check(arg):
    # Skip file related checks
    if 'pg_log' in arg:
        return True
    elif 'postmaster.pid' in arg or '.s.PGSQL' in arg:
        return False
    return False


if __name__ == '__main__':
    run_tests()
