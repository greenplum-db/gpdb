import socket

import unittest2 as unittest

from tinctest.lib.gpfdist import gpfdist

hostname = socket.gethostname()
mygpfdist = gpfdist("8080", hostname)

class GpfdistTestCase(unittest.TestCase):
    def testStartGpfdist(self):
        mygpfdist.start("")
        self.assertTrue(mygpfdist.is_gpfdist_connected())
        mygpfdist.stop()
        self.assertFalse(mygpfdist.is_gpfdist_connected())

    def testCheckGpfdistNotStarted(self):
        self.assertFalse(mygpfdist.is_gpfdist_connected())

    def testByPassCheck(self):
        mygpfdist.start(raise_assert=False)
        self.assertTrue(mygpfdist.is_gpfdist_connected())
        mygpfdist.stop()
        self.assertFalse(mygpfdist.is_gpfdist_connected())

    def testStartGpfdistWithString(self):
        mygpfdist.start(port="8080")
        self.assertTrue(mygpfdist.is_gpfdist_connected())
        mygpfdist.stop()

    def testStartGpfdistWithInteger(self):
        mygpfdist.start(port=8080)
        self.assertTrue(mygpfdist.is_gpfdist_connected())
        mygpfdist.stop()

    def testStartGpfdistInitWithInteger(self):
        mygpfdist2 = gpfdist(8080, hostname)
        mygpfdist2.start()
        self.assertTrue(mygpfdist2.is_gpfdist_connected())
        mygpfdist2.stop()
        self.assertFalse(mygpfdist2.is_gpfdist_connected())
