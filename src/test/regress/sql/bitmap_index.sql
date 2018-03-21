SET enable_seqscan = OFF;
SET enable_indexscan = ON;
SET enable_bitmapscan = ON;

create table bm_test (i int, t text);
insert into bm_test select i % 10, (i % 10)::text  from generate_series(1, 100) i;
create index bm_test_idx on bm_test using bitmap (i);
select count(*) from bm_test where i=1;
select count(*) from bm_test where i in(1, 2);
select * from bm_test where i > 10;
reindex index bm_test_idx;
select count(*) from bm_test where i in(1, 2);
drop index bm_test_idx;
create index bm_test_multi_idx on bm_test using bitmap(i, t);
select * from bm_test where i=5 and t='5';
select * from bm_test where i=5 or t='6';
select * from bm_test where i between 1 and 10 and i::text = t;
drop table bm_test;

-- test a bunch of different data types
create table bm_test (i2 int2, i4 int4, i8 int8, f4 float4, f8 float8,
	n numeric(10, 3), t1 varchar(3), t2 char(3), t3 text, a int[2],
	ip inet, b bytea, t timestamp, d date, g bool);

insert into bm_test values(1, 1, 1, 1.0, 1.0, 1000.333, '1', '1', '1',
    array[1, 3], '127.0.0.1', E'\001', '2007-01-01 01:01:01',
    '2007-01-01', 't');

insert into bm_test values(2, 2, 2, 2.0, 2.0, 2000.333, '2', '2', 'foo',
    array[2, 6], '127.0.0.2', E'\002', '2007-01-02 01:01:01',
    '2007-01-02', 'f');

insert into bm_test default values; -- test nulls

create index bm_i2_idx on bm_test using bitmap(i2);
create index bm_i4_idx on bm_test using bitmap(i4);
create index bm_i8_idx on bm_test using bitmap(i8);

create index bm_f4_idx on bm_test using bitmap(f4);
create index bm_f8_idx on bm_test using bitmap(f8);

create index bm_n_idx on bm_test using bitmap(n);

create index bm_t1_idx on bm_test using bitmap(t1);
create index bm_t2_idx on bm_test using bitmap(t2);
create index bm_t3_idx on bm_test using bitmap(t3);

create index bm_a_idx on bm_test using bitmap(a);

create index bm_ip_idx on bm_test using bitmap(ip);

create index bm_b_idx on bm_test using bitmap(b);

create index bm_t_idx on bm_test using bitmap(t);

create index bm_d_idx on bm_test using bitmap(d);

create index bm_g_idx on bm_test using bitmap(g);

create index bm_t3_upper_idx on bm_test using bitmap(upper(t3));
create index bm_n_null_idx on bm_test using bitmap(n) WHERE n ISNULL;
-- Try some cross type stuff
select a.t from bm_test a, bm_test b where a.i2 = b.i2;
select a.t from bm_test a, bm_test b where a.i2 = b.i4;
select a.t from bm_test a, bm_test b where a.i2 = b.i8;
select a.t from bm_test a, bm_test b where b.f4 = a.f8 and a.f8 = '2.0';

-- some range queries
select a.t from bm_test a, bm_test b where a.n < b.n;
select a.t from bm_test a, bm_test b where a.ip < b.ip;

-- or queries
select a.t from bm_test a, bm_test b where a.ip=b.ip OR a.b = b.b;

-- and
select a.t from bm_test a, bm_test b where a.ip=b.ip and a.b = b.b and a.i2=1;

-- subquery
select a.t from bm_test a where d in(select d from bm_test b where a.g=b.g);

-- functional and predicate indexes
select t from bm_test where upper(t3) = 'FOO';
select t from bm_test where n ISNULL;
-- test updates
update bm_test set i4 = 3;
-- should return nothing
select * from bm_test where i4 = 1;
-- should return all
select * from bm_test where i4=3;
-- should return one row
select * from bm_test where i2=1;
-- test splitting of words
-- We distribute by k and only insert a single distinct value in that 
-- field so that we can be guaranteed of behaviour. We're not testing
-- the parallel mechanism here so it's fine to harass a single backend
create table bm_test2 (i int, j int, k int) distributed by (k);
create index bm_test2_i_idx on bm_test2 using bitmap(i);
insert into bm_test2 select 1,
case when (i % (16 * 16 + 8)) = 0 then 2  else 1 end, 1
from generate_series(1, 16 * 16 * 16) i;
select count(*) from bm_test2 where i = 1;
select count(*) from bm_test2 where j = 2;
-- break some compressed words
update bm_test2 set i = 2 where j = 2;
select count(*) from bm_test2 where i = 1;
select count(*) from bm_test2 where i = 2;
update bm_test2 set i = 3 where i = 1;
select count(*) from bm_test2 where i = 1;
select count(*) from bm_test2 where i = 2;
select count(*) from bm_test2 where i = 3;
-- now try and break a whole page
-- bitmap words are 16 bits so, with no compression we get about 
-- 16500 words per 32K page. So, what we want to do is, insert
-- 8250 uncompressed words, then a compressed word, then more uncompressed
-- words until the page is full. After this, we can break the compressed word
-- and there by test the word spliting system
create table bm_test3 (i int, j int, k int) distributed by (k);
create index bm_test3_i_idx on bm_test3 using bitmap(i);
insert into bm_test3 select i, 1, 1 from
generate_series(1, 8250 * 8) g, generate_series(1, 2) i;
insert into bm_test3 

