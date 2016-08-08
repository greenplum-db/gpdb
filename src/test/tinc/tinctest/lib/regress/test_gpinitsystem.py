import os
import socket

import unittest2 as unittest

from gppylib.commands.base import Command

from tinctest.lib import local_path
from tinctest.lib.gpinitsystem import gpinitsystem

def isGPDB():
    """
    If DFS_URL is set in environment, assume gpinitsystem for HAWQ
    @todo: configFile doesn't put DFS_URL in the environment, thus we need another way to confirm HAWQ
    """
    return not os.environ.has_key("DFS_URL")

class GpinitsystemTestCase(unittest.TestCase):
    
    def setUp(self):
        seg_dir = os.getcwd()
        test_config = local_path("test_gpinit_config_nomirror.template")
        hostfile = local_path("test_hostfile")
        gpdb_dir = os.getenv("GPHOME", "/usr/local/greenplum-db")
        self.mdd = seg_dir + "/master/gpseg-1"
        self.create_template(seg_dir, test_config, hostfile)
        self.gpdbinit = gpinitsystem(gpdb_dir, test_config.strip(".template"), seg_dir, False)
    
    def tearDown(self):
        self.gpdbinit.delete_datadir()

    def create_template(self, seg_dir, config, hostfile, ext=".template"):
        """
        @todo: Need to refactor template transformation, use python string.Template
        This is copied from lib.models.gpdb.upgrade
        
        Reference:
        http://docs.python.org/2/tutorial/stdlib2.html
        http://docs.python.org/2/library/string.html
        """
        hostname = socket.gethostname()
        replace_key = {"%HOSTNAME%": hostname, "%DATA_DIR%":seg_dir, "%HOSTFILE%":hostfile}
    
        with open(config, 'r') as input:
            with open(config.strip(ext), 'w') as output:
                for line in input.readlines():
                    for key, value in replace_key.iteritems():
                        if key in line:
                            line = line.replace(key, value)
                            write = True
                            break
                        elif "%" not in line:
                            write = True
                            break
                        else:
                            write = False
                    if write == True:
                        output.write(line)  
    
    @unittest.skipUnless(isGPDB(), "This test is only for GPDB")
    def test_gpinitsystem(self):
        try:
            self.gpdbinit.run()
            cmd = Command(name='run gpstop', cmdStr='export MASTER_DATA_DIRECTORY=%s; gpstop -a' % (self.mdd))
            cmd.run()
        except:
            self.fail("Gpinitsystem Failed")        
        
    @unittest.skipUnless(isGPDB(), "This test is only for GPDB")
    def test_failed_gpinitsystem(self):
        cmd = Command(name='create folder', cmdStr='mkdir -p %s' % (self.mdd))
        cmd.run()
        try:
            self.gpdbinit.run()
            self.fail("Gpinitystem Failed")
        except:
            pass
        
    @unittest.skipIf(isGPDB(), "This test is only for HAWQ")
    def test_hawq_gpinitsystem(self):
        try:
            self.gpdbinit.run()
            cmd = Command(name='run gpstop', cmdStr='export MASTER_DATA_DIRECTORY=%s; gpstop -a' % (self.mdd))
            cmd.run()
        except:
            self.fail("Gpinitsystem Failed")                
