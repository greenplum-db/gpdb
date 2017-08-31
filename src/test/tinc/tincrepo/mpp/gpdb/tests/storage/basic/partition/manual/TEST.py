#!/usr/bin/env python
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

PARTITION
"""

############################################################################
# Set up some globals, and import gptest 
#    [YOU DO NOT NEED TO CHANGE THESE]
#
import sys, unittest, os, string
MYD = os.path.abspath(os.path.dirname(__file__))
mkpath = lambda *x: os.path.join(MYD, *x)
UPD = os.path.abspath(mkpath('../..'))
if UPD not in sys.path:
    sys.path.append(UPD)
import gptest 
from gptest import psql, shell

USER = os.environ.get("LOGNAME")
LEN = len(USER)
PATH = os.getcwd()
pos = PATH.find("partition")
if pos==-1:
        PATH += "/partition"

###########################################################################
#  A Test class must inherit from gptest.GPTestCase
#    [CREATE A CLASS FOR YOUR TESTS]
#
class partition(gptest.GPTestCase):

    #def setUp(self):
#	pass
#    def tearDown(self):
#	pass

    def doTest(self, num, default=''):
	# get file path to queryXX.sql 
	file = mkpath('query%d.sql' % num)
	# run psql on file, and check result
        psql.runfile(file,default)
	self.checkResult(file)

    def doTestFile(self, filename, default=''):
	file = mkpath(filename)
        psql.runfile(file,default)
	self.checkResult(file)

    def testQuery001(self): 
	"Partition: Drop partition table with lots of child partitions 43k"
  	self.doTestFile("partition_outofmemory.sql")

    def testQuery002(self): 
	"Partition: Drop schema with lots of partitions 86k"
  	self.doTestFile("partition_outofmemory2.sql")

    def testQuery003(self): 
	"Partition: Drop schema with lots of partitions"
  	self.doTestFile("partition_outofmemory3.sql")

    def testQuery004(self): 
	"Partition: Drop lots of partitions for BE 5k"
  	self.doTestFile("partition_outofmemory_BE.sql")

###########################################################################
#  Try to run if user launched this script directly
#     [YOU SHOULD NOT CHANGE THIS]
if __name__ == '__main__':
    gptest.main()
