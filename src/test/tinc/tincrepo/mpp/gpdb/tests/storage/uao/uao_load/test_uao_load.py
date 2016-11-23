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

from gppylib.commands.base import Command

from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from tinctest.lib import Gpdiff
import unittest2 as unittest

import os
import subprocess
import sys
from time import sleep

class UAOLoadTestCase(MPPTestCase):


    def get_sql_files(self, name):
        base_dir = os.path.dirname(sys.modules[self.__class__.__module__].__file__)
        sql_file = os.path.join(
            base_dir, "sql", name + ".sql");    
        out_file = os.path.join(base_dir, 
            "output", os.path.basename(sql_file).replace('.sql','.out'))
        ans_file = os.path.join(base_dir, 
            "expected", os.path.basename(sql_file).replace('.sql','.ans'))
        return (sql_file, out_file, ans_file)

    def test_uao_gpload_update(self):
        def create_yaml_file():
            database = os.environ.get("PGDATABASE", os.environ["USER"])
            user = os.environ.get("PGUSER",  os.environ["USER"])
            port = os.environ.get("PGPORT", "5432")
            load_port = os.environ.get("TINC_UAO_LOAD_PORT", "8082")
            load_dir = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 
                    "data")
            yaml_filename = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 
                "output/gpload_update.yaml")
            yaml_file = open(yaml_filename, "w")
            yaml = """VERSION: 1.0.0.1
DATABASE: %s
USER: %s
HOST: localhost
PORT: %s
GPLOAD:
   INPUT:
    - SOURCE:
         LOCAL_HOSTNAME:
           - localhost
         PORT: %s
         FILE:
           - %s/*.txt
    - COLUMNS:
           - id: int
           - name: text
           - sponsor: text
    - FORMAT: text
    - DELIMITER: ';'
    - ESCAPE: 'OFF'
    - ERROR_LIMIT: 25
    - LOG_ERRORS: True 
   OUTPUT:
    - TABLE: customer
    - MODE: MERGE
    - MATCH_COLUMNS:  
        - id  
    - UPDATE_COLUMNS:  
        - name   
        - sponsor
    - MAPPING:  
        id: id   
        name: name 
        sponsor: sponsor
   SQL:
""" % (database, user, port, load_port, load_dir)    
            yaml_file.write(yaml)
            return yaml_filename

        (setup_file, setup_out_file) = self.get_sql_files("uao_gpload_update_setup")[0:2]
        (sql_file, out_file, ans_file) = self.get_sql_files("uao_gpload_update")

        yaml_filename = create_yaml_file()
        PSQL.run_sql_file(setup_file, out_file=setup_out_file)

        gphome = os.environ["GPHOME"]
        output_file = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 
                "output/gpload.out")
        load_process = subprocess.Popen(["%s/bin/gpload" % gphome, "-f", yaml_filename], 
                stderr=subprocess.STDOUT, stdout=open(output_file, "w"))
        load_process.wait()

        PSQL.run_sql_file(sql_file, out_file=out_file)
        result = Gpdiff.are_files_equal(out_file, ans_file)
        self.assertTrue(result) 

    def test_uao_gpload(self):
        def create_yaml_file():
            database = os.environ.get("PGDATABASE", os.environ["USER"])
            user = os.environ.get("PGUSER",  os.environ["USER"])
            port = os.environ.get("PGPORT", "5432")
            load_port = os.environ.get("TINC_UAO_LOAD_PORT", "8082")
            load_dir = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 
                    "data")
            yaml_filename = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 
                "output/gpload.yaml")
            yaml_file = open(yaml_filename, "w")
            yaml = """VERSION: 1.0.0.1
DATABASE: %s
USER: %s
HOST: localhost
PORT: %s
GPLOAD:
   INPUT:
    - SOURCE:
         LOCAL_HOSTNAME:
           - localhost
         PORT: %s
         FILE:
           - %s/*.txt
    - COLUMNS:
           - id: int
           - name: text
           - sponsor: text
    - FORMAT: text
    - DELIMITER: ';'
    - ESCAPE: 'OFF'
    - ERROR_LIMIT: 25
    - LOG_ERRORS: True 
   OUTPUT:
    - TABLE: customer
    - MODE: INSERT
   SQL:
""" % (database, user, port, load_port, load_dir)    
            yaml_file.write(yaml)
            return yaml_filename

        (setup_file, setup_out_file) = self.get_sql_files("uao_gpload_setup")[0:2]
        (sql_file, out_file, ans_file) = self.get_sql_files("uao_gpload")

        yaml_filename = create_yaml_file()
        PSQL.run_sql_file(setup_file, out_file=setup_out_file)

        gphome = os.environ["GPHOME"]
        output_file = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 
                "output/gpload.out")
        load_process = subprocess.Popen(["%s/bin/gpload" % gphome, "-f", yaml_filename], 
                stderr=subprocess.STDOUT, stdout=open(output_file, "w"))
        load_process.wait()

        PSQL.run_sql_file(sql_file, out_file=out_file)
        result = Gpdiff.are_files_equal(out_file, ans_file)
        self.assertTrue(result)        

    def test_uao_gpfdist(self):
        load_process = None
        try:
            (setup_file, setup_out_file) = self.get_sql_files("uao_gpfdist_setup")[0:2]
            (sql_file1, out_file1, ans_file1) = self.get_sql_files("uao_gpfdist1")
            (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_gpfdist2")
            if not os.path.exists(os.path.dirname(out_file1)):
                os.mkdir(os.path.dirname(out_file1))

            PSQL.run_sql_file(setup_file, out_file=setup_out_file)

            # Start gpfdist
            port = os.environ.get("TINC_UAO_LOAD_PORT", "8082")
            load_dir = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 
                    "data")
            gphome = os.environ["GPHOME"]
            output_file = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 
                "output/gpfdisk.out")
            load_process = subprocess.Popen(["%s/bin/gpfdist" % gphome, "-p", port, "-d", load_dir], 
                stderr=subprocess.STDOUT, stdout=open(output_file, "w"))

            PSQL.run_sql_file(sql_file1, out_file=out_file1)
            result1 = Gpdiff.are_files_equal(out_file1, ans_file1)

            load_process.kill()
            load_process = None

            PSQL.run_sql_file(sql_file2, out_file=out_file2)
            result2 = Gpdiff.are_files_equal(out_file2, ans_file2)

            self.assertTrue(result1)        
            self.assertTrue(result2)

        finally:
            if load_process:
                load_process.kill()
