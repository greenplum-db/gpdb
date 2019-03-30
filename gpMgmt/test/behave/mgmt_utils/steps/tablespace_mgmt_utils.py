import shutil
import tempfile

from behave import given, when, then
from pygresql import pg

from gppylib.db import dbconn


class Tablespace:
    def __init__(self, name):
        self.name = name
        self.path = tempfile.mkdtemp()
        self.dbname = 'tablespace_db_%s' % name
        self.table_counter = 0
        self.initial_data = None

        with dbconn.connect(dbconn.DbURL()) as conn:
            db = pg.DB(conn)
            db.query("CREATE TABLESPACE %s LOCATION '%s'" % (self.name, self.path))
            db.query("CREATE DATABASE %s TABLESPACE %s" % (self.dbname, self.name))

        with dbconn.connect(dbconn.DbURL(dbname=self.dbname)) as conn:
            db = pg.DB(conn)
            db.query("CREATE TABLE tbl (i int) DISTRIBUTED RANDOMLY")
            db.query("INSERT INTO tbl VALUES (GENERATE_SERIES(0, 25))")
            # save the distributed data for later verification
            self.initial_data = db.query("SELECT gp_segment_id, i FROM tbl").getresult()

    def cleanup(self):
        with dbconn.connect(dbconn.DbURL()) as conn:
            db = pg.DB(conn)
            db.query("DROP DATABASE IF EXISTS %s" % self.dbname)
            db.query("DROP TABLESPACE IF EXISTS %s" % self.name)

        shutil.rmtree(self.path)

    def verify(self):
        """
        Verify tablespace functionality by ensuring the tablespace can be
        written to, read from, and the initial data is still correctly
        distributed.
        """
        with dbconn.connect(dbconn.DbURL(dbname=self.dbname)) as conn:
            db = pg.DB(conn)
            data = db.query("SELECT gp_segment_id, i FROM tbl").getresult()

            # verify that we can still write to the tablespace
            self.table_counter += 1
            db.query("CREATE TABLE tbl_%s (i int) DISTRIBUTED RANDOMLY" % self.table_counter)
            db.query("INSERT INTO tbl_%s VALUES (GENERATE_SERIES(0, 25))" % self.table_counter)

        if sorted(data) != sorted(self.initial_data):
            raise Exception("Tablespace data is not identically distributed. Expected:\n%r\n but found:\n%r" % (
                sorted(self.initial_data), sorted(data)))


@given('a tablespace is created with data')
def impl(context):
    _create_tablespace_with_data(context, "outerspace")


@given('another tablespace is created with data')
def impl(context):
    _create_tablespace_with_data(context, "myspace")


def _create_tablespace_with_data(context, name):
    if 'tablespaces' not in context:
        context.tablespaces = {}
    context.tablespaces[name] = Tablespace(name)


@then('the tablespace is valid')
def impl(context):
    context.tablespaces["outerspace"].verify()


@then('the other tablespace is valid')
def impl(context):
    context.tablespaces["myspace"].verify()
