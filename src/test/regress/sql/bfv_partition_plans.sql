create schema bfv_partition_plans;
set search_path=bfv_partition_plans;

--
-- Initial setup for all the partitioning test for this suite
--
-- start_ignore
create language plpythonu;
-- end_ignore

create or replace function count_operator(query text, operator text) returns int as
$$
rv = plpy.execute('EXPLAIN ' + query)
search_text = operator
result = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = result+1
return result
$$
language plpythonu;

create or replace function find_operator(query text, operator_name text) returns text as
$$
rv = plpy.execute('EXPLAIN ' + query)
search_text = operator_name
result = ['false']
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = ['true']
        break
return result
$$
language plpythonu;


-- Test UPDATE that moves row from one partition to another. The partitioning
-- key is also the distribution key in this case.
create table mpp3061 (i int) partition by range(i) (start(1) end(5) every(1));
insert into mpp3061 values(1);
update mpp3061 set i = 2 where i = 1;
drop table mpp3061;

--
-- Tests if it produces SIGSEGV from "select from partition_table group by rollup or cube function"
--

-- SETUP
create table mpp7980
(
 month_id date,
 bill_stmt_id  character varying(30),
 cust_type     character varying(10),
 subscription_status      character varying(30),
 voice_call_min           numeric(15,2),
 minute_per_call          numeric(15,2),
 subscription_id          character varying(15)
)
distributed by (subscription_id, bill_stmt_id)
  PARTITION BY RANGE(month_id)
    (
    start ('2009-02-01'::date) end ('2009-08-01'::date)  exclusive EVERY (INTERVAL '1 month')
    );
    
-- TEST
select count_operator('select cust_type, subscription_status,count(distinct subscription_id),sum(voice_call_min),sum(minute_per_call) from mpp7980 where month_id =E''2009-04-01'' group by rollup(1,2);','SIGSEGV');

insert into mpp7980 values('2009-04-01','xyz','zyz','1',1,1,'1');
insert into mpp7980 values('2009-04-01','zxyz','zyz','2',2,1,'1');
insert into mpp7980 values('2009-03-03','xyz','zyz','4',1,3,'1');
select cust_type, subscription_status,count(distinct subscription_id),sum(voice_call_min),sum(minute_per_call) from mpp7980 where month_id ='2009-04-01' group by rollup(1,2);

-- CLEANUP
drop table mpp7980;


-- ************ORCA ENABLED**********


--
-- MPP-23195
--

-- SETUP
-- start_ignore
set optimizer_enable_bitmapscan=on;
set optimizer_enable_indexjoin=on;
drop table if exists mpp23195_t1;
drop table if exists mpp23195_t2;
-- end_ignore

create table mpp23195_t1 (i int) partition by range(i) (partition pt1 start(1) end(10), partition pt2 start(10) end(20));
create index index_mpp23195_t1_i on mpp23195_t1(i);
create table mpp23195_t2(i int);

insert into mpp23195_t1 values (generate_series(1,19));
insert into mpp23195_t2 values (1);

-- TEST
select find_operator('select * from mpp23195_t1,mpp23195_t2 where mpp23195_t1.i < mpp23195_t2.i;', 'Dynamic Index Scan');
select * from mpp23195_t1,mpp23195_t2 where mpp23195_t1.i < mpp23195_t2.i;

-- CLEANUP
-- start_ignore
drop table if exists mpp23195_t1;
drop table if exists mpp23195_t2;
set optimizer_enable_bitmapscan=off;
set optimizer_enable_indexjoin=off;
-- end_ignore


--
-- Check we have Dynamic Index Scan operator and check we have Nest loop operator
--

-- SETUP
-- start_ignore
drop table if exists mpp21834_t1;
drop table if exists mpp21834_t2;
-- end_ignore

create table mpp21834_t1 (i int, j int) partition by range(i) (partition pp1 start(1) end(10), partition pp2 start(10) end(20));

create index index_1 on mpp21834_t1(i);

create index index_2 on mpp21834_t1(j);

create table mpp21834_t2(i int, j int);

-- TEST
set optimizer_enable_hashjoin = off;
select find_operator('analyze select * from mpp21834_t2,mpp21834_t1 where mpp21834_t2.i < mpp21834_t1.i;','Dynamic Index Scan');
select find_operator('analyze select * from mpp21834_t2,mpp21834_t1 where mpp21834_t2.i < mpp21834_t1.i;','Nested Loop');

