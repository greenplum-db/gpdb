-- start_matchsubs
-- m/Executing SQL: select pg_catalog.gp_acquire_sample_rows\(\d+, \d+, \'t\'\);/
-- s/Executing SQL: select pg_catalog.gp_acquire_sample_rows\(\d+, \d+, \'t\'\);/Executing SQL: select pg_catalog.gp_acquire_sample_rows\(XXX, XXX, \'t\'\)/;
-- end_matchsubs
DROP DATABASE IF EXISTS testanalyze;
CREATE DATABASE testanalyze;
\c testanalyze
set client_min_messages='WARNING';
-- Case 1: Analyzing root table with GUC optimizer_analyze_root_partition and optimizer_analyze_midlevel_partition set off should only populate stats for leaf tables
set optimizer_analyze_root_partition=off;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze p3_sales;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;


-- Case 2: Analyzing a midlevel partition directly should give a WARNING message and should not update any stats for the table.
set optimizer_analyze_root_partition=off;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze p3_sales_1_prt_2;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 3: Analyzing leaf table directly should update the stats only for itself
set optimizer_analyze_root_partition=off;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze p3_sales_1_prt_2_2_prt_2_3_prt_usa;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 4: Analyzing the database with the GUC optimizer_analyze_root_partition and optimizer_analyze_midlevel_partition set to OFF should only update stats for leaf partition tables
set optimizer_analyze_root_partition=off;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 5: Vacuum analyzing the database should vacuum all the tables for p3_sales and should only update the stats for all leaf partitions of p3_sales
set optimizer_analyze_root_partition=off;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
vacuum analyze;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;
select count(*) from pg_stat_last_operation pgl, pg_class pgc where pgl.objid=pgc.oid and pgc.relname like 'p3_sales%';

-- Case 6: Analyzing root table with ROOTPARTITION keyword should only update the stats of the root table when the GUC optimizer_analyze_root_partition and optimizer_analyze_midlevel_partition are set off
set optimizer_analyze_root_partition=off;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze rootpartition p3_sales;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 7: Analyzing a midlevel partition should give a warning if using ROOTPARTITION keyword and should not update any stats.
set optimizer_analyze_root_partition=off;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze rootpartition p3_sales_1_prt_2;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 8: Analyzing a leaf partition should give a warning if using ROOTPARTITION keyword and should not update any stats.
set optimizer_analyze_root_partition=off;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze rootpartition p3_sales_1_prt_2_2_prt_2_3_prt_usa;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 9: Analyzing root table with GUC optimizer_analyze_root_partition set to ON and GUC optimizer_analyze_midlevel_partition set to off should update the leaf table and the root table stats.
set optimizer_analyze_root_partition=on;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze p3_sales;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 10: Analyzing root table using ROOTPARTITION keyword with GUC optimizer_analyze_root_partition set to ON and GUC optimizer_analyze_midlevel_partition set to off should update the root table stats only.
set optimizer_analyze_root_partition=on;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze rootpartition p3_sales;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 11: Analyzing root table with GUC optimizer_analyze_root_partition and optimizer_analyze_midlevel_partition set to ON should update the stats for root, midlevel and leaf partitions.
set optimizer_analyze_root_partition=on;
set optimizer_analyze_midlevel_partition=on;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze p3_sales;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 12: Analyzing root table using ROOTPARTITION keyword with GUC optimizer_analyze_root_partition and optimizer_analyze_midlevel_partition set to ON should only update the stats for root partition.
set optimizer_analyze_root_partition=on;
set optimizer_analyze_midlevel_partition=on;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze rootpartition p3_sales;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 13: Analyzing root table using ROOTPARTITION keyword with GUC optimizer_analyze_root_partition and optimizer_analyze_midlevel_partition set to OFF should update the stats for root partition only.
set optimizer_analyze_root_partition=on;
set optimizer_analyze_midlevel_partition=off;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze rootpartition p3_sales;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 14: Analyzing root table with GUC optimizer_analyze_root_partition set to OFF and optimizer_analyze_midlevel_partition set to On should update the stats for midlevel and leaf partition only.
set optimizer_analyze_root_partition=off;
set optimizer_analyze_midlevel_partition=on;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze p3_sales;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- Case 15: Analyzing root table using ROOTPARTITION keyword with GUC optimizer_analyze_root_partition set to OFF and optimizer_analyze_midlevel_partition set to ON should only update the stats for root only.
set optimizer_analyze_root_partition=off;
set optimizer_analyze_midlevel_partition=on;
DROP TABLE if exists p3_sales;
CREATE TABLE p3_sales (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY RANGE (month)
       SUBPARTITION TEMPLATE (
        START (1) END (2) EVERY (1),
        DEFAULT SUBPARTITION other_months )
           SUBPARTITION BY LIST (region)
             SUBPARTITION TEMPLATE (
               SUBPARTITION usa VALUES ('usa'),
               DEFAULT SUBPARTITION other_regions )
( START (2002) END (2003) EVERY (1),
  DEFAULT PARTITION outlying_years );
insert into p3_sales values (1, 2002, 1, 20, 'usa');
insert into p3_sales values (1, 2002, 1, 20, 'usa');
analyze rootpartition p3_sales;
select relname, reltuples, relpages from pg_class where relname like 'p3_sales%' order by relname;
select * from pg_stats where tablename like 'p3_sales%' order by tablename, attname;

-- start_ignore
DROP TABLE IF EXISTS p3_sales;
-- end_ignore

--
-- Test statistics collection on very large datums. In the current implementation,
-- they are left out of the sample, to avoid running out of memory for the main relation
-- statistics. In case of indexes on the relation, large datums are masked as NULLs in the sample
-- and are evaluated as NULL in index stats collection.
-- Expression / partial indexes are not commonly used, and its rare to have them on wide columns, so the
-- effect of considering them as NULL is minimal.
--
CREATE TABLE foo_stats (a text, b bytea, c varchar, d int) DISTRIBUTED RANDOMLY;
CREATE INDEX expression_idx_foo_stats ON foo_stats (upper(a));
INSERT INTO foo_stats values ('aaa', 'bbbbb', 'cccc', 2);
INSERT INTO foo_stats values ('aaa', 'bbbbb', 'cccc', 2);
-- Insert large datum values
INSERT INTO foo_stats values (repeat('a', 3000), 'bbbbb2', 'cccc2', 3);
INSERT INTO foo_stats values (repeat('a', 3000), 'bbbbb2', 'cccc2', 3);
ANALYZE foo_stats;
SELECT schemaname, tablename, attname, null_frac, avg_width, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename='foo_stats' ORDER BY attname;
SELECT schemaname, tablename, attname, null_frac, avg_width, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename='expression_idx_foo_stats' ORDER BY attname;
DROP TABLE IF EXISTS foo_stats;

-- Test the case that every value in a column is "very large".
CREATE TABLE foo_stats (a text, b bytea, c varchar, d int) DISTRIBUTED RANDOMLY;
alter table foo  alter column t set storage external;
INSERT INTO foo_stats values (repeat('a', 100000), 'bbbbb2', 'cccc2', 3);
INSERT INTO foo_stats values (repeat('b', 100000), 'bbbbb2', 'cccc2', 3);
ANALYZE foo_stats;
SELECT schemaname, tablename, attname, null_frac, avg_width, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename='foo_stats' ORDER BY attname;
DROP TABLE IF EXISTS foo_stats;


--
-- Test statistics collection with a "partially distributed" table. That is, with a table
-- that has a smaller 'numsegments' in the distribution policy than the segment count
-- of the cluster.
--
set allow_system_table_mods=true;

create table twoseg_table(a int, b int, c int) distributed by (a);
update gp_distribution_policy set numsegments=2 where localoid='twoseg_table'::regclass;
insert into twoseg_table select i, i % 10, 0 from generate_series(1, 50) I;
analyze twoseg_table;

select relname, reltuples, relpages from pg_class where relname ='twoseg_table' order by relname;
select attname, null_frac, avg_width, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename='twoseg_table' ORDER BY attname;

drop table twoseg_table;

--
-- Test statistics collection on a replicated table.
--
create table rep_table(a int, b int, c int) distributed replicated;
insert into rep_table select i, i % 10, 0 from generate_series(1, 50) I;
analyze rep_table;

select relname, reltuples, relpages from pg_class where relname ='rep_table' order by relname;
select attname, null_frac, avg_width, n_distinct, most_common_vals, most_common_freqs, histogram_bounds FROM pg_stats WHERE tablename='rep_table' ORDER BY attname;

drop table rep_table;


--
-- Test relpages collection for AO tables.
--

-- use a lower target, so that the whole table doesn't fit in the sample.
set default_statistics_target=10;

create table ao_analyze_test (i int4) with (appendonly=true);
insert into ao_analyze_test select g from generate_series(1, 100000) g;
create index ao_analyze_test_idx on ao_analyze_test (i);
analyze ao_analyze_test;
select relname, reltuples from pg_class where relname like 'ao_analyze_test%' order by relname;

-- and same for AOCS
create table aocs_analyze_test (i int4) with (appendonly=true, orientation=column);
insert into aocs_analyze_test select g from generate_series(1, 100000) g;
create index aocs_analyze_test_idx on aocs_analyze_test (i);
analyze aocs_analyze_test;
select relname, reltuples from pg_class where relname like 'aocs_analyze_test%' order by relname;

reset default_statistics_target;

-- Test column name called totalrows
create table test_tr (totalrows int4);
analyze test_tr;
drop table test_tr;

--
-- Test with both a dropped column and an oversized column
-- (github issue https://github.com/greenplum-db/gpdb/issues/9503)
--
create table analyze_dropped_col (a text, b text, c text, d text);
insert into analyze_dropped_col values('a','bbb', repeat('x', 5000), 'dddd');
alter table analyze_dropped_col drop column b;
analyze analyze_dropped_col;
select attname, null_frac, avg_width, n_distinct from pg_stats where tablename ='analyze_dropped_col';
-- Test analyze without USAGE privilege on schema
create schema test_ns;
revoke all on schema test_ns from public;
create role nsuser1;
grant create on schema test_ns to nsuser1;
set search_path to 'test_ns';
create extension citext;
create table testid (id int , test citext);
alter table testid owner to nsuser1;
analyze testid;
drop table testid;
drop extension citext;
drop schema test_ns;
drop role nsuser1;
set search_path to default;

--
-- Test analyze on inherited table.
-- We used to have a bug for acquiring sample rows on QE. It always return
-- rows for all inherited tables even the QD only wants samples for parent table's.
--
CREATE TABLE ana_parent (aa int);
CREATE TABLE ana_c1 (bb text) INHERITS (ana_parent);
CREATE TABLE ana_c2 (cc text) INHERITS (ana_c1);
INSERT INTO ana_c1 SELECT i, 'bb' FROM generate_series(1, 10) AS i;
INSERT INTO ana_c2 SELECT i, 'bb', 'cc' FROM generate_series(10, 20) AS i;
ANALYZE ana_parent;
ANALYZE ana_c1;
ANALYZE ana_c2;

-- Check pg_class entry
SELECT relpages, reltuples FROM pg_class WHERE relname = 'ana_parent';
SELECT relpages, reltuples FROM pg_class WHERE relname = 'ana_c1';
SELECT relpages, reltuples FROM pg_class WHERE relname = 'ana_c2';

-- Check pg_stats entries
SELECT * FROM pg_stats WHERE tablename = 'ana_parent';
SELECT * FROM pg_stats WHERE tablename = 'ana_c1';
SELECT * FROM pg_stats WHERE tablename = 'ana_c2';

-- Analyzing root table using ROOTPARTITION keyword on a column data type with no equality operator.
create table no_eqop (a int, b int, c xml) partition by range(b) (start(1) end (6) every (3));
insert into no_eqop select i, i % 5 + 1, '<foo>bar</foo>'::xml from generate_series(1, 1000)i;
insert into no_eqop select i, i % 5 + 1, NULL from generate_series(1, 1000)i;
insert into no_eqop select NULL, i % 5 + 1, '<foo>bar</foo>'::xml from generate_series(1, 1000)i;
analyze verbose rootpartition no_eqop(c);
select * from pg_stats where tablename = 'no_eqop';
analyze no_eqop(c);
-- Simply merges leaf stats. gp_acquire_sample_rows() is not executed
analyze verbose rootpartition no_eqop(c);
select * from pg_stats where tablename = 'no_eqop';
-- Issue 14644 keep catalog inconsistency of relhassubclass after analyze
CREATE TYPE test_type_14644 AS (a int, b text);
CREATE TABLE test_tb_14644 OF test_type_14644;
CREATE TABLE test_tb_14644_subclass () INHERITS (test_tb_14644);
DROP TABLE test_tb_14644_subclass;
select relhassubclass from pg_class where relname = 'test_tb_14644';
select relhassubclass from gp_dist_random('pg_class') where relname = 'test_tb_14644';
ANALYZE;
select relhassubclass from pg_class where relname = 'test_tb_14644';
select relhassubclass from gp_dist_random('pg_class') where relname = 'test_tb_14644';
