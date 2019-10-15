import os

from behave import given, when, then

from test.behave_utils.utils import (
    stop_database,
    run_command,
    stop_primary,
    execute_sql,
    wait_for_unblocked_transactions,
)


from mirrors_mgmt_utils import (add_three_mirrors)


def assert_successful_command(context):
    if context.ret_code != 0:
        raise Exception('%s : %s' % (context.error_message, context.stdout_message))


def create_cluster(context, with_mirrors=True):
    context.initial_cluster_size = 3
    context.current_cluster_size = context.initial_cluster_size

    os.environ['PGPORT'] = '15432'

    cmd = """
    cd ../gpAux/gpdemo; \
        export DEMO_PORT_BASE={port_base} && \
        export NUM_PRIMARY_MIRROR_PAIRS={num_primary_mirror_pairs} && \
        export WITH_MIRRORS={with_mirrors} && \A
        ./demo_cluster.sh -d && ./demo_cluster.sh -c && \
        ./demo_cluster.sh
    """.format(port_base=os.getenv('PORT_BASE', 15432),
               num_primary_mirror_pairs=os.getenv(
                   'NUM_PRIMARY_MIRROR_PAIRS', context.initial_cluster_size),
               with_mirrors=('true' if with_mirrors else 'false'))

    run_command(context, cmd)
    assert_successful_command(context)


def ensure_temp_directory_is_empty(context, temp_directory):
    run_command(context, "rm -rf /tmp/{temp_directory}".format(
        temp_directory=temp_directory))


def expand(context):
    ensure_temp_directory_is_empty(context, "behave_test_expansion_primary")
    ensure_temp_directory_is_empty(context, "behave_test_expansion_mirror")

    expansion_command = """gpexpand --input <(echo '
    localhost|localhost|25438|/tmp/behave_test_expansion_primary|8|3|p
    localhost|localhost|25439|/tmp/behave_test_expansion_mirror|9|3|m
')
"""
    # Initialize
    run_command(context, expansion_command)
    assert_successful_command(context)

    # Redistribute tables
    run_command(context, expansion_command)
    assert_successful_command(context)


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


@when(u'a mirror has crashed')
def step_impl(context):
    run_command(context, "ps aux | grep dbfast_mirror1 | awk '{print $2}' | xargs kill -9")
    wait_for_unblocked_transactions(context)


@when(u'I create a cluster')
def step_impl(context):
    create_cluster(context)


@then(u'the primaries and mirrors should be replicating using replication slots')
def step_impl(context):
    result_cursor = execute_sql(
        "postgres",
        "select pg_get_replication_slots() from gp_dist_random('gp_id') order by gp_segment_id"
    )

    if result_cursor.rowcount != context.current_cluster_size:
        raise Exception("expected all %d primaries to have replication slots, only %d have slots" % (context.current_cluster_size, results.rowcount))

    for content_id, result in enumerate(result_cursor.fetchall()):
        if not result[0].startswith('(internal_wal_replication_slot,,physical,,t,'):
            raise Exception(
                "expected replication slot to be active for content id %d, got %s" %
                (content_id, result[0])
            )

@then(u'the mirrors should not have replication slots')
def step_impl(context):
    result_cursor = execute_sql(
        "postgres",
        "select datadir from gp_segment_configuration where role='m';"
    )

    for content_id, result in enumerate(result_cursor.fetchall()):
        path_to_replslot = os.path.join(result[0], 'pg_replslot')
        if len(os.listdir(path_to_replslot)) > 0:
            raise Exception("expected replication slot directory to be empty")


@given(u'a preferred primary has failed')
def step_impl(context):
    stop_primary(context, 0)
    wait_for_unblocked_transactions(context)


@when('primary and mirror switch to non-preferred roles')
def step_impl(context):
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


@when("I add a segment to the cluster")
def step_imp(context):
    context.current_cluster_size = 4
    expand(context)
