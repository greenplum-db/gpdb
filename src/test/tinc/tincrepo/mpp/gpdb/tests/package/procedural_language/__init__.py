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

@summary: Provides utilties to install/uninstall the given procedural language using gppkg utility.
"""
import sys
import os
import traceback
import time
import re
import tinctest
from mpp.lib.gpdbSystem import GpdbSystem
from mpp.lib.gpSystem import GpSystem
from mpp.lib.PSQL import PSQL
from mpp.lib.mppUtil import hasExpectedStr
from tinctest.lib import local_path, run_shell_command

class ProceduralLanguage:
    """
    @summary: Base test class for procedural languages.
    
    """


    def __init__(self):
        self.__setSystemInfo()

    def __setSystemInfo(self):
        """
        @summary: Internal function. Sets attributes for this test using information from current GPDB system. Used in constructor.
        
        """ 
        system = GpSystem()
        gpdb = GpdbSystem()
        self.gppkg_os = system.GetOS().lower() + system.GetOSMajorVersion()
        self.gppkg_platform = system.GetArchitecture()
        self.gppkg_branch = gpdb.GetGpdbVersion()[0]
        self.gppkg_build = gpdb.GetGpdbVersion()[1]
    
    def language_in_db(self, language_name, dbname=os.environ.get('PGDATABASE', 'gptest')):
        """
        @summary: Checks if a given procedural language is defined in a given database.
        
        @param language_name: The name of the procedural language, e.g. plperl
        @param dbname: Optional. The name of the database.  If not specified, uses PGDATABASE if defined in environment or gptest if not.
        @return: True if language is found in pg_language, False otherwise 
        """  
        sql = "select lanname from pg_language where lanname = '%s'" % language_name
        result = PSQL.run_sql_command(sql_cmd = sql, flags = '-q -t', dbname=dbname)
        tinctest.logger.info("Running command - %s" %sql)
        if len(result.strip()) > 0:
            return True
        else:
            return False
       
    def create_language_in_db(self, language_name, dbname=os.environ.get('PGDATABASE', 'gptest')):
        """
        @summary: Creates a procedural language from a given database
        
        @param language_name: The name of the procedural language to added, e.g. plperl
        @param dbname: Optional. The name of the database. If not specified, uses PGDATABASE if defined in environment or gptest if not.
        @return: list - (True, output of sql) if language is added, (False, output of sql) otherwise
        @raise Exception: If sql returns an error  
        """
        sql = 'create language %s' % language_name
        result = PSQL.run_sql_command(sql_cmd = sql, flags = '-q -t', dbname=dbname)
        tinctest.logger.info("Running command - %s" %sql)
        if 'error' in result.lower():
            return False
        else:
            return True
