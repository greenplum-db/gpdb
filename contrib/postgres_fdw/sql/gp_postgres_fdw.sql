-- ===================================================================
-- Greenplum-specific features for postgres_fdw
-- ===================================================================

-- ===================================================================
-- Create source tables and populate with data
-- ===================================================================
CREATE SCHEMA postgres_fdw_gp;
set search_path=postgres_fdw_gp;
CREATE EXTENSION IF NOT EXISTS postgres_fdw;

DO $d$
    BEGIN
        EXECUTE $$CREATE SERVER loopback FOREIGN DATA WRAPPER postgres_fdw
            OPTIONS (dbname '$$||current_database()||$$',
                     port '$$||current_setting('port')||$$'
            )$$;
    END;
$d$;

CREATE USER MAPPING IF NOT EXISTS FOR CURRENT_USER SERVER loopback;

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

CREATE TABLE postgres_fdw_gp."GP 1" (
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
) SERVER loopback OPTIONS (schema_name 'postgres_fdw_gp', table_name 'GP 1', mpp_execute 'all segments');

-- ===================================================================
-- validate parallel writes (mpp_execute set to all segments)
-- ===================================================================

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

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
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

-- ===================================================================
-- validate writes on any segment (mpp_execute set to any)
-- ===================================================================

ALTER FOREIGN TABLE gp_ft1 OPTIONS ( SET mpp_execute 'any' );

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

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
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

-- ===================================================================
-- validate writes on coordinator (mpp_execute set to coordinator)
-- ===================================================================

ALTER FOREIGN TABLE gp_ft1 OPTIONS ( SET mpp_execute 'coordinator' );

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
INSERT INTO gp_ft1 SELECT * FROM table_dist_repl;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_text;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
INSERT INTO gp_ft1 SELECT * FROM table_dist_int_text;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

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
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

-- Validate queries on different execution locations
create table t1(a int, b int);
create table t2(a int, b int);
create table t3(a int, b int);

CREATE FOREIGN TABLE gp_all (
	a int,
	b int
) SERVER loopback OPTIONS (schema_name 'postgres_fdw_gp', table_name 't1', mpp_execute 'all segments');

CREATE FOREIGN TABLE gp_any (
	a int,
	b int
) SERVER loopback OPTIONS (schema_name 'postgres_fdw_gp', table_name 't2', mpp_execute 'any');

CREATE FOREIGN TABLE gp_coord (
	a int,
	b int
) SERVER loopback OPTIONS (schema_name 'postgres_fdw_gp', table_name 't3', mpp_execute 'coordinator');

create table part_mixed (a int, b int) partition by range (b);
alter table part_mixed attach partition gp_all for values from (0) to (5);
alter table part_mixed attach partition gp_any for values from (5) to (10);
alter table part_mixed attach partition gp_coord for values from (10) to (15);
insert into part_mixed select i,i from generate_series(0,14)i;
analyze part_mixed;

explain select * from gp_all;
select * from gp_all;

explain select * from gp_any;
select * from gp_any;

explain select * from gp_coord;
select * from gp_coord;

-- validate partition with different execution locations
explain select * from part_mixed;
select * from part_mixed;

-- validate joins on different execution locations
create table non_part (a int, b int);
insert into non_part select i, i from generate_series(8,12)i;
analyze non_part;

explain select * from part_mixed join non_part on part_mixed.a=non_part.a;
select * from part_mixed join non_part on part_mixed.a=non_part.a;

explain select * from part_mixed left join non_part on part_mixed.a=non_part.a;
select * from part_mixed left join non_part on part_mixed.a=non_part.a;

explain select * from part_mixed right join non_part on part_mixed.a=non_part.a;
select * from part_mixed right join non_part on part_mixed.a=non_part.a;

-- validate join is on segments for distributed table
explain select * from gp_any, table_dist_int where gp_any.a=table_dist_int.f1;
select * from gp_any, table_dist_int where gp_any.a=table_dist_int.f1;

-- validate join is on segments for replicated table
explain select * from gp_any, table_dist_repl where gp_any.a=table_dist_repl.f1;
select * from gp_any, table_dist_repl where gp_any.a=table_dist_repl.f1;

create table part_mixed_dpe(a int, b int) partition by range(b);
alter table part_mixed detach partition gp_any;
alter table part_mixed_dpe attach partition gp_any for values from (5) to (10);
insert into part_mixed_dpe select 6,6 from generate_series(1,10);
analyze part_mixed_dpe;
explain select * from part_mixed_dpe, non_part where part_mixed_dpe.b=non_part.b;
select * from part_mixed_dpe, non_part where part_mixed_dpe.b=non_part.b;
