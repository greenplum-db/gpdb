import os
import glob

from test.behave_utils.utils import execute_sql_singleton

master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
if master_data_dir is None:
    raise Exception('Please set MASTER_DATA_DIRECTORY in environment')

def gp_fts_log_in_master_log_count(mdd):
    return gp_in_master_log_count(mdd, 'FTS: probe result processing is complete')

def gp_in_master_log_count(mdd, pattern):
    pg_log_glob = os.path.join(mdd, 'pg_log', 'gpdb-20??-??-*.csv')
    files = glob.glob(pg_log_glob)
    files.sort()
    if not files:
        raise Exception('pg_log not found with the following pattern on master: %s' % pg_log_glob)
    fd = open(files[-1])
    output = fd.read()
    counter = 0
    for line in output.splitlines():
        if pattern in line:
            counter = counter + 1
    return counter

def gp_fts_log_in_segment_log_count():
    COUNT_SQL = """
        SELECT count(*) from gp_toolkit.__gp_log_segment_ext 
        WHERE logmessage like '%FTS: Probe Request%';
        """

    result = execute_sql_singleton('template1', COUNT_SQL)
    return result

def get_latest_pglog_message_containing(msg):
    MSG_SQL = """
        SELECT count(*) FROM gp_toolkit.gp_log_system WHERE logmessage LIKE '%{0}%'
        """
    # MSG_SQL = """
    #     SELECT count(*) FROM gp_toolkit.gp_log_system
    #     """
    result = execute_sql_singleton('template1', MSG_SQL.format(msg))
    return result

QUERY_PLAN_SIZE_STRING = 'Query plan size to dispatch:'

@given(u'the count for "{msg}" in pglog is stored')
def impl(context, msg):
    if 'pglog_count' not in context:
        context.pglog_count = {}
    context.pglog_count[msg] = get_latest_pglog_message_containing(msg)

@then(u'the count for "{msg}" in pglog is incremented by "{count}"')
def impl(context, msg, count):
    prev_count = context.pglog_count[msg]
    curr_count = get_latest_pglog_message_containing(msg)
    if curr_count != prev_count + int(count):
        raise Exception("msg count didn't increase as much as expected")

@then(u'the count of query plan logs in pg_log is stored')
def impl(context):
    context.master_plan_size_count = gp_in_master_log_count(master_data_dir, QUERY_PLAN_SIZE_STRING)

@then(u'the count of query plan logs is not changed')
def impl(context):
    count = gp_in_master_log_count(master_data_dir, QUERY_PLAN_SIZE_STRING)
    if count != context.master_plan_size_count:
        raise Exception("'%s' is still being logged in pg_log when it should be off counts (%d, %d)" % (QUERY_PLAN_SIZE_STRING, count, context.master_plan_size_count))
    print "IVAN  query plan logs is unchanged: %d %d" % (count, context.master_plan_size_count)

@then(u'the count of query plan logs is increased')
def impl(context):
    count = gp_in_master_log_count(master_data_dir, QUERY_PLAN_SIZE_STRING)
    if count <= context.master_plan_size_count:
        raise Exception("'%s' is not being logged in pg_log when it should be on counts (%d, %d)" % (QUERY_PLAN_SIZE_STRING, count, context.master_plan_size_count))
    print "IVAN  query plan logs is increased: %d %d" % (count, context.master_plan_size_count)

@then('the count of verbose logs in pg_log is stored')
def impl(context):
    context.master_fts_log_count = gp_fts_log_in_master_log_count(master_data_dir)
    context.segment_fts_log_count = gp_fts_log_in_segment_log_count()

@then('the count of verbose fts logs is not changed')
def impl(context):
    master_fts_log_count = gp_fts_log_in_master_log_count(master_data_dir)
    segment_fts_log_count = gp_fts_log_in_segment_log_count()

    if master_fts_log_count != context.master_fts_log_count:
        raise Exception("Number of FTS logs on master has changed when logging is turned off: orig count %d new count %d" % (context.master_fts_log_count, master_fts_log_count))

    if segment_fts_log_count != context.segment_fts_log_count:
        raise Exception("Number of FTS logs on segments has changed when logging is turned off: orig count %d new count %d" % (context.segment_fts_log_count, segment_fts_log_count))

    context.master_fts_log_count = master_fts_log_count
    context.segment_fts_log_count = segment_fts_log_count

@then('the count of verbose fts logs is increased on all segments')
def impl(context):
    master_fts_log_count = gp_fts_log_in_master_log_count(master_data_dir)
    segment_fts_log_count = gp_fts_log_in_segment_log_count()

    if master_fts_log_count <= context.master_fts_log_count:
        raise Exception("Number of FTS logs on master has not increased changed when logging is turned on: orig count %d new count %d" % (context.master_fts_log_count, master_fts_log_count))

    if segment_fts_log_count <= context.segment_fts_log_count:
        raise Exception("Number of FTS logs on segments has not increased when logging is turned on: orig count %d new count %d" % (context.segment_fts_log_count, segment_fts_log_count))

    context.master_fts_log_count = master_fts_log_count
    context.segment_fts_log_count = segment_fts_log_count


