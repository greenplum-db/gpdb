-- Test a bug that restarting a primary could lead to startup process hang if
-- before shutdown checkpoint there are prepared but not yet committed/aborted
-- transactions.

include: helpers/server_helpers.sql;

-- Allow extra time for mirror promotion to complete recovery to avoid
-- gprecoverseg BEGIN failures due to gang creation failure as some primaries
-- are not up. Setting these increase the number of retries in gang creation in
-- case segment is in recovery.
!\retcode gpconfig -c gp_gang_creation_retry_count -v 120 --skipvalidation --masteronly;
!\retcode gpconfig -c gp_gang_creation_retry_timer -v 1000 --skipvalidation --masteronly;
!\retcode gpstop -u;

create extension if not exists gp_inject_fault;

create table t_ckpt (a int);

-- generate an 'orphaned' prepare transaction.
select gp_inject_fault('dtm_broadcast_prepare', 'suspend', dbid)
  from gp_segment_configuration where role = 'p' and content = -1;
-- assume (2), (1) are on different segments and one tuple is on the first
-- segment.  the test finally double-check that.
1&: insert into t_ckpt values(2),(1);
select gp_wait_until_triggered_fault('dtm_broadcast_prepare', 1, dbid)
  from gp_segment_configuration where role = 'p' and content = -1;

-- restart seg0 with 'fast' mode.
-1U: select pg_ctl((SELECT datadir from gp_segment_configuration c
  where c.role='p' and c.content=0), 'restart', 'fast');

-- run the query and fail as expected since the gangs are gone and will be
-- recreated in later query.
select * from t_ckpt;

-- the below queries also verify that the startup process on seg0 is not hanging.

-- finish the suspended 2pc query.
select gp_inject_fault('dtm_broadcast_prepare', 'reset', dbid)
  from gp_segment_configuration where role = 'p' and content = -1;
1<:

-- double confirm the assumption for the test:
--  1. tuples (2) and (1) are located on two segments (thus we are testing 2pc with prepared transaction).
--  2. there are tuples on the first segment (we have been testing on the first segment).
select gp_segment_id, * from t_ckpt;

-- cleanup
drop table t_ckpt;
!\retcode gpconfig -r gp_gang_creation_retry_count --skipvalidation;
!\retcode gpconfig -r gp_gang_creation_retry_timer --skipvalidation;
!\retcode gpstop -u;