select 17, 1, 1 from generate_series(1, 16 * 16) i;
insert into bm_test3 values(17, 2, 1);
insert into bm_test3
select 17, 1, 1 from generate_series(1, 16 * 16) i;

insert into bm_test3 select i, 1, 1 from
generate_series(1, 8250 * 8) g, generate_series(1, 2) i;
select count(*) from bm_test3 where i = 1;
select count(*) from bm_test3 where i = 17;
select count(*) from bm_test3 where i = 17 and j = 2;
update bm_test3 set i = 18 where i = 17 and j = 2;
select count(*) from bm_test3 where i = 1;
select count(*) from bm_test3 where i = 2;
select count(*) from bm_test3 where i = 17;
select count(*) from bm_test3 where i = 18;
drop table bm_test;
drop table bm_test2;
drop table bm_test3;

create table bm_test (i int, j int);
insert into bm_test values (0, 0), (0, 0), (0, 1), (1,0), (1,0), (1,1);
create index bm_test_j on bm_test using bitmap(j);
delete from bm_test where j =1;
vacuum bm_test;
insert into bm_test values (0, 0), (1,0);

set enable_seqscan=off;
set enable_bitmapscan=off;
set optimizer_enable_bitmapscan=off;
-- start_ignore
-- Known_opt_diff: MPP-19808
-- end_ignore
explain select * from bm_test where j = 1;
select * from bm_test where j = 1;
drop table bm_test;
-- MPP-3232
create table bm_test (i int,j int);
insert into bm_test values (1, 1), (1, 2);
create index bm_test_j on bm_test using bitmap(j);
update bm_test set j=20 where j=1;
vacuum bm_test;
drop table bm_test;

-- unique index with null value tests
drop table if exists ijk;
create table ijk(i int, j int, k int) distributed by (i);
insert into ijk values (1, 1, 3);
insert into ijk values (1, 2, 4);
insert into ijk values (1, 3, NULL);
insert into ijk values (1, 3, NULL);
insert into ijk values (1, NULL, NULL);
insert into ijk values (1, NULL, NULL);

-- should fail.
create unique index ijk_i on ijk(i);
create unique index ijk_ij on ijk(i,j);
-- should OK.
create unique index ijk_ijk on ijk(i,j,k);

set gp_enable_mk_sort=on;
drop table if exists ijk;
create table ijk(i int, j int, k int) distributed by (i);
insert into ijk values (1, 1, 3);
insert into ijk values (1, 2, 4);
insert into ijk values (1, 3, NULL);
insert into ijk values (1, 3, NULL);
insert into ijk values (1, NULL, NULL);
insert into ijk values (1, NULL, NULL);

-- should fail.
create unique index ijk_i on ijk(i);
create unique index ijk_ij on ijk(i,j);
-- should OK.
create unique index ijk_ijk on ijk(i,j,k);

set gp_enable_mk_sort=off;
drop table ijk;

---------
-- test bitmaps with NULL and non-NULL values (MPP-8461)
--
create table bmap_test (x int, y int, z int);
insert into bmap_test values (1,NULL,NULL);
insert into bmap_test values (NULL,1,NULL);
insert into bmap_test values (NULL,NULL,1);
insert into bmap_test values (1,NULL,NULL);
insert into bmap_test values (NULL,1,NULL);
insert into bmap_test values (NULL,NULL,1);
insert into bmap_test values (1,NULL,5);
insert into bmap_test values (NULL,1,NULL);
insert into bmap_test values (NULL,NULL,1);
insert into bmap_test select a from generate_series(1,10*1000) as s(a);
create index bmap_test_idx_1 on bmap_test using bitmap (x,y,z);
analyze bmap_test;
select * from bmap_test where x = 1 order by x,y,z;

drop table bmap_test;
