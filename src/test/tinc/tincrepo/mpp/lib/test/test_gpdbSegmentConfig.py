#! /usr/bin/env python

"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import unittest2 as unittest
import os

from mpp.lib.gpdbSegmentConfig import GpdbSegmentConfig

from tinctest.lib import run_shell_command

class GpdbSegmentConfigTests(unittest.TestCase):
    gpdbSegmentConfig = GpdbSegmentConfig()

    @classmethod
    def setUpClass(cls):
        # Hardcoding this to make these tests pass.
        # The library has a hardcoded dbname which has to be changed.
        run_shell_command("createdb gptest")
        
    @classmethod
    def tearDownClass(cls):
        run_shell_command("dropdb gptest")

    def test_GetSqlData(self):
        (rc,data) = self.gpdbSegmentConfig.GetSqlData('')
        expected = (0,[])
        self.assertEquals((rc,data),expected)
    def test_hasStandbyMaster(self):
        ret = self.gpdbSegmentConfig.hasStandbyMaster()
        expected = False
        self.assertEquals(ret,expected)
    def test_hasMirrors(self):
        ret = self.gpdbSegmentConfig.hasMirrors()
        expected = False
        self.assertEquals(ret,expected)
    def test_GetSegmentInvalidCount(self):
        (rc,count) = self.gpdbSegmentConfig.GetSegmentInvalidCount()
        expected = (0,'0')
        self.assertEquals((rc,count),expected)
    def test_GetSegmentInSync(self):
        (rc,inSync) = self.gpdbSegmentConfig.GetSegmentInSync(repeatCnt=0)
        expected = (0,False)
        self.assertEquals((rc,inSync),expected)

    def test_GetMasterHost(self):
        (rc,data) = self.gpdbSegmentConfig.GetMasterHost()
        expected = (0,None)
        self.assertEquals((rc,data),expected)
    def test_GetServerList(self):
        (rc,data) = self.gpdbSegmentConfig.GetServerList()
        expected = (0,[])
        self.assertEquals((rc,data),expected)
    def test_GetMasterStandbyHost(self):
        (rc,data) = self.gpdbSegmentConfig.GetMasterStandbyHost()
        expected = (0,None)
        self.assertEquals((rc,data),expected)
    def test_GetHostAndPort(self):
        (rc,data1,data2) = self.gpdbSegmentConfig.GetHostAndPort('')
        expected = (0,"","")
        self.assertEquals((rc,data1,data2),expected)
    def test_GetContentIdList(self):
        contentList = self.gpdbSegmentConfig.GetContentIdList()
        expected = []
        self.assertEquals(contentList,expected)
    def test_GetMasterDataDirectory(self):
        datadir = self.gpdbSegmentConfig.GetMasterDataDirectory()
        expected = ''
        self.assertEquals(datadir,expected)
    def test_GetSegmentData(self):
        segmentData = self.gpdbSegmentConfig.GetSegmentData(1)
        expected = []
        self.assertEquals(segmentData,expected)

if __name__ == "__main__":

    unittest.main()
