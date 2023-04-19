--
-- Test correlated subquery in subplan with motion chooses correct scan type
--
-- Given I have two distributed tables
create table choose_seqscan_t1(id1 int,id2 int);
create table choose_seqscan_t2(id1 int,id2 int);
-- and they have some data
insert into choose_seqscan_t1 select i+1,i from generate_series(1,50)i;
insert into choose_seqscan_t2 select i+1,i from generate_series(1,50)i;
-- and one of the tables has an index on a column which is not the distribution column
create index bidx2 on choose_seqscan_t2(id2);
-- and the statistics reflect the newly inserted data
analyze choose_seqscan_t1; analyze choose_seqscan_t2;
-- making an indexscan cheaper with this GUC is only necessary with this small dataset
-- if you insert more data, you can still ensure an indexscan is considered
set random_page_cost=1;
set seq_page_cost=5;
-- and I query the table with the index from inside a subquery which will be pulled up inside of a subquery that will stay a subplan
select (select id1 from (select * from choose_seqscan_t2) foo where id2=choose_seqscan_t1.id2) from choose_seqscan_t1 order by id1;
explain select (select id1 from (select * from choose_seqscan_t2) foo where id2=choose_seqscan_t1.id2) from choose_seqscan_t1;
-- then, a sequential scan is chosen because I need a motion to move choose_seqscan_t2

-- Index Scan can be used on quals that don't depend on the correlation vars, however.
select t1.id1, (select count(*) from choose_seqscan_t2 t2 where t2.id1 = t1.id1 and t2.id2 = 1) from choose_seqscan_t1 t1 where t1.id1 < 10;
explain select t1.id1, (select count(*) from choose_seqscan_t2 t2 where t2.id1 = t1.id1 and t2.id2 = 1) from choose_seqscan_t1 t1 where t1.id1 < 10;

-- Test using a join within the subplan. It could perhaps use an Nested Loop Join +
-- Index Scan to do the join, but at the moment, the planner doesn't consider distributing
-- the Function Scan.
select t1.id1, (select count(*) from generate_series(1,5) g, choose_seqscan_t2 t2 where t1.id1 = t2.id1 and t2.id2 = g) from choose_seqscan_t1 t1 where t1.id1 < 10;
explain select t1.id1, (select count(*) from generate_series(1,5) g, choose_seqscan_t2 t2 where t1.id1 = t2.id1 and t2.id2 = g) from choose_seqscan_t1 t1 where t1.id1 < 10;

-- Similar, but use a real table. One possible plan for the subplan here would be to do the join
-- first, and then filter the join result based on the correlation qual "t1.id1 = t2.id1". But
-- the planner isn't smart enough to generate that plan, currently.
create table choose_seqscan_t3(id1 int,id2 int);
create index bidx3 on choose_seqscan_t3(id1);
insert into choose_seqscan_t3 select i+1,i from generate_series(1,50)i;
analyze choose_seqscan_t3;
select t1.id1, (select count(*) from choose_seqscan_t3 t3, choose_seqscan_t2 t2 where t1.id1 = t2.id1 and t3.id1 = t2.id1) from choose_seqscan_t1 t1 where t1.id1 < 10;
explain select t1.id1, (select count(*) from choose_seqscan_t3 t3, choose_seqscan_t2 t2 where t1.id1 = t2.id1 and t3.id1 = t2.id1) from choose_seqscan_t1 t1 where t1.id1 < 10;


-- start_ignore
drop table if exists choose_seqscan_t1;
drop table if exists choose_seqscan_t2;
-- end_ignore

-- Given I have one replicated table
create table choose_indexscan_t1(id1 int, id2 int);
create table choose_indexscan_t2(id1 int, id2 int) distributed replicated;
-- and it has data
insert into choose_indexscan_t1 select i+1, i from generate_series(1,20)i;
insert into choose_indexscan_t2 select i+1, i from generate_series(1,100)i;
-- and the replicated table has an index on a column which is not the distribution key
create index choose_indexscan_t2_idx on choose_indexscan_t2(id2);
-- and the statistics reflect the newly inserted data
analyze choose_indexscan_t1; analyze choose_indexscan_t2;
-- making an indexscan cheaper with this GUC is only necessary with this small dataset
-- if you insert more data, you can still ensure an indexscan is considered
set random_page_cost=1;
-- and I query the table with the index from inside a subquery which will be pulled up inside of a subquery that will stay a subplan
select (select id1 from (select * from choose_indexscan_t2) foo where id2=choose_indexscan_t1.id2) from choose_indexscan_t1 order by id1;
explain select (select id1 from (select * from choose_indexscan_t2) foo where id2=choose_indexscan_t1.id2) from choose_indexscan_t1;
-- then an indexscan is chosen because it is correct to do this on a replicated table since no motion is required

-- Test that Motions are added when you mix replicated tables and catalog
-- tables in the same query. A replicated table is available on all segments,
-- but *not* on the QD node, so we need a motion for this, because the catalog
-- table is scanned in the QD. (Catalog tables are present with same contents
-- on all segments, too, so we could alternatively perform scan the catalog
-- table oon one of the segments.)
-- https://github.com/greenplum-db/gpdb/issues/8648
create table mytables (tablename text, explanation text) distributed replicated;
insert into mytables values ('pg_class', 'contains all relations');
create index on mytables(tablename);

select c.relname, (select explanation from mytables mt where mt.tablename=c.relname ) from pg_class c where relname = 'pg_class';
set enable_seqscan=off;
explain select c.relname, (select explanation from mytables mt where mt.tablename=c.relname ) from pg_class c where relname = 'pg_class';
select c.relname, (select explanation from mytables mt where mt.tablename=c.relname ) from pg_class c where relname = 'pg_class';
reset enable_seqscan;

-- start_ignore
drop table if exists mytables;
drop table if exists choose_indexscan_t1;
drop table if exists choose_indexscan_t2;
-- end_ignore
