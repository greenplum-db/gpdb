-- Test Covering Indexes Feature
--
-- Purpose: Test that plans are optimal and correct using permutations of cover
--          indexes in varying scenarios. Correctness is determined by the
--          number of output rows from each query.
--          Does the plan use index-scan, index-only-scan, or seq-scan?
--
-- N.B. "VACUUM ANALYZE" is to update relallvisible used to determine cost of an
--      index-only scan.

-- start_matchsubs
-- m/Memory Usage: \d+\w?B/
-- s/Memory Usage: \d+\w?B/Memory Usage: ###B/
-- m/Memory: \d+kB/
-- s/Memory: \d+kB/Memory: ###kB/
-- m/Buckets: \d+/
-- s/Buckets: \d+/Buckets: ###/
-- m/Hash chain length \d+\.\d+ avg, \d+ max/
-- s/Hash chain length \d+\.\d+ avg, \d+ max/Hash chain length ###/
-- m/using \d+ of \d+ buckets/
-- s/using \d+ of \d+ buckets/using ## of ### buckets/
-- end_matchsubs

set optimizer_trace_fallback=on;
set enable_seqscan=off;


-- Basic scenario
CREATE TABLE test_basic_cover_index(a int, b int, c int);
CREATE INDEX i_test_basic_index ON test_basic_cover_index(a) INCLUDE (b);
INSERT INTO test_basic_cover_index SELECT i, i+i, i*i FROM generate_series(1, 100)i;
VACUUM ANALYZE test_basic_cover_index;

-- KEYS: [a]    INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM test_basic_cover_index WHERE a>42 AND b>42;


-- Test CTE with cover indexes
--
-- Check that CTE over scan with cover index and cover index over cte both work
-- KEYS: [a]    INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
WITH cte AS
(
    SELECT b FROM test_basic_cover_index WHERE a < 42
)
SELECT b FROM cte WHERE b%2=0;

-- KEYS: [a]    INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
WITH cte AS
(
    SELECT a, b FROM test_basic_cover_index
)
SELECT b FROM cte WHERE a<42;


-- Views over cover indexes
CREATE VIEW view_test_cover_indexes_with_filter AS
SELECT a, b FROM test_basic_cover_index WHERE a<42;
CREATE VIEW view_test_cover_indexes_without_filter AS
SELECT a, b FROM test_basic_cover_index;

-- KEYS: [a]    INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM view_test_cover_indexes_with_filter;

-- KEYS: [a]    INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM view_test_cover_indexes_without_filter WHERE a<42;


-- Various Column Types
--
-- Use different column types to check that the scan associates the correct
-- type to the correct column
CREATE TABLE test_various_col_types(inttype int, texttype text, decimaltype decimal(10,2));
INSERT INTO test_various_col_types SELECT i, 'texttype'||i, i FROM generate_series(1,9999) i;
CREATE INDEX i_test_various_col_types ON test_various_col_types(inttype) INCLUDE (texttype);
VACUUM ANALYZE test_various_col_types;

-- KEYS: [inttype] INCLUDED: [texttype]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF) SELECT texttype FROM test_various_col_types WHERE inttype<42;

DROP INDEX i_test_various_col_types;
CREATE INDEX i_test_various_col_types ON test_various_col_types(decimaltype) INCLUDE (inttype);
VACUUM ANALYZE test_various_col_types;

-- KEYS: [decimaltype] INCLUDED: [inttype]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF) SELECT decimaltype, inttype FROM test_various_col_types WHERE decimaltype<42;

ALTER TABLE test_various_col_types ADD COLUMN boxtype box;
DROP INDEX i_test_various_col_types;
CREATE INDEX i_test_various_col_types ON test_various_col_types(decimaltype) INCLUDE (boxtype);
VACUUM ANALYZE test_various_col_types;

