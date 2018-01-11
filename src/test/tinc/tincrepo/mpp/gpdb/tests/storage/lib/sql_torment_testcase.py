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

from mpp.models import SQLTestCase
from gppylib.commands.base import Command, CommandResult, REMOTE
from gppylib.commands.gp import GpLogFilter
import inspect, math, os, time
import logging, sys
from time import localtime, strftime

import tinctest

class PTormentSQL(Command):
    """This is a wrapper for running sql torment command."""
    def __init__(self, sql_file = None, parallel=1, dbname = None, username = None, password = None,
            PGOPTIONS = None, host = None, port = None, out_file = None, output_to_file=True):

        if dbname == None:
            dbname_option = "-d %s" % os.environ.get("PGDATABASE", os.environ["USER"])
        else:
            dbname_option = "-d %s" % (dbname)
        if username == None:
            username_option = ""
        else:
            username_option = "-U %s" % (username)
        if PGOPTIONS == None:
            PGOPTIONS = ""
        else:
            PGOPTIONS = "PGOPTIONS='%s'" % PGOPTIONS
        if host == None:
            hostname_option = ""
        else:
            hostname_option = "-h %s" % (host)
        if port == None:
            port_option = "-p %s" % os.environ.get("PGPORT", 5432)
        else:
            port_option = "-p %s" % (port)

        assert os.path.exists(sql_file)
        if out_file == None:
            out_file = sql_file.replace('.sql', '.out')
        if out_file[-2:] == '.t':
            out_file = out_file[:-2]

        cmd_str = '%s gptorment.pl -connect="%s %s %s %s" -parallel=%s -sqlfile %s' \
            % (PGOPTIONS, dbname_option, username_option, hostname_option, port_option, parallel, sql_file)

        if output_to_file:
            cmd_str = "%s &> %s" % (cmd_str, out_file)
        Command.__init__(self, 'run sql test', cmd_str)


    @staticmethod
    def run_sql_file(sql_file = None, parallel = 1, dbname = None, username = None, password = None,
            PGOPTIONS = None, host = None, port = None, out_file = None, output_to_file=True):
        cmd = PTormentSQL(sql_file, parallel, dbname, username, password, PGOPTIONS, host, port, out_file = out_file, output_to_file=output_to_file)
        tinctest.logger.info("Running sql file - %s" %cmd)
        cmd.run(validateAfter = False)
        result = cmd.get_results()
        tinctest.logger.info("Output - %s" %result)
        if result.rc != 0:
            return False
        return True

@tinctest.skipLoading("Model class.Need not load.")
class SQLTormentTestCase(SQLTestCase):
    """
        The SQLTormentTestCase is an extension of Tinc's SQLTestCase.
        During setup is searched for a file named <testcasename>_torment_setup.sql.
        If found, it executes it with gptorment.pl. 

        This test case model allows to ensure that multiple transaction are used
        for setup. This is important for some append-only tests to ensure
        that multiple segment files are used.
    """
    torment_parallel = 2

    def _run_setup_sql(self):
        super(SQLTormentTestCase, self)._run_setup_sql()

        test_case_setup_torment_sql_file = self.sql_file.replace('.sql', '_torment_setup.sql')
        if os.path.exists(test_case_setup_torment_sql_file):
            tinctest.logger.info("Running setup torment sql for test - %s" % test_case_setup_torment_sql_file)
            self._run_torment_sql_file(test_case_setup_torment_sql_file)

    def _run_torment_sql_file(self, sql_file):
        result = True
        
        self.test_artifacts.append(sql_file)
        out_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql','.out'))
        self.test_artifacts.append(out_file)

        PTormentSQL.run_sql_file(sql_file, parallel=self.__class__.torment_parallel, 
                dbname = self.db_name, out_file = out_file)
        return result

    def get_ans_suffix(self):
        return

    def run_test(self):
        sql_file = self.sql_file
        ans_file = self.ans_file
        def check_valid_suffix(suffix):
            import re
            if not re.match("[a-zA-Z0-9]+", suffix):
                raise Exception("Invalid ans file suffix %s" % suffix)
        # Modify the ans file based on the suffix
        suffix = self.get_ans_suffix()
        if suffix:
            check_valid_suffix(suffix)
            new_ans_file = ans_file[:-4] + "_" + suffix + ".ans"
            if os.path.exists(new_ans_file):
                self.ans_file = new_ans_file
        return super(SQLTormentTestCase, self).run_test()
