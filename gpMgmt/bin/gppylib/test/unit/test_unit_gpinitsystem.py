import os
from mock import *
from gp_unittest import *
from StringIO import StringIO
try:
    from subprocess32 import Popen, PIPE
except:
    from subprocess import Popen, PIPE

class GpInitSystemTest(GpTestCase):
    def setUp(self):
        # resolve the gpinitsystem path
        self.gpinitsystem_path = os.path.realpath(os.path.join(os.path.dirname(os.path.realpath(__file__)), '../../../gpinitsystem'))

    def test_option_cluster_configfile_and_input_configfile_should_error(self):
        p = Popen([self.gpinitsystem_path, '-c', 'cluster_configfile', '-I', 'input_configfile'], stdout=PIPE)
        output = p.stdout.read()
        self.assertIn("[FATAL]:-Options [-c] and [-I] cannot be used at the same time.", output)
        self.assertNotIn("Creates a new Greenplum Database instance", output)
        self.assertNotIn("Initializes a Greenplum Database system by using configuration", output)

    def test_option_without_neither_cluster_configfile_and_input_configfile_should_error(self):
        p = Popen([self.gpinitsystem_path], stdout=PIPE)
        output = p.stdout.read()
        self.assertIn("[FATAL]:-At least one of two options, [-c] or [-I], is required", output)
        self.assertNotIn("Creates a new Greenplum Database instance", output)
        self.assertNotIn("Initializes a Greenplum Database system by using configuration", output)

    def test_option_with_input_configfile_should_work(self):
        p = Popen([self.gpinitsystem_path, '-I', 'input_configfile'], stdout=PIPE)
        output = p.stdout.read()
        self.assertIn("[INFO]:-Checking configuration parameters, please wait...", output)

    def test_option_with_cluster_configfile_should_work(self):
        p = Popen([self.gpinitsystem_path, '-c', 'cluster_configfile'], stdout=PIPE)
        output = p.stdout.read()
        self.assertIn("[INFO]:-Checking configuration parameters, please wait...", output)

    def test_option_help_prints_docs_usage(self):
        p = Popen([self.gpinitsystem_path, '--help'], stdout=PIPE)
        output = p.stdout.read()
        self.assertIn("Initializes a Greenplum Database system by using configuration", output)
        self.assertNotIn("Creates a new Greenplum Database instance", output)

    def test_invalid_option_prints_raw_usage(self):
        p = Popen([self.gpinitsystem_path, '--unknown-option'], stdout=PIPE)
        output = p.stdout.read()
        self.assertIn("[ERROR]:-Unknown option --unknown-option", output)
        self.assertIn("Creates a new Greenplum Database instance", output)
        self.assertNotIn("Initializes a Greenplum Database system by using configuration", output)

if __name__ == '__main__':
    run_tests()
