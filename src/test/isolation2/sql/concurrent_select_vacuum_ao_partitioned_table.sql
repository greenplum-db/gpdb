-- Ensure that a SELECT tx on a partition hierarchy comprised of AO tables, that
-- predates a concurrent vacuum doesn't error out, because the tuples have moved
-- to a new segfile and it doesn't know about it.

CREATE TABLE foo_part (a int, b int)
    WITH (appendonly=true) PARTITION BY range(a)
(start (1) inclusive end (2) inclusive every (1));

-- Insert 2 rows into the 1st partition on a single QE.
INSERT INTO foo_part SELECT 1, 1;
INSERT INTO foo_part SELECT 1, 2;
-- Delete 1 row from the first partition on a single QE.
DELETE FROM foo_part WHERE b = 1;

1: SET gp_keep_partition_children_locks TO on;
2: SELECT gp_inject_fault('locks_check_at_select_portal_create', 'suspend', c.dbid)
   FROM gp_segment_configuration c WHERE role='p' and content=-1;
1&: SELECT * FROM foo_part;
2: SELECT gp_wait_until_triggered_fault('locks_check_at_select_portal_create', 1, dbid)
   FROM gp_segment_configuration WHERE role='p' AND content = -1;

-- VACUUM shouldn't drop the files and SELECT should complete fine
VACUUM ANALYZE foo_part_1_prt_1;

2: SELECT gp_inject_fault('locks_check_at_select_portal_create', 'reset', c.dbid)
   FROM gp_segment_configuration c WHERE role='p' and content=-1;

1<:
DROP TABLE foo_part;

-- Test for Dynamic Bitmap Heap Scan
CREATE TABLE foo_part (a int, b int)
    WITH (appendonly=true) PARTITION BY range(a)
(start (1) inclusive end (2) inclusive every (1));

-- Insert 2 rows into the 1st partition on a single QE.
INSERT INTO foo_part SELECT 1, 1;
INSERT INTO foo_part SELECT 1, 2;
create index ind1 on foo_part using btree(a);
-- Delete 1 row from the first partition on a single QE.
DELETE FROM foo_part WHERE b = 1;


1: SET gp_keep_partition_children_locks TO on;
1: SET enable_seqscan to off;
2: SELECT gp_inject_fault('locks_check_at_select_portal_create', 'suspend', c.dbid)
   FROM gp_segment_configuration c WHERE role='p' and content=-1;
1&: SELECT * FROM foo_part where a>=1;
2: SELECT gp_wait_until_triggered_fault('locks_check_at_select_portal_create', 1, dbid)
   FROM gp_segment_configuration WHERE role='p' AND content = -1;

-- VACUUM shouldn't drop the files and SELECT should complete fine
VACUUM ANALYZE foo_part_1_prt_1;

2: SELECT gp_inject_fault('locks_check_at_select_portal_create', 'reset', c.dbid)
   FROM gp_segment_configuration c WHERE role='p' and content=-1;

1<:
DROP TABLE foo_part;

-- RTE inside CTE
CREATE TABLE foo_part (a int, b int)
    WITH (appendonly=true) PARTITION BY range(a)
(start (1) inclusive end (2) inclusive every (1));

-- Insert 2 rows into the 1st partition on a single QE.
INSERT INTO foo_part SELECT 1, 1;
INSERT INTO foo_part SELECT 1, 2;
-- Delete 1 row from the first partition on a single QE.
DELETE FROM foo_part WHERE b = 1;

1: SET gp_keep_partition_children_locks TO on;
2: SELECT gp_inject_fault('locks_check_at_select_portal_create', 'suspend', c.dbid)
   FROM gp_segment_configuration c WHERE role='p' and content=-1;
1&: WITH cte AS (SELECT * FROM foo_part) SELECT * FROM cte;
2: SELECT gp_wait_until_triggered_fault('locks_check_at_select_portal_create', 1, dbid)
   FROM gp_segment_configuration WHERE role='p' AND content = -1;

-- VACUUM shouldn't drop the files and SELECT should complete fine
VACUUM ANALYZE foo_part_1_prt_1;

2: SELECT gp_inject_fault('locks_check_at_select_portal_create', 'reset', c.dbid)
   FROM gp_segment_configuration c WHERE role='p' and content=-1;

1<:
DROP TABLE foo_part;

-- RTE inside SUBLINK
CREATE TABLE bar ( p int);
CREATE TABLE foo_part (a int, b int)
    WITH (appendonly=true) PARTITION BY range(a)
(start (1) inclusive end (2) inclusive every (1));
INSERT INTO bar SELECT 1;

-- Insert 2 rows into the 1st partition on a single QE.
INSERT INTO foo_part SELECT 1, 1;
INSERT INTO foo_part SELECT 1, 2;
-- Delete 1 row from the first partition on a single QE.
DELETE FROM foo_part WHERE b = 1;

1: SET gp_keep_partition_children_locks TO on;
2: SELECT gp_inject_fault('locks_check_at_select_portal_create', 'suspend', c.dbid)
   FROM gp_segment_configuration c WHERE role='p' and content=-1;
1&: SELECT bar.p FROM bar WHERE bar.p IN (SELECT a FROM foo_part);
2: SELECT gp_wait_until_triggered_fault('locks_check_at_select_portal_create', 1, dbid)
   FROM gp_segment_configuration WHERE role='p' AND content = -1;

-- VACUUM shouldn't drop the files and SELECT should complete fine
VACUUM ANALYZE foo_part_1_prt_1;

2: SELECT gp_inject_fault('locks_check_at_select_portal_create', 'reset', c.dbid)
   FROM gp_segment_configuration c WHERE role='p' and content=-1;

1<:
DROP TABLE foo_part;
DROP TABLE bar;

-- Testing multilevel partitions
CREATE TABLE foo_part(a int, b int, c int)
WITH (APPENDONLY = true)
DISTRIBUTED BY (a)
PARTITION BY RANGE (b)
SUBPARTITION BY RANGE (c)
SUBPARTITION TEMPLATE (
start (1) inclusive end (2) inclusive every (1))
( start (1) inclusive end (2) inclusive every (1));

INSERT INTO foo_part SELECT 1, 1, 1;
INSERT INTO foo_part SELECT 1, 1, 2;
DELETE FROM foo_part WHERE c = 1;

1: SET gp_keep_partition_children_locks TO on;
2: SELECT gp_inject_fault('locks_check_at_select_portal_create', 'suspend', c.dbid)
   FROM gp_segment_configuration c WHERE role='p' and content=-1;
1&: SELECT * from foo_part_1_prt_1;
2: SELECT gp_wait_until_triggered_fault('locks_check_at_select_portal_create', 1, dbid)
   FROM gp_segment_configuration WHERE role='p' AND content = -1;

-- VACUUM shouldn't drop the files and SELECT should complete fine
VACUUM ANALYZE foo_part_1_prt_1_2_prt_1;

2: SELECT gp_inject_fault('locks_check_at_select_portal_create', 'reset', c.dbid)
   FROM gp_segment_configuration c WHERE role='p' and content=-1;

1<:
DROP TABLE foo_part;