import os

from behave import given, when, then

from test.behave_utils.utils import (
    stop_database,
    run_command,
    execute_sql
)

def create_cluster(context):
    cmd = """
    cd ../gpAux/gpdemo; \
        export MASTER_DEMO_PORT={master_port} && \
        export DEMO_PORT_BASE={port_base} && \
        export NUM_PRIMARY_MIRROR_PAIRS={num_primary_mirror_pairs} && \
        export WITH_MIRRORS={with_mirrors} && \A
        ./demo_cluster.sh -d && ./demo_cluster.sh -c && \
        ./demo_cluster.sh
    """.format(master_port=os.getenv('MASTER_PORT', 15432),
               port_base=os.getenv('PORT_BASE', 25432),
               num_primary_mirror_pairs=os.getenv('NUM_PRIMARY_MIRROR_PAIRS', 3),
               with_mirrors='true')

    run_command(context, cmd)

    if context.ret_code != 0:
        raise Exception('%s' % context.error_message)


@given(u'I have a machine with no cluster')
def step_impl(context):
    stop_database(context)

@when(u'I create a cluster')
def step_impl(context):
    create_cluster(context)

@then(u'the primaries and mirrors should be replicating using replication slots')
def step_impl(context):
    results = execute_sql(
        "postgres",
        "select pg_get_replication_slots() from gp_dist_random('gp_id')"
    )

    if results.rowcount != 3:
        raise Exception("expected all three primaries to have replication slots")

    for result in results.fetchall():
        if not result[0].startswith('(wal_replication_slot,,physical,,t,'):
            raise Exception("expected replication slot to be active")