-- CLEANUP
-- start_ignore
drop index index_2;
drop index index_1;
drop table if exists mpp21834_t2;
drop table if exists mpp21834_t1;
reset optimizer_enable_hashjoin;
-- end_ignore


--
-- A rescanning of DTS with its own partition selector (under sequence node)
--

-- SETUP
-- start_ignore
set optimizer_enable_broadcast_nestloop_outer_child=on;
drop table if exists mpp23288;
-- end_ignore

create table mpp23288(a int, b int) 
  partition by range (a)
  (
      PARTITION pfirst  END(5) INCLUSIVE,
      PARTITION pinter  START(5) EXCLUSIVE END (10) INCLUSIVE,
      PARTITION plast   START (10) EXCLUSIVE
  );

insert into mpp23288(a) select generate_series(1,20);

analyze mpp23288;

-- TEST
select count_operator('select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on (t1.a < t2.a and t2.a =10) order by t2.a, t1.a;','Dynamic Table Scan');
select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on (t1.a < t2.a and t2.a =10) order by t2.a, t1.a;

select count_operator('select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on (t1.a < t2.a and (t2.a = 10 or t2.a = 5 or t2.a = 12)) order by t2.a, t1.a;','Dynamic Table Scan');
select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on (t1.a < t2.a and (t2.a = 10 or t2.a = 5 or t2.a = 12)) order by t2.a, t1.a;

select count_operator('select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on t1.a < t2.a and t2.a = 1 or t2.a < 10 order by t2.a, t1.a;','Dynamic Table Scan');
select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on t1.a < t2.a and t2.a = 1 or t2.a < 10 order by t2.a, t1.a;

-- CLEANUP
-- start_ignore
drop table if exists mpp23288;
set optimizer_enable_broadcast_nestloop_outer_child=off;
-- end_ignore


--
-- Tests if DynamicIndexScan sets tuple descriptor of the planstate->ps_ResultTupleSlot
--

-- SETUP
-- start_ignore
drop table if exists mpp24151_t;
drop table if exists mpp24151_pt;
-- end_ignore

create table mpp24151_t(dist int, tid int, t1 text, t2 text);
create table mpp24151_pt(dist int, pt1 text, pt2 text, pt3 text, ptid int) 
DISTRIBUTED BY (dist)
PARTITION BY RANGE(ptid) 
          (
          START (0) END (5) EVERY (1),
          DEFAULT PARTITION junk_data
          )
;
create index pt1_idx on mpp24151_pt using btree (pt1);
create index ptid_idx on mpp24151_pt using btree (ptid);
insert into mpp24151_pt select i, 'hello' || 0, 'world', 'drop this', i % 6 from generate_series(0,100)i;
insert into mpp24151_pt select i, 'hello' || i, 'world', 'drop this', i % 6 from generate_series(0,200000)i;

insert into mpp24151_t select i, i % 6, 'hello' || i, 'bar' from generate_series(0,10)i;
analyze mpp24151_pt;
analyze mpp24151_t;

-- TEST
set optimizer_enable_dynamictablescan = off;

select count_operator('select * from mpp24151_t, mpp24151_pt where tid = ptid and pt1 = E''hello0'';','Result');
select * from mpp24151_t, mpp24151_pt where tid = ptid and pt1 = 'hello0';

-- CLEANUP
-- start_ignore
drop index ptid_idx;
drop index pt1_idx;
drop table if exists mpp24151_t;
drop table if exists mpp24151_pt;
reset optimizer_enable_dynamictablescan;
-- end_ignore


--
-- No DPE (Dynamic Partition Elimination) on second child of a union under a join
--

-- SETUP
-- start_ignore
drop table if exists t;
drop table if exists p1;
drop table if exists p2;
drop table if exists p3;
drop table if exists p;
-- end_ignore

create table p1 (a int, b int) partition by range(b) (start (1) end(100) every (20));
create table p2 (a int, b int) partition by range(b) (start (1) end(100) every (20));
create table p3 (a int, b int) partition by range(b) (start (1) end(100) every (20));
create table p (a int, b int);
create table t(a int, b int);

insert into t select g, g*10 from generate_series(1,100) g;

