import os

from behave import given, when, then

from test.behave_utils.utils import (
    stop_database,
    run_command,
    stop_primary,
    trigger_fts_probe,
    run_gprecoverseg,
    execute_sql,
)


from addmirrors_mgmt_utils import (add_three_mirrors)


def run_recovery_for_segments(context):
    run_command(context, "gprecoverseg -aFv")

    if context.ret_code != 0:
        raise Exception('%s' % context.error_message)


def create_cluster(context, with_mirrors=True):
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
               with_mirrors=('true' if with_mirrors else 'false'))

    run_command(context, cmd)

    if context.ret_code != 0:
        raise Exception('%s' % context.error_message)


def ensure_primary_mirror_switched_roles():
    results = execute_sql(
        "postgres",
        "select * from gp_segment_configuration where preferred_role <> role"
    )

    if results.rowcount != 2:
        raise Exception("expected 2 segments to not be in preferred roles")


@given(u'I have a machine with no cluster')
def step_impl(context):
    stop_database(context)


@given(u'a mirror has crashed')
def step_impl(context):
    run_command(context, "ps aux | grep dbfast_mirror1 | awk '{print $2}' | xargs kill -9")


@when(u'I create a cluster')
def step_impl(context):
    create_cluster(context)


@then(u'the primaries and mirrors should be replicating using replication slots')
def step_impl(context):
    result_cursor = execute_sql(
        "postgres",
        "select pg_get_replication_slots() from gp_dist_random('gp_id') order by gp_segment_id"
    )

    if result_cursor.rowcount != 3:
        raise Exception("expected all three primaries to have replication slots")

    for content_id, result in enumerate(result_cursor.fetchall()):
        if not result[0].startswith('(internal_wal_replication_slot,,physical,,t,'):
            raise Exception(
                "expected replication slot to be active for content id %d, got %s" %
                (content_id, result[0])
            )


@given(u'a preferred primary has failed')
def step_impl(context):
    stop_primary(context, 0)


@when('primary and mirror switch to non-preferred roles')
def step_impl(context):
    trigger_fts_probe()
    run_gprecoverseg()

    ensure_primary_mirror_switched_roles()



@given("I cluster with no mirrors")
def step_impl(context):
    create_cluster(context, with_mirrors=False)


@when("I add mirrors to the cluster")
def step_impl(context):
    add_three_mirrors(context)


@given("I create a cluster")
def step_impl(context):
    create_cluster(context, with_mirrors=True)


@when("I fully recover a mirror")
def step_impl(context):
    run_recovery_for_segments(context)
