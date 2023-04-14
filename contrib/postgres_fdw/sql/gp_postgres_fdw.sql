-- ===================================================================
-- Greenplum-specific features for postgres_fdw
-- ===================================================================

-- ===================================================================
-- Create source tables and populate with data
-- ===================================================================

CREATE TABLE table_dist_rand
(
	f1 int,
	f2 text,
	f3 text
) DISTRIBUTED RANDOMLY;

CREATE TABLE table_dist_repl
(
	f1 int,
	f2 text,
	f3 text
) DISTRIBUTED REPLICATED;

CREATE TABLE table_dist_int
(
	f1 int,
	f2 text,
	f3 text
) DISTRIBUTED BY (f1);

CREATE TABLE table_dist_text
(
	f1 int,
	f2 text,
	f3 text
) DISTRIBUTED BY (f2);

CREATE TABLE table_dist_int_text
(
	f1 int,
	f2 text,
	f3 text
) DISTRIBUTED BY (f1, f2);

INSERT INTO table_dist_rand
VALUES (1, 'a', 'aa'),
	   (2, 'b', 'bb'),
	   (3, 'c', 'cc'),
	   (4, 'd', 'dd'),
	   (5, 'e', 'ee'),
	   (6, 'f', 'ff'),
	   (7, 'g', 'gg'),
	   (8, 'h', 'hh'),
	   (9, 'i', 'ii'),
	   (10, 'j', 'jj'),
	   (11, 'k', 'kk'),
	   (12, 'l', 'll');

INSERT INTO table_dist_repl     SELECT * FROM table_dist_rand;
INSERT INTO table_dist_int      SELECT * FROM table_dist_rand;
INSERT INTO table_dist_text     SELECT * FROM table_dist_rand;
INSERT INTO table_dist_int_text SELECT * FROM table_dist_rand;

-- ===================================================================
-- create target table
-- ===================================================================

CREATE TABLE "S 1"."GP 1" (
	f1 int,
	f2 text,
	f3 text
);

-- ===================================================================
-- create foreign tables
-- ===================================================================

CREATE FOREIGN TABLE gp_ft1 (
	f1 int,
	f2 text,
	f3 text
) SERVER loopback OPTIONS (schema_name 'S 1', table_name 'GP 1', mpp_execute 'all segments');

-- ===================================================================
-- validate parallel writes (mpp_execute set to all segments)
-- ===================================================================

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1
SELECT id,
	   'AAA' || to_char(id, 'FM000'),
	   'BBB' || to_char(id, 'FM000')
FROM generate_series(1, 100) id;
INSERT INTO gp_ft1
SELECT id,
       'AAA' || to_char(id, 'FM000'),
       'BBB' || to_char(id, 'FM000')
FROM generate_series(1, 100) id;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

-- ===================================================================
-- validate writes on any segment (mpp_execute set to any)
-- ===================================================================

ALTER FOREIGN TABLE gp_ft1 OPTIONS ( SET mpp_execute 'any' );

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1
SELECT id,
	   'AAA' || to_char(id, 'FM000'),
	   'BBB' || to_char(id, 'FM000')
FROM generate_series(1, 100) id;
INSERT INTO gp_ft1
SELECT id,
       'AAA' || to_char(id, 'FM000'),
       'BBB' || to_char(id, 'FM000')
FROM generate_series(1, 100) id;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

-- ===================================================================
-- validate writes on coordinator (mpp_execute set to coordinator)
-- ===================================================================

ALTER FOREIGN TABLE gp_ft1 OPTIONS ( SET mpp_execute 'coordinator' );

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1
SELECT id,
	   'AAA' || to_char(id, 'FM000'),
	   'BBB' || to_char(id, 'FM000')
FROM generate_series(1, 100) id;
INSERT INTO gp_ft1
SELECT id,
       'AAA' || to_char(id, 'FM000'),
       'BBB' || to_char(id, 'FM000')
FROM generate_series(1, 100) id;
SELECT * FROM "S 1"."GP 1" ORDER BY f1;
TRUNCATE TABLE "S 1"."GP 1";