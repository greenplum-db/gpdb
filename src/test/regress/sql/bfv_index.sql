-- tests index filter with outer refs
drop table if exists bfv_tab1;

CREATE TABLE bfv_tab1 (
	unique1		int4,
	unique2		int4,
	two			int4,
	four		int4,
	ten			int4,
	twenty		int4,
	hundred		int4,
	thousand	int4,
	twothousand	int4,
	fivethous	int4,
	tenthous	int4,
	odd			int4,
	even		int4,
	stringu1	name,
	stringu2	name,
	string4		name
) distributed by (unique1);

create index bfv_tab1_idx1 on bfv_tab1 using btree(unique1);

explain select * from bfv_tab1, (values(147, 'RFAAAA'), (931, 'VJAAAA')) as v (i, j)
    WHERE bfv_tab1.unique1 = v.i and bfv_tab1.stringu1 = v.j;

set gp_enable_relsize_collection=on;

explain select * from bfv_tab1, (values(147, 'RFAAAA'), (931, 'VJAAAA')) as v (i, j)
    WHERE bfv_tab1.unique1 = v.i and bfv_tab1.stringu1 = v.j;

reset gp_enable_relsize_collection;

--start_ignore
DROP TABLE IF EXISTS bfv_tab2_facttable1;
DROP TABLE IF EXISTS bfv_tab2_dimdate;
DROP TABLE IF EXISTS bfv_tab2_dimtabl1;
--end_ignore

-- Bug-fix verification for MPP-25537: PANIC when bitmap index used in ORCA select
CREATE TABLE bfv_tab2_facttable1 (
col1 integer,
wk_id smallint,
id integer
)
with (appendonly=true, orientation=column, compresstype=zlib, compresslevel=5)
partition by range (wk_id) (
start (1::smallint) END (20::smallint) inclusive every (1),
default partition dflt
)
;

insert into bfv_tab2_facttable1 select col1, col1, col1 from (select generate_series(1,20) col1)a;

CREATE TABLE bfv_tab2_dimdate (
wk_id smallint,
col2 date
)
;

insert into bfv_tab2_dimdate select col1, current_date - col1 from (select generate_series(1,20,2) col1)a;

CREATE TABLE bfv_tab2_dimtabl1 (
id integer,
col2 integer
)
;

insert into bfv_tab2_dimtabl1 select col1, col1 from (select generate_series(1,20,3) col1)a;

CREATE INDEX idx_bfv_tab2_facttable1 on bfv_tab2_facttable1 (id); 

--start_ignore
set optimizer_analyze_root_partition to on;
--end_ignore

ANALYZE bfv_tab2_facttable1;
ANALYZE bfv_tab2_dimdate;
ANALYZE bfv_tab2_dimtabl1;

SELECT count(*) 
FROM bfv_tab2_facttable1 ft, bfv_tab2_dimdate dt, bfv_tab2_dimtabl1 dt1
WHERE ft.wk_id = dt.wk_id
AND ft.id = dt1.id;

explain SELECT count(*) 
FROM bfv_tab2_facttable1 ft, bfv_tab2_dimdate dt, bfv_tab2_dimtabl1 dt1
WHERE ft.wk_id = dt.wk_id
AND ft.id = dt1.id;

-- experimental cost model guc generates bitmap scan
set optimizer_cost_model=experimental;
explain SELECT count(*)
FROM bfv_tab2_facttable1 ft, bfv_tab2_dimdate dt, bfv_tab2_dimtabl1 dt1
WHERE ft.wk_id = dt.wk_id
AND ft.id = dt1.id;

reset optimizer_cost_model;
-- start_ignore
create language plpythonu;
-- end_ignore

create or replace function count_index_scans(explain_query text) returns int as
$$
rv = plpy.execute(explain_query)
search_text = 'Index Scan'
result = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = result+1
return result
$$
language plpythonu;

DROP TABLE bfv_tab1;
DROP TABLE bfv_tab2_facttable1;
DROP TABLE bfv_tab2_dimdate;
DROP TABLE bfv_tab2_dimtabl1;

