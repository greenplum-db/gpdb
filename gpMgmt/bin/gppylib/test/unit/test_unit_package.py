from mock import *
from gp_unittest import *
from gppylib.operations.package import IsVersionCompatible


class IsVersionCompatibleTestCase(GpTestCase):
    def setUp(self):
        self.gppkg_mock_values = \
            {'main_rpm': 'plperl-1.1-2.x86_64.rpm',
             'postupdate': [],
             'pkgname': 'plperl',
             'description': 'some description.',
             'postinstall': [{'Master': "some reason to restart database"}],
             'postuninstall': [],
             'abspath': 'plperl-ossv5.12.4_pv1.3_gpdb4.3-rhel5-x86_64.gppkg',
             'preinstall': [],
             'version': 'ossv5.12.4_pv1.2_gpdb4.3',
             'pkg': 'plperl-ossv5.12.4_pv1.3_gpdb4.3-rhel5-x86_64.gppkg',
             'dependencies': [],
             'file_list': ['deps',
                           'gppkg_spec.yml',
                           'plperl-1.1-2.x86_64.rpm'],
             'gpdbversion': Mock(),
             'preuninstall': [],
             'os': 'rhel5',
             'architecture': 'x86_64'}

        self.apply_patches([
            patch('gppylib.operations.package.logger',
                  return_value=Mock(spec=['log', 'info', 'debug', 'error'])),
        ])
        self.mock_logger = self.mock_objs[0]

    def _is_requires_orca_logged(self, gppkg_name, log_messages):
        return ('Greenplum Database requires orca version of '
                '%s' % gppkg_name in log_messages)

    @patch('gppylib.operations.package.GpVersion',
           return_value=Mock(version=[4, 3, 10, 0]))
    def test__execute_reports_incompatability(self, mock_gpversion):
        logger = self.mock_logger
        gppkg_mock_values = self.gppkg_mock_values
        gppkg = Mock(**gppkg_mock_values)

        subject = IsVersionCompatible(gppkg)
        subject.execute()

        gppkg_name = 'plperl-ossv5.12.4_pv1.3_gpdb4.3-rhel5-x86_64.gppkg'
        # call object is a tuple of method name and arg list tuple
        log_messages = [args[1][0] for args in logger.method_calls]
        self.assertTrue(self._is_requires_orca_logged(gppkg_name,
                                                      log_messages))

    @patch('gppylib.operations.package.GpVersion',
           return_value=Mock(version=[4, 3, 3, 0]))
    def test__execute_reports_compatability_with_older_version(self,
                                                               mock_gpversion):
        logger = self.mock_logger
        gppkg_mock_values = self.gppkg_mock_values
        gppkg = Mock(**gppkg_mock_values)

        subject = IsVersionCompatible(gppkg)
        subject.execute()

        gppkg_name = 'plperl-ossv5.12.4_pv1.3_gpdb4.3-rhel5-x86_64.gppkg'
        # call object is a tuple of method name and arg list tuple
        log_messages = [args[1][0] for args in logger.method_calls]
        self.assertFalse(self._is_requires_orca_logged(gppkg_name,
                                                       log_messages))

    def test__execute_compatible(self):
        logger = self.mock_logger
        gppkg_name = 'plperl-ossv5.12.4_pv1.3_gpdb4.3orca-rhel5-x86_64.gppkg'
        modified_gppkg_mock_values = \
            {'abspath': gppkg_name,
             'version': 'ossv5.12.4_pv1.2_gpdb4.3orca',
             'pkg': gppkg_name}

        gppkg_mock_values = self.gppkg_mock_values
        gppkg_mock_values.update(**modified_gppkg_mock_values)
        gppkg = Mock(**gppkg_mock_values)

        subject = IsVersionCompatible(gppkg)
        subject.execute()

        log_messages = [args[1][0] for args in logger.method_calls]
        self.assertFalse(self._is_requires_orca_logged(gppkg_name,
                                                       log_messages))

if __name__ == '__main__':
    run_tests()
