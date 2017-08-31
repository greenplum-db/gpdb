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
"""

import os
import unittest2 as unittest
from tinctest import logger
from gppylib.commands.base import Command, REMOTE
from gppylib.commands import unix
from gppylib.db import dbconn
from gppylib.gparray import GpArray
from tinctest.models.scenario import ScenarioTestCase

DEFAULT_DATABASE = os.environ['PGDATABASE']

class GpInitStandbyTest(ScenarioTestCase):
    def __init__(self, methodName):
        super(GpInitStandbyTest, self).__init__(methodName)

    def setUp(self):
        super(GpInitStandbyTest, self).setUp()
        
    def tearDown(self):
        # Check and put cluster into up and insync before each test case
        super(GpInitStandbyTest, self).tearDown()
         
    @classmethod
    def setUpClass(cls):
        pass

    @classmethod
    def tearDownClass(cls):
        pass

    def get_standby_host(self, gparr):
        standby_host = None
        for seg in gparr.getDbList():
            if not seg.isSegmentMaster() and not (seg.getSegmentHostName() == gparr.master.getSegmentHostName()):
                standby_host = seg.getSegmentHostName()
                break
        return standby_host

    def create_standby_master(self, gparr):
        standby_host = self.get_standby_host(gparr)
        if standby_host is None:
            raise Exception('Unable to get standby host')
        cmd = Command('create a standby master', cmdStr='gpinitstandby -s %s -a' % standby_host)
        cmd.run(validateAfter=True)
        return standby_host
        
    def remove_standby_master(self, gparr):
        if not gparr.standbyMaster:
            raise Exception('Standby master not configured. Cannot remove standby')
        cmd = Command('remove standby master', cmdStr='gpinitstandby -r -a')
        cmd.run(validateAfter=True)

    def get_standby_pg_hba_info(self, standby_host):
        standby_ips = unix.InterfaceAddrs.remote('get standby ips', standby_host)
        current_user = unix.UserId.local('get userid')
        new_section = ['# standby master host ip addresses']
        for ip in standby_ips:
            cidr_suffix = '/128' if ':' in ip else '/32' 
            new_section.append('host\tall\t%s\t%s%s\ttrust' % (current_user, ip, cidr_suffix))
        return new_section

    def verify_standby_ips(self, gparr, standby_host):
        standby_pg_hba_info = self.get_standby_pg_hba_info(standby_host)
        for seg in gparr.getDbList():
            if seg.isSegmentMaster():
                continue
            cmd = Command('cat the pg_hba of all segments', cmdStr='cat %s/pg_hba.conf' % seg.getSegmentDataDirectory(),
                           ctxt=REMOTE, remoteHost=seg.getSegmentHostName())
            cmd.run(validateAfter=True)
            results = cmd.get_results()
            pg_hba_contents = results.stdout.strip()
            if '\n'.join(standby_pg_hba_info) not in pg_hba_contents:
                raise Exception('Unable to find standby ip address in pg_hba for segment %s:%s (%s)' % (seg.getSegmentHostName(), 
                                                                                                        seg.getSegmentDataDirectory(),
                                                                                                        pg_hba_contents))

    def test_init_standby(self):
        gparr = GpArray.initFromCatalog(dbconn.DbURL(dbname=DEFAULT_DATABASE))
        if not gparr.standbyMaster:
            standby_host = self.create_standby_master(gparr)
        else:
            self.remove_standby_master(gparr)
            standby_host = self.create_standby_master(gparr)
        self.verify_standby_ips(gparr, standby_host)
