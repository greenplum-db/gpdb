-- This test triggers failover of content 0 and content 1
-- Content 0 is used to test if FTS can handle DNS errors
-- Content 1 is used to test the gang interaction in various
-- sessions when a failover is triggered and mirror is promoted
-- to primary
create extension if not exists gp_inject_fault;

-- start_matchsubs
-- m/^ERROR:  Error on receive from .*: server closed the connection unexpectedly/
-- s/^ERROR:  Error on receive from .*: server closed the connection unexpectedly/ERROR: server closed the connection unexpectedly/
-- end_matchsubs

-- to make test deterministic and fast
!\retcode gpconfig -c gp_fts_probe_retries -v 2 --masteronly;

-- Allow extra time for mirror promotion to complete recovery to avoid
-- gprecoverseg BEGIN failures due to gang creation failure as some primaries
-- are not up. Setting these increase the number of retries in gang creation in
-- case segment is in recovery. Approximately we want to wait 30 seconds.
!\retcode gpconfig -c gp_gang_creation_retry_count -v 127 --skipvalidation --masteronly;
!\retcode gpconfig -c gp_gang_creation_retry_timer -v 250 --skipvalidation --masteronly;
!\retcode gpstop -u;

include: helpers/server_helpers.sql;

-- no segment down.
select count(*) from gp_segment_configuration where status = 'd';

1:BEGIN;
1:END;
2:BEGIN;
3:BEGIN;
3:CREATE TEMP TABLE tmp3 (c1 int, c2 int);
3:DECLARE c1 CURSOR for select * from tmp3;
4:CREATE TEMP TABLE tmp4 (c1 int, c2 int);
5:BEGIN;
5:CREATE TEMP TABLE tmp5 (c1 int, c2 int);
5:SAVEPOINT s1;
5:CREATE TEMP TABLE tmp51 (c1 int, c2 int);

-- probe to make sure when we call gp_request_fts_probe_scan() next
-- time below, don't overlap with auto-trigger of FTS scans by FTS
-- process. As if that happens, due to race condition will not trigger
-- the fault and fail the test.
select gp_request_fts_probe_scan();
-- stop a primary in order to trigger a mirror promotion for content 1
select pg_ctl((select datadir from gp_segment_configuration c
where c.role='p' and c.content=1), 'stop');

-- trigger a DNS error. This fault internally gets trigerred for content 0
select gp_inject_fault_infinite('get_dns_cached_address', 'skip', 1);

-- trigger failover
select gp_request_fts_probe_scan();

select gp_inject_fault_infinite('get_dns_cached_address', 'reset', 1);

-- verify a segment is down
select count(*) from gp_segment_configuration where status = 'd';

-- session 1: in no transaction and no temp table created, it's safe to
--            update cdb_component_dbs and use the new promoted primary 
1:BEGIN;
1:END;
-- session 2: in transaction, gxid is dispatched to writer gang, cann't
--            update cdb_component_dbs, following query should fail
2:END;
-- session 3: in transaction and has a cursor, cann't update
--            cdb_component_dbs, following query should fail 
3:FETCH ALL FROM c1;
3:END;
-- session 4: not in transaction but has temp table, cann't update
--            cdb_component_dbs, following query should fail and session
--            is reset 
4:select * from tmp4;
4:select * from tmp4;
-- session 5: has a subtransaction, cann't update cdb_component_dbs,
--            following query should fail 
5:select * from tmp51;
5:ROLLBACK TO SAVEPOINT s1;
5:END;
1q:
2q:
3q:
4q:
5q:

-- immediate stop mirror for content 0. This is just to speed up the test, next
-- step gprecovertseg will do the same but it uses gpstop fast mode and not
-- immediate, which add time to tests.
select pg_ctl((select datadir from gp_segment_configuration c
where c.role='m' and c.content=0), 'stop');

-- fully recover the failed primary as new mirror
!\retcode gprecoverseg -aF;

-- loop while segments come in sync
do $$
begin /* in func */
  for i in 1..120 loop /* in func */
    if (select count(*) = 2 from gp_segment_configuration where content in (0, 1) and mode = 's' and role = 'p') then /* in func */ 
      return; /* in func */
    end if; /* in func */
    perform gp_request_fts_probe_scan(); /* in func */
  end loop; /* in func */
end; /* in func */
$$;

!\retcode gprecoverseg -ar;

-- loop while segments come in sync
do $$
begin /* in func */
  for i in 1..120 loop /* in func */
    if (select count(*) = 2 from gp_segment_configuration where content in (0, 1) and mode = 's' and role = 'p') then /* in func */ 
      return; /* in func */
    end if; /* in func */
    perform gp_request_fts_probe_scan(); /* in func */
  end loop; /* in func */
end; /* in func */
$$;

-- verify no segment is down after recovery
select count(*) from gp_segment_configuration where status = 'd';

!\retcode gpconfig -r gp_fts_probe_retries --masteronly;
!\retcode gpconfig -r gp_gang_creation_retry_count --skipvalidation --masteronly;
!\retcode gpconfig -r gp_gang_creation_retry_timer --skipvalidation --masteronly;
!\retcode gpstop -u;


