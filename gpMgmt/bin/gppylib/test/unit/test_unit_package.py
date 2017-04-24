from mock import *
from gp_unittest import *
from gppylib.operations.package import IsVersionCompatible


class IsVersionCompatibleTestCase(GpTestCase):
    def setUp(self):
        self.gppkg_mock_values = \
            {
                'main_rpm': 'plperl-1.1-2.x86_64.rpm',
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
            patch('gppylib.operations.package.logger', return_value=Mock(spec=['log', 'info', 'debug', 'error'])),
        ])
        self.mock_logger = self.get_mock_from_apply_patch('logger')

    def test__execute_happy_compatible(self):
        gppkg = Mock(**self.gppkg_mock_values)

        subject = IsVersionCompatible(gppkg)
        subject.execute()

        log_messages = [args[1][0] for args in self.mock_logger.method_calls]
        self.assertTrue(len(log_messages) > 2)
        self.assertFalse(any("requires" in message for message in log_messages))

    def test__execute_no_version_fails(self):
        self.gppkg_mock_values['gpdbversion'].isVersionRelease.return_value = False
        gppkg = Mock(**self.gppkg_mock_values)

        subject = IsVersionCompatible(gppkg)
        subject.execute()

        log_messages = [args[1][0] for args in self.mock_logger.method_calls]
        self.assertTrue(len(log_messages) > 2)
        self.assertTrue(any("requires" in message for message in log_messages))


if __name__ == '__main__':
    run_tests()