-- KEYS: [decimaltype] INCLUDED: [boxtype]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF) SELECT decimaltype, boxtype FROM test_various_col_types WHERE decimaltype<42;


-- Test drop/add columns before and after creation of the index
--
-- Alter (add/drop) columns to check that the correct data is read from the
-- physical scan.
CREATE TABLE test_add_drop_columns(a int, aa int, b int, bb int, c int, d int);
ALTER TABLE test_add_drop_columns DROP COLUMN aa;
INSERT INTO test_add_drop_columns SELECT i, i+i, i*i, i*i*i, i+i+i FROM generate_series(1, 100)i;
ALTER TABLE test_add_drop_columns DROP COLUMN bb;
ALTER TABLE test_add_drop_columns ADD COLUMN e int;
CREATE INDEX i_test_add_drop_columns ON test_add_drop_columns(a, b) INCLUDE (c);
ALTER TABLE test_add_drop_columns ADD COLUMN f int;
VACUUM ANALYZE test_add_drop_columns;

-- KEYS: [a, b] INCLUDED: [c]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b FROM test_add_drop_columns WHERE a<42 AND b>42;

-- KEYS: [a, b] INCLUDED: [c]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b FROM test_add_drop_columns WHERE a<42 AND b>42 AND c>42;

-- KEYS: [a, b] INCLUDED: [c]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b, c, d, e FROM test_add_drop_columns WHERE a<42 AND b>42 AND c>42 AND e IS NULL;


-- Test various table types (e.g. AO/AOCO/replicated)
--
-- Check that different tables types (storage/distribution) leveage cover
-- indexes correctly.
CREATE TABLE test_replicated(a int, b int, c int) DISTRIBUTED REPLICATED;
CREATE INDEX i_test_replicated ON test_replicated(a) INCLUDE (b);
INSERT INTO test_replicated SELECT i, i+i, i*i FROM generate_series(1, 100)i;
VACUUM ANALYZE test_replicated;

-- KEYS: [a] INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM test_replicated WHERE a<42 AND b>42;

-- KEYS: [a] INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b, c FROM test_replicated WHERE a<42 AND b>42;

-- Expect Seq Scan because predicate "c" is not in KEYS
-- KEYS: [a] INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b, c FROM test_replicated WHERE c>42;

CREATE TABLE test_ao(a int, b int, c int) WITH (appendonly=true) DISTRIBUTED BY (a);
CREATE INDEX i_test_ao ON test_ao(a) INCLUDE (b);
INSERT INTO test_ao SELECT i, i+i, i*i FROM generate_series(1, 100)i;
VACUUM ANALYZE test_ao;

-- KEYS: [a] INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM test_ao WHERE a<42 AND b>42;

-- KEYS: [a] INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b, c FROM test_ao WHERE a<42 AND b>42;

-- KEYS: [a] INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b, c FROM test_ao WHERE c>42;


-- Test select best covering index
--
-- Check that the best cover index is chosen for a plan when multiple cover
-- indexes are available.
CREATE TABLE test_select_best_cover(a int, b int, c int);
CREATE INDEX i_test_select_best_cover_a_bc ON test_select_best_cover(a) INCLUDE (b, c);
CREATE INDEX i_test_select_best_cover_a_b ON test_select_best_cover(a) INCLUDE (b);
CREATE INDEX i_test_select_best_cover_a ON test_select_best_cover(a);
CREATE INDEX i_test_select_best_cover_ab ON test_select_best_cover(a, b);
CREATE INDEX i_test_select_best_cover_b_ac ON test_select_best_cover(b) INCLUDE (a, c);
CREATE INDEX i_test_select_best_cover_b_a ON test_select_best_cover(b) INCLUDE (a);
CREATE INDEX i_test_select_best_cover_b ON test_select_best_cover(b);
INSERT INTO test_select_best_cover SELECT i, i+i, i*i FROM generate_series(1, 100)i;
VACUUM ANALYZE test_select_best_cover;

