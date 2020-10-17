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
