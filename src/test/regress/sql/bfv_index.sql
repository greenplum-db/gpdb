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

--start_ignore
DROP TABLE IF EXISTS bfv_tab2_facttable1;
DROP TABLE IF EXISTS bfv_tab2_dimdate;
DROP TABLE IF EXISTS bfv_tab2_dimtabl1;
--end_ignore

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
-- start_ignore
select disable_xform('CXformDynamicGet2DynamicTableScan');
-- end_ignore

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

-- when optimizer is on PG exception raised during DXL->PlStmt translation for IndexScan query

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
