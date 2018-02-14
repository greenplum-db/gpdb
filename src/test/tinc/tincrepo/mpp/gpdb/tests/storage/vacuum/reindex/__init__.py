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

import fileinput
import re
import unittest2 as unittest
import socket
import os, string, sys

import tinctest
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.gpdbverify import GpdbVerify

class Reindex():

    def __init__(self):
        self.pgport = os.environ.get('PGPORT')

    def do_gpcheckcat(self):
        dbstate = GpdbVerify()
        i = 0 
        errorCode = 1 
        while(errorCode>0 and i<5):
            (errorCode, hasError, gpcheckcat_output, repairScriptDir) = dbstate.gpcheckcat(alldb = False)
            tinctest.logger.info(" %s Gpcheckcat iteration . ErrorCode: %s " % (i,errorCode))
            if (errorCode>0):
                dbstate.run_repair_script(repairScriptDir)
                i = i+1 
            if errorCode!=0 and i>=5 :
                raise Exception('gpcheckcat finished with error(s)')
