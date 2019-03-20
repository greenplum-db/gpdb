import tempfile
import os

from behave import given, when, then
from pygresql import pg

from gppylib.db import dbconn

@given('a tablespace is created')
def impl(context):
    name = "outerspace"
    path = tempfile.mkdtemp()
    context.tablespace = (name, path)

    with dbconn.connect(dbconn.DbURL()) as conn:
        db = pg.DB(conn)
        db.query("CREATE TABLESPACE %s LOCATION '%s'" % (name, path))

@then('the tablespace directories got created on the standby')
def impl(context):
    name, path = context.tablespace
    tablespace_dirs = os.listdir(path)

    with dbconn.connect(dbconn.DbURL()) as conn:
        sql = "SELECT dbid FROM gp_segment_configuration WHERE content=-1 AND role='m';"
        standby_dbid = dbconn.execSQLForSingleton(conn, sql)

    for d in tablespace_dirs:
        if d.endswith("_db{}".format(standby_dbid)):
            return

    raise Exception('Expected to find a standby tablespace directory ending in "_db%d" but found %r' % (
        standby_dbid, sorted(tablespace_dirs)
    ))