-- KEYS: [a]    INCLUDED: []
-- KEYS: [a]    INCLUDED: [b]
-- KEYS: [a, b] INCLUDED: []
-- KEYS: [b]    INCLUDED: []
-- KEYS: [a]    INCLUDED: [b]
-- KEYS: [a]    INCLUDED: [b, c]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a FROM test_select_best_cover WHERE a>42;

-- KEYS: [a]    INCLUDED: []
-- KEYS: [a]    INCLUDED: [b]
-- KEYS: [a, b] INCLUDED: []
-- KEYS: [b]    INCLUDED: []
-- KEYS: [a]    INCLUDED: [b]
-- KEYS: [a]    INCLUDED: [b, c]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM test_select_best_cover WHERE b>42;

-- KEYS: [a]    INCLUDED: []
-- KEYS: [a]    INCLUDED: [b]
-- KEYS: [a, b] INCLUDED: []
-- KEYS: [b]    INCLUDED: []
-- KEYS: [a]    INCLUDED: [b]
-- KEYS: [a]    INCLUDED: [b, c]
-- ORCA_FEATURE_NOT_SUPPORTED: use i_test_select_best_cover_ab
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM test_select_best_cover WHERE a>42 AND b>42;

-- KEYS: [a]    INCLUDED: []
-- KEYS: [a]    INCLUDED: [b]
-- KEYS: [a, b] INCLUDED: []
-- KEYS: [b]    INCLUDED: []
-- KEYS: [a]    INCLUDED: [b]
-- KEYS: [a]    INCLUDED: [b, c]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM test_select_best_cover WHERE a>42 AND b>42 AND c>42;


-- Test DML operations
--
-- Check that cover indexes can be used with DML operations
CREATE TABLE test_dml_using_cover_index(a int, b int, c int);
CREATE INDEX i_test_dml_using_cover_index ON test_dml_using_cover_index(a) INCLUDE (b);
INSERT INTO test_dml_using_cover_index SELECT i, i+i, i*i FROM generate_series(1, 100)i;
VACUUM ANALYZE test_dml_using_cover_index;

-- KEYS: [a]    INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
INSERT INTO test_dml_using_cover_index (SELECT a, a, a FROM test_dml_using_cover_index WHERE a>42);


-- Test index scan over partition tables
--
-- Check that cover indexes can be used with partition tables. This includes
-- scenario when root/leaf partitions have different underlying physical format
-- (e.g. drop column / exchange partition or leaf partition has cover index not
-- defined on root).
CREATE TABLE test_cover_index_on_pt(a int, b int, c int)
DISTRIBUTED BY (a)
PARTITION BY RANGE (b)
(
    START (0) END (4) EVERY (1)
);
CREATE INDEX i_test_cover_index_scan_on_partition_table ON test_cover_index_on_pt(a) INCLUDE(b);
INSERT INTO test_cover_index_on_pt SELECT i+i, i%4 FROM generate_series(1, 10)i;
VACUUM ANALYZE test_cover_index_on_pt;

-- KEYS: [a]    INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b FROM test_cover_index_on_pt WHERE a<10;

-- KEYS: [a]    INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b, c FROM test_cover_index_on_pt WHERE a<10;

-- Expect Seq Scan because predicate "b" is not in KEYS
-- KEYS: [a]    INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b, c FROM test_cover_index_on_pt WHERE b<10;

-- Expect static eliminated partitions due to predicate on column 'b'
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b FROM test_cover_index_on_pt WHERE a<10 and b=2;

-- Expect both sides of join to perform index only scan
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT pt.a, pt.b FROM test_cover_index_on_pt AS pt JOIN test_basic_cover_index AS t ON pt.a=t.a WHERE pt.a<10 and pt.b=2;
SELECT pt.a, pt.b FROM test_cover_index_on_pt AS pt JOIN test_basic_cover_index AS t ON pt.a=t.a WHERE pt.a<10 and pt.b=2;

