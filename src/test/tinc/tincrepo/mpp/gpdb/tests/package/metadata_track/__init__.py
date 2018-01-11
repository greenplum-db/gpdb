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

import shutil
import fileinput
import os, re
import socket
import tinctest
import getpass
from mpp.lib.mppUtil import getOpenPort
from mpp.lib.GPFDIST import GPFDIST
from tinctest.lib import local_path

class MDT:

    def __init__(self):
        self.host = str(socket.gethostbyname(socket.gethostname()))
        self.port = str(getOpenPort())
        self.gpfdist_dir = local_path('')
        self.gpfdist = GPFDIST(self.port, self.host, directory=self.gpfdist_dir)

    def setup_gpfdist(self):
        self.gpfdist.killGpfdist()
        self.gpfdist.startGpfdist()

    def cleanup_gpfdist(self):
        self.gpfdist.killGpfdist()
        return True

    def pre_process_sql(self, sql_path = local_path("sql")):
        for dir in os.listdir(sql_path):
            file = os.path.join(local_path('sql'), dir)
            if os.path.isfile(file):
                self.do_insert_select(file)
                self.modify_sql_file(file)

    def pre_process_ans(self, sql_path = local_path("expected")):
        for dir in os.listdir(sql_path):
            file = os.path.join(local_path('expected'), dir)
            if os.path.isfile(file):
                self.modify_ans_file(file)

    def do_insert_select(self, filename=None):
        tmp_file = filename + '.tmp'
        a=0
        #if (filename.find('alter_part_table')>=0) or (filename.find('create_table_partitions')>=0):
        if (filename.find('part')>=0):
            selectString='select classname,schemaname, objname, usestatus, usename, actionname, subtype, partitionlevel, parenttablename, parentschemaname  from pg_stat_partition_operations  where statime > ( select statime from pg_stat_partition_operations where objname =\'my_first_table\' and actionname =\'CREATE\') and objname  not in (\'pg_stat_operations\',\'pg_stat_partition_operations\') order by statime;'
        else:
            selectString='select classname  , schemaname , objname  , usestatus , usename , actionname , subtype from pg_stat_operations  where statime > ( select statime from pg_stat_operations where objname =\'my_first_table\' and actionname =\'CREATE\') and objname  not in (\'pg_stat_operations\',\'pg_stat_partition_operations\') order by statime;'
        f = open(filename,'r')
        f1 = open(tmp_file, 'w')
        for line in f:
            if (line.find('drop ')!=-1) and (a==0):
                f1.write(selectString)
                f1.write('\n')
                a = 1
            f1.write(line)
        f.close()
        f1.write(selectString)
        f1.write('\n')
        f1.close()
        shutil.move(tmp_file, filename)

    def modify_sql_file(self, file = None):    
        for line in fileinput.FileInput(file,inplace=1):
            line = re.sub('(\d+)\.(\d+)\.(\d+)\.(\d+)\:(\d+)', self.host+':'+self.port, line)
            print str(re.sub('\n','',line))

    def modify_ans_file(self, file = None):
        for line in fileinput.FileInput(file,inplace=1):
            line = re.sub('gpadmin', getpass.getuser(), line)
            print str(re.sub('\n','',line))
