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

import os, sys
import tinctest
from mpp.models import SQLTestCase
from mpp.lib.config import GPDBConfig
from tinctest.lib import local_path, run_shell_command
from mpp.gpdb.tests.package.gppc import Gppc_util
from mpp.lib.gppkg.gppkg import Gppkg

LIBDIR = '%s/lib/postgresql' % (os.environ.get('GPHOME'))
gpdbconfig = GPDBConfig()
gpccutil = Gppc_util()

class GppcTestCase(SQLTestCase):
    """ 
    @optimizer_mode both
    @tags gppkg
    """

    sql_dir = 'sql/'
    ans_dir = 'expected'
    out_dir = 'output/'

    @classmethod
    def setUpClass(cls):
        super(GppcTestCase, cls).setUpClass()
        """
        compile gppc_test.c and install the gppc_test.so
        """
        gpccutil.pre_process()
        cmd = 'gpssh --version'
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command (cmd, 'check product version', res)
        gppkg = Gppkg()
        product_version = res['stdout']
        result = gppkg.gppkg_install(product_version, 'libgppc')
        #makeLog = loal_path('test00MakeLog.out')
        if result:
            cmdMakeInstall = 'cd '+local_path('data')+' && make clean && make CPPFLAGS=-D_GNU_SOURCE && make install'
            res = {'rc':0, 'stderr':'', 'stdout':''}
            run_shell_command (cmdMakeInstall, 'compile gppc_test.c', res)

            # Current make file works for linux, but not for Solaris or OSX.
            # If compilation fails or installation fails, force system quit: os._exit(1)
            if res['rc']:
                os._exit(1) # This will exit the test including the next test suites
            sharedObj = local_path('data/gppc_test.so')
            if not os.path.isfile(sharedObj):
                os._exit(1)

            # For multinode cluster, need to copy shared object tabfunc_gppc_demo.so to all primary segments
            res = {'rc':0, 'stderr':'', 'stdout':''}
            hosts = gpdbconfig.get_hosts(segments=True)
            scp_cmd = 'gpscp  -h ' +' -h '.join(map(str,hosts)) +' '+ sharedObj + ' =:%s' % LIBDIR
            run_shell_command(scp_cmd, 'scp share object to all segment', res)
            tinctest.logger.info('scp shared object result %s' % res['stdout'])
            if res['rc']:
                raise Excpetion('Could not copy shared object to primary segment')

    def setUp(self):
        machine = ''
        if sys.platform == 'linux' or sys.platform == 'linux2': # Both SuSE and RHEL returns linux
            if  os.path.exists("/etc/SuSE-release"):
                machine = 'suse'
            else:
                machine = 'redhat'
        elif sys.platform == 'sunos5':
            machine = 'solaris'
        elif sys.platform =='darwin':
            machine = 'darwin'
        if machine in ["sunos5","darwin","suse"]:
            tinctest.logger.info('skip the test on upsupported platform')
            self.skipTest('skip the test on upsupported platform: %s'%machine)
