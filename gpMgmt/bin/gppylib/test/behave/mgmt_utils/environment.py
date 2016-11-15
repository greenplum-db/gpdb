import os
import shutil

from gppylib.test.behave_utils.utils import drop_database_if_exists, start_database_if_not_started,\
                                            create_database, enable_postgis_and_load_test_data,    \
                                            enable_postgis_and_load_test_data_for_postgis_1,       \
                                            run_command, check_user_permissions, run_gpcommand
from gppylib.db import dbconn

def before_feature(context, feature):
    drop_database_if_exists(context, 'testdb')
    drop_database_if_exists(context, 'bkdb')
    drop_database_if_exists(context, 'fullbkdb')
    drop_database_if_exists(context, 'schematestdb')

    # enable postgis when running test cases for postgis
    if 'postgis' in feature.tags:
        drop_database_if_exists(context, 'opengeo')
        create_database(context, 'opengeo')
        enable_postgis_and_load_test_data(context)

    if 'postgis_14' in feature.tags:
        drop_database_if_exists(context, 'opengeo')
        create_database(context, 'opengeo')
        enable_postgis_and_load_test_data_for_postgis_1(context)

    if 'gpperfmon' in feature.tags:
        drop_database_if_exists(context, 'gpperfmon')
        pgport = os.getenv('PGPORT', 5432)
        command = "gpperfmon_install --enable --password changeme --port %s" % pgport
        run_gpcommand(context, command)
        run_gpcommand(context, "gpstop -ar")

    if 'analyzedb' in feature.tags:
        start_database_if_not_started(context)
        drop_database_if_exists(context, 'incr_analyze')
        create_database(context, 'incr_analyze')
        drop_database_if_exists(context, 'incr_analyze_2')
        create_database(context, 'incr_analyze_2')
        context.conn = dbconn.connect(dbconn.DbURL(dbname='incr_analyze'))
        context.dbname = 'incr_analyze'

        # setting up the tables that will be used
        context.execute_steps(u"""
        Given there is a regular "ao" table "t1_ao" with column name list "x,y,z" and column type list "int,text,real" in schema "public"
        And there is a regular "heap" table "t2_heap" with column name list "x,y,z" and column type list "int,text,real" in schema "public"
        And there is a regular "ao" table "t3_ao" with column name list "a,b,c" and column type list "int,text,real" in schema "public"
        And there is a hard coded ao partition table "sales" with 4 child partitions in schema "public"
        """)

    if 'minirepro' in feature.tags:
        start_database_if_not_started(context)
        minirepro_db = 'minireprodb'
        drop_database_if_exists(context, minirepro_db)
        create_database(context, minirepro_db)
        context.conn = dbconn.connect(dbconn.DbURL(dbname=minirepro_db))
        context.dbname = minirepro_db
        dbconn.execSQL(context.conn, 'create table t1(a integer, b integer)')
        dbconn.execSQL(context.conn, 'create table t2(c integer, d integer)')
        dbconn.execSQL(context.conn, 'create table t3(e integer, f integer)')
        dbconn.execSQL(context.conn, 'create view v1 as select a, b from t1, t3 where t1.a=t3.e')
        dbconn.execSQL(context.conn, 'create view v2 as select c, d from t2, t3 where t2.c=t3.f')
        dbconn.execSQL(context.conn, 'create view v3 as select a, d from v1, v2 where v1.a=v2.c')
        dbconn.execSQL(context.conn, 'insert into t1 values(1, 2)')
        dbconn.execSQL(context.conn, 'insert into t2 values(1, 3)')
        dbconn.execSQL(context.conn, 'insert into t3 values(1, 4)')
        context.conn.commit()

def after_feature(context, feature):
    if 'analyzedb' in feature.tags:
        context.conn.close()
    if 'minirepro' in feature.tags:
        context.conn.close()

def before_scenario(context, scenario):
    if 'analyzedb' not in context.feature.tags:
        master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
        fn = os.path.join(master_data_dir, 'dirty_hack.txt')
        if os.path.exists(fn):
            os.remove(fn)
        start_database_if_not_started(context)
        drop_database_if_exists(context, 'testdb')

def after_scenario(context, scenario):
    if 'analyzedb' not in context.feature.tags:
        master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
        fn = os.path.join(master_data_dir, 'dirty_hack.txt')
        if os.path.exists(fn):
            #os.remove(fn)
            pass
        start_database_if_not_started(context)

        home_dir = os.path.expanduser('~')
        if not check_user_permissions(home_dir, 'write') and hasattr(context, 'orig_write_permission')\
                                                         and context.orig_write_permission:
            run_command(context, 'sudo chmod u+w %s' % home_dir)

        if os.path.isdir('%s/gpAdminLogs.bk' % home_dir):
            shutil.move('%s/gpAdminLogs.bk' % home_dir, '%s/gpAdminLogs' % home_dir)
