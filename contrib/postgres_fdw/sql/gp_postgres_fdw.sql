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

CREATE FOREIGN TABLE gp_ft2 ( f1 int ) SERVER loopback OPTIONS (num_segments '3');

-- ===================================================================
-- validate parallel writes (mpp_execute set to all segments)
-- ===================================================================

EXPLAIN (VERBOSE, COSTS FALSE) SELECT * FROM gp_ft1;
EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";

\c
set search_path=postgres_fdw_gp;
alter server loopback options(add num_segments '4');
EXPLAIN (VERBOSE, COSTS FALSE) SELECT * FROM gp_ft1;
EXPLAIN ANALYZE SELECT * FROM gp_ft1;
EXPLAIN (VERBOSE, COSTS FALSE) SELECT count(*) FROM gp_ft1;
EXPLAIN (VERBOSE, COSTS FALSE) SELECT * FROM gp_ft1 t1 INNER JOIN gp_ft1 t2 ON t1.f1 = t2.f1 LIMIT 3;
EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";
alter server loopback options(set num_segments '2');
EXPLAIN (VERBOSE, COSTS FALSE) SELECT * FROM gp_ft1;
EXPLAIN (COSTS FALSE) INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
INSERT INTO gp_ft1 SELECT * FROM table_dist_rand;
SELECT * FROM postgres_fdw_gp."GP 1" ORDER BY f1;
TRUNCATE TABLE postgres_fdw_gp."GP 1";
alter server loopback options(drop num_segments);
\c
set search_path=postgres_fdw_gp;

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

-- compare difference plans among when mpp_execute set to 'all segments', 'coordinator' and 'any'
explain (costs false) update t1 set b = b + 1 where b in (select a from gp_all where gp_all.a > 10);
explain (costs false) update t1 set b = b + 1 where b in (select a from gp_any where gp_any.a > 10);
explain (costs false) update t1 set b = b + 1 where b in (select a from gp_coord where gp_coord.a > 10);

---
--- Test for #16376 of multi-level partition table with foreign table
---
CREATE TABLE sub_part (
                          a int,
                          b int,
                          c int)
    DISTRIBUTED BY (a)
partition by range(b) subpartition by list(c) 
 SUBPARTITION TEMPLATE 
  (
   SUBPARTITION one values (1),
   SUBPARTITION two values (2)
  )
(
   START (0) INCLUSIVE END (5) EXCLUSIVE EVERY (1)
);

-- Create foreign tables
CREATE FOREIGN TABLE sub_part_1_prt_1_2_prt_one_foreign (
    a int,
    b int,
    c int)
SERVER loopback;

CREATE FOREIGN TABLE sub_part_1_prt_1_2_prt_two_foreign (
    a int,
    b int,
    c int)
SERVER loopback;

-- change a sub partition's all leaf table to foreign table
ALTER TABLE sub_part_1_prt_1 EXCHANGE PARTITION for(1) WITH TABLE sub_part_1_prt_1_2_prt_one_foreign;
ALTER TABLE sub_part_1_prt_1 EXCHANGE PARTITION for(2) WITH TABLE sub_part_1_prt_1_2_prt_two_foreign;

-- explain with ORCA should fall back to planner, rather than raise ERROR
explain select * from sub_part;

--- Clean up
DROP TABLE sub_part;
DROP TABLE sub_part_1_prt_1_2_prt_one_foreign;
DROP TABLE sub_part_1_prt_1_2_prt_two_foreign;

-- GPDB #16219: validate scram-sha-256 in postgres_fdw
alter system set password_encryption = 'scram-sha-256';
-- add created user to pg_hba.conf
\! echo "host    all    u16219  0.0.0.0/0 scram-sha-256" >> $COORDINATOR_DATA_DIRECTORY/pg_hba.conf
\! echo "host    all    u16219   ::1/128  scram-sha-256" >> $COORDINATOR_DATA_DIRECTORY/pg_hba.conf
\! echo "local    all    u16219   scram-sha-256" >> $COORDINATOR_DATA_DIRECTORY/pg_hba.conf
select pg_reload_conf();
\c postgres
create user u16219 password '123456';

create database database_16219;
\c database_16219
create extension postgres_fdw;
grant usage on FOREIGN DATA WRAPPER postgres_fdw to public;

set role u16219;
create table t1 (a int, b int);
insert into t1 values(generate_series(1,10),generate_series(11,20));

DO $d$
    BEGIN
        EXECUTE $$CREATE SERVER database_16219 FOREIGN DATA WRAPPER postgres_fdw
            OPTIONS (dbname '$$||current_database()||$$',
                     port '$$||current_setting('port')||$$',
                     host 'localhost'
            )$$;
    END;
$d$;

CREATE USER MAPPING FOR CURRENT_USER SERVER database_16219
    OPTIONS (user 'u16219', password '123456');

CREATE FOREIGN TABLE f_t1(a int, b int) 
	server database_16219 options(schema_name 'public', table_name 't1');

select count(*) from f_t1;
DO $d$
    BEGIN
        EXECUTE $$ALTER SERVER database_16219
            OPTIONS (SET port '$$||current_setting('port')||$$')$$;
    END;
$d$;
select count(*) from f_t1;
\c postgres
drop database database_16219;
drop user u16219;
alter system reset password_encryption;
select pg_reload_conf();
