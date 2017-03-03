import sys
import os
import tinctest
from mpp.lib.PSQL import PSQL
from tinctest.lib import local_path
from gppylib.commands.base import Command, ExecutionError
from mpp.gpdb.lib.models.sql.template import SQLTemplateTestCase

gphome = os.environ["GPHOME"]

def build_udf():
    tinctest.logger.info('Building UDF library runaway_test.so in the %s directory' % local_path('./udfs'))
    Command('re-build runaway_test.so',
            'make -C %s/udfs runaway_test.so' % local_path('.')).run(validateAfter=True)

def create_runaway_udf(dbname = None):
    build_udf()

    SQLTemplateTestCase.perform_transformation_on_sqlfile(local_path('./udfs/runaway_test.sql.in'),
        local_path('./udfs/runaway_test.sql'),
        {'@source@' : local_path('./udfs/runaway_test.so')})

    tinctest.logger.info('Creating Runaway Query Termination testing UDFs by running sql file %s' % local_path('./udfs/runaway_test.sql'))

    PSQL.run_sql_file(sql_file=local_path('./udfs/runaway_test.sql'),
        out_file=local_path('./udfs/runaway_test.out'), dbname=dbname)


def drop_runaway_udf(dbname = None):
    tinctest.logger.info('Removing Runaway Query Termination testing UDFs by running sql file %s' % local_path('./udfs/uninstall_runaway_test.sql'))

    PSQL.run_sql_file(sql_file=local_path('./udfs/uninstall_runaway_test.sql'),
                      out_file=local_path('./udfs/uninstall_runaway_test.out'), dbname=dbname)

    
def create_session_state_view(dbname = None):
    cmd = os.path.join(gphome, "share/postgresql/contrib/gp_session_state.sql")
    tinctest.logger.info('Creating Session State View by running sql file %s' % cmd)
    PSQL.run_sql_file(cmd, dbname=dbname)

def drop_session_state_view(dbname = None):
    cmd = os.path.join(gphome, "share/postgresql/contrib/uninstall_gp_session_state.sql")
    tinctest.logger.info('Removing Session State View by running sql file %s' % cmd)
    PSQL.run_sql_file(cmd, dbname=dbname)
