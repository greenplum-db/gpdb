CREATE EXTENSION IF NOT EXISTS gp_inject_fault;


-- Two concurrent transactions on partitioned table
--    1) dynamic scan
--    2) truncate
-- should not cause deadlock
CREATE TABLE pt(a int, b text) DISTRIBUTED BY (a) PARTITION BY range(a) (start (0) end(10) every(2));
INSERT INTO pt SELECT i%10, 'text'||i FROM generate_series(1, 10)i;
VACUUM ANALYZE pt;

1: EXPLAIN (costs off) SELECT a, b FROM pt WHERE a<4;
-- [ORCA] Fetch stats outside transaction so that we skip locking inside the transaction due to fetching stats.
1: SELECT a, b FROM pt WHERE a<4;
1: BEGIN;

-- Ensure the order transaction 1 and transaction 2 arrive on segments is
-- intertwined.

-- Transaction 1: suspended on segment 1, processed on all other segments
-- Transaction 2: blocked at coordinator until Transaction 1 releases the partition lock on coordinator
3: SELECT gp_inject_fault('exec_mpp_query_start', 'suspend', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
1&: SELECT a, b FROM pt WHERE a<4;
2: SELECT gp_wait_until_triggered_fault('exec_mpp_query_start', 1, dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
2&: TRUNCATE pt_1_prt_1;

3: SELECT gp_inject_fault('exec_mpp_query_start', 'resume', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
3: SELECT gp_inject_fault('exec_mpp_query_start', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;

-- All transactions should complete without deadlock
1<:
1: END;
2<:


-- Two concurrent transactions on partitioned table
--    1) dynamic index scan
--    2) truncate
-- should not cause deadlock
CREATE INDEX idx ON pt(a);
VACUUM ANALYZE pt;

1: EXPLAIN (costs off) SELECT a, b FROM pt WHERE a<4;
-- [ORCA] Fetch stats outside transaction so that we skip locking inside the transaction due to fetching stats.
1: SELECT a, b FROM pt WHERE a<4;
1: BEGIN;

-- Ensure the order transaction 1 and transaction 2 arrive on segments is
-- intertwined.

-- Transaction 1: suspended on segment 1, processed on all other segments
-- Transaction 2: blocked at coordinator until Transaction 1 releases the partition lock on coordinator
3: SELECT gp_inject_fault('exec_mpp_query_start', 'suspend', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
1&: SELECT a, b FROM pt WHERE a<4;
2: SELECT gp_wait_until_triggered_fault('exec_mpp_query_start', 1, dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
2&: TRUNCATE pt_1_prt_1;

3: SELECT gp_inject_fault('exec_mpp_query_start', 'resume', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
3: SELECT gp_inject_fault('exec_mpp_query_start', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;

-- All transactions should complete without deadlock
1<:
1: END;
2<:


-- Two concurrent transactions on partitioned table
--    1) dynamic index only scan
--    2) truncate
-- should not cause deadlock
VACUUM ANALYZE pt;

1: EXPLAIN (costs off) SELECT a FROM pt WHERE a<4;
-- [ORCA] Fetch stats outside transaction so that we skip locking inside the transaction due to fetching stats.
1: SELECT a FROM pt WHERE a<4;
1: BEGIN;

-- Ensure the order transaction 1 and transaction 2 arrive on segments is
-- intertwined.

-- Transaction 1: suspended on segment 1, processed on all other segments
-- Transaction 2: blocked at coordinator until Transaction 1 releases the partition lock on coordinator
3: SELECT gp_inject_fault('exec_mpp_query_start', 'suspend', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
1&: SELECT a FROM pt WHERE a<4;
2: SELECT gp_wait_until_triggered_fault('exec_mpp_query_start', 1, dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
2&: TRUNCATE pt_1_prt_1;

3: SELECT gp_inject_fault('exec_mpp_query_start', 'resume', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
3: SELECT gp_inject_fault('exec_mpp_query_start', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;

-- All transactions should complete without deadlock
1<:
1: END;
2<:
