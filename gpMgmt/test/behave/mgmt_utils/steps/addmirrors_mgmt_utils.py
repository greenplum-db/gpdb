from behave import given, then

from test.behave_utils.utils import *


def _get_mirror_count():
    with dbconn.connect(dbconn.DbURL(dbname='template1')) as conn:
        sql = """SELECT count(*) FROM gp_segment_configuration WHERE role='m'"""
        count_row = dbconn.execSQL(conn, sql).fetchone()
        return count_row[0]


def _generate_input_config():
    datadir_config = _write_datadir_config()

    mirror_config_output_file = "/tmp/test_gpaddmirrors.config"
    cmd_str = 'gpaddmirrors -o %s -m %s' % (mirror_config_output_file, datadir_config)
    Command('generate mirror_config file', cmd_str).run(validateAfter=True)

    return mirror_config_output_file


def _write_datadir_config():
    mdd_parent_parent = os.path.realpath(os.getenv("MASTER_DATA_DIRECTORY") + "../../../")
    mirror_data_dir = os.path.join(mdd_parent_parent, 'mirror')
    if not os.path.exists(mirror_data_dir):
        os.mkdir(mirror_data_dir)
    datadir_config = '/tmp/gpaddmirrors_datadir_config'
    contents = """
{0}
{0}
{0}
""".format(mirror_data_dir)
    with open(datadir_config, 'w') as fp:
        fp.write(contents)
    return datadir_config


@given('the database is initialized with no mirrored segments in the configuration')
def impl(context):
    is_db_up = check_database_is_running(context)
    if is_db_up and _get_mirror_count() == 0:
        return

    # sad path
    if is_db_up:
        stop_database(context)

    cmd = """
    cd ../gpAux/gpdemo && \
        export NUM_PRIMARY_MIRROR_PAIRS=3 && \
        export MASTER_DEMO_PORT={master_port} && \
        export DEMO_PORT_BASE={port_base} && \
        export WITH_MIRRORS=false && \
        ./demo_cluster.sh -d && \
        ./demo_cluster.sh -c && \
        ./demo_cluster.sh
    """.format(master_port=os.getenv('MASTER_PORT', 15432),
               port_base=os.getenv('PORT_BASE', 25432),
               )

    run_command(context, cmd)

    if context.ret_code != 0:
        raise Exception('%s, stdout: %s' % (context.error_message, context.stdout_message))


@then('verify the database has mirrors')
def impl(context):
    if _get_mirror_count() == 0:
        raise Exception('No mirrors found')


@given('gpaddmirrors adds mirrors')
def impl(context):
    context.mirror_config = _generate_input_config()

    cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s ' % context.mirror_config)
    cmd.run(validateAfter=True)


@given('gpaddmirrors adds mirrors with temporary data dir')
def impl(context):
    context.mirror_config = _generate_input_config()
    mdd = os.getenv('MASTER_DATA_DIRECTORY', "")
    del os.environ['MASTER_DATA_DIRECTORY']
    try:
        cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s -d %s' % (context.mirror_config, mdd))
        cmd.run(validateAfter=True)
    finally:
        os.environ['MASTER_DATA_DIRECTORY'] = mdd
