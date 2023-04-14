import os
import shutil

import behave

from test.behave_utils.utils import drop_database_if_exists, start_database_if_not_started,\
                                            create_database, \
                                            run_command, check_user_permissions, run_gpcommand, execute_sql
from steps.mirrors_mgmt_utils import MirrorMgmtContext
from steps.gpconfig_mgmt_utils import GpConfigContext
from steps.gpssh_exkeys_mgmt_utils import GpsshExkeysMgmtContext
from steps.mgmt_utils import backup_bashrc, restore_bashrc
from gppylib.db import dbconn

def before_all(context):
    if list(map(int, behave.__version__.split('.'))) < [1,2,6]:
        raise Exception("Requires at least behave version 1.2.6 (found %s)" % behave.__version__)

def before_feature(context, feature):
    # we should be able to run gpexpand without having a cluster initialized
    tags_to_skip = ['gpexpand', 'gpaddmirrors', 'gpstate', 'gpmovemirrors',
                    'gpconfig', 'gpssh-exkeys', 'gpstop', 'gpinitsystem', 'cross_subnet']
    if set(context.feature.tags).intersection(tags_to_skip):
        return

    drop_database_if_exists(context, 'testdb')
    drop_database_if_exists(context, 'bkdb')
    drop_database_if_exists(context, 'fullbkdb')
    drop_database_if_exists(context, 'schematestdb')

    if 'analyzedb' in feature.tags:
        start_database_if_not_started(context)
        drop_database_if_exists(context, 'incr_analyze')
        create_database(context, 'incr_analyze')
        drop_database_if_exists(context, 'incr_analyze_2')
        create_database(context, 'incr_analyze_2')
        context.conn = dbconn.connect(dbconn.DbURL(dbname='incr_analyze'), unsetSearchPath=False)
        context.dbname = 'incr_analyze'

        # setting up the tables that will be used
        context.execute_steps("""
        Given there is a regular "ao" table "t1_ao" with column name list "x,y,z" and column type list "int,text,real" in schema "public"
        And there is a regular "heap" table "t2_heap" with column name list "x,y,z" and column type list "int,text,real" in schema "public"
        And there is a regular "ao" table "t3_ao" with column name list "a,b,c" and column type list "int,text,real" in schema "public"
        And there is a hard coded partition table "sales" with 4 child partitions in schema "public"
        """)

    if 'gpreload' in feature.tags:
        start_database_if_not_started(context)
        drop_database_if_exists(context, 'gpreload_db')
        create_database(context, 'gpreload_db')
        context.conn = dbconn.connect(dbconn.DbURL(dbname='gpreload_db'), unsetSearchPath=False)
        context.dbname = 'gpreload_db'

    if 'minirepro' in feature.tags:
        start_database_if_not_started(context)
        minirepro_db = 'minireprodb'
        drop_database_if_exists(context, minirepro_db)
        create_database(context, minirepro_db)
        context.conn = dbconn.connect(dbconn.DbURL(dbname=minirepro_db), unsetSearchPath=False)
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

    if 'gppkg' in feature.tags:
        run_command(context, 'bash demo/gppkg/generate_sample_gppkg.sh buildGppkg')
        run_command(context, 'cp -f /tmp/sample-gppkg/sample.gppkg test/behave/mgmt_utils/steps/data/')


def after_feature(context, feature):
    if 'analyzedb' in feature.tags:
        context.conn.close()
    if 'gpreload' in feature.tags:
        context.conn.close()
    if 'minirepro' in feature.tags:
        context.conn.close()
    if 'gpconfig' in feature.tags:
        context.execute_steps('''
            Then the user runs "gpstop -ar"
            And gpstop should return a return code of 0
            ''')

