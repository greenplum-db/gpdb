-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
drop database if exists gpreindextest;
create database gpreindextest template template0;
\c gpreindextest;

--Heap tables with indexes
create table heap_drop (a int, b int, c int);
create index a1_idx on heap_drop(a);
create table heap_dont_drop (a int, b int, c int);
create index a2_idx on heap_dont_drop(a);
create table heap_add_index (a int, b int, c int);
create index a3_idx on heap_add_index(a);
