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
