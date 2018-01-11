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
        self.util = Filerepe2e_Util()

    def inject_fault(self, fault_ = None, mode_ = None, operation_ = None, prim_mirr_ = None, seg_id_ = None, host_ = 'All',
                     table_ = None, database_ = None):
        if (fault_ is None or mode_ is None or operation_ is None or prim_mirr_ is None):
            raise Exception('Incorrect parameters provided for inject fault')

        return self.util.inject_fault(f=fault_, m=mode_ , y=operation_, r =prim_mirr_, seg_id= seg_id_, H='ALL', table=table_)

    def check_fault_status(self, fault_name_ = None, status_ = None, seg_id_ = 1, max_cycle_=10):
        if (fault_name_ is None or status_ is None):
            raise Exception('Incorrect parameters provided for inject fault')

        return self.util.check_fault_status(fault_name=fault_name_, status=status_, seg_id=seg_id_, max_cycle=max_cycle_)

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
