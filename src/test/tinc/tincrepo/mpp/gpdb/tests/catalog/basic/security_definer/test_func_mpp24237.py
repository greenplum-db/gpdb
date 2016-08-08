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
from mpp.models import MPPTestCase

from tinctest.lib import Gpdiff

from tinctest.lib import local_path, run_shell_command
from mpp.lib.PSQL import PSQL


class SecurityDefinerTestCase(MPPTestCase):
    '''
    @product_version gpdb: [4.2.8.2-4.2.99.99], [4.3.3-]
    '''

    def test_MPP24237(self):

        cmd_cleanup = "psql -Atc \"select datname from pg_database where datname != \'template0\'\" | while read a; do echo \"check for ${a}\";psql -Atc \"select \'drop schema if exists \' || nspname || \' cascade;\' from (select nspname from pg_namespace where nspname like \'pg_temp%\' union select nspname from gp_dist_random(\'pg_namespace\') where nspname like \'pg_temp%\' except select \'pg_temp_\' || sess_id::varchar from pg_stat_activity) as foo\" ${a}; done"

        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command(cmd_cleanup, 'do_clean', res)

        if res['rc'] > 0:
            raise Exception("Failed to do cleanup %s" %res[stderr])

        PSQL.run_sql_file(local_path('pre_script.sql'), out_file=local_path('pre_script.out'))
        self.assertTrue(Gpdiff.are_files_equal(local_path('pre_script.out'), local_path('pre_script.ans')))

        cmd = "select count(*) from pg_tables where schemaname like 'pg_temp%';"
        out = PSQL.run_sql_command(cmd, flags ='-q -t')

        if int(out) != 0:
            tinctest.logger.info("temp tables found")
            tinctest.logger.info(PSQL.run_sql_command("select * from pg_tables where schemaname like 'pg_temp%';"))
            self.fail("temp tables were found")
            PSQL.run_sql_file(local_path('clean_script.sql'))

        PSQL.run_sql_file(local_path('clean_script.sql'))

        run_shell_command(cmd_cleanup, 'do_clean', res)
        if res['rc'] > 0:
            raise Exception("Failed to do cleanup %s" %res[stderr])

