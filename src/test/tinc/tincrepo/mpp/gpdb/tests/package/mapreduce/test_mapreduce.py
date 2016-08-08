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

import os, string, sys, subprocess, signal, re, getpass
import tinctest
import unittest2 as unittest
from tinctest.lib import Gpdiff
from tinctest.lib import local_path, run_shell_command
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from mpp.lib.MAPREDUCE import mapreduce
from mpp.lib.config import GPDBConfig
from mpp.lib.gppkg.gppkg import Gppkg

gpdbconfig = GPDBConfig()

localpath = local_path('')
mapr = mapreduce

cmd = 'gpssh --version'
res = {'rc':0, 'stderr':'', 'stdout':''}
run_shell_command (cmd, 'check product version', res)
product_version = res['stdout'].split('gpssh version ')[1].split(' build ')[0]

class MapreduceMPPTestCase(MPPTestCase):

    def __init__(self, methodName):
        self.dbname = os.environ.get('PGDATABASE')
        super(MapreduceMPPTestCase, self).__init__(methodName)

    @classmethod
    def setUpClass(self):
        super(MapreduceMPPTestCase, self).setUpClass()
        gppkg = Gppkg()
        gppkg.gppkg_install(product_version, 'plperl')
        setup_command = "create language plperl;"
        PSQL.run_sql_command(setup_command, dbname = os.environ.get('PGDATABASE'))

        "compile functions.c and build functions.so"
        makeLog = local_path('testBuildSOLog.out')
        cmdMake = 'cd '+local_path('c_functions') + ' && make clean && make'
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(cmdMake, 'compile functions.c', res)
        file = open(makeLog, 'w')
        file.write(res['stdout'])
        file.close()
        if res['rc']:
            raise Exception('a problem occurred while creating the so files ')
        so_dir = local_path('c_functions')
        sharedObj = local_path('c_functions/functions.so')
        # if not os.path.isfile(sharedObj):
            #raise gptest.GPTestError('so files does not exist')

        # For multinode cluster, need to copy shared object tabfunc_gppc_demo.so to all primary segments
        if gpdbconfig.is_multinode():
            res = {'rc':0, 'stderr':'', 'stdout':''}
            hosts = gpdbconfig.get_hosts(segments=True)
            scp_cmd = 'gpscp  -h ' +' -h '.join(map(str,hosts)) +' '+ sharedObj + ' =:%s' % so_dir
            run_shell_command(scp_cmd)
            if res['rc']:
                raise Excpetion('Could not copy shared object to primary segment')

    def check_orca(self):
        cmd = 'gpconfig -s optimizer'
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(cmd, 'check if orca enabled', res)        
        for line in res['stdout']:
            if 'Master  value: off' in line or 'Segment value: off' in line:
                return false
        return True

    def doTest(self, fileName):
        # get file path to queryXX.sql 
        file = local_path(fileName)
        # run psql on file, and check result
        mapr.runYml(file)
        mapr.checkResult(file)

    def doQuery(self, sqlfile, default=''):
        sql_file = local_path(sqlfile)
        filename_prefix = sqlfile.split('.sql')[0]
        out_file = local_path(filename_prefix + '.out')
        ans_file = local_path(filename_prefix + '.ans')
        PSQL.run_sql_file(sql_file = sql_file, out_file = out_file)
        self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file))

    def run_gpmapreduce_cmd(self, gpmaprcmd = None, expected_ret=0):
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(gpmaprcmd, 'compile functions.c', res)
        self.failUnless(res['rc'] == expected_ret)

    def runFunctionTest(self, functiontype, file):
        filepath = os.path.join(local_path("c_functions"),functiontype, file)
        #mapr.replaceTemplate(local_path("%s.sql.in" % (filepath)), localpath)
        so_loc = local_path('c_functions/functions.so')
        if not os.path.isfile(so_loc):
            self.skipTest()
        
        input = open(filepath+'.sql.in')
        output = open(filepath+'.sql','w')
        for s in input.xreadlines():
            if string.find(s,'%funclib_path%') >= 0:
                if string.find(sys.platform,"OSX")==0:
                    output.write(s.replace('%funclib_path%', local_path("c_functions/functions.NOTSUREEXTNAME")))
                else:
                    output.write(s.replace('%funclib_path%', local_path("c_functions/functions.so")))
            else:
                output.write(s)
        output.close()
        input.close()
       
        sqlfile = "%s.sql" % filepath
        PSQL.run_sql_file(sqlfile)
        self.doTest("%s.yml" % filepath)
        
    def test_MapReduceDemo001(self): 
        "MapReduce: BFS Demo Init"
        mapr.replaceTemplate(local_path("demo/BFS/*.in"), local_path(''))
        self.doTest("demo/BFS/bfs-init.yml")

    def test_MapReduceDemo002(self): 
        "MapReduce: BFS Demo Iter"
        self.doTest("demo/BFS/bfs-iter.yml")

    def test_MapReduceDemo003(self): 
        "MapReduce: PageRank Demo Init"
        mapr.replaceTemplate(local_path("demo/PageRank/*.in"), local_path(''))
        self.doTest("demo/PageRank/pagerank-init.yml")

    def test_MapReduceDemo004(self): 
        "MapReduce: PageRank Demo Iter"
        self.doTest("demo/PageRank/pagerank-iter.yml")

    def test_MapReduceDemo005(self): 
        "MapReduce: PageRank Demo Final"
        self.doTest("demo/PageRank/pagerank-final.yml")

    def test_MapReduceDemo006(self): 
        "MapReduce: PageRank Using pagerank.pl"
        cmd = local_path("demo/PageRank/pagerank.pl")
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(cmd, '', res)
 
    def test_MapReduceError001(self):
        "MapReduce: Test Error Handling 1: MPP-4808"
        self.doTest("mpp4808.yml")

    def test_MapReduceError002(self):
        "MapReduce: Test Error Handling 2: MPP-4807"
        self.skipTest("Fails always on RHEL62")
        self.doTest("mpp4807a.yml")

    def test_MapReduceError003(self):
        "MapReduce: Test Error Handling 3: MPP-4807"
        f1 = open(local_path('mpp4807b.yml.in'), 'r')
        f2 = open(local_path('mpp4807b.yml'), 'w')
        for line in f1:
            if '@db_user@' in line:
                line = line.replace('@db_user@', os.environ.get('PGUSER', getpass.getuser()))
            f2.write(line)
        f1.close()
        f2.close()
        self.doTest("mpp4807b.yml")

    def test_MapReduceError004(self):
        "MapReduce: Test Error Handling 4: MPP-5550"
        f1 = open(local_path('mpp5550.yml.in'), 'r')
        f2 = open(local_path('mpp5550.yml'), 'w')
        for line in f1:
            if '@db_user@' in line:
                line = line.replace('@db_user@', os.environ.get('PGUSER', getpass.getuser()))
            f2.write(line)
        f1.close()
        f2.close()
        self.doTest("mpp5550.yml")

    def test_MapReduceError005(self):
	"MapReduce: Test Error Handling 4: MPP-4863"
	PSQL.run_sql_file(local_path('mpp4863-create.sql'))
        p = subprocess.Popen(["gpfdist","-d",local_path(''),"-p","8090"])
        f1 = open(local_path('mpp4863.yml.in'), 'r')
        f2 = open(local_path('mpp4863.yml'), 'w')
        for line in f1:
            if '@db_user@' in line:
                line = line.replace('@db_user@', os.environ.get('PGUSER', getpass.getuser()))
            elif '@hostname@' in line:
                line = line.replace('@hostname@', 'localhost')
            f2.write(line)
        f1.close()
        f2.close()
	self.doTest("mpp4863.yml")
        os.kill(p.pid,signal.SIGKILL)

    def test_MapReduceError005(self):
	"MapReduce: MPP-5551: gpmapreduce - assertion trying to set output file format"
	mapr.replaceTemplate(local_path("mpp5551.yml.in"), local_path(''))
	self.doTest("mpp5551.yml")

    def test_MapReduceCASE0001(self):
	"MapReduce: Regression MPP-3478"
	PSQL.run_sql_file(local_path('mpp3478-create.sql'))
        f1 = open(local_path('mpp3478.yml.in'), 'r')
        f2 = open(local_path('mpp3478.yml'), 'w')
        for line in f1:
            if '@db_user@' in line:
                line = line.replace('@db_user@', os.environ.get('PGUSER', getpass.getuser()))
            f2.write(line)
        f1.close()
        f2.close()
	self.doTest("mpp3478.yml")
	self.doQuery("mpp3478-check.sql")

    def test_MapReduceDemo007(self): 
	"MapReduce: Demos Init"
	# mapr.replaceTemplate(local_path("regression/*.in"), local_path(''))

    # source is not included in the build regression so we cannot build gpmrdemo.o
    def DONTtestMapReduceError006(self):
	"MapReduce: MPP-11061: mapreduce crash c.yaml"
        os.system("cd %s/mpp11061; make" % (local_path('')))
	mapr.replaceTemplate(local_path("mpp11061/c.yaml.in"), local_path(''))
        p = subprocess.Popen(["gpfdist","-d",local_path(''),"-p","8090"])
	self.doTest("mpp11061/c.yaml")
        os.kill(p.pid,signal.SIGKILL)

    # source is not included in the build regression so we cannot build gpmrdemo.o
    def DONTtestMapReduceError007(self):
	"MapReduce: MPP-11061: mapreduce crash c_working.yaml"
        os.system("cd %s/mpp11061; make" % (local_path('')))
	mapr.replaceTemplate(local_path("mpp11061/c_working.yaml.in"), local_path)('')
        p = subprocess.Popen(["gpfdist","-d",local_path(''),"-p","8090"])
	self.doTest("mpp11061/c_working.yaml")
        os.kill(p.pid,signal.SIGKILL)

    def test_Noargument(self):
        "not giving any command line arguement"
        cmd = "gpmapreduce"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    # this is no-op, no exit code was returned
    def test_InvalidOption(self):
        "use invalid option"
        cmd = "gpmapreduce -d"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_ambigous_function(self):
        "ambigous_functions"
        cmd = "gpmapreduce -f neg_Ambiguous_function.yml template1"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_fileDir(self):
        "file path is a directory"
        cmd = "gpmapreduce -f neg_fileDir.yml.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_fileDOE(self):
        "file does not exist"
        cmd = "gpmapreduce -f neg_fileDOE.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_mapAsSource(self):
        "Map fucntion used as Source"
        cmd = "gpmapreduce -f neg_mapAsSource.yml template1"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_nonScalar(self):
        "non scalar used"
        cmd = "gpmapreduce -f neg_nonScalar.yaml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_returnVoid(self):
        "return Void functions"
        cmd = "gpmapreduce -f neg_returnVoid.yml template1"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_wrongReturnValue(self):
        "wrong return value used"
        cmd = "gpmapreduce -f neg_wrongReturnValue.yml template1"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_InvalidHost(self):
        "Invalid Host"
        cmd = "gpmapreduce -f working.yml -h rh55-qavm01"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_InvalidPort(self):
        "Invalid Port"
        cmd = "gpmapreduce -f working.yml -p 0000"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_InvalidPassword(self):
        "invalid password"
        cmd = "gpmapreduce -f working.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_InvalidUser(self):
        "invalid User"
        cmd = "gpmapreduce -f working.yml -U wrongUser"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_InvalidDB(self):
        "invalid DB"
        cmd = "gpmapreduce -f working.yml template100"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_gpfdistDOE(self):
        "gpfdist not up"
        cmd = "gpmapreduce -f neg_gpfdistDOE.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_DataBaseDOE_DBspecifiedINdb(self):
        "Invalid DB, where DB specified in yml"
        cmd = "gpmapreduce -f neg_InvalidDB.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_mpp12767_emptyYML(self):
        "empty YML"
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("touch empty.yml;echo $?", '', res)
        cmd = "gpmapreduce -f empty.yml;echo $?" 
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 0)

    def test_mpp12767_notproper(self):
        "notproper.yaml from Symantec"
        cmd = "gpmapreduce -f mpp12767.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_mpp12767_badyaml(self):
        "badyaml.yaml from Symantec"
        cmd = "gpmapreduce -f mpp12767b.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_InvalidTable(self):
        "Invalid Table"
        cmd = "gpmapreduce -f mpp5551.yml;echo $?"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_InvalidTable(self):
        "Invalid Table"
        cmd = "gpmapreduce -f mpp5550.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_binaryFile(self):
        "binary file was used as yml"
        GPHOME = os.environ.get('GPHOME')
        binaryPath = '%s/bin/gpmapreduce' % GPHOME
        cmd = 'gpmapreduce -f %s' % binaryPath
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_missing_colon(self):
        "missing : in yml"
        cmd = "gpmapreduce -f neg_missing_colon.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_MissingYMLversion(self):
        "missing YML version"
        cmd = "gpmapreduce -f neg_missingYMLversion.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_MissingYMLversion(self):
        "missing YML version"
        cmd = "gpmapreduce -f neg_invalidYMLversion.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_MultipleDefine(self):
        "Multiple Define block"
        cmd = "gpmapreduce -f neg_multipleDefine.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_invalidName(self):
        "Invalid Name value"
        cmd = "gpmapreduce -f neg_invalidName.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_MissingRequiredAttribute(self):
        "Missing Required Attribuite: Name"
        cmd = "gpmapreduce -f neg_missingName.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_2inputs(self):
        "2 inputs"
        cmd = "gpmapreduce -f neg_2inputs.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_Ambigous_function_byCaleb(self):
        "Ambigous function"
        cmd = "gpmapreduce -f neg_Ambiguous_function2.yml template1"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_undefinedFunction(self):
        "Undefined Function"
        cmd = "gpmapreduce -f neg_undefinedFunction.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_undefinedReduce(self):
        "Undefined Reduce function"
        cmd = "gpmapreduce -f neg_undefinedReduce.yml template1"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Neg_undefinedTable(self):
        "undefined table target"
        cmd = "gpmapreduce -f neg_undefinedTable.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_OutputFileAlreadyExist(self):
        "Output File Already Exist"
        cmd = "gpmapreduce -f neg_outputFileAlreadyExist.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_Noperm(self):
        "no permission to write output file"
        PSQL.run_sql_command('create table mytest (a text);', dbname='template1')
        os.system("mkdir ./noperm")
        os.system("chmod 000 ./noperm")
        cmd = "gpmapreduce -f neg_nopermtowrite.yml"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)
        os.system("chmod 777 ./noperm")
        os.system("rm -rf ./noperm")

    def test_Invalidparameter(self):
        "invalid parameter for option -k"
        cmd = "gpmapreduce -k"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_EmptyArguement(self):
        "empty argument"
        cmd = "gpmapreduce -f"
        self.run_gpmapreduce_cmd(gpmaprcmd=cmd, expected_ret = 1)

    def test_builtinfunction(self):
        """
        builtin functions as reducer 
        """

        self.runFunctionTest("","builtinfunction")

    def test_aggFunction(self):
        """
        use custom aggregation functions as reducer 
        """

        self.runFunctionTest("","aggFunction")

    #### test_scalar_transistion ######

    def test_scalar_transition_namedInYml_namedInDB(self):
        """
        scalar  Transition  "param specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("scalar_transition", "namedInYml_namedInDB")

    def test_scalar_transition_namedInYml_unnamedInDB(self):
        """
        scalar  Transition  "param specified in yaml, DB defined with unnamed param" 
        """
        self.runFunctionTest("scalar_transition", "namedInYml_unnamedInDB")

    def test_scalar_transition_unnamedInYml_namedInDB(self):
        """
        scalar  Transition  "param not specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("scalar_transition","unnamedInYml_namedInDB")

    def test_scalar_transition_unnamedInYml_unnamedInDB(self):
        """
        scalar  Transition  "parameters not specified in yaml, DB defined with named param"
        """
        self.runFunctionTest("scalar_transition","unnamedInYml_unnamedInDB")

    def test_scalar_transition_retValInYml(self):
        """
        scalar  Transition return val specified in yaml, DB defined with named return val
        """
        self.runFunctionTest("scalar_transition","retValInYml")

    def test_scalar_transition_retValNotInYml(self):
        """
        scalar  Transition return value not specified in yaml
        """
        self.runFunctionTest("scalar_transition","retValNotInYml")

    def test_scalar_transition_1outParam_namedInYml_namedInDB(self):
        """
        scalar  Transition use 1 named out parameter, specified in yaml 
        """
        self.runFunctionTest("scalar_transition","1outParam_namedInYml_namedInDB")

    def test_scalar_transition_1outParam_unnamedInYml_namedInDB(self):
        """
        scalar  Transition use 1 unnamed out parameter, specified in yaml
        """
        self.runFunctionTest("scalar_transition","1outParam_unnamedInYml_namedInDB")

    def test_scalar_transition_1outParam_unnamedInYml_unnamedInDB(self):
        """
        scalar  Transition use 1 unnamed out parameter, not specified in yaml
        """
        self.runFunctionTest("scalar_transition","1outParam_unnamedInYml_unnamedInDB")

    def test_scalar_transition_1outParam_namedInYml_unnamedInDB(self):
        """
        scalar  Transition use 1 named out parameter, not specified in yaml 
        """
        self.runFunctionTest("scalar_transition","1outParam_namedInYml_unnamedInDB")

    def test_scalar_transition_1outParam_paramOverride(self):
        """
        scalar  Transition use 1 named out parameter, specified with a different value in yaml 
        """
        self.runFunctionTest("scalar_transition","1outParam_paramOverride")

    def test_scalar_transition_mismatchingReturnVal(self):
        """
        scalar  Transition override return value with what's defined in db vs library 
        """
        self.runFunctionTest("scalar_transition","mismatchingReturnVal")

    def test_scalar_transition_ambiguousFunction(self):
        """
        scalar  Transition NEG specify more than 1 function in db with the same name 
        """
        self.runFunctionTest("scalar_transition","ambiguousFunction")

    def test_scalar_transition_NEG_2outParam(self):
        """
        scalar  Transition NEG use 2 out parameters
        """
        self.runFunctionTest("scalar_transition","NEG_2outParam")

    def test_scalar_transition_NEG_1param(self):
        """
        scalar  Transition NEG use 1 parameters
        """
        self.runFunctionTest("scalar_transition","NEG_1param")

    ##### test_scalar_consolidation ######

    def test_scalar_consolidation_namedInYml_namedInDB(self):
        """
        scalar  Consolidation  "param specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("scalar_consolidation", "namedInYml_namedInDB")

    def test_scalar_consolidation_namedInYml_unnamedInDB(self):
        """
        scalar  Consolidation  "param specified in yaml, DB defined with unnamed param" 
        """
        self.runFunctionTest("scalar_consolidation", "namedInYml_unnamedInDB")

    def test_scalar_consolidation_unnamedInYml_namedInDB(self):
        """
        scalar  Consolidation  "param specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("scalar_consolidation","unnamedInYml_namedInDB")

    def test_scalar_consolidation_unnamedInYml_unnamedInDB(self):
        """
        scalar  Consolidation  "parameters not specified in yaml, DB defined with unnamed parameters "
        """
        self.runFunctionTest("scalar_consolidation","unnamedInYml_unnamedInDB")

    def test_scalar_consolidation_retValInYml(self):
        """
        scalar  Consolidation return val specified in yaml, DB defined with named return val
        """
        self.runFunctionTest("scalar_consolidation","retValInYml")

    def test_scalar_consolidation_retValNotInYml(self):
        """
        scalar  Consolidation return value not specified in yaml
        """
        self.runFunctionTest("scalar_consolidation","retValNotInYml")

    def test_scalar_consolidation_1outParam_namedInYml_namedInDB(self):
        """
        scalar  Consolidation use 1 named out parameter, specified in yaml 
        """
        self.runFunctionTest("scalar_consolidation","1outParam_namedInYml_namedInDB")

    def test_scalar_consolidation_1outParam_unnamedInYml_namedInDB(self):
        """
        scalar  Consolidation use 1 unnamed out parameter, specified in yaml
        """
        self.runFunctionTest("scalar_consolidation","1outParam_unnamedInYml_namedInDB")

    def test_scalar_consolidation_1outParam_unnamedInYml_unnamed(self):
        """
        scalar  Consolidation use 1 unnamed out parameter, not specified in yaml
        """
        self.runFunctionTest("scalar_consolidation","1outParam_unnamedInYml_unnamed")

    def test_scalar_consolidation_1outParam_unnamedInYml_namedInDB(self):
        """
        scalar  Consolidation use 1 named out parameter, not specified in yaml 
        """
        self.runFunctionTest("scalar_consolidation","1outParam_unnamedInYml_namedInDB")

    def test_scalar_consolidation_1outParam_paramOverride(self):
        """
        scalar  Consolidation use 1 named out parameter, specified with a different value in yaml 
        """
        self.runFunctionTest("scalar_consolidation","1outParam_paramOverride")

    def test_scalar_consolidation_mismatchingReturnVal(self):
        """
        scalar  Consolidation override return value with what's defined in db vs library 
        """
        self.runFunctionTest("scalar_consolidation","mismatchingReturnVal")

    def test_scalar_consolidation_ambiguousFunction(self):
        """
        scalar  Consolidation NEG specify more than 1 function in db with the same name 
        """
        self.runFunctionTest("scalar_consolidation","ambiguousFunction")

    def test_scalar_consolidation_NEG_1param(self):
        """
        scalar  Consolidation NEG specify only 1 parameter 
        """
        self.runFunctionTest("scalar_consolidation","NEG_1param")

    def test_scalar_consolidation_NEG_paramDiffType(self):
        """
        scalar  Consolidation NEG two parameters with different type
        """
        self.runFunctionTest("scalar_consolidation","NEG_paramDiffType")
        filename = local_path("c_functions/scalar_consolidation/NEG_paramDiffType_cleanup.sql")
        PSQL.run_sql_file(filename)
        


    ###### test_scalar_finalize #######

    def test_scalar_finalize_namedInYml_namedInDB(self):
        """
        scalar  Finalize  "param specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("scalar_finalize", "namedInYml_namedInDB")

    def test_scalar_finalize_namedInYml_unnamedInDB(self):
        """
        scalar  Finalize  "param specified in yaml, DB defined with unnamed param" 
        """
        self.runFunctionTest("scalar_finalize", "namedInYml_unnamedInDB")

    def test_scalar_finalize_unnamedInYml_namedInDB(self):
        """
        scalar  Finalize  "param specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("scalar_finalize","unnamedInYml_namedInDB")

    def test_scalar_finalize_unnamedInYml_unnamedInDB(self):
        """
        scalar  Finalize  "parameters not specified in yaml, DB defined with unnamed parameters "
        """
        self.runFunctionTest("scalar_finalize","unnamedInYml_unnamedInDB")

    def test_scalar_finalize_retValInYml(self):
        """
        scalar  Finalize return val specified in yaml, DB defined with named return val
        """
        self.runFunctionTest("scalar_finalize","retValInYml")

    def test_scalar_finalize_retValNotInYml(self):
        """
        scalar  Finalize return value not specified in yaml
        """
        self.runFunctionTest("scalar_finalize","retValNotInYml")

    def test_scalar_finalize_1outParam_namedInYml_namedInDB(self):
        """
        scalar  Finalize use 1 named out parameter, specified in yaml 
        """
        self.runFunctionTest("scalar_finalize","1outParam_namedInYml_namedInDB")

    def test_scalar_finalize_1outParam_namedInYml_unnamedInDB(self):
        """
        scalar  Finalize use 1 unnamed out parameter, specified in yaml
        """
        self.runFunctionTest("scalar_finalize","1outParam_namedInYml_unnamedInDB")

    def test_scalar_finalize_1outParam_unnamedInYml_unnamed(self):
        """
        scalar  Finalize use 1 unnamed out parameter, not specified in yaml
        """
        self.runFunctionTest("scalar_finalize","1outParam_unnamedInYml_unnamed")

    def test_scalar_finalize_1outParam_unnamedInYml_namedInDB(self):
        """
        scalar  Finalize use 1 named out parameter, not specified in yaml 
        """
        self.runFunctionTest("scalar_finalize","1outParam_unnamedInYml_namedInDB")

    def test_scalar_finalize_1outParam_paramOverride(self):
        """
        scalar  Finalize use 1 named out parameter, specified with a different value in yaml 
        """
        self.runFunctionTest("scalar_finalize","1outParam_paramOverride")

    def test_scalar_finalize_mismatchingReturnVal(self):
        """
        scalar  Finalize override return value with what's defined in db vs library 
        """
        self.runFunctionTest("scalar_finalize","mismatchingReturnVal")

    def test_scalar_finalize_ambiguousFunction(self):
        """
        scalar  Finalize NEG specify more than 1 function in db with the same name 
        """
        self.runFunctionTest("scalar_finalize","ambiguousFunction")

    def test_scalar_finalize_NEG_2param(self):
        """
        scalar  Finalize NEG specify more than 1 parameter
        """
        self.runFunctionTest("scalar_finalize","NEG_2param")

    ###### test_scalar_map #######


    def test_scalar_map_namedInYml_namedInDB(self):
        """
        scalar  Map  "param specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("scalar_map", "namedInYml_namedInDB")

    def test_scalar_map_namedInYml_unnamedInDB(self):
        """
        scalar  Map  "param specified in yaml, DB defined with unnamed param" 
        """
        self.runFunctionTest("scalar_map", "namedInYml_unnamedInDB")

    def test_scalar_map_unnamedInYml_namedInDB(self):
        """
        scalar  Map  "param not specified in yaml, DB defined with named param" 
        """
        if self.check_orca():
            self.skipTest("Skipping due to MPP-23877")
        self.runFunctionTest("scalar_map","unnamedInYml_namedInDB")

    def test_scalar_map_unnamedInYml_unnamedInDB(self):
        """
        scalar  Map  "parameters not specified in yaml, DB defined with unnamed parameters "
        """
        if self.check_orca():
            self.skipTest("Skipping due to MPP-23877")
        self.runFunctionTest("scalar_map","unnamedInYml_unnamedInDB")

    def test_scalar_map_retValInYml(self):
        """
        scalar  Map return val specified in yaml, DB defined with named return val
        """
        self.runFunctionTest("scalar_map","retValInYml")

    def test_scalar_map_retValNotInYml(self):
        """
        scalar  Map return value not specified in yaml
        """
        self.runFunctionTest("scalar_map","retValNotInYml")

    def test_scalar_map_1outParam_namedInYml_namedInDB(self):
        """
        scalar  Map use 1 named out parameter, specified in yaml 
        """
        self.runFunctionTest("scalar_map","1outParam_namedInYml_namedInDB")

    def test_scalar_map_1outParam_namedInYml_unnamedInDB(self):
        """
        scalar  Map use 1 unnamed out parameter, specified in yaml
        """
        self.runFunctionTest("scalar_map","1outParam_namedInYml_unnamedInDB")

    def test_scalar_map_1outParam_unnamedInYml_unnamed(self):
        """
        scalar  Map use 1 unnamed out parameter, not specified in yaml
        """
        self.runFunctionTest("scalar_map","1outParam_unnamedInYml_unnamed")

    def test_scalar_map_1outParam_unnamedInYml_namedInDB(self):
        """
        scalar  Map use 1 named out parameter, not specified in yaml 
        """
        self.runFunctionTest("scalar_map","1outParam_unnamedInYml_namedInDB")

    def test_scalar_map_1outParam_paramOverride(self):
        """
        scalar  Map use 1 named out parameter, specified with a different value in yaml 
        """
        self.runFunctionTest("scalar_map","1outParam_paramOverride")

    def test_scalar_map_mismatchingReturnVal(self):
        """
        scalar  Map override return value with what's defined in db vs library 
        """
        self.runFunctionTest("scalar_map","mismatchingReturnVal")

    def test_scalar_map_ambiguousFunction(self):
        """
        scalar  Map NEG specify more than 1 function in db with the same name 
        """
        self.runFunctionTest("scalar_map","ambiguousFunction")

    ##### test_composite_finalize ####

    def test_composite_finalize_namedInYml_namedInDB(self):
        """
        composite: finalize:  "param specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("composite_finalize", "namedInYml_namedInDB")

    def test_composite_finalize_namedInYml_unnamedInDB(self):
        """
        composite: finalize:  "param specified in yaml, DB defined with unnamed param" 
        """
        self.runFunctionTest("composite_finalize", "namedInYml_unnamedInDB")

    def test_composite_finalize_unnamedInYml_namedInDB(self):
        """
        composite: finalize:  "param specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("composite_finalize","unnamedInYml_namedInDB")

    def test_composite_finalize_unnamedInYml_unnamedInDB(self):
        """
        composite: finalize:  "parameters not specified in yaml, DB defined with unnamed parameters "
        """
        self.runFunctionTest("composite_finalize","unnamedInYml_unnamedInDB")

    def test_composite_finalize_retValInYml(self):
        """
        composite: finalize: return val specified in yaml, DB defined with named return val
        """
        self.runFunctionTest("composite_finalize","retValInYml")

    def test_composite_finalize_retValNotInYml(self):
        """
        composite: finalize: return value not specified in yaml
        """
        self.runFunctionTest("composite_finalize","retValNotInYml")

    def test_composite_finalize_outParam_namedInYml_namedInDB(self):
        """
        composite: finalize: use named out parameter, specified in yaml 
        """
        self.runFunctionTest("composite_finalize","outParam_namedInYml_namedInDB")

    def test_composite_finalize_outParam_namedInYml_unnamedInDB(self):
        """
        composite: finalize: use unnamed out parameter, specified in yaml
        """
        self.runFunctionTest("composite_finalize","outParam_namedInYml_unnamedInDB")

    def test_composite_finalize_outParam_unnamedInYml_unnamed(self):
        """
        composite: finalize: use unnamed out parameter, not specified in yaml
        """
        self.runFunctionTest("composite_finalize","outParam_unnamedInYml_unnamed")

    def test_composite_finalize_outParam_unnamedInYml_namedInDB(self):
        """
        composite: finalize: use named out parameter, not specified in yaml 
        """
        self.runFunctionTest("composite_finalize","outParam_unnamedInYml_namedInDB")

    def test_composite_finalize_outParam_paramOverride(self):
        """
        composite: finalize: use  named out parameter, specified with a different value in yaml 
        """
        self.runFunctionTest("composite_finalize","outParam_paramOverride")

    def test_composite_finalize_mismatchingReturnVal(self):
        """
        composite: finalize: override return value with what's defined in db vs library 
        """
        self.runFunctionTest("composite_finalize","mismatchingReturnVal")

    def test_composite_finalize_ambiguousFunction(self):
        """
        composite: finalize: NEG specify more than 1 function in db with the same name 
        """
        self.runFunctionTest("composite_finalize","ambiguousFunction")

    def test_composite_finalize_outTableDeclaration_namedInYml(self):
        """ 
        composite: finalize: returns Table declaration, named return Val in Yml
        """
        self.runFunctionTest("composite_finalize","outTableDeclaration_namedInYml")

    def test_composite_finalize_outTableDeclaration_unnamedInYml(self):
        """ 
        composite: finalize: returns Table declaration, named return Val unnamed in Yml
        """
        self.runFunctionTest("composite_finalize","outTableDeclaration_unnamedInYml")

    def test_composite_finalize_outToTable_namedInYml(self):
        """ 
        composite: finalize: returns To DB table, named return Val in Yml
        """
        self.runFunctionTest("composite_finalize","outToTable_namedInYml")

    def test_composite_finalize_outToTable_unnamedInYml(self):
        """ 
        composite: finalize: returns To DB table, named return Val not in Yml
        """
        self.runFunctionTest("composite_finalize","outToTable_unnamedInYml")

    def test_composite_finalize_NEG_2param(self):
        """ 
        composite: finalize: specify more than 1 parameter
        """
        self.runFunctionTest("composite_finalize","NEG_2param")


    ##### test_composite_map #####


    def test_composite_map_namedInYml_namedInDB(self):
        """
        composite: map:  "param specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("composite_map", "namedInYml_namedInDB")

    def test_composite_map_namedInYml_unnamedInDB(self):
        """
        composite: map:  "param specified in yaml, DB defined with unnamed param" 
        """
        self.runFunctionTest("composite_map", "namedInYml_unnamedInDB")

    def test_composite_map_unnamedInYml_namedInDB(self):
        """
        composite: map:  "param specified in yaml, DB defined with named param" 
        """
        self.runFunctionTest("composite_map","unnamedInYml_namedInDB")

    def test_composite_map_unnamedInYml_unnamedInDB(self):
        """
        composite: map:  "parameters not specified in yaml, DB defined with unnamed parameters "
        """
        self.runFunctionTest("composite_map","unnamedInYml_unnamedInDB")

    def test_composite_map_retValInYml(self):
        """
        composite: map: return val specified in yaml, DB defined with named return val
        """
        self.runFunctionTest("composite_map","retValInYml")

    def test_composite_map_retValNotInYml(self):
        """
        composite: map: return value not specified in yaml
        """
        self.runFunctionTest("composite_map","retValNotInYml")

    def test_composite_map_outParam_namedInYml_namedInDB(self):
        """
        composite: map: use named out parameter, specified in yaml 
        """
        self.runFunctionTest("composite_map","outParam_namedInYml_namedInDB")

    def test_composite_map_outParam_namedInYml_unnamedInDB(self):
        """
        composite: map: use unnamed out parameter, specified in yaml
        """
        self.runFunctionTest("composite_map","outParam_namedInYml_unnamedInDB")

    def test_composite_map_outParam_unnamedInYml_unnamed(self):
        """
        composite: map: use unnamed out parameter, not specified in yaml
        """
        self.runFunctionTest("composite_map","outParam_unnamedInYml_unnamed")

    def test_composite_map_outParam_unnamedInYml_namedInDB(self):
        """
        composite: map: use named out parameter, not specified in yaml 
        """
        self.runFunctionTest("composite_map","outParam_unnamedInYml_namedInDB")

    def test_composite_map_outParam_paramOverride(self):
        """
        composite: map: use  named out parameter, specified with a different value in yaml 
        """
        self.runFunctionTest("composite_map","outParam_paramOverride")

    def test_composite_map_mismatchingReturnVal(self):
        """
        composite: map: override return value with what's defined in db vs library 
        """
        self.runFunctionTest("composite_map","mismatchingReturnVal")

    def test_composite_map_ambiguousFunction(self):
        """
        composite: map: NEG specify more than 1 function in db with the same name 
        """
        self.runFunctionTest("composite_map","ambiguousFunction")

    def test_composite_map_outTableDeclaration_namedInYml(self):
        """ 
        composite: map: returns Table declaration, named return Val in Yml
        """
        self.runFunctionTest("composite_map","outTableDeclaration_namedInYml")

    def test_composite_map_outTableDeclaration_unnamedInYml(self):
        """ 
        composite: map: returns Table declaration, named return Val unnamed in Yml
        """
        self.runFunctionTest("composite_map","outTableDeclaration_unnamedInYml")

    def test_composite_map_outToTable_composite_map_namedInYml(self):
        """ 
        composite: map: returns To DB table, named return Val in Yml
        """
        self.runFunctionTest("composite_map","outToTable_composite_map_namedInYml")

    def test_composite_map_outToTable_composite_map_unnamedInYml(self):
        """ 
        composite: map: returns To DB table, named return Val not in Yml
        """
        self.runFunctionTest("composite_map","outToTable_composite_map_unnamedInYml")

