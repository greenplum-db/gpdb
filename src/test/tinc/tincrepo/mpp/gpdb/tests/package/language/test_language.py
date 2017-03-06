#!/usr/bin/env python
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
import mpp.lib.PgHba as PgHba
from tinctest.lib import Gpdiff
import sys, time, os, platform, socket
from tinctest.lib import local_path, run_shell_command
from mpp.models import MPPTestCase
from mpp.lib.config import GPDBConfig
from mpp.lib.PSQL import PSQL
from mpp.lib.gpUserRole import GpUserRole
from mpp.lib.mppUtil import generateFileFromEachTemplate
from mpp.lib.gpstop import GpStop
from gppylib.commands.base import Command
from mpp.lib.gppkg.gppkg import Gppkg
from mpp.lib.gp_procedural_languages import gp_procedural_languages

DBNAME = "gptest"
HOST = socket.gethostname()
USER = os.environ.get("LOGNAME")
GPHOME = os.environ.get("GPHOME")
LIBDIR = '%s/lib/postgresql' % (GPHOME)
HAS_RUN_SETUP = False
config = GPDBConfig()

cmd = 'gpssh --version'
res = {'rc':0, 'stderr':'', 'stdout':''}
run_shell_command (cmd, 'check product version', res)
product_version = res['stdout'].split('gpssh version ')[1].split(' build ')[0]

