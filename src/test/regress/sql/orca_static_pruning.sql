CREATE SCHEMA orca_static_pruning;
SET search_path TO orca_static_pruning;

CREATE TABLE rp (a int, b int, c int) DISTRIBUTED BY (a) PARTITION BY RANGE (b);
CREATE TABLE rp0 PARTITION OF rp FOR VALUES FROM (MINVALUE) TO (10);
CREATE TABLE rp1 PARTITION OF rp FOR VALUES FROM (10) TO (20);
CREATE TABLE rp2 PARTITION OF rp FOR VALUES FROM (4200) TO (4203);

INSERT INTO rp VALUES (0, 0, 0), (11, 11, 0), (4201, 4201, 0);

SELECT $query$
SELECT *
FROM rp
WHERE b > 4200
$query$ AS qry \gset

SET optimizer_trace_fallback TO on;
EXPLAIN (COSTS OFF, VERBOSE)
:qry ;

:qry ;

SELECT $query$
SELECT *
FROM rp
WHERE b = 4201
$query$ AS qry \gset

EXPLAIN (COSTS OFF, VERBOSE)
:qry ;

:qry ;

SELECT $query$
SELECT *
FROM rp
WHERE b IN (4201, 4200)
$query$ AS qry \gset

EXPLAIN (COSTS OFF, VERBOSE)
:qry ;

:qry ;

RESET optimizer_trace_fallback;

CREATE TABLE lp (a int, b int) DISTRIBUTED BY (a) PARTITION BY LIST (b);
CREATE TABLE lp0 PARTITION OF lp FOR VALUES IN (0, 1);
CREATE TABLE lp1 PARTITION OF lp FOR VALUES IN (10, 11);
CREATE TABLE lp2 PARTITION OF lp FOR VALUES IN (42, 43);
INSERT INTO lp VALUES (0, 0), (10, 10), (42, 42);

SET optimizer_trace_fallback TO on;

SELECT $query$
SELECT *
FROM lp
WHERE b > 42
$query$ AS qry \gset

EXPLAIN (COSTS OFF, VERBOSE)
:qry ;

:qry ;

SELECT $query$
SELECT *
FROM lp
WHERE b = 42
$query$ AS qry \gset

EXPLAIN (COSTS OFF, VERBOSE)
:qry ;

:qry ;

RESET optimizer_trace_fallback;

CREATE TABLE hp (a int, b int) PARTITION BY HASH (b);
CREATE TABLE hp0 PARTITION OF hp FOR VALUES WITH (MODULUS 2, REMAINDER 0);
CREATE TABLE hp1 PARTITION OF hp FOR VALUES WITH (MODULUS 2, REMAINDER 1);
INSERT INTO hp VALUES (0, 1), (0, 3), (0, 4), (0, 42);

SET optimizer_trace_fallback TO on;
SELECT $query$
SELECT *
FROM hp
WHERE b = 42
$query$ AS qry \gset

EXPLAIN (COSTS OFF, VERBOSE)
:qry ;

:qry ;

RESET optimizer_trace_fallback;

CREATE TABLE rp_multi_inds (a int, b int, c int) DISTRIBUTED BY (a) PARTITION BY RANGE (b);
CREATE TABLE rp_multi_inds_part1 PARTITION OF rp_multi_inds FOR VALUES FROM (MINVALUE) TO (10);
CREATE TABLE rp_multi_inds_part2 PARTITION OF rp_multi_inds FOR VALUES FROM (10) TO (20);
CREATE TABLE rp_multi_inds_part3 PARTITION OF rp_multi_inds FOR VALUES FROM (4201) TO (4203);

INSERT INTO rp_multi_inds VALUES (0, 0, 0), (11, 11, 11), (4201, 4201, 4201);

-- Create an index only on the selected partition
CREATE INDEX other_idx ON rp_multi_inds_part2 USING btree(b);
-- Create indexes on root table
CREATE INDEX rp_btree_idx ON rp_multi_inds USING btree(c);
CREATE INDEX rp_bitmap_idx ON rp_multi_inds USING bitmap(b);

-- Expect a plan that only uses the two indexes inherited from root
SET optimizer_enable_dynamictablescan TO off;
SET optimizer_trace_fallback TO on;
EXPLAIN (COSTS OFF, VERBOSE) SELECT * FROM rp_multi_inds WHERE b = 11 AND (c = 11 OR c = 4201);
SELECT * FROM rp_multi_inds WHERE b = 11 AND (c = 11 OR c = 4201);

RESET optimizer_trace_fallback;
RESET optimizer_enable_dynamictablescan;

CREATE TABLE foo (a int, b int) DISTRIBUTED BY (a) PARTITION BY RANGE (b);
CREATE TABLE foo_part1 PARTITION OF foo FOR VALUES FROM (MINVALUE) TO (10);
CREATE TABLE foo_part2 PARTITION OF foo FOR VALUES FROM (10) TO (20);
CREATE TABLE foo_part3 PARTITION OF foo FOR VALUES FROM (4201) TO (4203);
CREATE INDEX foo_idx on foo(a);
CREATE TABLE bar (a int) DISTRIBUTED BY (a);
INSERT INTO foo VALUES (0, 0), (11, 11), (4201, 4201);
INSERT INTO bar VALUES (0), (11), (42);

SET optimizer_trace_fallback TO on;
-- Test ORCA index nested loop join has correct outer ref
-- Set below GUCs for planner just to keep parity
SET enable_hashjoin TO off;
SET enable_mergejoin TO off;
SET enable_nestloop TO on;

EXPLAIN (COSTS OFF, VERBOSE) SELECT * FROM foo JOIN bar on foo.a = bar.a AND foo.b = 11;
SELECT * FROM foo JOIN bar on foo.a = bar.a AND foo.b = 11;

RESET enable_hashjoin;
RESET enable_mergejoin;
RESET enable_nestloop;