-- Expect both sides of join to perform index only scan
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT pt.a, pt.b FROM test_cover_index_on_pt AS pt LEFT JOIN test_basic_cover_index AS t ON pt.a=t.a WHERE pt.a<10 and pt.b=2;
SELECT pt.a, pt.b FROM test_cover_index_on_pt AS pt LEFT JOIN test_basic_cover_index AS t ON pt.a=t.a WHERE pt.a<10 and pt.b=2;

CREATE TABLE leaf_part(a int, b int, c int) DISTRIBUTED BY (a);
-- without explicit index declared on leaf_part
ALTER TABLE test_cover_index_on_pt EXCHANGE PARTITION FOR(2) WITH TABLE leaf_part;
INSERT INTO test_cover_index_on_pt VALUES (2, 2, 2);
VACUUM ANALYZE test_cover_index_on_pt;

-- KEYS: [a]    INCLUDED: [b]
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b FROM test_cover_index_on_pt WHERE a<10;

DROP INDEX i_test_cover_index_scan_on_partition_table;
-- with explicit index declared on leaf_part
CREATE INDEX i_test_cover_index_scan_on_partition_table ON leaf_part(a) INCLUDE(b);
VACUUM ANALYZE test_cover_index_on_pt;

-- ORCA_FEATURE_NOT_SUPPORTED: partial dynamic index scan
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b FROM test_cover_index_on_pt WHERE a<10;

-- Test mixed partitioned tables
--
-- AO partitioned table contains a non-AO leaf partition
CREATE TABLE ao_pt(a bigint) WITH (appendonly=true) PARTITION BY RANGE(a)
(
  START (1) END (11) WITH (tablename='ao_pt_1_prt_1'),
  START (11) END (21) WITH (tablename='ao_pt_1_prt_2', appendonly=false),
  START (21) END (31) WITH (tablename='ao_pt_1_prt_3')
);
INSERT INTO ao_pt SELECT i FROM generate_series(1,30)i;
CREATE INDEX idx_ao_pt_a ON ao_pt USING btree (a);
VACUUM ANALYZE ao_pt;

-- Allow dynamic index-only scan on mixed partitioned AO table
EXPLAIN SELECT a FROM ao_pt WHERE a=29;

-- imitate child partition has GPDB 6 version file via catalog
-- start_ignore
SET allow_system_table_mods=on;
UPDATE pg_appendonly SET version=1 WHERE relid='ao_pt_1_prt_3'::regclass;
RESET allow_system_table_mods;
-- end_ignore

-- Disallow if the table contains child partition with GPDB 6 version
EXPLAIN SELECT a FROM ao_pt WHERE a=29;
DROP TABLE ao_pt;

-- AO/CO partitioned table contains a non-AO leaf partition
CREATE TABLE aocs_pt(a bigint) WITH (appendonly=true, orientation=column) PARTITION BY RANGE(a)
(
  START (1) END (11) WITH (tablename='aocs_pt_1_prt_1'),
  START (11) END (21) WITH (tablename='aocs_pt_1_prt_2', appendonly=false),
  START (21) END (31) WITH (tablename='aocs_pt_1_prt_3')
);
INSERT INTO aocs_pt SELECT i FROM generate_series(1,30)i;
CREATE INDEX idx_aocs_pt_a ON aocs_pt USING btree (a);
VACUUM ANALYZE aocs_pt;

-- Allow dynamic index-only scan on mixed partitioned AO/CO table
EXPLAIN SELECT a FROM aocs_pt WHERE a=29;

-- imitate child partition has GPDB 6 version file via catalog
-- start_ignore
SET allow_system_table_mods=on;
UPDATE pg_appendonly SET version=1 WHERE relid='aocs_pt_1_prt_3'::regclass;
RESET allow_system_table_mods;
-- end_ignore