-- pick index scan when query has a relabel on the index key: non partitioned tables

set enable_seqscan = off;

-- start_ignore
drop table if exists Tab23383;
-- end_ignore

create table Tab23383(a int, b varchar(20));
insert into Tab23383 select g,g from generate_series(1,1000) g;
create index Tab23383_b on Tab23383(b);

-- start_ignore
select disable_xform('CXformGet2TableScan');
-- end_ignore

select count_index_scans('explain select * from Tab23383 where b=''1'';');
select * from Tab23383 where b='1';

select count_index_scans('explain select * from Tab23383 where ''1''=b;');
select * from Tab23383 where '1'=b;

select count_index_scans('explain select * from Tab23383 where ''2''> b order by a limit 10;');
select * from Tab23383 where '2'> b order by a limit 10;

select count_index_scans('explain select * from Tab23383 where b between ''1'' and ''2'' order by a limit 10;');
select * from Tab23383 where b between '1' and '2' order by a limit 10;

-- predicates on both index and non-index key
select count_index_scans('explain select * from Tab23383 where b=''1'' and a=''1'';');
select * from Tab23383 where b='1' and a='1';

--negative tests: no index scan plan possible, fall back to planner
select count_index_scans('explain select * from Tab23383 where b::int=''1'';');

drop table Tab23383;

-- pick index scan when query has a relabel on the index key: partitioned tables
-- start_ignore
drop table if exists Tbl23383_partitioned;
-- end_ignore

create table Tbl23383_partitioned(a int, b varchar(20), c varchar(20), d varchar(20))
partition by range(a)
(partition p1 start(1) end(500),
partition p2 start(500) end(1000) inclusive);
insert into Tbl23383_partitioned select g,g,g,g from generate_series(1,1000) g;
create index idx23383_b on Tbl23383_partitioned(b);

-- heterogenous indexes
create index idx23383_c on Tbl23383_partitioned_1_prt_p1(c);
create index idx23383_cd on Tbl23383_partitioned_1_prt_p2(c,d);
set optimizer_enable_dynamictablescan = off;
select count_index_scans('explain select * from Tbl23383_partitioned where b=''1''');
select * from Tbl23383_partitioned where b='1';

select count_index_scans('explain select * from Tbl23383_partitioned where ''1''=b');
select * from Tbl23383_partitioned where '1'=b;

select count_index_scans('explain select * from Tbl23383_partitioned where ''2''> b order by a limit 10;');
select * from Tbl23383_partitioned where '2'> b order by a limit 10;

select count_index_scans('explain select * from Tbl23383_partitioned where b between ''1'' and ''2'' order by a limit 10;');
select * from Tbl23383_partitioned where b between '1' and '2' order by a limit 10;

-- predicates on both index and non-index key
select count_index_scans('explain select * from Tbl23383_partitioned where b=''1'' and a=''1'';');
select * from Tbl23383_partitioned where b='1' and a='1';

--negative tests: no index scan plan possible, fall back to planner
select count_index_scans('explain select * from Tbl23383_partitioned where b::int=''1'';');

-- heterogenous indexes
select count_index_scans('explain select * from Tbl23383_partitioned where c=''1'';');
select * from Tbl23383_partitioned where c='1';

-- start_ignore
drop table Tbl23383_partitioned;
-- end_ignore

reset enable_seqscan;

-- negative test: due to non compatible cast and CXformGet2TableScan disabled no index plan possible, fallback to planner

-- start_ignore
drop table if exists tbl_ab;
-- end_ignore

create table tbl_ab(a int, b int);
create index idx_ab_b on tbl_ab(b);

-- start_ignore
select disable_xform('CXformGet2TableScan');
-- end_ignore

explain select * from tbl_ab where b::oid=1;

drop table tbl_ab;
drop function count_index_scans(text);
-- start_ignore
select enable_xform('CXformGet2TableScan');
-- end_ignore

