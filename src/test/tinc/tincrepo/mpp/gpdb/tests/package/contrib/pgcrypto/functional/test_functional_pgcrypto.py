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

import os
import tinctest
import unittest2 as unittest
from mpp.lib.gppkg.gppkg import Gppkg
from tinctest.lib import run_shell_command
from tinctest.models.scenario import ScenarioTestCase

cmd = 'gpssh --version'
res = {'rc':0, 'stderr':'', 'stdout':''}
run_shell_command (cmd, 'check product version', res)
product_version = res['stdout'].split('gpssh version ')[1].split(' build ')[0]

class PgcryptoScenarioTestCase(ScenarioTestCase):

    def __init__(self, methodName):
        super(PgcryptoScenarioTestCase, self).__init__(methodName)

    @classmethod
    def setUpClass(self):
        super(PgcryptoScenarioTestCase, self).setUpClass()
        gppkg = Gppkg()
        gppkg.gppkg_install(product_version, 'pgcrypto')

        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(". $GPHOME/greenplum_path.sh; psql -d %s -f $GPHOME/share/postgresql/contrib/pgcrypto.sql" % os.environ.get('PGDATABASE'), 'pgcrypto: setup', res)
        tinctest.logger.info('result from installing pgcrypto %s' % res['stdout'])

    def setUp(self):
        command = "gppkg -q --all | grep pgcrypto"
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(command, 'check if pgcrypto was installed', res)
        if (len(res['stdout']) < 1):
            self.skipTest('pgcrypto was not installed, skipping test')

    def test_non_fips_mode(self):
        pgcrypto_test_setup = []
        pgcrypto_test_setup.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.set_fips_mode", {'fips_mode':False}))
        self.test_case_scenario.append(pgcrypto_test_setup)

        pgcrypto_test_list = []
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_pgcrypto_crypt1_test", {'fips_mode':False}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_pgcrypto_crypt2_test", {'fips_mode':False}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_pgcrypto_functions_test", {'fips_mode':False}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_mpp17580_test", {'fips_mode':False}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_digest_test", {'fips_mode':False}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_hmac_test", {'fips_mode':False}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_crypt_and_gen_salt_test", {'fips_mode':False}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_pgp_sym_decrypt_and_encrypt_test", {'fips_mode':False}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_decrypt_and_encrypt_test", {'fips_mode':False}))
        self.test_case_scenario.append(pgcrypto_test_list)

    @unittest.skipIf(product_version.startswith('4.2'), 'skip the test on 4.2 version')
    @unittest.skipIf(product_version.startswith('4.3.6.0'), 'skip the test on 4.3.6.0 (STOP SHIP) version')
    @unittest.skipIf(product_version.startswith('4.3.99.'), 'skip the test on 4.3.99.* (STOP SHIP) version')
    @unittest.skipIf(product_version.startswith('5.'), 'skip the test on 5.x version')
    def test_fips_mode(self): 
        pgcrypto_test_setup = []
        pgcrypto_test_setup.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.set_fips_mode", {'fips_mode':True}))
        self.test_case_scenario.append(pgcrypto_test_setup)

        pgcrypto_test_list = []
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_pgcrypto_crypt1_test", {'fips_mode':True}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_pgcrypto_crypt2_test", {'fips_mode':True}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_pgcrypto_functions_test", {'fips_mode':True}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_mpp17580_test", {'fips_mode':True}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_digest_test", {'fips_mode':True}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_hmac_test", {'fips_mode':True}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_crypt_and_gen_salt_test", {'fips_mode':True}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_pgp_sym_decrypt_and_encrypt_test", {'fips_mode':True}))
        pgcrypto_test_list.append(("mpp.gpdb.tests.package.contrib.pgcrypto.functional.PgcryptoMPPTestCase.do_decrypt_and_encrypt_test", {'fips_mode':True}))
        self.test_case_scenario.append(pgcrypto_test_list)