-- Disallow if the table contains child partition with GPDB 6 version
EXPLAIN SELECT a FROM aocs_pt WHERE a=29;
DROP TABLE aocs_pt;


-- Test various index types
--
-- Check that different index types can be used with cover indexes.
-- Note: brin, hash, and spgist do not suport included columns.
CREATE TABLE test_index_types(a box, b int, c int) DISTRIBUTED BY (b);
INSERT INTO test_index_types VALUES ('(2.0,2.0,0.0,0.0)', 2, 2);
INSERT INTO test_index_types VALUES ('(1.0,1.0,3.0,3.0)', 3, 3);
CREATE INDEX i_test_index_types ON test_index_types USING GIST (a) INCLUDE (b);
VACUUM ANALYZE test_index_types;

-- KEYS: [a]    INCLUDED: [b]
-- Check support index-only-scan on GIST indexes
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b FROM test_index_types WHERE a<@ box '(0,0,3,3)';

-- KEYS: [a]    INCLUDED: []
CREATE TABLE tsvector_table(t text, a tsvector) DISTRIBUTED BY (t);
INSERT INTO tsvector_table values('\n', '');
CREATE INDEX a_gist_index ON tsvector_table USING GIST (a);

-- Check index-only-scan is not used when index can not return column
SET optimizer_enable_indexscan=off;
SET optimizer_enable_bitmapscan=off;
SET optimizer_enable_tablescan=off;

-- expect fallback
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT count(*) FROM tsvector_table WHERE a @@ 'w:*|q:*';
SELECT count(*) FROM tsvector_table WHERE a @@ 'w:*|q:*';

RESET optimizer_enable_indexscan;
RESET optimizer_enable_bitmapscan;
RESET optimizer_enable_tablescan;



-- KEYS: [a]    INCLUDED: [b]
-- Check support dynamic-index-only-scan on GIST indexes
CREATE TABLE test_partition_table_with_gist_index(a int, b_box box)
DISTRIBUTED BY (a)
PARTITION BY RANGE (a)
(
    START (0) END (4) EVERY (1)
);
CREATE INDEX gist_index_on_column_a_box ON test_partition_table_with_gist_index USING GIST (b_box) INCLUDE (a);
INSERT INTO test_partition_table_with_gist_index VALUES (2, '(2.0,2.0,0.0,0.0)');
INSERT INTO test_partition_table_with_gist_index VALUES (3, '(1.0,1.0,3.0,3.0)');
VACUUM ANALYZE test_partition_table_with_gist_index;
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a, b_box FROM test_partition_table_with_gist_index WHERE b_box<@ box '(0,0,3,3)';


-- 8) Test partial indexes
--
-- Check that partial cover indexes may be used
CREATE TABLE test_partial_index(a int, b int, c int);
CREATE INDEX i_test_partial_index ON test_partial_index(a) INCLUDE (b) WHERE a<42;
INSERT INTO test_partial_index SELECT i, i+i, i*i FROM generate_series(1, 100)i;
VACUUM ANALYZE test_partial_index;

-- KEYS: [a]    INCLUDED: [b]
-- ORCA_FEATURE_NOT_SUPPORTED: support partial indexes
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM test_partial_index WHERE a>42 AND b>42;


-- Test backward index scan
--
-- Check that cover indexes may be used for backward index scan
CREATE TABLE test_backward_index_scan(a int, b int, c int) DISTRIBUTED BY (a);
CREATE INDEX i_test_backward_index_scan ON test_backward_index_scan(a) INCLUDE (b);
INSERT INTO test_backward_index_scan SELECT i, i+i, i*i FROM generate_series(1, 100)i;
VACUUM ANALYZE test_backward_index_scan;

-- KEYS: [a]    INCLUDED: [b]
-- ORCA_FEATURE_NOT_SUPPORTED enable backward index scan
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM test_backward_index_scan WHERE a>42 AND b>42 ORDER BY a DESC;