--
-- Check that ORCA can use an index for joins on quals like:
--
-- indexkey CMP expr
-- expr CMP indexkey
--
-- where expr is a scalar expression free of index keys and may have outer
-- references.
--
create table nestloop_x (i int, j int) distributed by (i);
create table nestloop_y (i int, j int) distributed by (i);
insert into nestloop_x select g, g from generate_series(1, 20) g;
insert into nestloop_y select g, g from generate_series(1, 7) g;
create index nestloop_y_idx on nestloop_y (j);

-- Coerce the Postgres planner to produce a similar plan. Nested loop joins
-- are not enabled by default. And to dissuade it from choosing a sequential
-- scan, bump up the cost. enable_seqscan=off  won't help, because there is
-- no other way to scan table 'x', and once the planner chooses a seqscan for
-- one table, it will happily use a seqscan for other tables as well, despite
-- enable_seqscan=off. (On PostgreSQL, enable_seqscan works differently, and
-- just bumps up the cost of a seqscan, so it would work there.)
set seq_page_cost=10000000;
set enable_indexscan=on;
set enable_nestloop=on;

explain select * from nestloop_x as x, nestloop_y as y where x.i + x.j < y.j;
select * from nestloop_x as x, nestloop_y as y where x.i + x.j < y.j;

explain select * from nestloop_x as x, nestloop_y as y where y.j > x.i + x.j + 2;
select * from nestloop_x as x, nestloop_y as y where y.j > x.i + x.j + 2;

drop table nestloop_x, nestloop_y;

SET enable_seqscan = OFF;
SET enable_indexscan = ON;

DROP TABLE IF EXISTS bpchar_ops;
CREATE TABLE bpchar_ops(id INT8, v char(10)) DISTRIBUTED BY(id);
CREATE INDEX bpchar_ops_btree_idx ON bpchar_ops USING btree(v bpchar_pattern_ops);
INSERT INTO bpchar_ops VALUES (0, 'row');
SELECT * FROM bpchar_ops WHERE v = 'row '::char(20);

DROP TABLE bpchar_ops;


--
-- Test index rechecks with AO and AOCS tables (and heaps as well, for good measure)
--
create table shape_heap (c circle) with (appendonly=false);
create table shape_ao (c circle) with (appendonly=true, orientation=row);
create table shape_aocs (c circle) with (appendonly=true, orientation=column);

insert into shape_heap values ('<(0,0), 5>');
insert into shape_ao   values ('<(0,0), 5>');
insert into shape_aocs values ('<(0,0), 5>');

create index shape_heap_bb_idx on shape_heap using gist(c);
create index shape_ao_bb_idx   on shape_ao   using gist(c);
create index shape_aocs_bb_idx on shape_aocs using gist(c);

select c && '<(5,5), 1>'::circle,
       c && '<(5,5), 2>'::circle,
       c && '<(5,5), 3>'::circle
from shape_heap;

-- Test the same values with (bitmap) index scans
--
-- The first two values don't overlap with the value in the tables, <(0,0), 5>,
-- but their bounding boxes do. In a GiST index scan that uses the bounding
-- boxes, these will fetch the row from the index, but filtered out by the
-- recheck using the actual overlap operator. The third entry is sanity check
-- that the index returns any rows.
set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

-- Use EXPLAIN to verify that these use a bitmap index scan
explain select * from shape_heap where c && '<(5,5), 1>'::circle;
explain select * from shape_ao   where c && '<(5,5), 1>'::circle;
explain select * from shape_aocs where c && '<(5,5), 1>'::circle;

-- Test that they return correct results.
select * from shape_heap where c && '<(5,5), 1>'::circle;
select * from shape_ao   where c && '<(5,5), 1>'::circle;
select * from shape_aocs where c && '<(5,5), 1>'::circle;

select * from shape_heap where c && '<(5,5), 2>'::circle;
select * from shape_ao   where c && '<(5,5), 2>'::circle;
select * from shape_aocs where c && '<(5,5), 2>'::circle;

select * from shape_heap where c && '<(5,5), 3>'::circle;
select * from shape_ao   where c && '<(5,5), 3>'::circle;
select * from shape_aocs where c && '<(5,5), 3>'::circle;
