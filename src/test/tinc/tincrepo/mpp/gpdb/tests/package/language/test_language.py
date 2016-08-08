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
        gppkg.gppkg_install(product_version, 'pljava')
        gppkg.gppkg_install(product_version, 'plperl')


    def setUp(self):
        global HAS_RUN_SETUP
        if not HAS_RUN_SETUP:
            self.do_PLJAVA_setup()
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

    def setGPJAVACLASSPATH(self, classpath):
        """ set pljava_classpath in gpdb for pljava and pljavau """
        cmd = "gpconfig -c pljava_classpath -v '%s'" % (classpath)
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(cmd, 'set pljava_classpath', res)
        if res['rc']:
            raise Exception("Can not set pljava_classpath as %s" % (classpath))
        ok = self.gpstop.run_gpstop_cmd(reload = True)
        if not ok:
            raise Exception("Can not reload GPDB configuration after setting pljava_classpath as %s" % (classpath))


    def copyJARFILE(self, srcjarfile):
        """ copy jar file to $GPHOME/lib/postgresql/java on master and all segments """
 
        if not os.path.isfile(srcjarfile):
            raise Exception("Can not find jar file %s" % (srcjarfile))

        hosts = config.get_hosts()
        hoststr = ""
        for host in hosts:
            hoststr += " -h %s" % (host)

        # set acccess permissions to existing jar file so that gpscp can overwrite it with current one
        jarfilename = os.path.basename(srcjarfile)
        cmd = "gpssh%s -e 'chmod -Rf 755 %s/java/%s'" % (hoststr, LIBDIR, jarfilename)
        Command(name = 'set acccess permissions to existing jar', cmdStr = cmd).run(validateAfter=True)

        # copy current jar file to all hosts using gpscp
        cmd = "gpscp%s %s =:%s/java" % (hoststr, srcjarfile, LIBDIR)
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(cmd, 'copy current jar file to all hosts', res)

        if res['rc']:
            raise Exception("Can not copy jar file %s to hosts" % (srcjarfile))

        # set access permissions to current jar file so that it can be accessed by applications
        cmd = "gpssh%s -e 'chmod -Rf 755 %s/java/%s'" % (hoststr, LIBDIR, jarfilename)
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(cmd, 'set access permissions to current jar file', res)

        if res['rc']:
            raise Exception("Can not set access permissions of jar file %s to 755" % (jarfilename))

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

    def doPLJAVA(self, num, filename, default="-a"):
        """ run PL/JAVA test case """
        # If JAVA_HOME is set, then run PL/Java
        # Also check whether pljava.so is install in $GPHOME/lib/postgresql
        init_file_list = []
        init_file = local_path('pljava/init_file')
        init_file_list.append(init_file)
        if self.checkAPPHOMEandLIB("pljava","JAVA_HOME"):
            # If JDK is not 1.6 and up, then raise error
            res = {'rc': 0, 'stdout' : '', 'stderr': ''}
            run_shell_command("java -version 2>&1", 'check java version', res)
            out = res['stdout'].split('\n')
            if out[0].find("1.6.")>0:
                gp_procedural_languages().installPL('pljava')
                self.doTest(num, filename, default=default, match_sub=init_file_list)
            else:
                raise Exception("Requires JDK 1.6 and up, your current version is :%s" % (out[0]))
        else:
            # If JAVA_HOME is not set, then raise error
            if not os.environ.get("JAVA_HOME"):
                raise Exception("JAVA_HOME is not set")


    def doPLJAVAU(self, num, filename, default="-a"):
        """ run PL/JAVAU test case """
        # If JAVA_HOME is set, then run PL/Java
        # Also check whether pljava.so is install in $GPHOME/lib/postgresql

        if self.checkAPPHOMEandLIB("pljava", "JAVA_HOME"):
            # If JDK is not 1.6 and up, then raise error
            res = {'rc': 0, 'stdout' : '', 'stderr': ''}
            run_shell_command("java -version 2>&1", 'check java version', res)
            if res['stdout'][0].find("1.6.")>0:
                gp_procedural_languages().installPL('pljavau')
                self.doTest(num, filename, default=default)
            else:
                raise Exception("Requires JDK 1.6 and up, your current version is :%s" % (out[0]))
        else:
            # If JAVA_HOME is not set, then raise error
            if not os.environ.get("JAVA_HOME"):
                raise Exception("JAVA_HOME is not set")


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


    def test_LANG0001(self):
        """Language: Naive Bayes Classification MPP-3654"""
        self.doTest(1)

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

    def test_PLR0010(self):
        """Language: PL/R Examples from http://www.joeconway.com/plr/"""
        self.skipTest("Test not required. Ref: MPP-23940")
        if self.checkAPPHOMEandLIB("plr","R_HOME"):
            sql_file = local_path("plr/plr-function.sql")
            PSQL.run_sql_file(sql_file = sql_file)
            self.doPLR(1, "plr/plr-test", default='')
        else:
            self.skipTest('skipped')

    def test_PLR0011_function_call_modes(self):
        """ Language PL/R: function call modes """
        if sys.platform in ["sunos5","sunos6"]: self.skipTest("Not supported on solaris")
        self.doPLR(None, "plr/test001_function_call_modes", default='-e')

    def test_PLR0012_mpp16512(self):
        """ mpp16512 - regression for pl/R: R interpreter expression evaluation error OR connection to the server lost"""
        self.doPLR(None, "plr/mpp16512", default='-e')

    def do_PLJAVA_setup(self):
        """Language: PL/Java Setup"""
        if self.checkAPPHOMEandLIB("pljava","JAVA_HOME"):
            sql_file = local_path("pljava/setup.sql")
            PSQL.run_sql_file(sql_file = sql_file)
            javahome = os.environ.get("JAVA_HOME")
            ldpath = "LD_LIBRARY_PATH=%s/jre/lib/amd64/server:$LD_LIBRARY_PATH\nexport LD_LIBRARY_PATH" % javahome
            if platform.machine()=="i686" or platform.machine()=="i386":
                ldpath = "LD_LIBRARY_PATH=%s/i386/server/libjvm.so:$LD_LIBRARY_PATH\nexport LD_LIBRARY_PATH" % javahome
            Command(name='add ldpath into greenplum_path.sh', cmdStr="echo '%s' >> $GPHOME/greenplum_path.sh"%ldpath).run()
            self.gpstop.run_gpstop_cmd(restart=True)
            pljava_install = os.path.join(GPHOME, "share/postgresql/pljava/install.sql") 
            PSQL.run_sql_file(sql_file=pljava_install, dbname='gptest')
        else:
            self.skipTest('skipped')

    @unittest.skipIf(config.is_multinode(), 'Skipping this test on multinode')
    def test_PLJAVA002(self):
        """Language: PL/Java Examples"""
        # Disable this test for OSX
        if os.path.exists("/etc/redhat-release"):
            generateFileFromEachTemplate(local_path("pljava"), ["examples.ans.t"], [("@user@", USER)])
            self.doPLJAVA(None, "pljava/examples", default='')
        else:
            self.skipTest("Not supported on this platform")


    def test_PLJAVA003(self):
        """Language: PL/Java Examples"""
        self.doPLJAVA(None, "pljava/javaguc", default='')

    def test_PLJAVA004_function_call_modes(self):
        "" "Language PL/JAVA: function call modes """
        self.skipTest('skipped')
        self.copyJARFILE(local_path("pljava/pljavatest.jar"))
        self.setGPJAVACLASSPATH("pljavatest.jar")
        self.doPLJAVA(None, "pljava/test001_function_call_modes", default='-e')
        self.setGPJAVACLASSPATH("examples.jar")

    def test_PLJAVAU001_function_call_modes(self):
        """ Language PL/JAVAU: function call modes """
        self.skipTest('skipped')        
        self.copyJARFILE(local_path("pljavau/pljavatest.jar"))
        self.setGPJAVACLASSPATH("pljavatest.jar")
        self.doPLJAVAU(None, "pljavau/test001_function_call_modes", default='-e')
        self.setGPJAVACLASSPATH("examples.jar")

    def test_SQL001_function_call_modes(self):
        """ Language SQL: function call modes """
        self.doSQL(None, "sql/test001_function_call_modes", default='-e')

    def test_PLPGSQL001_function_call_modes(self):
        """ Language PL/PGSQL: function call modes """
        self.doPLPGSQL(None, "plpgsql/test001_function_call_modes", default='-e')

    def test_PLPERL001_function_call_modes(self):
        """ Language PL/PERL: function call modes """
        self.doPLPERL(None, "plperl/test001_function_call_modes", default='-e')

    def test_PLPERLU001_function_call_modes(self):
        """ Language PL/PERLU: function call modes """
        self.doPLPERLU(None, "plperlu/test001_function_call_modes", default='-e')

    def test_PLPYTHONU001_function_call_modes(self):
        """ Language PL/PYTHONU: function call modes """
        self.doPLPYTHONU(None, "plpythonu/test001_function_call_modes", default='-e')

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