def before_scenario(context, scenario):
    if "skip" in scenario.effective_tags:
        scenario.skip("skipping scenario tagged with @skip")
        return

    if 'gpmovemirrors' in context.feature.tags:
        context.mirror_context = MirrorMgmtContext()

    if 'gpaddmirrors' in context.feature.tags:
        context.mirror_context = MirrorMgmtContext()

    if 'gprecoverseg' in context.feature.tags:
        context.mirror_context = MirrorMgmtContext()

    if 'gprecoverseg_newhost' in context.feature.tags:
        context.mirror_context = MirrorMgmtContext()

    if 'gpconfig' in context.feature.tags:
        context.gpconfig_context = GpConfigContext()

    if 'gpssh-exkeys' in context.feature.tags:
        context.gpssh_exkeys_context = GpsshExkeysMgmtContext(context)

    tags_to_skip = ['gpexpand', 'gpaddmirrors', 'gpstate', 'gpmovemirrors',
                    'gpconfig', 'gpssh-exkeys', 'gpstop', 'gpinitsystem', 'cross_subnet']
    if set(context.feature.tags).intersection(tags_to_skip):
        return

    if 'analyzedb' not in context.feature.tags:
        start_database_if_not_started(context)
        drop_database_if_exists(context, 'testdb')
    if 'gp_bash_functions.sh' in context.feature.tags or 'backup_restore_bashrc' in scenario.effective_tags:
        backup_bashrc()

def after_scenario(context, scenario):
    #TODO: you'd think that the scenario.skip() in before_scenario() would
    #  cause this to not be needed
    if "skip" in scenario.effective_tags:
        return

    if 'tablespaces' in context:
        for tablespace in list(context.tablespaces.values()):
            tablespace.cleanup()

    if 'gpstop' in scenario.effective_tags:
        context.execute_steps('''
            # restart the cluster so that subsequent tests re-use the existing demo cluster
            Then the user runs "gpstart -a"
            And gpstart should return a return code of 0
            ''')

    if 'gp_bash_functions.sh' in context.feature.tags or 'backup_restore_bashrc' in scenario.effective_tags:
        restore_bashrc()

    # NOTE: gpconfig after_scenario cleanup is in the step `the gpconfig context is setup`
    tags_to_skip = ['gpexpand', 'gpaddmirrors', 'gpinitstandby',
                    'gpconfig', 'gpstop', 'gpinitsystem', 'cross_subnet']
    if set(context.feature.tags).intersection(tags_to_skip):
        return

    tags_to_cleanup = ['gpmovemirrors', 'gpssh-exkeys']
    if set(context.feature.tags).intersection(tags_to_cleanup) and "skip_cleanup" not in scenario.effective_tags:
        if 'temp_base_dir' in context and os.path.exists(context.temp_base_dir):
            os.chmod(context.temp_base_dir, 0o700)
            shutil.rmtree(context.temp_base_dir)

    tags_to_not_restart_db = ['analyzedb', 'gpssh-exkeys']
    if not set(context.feature.tags).intersection(tags_to_not_restart_db):
        start_database_if_not_started(context)

        home_dir = os.path.expanduser('~')
        if not check_user_permissions(home_dir, 'write') and hasattr(context, 'orig_write_permission')\
                                                         and context.orig_write_permission:
            run_command(context, 'sudo chmod u+w %s' % home_dir)

        if os.path.isdir('%s/gpAdminLogs.bk' % home_dir):
            shutil.move('%s/gpAdminLogs.bk' % home_dir, '%s/gpAdminLogs' % home_dir)

    if 'gpssh' in context.feature.tags:
        run_command(context, 'sudo tc qdisc del dev lo root netem')

    # for cleaning up after @given('"{path}" has its permissions set to "{perm}"')
    if (hasattr(context, 'path_for_which_to_restore_the_permissions') and
            hasattr(context, 'permissions_to_restore_path_to')):
        os.chmod(context.path_for_which_to_restore_the_permissions, context.permissions_to_restore_path_to)
    elif hasattr(context, 'path_for_which_to_restore_the_permissions'):
        raise Exception('Missing permissions_to_restore_path_to for %s' %
                        context.path_for_which_to_restore_the_permissions)
    elif hasattr(context, 'permissions_to_restore_path_to'):
        raise Exception('Missing path_for_which_to_restore_the_permissions despite the specified permission %o' %
                        context.permissions_to_restore_path_to)

    if 'gpstate' in context.feature.tags:
        create_fault_query = "CREATE EXTENSION IF NOT EXISTS gp_inject_fault;"
        execute_sql('postgres', create_fault_query)
        reset_fault_query = "SELECT gp_inject_fault_infinite('all', 'reset', dbid) FROM gp_segment_configuration WHERE status='u';"
        execute_sql('postgres', reset_fault_query)

    if os.getenv('SUSPEND_PG_REWIND') is not None:
        del os.environ['SUSPEND_PG_REWIND']
