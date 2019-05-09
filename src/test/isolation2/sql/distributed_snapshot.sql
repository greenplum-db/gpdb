-- Distributed snapshot tests

-- Scenario1: Test to validate GetSnapshotData()'s computation of globalXmin using
-- distributed snapshot. It mainly uses a old read-only transaction to help
-- create situation where globalXmin can be lower than distributed oldestXmin
-- when calling DistributedLog_AdvanceOldestXmin().

-- Setup
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

-- Scenario2: This scenario tests the boundary condition for Xmax in distributed snapshot

-- Setup
CREATE TABLE distributed_snapshot_test2 (a int);

-- start transaction assigns distributed xid.
1: BEGIN ISOLATION LEVEL REPEATABLE READ;
-- this sets latestCompletedXid
2: INSERT INTO distributed_snapshot_test2 VALUES(1);
-- here, take distributed snapshot
1: SELECT 123 AS "establish snapshot";
2: INSERT INTO distributed_snapshot_test2 VALUES(2);
-- expected to see just VALUES(1)
1: SELECT * FROM distributed_snapshot_test2;
