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
import shutil
import unittest2 as unittest

from gppylib.db import dbconn
from gppylib.commands.base import Command
from gppylib.commands.gp import GpStart, GpStop

import tinctest
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase

class high_transaction_id(MPPTestCase):
    def test_start_gpdb_with_high_transaction_id(self):
        """
        
        @description GPDB hang after high transaction id
        @created 2013-04-18 00:00:00
        @modified 2013-04-18 00:00:00
        @tags transaction MPP-17302 MPP-17323 MPP-17325 MPP-18462 MPP-18463 schedule_transaction 
        
        @note This requires that both primary and mirror to reset xlog.

        Repro step from Hitoshi:
        gpstop -a
        pg_resetxlog -x 100000000 /data/haradh1/gpdata/d/gpseg0
        dd if=/dev/zero of=/data/haradh1/gpdata/d/gpseg0/pg_clog/0017 oflag=append conv=notrunc bs=1048576 count=1
        cp /data/haradh1/gpdata/d/gpseg0/pg_clog/0017 /data/haradh1/gpdata/d/gpseg0/pg_distributed/02FA
        gpstart -a
        """

        # @note: need a class to get GPDB configuration, need to get primary/mirror segment location
        sqlcmd = "select fselocation from gp_segment_configuration, pg_filespace_entry where dbid=fsedbid and content=0"
        with dbconn.connect(dbconn.DbURL()) as conn:
            segments = dbconn.execSQL(conn, sqlcmd)

        # @note: Issue with self.run_gpstop, hard-coded remoteHost to mdw
        # @note: upgrade model uses a series of gpstop and gpstart command, need helper classes
        cmd = GpStop("gpstop")
        cmd.run(validateAfter=True)
        
        for segment in segments:
            cmd = Command(name="reset xlog",
                    cmdStr="echo yes | pg_resetxlog -f -x 100000000 %s" % segment[0])
            cmd.run(validateAfter=True)
    
            xlogfile = local_path('xlog_file')
            # @todo: able to copy the xlogfile remotely
            shutil.copyfile(xlogfile, "%s/pg_clog/0017" % segment[0])
            shutil.copyfile(xlogfile, "%s/pg_distributedlog/02FA" % segment[0])

        cmd = GpStart("gpstart")
        cmd.run(validateAfter=True)