-- Test index expressions
--
-- Check that cover indexes may be used for index expressions
CREATE OR REPLACE FUNCTION add_one(integer)
RETURNS INTEGER
LANGUAGE 'plpgsql'
AS $$
BEGIN
    RETURN $1 + 1;
END;
$$;
CREATE TABLE test_index_expression_scan(a int, b int, c int) DISTRIBUTED BY (a);
CREATE INDEX i_test_index_expression_scan ON test_index_expression_scan(a) INCLUDE (b);
INSERT INTO test_index_expression_scan SELECT i, i+i, i*i FROM generate_series(1, 100)i;
VACUUM ANALYZE test_index_expression_scan;

-- KEYS: [a]    INCLUDED: [b]
-- ORCA_FEATURE_NOT_SUPPORTED enable index expression
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT add_one(b) FROM test_index_expression_scan WHERE add_one(a) < 42;


-- Test combined indexes
--
-- Check that combined indexes may be used.  (on OR conditions) https://www.postgresql.org/docs/current/indexes-bitmap-scans.html
CREATE TABLE test_combined_index_scan(a int, b int, c int) DISTRIBUTED BY (a);
CREATE INDEX i_test_combined_index_scan_a ON test_combined_index_scan(a) INCLUDE (b);
CREATE INDEX i_test_combined_index_scan_b ON test_combined_index_scan(b) INCLUDE (a);
INSERT INTO test_combined_index_scan SELECT i, i+i, i*i FROM generate_series(1, 100)i;
VACUUM ANALYZE test_combined_index_scan;

-- KEYS: [a]    INCLUDED: [b]
-- KEYS: [b]    INCLUDED: [a]
-- ORCA_FEATURE_NOT_SUPPORTED enable combined index
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM test_combined_index_scan WHERE a < 42 OR b < 42;

-- Test UNIQUE constraint with INCLUDE clause
--
CREATE TABLE test_unique_index_include(a int, b int, c int) DISTRIBUTED BY (a);
CREATE UNIQUE INDEX i_test_unique_index_include_a ON test_unique_index_include(a) INCLUDE (b);
INSERT INTO test_unique_index_include SELECT i, i+i, i*i FROM generate_series(1, 10)i;
INSERT INTO test_unique_index_include SELECT max(a)+1, max(b), max(c) FROM test_unique_index_include;
ALTER TABLE test_unique_index_include add UNIQUE (b) INCLUDE (c);
ALTER TABLE test_unique_index_include add UNIQUE (a) INCLUDE (c);
VACUUM ANALYZE test_unique_index_include;

-- KEYS: [a]    INCLUDED: [b]
-- index-only scan using i_test_unique_index_include_a
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b FROM test_unique_index_include WHERE a > 5;

-- KEYS: [a]    INCLUDED: [c]
-- index-only scan using test_unique_index_include_a_c_key
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT c FROM test_unique_index_include WHERE a > 5;

-- KEYS: [a]
-- index scan using i_test_unique_index_include_a
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT b, c FROM test_unique_index_include WHERE a > 5;

-- Test drop behavior
--
CREATE TABLE test_cover_index_drop(a int, b int, c int);
CREATE INDEX i_test_cover_index_drop ON test_cover_index_drop(a) INCLUDE (b);
INSERT INTO test_cover_index_drop SELECT i, i+i, i*i FROM generate_series(1, 10)i;
VACUUM ANALYZE test_cover_index_drop;

-- before dropping column b, index-only scan
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a FROM test_cover_index_drop WHERE a > 5;

ALTER TABLE test_cover_index_drop DROP column b;

-- after dropping column b, seqscan
-- Index has been dropped as a result of dropping the column.
EXPLAIN (ANALYZE, COSTS OFF, TIMING OFF, SUMMARY OFF)
SELECT a FROM test_cover_index_drop WHERE a > 5;

reset optimizer_trace_fallback;
reset enable_seqscan;
