#!/usr/bin/env python

import sys, os, pwd
import unittest2 as unittest

try:
    gphome = os.environ.get('GPHOME')
    if not gphome:
        raise Exception("GPHOME not set")
    location = "%s/bin/lib" % gphome
    sys.path.append(location)
    from gppylib.util.ssh_utils import HostList, Session
except Exception as e:
    print "PYTHON PATH: %s" % ":".join(sys.path)
    print str(e)
    raise

class SshUtilsTestCase(unittest.TestCase):

    def test00_test_filterMultiHomedHosts(self):
        """ 
            filterMultiHomedHosts should deduplicate hostnames
        """

        hostlist = HostList()
        hostlist.add('localhost')
        hostlist.add('localhost')
        hostlist.add('localhost')
        hostlist.filterMultiHomedHosts()
        self.assertEqual(len(hostlist.get()), 1, 
                         "There should be only 1 host in the hostlist after calling filterMultiHomedHosts")

    def test01_test_SessionLogin(self):
        """ 
            Session.login test, one success and one failure
        """

        uname = pwd.getpwuid(os.getuid()).pw_name

        s = Session()
        s.login(['localhost', 'fakehost'], uname)
        pxssh_hosts = [pxssh_session.x_peer for pxssh_session in s.pxssh_list]
        self.assertEqual(pxssh_hosts, ['localhost'])
 
if __name__ == "__main__":
    unittest.main()
