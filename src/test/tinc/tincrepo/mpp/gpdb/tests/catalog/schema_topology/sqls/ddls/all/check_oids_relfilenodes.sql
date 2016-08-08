-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description checks the oids and relfilenodes

--CHANGING OIDS and RELFILENODES (OS FILES)
create database check_oid_relfilenode_db;
\c check_oid_relfilenode_db

--CLUSTER TABLE: reorders the entire table by b-tree index and rebuilds the index

create table table2 (col1 int,col2 int) distributed randomly;

insert into table2 values (generate_series(1,100),generate_series(1,100));

create index clusterindex on table2(col1);
-- start_ignore
select oid,relname, relfilenode from pg_class where relname = 'table2';
-- end_ignore
CLUSTER clusterindex on table2;
-- start_ignore
select oid, relname, relfilenode from pg_class where relname = 'table2';
-- end_ignore
--RE-INDEXING: changes the relfilenode of the indexes

create table bm_test (i int, j int);
insert into bm_test values (0, 0), (0, 0), (0, 1), (1,0), (1,0), (1,1);
create index bm_test_j on bm_test using bitmap(j);
delete from bm_test where j =1;
insert into bm_test values (0, 0), (1,0);
insert into bm_test values (generate_series(2,100),generate_series(2,100));

-- start_ignore
select relname, relfilenode from pg_class where relname = 'bm_test';
-- end_ignore
REINDEX index "public"."bm_test_j";
-- start_ignore
select relname, relfilenode from pg_class where relname = 'bm_test';
-- end_ignore

--The relfilenode should stay the same before and after the delete from tablename

create table foo (a int, b text) distributed randomly;
insert into foo values (1, '1_one'), (2, '2_two');
insert into foo select i,i||'_'||repeat('text',100) from generate_series(3,100)i;
-- start_ignore
select relname, relfilenode from pg_class where relname = 'foo';
-- end_ignore
delete from foo where a = 1;
-- start_ignore
select relname, relfilenode from pg_class where relname = 'foo';
-- end_ignore
--the relfilenode should have stayed the same.
delete from foo;
-- start_ignore
select relname, relfilenode from pg_class where relname = 'foo';
-- end_ignore
--the relfilenode should still have stayed the same.
