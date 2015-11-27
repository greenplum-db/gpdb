create table bitmapscan_bug as select '2008-02-01'::DATE AS DT, 
	case when j <= 96 
		then 0 
	when j<= 98 then 2 
	when j<= 99 then 3 
	when i % 1000 < 900 then 4 
	when i % 1000 < 800 then 5 
	when i % 1000 <= 998 then 5 else 6 
	end as ind, 
	(i*117-j)::bigint as s from generate_series(1,100) i, generate_series(1,100) j distributed randomly;

create table bitmapscan_bug2 as select * from bitmapscan_bug;

insert into bitmapscan_bug select DT + 1, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 2, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 3, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 4, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 5, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 6, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 7, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 8, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 9, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 10, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 11, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 12, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 13, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 14, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 15, ind, s from bitmapscan_bug2;

create index bitmapscan_bug_idx on bitmapscan_bug using bitmap (ind, dt);

vacuum analyze bitmapscan_bug;


create table mpp4593_bmbug (dt date, ind int, s bigint);
create index bmbug_idx on mpp4593_bmbug using bitmap(ind, dt);

-- Test Bitmap Indexscan
create table bm_test (a integer, b integer) distributed by (a);
insert into bm_test select a, a%25 from generate_series(1,100) a;
create index bm_test_a on bm_test (a);
set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

-- returns one or more tuples
select * from bm_test where a<10;

-- returns no tuples
select * from bm_test where a>100;

-- Test Bitmap Heapscan + Bitmap OR + Bitmap Indexscan

drop table if exists bm_test;
create table bm_test (a integer, b integer) distributed by (a);
insert into bm_test select a, a%25 from generate_series(1,100) a;

-- Test on 2 btrees
create index bm_test_a on bm_test (a);
create index bm_test_b on bm_test (b);

set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

select * from bm_test where a<100 or b>10;

-- Returns no tuples from one branch
select * from bm_test where a<100 or b>30;

-- Returns no tuples from both branch
select * from bm_test where a<1 or b>30;

-- Test on 2 bitmaps
drop index bm_test_a;
drop index bm_test_b;
create index bm_test_bm_a on bm_test using bitmap(a);
create index bm_test_bm_b on bm_test using bitmap(b);

select * from bm_test where a<100 or b>10;

-- Returns no tuples from one branch
select * from bm_test where a<100 or b>30;

-- Returns no tuples from both branch
select * from bm_test where a<1 or b>30;

-- Test on 1 btree, 1 bitmap
drop index bm_test_bm_a;
drop index bm_test_bm_b;
create index bm_test_a on bm_test (a);
create index bm_test_bm_b on bm_test using bitmap(b);

select * from bm_test where a<100 or b>10;

-- Returns no tuples from one branch
select * from bm_test where a<100 or b>30;

-- Returns no tuples from both branch
select * from bm_test where a<1 or b>30;

-- Test ArrayKeys
drop table if exists bm_test;
create table bm_test (a integer, b integer) distributed by (a);
insert into bm_test select a, a%25 from generate_series(1,100) a;
create index bm_test_a on bm_test (a);
set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

select * from bm_test where a in (1,3,5);
