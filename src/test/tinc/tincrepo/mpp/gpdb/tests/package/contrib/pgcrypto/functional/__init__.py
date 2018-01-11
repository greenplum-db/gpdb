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
from mpp.lib.PSQL import PSQL
from tinctest.lib import Gpdiff
from mpp.lib.gpstop import GpStop
from mpp.models import MPPTestCase
from tinctest.lib import local_path, run_shell_command

class PgcryptoMPPTestCase(MPPTestCase):

    def __init__(self, methodName):
        self.gpstop = GpStop()
        super(PgcryptoMPPTestCase, self).__init__(methodName)

    def set_fips_mode(self, fips_mode = False):
        res = {'rc':0, 'stderr':'', 'stdout':''}
        if fips_mode:
            cmd = 'gpconfig -c custom_variable_classes -v pgcrypto --skipvalidation && gpconfig -c pgcrypto.fips -v on --skipvalidation'
        else:
            cmd = 'gpconfig -r custom_variable_classes && gpconfig -r pgcrypto.fips --skipvalidation'
        run_shell_command (cmd, 'run command %s' % cmd, res)
        tinctest.logger.info('Unable to config the fips_mode, stdout: %s /n error: %s' % (res['stdout'], res['stderr']))
        self.gpstop.run_gpstop_cmd(restart='ar')

    def find_error_in_nonfips(self, file = None):
        fd = open(file, 'r')
        contains_error = False
        for line in fd:
            if 'ERROR' in line:
                contains_error = True
                break
        fd.close()
        return contains_error

    def checkLIB(self,libso):
        return os.path.isfile("%s/lib/postgresql/%s.so" % (os.environ.get("GPHOME"),libso))

    def doTest(self, sql_file=None, out_file=None, ans_file=None):
        PSQL.run_sql_file(sql_file = sql_file, out_file = out_file)
        init_file_list = []
        init_file = local_path('init_file')
        init_file_list.append(init_file)
        self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file, match_sub=init_file_list))


    def doPGCRYPT(self, sql_file=None, out_file=None, ans_file=None):
        if self.checkLIB("pgcrypto"):
            self.doTest(sql_file=sql_file, out_file=out_file, ans_file=ans_file)
        else:
            # pgcrypto is not found, skipping test
            self.skipTest() #pgcrypto was not found, skipping test

    def do_pgcrypto_crypt1_test(self, fips_mode = False):
        """ pgcrypto: crypt function"""
        sql_file = local_path('query01.sql')
        out_file = (local_path('query01.out') if not fips_mode else local_path('query01_fips.out'))
        ans_file = (local_path('query01.ans') if not fips_mode else local_path('query01_fips.ans'))
        self.doPGCRYPT(sql_file, out_file, ans_file)

    def do_pgcrypto_crypt2_test(self, fips_mode = False):
        """ pgcrypto: crypt functions 2 way encryption with pgp"""
        sql_file = local_path('query02.sql')
        out_file = (local_path('query02.out') if not fips_mode else local_path('query02_fips.out'))
        ans_file = (local_path('query02.ans') if not fips_mode else local_path('query02_fips.ans'))
        self.doPGCRYPT(sql_file, out_file, ans_file)

    def do_pgcrypto_functions_test(self, fips_mode = False):
        """ pgcrypto: other pgcrypto functions"""
        sql_file = local_path('query03.sql')
        out_file = (local_path('query03.out') if not fips_mode else local_path('query03_fips.out'))
        ans_file = (local_path('query03.ans') if not fips_mode else local_path('query03_fips.ans'))
        self.doPGCRYPT(sql_file, out_file, ans_file)

    def do_mpp17580_test(self, fips_mode = False):
        """ mpp17580: incorrect password transformation"""
        sql_file = local_path('query04.sql')
        out_file = (local_path('query04.out') if not fips_mode else local_path('query04_fips.out'))
        ans_file = (local_path('query04.ans') if not fips_mode else local_path('query04_fips.ans'))
        self.doPGCRYPT(sql_file, out_file, ans_file)

    def do_digest_test(self, fips_mode = False):
        """When fip mode is on,  digest allows sha algos only (not md5)"""
        sql_file = local_path('query05.sql')
        out_file = (local_path('query05.out') if not fips_mode else local_path('query05_fips.out'))
        ans_file = (local_path('query05.ans') if not fips_mode else local_path('query05_fips.ans'))
        self.doPGCRYPT(sql_file, out_file, ans_file)

    def do_hmac_test(self, fips_mode = False):
        """When fip mode is on, hmac allow sha algos only (not md5)"""
        sql_file = local_path('query06.sql')
        out_file = (local_path('query06.out') if not fips_mode else local_path('query06_fips.out'))
        ans_file = (local_path('query06.ans') if not fips_mode else local_path('query06_fips.ans'))
        self.doPGCRYPT(sql_file, out_file, ans_file)

    def do_crypt_and_gen_salt_test(self, fips_mode = False):
        """When fip mode is on,  crypt and gen_salt algos are disabled, this test case is diferent
           from others in the sense that when in nonfipes mode, it does not generate constant 
           encryption data. Therefore, to verify that it works for non-fips mode, needs to check
           that no error message in the output file
        """
        sql_file = local_path('query07.sql')
        out_file = (local_path('query07.out') if not fips_mode else local_path('query07_fips.out'))
        if fips_mode:
            ans_file = local_path('query07_fips.ans')
            self.doPGCRYPT(sql_file, out_file, ans_file)
        else:
            PSQL.run_sql_file(sql_file = sql_file, out_file = out_file)
            self.assertFalse(self.find_error_in_nonfips(file = out_file))

    def do_pgp_sym_decrypt_and_encrypt_test(self, fips_mode = False):
        """When fip mode is on,  pgp encrypt/decrypt functions support AES only (not blowfish)"""
        sql_file = local_path('query08.sql')
        out_file = (local_path('query08.out') if not fips_mode else local_path('query08_fips.out'))
        ans_file = (local_path('query08.ans') if not fips_mode else local_path('query08_fips.ans'))
        self.doPGCRYPT(sql_file, out_file, ans_file)

    def do_decrypt_and_encrypt_test(self, fips_mode = False):
        """When fip mode is on, raw encrypt/decrypt functions support AES only (not blowfish)"""
        sql_file = local_path('query09.sql')
        out_file = (local_path('query09.out') if not fips_mode else local_path('query09_fips.out'))
        ans_file = (local_path('query09.ans') if not fips_mode else local_path('query09_fips.ans'))
        self.doPGCRYPT(sql_file, out_file, ans_file)
