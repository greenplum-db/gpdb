"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

import tinctest
import unittest2 as unittest
from tinctest import TINCTestCase
import os
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from gppylib.commands.base import Command

class sysops(Command):
	def __init__(self, cmd = None):
	        Command.__init__(self, 'Running fault command', cmd)

	def _run_sys_cmd(self, cmd_str, validate = False):
        	'''helper function to run a sys cmd'''
	        tinctest.logger.info("execute:" +cmd_str)
        	cmd = sysops(cmd_str)
	        cmd.run(validateAfter = validate)
        	return cmd.get_results()

class UpdateGPDBGUCs(TINCTestCase):
	def test_set_max_prepared_transactions(self):
	    newops = sysops()
	    cmd_str = 'gpconfig -c max_prepared_transactions -v 0'    
	    result = newops._run_sys_cmd(cmd_str, True)
	    print result.stdout
	def test_get_max_prepared_transactions(self):
	    newops = sysops()
	    cmd_str = 'gpconfig -s max_prepared_transactions'
	    result = newops._run_sys_cmd(cmd_str, True)
            tinctest.logger.info(result.stdout)
	    if 'Master  value: 50' not in result.stdout :
		raise 'Default value not set to 50!'
 
class GPDBOps(TINCTestCase):
	def test_reboot_gpdb(self):
	    newops = sysops()
	    cmd_str = 'source $GPHOME/greenplum_path.sh; gpstop -ar'
	    newops._run_sys_cmd(cmd_str, True)
	def test_gp_segment_configuration(self):
	    sql_cmd = ' SELECT count(hostname) from gp_segment_configuration; '
	    result = PSQL.run_sql_command(sql_cmd).split('\n')
	    print result[3]