insert into p1 select g, g%99 +1 from generate_series(1,10000) g;

insert into p2 select g, g%99 +1 from generate_series(1,10000) g;

insert into p3 select g, g%99 +1 from generate_series(1,10000) g;

insert into p select g, g%99 +1 from generate_series(1,10000) g;

analyze t;
analyze p1;
analyze p2;
analyze p3;
analyze p;

-- TEST
select count_operator('select * from (select * from p1 union all select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 except all select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 except select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 intersect all select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p2 union all select * from p3) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p2 union all select * from p) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p union all select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p2 intersect all select * from p3) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p intersect all select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

-- CLEANUP
-- start_ignore
drop table t;
drop table p1;
drop table p2;
drop table p3;
drop table p;
-- end_ignore


--
-- Gracefully handle NULL partition set from BitmapTableScan, DynamicTableScan and DynamicIndexScan
--

-- SETUP
-- start_ignore
drop table if exists dts;
drop table if exists dis;
drop table if exists dbs;
-- end_ignore

create table dts(c1 int, c2 int) partition by range(c2) (start(1) end(11) every(1));
create table dis(c1 int, c2 int, c3 int) partition by range(c2) (start(1) end(11) every(1));
create index dis_index on dis(c3);
CREATE TABLE dbs(c1 int, c2 int, c3 int) partition by range(c2) (start(1) end(11) every(1));
create index dbs_index on dbs using bitmap(c3);


-- TEST
select find_operator('(select * from dts where c2 = 1) union (select * from dts where c2 = 2) union (select * from dts where c2 = 3) union (select * from dts where c2 = 4) union (select * from dts where c2 = 5) union (select * from dts where c2 = 6) union (select * from dts where c2 = 7) union (select * from dts where c2 = 8) union (select * from dts where c2 = 9) union (select * from dts where c2 = 10);', 'Dynamic Table Scan');

(select * from dts where c2 = 1) union
(select * from dts where c2 = 2) union
(select * from dts where c2 = 3) union
(select * from dts where c2 = 4) union
(select * from dts where c2 = 5) union
(select * from dts where c2 = 6) union
(select * from dts where c2 = 7) union
(select * from dts where c2 = 8) union
(select * from dts where c2 = 9) union
(select * from dts where c2 = 10);

set optimizer_enable_dynamictablescan = off;
select find_operator('(select * from dis where c3 = 1) union (select * from dis where c3 = 2) union (select * from dis where c3 = 3) union (select * from dis where c3 = 4) union (select * from dis where c3 = 5) union (select * from dis where c3 = 6) union (select * from dis where c3 = 7) union (select * from dis where c3 = 8) union (select * from dis where c3 = 9) union (select * from dis where c3 = 10);', 'Dynamic Index Scan');

(select * from dis where c3 = 1) union
(select * from dis where c3 = 2) union
(select * from dis where c3 = 3) union
(select * from dis where c3 = 4) union
(select * from dis where c3 = 5) union
(select * from dis where c3 = 6) union
(select * from dis where c3 = 7) union
(select * from dis where c3 = 8) union
(select * from dis where c3 = 9) union
(select * from dis where c3 = 10);

select find_operator('select * from dbs where c2= 15 and c3 = 5;', 'Bitmap Table Scan');

select * from dbs where c2= 15 and c3 = 5;

-- CLEANUP
-- start_ignore
drop index dbs_index;
drop table if exists dbs;
drop index dis_index;
drop table if exists dis;
drop table if exists dts;
reset optimizer_enable_dynamictablescan;
-- end_ignore

--
-- Partition elimination for heterogenous DynamicIndexScans
--

-- SETUP
-- start_ignore
drop table if exists pp;
drop index if exists pp_1_prt_1_idx;
drop index if exists pp_rest_1_idx;
drop index if exists pp_rest_2_idx;
set optimizer_segments=2;
set optimizer_partition_selection_log=on;
-- end_ignore
create table pp(a int, b int, c int) partition by range(b) (start(1) end(15) every(5));
insert into pp values (1,1,2),(2,6,2), (3,11,2);
-- Heterogeneous Index on the partition table
create index pp_1_prt_1_idx on pp_1_prt_1(c);
-- Create other indexes so that we can automate the repro for MPP-21069 by disabling tablescan
create index pp_rest_1_idx on pp_1_prt_2(c,a);
create index pp_rest_2_idx on pp_1_prt_3(c,a);
-- TEST
set optimizer_enable_dynamictablescan = off;
select * from pp where b=2 and c=2;
select count_operator('select * from pp where b=2 and c=2;','Partition Selector');

-- CLEANUP
-- start_ignore
drop index if exists pp_rest_2_idx;
drop index if exists pp_rest_1_idx;
drop index if exists pp_1_prt_1_idx;
drop table if exists pp;
reset optimizer_enable_dynamictablescan;
reset optimizer_segments;
set optimizer_partition_selection_log=off;
-- end_ignore


--
-- Partition elimination with implicit CAST on the partitioning key
--

-- SETUP
-- start_ignore
set optimizer_segments=2;
set optimizer_partition_selection_log=on;
DROP TABLE IF EXISTS ds_4;
-- end_ignore

CREATE TABLE ds_4
(
  month_id character varying(6),
  cust_group_acc numeric(10),
  mobile_no character varying(10)
)
DISTRIBUTED BY (cust_group_acc, mobile_no)
PARTITION BY LIST(month_id)
          (
          PARTITION p200800 VALUES('200800'),
          PARTITION p200801 VALUES('200801'),
          PARTITION p200802 VALUES('200802'),
          PARTITION p200803 VALUES('200803')
);

-- TEST
select * from ds_4 where month_id = '200800';
select count_operator('select * from ds_4 where month_id = E''200800'';','Partition Selector');

select * from ds_4 where month_id > '200800';
select count_operator('select * from ds_4 where month_id > E''200800'';','Partition Selector');

select * from ds_4 where month_id <= '200800';
select count_operator('select * from ds_4 where month_id <= E''200800'';','Partition Selector');

select * from ds_4 a1,ds_4 a2 where a1.month_id = a2.month_id and a1.month_id > '200800';
select count_operator('select * from ds_4 a1,ds_4 a2 where a1.month_id = a2.month_id and a1.month_id > E''200800'';','Partition Selector');

-- CLEANUP
-- start_ignore
DROP TABLE IF EXISTS ds_4;
set optimizer_partition_selection_log=off;
reset optimizer_segments;

-- end_ignore

--
-- Test a hash agg that has a Sequence + Partition Selector below it.
--

-- SETUP
-- start_ignore
DROP TABLE IF EXISTS bar;
-- end_ignore
CREATE TABLE bar (b int, c int)
PARTITION BY RANGE (b)
(
  START (0) END (10),
  START (10) END (20)
);

INSERT INTO bar SELECT g % 20, g % 20 from generate_series(1, 1000) g;
ANALYZE bar;

SELECT b FROM bar GROUP BY b;

EXPLAIN SELECT b FROM bar GROUP BY b;


-- CLEANUP
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS bar;


-- Test EXPLAIN ANALYZE on a partitioned table. There used to be a bug, where
-- you got an internal error with this, because the EXPLAIN ANALYZE sends the
-- stats from QEs to the QD at the end of query, but because the subnodes are
-- terminated earlier, their stats were already gone.
create table mpp8031 (oid integer,
odate timestamp without time zone,
cid integer)
PARTITION BY RANGE(odate)
(
PARTITION foo START ('2005-05-01 00:00:00'::timestamp
without time zone) END ('2005-07-01 00:00:00'::timestamp
without time zone) EVERY ('2 mons'::interval),

START ('2005-07-01 00:00:00'::timestamp without time zone)
END ('2006-01-01 00:00:00'::timestamp without time zone)
EVERY ('2 mons'::interval)
);
explain analyze select a.* from mpp8031 a, mpp8031 b where a.oid = b.oid;
drop table mpp8031;

-- Partitioned tables with default partitions and indexes on all parts,
-- queries on them with a predicate on index column must not consider the scan
-- as partial and should not fallback.
CREATE TABLE part_tbl
(
	time_client_key numeric(16,0) NOT NULL,
	ngin_service_key numeric NOT NULL,
	profile_key numeric NOT NULL
)
DISTRIBUTED BY (time_client_key)
PARTITION BY RANGE(time_client_key)
SUBPARTITION BY LIST (ngin_service_key)
SUBPARTITION TEMPLATE
(
	SUBPARTITION Package5 VALUES (479534741),
	DEFAULT SUBPARTITION other_services
)
(
	PARTITION p20151110 START (2015111000::numeric) 
END (2015111100::numeric) WITH (appendonly=false)
);
INSERT INTO part_tbl VALUES (2015111000, 479534741, 99999999);
INSERT INTO part_tbl VALUES (2015111000, 479534742, 99999999);
CREATE INDEX part_tbl_idx 
ON part_tbl(profile_key);
EXPLAIN SELECT * FROM part_tbl WHERE profile_key = 99999999;
SELECT * FROM part_tbl WHERE profile_key = 99999999;
DROP TABLE part_tbl;

--
-- Test partition elimination, MPP-7891
--

-- cleanup
-- start_ignore
drop table if exists r_part;
drop table if exists r_co;

deallocate f1;
deallocate f2;
deallocate f3;
-- end_ignore

create table r_part(a int, b int) partition by range(a) (start (1) end(10) every(1));
create table r_co(a int, b int) with (orientation=column, appendonly=true) partition by range(a) (start (1) end(10) every(1)) ;

insert into r_part values (1,1), (2,2), (3,3);

select * from r_part order by a,b;

analyze r_part;

explain select * from r_part r1, r_part r2 where r1.a=1; -- should eliminate partitions in the r1 copy of r_part

explain select * from r_part where a in (1,2); -- should eliminate partitions

-- Test partition elimination in prepared statements
prepare f1(int) as select * from r_part where a = 1 order by a,b; 
prepare f2(int) as select * from r_part where a = $1 order by a,b;

execute f1(1); 
execute f2(1); 
execute f2(2); 


explain select * from r_part where a = 1 order by a,b; -- should eliminate partitions
--force_explain
explain execute f1(1); -- should eliminate partitions 
--force_explain
explain execute f2(2); -- should eliminate partitions


-- Test partition elimination on CO tables
insert into r_co values (1,1), (2,2), (3,3);
analyze r_co; 
explain select * from r_co where a=2; -- should eliminate partitions

-- test partition elimination in prepared statements on CO tables
prepare f3(int) as select * from r_co where a = $1 order by a,b; 
--force_explain
explain execute f3(2); -- should eliminate partitions

-- start_ignore
drop table r_part;
drop table r_co;
deallocate f1;
deallocate f2;
deallocate f3;
-- end_ignore

--
-- Test partition elimination, MPP-7891
--

-- start_ignore
drop table if exists fact;
deallocate f1;

create table fact(x int, dd date, dt text) distributed by (x) partition by range (dd) ( start('2008-01-01') end ('2320-01-01') every(interval '100 years'));
-- end_ignore

analyze fact;

select '2009-01-02'::date = to_date('2009-01-02','YYYY-MM-DD'); -- ensure that both are in fact equal

explain select * from fact where dd < '2009-01-02'::date; -- partitions eliminated

explain select * from fact where dd < to_date('2009-01-02','YYYY-MM-DD'); -- partitions eliminated

explain select * from fact where dd < current_date; --partitions eliminated

-- Test partition elimination in prepared statements

prepare f1(date) as select * from fact where dd < $1;

-- force_explain
explain execute f1('2009-01-02'::date); -- should eliminate partitions
-- force_explain
explain execute f1(to_date('2009-01-02', 'YYYY-MM-DD')); -- should eliminate partitions

-- start_ignore
drop table fact;
deallocate f1;
-- end_ignore

-- MPP-6247
-- Delete Using on partitioned table causes repetitive scans on using table	
create table mpp6247_foo ( c1 int, dt date ) distributed by ( c1 ) partition by range (dt) ( start ( date '2009-05-01' ) end ( date '2009-05-11' ) every ( interval '1 day' ) );
create table mpp6247_bar (like mpp6247_foo);

-- EXPECT: Single HJ after partition elimination instead of sequence of HJ under Append
select count_operator('delete from mpp6247_foo using mpp6247_bar where mpp6247_foo.c1 = mpp6247_bar.c1 and mpp6247_foo.dt = ''2009-05-03''', 'Hash Join');

drop table mpp6247_bar;
drop table mpp6247_foo;

-- CLEANUP
-- start_ignore
drop schema if exists bfv_partition_plans cascade;
-- end_ignore
