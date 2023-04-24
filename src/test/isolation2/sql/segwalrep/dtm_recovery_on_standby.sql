-- Validate that standby performs DTM recovery upon promotion.

-- Check that are starting with a clean slate, standby must be in sync
-- with coordinator.
select application_name, state, sync_state from pg_stat_replication;

-- Scenario1: standby broadcasts commit-prepared for a transaction
-- that was prepared on coordinator.

-- Suspend coordinator backend before it sends commit-prepared
select gp_inject_fault('dtm_broadcast_commit_prepared', 'suspend', dbid)
from gp_segment_configuration where content = -1 and role = 'p';

1&: create table committed_by_standby(a int, b int);

select gp_wait_until_triggered_fault('dtm_broadcast_commit_prepared', 1, dbid)
from gp_segment_configuration where content = -1 and role = 'p';

-- Scenario2: standby broadcasts abort-prepared for a transaction that
-- doesn't have distributed commit recorded in XLOG.  Inject faults
-- such that the QD backend errors out after prepare broadcast and
-- then suspends in AbortTransaction(), before it can send abort
-- prepared broadcast.  QEs still have the transaction as prepared.
select gp_inject_fault('transaction_abort_after_distributed_prepared', 'error', dbid)
from gp_segment_configuration where content = -1 and role = 'p';
select gp_inject_fault('transaction_abort_failure', 'suspend', dbid)
from gp_segment_configuration where content = -1 and role = 'p';

2&: create table aborted_by_standby(a int, b int);

select gp_wait_until_triggered_fault('transaction_abort_failure', 1, dbid)
from gp_segment_configuration where content = -1 and role = 'p';

-- Promote standby
select pg_ctl(datadir, 'promote') from gp_segment_configuration
where content = -1 and role = 'm';

-- "-1S" means connect to standby's port assuming it's accepting
-- connections.  This select should succeed because the create table
-- transaction's commit prepared broadcast must have been sent by
-- standby after promotion.
-1S: select count(*) from committed_by_standby;

-- Should fail because the create table transaction's abort prepared
-- broadcast must have been sent by standby after promotion.
-1S: select count(*) from aborted_by_standby;

-1Sq:

-- Reset faults.  The suspended backend will be unblocked and try to
-- broadcast commit prepared again.  QEs ignore this request, so that
-- broadcast is essentially a no-op.
select gp_inject_fault('all', 'reset', dbid)
from gp_segment_configuration where content = -1 and role = 'p';
1<:
2<:

-- Destroy and recreate the standby
select pg_ctl(datadir, 'stop', 'immediate') from gp_segment_configuration
where content = -1 and role = 'm';
-- Standby details need to be stored in a separate table because
-- reinitialize_standby() first removes standby from catalog and then
-- adds it to the catalog using gpinitstandby.  If temp table is not
-- used and the function is invoked directly on
-- gp_segment_configuration, we get a deadlock.  The backend started
-- by gpinitstandby needs access exclusive lock and the backend for
-- this isolation spec is already holding an access share lock on
-- gp_segment_configuration.
-- NOTE: the select query should fail since the gang for coordinator has been
-- terminated by the dtx recovery process on standby during standby promote. We
-- do not test the result of the select query; just expect it fail so that the
-- next mpp query could recreate the gang and succeed.
-- start_ignore
select count(*) from committed_by_standby;
-- end_ignore
create table standby_config as (select hostname, datadir, port, role
from gp_segment_configuration where content = -1) distributed by (hostname);

create or replace function reinitialize_standby()
returns text as $$
    import subprocess
    rv = plpy.execute("select hostname, datadir, port from standby_config order by role", 2)
    standby = rv[0] # role = 'c'
    coordinator = rv[1] # role = 'p'
    try:
        cmd = 'rm -rf %s.dtm_recovery && cp -R %s %s.dtm_recovery' % (standby['datadir'], standby['datadir'], standby['datadir'])
        remove_output = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT).decode('ascii')
        cmd = 'gpinitstandby -ar -P %d' % coordinator['port']
        remove_output += subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT).decode('ascii')
        cmd = 'export PGPORT=%d; gpinitstandby -a -s %s -S %s -P %d' % (coordinator['port'], standby['hostname'], standby['datadir'], standby['port'])
        init_output = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT).decode('ascii')
    except subprocess.CalledProcessError as e:
        plpy.info(e.output)
        raise

    if "ERROR" not in remove_output and "FATAL" not in remove_output and \
       "ERROR" not in init_output and "FATAL" not in init_output:
        return "standby initialized"

    return remove_output + "\n" + init_output
$$ language plpython3u;

select reinitialize_standby();

-- Sync state between coordinator and standby must be restored at the end.
select wait_until_standby_in_state('streaming');
