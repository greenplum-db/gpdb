-- Test to validate GetSnapshotData()'s computation of globalXmin using
-- distributed snapshot. It mainly uses a old read-only transaction to help
-- create situation where globalXmin can be lower than distributed oldestXmin
-- when calling DistributedLog_AdvanceOldestXmin().

-- Setup
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
CREATE TABLE distributed_snapshot_test1 (a int);

1: BEGIN;
1: INSERT INTO distributed_snapshot_test1 values(1);
-- Read transaction which helps to get lower globalXmin for session 3. As this
-- will have MyProc->xmin set to transaction 1's xid.
2: BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
2: SELECT * from distributed_snapshot_test1;
-- Complete transaction 1, so that it no more appears in in-progress transaction
-- list for following transactions.
1: COMMIT;

-- Transaction to bump the latestCompletedXid
1: INSERT INTO distributed_snapshot_test1 values(1);

-- Hold after walking over ProcArray in GetSnpashotData(), right at start of
-- DistributedLog_AdvanceOldestXmin()
1: SELECT gp_inject_fault('distributedlog_advance_oldest_xmin', 'suspend',
   '', 'postgres', '', 1, -1, 5, dbid) from gp_segment_configuration
   where content = 0 and role = 'p';
3&:@db_name postgres: SELECT count(*) > 0 from gp_dist_random('gp_id');
1: SELECT gp_wait_until_triggered_fault('distributedlog_advance_oldest_xmin', 1, dbid)
   from gp_segment_configuration where content = 0 and role = 'p';
2: COMMIT;

-- Transaction used to bump the distributed oldestXmin
1: INSERT INTO distributed_snapshot_test1 values(1);
-- let session 3 now move forward to compute distributed oldest xmin
1: SELECT gp_inject_fault('distributedlog_advance_oldest_xmin', 'reset', dbid)
   from gp_segment_configuration where content = 0 and role = 'p';
3<:
