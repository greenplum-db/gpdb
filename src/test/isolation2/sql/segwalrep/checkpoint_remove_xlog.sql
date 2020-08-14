-- Test a bug that checkpoint removes xlog segment files which still
-- has orphaned prepared transactions. See below comments for details.

include: helpers/server_helpers.sql;

!\retcode gpconfig -c wal_keep_segments -v 0 --skipvalidation;
!\retcode gpstop -u;

create extension if not exists gp_inject_fault;
create table t_checkpoint1 (a int);
create table t_checkpoint2 (a int);

-- generate an temporarily orphaned prepared transaction.  we expect it to be
-- triggered twice since we'd generate two orphaned prepared transactions.
select gp_inject_fault('dtm_broadcast_prepare', 'suspend', '', '', '', 1, 2, 0, dbid)
  from gp_segment_configuration where role = 'p' and content = -1;
-- assume (2), (1) are on different segments and one tuple is on the first segment.
-- the test finally double-check that.
1&: insert into t_checkpoint1 values(2),(1);
select gp_wait_until_triggered_fault('dtm_broadcast_prepare', 1, dbid)
  from gp_segment_configuration where role = 'p' and content = -1;

-- trigger xlog file switch on the first segment. see below comment for explanation.
-- start_ignore
0U: select pg_switch_xlog();
-- end_ignore

-- generate another temporarily orphaned prepared transaction. the PREPARE
-- transaction xlog will be located on a different xlog segment file than the
-- one that has the previous orphaned prepared transaction. Previously there is
-- a bug: after crash recovery finishes, the startup process will create an
-- end-of-recovery checkpoint. the checkpoint will recycle/remove xlog files
-- according to orphaned prepared transaction LSNs, replication slot data, some
-- related GUC values, etc. The orphaned prepared transaction LSN data
-- (TwoPhaseState->prepXacts, etc) for checkpoint are populated in startup
-- process RecoverPreparedTransactions(), but the function is called after the
-- end-of-recovery checkpoint creation so xlog files with orphaned prepared
-- transactions could be recycled/removed. this might cause "requested WAL
-- segment pg_xlog/000000010000000000000009 has already been removed" kind of
-- error when bringing up the primary during single postgres running in
-- 'gprecoverseg -a -v' pg_rewind if failover happens).
-- As to why we need the new orphaned prepared transaction on another xlog
-- file?  If a xlog file was opened, even the file is unlinked, we could still
-- read the file with the file descriptor, so to reproduce this issue we need
-- PrescanPreparedTransactions(), which scans all xlog files that have prepared
-- transaction before the end-of-recovery creation, closes the opened file
-- descriptor of the xlog file that includes the first orphaned prepared
-- transation and thus later RecoverPreparedTransactions() will fail when
-- opening the missing xlog file that has the first orphaned prepared
-- transaction.  Refer xlogutils.c:XLogRead() for context.
2&: insert into t_checkpoint2 values(2),(1);
select gp_wait_until_triggered_fault('dtm_broadcast_prepare', 2, dbid)
  from gp_segment_configuration where role = 'p' and content = -1;

-- trigger xlog file switch on the first segment.
-- start_ignore
0U: select pg_switch_xlog();
-- end_ignore

-- issue a checkpoint since a new checkpoint depends on previous checkpoint.redo
-- for xlog file recycling/removing.
checkpoint;

-- shutdown primary and make sure the segment is down
-1U: select pg_ctl((SELECT datadir from gp_segment_configuration c
  where c.role='p' and c.content=0), 'stop', 'immediate');
select gp_request_fts_probe_scan();
select role, preferred_role from gp_segment_configuration where content = 0;

-- double confirm that promote succeeds.
-- also double confirm that
--  1. tuples (2) and (1) are located on two segments (thus we are testing 2pc with prepared transaction).
--  2. there are tuples on the first segment (we have been testing on the first segment).
insert into t_checkpoint1 values(2),(1);
select gp_segment_id, * from t_checkpoint1;

select gp_inject_fault('dtm_broadcast_prepare', 'reset', dbid)
  from gp_segment_configuration where role = 'p' and content = -1;
1<:
2<:

-- confirm the "orphaned" prepared trnasaction commits finally.
select * from t_checkpoint1;
select * from t_checkpoint2;

-- recovery the nodes. it should succeed without "requested WAL segment
-- pg_xlog/000000010000000000000009 has already been removed" kind of error.
!\retcode gprecoverseg -a -v;
select wait_until_segment_synchronized(0);

!\retcode gprecoverseg -ar;
select wait_until_segment_synchronized(0);

-- verify the first segment is recovered to the original state.
select role, preferred_role from gp_segment_configuration where content = 0;

-- cleanup
!\retcode gpconfig -r wal_keep_segments --skipvalidation;
!\retcode gpstop -u;
drop table t_checkpoint1;
drop table t_checkpoint2;
