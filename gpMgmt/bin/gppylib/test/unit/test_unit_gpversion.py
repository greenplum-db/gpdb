#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved. 
#
""" 
Unit testing for gpversion module
"""
import unittest

from gpversion import *

class GpVersionTestCase(unittest.TestCase):
    def setUp(self):
        pass
        
    def test_case_1(self):
        vMain = GpVersion("main")

        self.assertTrue(vMain.isVersionCurrentRelease())
        self.assertTrue(vMain.getVersionBuild() == 'dev')
        self.assertTrue(str(vMain) == 'main build dev')

    def test_case_2(self):
        vTuple = GpVersion([3,3,0,0])

        self.assertTrue(vTuple.isVersionCurrentRelease() == False )
        self.assertTrue(vTuple.getVersionBuild() == 'dev')
        self.assertTrue(vTuple.getVersionRelease() == "3.3")
        self.assertTrue(vTuple.isVersionRelease("3.3"))
        self.assertTrue(vTuple.isVersionRelease("3.2") == False)
        self.assertTrue(vTuple > "3.2.0.5")
        self.assertTrue(vTuple < "4.0")

    def test_case_3(self):
        v33 = GpVersion("3.3 build dev")

        self.assertTrue(v33.isVersionCurrentRelease() == False )
        self.assertTrue(v33.getVersionBuild() == 'dev')
        self.assertTrue(v33.getVersionRelease() == "3.3")
        self.assertTrue(v33.isVersionRelease("3.3"))
        self.assertTrue(v33.isVersionRelease("3.2") == False)
        self.assertTrue(v33 > "3.2.0.5")
        self.assertTrue(v33 < "4.0")

    def test_case_4(self):
        v34 = GpVersion("3.4.filerep")

        self.assertTrue(v34.isVersionCurrentRelease() == False )
        self.assertTrue(v34.getVersionBuild() == 'filerep')
        self.assertTrue(v34.getVersionRelease() == "3.4")
        self.assertTrue(v34.isVersionRelease("3.4"))
        self.assertTrue(v34.isVersionRelease("3.2") == False)
        self.assertTrue(v34 > "3.2.0.5")
        self.assertTrue(v34 < "4.0")

    def test_case_5(self):
        vShort = GpVersion("postgres (Greenplum Database) 3.3.5.0 build 3")

        self.assertTrue(vShort.isVersionCurrentRelease() == False )
        self.assertTrue(vShort.getVersionBuild() == '3')
        self.assertTrue(vShort.getVersionRelease() == "3.3")
        self.assertTrue(vShort.isVersionRelease("3.3"))
        self.assertTrue(vShort.isVersionRelease("3.2") == False)
        self.assertTrue(vShort > "3.2.0.5")
        self.assertTrue(vShort < "4.0")


    def test_case_6(self):
        vLong = GpVersion("PostgreSQL 8.2.14 (Greenplum Database 3.4.filerep build 0) on i386-apple-darwin9.8.0, compiled by GCC i686-apple-darwin9-gcc-4.0.1 (GCC) 4.0.1 (Apple Inc. build 5465) compiled on Feb 16 2010 11:25:31 (with assert checking)")
    
        self.assertTrue(vLong.isVersionCurrentRelease() == False )
        self.assertTrue(vLong.getVersionBuild() == 'filerep')
        self.assertTrue(vLong.getVersionRelease() == "3.4")
        self.assertTrue(vLong.isVersionRelease("3.4"))
        self.assertTrue(vLong.isVersionRelease("3.2") == False)
        self.assertTrue(vLong > "3.2.0.5")
        self.assertTrue(vLong < "4.0")

    def test_lshift(self):
        v = GpVersion('4.3')
        v_1 = v << 1
        v_2 = v << 2
        self.assertEqual(v_1 << 1, v_2)
        self.assertLess(v_1, v)
        self.assertLess(v_2, v)
        self.assertEqual(v.getVersionRelease(), "4.3")
        self.assertEqual(v_1.getVersionRelease(), "4.2")
        self.assertEqual(v_2.getVersionRelease(), "4.1")

    def test_50(self):
        v = GpVersion('5.0')
        v_1 = v << 1
        v_2 = v << 2
        self.assertEqual(v_1 << 1, v_2)
        self.assertLess(v_1, v)
        self.assertLess(v_2, v)
        self.assertEqual(v.getVersionRelease(), "5")
        self.assertEqual(v_1.getVersionRelease(), "4.3")
        self.assertEqual(v_2.getVersionRelease(), "4.2")
       
    def test_case_7(self):
        vLong = GpVersion("PostgreSQL 9.4.20 (Greenplum Database 6.0.0 build dev) on x86_64-unknown-linux-gnu, compiled by gcc (Ubuntu 7.4.0-1ubuntu1~18.04.1) 7.4.0, 64-bit compiled on Jul  8 2019 16:27:59")
        self.assertTrue(vLong.isVersionCurrentRelease() == True )
        self.assertTrue(vLong.getVersionBuild() == 'dev')
        self.assertTrue(vLong.getVersionRelease() == "6")
        self.assertTrue(vLong.isVersionRelease("6.0"))
        self.assertTrue(vLong.isVersionRelease("3.2") == False)
        self.assertTrue(vLong > "4.0.0")
        self.assertTrue(vLong > "4.0")

    def test_lshift_negative(self):
        v = GpVersion('3.2')
        with self.assertRaisesRegexp(StandardError, 'invalid version shift'):
            v << -1
        with self.assertRaisesRegexp(StandardError, 'invalid version shift'):
            v << 1


#------------------------------- Mainline --------------------------------
if __name__ == '__main__':
    unittest.main()