class languageTestCase(MPPTestCase):
    """ Procedural Languages Test """

    def __init__(self, methodName):
        self.gpstop = GpStop()
        self.dbname = os.environ.get('PGDATABASE')
        super(languageTestCase, self).__init__(methodName)

    def restartGPDB(self):
        time.sleep(3)
        self.gpstop.run_gpstop_cmd(restart=True)

    @classmethod
    def setUpClass(self):
        gppkg = Gppkg()
        gppkg.gppkg_install(product_version, 'plr')
        gppkg.gppkg_install(product_version, 'plperl')


    def setUp(self):
        global HAS_RUN_SETUP
        if not HAS_RUN_SETUP:
            self.do_PLPERL_initialize()
            self.restartGPDB()
            HAS_RUN_SETUP = True

    def tearDown(self):
        pass        

    def doTest(self, num=None, filename='query', default='-a', match_sub=[]):
	# get file path to queryXX.sql 

        if num == None:
            sql_file = local_path('%s.sql' % (filename))
            out_file = local_path('%s.out' % (filename))
            ans_file = local_path('%s.ans' % (filename))
        else:
            sql_file = local_path('%s%02d.sql' % (filename, num))
            out_file = local_path('%s%02d.out' % (filename, num))
            ans_file = local_path('%s%02d.ans' % (filename, num))
	# run psql on fname, and check result
        PSQL.run_sql_file(sql_file = sql_file, out_file = out_file, flags=default)
        self.assertTrue(Gpdiff.are_files_equal(out_file=out_file, ans_file=ans_file, match_sub=match_sub))


    def checkAPPHOMEandLIB(self, libso, apphome = ''):
        if apphome == '':
           """ check application library """
           return os.path.isfile("%s/%s.so" % (LIBDIR, libso))
        else:
           """ check application home and library """
           return os.environ.get(apphome) and os.path.isfile("%s/%s.so" % (LIBDIR, libso))

    def runCmd(self, command):
        '''
        run shell command, redirecting standard error message to standard output
        '''
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(command + " 2>&1", 'set access permissions to current jar file', res)
        return (not res['rc'], res['stdout'])


    def doPLR(self, num, filename, default="-a"):
        """ run PL/R test case """
        # If R_HOME is set, then run PL/R
        # Also check whether plr.so is install in $GPHOME/lib/postgresql

        if self.checkAPPHOMEandLIB("plr","R_HOME"):
            gp_procedural_languages().installPL('plr')
            self.doTest(num, filename, default=default)
        else:
            self.skipTest('skipping test: not plr.so found in $GPHOME/lib/postgresql')

    def doSQL(self, num, filename, default='-a'):
        """ run SQL test case """
        # Run test cases for procedure language SQL

        gp_procedural_languages().installPL('sql')
        self.doTest(num, filename, default=default)

    def doPLPGSQL(self, num, filename, default="-a"):
        """ run PL/PGSQL test case """
        # Check if plpgsql.so is installed in $GPHOME/lib/postgresql

        if self.checkAPPHOMEandLIB("plpgsql"):
            gp_procedural_languages().installPL('plpgsql')
            self.doTest(num, filename, default=default)
        else:
            self.skipTest('skipping test: not found plpgsql.so installed in $GPHOME/lib/postgresql')

    def doPLPERLbyuser(self, filename, user):
        """ run PL/PERL test case """
        # Check if plperl.so is installed in $GPHOME/lib/postgresql

        if self.checkAPPHOMEandLIB("plperl"):
            gp_procedural_languages().installPL('plperl')
            sql_file = local_path('%s.sql' % (filename))
            out_file = local_path('%s.out' % (filename))
            ans_file = local_path('%s.ans' % (filename))
            PSQL.run_sql_file(sql_file = sql_file, out_file = out_file, flags='-e', username=user)
            self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file))
        else:
            self.skipTest('skipping test: not found plperl.so installed in $GPHOME/lib/postgresql')


    def doPLPERLUbyuser(self, filename, user):
        """ run PL/PERLU test case """
        # Check if plperl.so is installed in $GPHOME/lib/postgresql

        if self.checkAPPHOMEandLIB("plperl"):
            gp_procedural_languages().installPL('plperlu')
            sql_file = local_path('%s.sql' % (filename))
            out_file = local_path('%s.out' % (filename))
            ans_file = local_path('%s.ans' % (filename))
            PSQL.run_sql_file(sql_file = sql_file, out_file = out_file, flags='-e', username=user)
            self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file))
        else:
            self.skipTest('skipping test: plperl.so is not installed in $GPHOME/lib/postgresql')

    def doPLPERL(self, num, filename, default='-a'):
        """ run PL/PERL test case """
        # Check if plperl.so is installed in $GPHOME/lib/postgresql

        if self.checkAPPHOMEandLIB("plperl"):
            gp_procedural_languages().installPL('plperl')
            self.doTest(num, filename, default=default)
        else:
            self.skipTest('skipping plperl.so is not installed in $GPHOME/lib/postgresql')

    def doPLPERLU(self, num, filename, default='-a'):
        """ run PL/PERLU test case """
        # Check if plperl.so is installed in $GPHOME/lib/postgresql

        if self.checkAPPHOMEandLIB("plperl"):
            gp_procedural_languages().installPL('plperlu')
            self.doTest(num, filename, default=default)
        else:
            self.skipTest('skipping test: if plperl.so is installed in $GPHOME/lib/postgresql')

    def doPLPYTHONU(self, num, filename, default='-a'):
        """ run PL/PYTHONU test case """
        # Check if plpython.so is installed in $GPHOME/lib/postgresql

        if self.checkAPPHOMEandLIB("plpython"):
            gp_procedural_languages().installPL('plpythonu')
            self.doTest(num, filename, default=default)
        else:
            self.skipTest('skipping test: plpython.so is not installed in $GPHOME/lib/postgresql')


    def test_PLR0000(self):
        """Language: PL/R Setup"""
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        self.doPLR(0,"plr/query")

    def test_PLR0001(self):
        """Language: PL/R 1"""
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        self.doPLR(1,"plr/query")

    def test_PLR0002(self):
        """Language: PL/R 2"""
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        self.doPLR(2,"plr/query")

    def test_PLR0003(self):
        """Language: PL/R 3"""
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        self.doPLR(3,"plr/query")

    def test_PLR0004(self):
        """Language: PL/R 4"""
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        self.doPLR(4,"plr/query")

    def test_PLR0005(self):
        """Language: PL/R 5"""
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        self.doPLR(4,"plr/query")

    def test_PLR0006(self):
        """Language: PL/R 6"""
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        self.doPLR(6,"plr/query")

    def test_PLR0007(self):
        """Language: PL/R 7"""
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        self.doPLR(7,"plr/query")

    def test_PLR0008(self):
        """Language: PL/R 8"""
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        generateFileFromEachTemplate(local_path("plr"), ['query08.sql.t','query08.ans.t'], [("@user@", USER)])
        self.doPLR(8,"plr/query")

    def test_PLR0009(self):
        """Language: PL/R 9"""
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        self.doPLR(9,"plr/query")

    def test_PLR0012_mpp16512(self):
        """ mpp16512 - regression for pl/R: R interpreter expression evaluation error OR connection to the server lost"""
        self.doPLR(None, "plr/mpp16512", default='-e')

    def do_PLPERL_initialize(self):
        """ Language PL/PERL upgrade to 9.1: initialize test data  """
        self.doTest(None, "plperl91/test000_initialize", default='-e')
        
        """ Initialize: generate data tbctest.lineitem.tbl, and add users to pg_hba.conf """
        fname = os.environ.get('TINCREPOHOME') + '/mpp/lib/datagen/datasets/lineitem.csv'
        copycmd = 'copy pltest.lineitem from \'' + fname + '\' DELIMITER \'|\';'
        cmd = PSQL(sql_cmd = copycmd, dbname=DBNAME)
        tinctest.logger.info("Running command - %s" %cmd)
        cmd.run(validateAfter = False)
        result = cmd.get_results()
        ok = result.rc
        out = result.stdout
        if ok:
            raise Exception('Copy statement failed: %s'%out )
        gpuserRole = GpUserRole(HOST, USER, DBNAME)
        gpuserRole.createUser('pltestuser','NOSUPERUSER')
        gpuserRole.createUser('plsuperuser','SUPERUSER')
        pg_hba_path = os.path.join(os.environ.get('MASTER_DATA_DIRECTORY'), 'pg_hba.conf')
        print 'pg_hba_path', pg_hba_path
        pghba_file = PgHba.PgHba(pg_hba_path)
        new_ent = PgHba.Entry(entry_type='local',
                              database = DBNAME,
                              user = 'pltestuser',
                              authmethod = 'trust')
        pghba_file.add_entry(new_ent)
        new_ent = PgHba.Entry(entry_type='local',
                              database = DBNAME,
                              user = 'plsuperuser',
                              authmethod = 'trust')
        pghba_file.add_entry(new_ent)
        pghba_file.write()
        grantcmd = 'GRANT ALL ON SCHEMA pltest TO pltestuser;'
        cmd = PSQL(sql_cmd = grantcmd, dbname=DBNAME)
        tinctest.logger.info("Running command - %s" %cmd)
        cmd.run(validateAfter = False)
        result = cmd.get_results()
        ok = result.rc
        out = result.stdout
        if ok:
            raise Exception('Grant all on schema pltest to pltestuser failed: %s'%out )

    def test_PLPERL91001_inparameters(self):
        """ Language PL/PERL upgrade to 9.1: IN parameters """
        self.doPLPERL(None, "plperl91/test001_inparameters", default='-e')

    def test_PLPERL91002_outparameters(self):
        """ Language PL/PERL upgrade to 9.1: OUT parameters """
        self.doPLPERL(None, "plperl91/test002_outparameters", default='-e')

    def test_PLPERL91003_inoutparameters(self):
        """ Language PL/PERL upgrade to 9.1: INOUT parameters """
        self.doPLPERL(None, "plperl91/test003_inoutparameters", default='-e')

    def test_PLPERL91004_miscparameters(self):
        """ Language PL/PERL upgrade to 9.1: Multiple parameters """
        self.doPLPERL(None, "plperl91/test004_miscparameters", default='-e')

    def test_PLPERL91005_buildinfeatures(self):
        """ Language PL/PERL upgrade to 9.1: Buidl in features """
        self.doPLPERL(None, "plperl91/test005_buildinfeatures", default='-e')

    def test_PLPERL91006_returntypes(self):
        """ Language PL/PERL upgrade to 9.1:Different return types """
        self.doPLPERL(None, "plperl91/test006_returntypes", default='-e')

    def test_PLPERL91007_returnrecords(self):
        """ Language PL/PERL upgrade to 9.1:return tables """
        self.doPLPERL(None, "plperl91/test007_returnrecords", default='-e')

    def test_PLPERL91008_returntables(self):
        """ Language PL/PERL upgrade to 9.1:return record """
        self.doPLPERL(None, "plperl91/test008_returntables", default='-e')

    def test_PLPERL91009_trust(self):
        """ Language PL/PERL upgrade to 9.1:File system operations are not allowed for trusted PL/PERL """
        self.doPLPERLbyuser("plperl91/test009_super_trust", 'plsuperuser')
        self.doPLPERLbyuser("plperl91/test009_nonsuper_trust", 'pltestuser')

    def test_PLPERL91010_super_untrust(self):
        """ Language PL/PERL upgrade to 9.1:File system operations are allowed for untrusted PL/PERL """
        if self.checkAPPHOMEandLIB("plperl"):
            print 'installation'
            gp_procedural_languages().installPL('plperlu')
        tmpfilename = local_path('plperl91/plsuperuser.tmp')
        tmpfile = ''
        for i in tmpfilename:
           if i == '/':
               tmpfile = tmpfile + '\/'
           else:
               tmpfile = tmpfile + i
        tmpfilename = tmpfile
        localpath = local_path('')

        if sys.platform == 'sunos5':
            cmd = 'sed \'s/TMPFILENAME/%s/g\' %s/plperl91/test010_super_untrust.sql > %s/plperl91/test010_super_untrust.sql.tmp && mv %s/plperl91/test010_super_untrust.sql.tmp %s/plperl91/test010_super_untrust.sql' % ( tmpfilename, localpath, localpath, localpath, localpath)
        elif sys.platform == 'darwin':
            cmd = 'sed -i \'\' \'s/TMPFILENAME/%s/g\' %s/plperl91/test010_super_untrust.sql' % ( tmpfilename, localpath )
        else:
            cmd = 'sed -i \'s/TMPFILENAME/%s/g\' %s/plperl91/test010_super_untrust.sql' % ( tmpfilename, localpath )
        os.system( cmd )
        self.doPLPERLUbyuser("plperl91/test010_super_untrust", 'plsuperuser')
        checkcmd = 'cat ' + tmpfilename
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(checkcmd, 'run command %s'%checkcmd, res)
        if res['rc']:
            raise Exception("Unable to open created file")

    def test_PLPERL91011_nonsuper_untrust(self):
        """ Language PL/PERL upgrade to 9.1:File system operations are not allowed for untrusted PL/PERL """
        self.doPLPERLUbyuser("plperl91/test011_nonsuper_untrust", 'pltestuser')

    def test_PLPERL91015_largedata_int(self):
        """ Language PL/PERL upgrade to 9.1:large data set """
        self.doPLPERL(None, "plperl91/test015_largedata_int", default='-e')

    def test_PLPERL91016_largedata_type(self):
        """ Language PL/PERL upgrade to 9.1:large data set """
        self.doPLPERL(None, "plperl91/test016_largedata_type", default='-e')

    def test_PLPERL91017_largedata_record(self):
        """ Language PL/PERL upgrade to 9.1:large data set """
        self.doPLPERL(None, "plperl91/test017_largedata_record", default='-e')

