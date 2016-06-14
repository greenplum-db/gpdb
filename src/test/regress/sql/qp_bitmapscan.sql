
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_bitmapscan;
set search_path to qp_bitmapscan;
-- end_ignore
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: query01.sql
-- ----------------------------------------------------------------------

-- Test Bitmap Indexscan
drop table if exists test;
create table test (a integer, b integer);
insert into test select a, a%25 from generate_series(1,1000) a;
create index test_a on test (a);
set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

-- returnning one or more tuples
select * from test where a<10;

-- returninig no tuples
select * from test where a>1000;
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: query02.sql
-- ----------------------------------------------------------------------

-- Test Bitmap Heapscan + Bitmap And + Bitmap Indexscan

drop table if exists test;
create table test (a integer, b integer);
insert into test select a%10, a%25 from generate_series(1,100000) a;

-- Test on 2 btrees
create index test_a on test(a);
create index test_b on test(b);

set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

-- Both branches return tuples.
select * from test where a=5 and b=10;

-- One branch returns 0 tuple.
select * from test where a=20 and b=10;

-- Both branches return 0 tuple.
select * from test where a=20 and b=30;

-- Test on 2 bitmaps
drop index test_a;
drop index test_b;
create index test_bm_a on test using bitmap(a);
create index test_bm_b on test using bitmap(b);

select * from test where a=5 and b=10;

-- One branch returns 0 tuple.
select * from test where a=20 and b=10;

-- Both branches return 0 tuple.
select * from test where a=20 and b=30;

-- Test on 1 btree and 1 bitmap
drop index test_bm_a;
drop index test_bm_b;
create index test_a on test(a);
create index test_bm_b on test using bitmap(b);

select * from test where a=5 and b=10;

-- One branch returns 0 tuple.
select * from test where a=20 and b=10;

-- Both branches return 0 tuple.
select * from test where a=20 and b=30;
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: query03.sql
-- ----------------------------------------------------------------------

-- Test Bitmap Heapscan + Bitmap OR + Bitmap Indexscan

drop table if exists test;
create table test (a integer, b integer);
insert into test select a, a%25 from generate_series(1,1000) a;

-- Test on 2 btrees
create index test_a on test (a);
create index test_b on test (b);

set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

select * from test where a<100 or b>10;

-- Returnning no tuples from one branch
select * from test where a<100 or b>30;

-- Returnning no tuples from both branch
select * from test where a<1 or b>30;

-- Test on 2 bitmaps
drop index test_a;
drop index test_b;
create index test_bm_a on test using bitmap(a);
create index test_bm_b on test using bitmap(b);

select * from test where a<100 or b>10;

-- Returnning no tuples from one branch
select * from test where a<100 or b>30;

-- Returnning no tuples from both branch
select * from test where a<1 or b>30;

-- Test on 1 btree, 1 bitmap
drop index test_bm_a;
drop index test_bm_b;
create index test_a on test (a);
create index test_bm_b on test using bitmap(b);

select * from test where a<100 or b>10;

-- Returnning no tuples from one branch
select * from test where a<100 or b>30;

-- Returnning no tuples from both branch
select * from test where a<1 or b>30;
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: query04.sql
-- ----------------------------------------------------------------------

-- Test combination of BitmapAnd and BitmapOr

drop table if exists test;
create table test (a integer, b integer, c integer);
insert into test select a%10, a%25, a%30 from generate_series(1,100000) a;

-- Test BitmapAnd + 2 (BitmapOr)
create index test_a on test (a);
create index test_b on test (b);
create index test_c on test (c);

set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

select * from test where a=5 and (b=10 or c=20);

-- Test BitmapOr + 2 (BitmapAnd)
select * from test where a=5 or (b=10 and c=20);
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: query05.sql
-- ----------------------------------------------------------------------

-- Test ArrayKeys
drop table if exists test;
create table test (a integer, b integer);
insert into test select a, a%25 from generate_series(1,1000) a;
create index test_a on test (a);
set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

select * from test where a in (1,3,5);
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: query08.sql
-- ----------------------------------------------------------------------

drop table if exists foo;

create table foo (a int, b char(20));

insert into foo select a from generate_series(1,10000)a;
select count(*) from foo;

update foo set b = 'A';

create index foo_idx on foo using bitmap (b);
insert into foo select a from generate_series(1,10000)a;

update foo set b = 'B' where b is null;

vacuum full foo;
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: query09.sql
-- ----------------------------------------------------------------------

----------------------------------------------------------------
create table mpp8014 (i int) distributed by (i);
create index mpp8014_bitmap_idx on mpp8014 using bitmap(i);
select localoid::regclass, attrnums from gp_distribution_policy where localoid='mpp8014'::regclass order by localoid::regclass;
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: query10.sql
-- ----------------------------------------------------------------------

create table mpp7966 (i int, j int, k int);
insert into mpp7966 select i, i%123, i%234 from generate_series(0, 9999) i;
insert into mpp7966 select i, (random()::int * 1000000)%123, i%234 from generate_series(0, 9999) i;
insert into mpp7966 select * from mpp7966;
insert into mpp7966 select * from mpp7966;
insert into mpp7966 select * from mpp7966;
insert into mpp7966 select * from mpp7966;

create index mpp7966_j on mpp7966 using bitmap(j);

vacuum mpp7966;
analyze mpp7966;

select count(*) from mpp7966 where j in (1, 20);

select count(*) from mpp7966 where j in (1, 20, 30);

select count(*) from mpp7966 where j in (11, 22, 33);

drop table mpp7966;
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_bitmapscan cascade;
-- end_ignore
RESET ALL;
