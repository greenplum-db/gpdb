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

from mpp.lib.ClusterControl import SegmentControl


class ClusterControlTests(unittest.TestCase):
    cluster_control = SegmentControl("localhost",os.environ.get("LOGNAME"))

    def test_getSegmentInfo(self):
        seg_info = self.cluster_control.getSegmentInfo("p",0,False)
        expected = None
        self.assertEquals(seg_info,expected)

    def test_getMasterInfo(self):
        master_info = self.cluster_control.getMasterInfo("p")
        expected = None
        self.assertEquals(master_info,expected)

    def test_getFirstSegmentInfo(self):
        first_seg_info = self.cluster_control.getFirstSegmentInfo("p")
        expected = None
        self.assertEquals(first_seg_info, expected)

    def test_killProcessUnix(self):
        segment = ('','')
        processes = ["all"]

        expected = 'ps -ef failed with rc:  (123)'
        with self.assertRaises(Exception) as context:
            self.cluster_control.killProcessUnix(segment, processes, "9")
        self.assertEqual(context.exception.message,expected)


    def test_killFirstMirror(self):
        rc = self.cluster_control.killFirstMirror()
        expected = 0
        self.assertEquals(rc, expected)

    def test_killFirstPrimary(self):
        rc = self.cluster_control.killFirstPrimary()
        expected = 0
        self.assertEquals(rc, expected)

    def test_killMirror(self):
        rc = self.cluster_control.killMirror(0)
        expected = 0
        self.assertEquals(rc, expected)

    def test_killPrimary(self):
        rc = self.cluster_control.killPrimary(0)
        expected = 0
        self.assertEquals(rc,expected)
if __name__ =="__main__":

    unittest.main()
