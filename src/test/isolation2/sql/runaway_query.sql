CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
CREATE OR REPLACE LANGUAGE plpgsql;

CREATE TABLE runaway_query_test_table(a bigint NOT NULL) DISTRIBUTED BY (a);

-- Use vmem_protect_limit fault to simulate vmem protect error and force cancel query.
SELECT gp_inject_fault_infinite('gpdbwrappers_get_comparison_operator', 'vmem_protect_limit', 1);

-- Run query that will trip the vmem protect limit fault.
EXPLAIN (COSTS OFF) SELECT a FROM runaway_query_test_table WHERE (a = ANY ((ARRAY[42])));

-- start_ignore
SELECT gp_inject_fault('all', 'reset', dbid) FROM gp_segment_configuration;
-- end_ignore
