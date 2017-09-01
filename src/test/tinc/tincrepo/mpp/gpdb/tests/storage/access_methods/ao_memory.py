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

from gppylib.commands.base import Command
from tinctest import logger
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase

import os
import re
import socket
import time
import shutil
import sys
import signal

class aoreadmemory(MPPTestCase):

    def tearDown(self):
        gpfaultinjector = Command('fault injector',
                                  'source $GPHOME/greenplum_path.sh; '
                                  'gpfaultinjector -f malloc_failure '
                                  '-y reset -H ALL -r primary')
	gpfaultinjector.run()
 
    def test_ao_malloc_failure(self):
        """
        @product_version gpdb: [4.3.5.1 -]
        """
        PSQL.run_sql_command('DROP table if exists ao_read_malloc')
        PSQL.run_sql_command('create table ao_read_malloc (a int) with (appendonly=true, compresstype=quicklz)')
        PSQL.run_sql_command('insert into ao_read_malloc '
                             'select * from generate_series(1, 1000)')

        gpfaultinjector = Command('fault injector',
                                  'source $GPHOME/greenplum_path.sh; '
                                  'gpfaultinjector -f malloc_failure '
                                  '-y error -H ALL -r primary')
	gpfaultinjector.run()

	res ={'rc':0, 'stdout':'', 'stderr':''}
	PSQL.run_sql_command(sql_cmd='select count(*) from ao_read_malloc', results=res)
        logger.info(res)

	self.assertTrue("ERROR:  fault triggered" in res['stderr'])
	self.assertFalse("ERROR:  could not temporarily connect to one or more segments" in res['stderr'])

        logger.info('Pass')

