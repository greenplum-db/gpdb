import os
from mock import *
from gp_unittest import *
from StringIO import StringIO
from subprocess import Popen, PIPE

class GpInitSystemTest(GpTestCase):
    def setUp(self):
        # resolve the gpinitsystem path
        self.gpinitsystem_path = os.path.realpath(os.path.join(os.path.dirname(os.path.realpath(__file__)), '../../../gpinitsystem'))

    def test_option_cluster_configfile_and_input_configfile_should_error(self):
        p = Popen([self.gpinitsystem_path, '-c', 'cluster_configfile', '-I', 'input_configfile'], stdout=PIPE)
        output = p.stdout.read()
        self.assertIn("[FATAL]:-Option -c and -I cannot be both specified.", output)

    def test_option_without_neither_cluster_configfile_and_input_configfile_should_error(self):
        p = Popen([self.gpinitsystem_path], stdout=PIPE)
        output = p.stdout.read()
        self.assertIn("[FATAL]:-Please specify either -c or -I as an option.", output)

    def test_option_with_input_configfile_should_work(self):
        p = Popen([self.gpinitsystem_path, '-I', 'input_configfile'], stdout=PIPE)
        output = p.stdout.read()
        self.assertIn("[INFO]:-Checking configuration parameters, please wait...", output)

    def test_option_with_cluster_configfile_should_work(self):
        p = Popen([self.gpinitsystem_path, '-c', 'cluster_configfile'], stdout=PIPE)
        output = p.stdout.read()
        self.assertIn("[INFO]:-Checking configuration parameters, please wait...", output)

if __name__ == '__main__':
    run_tests()
