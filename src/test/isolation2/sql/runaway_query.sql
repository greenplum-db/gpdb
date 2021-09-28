CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
CREATE OR REPLACE LANGUAGE plpgsql;

CREATE TABLE runaway_query_test_table(a bigint NOT NULL) DISTRIBUTED BY (a);

-- Use error fault to simulate vmem protect error and force cancel query.
SELECT gp_inject_fault_infinite('gpdbwrappers_get_comparison_operator', 'error', 1);

-- Following query will trip the fault during ORCA optimization
EXPLAIN (COSTS OFF) SELECT a FROM runaway_query_test_table WHERE (a = ANY ((ARRAY[42])));

SELECT gp_inject_fault('all', 'reset', 1);
