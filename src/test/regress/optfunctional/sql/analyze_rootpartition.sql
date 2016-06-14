
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema analyze_rootpartition;
set search_path to analyze_rootpartition;
drop table if exists t1 cascade;
set gp_create_table_random_default_distribution=off;

create table t1(c1 int);
insert into t1 values(1);
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: sql/analyze_rootpartition.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop table if exists arp_test;
drop table if exists arp_test2;
set gp_create_table_random_default_distribution=off;

create table arp_test(a int, b int) partition by range(b) (start(1) end(10) every(3));
insert into arp_test select i, i%9+1 from generate_series(1,1000) i;

create table arp_test2(a int, b int) partition by range(a) (start(1) end(10) every(3));
insert into arp_test2 select i%9+1, i from generate_series(1,1000) i;
-- end_ignore

-- @author shene
-- @created 2014-05-02 12:00:00
-- @product_version gpdb: [4.3-]
-- @db_name analyze_db
-- @tags analyze ORCA
-- @optimizer_mode both
-- @description Test 'ANALYZE ROOTPARTITION' functionality which only analyzes the root table of a partition table
select * from gp_toolkit.gp_stats_missing where smitable like 'arp_%' order by smitable;

analyze rootpartition arp_test;
select * from gp_toolkit.gp_stats_missing where smitable like 'arp_%' order by smitable;

analyze rootpartition all;
select * from gp_toolkit.gp_stats_missing where smitable like 'arp_%' order by smitable;

analyze rootpartition arp_test(a);

-- negative tests below

-- to analyze all the root parts in the database, 'ALL' is required after rootpartition
analyze rootpartition;
-- if a table is specified, it must be a root part
analyze rootpartition arp_test_1_prt_1;
-- rootpartition cannot be specified with VACUUM
vacuum analyze rootpartition arp_test;
vacuum analyze rootpartition all;
vacuum rootpartition arp_test;

drop table arp_test;
drop table arp_test2;

-- when there are no partition tables in the database, analyze rootpartition should give a warning
analyze rootpartition all;



-- ----------------------------------------------------------------------
-- Test: sql/analyze_rootpartition_correctness.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop table if exists arp_test3;
set gp_create_table_random_default_distribution=off;

create table arp_test3(a int, b int) partition by range(a) (start(1) end(10) every(3));
insert into arp_test3 select i%4+1, i from generate_series(1,1000) i;
insert into arp_test3 select 2, i from generate_series(1001,1050) i;
insert into arp_test3 select 3, i from generate_series(1051,1225) i;
insert into arp_test3 select 4, i from generate_series(1226,1600) i;
-- end_ignore

-- @author raghav
-- @created 2014-06-27 12:00:00
-- @product_version gpdb: [4.3-],hawq: [1.2.1.0-]
-- @db_name analyze_db
-- @tags analyze ORCA
-- @optimizer_mode both
-- @description Test 'ANALYZE ROOTPARTITION' correctness

-- check if analyze and analyze rootpartition generates same statistics
analyze arp_test3;

select * from pg_stats where tablename like 'arp_test3%' order by tablename, attname;

set allow_system_table_mods="DML";

delete from pg_statistic where starelid='arp_test3'::regclass;

select * from pg_stats where tablename like 'arp_test3%' order by tablename, attname;

analyze rootpartition arp_test3;

select * from pg_stats where tablename like 'arp_test3%' order by tablename, attname;

drop table arp_test3;


-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema analyze_rootpartition cascade;
-- end_ignore
