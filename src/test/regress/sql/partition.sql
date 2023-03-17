
drop table if exists d;
drop table if exists c;
drop table if exists b;
drop table if exists a;
-- Check multi level partition COPY
create table region
(
	r_regionkey integer not null,
	r_name char(25),
	r_comment varchar(152)
)
distributed by (r_regionkey)
partition by range (r_regionkey)
subpartition by list (r_name) subpartition template
(
	subpartition africa values ('AFRICA'),
	subpartition america values ('AMERICA'),
	subpartition asia values ('ASIA'),
	subpartition europe values ('EUROPE'),
	subpartition mideast values ('MIDDLE EAST'),
	subpartition australia values ('AUSTRALIA'),
	subpartition antarctica values ('ANTARCTICA')
)
(
	partition region1 start (0),
	partition region2 start (3),
	partition region3 start (5) end (8)
);

-- root and internal parent partitions should have relfrozenxid as 0
select relname, relkind from pg_class where relkind in ('r', 'p') and relname like 'region%' and relfrozenxid=0;
select gp_segment_id, relname, relkind from gp_dist_random('pg_class') where relkind in ('r', 'p') and relname like 'region%' and relfrozenxid=0;

create unique index region_pkey on region(r_regionkey, r_name);

copy region from stdin with delimiter '|';
0|AFRICA|lar deposits. blithely final packages cajole. regular waters are 
1|AMERICA|hs use ironic, even requests. s
2|ASIA|ges. thinly even pinto beans ca
3|EUROPE|ly final courts cajole furiously final excuse
4|MIDDLE EAST|uickly special accounts cajole carefully blithely close 
5|AUSTRALIA|sdf
6|ANTARCTICA|dsfdfg
\.

-- Test indexes
set enable_seqscan to off;
select * from region where r_regionkey = 1;
select * from region where r_regionkey = 2;
select * from region where r_regionkey = 3;
select * from region where r_regionkey = 4;
select * from region where r_regionkey = 5;
select * from region where r_regionkey = 6;

-- Test indexes with insert

insert into region values(7, 'AUSTRALIA', 'def');
select * from region where r_regionkey = '7';
-- test duplicate key. We shouldn't really allow primary keys on partitioned
-- tables since we cannot enforce them. But since this insert maps to a
-- single definitive partition, we can detect it.
insert into region values(7, 'AUSTRALIA', 'def');

drop table region;

-- exchange
-- 1) test all sanity checking

create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));

-- policies are different
create table bar_p_diff_pol (i int, j int) distributed by (j);
-- should fail
alter table foo_p exchange partition for(6) with table bar_p_diff_pol;

-- random policy vs. hash policy
create table bar_p_rand_pol (i int, j int) distributed randomly;
-- should fail
alter table foo_p exchange partition for(6) with table bar_p_rand_pol;

-- different number of columns
create table bar_p_diff_col (i int, j int, k int) distributed by (i);
-- should fail
alter table foo_p exchange partition for(6) with table bar_p_diff_col;

-- different types
create table bar_p_diff_typ (i int, j int8) distributed by (i);
-- should fail
alter table foo_p exchange partition for(6) with table bar_p_diff_typ;

-- different column names
create table bar_p_diff_colnam (i int, m int) distributed by (i);
-- should fail
alter table foo_p exchange partition for(6) with table bar_p_diff_colnam;

-- still different schema, but more than one level partitioning
CREATE TABLE two_level_pt(a int, b int, c int)
DISTRIBUTED BY (a)
PARTITION BY RANGE (b)
      SUBPARTITION BY RANGE (c)
      SUBPARTITION TEMPLATE (
      START (11) END (12) EVERY (1))
      ( START (1) END (2) EVERY (1));

CREATE TABLE candidate_for_leaf(a int, c int);

-- should fail
ALTER TABLE two_level_pt ALTER PARTITION FOR (1)
      EXCHANGE PARTITION FOR (11) WITH TABLE candidate_for_leaf;


-- different owner 
create role part_role;
create table bar_p (i int, j int) distributed by (i);
set session authorization part_role;
-- should fail
alter table foo_p exchange partition for(6) with table bar_p;

-- back to superuser
\c -
alter table bar_p owner to part_role;
set session authorization part_role;
-- should fail
alter table foo_p exchange partition for(6) with table bar_p;
\c -

-- owners should be the same, error out 
alter table foo_p exchange partition for(6) with table bar_p;
drop table foo_p;
drop table bar_p;

-- should work, and new partition should inherit ownership (mpp-6538)
set role part_role;
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(6) every(3));
reset role;
alter table foo_p split partition for (1) at (2) into (partition prt_11, partition prt_12);
\dt foo_*
drop table foo_p;

drop role part_role;

-- WITH OIDS is no longer supported. Check that it't rejected with the GPDB
-- partitioning syntax, too.
create table foo_p (i int, j int) with (oids = true) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));

-- non-partition table involved in inheritance
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));

create table barparent(i int, j int) distributed by (i);
create table bar_p () inherits(barparent);
-- should fail
alter table foo_p exchange partition for(6) with table bar_p;
drop table bar_p;
drop table barparent;

-- non-partition table involved in inheritance
create table bar_p(i int, j int) distributed by (i);
create table barchild () inherits(bar_p);
-- should fail
alter table foo_p exchange partition for(6) with table bar_p;
drop table barchild;
drop table bar_p;

-- rules on non-partition table
create table bar_p(i int, j int) distributed by (i);
create table baz_p(i int, j int) distributed by (i);
create rule bar_baz as on insert to bar_p do instead insert into baz_p
  values(NEW.i, NEW.j);

alter table foo_p exchange partition for(2) with table bar_p;
drop table foo_p, bar_p, baz_p;

create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(5) every(1));

-- Shouldn't fail: check constraint matches partition rule.
-- Note this test is slightly different from prior versions to get
-- in line with constraint consistency requirement.
create table bar_d(i int, j int check (j >= 2 and j < 3 ))
distributed by (i);
insert into bar_d values(100000, 2);
alter table foo_p exchange partition for(2) with table bar_d;
insert into bar_d values(200000, 2);
select * from bar_d;

drop table foo_p, bar_d;

-- permissions
create role part_role;
create table foo_p (i int) partition by range(i) (start(1) end(10) every(1));
create table bar_p (i int);

grant select on foo_p to part_role;
revoke all on bar_p from part_role;
select has_table_privilege('part_role', 'foo_p_1_prt_6'::regclass, 'select');
select has_table_privilege('part_role', 'bar_p'::regclass, 'select');
alter table foo_p exchange partition for(6) with table bar_p;

select has_table_privilege('part_role', 'foo_p_1_prt_6'::regclass, 'select');
select has_table_privilege('part_role', 'bar_p'::regclass, 'select');

-- the ONLY keyword will affect just the partition root for both grant/revoke
create role part_role2;
grant select on only foo_p to part_role2;
select has_table_privilege('part_role2', 'foo_p'::regclass, 'select');
select has_table_privilege('part_role2', 'foo_p_1_prt_6'::regclass, 'select');

grant select on foo_p to part_role2;
revoke select on only foo_p from part_role2;
select has_table_privilege('part_role2', 'foo_p'::regclass, 'select');
select has_table_privilege('part_role2', 'foo_p_1_prt_6'::regclass, 'select');
revoke select on foo_p from part_role2;
select has_table_privilege('part_role2', 'foo_p_1_prt_6'::regclass, 'select');

create table foo_p2 (a int, b int) partition by range(a) (start(1) end(10) every(1));
grant select on foo_p, only foo_p2 to part_role2; -- multiple tables in same statement
select has_table_privilege('part_role2', 'foo_p'::regclass, 'select');
select has_table_privilege('part_role2', 'foo_p_1_prt_6'::regclass, 'select');
select has_table_privilege('part_role2', 'foo_p2'::regclass, 'select');
select has_table_privilege('part_role2', 'foo_p2_1_prt_6'::regclass, 'select');

-- more cases
revoke all on foo_p from part_role2;
revoke all on foo_p2 from part_role2;
grant select on only public.foo_p to part_role2; -- with schema
select has_table_privilege('part_role2', 'foo_p'::regclass, 'select');
select has_table_privilege('part_role2', 'foo_p_1_prt_6'::regclass, 'select');
grant update(b) on only foo_p2 to part_role2; -- column level priviledge
select relname, has_column_privilege('part_role2', oid, 'b', 'update') from pg_class
where relname = 'foo_p2' or relname = 'foo_p2_1_prt_6';

drop table foo_p;
drop table foo_p2;
drop table bar_p;
drop role part_role;
drop role part_role2;

-- validation
create table foo_p (i int) partition by range(i)
(start(1) end(10) every(1));
create table bar_p (i int);

insert into bar_p values(6);
insert into bar_p values(100);
-- should fail
alter table foo_p exchange partition for(6) with table bar_p;
alter table foo_p exchange partition for(6) with table bar_p without
validation;
analyze foo_p;
select * from foo_p;
drop table foo_p, bar_p;

-- basic test
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p(i int, j int) distributed by (i);

insert into bar_p values(0,6);
alter table foo_p exchange partition for(6) with table bar_p;
analyze foo_p;
select * from foo_p;
select * from bar_p;
-- test that we got the dependencies right
drop table bar_p;
select * from foo_p;
drop table foo_p;
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p(i int, j int) distributed by (i);

insert into bar_p values(6, 6);
alter table foo_p exchange partition for(6) with table bar_p;
insert into bar_p values(10, 10);
drop table foo_p;
select * from bar_p;
insert into bar_p values(6, 6);
select * from bar_p;
drop table bar_p;

-- AO exchange with heap
create table foo_p (i int, j int) with(appendonly = true) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p(i int, j int) distributed by (i);

insert into foo_p values(1, 1), (2, 1), (3, 1);
insert into bar_p values(6, 6);
alter table foo_p exchange partition for(6) with table bar_p;
analyze foo_p;
select * from foo_p;
drop table bar_p;
drop table foo_p;

-- other way around
create table foo_p (i int, j int) with(appendonly = false) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p(i int, j int) with(appendonly = true) distributed by (i);

insert into foo_p values(1, 1), (2, 1), (3, 2);
insert into bar_p values(6, 6);
alter table foo_p exchange partition for(6) with table bar_p;
analyze foo_p;
select * from foo_p;
drop table bar_p;
drop table foo_p;

-- exchange AO with AO
create table foo_p (i int, j int) with(appendonly = true) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p(i int, j int) with(appendonly = true) distributed by (i);

insert into foo_p values(1, 2), (2, 3), (3, 4);
insert into bar_p values(6, 6);
alter table foo_p exchange partition for(6) with table bar_p;
analyze foo_p;
select * from foo_p;
drop table bar_p;
drop table foo_p;

-- exchange same table more than once
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p(i int, j int) distributed by (i);

insert into bar_p values(6, 6);
alter table foo_p exchange partition for(6) with table bar_p;
analyze foo_p;
select * from foo_p;
select * from bar_p;

alter table foo_p exchange partition for(6) with table bar_p;
select * from foo_p;
select * from bar_p;

alter table foo_p exchange partition for(6) with table bar_p;
select * from foo_p;
select * from bar_p;
drop table foo_p;
drop table bar_p;

-- exchange default partition is not allowed (single level)
drop table if exists dex;
drop table if exists exh_abc;
create table dex (i int,  j int) partition by range(j)
(partition a start (1) end(10), partition b start(11) end(20),
default partition abc);

create table exh_abc (like dex);
alter table dex exchange default partition with table exh_abc;

drop table dex;
drop table exh_abc;

-- exchange default partition is not allowed (multi level)
Drop table if exists sto_ao_ao;
drop table if exists exh_ao_ao;

Create table sto_ao_ao
 (
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true)
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two'))
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

create table exh_ao_ao (like sto_ao_ao) with (appendonly=true);
alter table sto_ao_ao alter partition for ('2008-03-01') exchange default partition with table exh_ao_ao;

-- Exchange a non-default sub-partition of a default partition, should fail
alter table sto_ao_ao alter default partition exchange partition for ('one') with table exh_ao_ao;

-- Exchange a partition that has sub partitions, should fail.
alter table sto_ao_ao exchange partition for ('2008-01-01') with table exh_ao_ao;

-- XXX: not yet: VALIDATE parameter

-- Exchange a partition with an external table;
create table foo_p (i int, j int) distributed by (i) partition by range(j) (start(1) end(10) every(2));
create readable external table bar_p(i int, j int) location ('gpfdist://host.invalid:8000/file') format 'text';
alter table foo_p exchange partition for(3) with table bar_p;
truncate foo_p;
drop table foo_p;
drop table bar_p;

-- Check for overflow of circular data types like time
-- Should fail
CREATE TABLE TIME_TBL_HOUR_2 (f1 time(2)) distributed by (f1)
partition by range (f1)
(
  start (time '00:00') end (time '24:00') EVERY (INTERVAL '1 hour')
);
-- Should fail
CREATE TABLE TIME_TBL_HOUR_2 (f1 time(2)) distributed by (f1)
partition by range (f1)
(
  start (time '00:00') end (time '23:59') EVERY (INTERVAL '1 hour')
);
-- Should work
CREATE TABLE TIME_TBL_HOUR_2 (f1 time(2)) distributed by (f1)
partition by range (f1)
(
  start (time '00:00') end (time '23:00') EVERY (INTERVAL '1 hour')
);
drop table TIME_TBL_HOUR_2;
-- Check for every parameters that just don't make sense
create table hhh_r2 (a char(1), b date, d char(3))
distributed by (a) partition by range (b)
(                                                              
partition aa start (date '2007-01-01') end (date '2008-01-01') 
      every (interval '0 days')
);

create table foo_p (i int) distributed by(i)
partition by range(i)
(start (1) end (20) every(0));

-- Check for ambiguous EVERY parameters
create table foo_p (i int) distributed by (i)
partition by range(i)
(start (1) end (3) every (0.6));
\d+ foo_p
drop table foo_p;
-- should fail
create table foo_p (i int) distributed by (i)
partition by range(i)
(start (1) end (3) every (0.3));
create table foo_p (i int) distributed by (i)
partition by range(i)
(start (1) end (3) every (1.3));
\d+ foo_p
drop table foo_p;

create table foo_p (i int) distributed by (i)
partition by range(i)
(start (1) end (20) every (10.9));
\d+ foo_p
drop table foo_p;

-- should fail
create table foo_p (i int, j date) distributed by (i)
partition by range(j)
(start ('2007-01-01') end ('2008-01-01') every (interval '0.5 days'));

-- should fail
create table foo_p (i int, j date) distributed by (i)
partition by range(j)
(start ('2007-01-01') end ('2008-01-01') every (interval '12 hours'));

create table foo_p (i int, j date) distributed by (i)
partition by range(j)
(start ('2007-01-01') end ('2007-01-05') every (interval '1.2 days'));
\d+ foo_p
drop table foo_p;

-- should work
create table foo_p (i int, j timestamp) distributed by (i)
partition by range(j)
(start ('2007-01-01') end ('2007-01-05') every (interval '1.2 days'));
\d+ foo_p
drop table foo_p;

-- test inclusive/exclusive
CREATE TABLE supplier2(
                S_SUPPKEY INTEGER,
                S_NAME CHAR(25),
                S_ADDRESS VARCHAR(40),
                S_NATIONKEY INTEGER,
                S_PHONECHAR char(15),
                S_ACCTBAL decimal,
				S_COMMENT VARCHAR(100)
)
partition by range (s_nationkey)
(
partition p1 start(0) , 
partition p2 start(12) end(13), 
partition p3 end(20) inclusive, 
partition p4 start(20) exclusive , 
partition p5 start(22) end(25)
);

\d+ supplier2

insert into supplier2 (s_suppkey, s_nationkey) select i, i 
from generate_series(1, 24) i;
select * from supplier2_1_prt_p1 order by S_NATIONKEY;
select * from supplier2_1_prt_p2 order by S_NATIONKEY;
select * from supplier2_1_prt_p3 order by S_NATIONKEY;
select * from supplier2_1_prt_p4 order by S_NATIONKEY;
select * from supplier2_1_prt_p5 order by S_NATIONKEY;
drop table supplier2;

-- mpp3238
create table foo_p (i int) partition by range (i)
(
 partition p1 start('1') ,
 partition p2 start('2639161') ,
 partition p3 start('5957166') ,
 partition p4 start('5981976') end('5994376') inclusive,
 partition p5 end('6000001')
);
\d+ foo_p

insert into foo_p values(5994400);
insert into foo_p values(1);
insert into foo_p values(6000002);
insert into foo_p values(5994376);
drop table foo_p;

create table foo_p (i int) 
partition by range(i)
(partition p1 start(1) end(5),
 partition p2 start(10),
 partition p3 end(10) exclusive);
\d+ foo_p

drop table foo_p;
create table foo_p (i int) 
partition by range(i)
(partition p1 start(1) end(5),
 partition p2 start(10) exclusive,
 partition p3 end(10) inclusive);
\d+ foo_p

insert into foo_p values(1), (5), (10);
drop table foo_p;

-- MPP-3264
-- mix AO with master HEAP and see if copy works
create table foo_p (i int)
partition by list(i)
(partition p1 values(1, 2, 3) with (appendonly = true),
 partition p2 values(4)
);

copy foo_p from stdin;
1
2
3
4
\.
select * from foo_p;
select * from foo_p_1_prt_p1;
select * from foo_p_1_prt_p2;
drop table foo_p;
-- other way around
create table foo_p (i int) with(appendonly = true)
partition by list(i)
(partition p1 values(1, 2, 3) with (appendonly = false),
 partition p2 values(4)
);

copy foo_p from stdin;
1
2
3
4
\.
select * from foo_p;
select * from foo_p_1_prt_p1;
select * from foo_p_1_prt_p2;
drop table foo_p;

-- Same as above, but the input is ordered so that the inserts to the heap
-- partition happen first. Had a bug related flushing the multi-insert
-- buffers in that scenario at one point.
-- (https://github.com/greenplum-db/gpdb/issues/6678
create table mixed_ao_part(distkey int, partkey int)
with (appendonly=true) distributed by(distkey)
partition by range(partkey) (
  partition p1 start(0) end(100) with (appendonly = false),
   partition p2 start(100) end(199)
);
copy mixed_ao_part from stdin;
1	95
2	96
3	97
4	98
5	99
1	100
2	101
3	102
4	103
5	104
\.
select * from mixed_ao_part;

-- Don't drop the table, so that we leave behind a mixed table in the
-- regression database for pg_dump/restore testing.

-- MPP-3283
CREATE TABLE PARTSUPP (
PS_PARTKEY INTEGER,
PS_SUPPKEY INTEGER,
PS_AVAILQTY integer,
PS_SUPPLYCOST decimal,
PS_COMMENT VARCHAR(199)
)
partition by range (ps_suppkey)
subpartition by range (ps_partkey)
subpartition by range (ps_supplycost) subpartition template (start('1')
end('1001') every(500))
(
partition p1 start('1') end('10001') every(5000)
(subpartition sp1 start('1') end('200001') every(66666)
)
);
insert into partsupp values(1,2,3325,771.64,', even theodolites. regular, final
theodolites eat after the carefully pending foxes. furiously regular deposits
sleep slyly. carefully bold realms above the ironic dependencies haggle
careful');
copy partsupp from stdin with delimiter '|';
1|2|3325|771.64|, even theodolites. regular, final theodolites eat after the
\.
drop table partsupp;
--MPP-3285
CREATE TABLE PARTLINEITEM (
                L_ORDERKEY INT8,
                L_PARTKEY INTEGER,
                L_SUPPKEY INTEGER,
                L_LINENUMBER integer,
                L_QUANTITY decimal,
                L_EXTENDEDPRICE decimal,
                L_DISCOUNT decimal,
                L_TAX decimal,
                L_RETURNFLAG CHAR(1),
                L_LINESTATUS CHAR(1),
                L_SHIPDATE date,
                L_COMMITDATE date,
                L_RECEIPTDATE date,
                L_SHIPINSTRUCT CHAR(25),
                L_SHIPMODE CHAR(10),
                L_COMMENT VARCHAR(44)
                )
partition by range (l_commitdate)
(
partition p1 start('1992-01-31') end('1998-11-01') every(interval '20 months')

);
copy partlineitem from stdin with delimiter '|';
18182|5794|3295|4|9|15298.11|0.04|0.01|N|O|1995-07-04|1995-05-30|1995-08-03|DELIVER IN PERSON|RAIL|y special platelets
\.

\d+ partlineitem

drop table partlineitem;

-- Make sure ADD creates dependencies
create table i (i int) partition by range(i) (start (1) end(3) every(1));
alter table i add partition foo2 start(40) end (50);
drop table i;

create table i (i int) partition by range(i) (start (1) end(3) every(1));
alter table i add partition foo2 start(40) end (50);
alter table i drop partition foo2;

-- when using the partition name to find target partition table,
-- we shoud check whether the matched table belong to the partitioned table.
-- raise error instead of executing on the irrelevant table.
create table i_1_prt_3 (like i);

-- create another partitioned table which contains a partition table that
-- could be matched by partition name when targeted on an irrelevant table.
create table i2 (i int) partition by range(i) (start (1) end(3) every(1));
create table i_1_prt_4 partition of i2 for values from (4) to (5);

-- the matechd table name is i_1_prt_3, but it's a normal table, raise error.
alter table i drop partition "3";
-- the matched table name is i_1_prt_4, but it belongs to i2, raise error
alter table i drop partition "4";

-- should raise error since we always make sure the partitioned table at least have one partition;
alter table i drop partition "1", drop partition "2";

drop table i;
drop table i2;
drop table i_1_prt_3;

create table i (i int) partition by range(i) (start(1) end(3) every(1), default partition extra);
-- should raise error since we always make sure the partitioned table at least have one partition;
alter table i drop partition "2", drop partition "3", drop default partition;
drop table i;

create table i (i int, c text) partition by range(i) subpartition by list(c)
subpartition template (subpartition a values ('a'), subpartition b values ('b'))
(start (1) end(3) every(1));
-- should raise error since we always make sure the partitioned table at least have one partition;
alter table i alter partition "1" drop partition a, alter partition "1" drop partition b;
drop table i;

CREATE TABLE PARTSUPP (
PS_PARTKEY INTEGER,
PS_SUPPKEY INTEGER,
PS_AVAILQTY integer,
PS_SUPPLYCOST decimal,
PS_COMMENT VARCHAR(199)
)
partition by range (ps_suppkey)
subpartition by range (ps_partkey)
subpartition by range (ps_supplycost) subpartition template (start('1')
end('1001') every(500))
(
partition p1 start('1') end('10001') every(5000)
(subpartition sp1 start('1') end('200001') every(66666)
)
);

select relname, pg_get_expr(relpartbound, oid, false) from pg_class where relname like 'partsupp%';
drop table partsupp;

-- ALTER TABLE ALTER PARTITION tests

CREATE TABLE ataprank (id int, rank int,
year date, gender char(1),
usstate char(2))
DISTRIBUTED BY (id, gender, year, usstate)
partition by list (gender)
subpartition by range (year)
subpartition template (
subpartition jan01 start (date '2001-01-01'),
subpartition jan02 start (date '2002-01-01'),
subpartition jan03 start (date '2003-01-01'),
subpartition jan04 start (date '2004-01-01'),
subpartition jan05 start (date '2005-01-01')
)
subpartition by list (usstate)
subpartition template (
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
)
(
  partition boys values ('M'),
  partition girls values ('F')
);

-- and without subpartition templates...
CREATE TABLE ataprank2 (id int, rank int,
year date, gender char(1),
usstate char(2))
DISTRIBUTED BY (id, gender, year, usstate)
partition by list (gender)
subpartition by range (year)
subpartition by list (usstate)
(
  partition boys values ('M') 
(
subpartition jan01 start (date '2001-01-01') 
(
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
),
subpartition jan02 start (date '2002-01-01')
(
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
),
subpartition jan03 start (date '2003-01-01')
(
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
),
subpartition jan04 start (date '2004-01-01')
(
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
),
subpartition jan05 start (date '2005-01-01')
(
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
)
),

  partition girls values ('F')
(
subpartition jan01 start (date '2001-01-01')
(
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
),
subpartition jan02 start (date '2002-01-01')
(
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
),
subpartition jan03 start (date '2003-01-01')
(
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
),
subpartition jan04 start (date '2004-01-01')
(
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
),
subpartition jan05 start (date '2005-01-01')
(
subpartition mass values ('MA'),
subpartition cali values ('CA'),
subpartition ohio values ('OH')
)
)
);

-- ok
alter table ataprank truncate partition girls;
alter table ataprank alter partition girls truncate partition for ('2001-01-01');
alter table ataprank alter partition girls alter partition 
for ('2001-01-01') truncate partition mass;

-- addressing partitions by RANK is no longer supported
alter table ataprank truncate partition for (rank (1));


-- don't NOTIFY of children if cascade
alter table ataprank truncate partition girls cascade;

-- fail - no partition for '1999-01-01'
alter table ataprank alter partition girls truncate partition for ('1999-01-01');

-- fail - no funky
alter table ataprank alter partition girls alter partition 
for ('2001-01-01') truncate partition "funky";

-- fail - no funky (drop)
alter table ataprank alter partition girls alter partition 
for ('2001-01-01') drop partition "funky";

-- fail - missing name
alter table ataprank alter partition girls alter partition 
for ('2001-01-01') drop partition ;

-- ok
alter table ataprank alter partition girls drop partition 
for ('2001-01-01') ;

-- ok , skipping
alter table ataprank alter partition girls drop partition if exists jan01;

-- ok
alter table ataprank alter partition girls drop partition for ('2002-01-01');
alter table ataprank alter partition girls drop partition for ('2003-01-01');
alter table ataprank alter partition girls drop partition for ('2004-01-01');

-- ok, skipping
alter table ataprank alter partition girls drop partition if exists for ('2004-01-01');

-- ok
alter table ataprank alter partition girls rename partition jan05 
to "funky fresh";
alter table ataprank alter partition girls rename partition "funky fresh"
to jan05;

-- fail , not exist
alter table ataprank alter partition girls alter partition jan05 rename
partition jan01 to foo;

-- fail not exist
alter table ataprank alter partition girls alter partition jan05 alter
partition cali rename partition foo to bar;

-- fail not partitioned
alter table ataprank alter partition girls alter partition jan05 alter
partition cali alter partition foo drop partition bar;

-- ADD PARTITION, with and without templates

-- fails for ataprank (due to template), works for ataprank2
alter table ataprank
add partition neuter values ('N')
    (subpartition foo
         start ('2001-01-01') end ('2002-01-01')
         every (interval '1 month')
            (subpartition bar values ('AZ')));
alter table ataprank2
add partition neuter values ('N')
    (subpartition foo
         start ('2001-01-01') end ('2002-01-01')
         every (interval '1 month')
            (subpartition bar values ('AZ')));

-- fail , no subpartition spec for ataprank2, works for ataprank
alter table ataprank alter partition boys
add partition jan00 start ('2000-01-01') end ('2001-01-01');
alter table ataprank2 alter partition boys
add partition jan00 start ('2000-01-01') end ('2001-01-01');

-- work - create subpartition for ataprank2, fail for ataprank
alter table ataprank alter partition boys
add partition jan99 start ('1999-01-01') end ('2000-01-01')
  (subpartition ariz values ('AZ'));
alter table ataprank2 alter partition boys
add partition jan00 start ('2000-01-01') end ('2001-01-01')
  (subpartition ariz values ('AZ'));

-- works for both -- adding leaf partition doesn't conflict with template
alter table ataprank alter partition boys
alter partition jan00 
add partition haw values ('HI');
alter table ataprank2 alter partition boys
alter partition jan00 
add partition haw values ('HI');

alter table ataprank drop partition neuter;
alter table ataprank2 drop partition neuter;

-- fail , no subpartition spec for ataprank2, work for ataprank
alter table ataprank
add default partition neuter ;
alter table ataprank2
add default partition neuter ;

alter table ataprank
add default partition neuter 
    (subpartition foo
         start ('2001-01-01') end ('2002-01-01')
         every (interval '1 month')
            (subpartition ariz values ('AZ')));
alter table ataprank2
add default partition neuter 
    (subpartition foo
         start ('2001-01-01') end ('2002-01-01')
         every (interval '1 month')
            (subpartition ariz values ('AZ')));

-- fail
alter table ataprank
alter default partition add default partition def1 
(subpartition haw values ('HI'));
-- fail
alter table ataprank
alter default partition alter default partition 
add default partition def2;
-- work
alter table ataprank
alter default partition add default partition def1;
alter table ataprank
alter default partition alter default partition 
add default partition def2;



alter table ataprank2
alter default partition add default partition def1 
(subpartition haw values ('HI'));
alter table ataprank2
alter default partition alter default partition 
add default partition def2;


drop table ataprank ;
drop table ataprank2 ;

-- **END** ALTER TABLE ALTER PARTITION tests

-- Test casting
create table f (i int) partition by range (i) (start(1::int) end(10::int));
drop table f;
create table f (i bigint) partition by range (i) (start(1::int8)
end(1152921504606846976::int8) every(576460752303423488));
drop table f;
create table f (n numeric(20, 2)) partition by range(n) (start(1::bigint)
end(10000::bigint));
drop table f;
--should fail, there's no assignment cast from text to numeric
create table f (n numeric(20, 2)) partition by range(n) (start(1::bigint)
end(10000::text));
--should fail. there's no assignment cast from bool to numeric
create table f (n numeric(20, 2)) partition by range(n) (start(1::bigint)
end('f'::bool));

-- see that grant and revoke cascade to children
create role part_role;
create table granttest (i int, j int) partition by range(i) 
subpartition by list(j) subpartition template (values(1, 2, 3))
(start(1) end(4) every(1));

select relname, has_table_privilege('part_role', oid,'select') as tabpriv,
       has_column_privilege('part_role', oid, 'i', 'select') as i_priv,
       has_column_privilege('part_role', oid, 'j', 'select') as j_priv
from pg_class where relname like 'granttest%';

grant select (i) on granttest to part_role;
select relname, has_table_privilege('part_role', oid,'select') as tabpriv,
       has_column_privilege('part_role', oid, 'i', 'select') as i_priv,
       has_column_privilege('part_role', oid, 'j', 'select') as j_priv
from pg_class where relname like 'granttest%';

grant select on granttest to part_role;
select relname, has_table_privilege('part_role', oid,'select') as tabpriv,
       has_column_privilege('part_role', oid, 'i', 'select') as i_priv,
       has_column_privilege('part_role', oid, 'j', 'select') as j_priv
from pg_class where relname like 'granttest%';

grant insert on granttest to part_role;
select relname, has_table_privilege('part_role', oid, 'insert') as tabpriv,
       has_column_privilege('part_role', oid, 'i', 'insert') as i_priv,
       has_column_privilege('part_role', oid, 'j', 'insert') as j_priv
from pg_class where relname like 'granttest%';

revoke insert on granttest from part_role;
grant insert (j) on granttest to part_role;
select relname, has_table_privilege('part_role', oid, 'insert') as tabpriv,
       has_column_privilege('part_role', oid, 'i', 'insert') as i_priv,
       has_column_privilege('part_role', oid, 'j', 'insert') as j_priv
from pg_class where relname like 'granttest%';

-- Check that when a new partition is created, it inherits the permissions
-- from the parent.
alter table granttest add partition newpart start(100) end (101);

-- same with the new upstream syntax.
create table granttest_newpartsyntax partition of granttest for values from (110) to (120);

select relname, has_table_privilege('part_role', oid, 'select') as tabpriv,
       has_column_privilege('part_role', oid, 'i', 'select') as i_priv,
       has_column_privilege('part_role', oid, 'j', 'select') as j_priv
from pg_class where relname like 'granttest%';
select relname, has_table_privilege('part_role', oid, 'insert') as tabpriv,
       has_column_privilege('part_role', oid, 'i', 'insert') as i_priv,
       has_column_privilege('part_role', oid, 'j', 'insert') as j_priv
from pg_class where relname like 'granttest%';

revoke all on granttest from part_role;
select relname, has_table_privilege('part_role', oid, 'insert') as tabpriv,
       has_column_privilege('part_role', oid, 'i', 'insert') as i_priv,
       has_column_privilege('part_role', oid, 'j', 'insert') as j_priv
from pg_class where relname like 'granttest%';

drop table granttest;
drop role part_role;

-- deep inline + optional subpartition comma:
CREATE TABLE partsupp_1 (
    ps_partkey integer,
    ps_suppkey integer,
    ps_availqty integer,
    ps_supplycost numeric,
    ps_comment character varying(199)
) distributed by (ps_partkey) PARTITION BY RANGE(ps_suppkey)
          SUBPARTITION BY RANGE(ps_partkey)
                  SUBPARTITION BY RANGE(ps_supplycost) 
          (
          PARTITION p1_1 START (1) END (1666667) EVERY (1666666) 
                  (
                  START (1) END (19304783) 
                          (
                          START (1::numeric) END (501::numeric) EVERY (500), 
                          START (501::numeric) END (1001::numeric) EVERY (500)
                          ), 
                  START (19304783) END (100000001) 
                          (
                          START (1::numeric) END (501::numeric) EVERY (500), 
                          START (501::numeric) END (1001::numeric) EVERY (500)
                          )
                  ), 
          PARTITION p1_2 START (1666667) END (3333333) EVERY (1666666) 
                  (
                  START (1) END (19304783) 
                          (
                          START (1::numeric) END (501::numeric) EVERY (500), 
                          START (501::numeric) END (1001::numeric) EVERY (500)
                          ), 
                  START (19304783) END (100000001) 
                          (
                          START (1::numeric) END (501::numeric) EVERY (500), 
                          START (501::numeric) END (1001::numeric) EVERY (500)
                          )
                  ), 
          PARTITION p1_3 START (3333333) END (4999999) EVERY (1666666) 
                  (
                  START (1) END (19304783) 
                          (
                          START (1::numeric) END (501::numeric) EVERY (500), 
                          START (501::numeric) END (1001::numeric) EVERY (500)
                          ), 
                  START (19304783) END (100000001) 
                          (
                          START (1::numeric) END (501::numeric) EVERY (500), 
                          START (501::numeric) END (1001::numeric) EVERY (500)
                          )
                  ), 
          PARTITION p1_4 START (4999999) END (5000001) EVERY (1666666) 
                  (
                  START (1) END (19304783) 
                          (
                          START (1::numeric) END (501::numeric) EVERY (500), 
                          START (501::numeric) END (1001::numeric) EVERY (500)
                          ), 
                  START (19304783) END (100000001) 
                          (
                          START (1::numeric) END (501::numeric) EVERY (500), 
                          START (501::numeric) END (1001::numeric) EVERY (500)
                          )
                  )
          );

-- Accept negative values trivially:
create table partition_g (i int) partition by range(i) (start((-1)) end(10));
drop table partition_g;
create table partition_g (i int) partition by range(i) (start(-1) end(10));
drop table partition_g;

CREATE TABLE orders (
    o_orderkey bigint,
    o_custkey integer,
    o_orderstatus character(1),
    o_totalprice numeric,
    o_orderdate date,
    o_orderpriority character(15),
    o_clerk character(15),
    o_shippriority integer,
    o_comment character varying(79)
)
WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) PARTITION BY RANGE(o_orderdate)
          SUBPARTITION BY RANGE(o_custkey)
                  SUBPARTITION BY RANGE(o_orderkey) 
          (
          PARTITION p1_1 START ('1992-01-01'::date) END ('1993-06-01'::date) EVERY ('1 year 5 mons'::interval) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                  (
                  SUBPARTITION sp1 START (1) END (46570) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                          (
                          START (1::bigint) END (1500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (1500001::bigint) END (3000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (3000001::bigint) END (4500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (4500001::bigint) END (6000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9)
                          ), 
                  SUBPARTITION sp2 START (46570) END (150001) INCLUSIVE WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                          (
                          START (1::bigint) END (1500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (1500001::bigint) END (3000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (3000001::bigint) END (4500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (4500001::bigint) END (6000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9)
                          )
                  ), 
          PARTITION p1_2 START ('1993-06-01'::date) END ('1994-11-01'::date) EVERY ('1 year 5 mons'::interval) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                  (
                  SUBPARTITION sp1 START (1) END (46570) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                          (
                          START (1::bigint) END (1500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (1500001::bigint) END (3000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (3000001::bigint) END (4500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (4500001::bigint) END (6000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9)
                          ), 
                  SUBPARTITION sp2 START (46570) END (150001) INCLUSIVE WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                          (
                          START (1::bigint) END (1500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (1500001::bigint) END (3000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (3000001::bigint) END (4500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (4500001::bigint) END (6000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9)
                          )
                  ), 
          PARTITION p1_3 START ('1994-11-01'::date) END ('1996-04-01'::date) EVERY ('1 year 5 mons'::interval) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                  (
                  SUBPARTITION sp1 START (1) END (46570) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                          (
                          START (1::bigint) END (1500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (1500001::bigint) END (3000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (3000001::bigint) END (4500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (4500001::bigint) END (6000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9)
                          ), 
                  SUBPARTITION sp2 START (46570) END (150001) INCLUSIVE WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                          (
                          START (1::bigint) END (1500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (1500001::bigint) END (3000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (3000001::bigint) END (4500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (4500001::bigint) END (6000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9)
                          )
                  ), 
          PARTITION p1_4 START ('1996-04-01'::date) END ('1997-09-01'::date) EVERY ('1 year 5 mons'::interval) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                  (
                  SUBPARTITION sp1 START (1) END (46570) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                          (
                          START (1::bigint) END (1500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (1500001::bigint) END (3000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (3000001::bigint) END (4500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (4500001::bigint) END (6000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9)
                          ), 
                  SUBPARTITION sp2 START (46570) END (150001) INCLUSIVE WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                          (
                          START (1::bigint) END (1500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (1500001::bigint) END (3000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (3000001::bigint) END (4500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (4500001::bigint) END (6000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9)
                          )
                  ), 
          PARTITION p1_5 START ('1997-09-01'::date) END ('1998-08-03'::date) EVERY ('1 year 5 mons'::interval) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                  (
                  SUBPARTITION sp1 START (1) END (46570) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                          (
                          START (1::bigint) END (1500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (1500001::bigint) END (3000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (3000001::bigint) END (4500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (4500001::bigint) END (6000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9)
                          ), 
                  SUBPARTITION sp2 START (46570) END (150001) INCLUSIVE WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9) 
                          (
                          START (1::bigint) END (1500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (1500001::bigint) END (3000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (3000001::bigint) END (4500001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9), 
                          START (4500001::bigint) END (6000001::bigint) EVERY (1500000) WITH (appendonly=true, checksum=true, blocksize=368640, compresslevel=9)
                          )
                  )
          );

-- grammar bug: MPP-3361
create table i2 (i int) partition by range(i) (start(-2::int) end(20));
drop table i2;
create table i2 (i int) partition by range(i) (start((-2)::int) end(20));
drop table i2;
create table i2 (i int) partition by range(i) (start(cast ((-2)::bigint as int))
end(20));
drop table i2;
CREATE TABLE partsupp (
    ps_partkey integer,
    ps_suppkey integer,
    ps_availqty integer,
    ps_supplycost numeric,
    ps_comment character varying(199)
) PARTITION BY RANGE(ps_supplycost)
          (
          PARTITION newpart START ((-10000)::numeric) EXCLUSIVE END (1::numeric)
,
          PARTITION p1 START (1::numeric) END (1001::numeric)
          );
drop table partsupp;

-- Deletion tests
CREATE TABLE tmp_nation_region (n_regionkey integer);
drop table if exists tmp_nation;
CREATE TABLE tmp_nation (N_NATIONKEY INTEGER, N_NAME CHAR(25), N_REGIONKEY INTEGER, N_COMMENT VARCHAR(152))  
partition by range (n_nationkey) 
 (
partition p1 start('0')  WITH (appendonly=true,checksum=true,blocksize=1998848,compresslevel=4),  
partition p2 start('11') end('15') inclusive WITH (checksum=false,appendonly=true,blocksize=655360,compresslevel=4),
partition p3 start('15') exclusive end('19'), partition p4 start('19')  WITH (compresslevel=8,appendonly=true,checksum=false,blocksize=884736), 
partition p5 start('20')
);
delete from tmp_nation where n_regionkey in (select n_regionkey from tmp_nation_region) and n_nationkey between 1 and 5;
drop table tmp_nation;

-- SPLIT tests
-- basic sanity tests. All should pass.
create table k (i int) partition by range(i) (start(1) end(10) every(2), 
default partition mydef);
insert into k select i from generate_series(1, 100) i;
alter table k split partition mydef at (20) into (partition mydef, 
partition foo);
drop table k;

create table j (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8));
insert into j select i from generate_series(1, 8) i;
alter table j split partition for(1) at (2, 3) into (partition fa, partition
fb);
select * from j_1_prt_fa;
select * from j_1_prt_fb;
alter table j split partition for(5) at (6);
select * from j;
-- should fail
alter table j split partition for (1) at (100);
drop table j;
create table k (i int) partition by range(i) (start(1) end(10) every(2), 
default partition mydef);
-- should fail
alter table k split default partition start(30) end (300) into (partition mydef, partition mydef);
alter table k split partition for(3) at (20);
drop table k;
-- should work
create table k (i int) partition by range(i) (start(1) end(10) every(2), 
default partition mydef);
insert into k select i from generate_series(1, 30) i;
alter table k split default partition start(15) end(20) into
(partition mydef, partition foo);
select * from k_1_prt_foo;
alter table k split default partition start(22) exclusive end(25) inclusive
into (partition bar, partition mydef);
select * from k_1_prt_bar;

-- This fails, because it would create a partition with an empty range. Before
-- GPDB 7, this passed, because a partition like "start (22) exclusive end (23)
-- exclusive" was considered valid.
alter table k split partition bar at (23) into (partition baz, partition foz);
\d+ k
drop table k;
-- Add CO partition and split, reported in MPP-17761
create table k (i int) with (appendonly = true, orientation = column) distributed by (i) partition by range(i) (start(1) end(10) every(5));
alter table k add partition co start(11) end (17) with (appendonly = true, orientation = column);
alter table k split partition co at (14) into (partition co1, partition co2);
drop table k;
create table k (a int, b int) with (appendonly = true) distributed by (a) partition by list(b)
(
	partition a values (1, 2, 3, 4) with (appendonly = true, orientation = column),
	partition b values (5, 6, 7 ,8) with (appendonly = true, orientation = column)
);
alter table k split partition for(2) at(2) into (partition one, partition two);
drop table k;
-- Test errors for default handling
create table k (i int) partition by range(i) (start(1) end(2), 
default partition mydef);
alter table k split partition mydef at (25) into (partition foo, partition
mydef);
drop table k;
create table k (i int) partition by list(i) (values(1), values(2),
default partition mydef);
alter table k split default partition start(10) end(20);
drop table k;

-- Check that we support int2
CREATE TABLE myINT2_TBL(q1 int2)
 partition by range (q1)
 (start (1) end (3) every (1));
insert into myint2_tbl values(1), (2);
drop table myint2_tbl;

-- try SREH on a partitioned table.
create table ao_p (i int) with (appendonly = true)
 partition by range(i)
 (start(1) end(5) every(1));

copy ao_p from stdin log errors segment reject limit 100;
2
3
10000
f
\.
select * from ao_p;
drop table ao_p;

-- MPP-3591: make sure we get inclusive/exclusive right with every().
create table k (i int) partition by range(i)
(start(0) exclusive end(100) inclusive every(25));
\d+ k
insert into k select i from generate_series(1, 100) i;
drop table k;

-- ADD and SPLIT must get inherit permissions of the partition they're
-- modifying
create role part_role;
create table a (a int, b int, c int) partition by range(a) subpartition by
range(b) subpartition template (subpartition h start(1) end(10)) 
subpartition by range(c)
subpartition template(subpartition i start(1) end(10)) 
(partition g start(1) end(2));
revoke all on a from public;
grant insert on a to part_role;
-- revoke it from one existing partition, to make sure we don't screw up
-- existing permissions
revoke all on a_1_prt_g_2_prt_h_3_prt_i from part_role;
alter table a add partition b start(40) end(50);
set session authorization part_role;
select has_table_privilege('part_role', 'a'::regclass,'insert');
select has_table_privilege('part_role', 'a_1_prt_b_2_prt_h'::regclass,'insert');
select has_table_privilege('part_role', 'a_1_prt_b_2_prt_h_3_prt_i'::regclass,'insert');
select has_table_privilege('part_role', 'a_1_prt_g_2_prt_h_3_prt_i'::regclass,
'insert');
insert into a values(45, 5, 5);
-- didn't grant select
select has_table_privilege('part_role', 'a'::regclass,'select');
select has_table_privilege('part_role', 'a_1_prt_b_2_prt_h'::regclass,'select');
select has_table_privilege('part_role', 'a_1_prt_b_2_prt_h_3_prt_i'::regclass,'select');
\c -
drop table a;
create table a (i date) partition by range(i) 
(partition f start(date '2005-01-01') end (date '2009-01-01')
	every(interval '2 years'));
revoke all on a from public;
grant insert on a to part_role;
alter table a split partition for ('2005-01-01') at (date '2006-01-01')
  into (partition f, partition g);
alter table a add default partition mydef;
alter table a split default partition start(date '2010-01-01') end(date
'2011-01-01') into(partition mydef, partition other);
set session authorization part_role;
select has_table_privilege('part_role', 'a'::regclass,'insert');
select has_table_privilege('part_role', 'a_1_prt_f'::regclass,'insert');
select has_table_privilege('part_role', 'a_1_prt_mydef'::regclass,'insert');
select has_table_privilege('part_role', 'a_1_prt_other'::regclass,'insert');
insert into a values('2005-05-05');
insert into a values('2006-05-05');
insert into a values('2010-10-10');
\c -
drop table a;
drop role part_role;
-- Check that when we split a default, the INTO clause must named the default
create table k (i date) partition by range(i) (start('2008-01-01')
end('2009-01-01') every(interval '1 month'), default partition default_part);
alter table k split default partition start ('2009-01-01') end ('2009-02-01')
into (partition aa, partition nodate);
alter table k split default partition start ('2009-01-01') end ('2009-02-01')
into (partition aa, partition default_part);
-- check that it works without INTO
alter table k split default partition start ('2009-02-01') end ('2009-03-01');
drop table k;
-- List too
create table k (i int) partition by list(i) (partition a values(1, 2),
partition b values(3, 4), default partition mydef);
alter table k split partition mydef at (5) into (partition foo, partition bar);
alter table k split partition mydef at (5) into (partition foo, partition mydef);
alter table k split partition mydef at (10);
drop table k;

-- For LIST, make sure that we reject AT() clauses which match all parameters
create table j (i int) partition by list(i) (partition a values(1, 2, 3, 4),
 partition b values(5, 6, 7, 8));
alter table j split partition for(1) at (1,2) into (partition fa, partition fb);
alter table j split partition for(1) at (1,2) 
into (partition f1a, partition f1b); -- This has partition rules that overlaps
drop table j;

-- Check that we can split LIST partitions that have a default partition
create table j (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
alter table j split partition for(1) at (1,2) into (partition f1a, partition
f1b);
drop table j;
-- Make sure range can too
create table j (i int) partition by range(i) (partition a start(1) end(10),
default partition default_part);
alter table j split partition for(1) at (5) into (partition f1a, partition f1b);
drop table j;

-- MPP-3667 ADD PARTITION overlaps
create table mpp3621 (aa date, bb date) partition by range (bb)
(partition foo start('2008-01-01'));

-- these are ok
alter table mpp3621 add partition a1 start ('2007-01-01') end ('2007-02-01');
alter table mpp3621 add partition a2 start ('2007-02-01') end ('2007-03-01');
alter table mpp3621 add partition a3 start ('2007-03-01') end ('2007-04-01');
alter table mpp3621 add partition a4 start ('2007-09-01') end ('2007-10-01');
alter table mpp3621 add partition a5 start ('2007-08-01') end ('2007-09-01');
alter table mpp3621 add partition a6 start ('2007-04-01') end ('2007-05-01');
alter table mpp3621 add partition a7 start ('2007-05-01') end ('2007-06-01');

 -- was error due to startSearchpoint != endSearchpoint
alter table mpp3621 add partition a8 start ('2007-07-01') end ('2007-08-01');

-- ok
alter table mpp3621 add partition a9 start ('2007-06-01') end ('2007-07-01');

drop table mpp3621;

-- Check for MPP-3679 and MPP-3692
create table list_test (a text, b text) partition by list (a) (
  partition foo values ('foo'),
  partition bar values ('bar'),
  default partition baz);
insert into list_test values ('foo', 'blah');
insert into list_test values ('bar', 'blah');
insert into list_test values ('baz', 'blah');
alter table list_test split default partition at ('baz')
  into (partition bing, default partition);
drop table list_test;

-- MPP-3816: cannot drop column  which is the subject of partition config
create table list_test(a int, b int, c int) distributed by (a)
  partition by list(b) 
  subpartition by list(c) subpartition template(subpartition c values(2))
  (partition b values(1));
-- should fail
alter table list_test drop column b;
alter table list_test drop column c;
drop table list_test;

-- MPP-3678: allow exchange and split on tables with subpartitioning
CREATE TABLE rank_exc (
id int,
rank int,
year int,
gender char(1),
count int ) 

DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
SUBPARTITION BY RANGE (year)
SUBPARTITION TEMPLATE (
SUBPARTITION year1 START (2001),
SUBPARTITION year2 START (2002),
SUBPARTITION year3 START (2003),
SUBPARTITION year4 START (2004),
SUBPARTITION year5 START (2005),
SUBPARTITION year6 START (2006) END (2007) )
(PARTITION girls VALUES ('F'),
PARTITION boys VALUES ('M')
);
alter table rank_exc alter partition girls add default partition gfuture;
alter table rank_exc alter partition boys add default partition bfuture;
insert into rank_exc values(1, 1, 2007, 'M', 1);
insert into rank_exc values(2, 2, 2008, 'M', 3);
select * from rank_exc;
alter table rank_exc alter partition boys split default partition start ('2007')
end ('2008') into (partition bfuture, partition year7);
select * from rank_exc_1_prt_boys_2_prt_bfuture;
select * from rank_exc_1_prt_boys_2_prt_year7;
select * from rank_exc;

--exchange test
create table r (like rank_exc);
insert into rank_exc values(3, 3, 2004, 'F', 100);
insert into r values(3, 3, 2004, 'F', 100000);
alter table rank_exc alter partition girls exchange partition year4 with table r;
select * from rank_exc_1_prt_girls_2_prt_year4;
select * from r;
alter table rank_exc alter partition girls exchange partition year4 with table r;
select * from rank_exc_1_prt_girls_2_prt_year4;
select * from r;

-- Split test
alter table rank_exc alter partition girls split default partition start('2008')
  end('2020') into (partition years, partition gfuture);
insert into rank_exc values(4, 4, 2009, 'F', 100);
drop table rank_exc;
drop table r;

-- MPP-4245: remove virtual subpartition templates when we drop the partitioned
-- table
create table bar_p (i int, j int) partition by range(i) subpartition by range(j)
subpartition template(start(1) end(10) every(1)) subpartition by range(i)
subpartition template(start(1) end(10) every(5)) (start(1) end(10));
alter table bar_p alter partition for ('5') alter partition for ('5')
  drop partition for ('5');
insert into bar_p values(1, 1);
insert into bar_p values(5, 5);
drop table bar_p;
-- Drop should not leave anything lingering for bar_p or its
-- subpartitions in pg_partition* catalog tables.
select relid, level, pg_get_expr(template, relid) from gp_partition_template where not exists (select oid from pg_class where oid = relid);

-- MPP-4172
-- should fail
create table ggg (a char(1), b int)
distributed by (b)
partition by range(a)
(
partition aa start ('2006') end ('2009'), partition bb start ('2007') end ('2008')
);


-- MPP-4892 SET SUBPARTITION TEMPLATE
create table mpp4892 (a char, b int, d char)
partition by range (b)
subpartition by list (d)
subpartition template (
 subpartition sp1 values ('a'),
 subpartition sp2 values ('b'))
(
start (1) end (10) every (1)
);

-- works
alter table mpp4892 add partition p1 end (11);

-- complain about existing template
alter table mpp4892 add partition p3 end (13) (subpartition sp3 values ('c'));

-- remove template
alter table mpp4892 set	subpartition template ();

-- should work (because the template is gone)
alter table mpp4892 add partition p3 end (13) (subpartition sp3 values ('c'));

-- complain because the template is already gone
alter table mpp4892 set	subpartition template ();

-- should work
alter table mpp4892 set subpartition template (subpartition sp3 values ('c'));

-- should work
alter table mpp4892 add partition p4 end (15);

drop table mpp4892;


-- make sure we do not allow overlapping range intervals
-- should fail
-- unordered elems
create table ttt (t int) partition by range(t) (
partition a start (1) end(10) inclusive,
partition c start(11) end(14),
partition b start(5) end(15)
);

-- should fail, this time it's ordered
create table ttt (t int) partition by range(t) (
partition a start (1) end(10) inclusive,
partition b start(5) end(15),
partition c start(11) end(14)
);

-- should fail
create table ttt (t date) partition by range(t) (
partition a start ('2005-01-01') end('2006-01-01') inclusive,
partition b start('2005-05-01') end('2005-06-11'),
partition c start('2006-01-01') exclusive end('2006-01-10')
);

-- should fail
create table ttt (t char) partition by range(t) (
partition a start('a') end('f'),
partition b start('e') end('g')
);

-- MPP-5159 MPP-26829
-- Should fail -- missing partition spec and subpartition template follows the
-- partition declaration.
CREATE TABLE list_sales (trans_id int, date date, amount
decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY LIST (region)
SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'),
  SUBPARTITION asia VALUES ('asia'),
  SUBPARTITION europe VALUES ('europe')
);

-- MPP-5185 MPP-26829
-- Should work
CREATE TABLE rank_settemp (id int, rank int, year date, gender
char(1)) DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'),
start (date '2002-01-01'),
start (date '2003-01-01'),
start (date '2004-01-01'),
start (date '2005-01-01')
)
(
partition boys values ('M'),
partition girls values ('F')
);

alter table rank_settemp set subpartition template ();

-- nothing there
select relid::regclass, level from gp_partition_template where relid = 'rank_settemp'::regclass;

alter table rank_settemp set subpartition template (default subpartition def2);

-- def2 is there
select relid::regclass, level, pg_get_expr(template, relid) from gp_partition_template where relid = 'rank_settemp'::regclass;

alter table rank_settemp set subpartition template (default subpartition def2);
-- Should still be there
select relid::regclass, level, pg_get_expr(template, relid) from gp_partition_template where relid = 'rank_settemp'::regclass;


alter table rank_settemp set subpartition template (start (date '2006-01-01') with (appendonly=true));
alter table rank_settemp add partition f1 values ('N');
alter table rank_settemp set subpartition template (start (date '2007-01-01') with (appendonly=true, compresslevel=5));
alter table rank_settemp add partition f2 values ('C');

select relid::regclass, level, pg_get_expr(template, relid) from gp_partition_template where relid = 'rank_settemp'::regclass;

drop table rank_settemp;

-- MPP-5397 and MPP-7002
-- should be able to add/split/exchange partition after dropped a col

create table mpp_5397 (a int, b int, c int, d int)
  distributed by (a) 
  partition by range (b)  
  (partition a1 start (0) end (5), 
   partition a2 end (10),  
   partition a3 end(15));

alter table mpp_5397 drop column c;

-- should work now
alter table mpp_5397 add partition z end (20);

-- ensure splitting default partition also works
alter table mpp_5397 add default partition adefault;
alter table mpp_5397 drop column d;
alter table mpp_5397 split default partition start (21) inclusive end (25) inclusive;

drop table mpp_5397;

-- MPP-4987 -- make sure we can't damage a partitioning configuration
-- this works
create table rank_damage (i int, j int)
partition by range(j) (start(1) end(5) every(1));
-- should all fail
alter table rank_damage_1_prt_1 no inherit rank_damage;
create table rank2_damage(like rank_damage);
alter table rank_damage_1_prt_1 inherit rank2_damage;
alter table rank_damage_1_prt_1 alter column i type bigint;
alter table rank_damage_1_prt_1 set without oids; -- no-op
alter table rank_damage add partition ppo end (22) with (oids = true);
drop table rank_damage, rank2_damage;

-- MPP-5831, type cast in SPLIT
CREATE TABLE sg_cal_event_silvertail_hour (
caldt date NOT NULL,
calhr smallint NOT NULL,
ip character varying(128),
transactionid character varying(32),
transactiontime timestamp(2) without time zone
)
WITH (appendonly=true, compresslevel=5)
distributed by (ip) PARTITION BY RANGE(transactiontime)
(

PARTITION "P2009041607"
START ('2009-04-16 07:00:00'::timestamp without time zone)
END ('2009-04-16 08:00:00'::timestamp without time zone),
PARTITION "P2009041608"
START ('2009-04-16 08:00:00'::timestamp without time zone)
END ('2009-04-16 09:00:00'::timestamp without time zone),
DEFAULT PARTITION st_default

);

ALTER TABLE SG_CAL_EVENT_SILVERTAIL_HOUR SPLIT DEFAULT PARTITION
START ('2009-04-29 07:00:00'::timestamp) INCLUSIVE END ('2009-04-29
08:00:00'::timestamp) EXCLUSIVE INTO ( PARTITION P2009042907 ,
PARTITION st_default );

\d+ sg_cal_event_silvertail_hour
\d+ sg_cal_event_silvertail_hour_1_prt_P2009042907

drop table sg_cal_event_silvertail_hour;

-- Make sure we inherit master's storage settings
create table foo_p (i int, j int, k text)
with (appendonly = true, compresslevel = 5)
partition by range(j) (start(1) end(10) every(1), default partition def);
insert into foo_p select i, i+1, repeat('fooo', 9000) from generate_series(1, 100) i;
alter table foo_p split default partition start (10) end(20) 
into (partition p10_20, default partition);
select c.oid::regclass, relkind, amname, reloptions from pg_class c left join pg_am am on am.oid = relam where c.oid = 'foo_p_1_prt_p10_20'::regclass or c.oid = 'foo_p'::regclass;
select count(distinct k) from foo_p;
drop table foo_p;

create table foo_p (i int, j int, k text)
partition by range(j) (start(1) end(10) every(1), default partition def
with(appendonly = true));
insert into foo_p select i, i+1, repeat('fooo', 9000) from generate_series(1, 100) i;
alter table foo_p split default partition start (10) end(20) 
into (partition p10_20, default partition);
select c.oid::regclass, relkind, amname, reloptions from pg_class c left join pg_am am on am.oid = relam where c.oid in ('foo_p_1_prt_p10_20'::regclass, 'foo_p_1_prt_def'::regclass);
select count(distinct k) from foo_p;
drop table foo_p;

-- MPP-5941: work with many levels of templates

CREATE TABLE mpp5941 (a int, b date, c char, 
	   		 		 d char(4), e varchar(20), f timestamp)
partition by range (b)
subpartition by list (a) 
subpartition template ( 
subpartition l1 values (1,2,3,4,5), 
subpartition l2 values (6,7,8,9,10) )
subpartition by list (e) 
subpartition template ( 
subpartition ll1 values ('Engineering'), 
subpartition ll2 values ('QA') )
subpartition by list (c) 
subpartition template ( 
subpartition lll1 values ('M'), 
subpartition lll2 values ('F') )
(
  start (date '2007-01-01')
  end (date '2010-01-01') every (interval '1 year')
);

-- just truncate for fun to see that everything is there
alter table mpp5941 alter partition for ('2008-01-01') 
alter partition for (1) alter partition for ('QA')
truncate partition for ('M');

alter table mpp5941 alter partition for ('2008-01-01') 
alter partition for (1) truncate partition for ('QA');

alter table mpp5941 alter partition for ('2008-01-01') 
truncate partition for (1);

alter table mpp5941 truncate partition for ('2008-01-01') ;

truncate table mpp5941;

-- now look at the templates that we have

select relid::regclass, level from gp_partition_template where relid = 'mpp5941'::regclass;

-- clear level 1

alter table mpp5941 set subpartition template ();

select relid::regclass, level from gp_partition_template where relid = 'mpp5941'::regclass;

-- clear level 2

alter table mpp5941 alter partition for ('2008-01-01') 
set subpartition template ();

select relid::regclass, level from gp_partition_template where relid = 'mpp5941'::regclass;

-- clear level 3

alter table mpp5941 alter partition for ('2008-01-01') 
alter partition for (1)
set subpartition template ();

select relid::regclass, level from gp_partition_template where relid = 'mpp5941'::regclass;

-- no level 4 (error)

alter table mpp5941 alter partition for ('2008-01-01') 
alter partition for (1) alter partition for ('QA')
set subpartition template ();

select relid::regclass, level from gp_partition_template where relid = 'mpp5941'::regclass;

-- no level 5 (error)

alter table mpp5941 alter partition for ('2008-01-01') 
alter partition for (1) alter partition for ('QA')
alter partition for ('M')
set subpartition template ();

select relid::regclass, level from gp_partition_template where relid = 'mpp5941'::regclass;

-- set level 1 (error, because no templates for level 2, 3)

alter table mpp5941 set subpartition template (
subpartition l1 values (1,2,3,4,5), 
subpartition l2 values (6,7,8,9,10) );

-- MPP-5992 - add deep templates correctly

-- Note: need to re-add the templates from deepest to shallowest,
-- because adding a template has a dependency on the existence of the
-- deeper template.

-- set level 3

alter table mpp5941 alter partition for ('2008-01-01') 
alter partition for (1)
set subpartition template (
subpartition lll1 values ('M'), 
subpartition lll2 values ('F') );

select relid::regclass, level from gp_partition_template where relid = 'mpp5941'::regclass;

-- set level 2

alter table mpp5941 alter partition for ('2008-01-01') 
set subpartition template (
subpartition ll1 values ('Engineering'), 
subpartition ll2 values ('QA') );

select relid::regclass, level from gp_partition_template where relid = 'mpp5941'::regclass;

-- set level 1

alter table mpp5941 set subpartition template (
subpartition l1 values (1,2,3,4,5), 
subpartition l2 values (6,7,8,9,10) );

select relid::regclass, level from gp_partition_template where relid = 'mpp5941'::regclass;


drop table mpp5941;

-- MPP-5984 - NULL is not allowed in RANGE partition
CREATE TABLE partsupp ( ps_partkey integer,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_partkey)
(
partition nnull start (NULL) end (300)
);
CREATE TABLE partsupp ( ps_partkey integer,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_partkey)
(
partition nnull start (300) end (NULL)
);
CREATE TABLE partsupp ( ps_partkey integer,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_partkey)
(
partition nnull start (300) end (NULL::int)
);
CREATE TABLE partsupp ( ps_partkey integer,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_partkey)
(
partition p1 start(1) end(10),
partition p2 start(10) end(20),
default partition def
);
alter table partsupp split partition p2 at (NULL);
alter table partsupp split default partition start(null) end(200);
drop table partsupp;
CREATE TABLE partsupp ( ps_partkey integer,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_partkey)
(
partition nnull start (300) end (400)
);
alter table partsupp add partition foo start(500) end(NULL);
drop table partsupp;

-- Test for an old bug, where we used to crash on NULLs, because the code
-- to order the partitions by their start/end boundaries did not anticipate
-- NULLs. NULLs in boundaries are not accepted, but because we check for
-- them only after ordering the partitions, the sorting code needs to
-- handle them. (This test needs at least two partitions, so that there
-- is something to sort.)
create table partnulltest (
  col1 int,
  col2 numeric
)
distributed by (col1)
partition by range(col2)
(
  partition part2 start(1) end(10) ,
  partition part1 start (NULL)
);

-- Test pg_dump on a somewhat complicated partitioned table. (MPP-6240)
-- We actually just create the table here, and leave it behind, so that
-- it gets dumped by pg_dump at end of the regression test suite.
CREATE TABLE supplier_hybrid_part(
                S_SUPPKEY INTEGER,
                S_NAME CHAR(25),
                S_ADDRESS VARCHAR(40),
                S_NATIONKEY INTEGER,                S_PHONE CHAR(15),
                S_ACCTBAL decimal,
                S_COMMENT VARCHAR(101)
                )
partition by range (s_suppkey) 
subpartition by list (s_nationkey) subpartition template (
    values('22','21','17'),
    values('6','11','1','7','16','2') WITH (checksum=false,appendonly=true,blocksize=1171456,         compresslevel=3),
    values('18','20'),
    values('9','23','13') WITH (checksum=true,appendonly=true,blocksize=1335296,compresslevel=7),
    values('0','3','12','15','14','8','4','24','19','10','5')
)               
(               
partition p1 start('1') end('10001') every(10000)
);

-- MPP-3544
-- Domain
create domain domainvarchar varchar(5);
create domain domainnumeric numeric(8,2);
create domain domainint4 int4;
create domain domaintext text;

-- Test tables using domains
-- list
create table basictest1
           ( testint4 domainint4
           , testtext domaintext
           , testvarchar domainvarchar
           , testnumeric domainnumeric
           )
partition by LIST(testvarchar)
(
partition aa values ('aaaaa'),
partition bb values ('bbbbb'),
partition cc values ('ccccc')
);

alter table basictest1 add partition dd values('ddddd');
insert into basictest1 values(1, 1, 'ddddd', 1);
insert into basictest1 values(1, 1, 'ccccc', 1);
insert into basictest1 values(1, 1, 'bbbbb', 1);
insert into basictest1 values(1, 1, 'aaaaa', 1);
drop table basictest1;
--range
create table basictest1 (testnumeric domainint4)
partition by range(testnumeric)
 (start(1) end(10) every(5));
insert into basictest1 values(1);
insert into basictest1 values(2);
alter table basictest1 add partition ff start(10) end(20);
insert into basictest1 values(10);
drop table basictest1;
drop domain domainvarchar, domainnumeric, domainint4, domaintext;

-- Test index inheritance with partitions
create table ti (i int not null, j int)
distributed by (i)
partition by range (j) 
(start(1) end(3) every(1));
create unique index ti_pkey on ti(i,j);

select * from pg_indexes where schemaname = 'public' and tablename like 'ti%';
create index ti_j_idx on ti using bitmap(j);
select * from pg_indexes where schemaname = 'public' and tablename like 'ti%';
alter table ti add partition p3 start(3) end(10);
select * from pg_indexes where schemaname = 'public' and tablename like 'ti%';
-- Should not be able to drop child indexes added implicitly via ADD PARTITION
drop index ti_1_prt_p3_i_j_idx;
drop index ti_1_prt_p3_j_idx;
alter table ti split partition p3 at (7) into (partition pnew1, partition pnew2);
select * from pg_indexes where schemaname = 'public' and tablename like 'ti%';
-- Should not be able to drop child indexes added implicitly via SPLIT PARTITION
drop index ti_1_prt_pnew1_i_j_idx;
drop index ti_1_prt_pnew1_j_idx;
drop index ti_1_prt_pnew2_i_j_idx;
drop index ti_1_prt_pnew2_j_idx;
-- Index drop should cascade to all partitions- including those later added via
-- ADD PARTITION or SPLIT PARTITION
drop index ti_pkey;
drop index ti_j_idx;
select * from pg_indexes where schemaname = 'public' and tablename like 'ti%';
drop table ti;

-- Partitioned table with btree index and hash aggregate should use a correct
-- memory context for its tuples` descriptor
drop table if exists dis_tupdesc;
create table dis_tupdesc (a int, b int, c int)
distributed by (a)
partition by list (b)
(
    partition p1 values (1),
    partition p2 values (2),
    default partition junk_data
);
create index dis_tupdesc_idx on dis_tupdesc using btree (c);
insert into dis_tupdesc select i, i % 3, i % 4 from generate_series (1, 240) as i;
analyze dis_tupdesc;
set gp_segments_for_planner = 2;
set optimizer_segments = 2;
select distinct b from dis_tupdesc where c >= 2;
reset gp_segments_for_planner;
reset optimizer_segments;

-- MPP-6611, make sure rename works with default partitions
create table it (i int, j int) partition by range(i) 
subpartition by range(j) subpartition template(start(1) end(10) every(5))
(start(1) end(3) every(1));
alter table it rename to newit;
select schemaname, tablename from pg_tables where schemaname = 'public' and tablename like 'newit%';
alter table newit add default partition def;
select schemaname, tablename from pg_tables where schemaname = 'public' and tablename like 'newit%';
alter table newit rename to anotherit;
select schemaname, tablename from pg_tables where schemaname = 'public' and tablename like
'anotherit%';
drop table anotherit;

--
-- Test table constraint inheritance
--
-- with a named UNIQUE constraint
create table it (i int) distributed by (i) partition by range(i) (start(1) end(3) every(1));
select schemaname, tablename, indexname from pg_indexes where schemaname = 'public' and tablename like 'it%';
alter table it add constraint it_unique_i unique (i);
select schemaname, tablename, indexname from pg_indexes where schemaname = 'public' and tablename like 'it%';
alter table it drop constraint it_unique_i;
select schemaname, tablename, indexname from pg_indexes where schemaname = 'public' and tablename like 'it%';
drop table it;

-- with a PRIMARY KEY constraint, without giving it a name explicitly.
create table it (i int) distributed by (i) partition by range(i) (start(1) end(3) every(1));
select schemaname, tablename, indexname from pg_indexes where schemaname = 'public' and tablename like 'it%';
alter table it add primary key(i);
select schemaname, tablename, indexname from pg_indexes where schemaname = 'public' and tablename like 'it%';
-- FIXME: dropping a primary key doesn't currently work correctly. It doesn't
-- drop the key on the partitions, only the parent. See
-- https://github.com/greenplum-db/gpdb/issues/3750
--
-- alter table it add primary key(i);
-- select schemaname, tablename, indexname from pg_indexes where schemaname = 'public' and tablename like 'it%';
drop table it;


create table it (i int) distributed by (i) partition by range(i) (start(1) end(3) every(1));
select schemaname, tablename, indexname from pg_indexes where schemaname = 'public' and tablename like 'it%';
alter table it add primary key(i);
select schemaname, tablename, indexname from pg_indexes where schemaname = 'public' and tablename like 'it%';
drop table it;

--
-- Exclusion constraints are currently not supported on partitioned tables.
--
create table parttab_with_excl_constraint (
  i int,
  j int,
  CONSTRAINT part_excl EXCLUDE (i WITH =) )
distributed by (i) partition by list (i) (
  partition a values (1),
  partition b values (2),
  partition c values (3)
);

create table parttab_with_excl_constraint (
  i int,
  j int)
distributed by (i) partition by list (i) (
  partition a values (1),
  partition b values (2),
  partition c values (3)
);
alter table parttab_with_excl_constraint ADD CONSTRAINT part_excl EXCLUDE (i WITH =);

drop table parttab_with_excl_constraint;


-- MPP-6297: test special WITH(tablename=...) syntax for dump/restore

-- original table was:
-- PARTITION BY RANGE(l_commitdate) 
-- (
--     PARTITION p1 
--       START ('1992-01-31'::date) END ('1995-04-30'::date) 
--       EVERY ('1 year 1 mon'::interval)
-- )

-- dump used to give a definition like this:

-- without the WITH(tablename=...), the vagaries of EVERY arithmetic
-- create >3 partitions
CREATE TABLE mpp6297 ( l_orderkey bigint,
l_commitdate date
)
distributed BY (l_orderkey) PARTITION BY RANGE(l_commitdate)
(
PARTITION p1_1 START ('1992-01-31'::date) END ('1993-02-28'::date)
EVERY ('1 year 1 mon'::interval)
,
PARTITION p1_2 START ('1993-02-28'::date) END ('1994-03-31'::date)
EVERY ('1 year 1 mon'::interval)
,
PARTITION p1_3 START ('1994-03-31'::date) END ('1995-04-30'::date)
EVERY ('1 year 1 mon'::interval)
);

\d+ mpp6297
drop table mpp6297;


-- when WITH(tablename=...) is specified, the EVERY is stored as an
-- attribute, but not expanded into additional partitions
CREATE TABLE mpp6297 ( l_orderkey bigint,
l_commitdate date
)
distributed BY (l_orderkey) PARTITION BY RANGE(l_commitdate)
(
PARTITION p1_1 START ('1992-01-31'::date) END ('1993-02-28'::date)
EVERY ('1 year 1 mon'::interval)
WITH (tablename='mpp6297_1_prt_p1_1'),
PARTITION p1_2 START ('1993-02-28'::date) END ('1994-03-31'::date)
EVERY ('1 year 1 mon'::interval)
WITH (tablename='mpp6297_1_prt_p1_2'),
PARTITION p1_3 START ('1994-03-31'::date) END ('1995-04-30'::date)
EVERY ('1 year 1 mon'::interval)
WITH (tablename='mpp6297_1_prt_p1_3')
);

\d+ mpp6297
drop table mpp6297;

-- more with basic cases
create table mpp6297 
(a int, 
b int) 
partition by range (b)
(
start (1) end (10) every (1),
end (11)		
);

\d+ mpp6297

alter table mpp6297 drop partition for (3);
\d+ mpp6297

-- this isn't legal (but it would be nice)
alter table mpp6297 add partition start (3) end (4) every (1);

-- this is legal but it doesn't fix the EVERY clause
alter table mpp6297 add partition start (3) end (4) ;

\d+ mpp6297
drop table mpp6297;

-- similarly, we can merge adjacent EVERY clauses if they match

create table mpp6297 
(a int, 
b int) 
partition by range (b)
(
start (1) end (5) every (1),
start (5) end (10) every (1)
);

\d+ mpp6297
drop table mpp6297;

-- we cannot merge adjacent EVERY clauses if inclusivity/exclusivity is wrong
create table mpp6297 
(a int, 
b int) 
partition by range (b)
(
start (1) end (5) every (1),
start (5) exclusive end (10) every (1)
);

\d+ mpp6297
drop table mpp6297;

-- more fun with inclusivity/exclusivity (normal case)
create table mpp6297 
(a int, 
b int) 
partition by range (b)
(
start (1) inclusive end (10) exclusive every (1)
);

\d+ mpp6297
drop table mpp6297;

-- more fun with inclusivity/exclusivity (abnormal case)
create table mpp6297 
(a int, 
b int) 
partition by range (b)
(
start (1) exclusive end (10) inclusive every (1)
);

\d+ mpp6297

alter table mpp6297 drop partition for (3);

\d+ mpp6297
drop table mpp6297;

-- we cannot merge adjacent EVERY clauses, even though the
-- inclusivity/exclusivity matches, because it is different from the
-- normal start inclusive/end exclusive
create table mpp6297 
(a int, 
b int) 
partition by range (b)
(
start (1) end (5) inclusive every (1),
start (5) exclusive end (10) every (1)
);

\d+ mpp6297
drop table mpp6297;

-- MPP-6589: SPLITting an "open" ended partition (ie, no start or end)

CREATE TABLE mpp6589a
(
  id bigint,
  day_dt date
)
DISTRIBUTED BY (id)
PARTITION BY RANGE(day_dt)
          (
          PARTITION p20090312  END ('2009-03-12'::date)
          );

select relname, pg_get_expr(relpartbound, oid, false) from pg_class where relname like 'mpp6589a%';

-- should work
ALTER TABLE mpp6589a 
SPLIT PARTITION p20090312 AT( '20090310' ) 
INTO( PARTITION p20090309, PARTITION p20090312_tmp);

select relname, pg_get_expr(relpartbound, oid, false) from pg_class where relname like 'mpp6589a%';

CREATE TABLE mpp6589i(a int, b int) 
partition by range (b) (start (1) end (3));
select relname, pg_get_expr(relpartbound, oid, false) from pg_class where relname like 'mpp6589i%';

-- should fail (overlap)
ALTER TABLE mpp6589i ADD PARTITION start (2);
-- should work (there's a "point" hole at value 2)
ALTER TABLE mpp6589i ADD PARTITION start (3) exclusive;
select relname, pg_get_expr(relpartbound, oid, false) from pg_class where relname like 'mpp6589i%';

-- test open-ended SPLIT
CREATE TABLE mpp6589b
(
  id bigint,
  day_dt date
)
DISTRIBUTED BY (id)
PARTITION BY RANGE(day_dt)
          (
          PARTITION p20090312  START ('2008-03-12'::date)
          );

select relname, pg_get_expr(relpartbound, oid, false) from pg_class where relname like 'mpp6589b%';

-- should work
ALTER TABLE mpp6589b 
SPLIT PARTITION p20090312 AT( '20090310' ) 
INTO( PARTITION p20090309, PARTITION p20090312_tmp);

select relname, pg_get_expr(relpartbound, oid, false) from pg_class where relname like 'mpp6589b%';

-- MPP-7191, MPP-7193: partitioned tables - fully-qualify storage type
-- if not specified (and not a template)
CREATE TABLE mpp5992 (a int, b date, c char,
                     d char(4), e varchar(20), f timestamp)
WITH (orientation=column,appendonly=true)
partition by range (b)
subpartition by list (a)
subpartition template (
subpartition l1 values (1,2,3,4,5),
subpartition l2 values (6,7,8,9,10) )
subpartition by list (e)
subpartition template (
subpartition ll1 values ('Engineering'),
subpartition ll2 values ('QA') )
subpartition by list (c)
subpartition template (
subpartition lll1 values ('M'),
subpartition lll2 values ('F') )
(
  start (date '2007-01-01')
  end (date '2010-01-01') every (interval '1 year')
);

-- Delete subpartition template
alter table mpp5992 alter partition for ('2008-01-01')
set subpartition template ();
alter table mpp5992 alter partition for ('2008-01-01')
alter partition for (1)
set subpartition template ();
alter table mpp5992 set subpartition template ();

-- Add subpartition template
alter table mpp5992 alter partition for ('2008-01-01')
alter partition for (1)
set subpartition template ( subpartition lll1 values ('M'),
subpartition lll2 values ('F'));

alter table mpp5992 alter partition for ('2008-01-01')
set subpartition template (
subpartition ll1 values ('Engineering'),
subpartition ll2 values ('QA')
);
alter table mpp5992 
set subpartition template (subpartition l1 values (1,2,3,4,5), 
subpartition l2 values (6,7,8,9,10) );
alter table mpp5992 
set subpartition template (subpartition l1 values (1,2,3), 
subpartition l2 values (4,5,6), subpartition l3 values (7,8,9,10));
select relid::regclass, level, pg_get_expr(template, relid) from gp_partition_template where relid = 'mpp5992'::regclass;

-- Now we can add a new partition
alter table mpp5992 
add partition foo1 
start (date '2011-01-01') 
end (date '2012-01-01'); -- should inherit from parent storage option

alter table mpp5992 
add partition foo2 
start (date '2012-01-01') 
end (date '2013-01-01') WITH (orientation=column,appendonly=true);

alter table mpp5992 
add partition foo3 
start (date '2013-01-01') end (date '2014-01-01') WITH (appendonly=true);

select * from pg_partition_tree('mpp5992');
select relname, relam, pg_get_expr(relpartbound, oid) from pg_class where relname like 'mpp5992%';
select relid::regclass, level, pg_get_expr(template, relid) from gp_partition_template where relid = 'mpp5992'::regclass;

-- MPP-10223: split subpartitions
CREATE TABLE MPP10223pk
(
rnc VARCHAR(100),
wbts VARCHAR(100),
axc VARCHAR(100),
vptt VARCHAR(100),
vcct VARCHAR(100),
agg_level CHAR(5),
period_start_time TIMESTAMP WITH TIME ZONE,
load_time TIMESTAMP WITH TIME ZONE DEFAULT now(),
interval INTEGER,
totcellsegress double precision,
totcellsingress double precision,
 
  CONSTRAINT "axc_vcct1_atmvcct_pk_test2" 
PRIMARY KEY (rnc,wbts,axc,vptt,vcct,agg_level,period_start_time)
)
 
DISTRIBUTED BY (rnc,wbts,axc,vptt,vcct)
 
PARTITION BY LIST (AGG_LEVEL)
  SUBPARTITION BY RANGE (PERIOD_START_TIME)
(
  PARTITION min15part  VALUES ('15min')
    (
       SUBPARTITION P_FUTURE  START (date '2001-01-01') INCLUSIVE,
       SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                              END (date '2999-12-31') EXCLUSIVE
    ),
  PARTITION hourpart   VALUES ('hour')
    (
               SUBPARTITION P20100622 START (date '2010-06-22') INCLUSIVE,
               SUBPARTITION P20100623 START (date '2010-06-23') INCLUSIVE,
               SUBPARTITION P20100624 START (date '2010-06-24') INCLUSIVE,
               SUBPARTITION P20100625 START (date '2010-06-25') INCLUSIVE,
               SUBPARTITION P20100626 START (date '2010-06-26') INCLUSIVE,
               SUBPARTITION P_FUTURE  START (date '2001-01-01') INCLUSIVE,
               SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                                      END (date '2999-12-31') EXCLUSIVE
    ),
  PARTITION daypart    VALUES ('day')
    (
               SUBPARTITION P20100622 START (date '2010-06-22') INCLUSIVE,
               SUBPARTITION P20100623 START (date '2010-06-23') INCLUSIVE,
               SUBPARTITION P20100624 START (date '2010-06-24') INCLUSIVE,
               SUBPARTITION P20100625 START (date '2010-06-25') INCLUSIVE,
               SUBPARTITION P20100626 START (date '2010-06-26') INCLUSIVE,
               SUBPARTITION P_FUTURE  START (date '2001-01-01') INCLUSIVE,
               SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                                      END (date '2999-12-31') EXCLUSIVE
    )
);

-- MPP-10421: works -- can re-use name for non-DEFAULT partitions, and
-- primary key problems fixed
ALTER TABLE MPP10223pk
 ALTER PARTITION min15part 
SPLIT PARTITION  P_FUTURE AT ('2010-06-25') 
INTO (PARTITION P20010101, PARTITION P_FUTURE);
drop table mpp10223pk;
-- rebuild the table without a primary key
CREATE TABLE MPP10223
(
rnc VARCHAR(100),
wbts VARCHAR(100),
axc VARCHAR(100),
vptt VARCHAR(100),
vcct VARCHAR(100),
agg_level CHAR(5),
period_start_time TIMESTAMP WITH TIME ZONE,
load_time TIMESTAMP WITH TIME ZONE DEFAULT now(),
interval INTEGER,
totcellsegress double precision,
totcellsingress double precision
)
 
DISTRIBUTED BY (rnc,wbts,axc,vptt,vcct)
 
PARTITION BY LIST (AGG_LEVEL)
  SUBPARTITION BY RANGE (PERIOD_START_TIME)
(
  PARTITION min15part  VALUES ('15min')
    (
       SUBPARTITION P_FUTURE  START (date '2001-01-01') INCLUSIVE,
       SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                              END (date '2999-12-31') EXCLUSIVE
    ),
  PARTITION hourpart   VALUES ('hour')
    (
               SUBPARTITION P20100622 START (date '2010-06-22') INCLUSIVE,
               SUBPARTITION P20100623 START (date '2010-06-23') INCLUSIVE,
               SUBPARTITION P20100624 START (date '2010-06-24') INCLUSIVE,
               SUBPARTITION P20100625 START (date '2010-06-25') INCLUSIVE,
               SUBPARTITION P20100626 START (date '2010-06-26') INCLUSIVE,
               SUBPARTITION P_FUTURE  START (date '2001-01-01') INCLUSIVE,
               SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                                      END (date '2999-12-31') EXCLUSIVE
    ),
  PARTITION daypart    VALUES ('day')
    (
               SUBPARTITION P20100622 START (date '2010-06-22') INCLUSIVE,
               SUBPARTITION P20100623 START (date '2010-06-23') INCLUSIVE,
               SUBPARTITION P20100624 START (date '2010-06-24') INCLUSIVE,
               SUBPARTITION P20100625 START (date '2010-06-25') INCLUSIVE,
               SUBPARTITION P20100626 START (date '2010-06-26') INCLUSIVE,
               SUBPARTITION P_FUTURE  START (date '2001-01-01') INCLUSIVE,
               SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                                      END (date '2999-12-31') EXCLUSIVE
    )
);

-- this works
ALTER TABLE MPP10223
 ALTER PARTITION min15part 
SPLIT PARTITION  P_FUTURE AT ('2010-06-25') 
INTO (PARTITION P20010101, PARTITION P_FUTURE2);

select * from pg_partition_tree('mpp10223');
select relname, pg_get_expr(relpartbound, oid) from pg_class where relname like 'mpp10223%';

-- simpler version
create table mpp10223b (a int, b int , d int)
partition by range (b)
subpartition by range (d)
(partition p1 start (1) end (10)
(subpartition sp2 start (20) end (30)));

-- MPP-10421: allow re-use sp2 for non-DEFAULT partition
alter table mpp10223b alter partition p1 
split partition for (20) at (25)
into (partition sp2, partition sp3);

select relname, pg_get_expr(relpartbound, oid) from pg_class where relname like 'mpp10223b%';

-- MPP-10480: dump templates (but don't use "foo")
create table MPP10480 (a int, b int, d int)
partition by range (b)
subpartition by range(d)
subpartition template (start (1) end (10) every (1))
(start (20) end (30) every (1));

select relid::regclass, level, pg_get_expr(template, relid) from gp_partition_template where relid = 'MPP10480'::regclass;

-- MPP-10421: fix SPLIT of partitions with PRIMARY KEY constraint/indexes
CREATE TABLE mpp10321a
(
        rnc VARCHAR(100),
        wbts VARCHAR(100),
        axc VARCHAR(100),
        vptt VARCHAR(100),
        vcct VARCHAR(100),
        agg_level CHAR(5),
        period_start_time TIMESTAMP WITH TIME ZONE,
        load_time TIMESTAMP WITH TIME ZONE DEFAULT now(),
        interval INTEGER,
        totcellsegress double precision,
        totcellsingress double precision,

  CONSTRAINT "mpp10321a_pk"
PRIMARY KEY (rnc,wbts,axc,vptt,vcct,agg_level,period_start_time)
)

DISTRIBUTED BY (rnc,wbts,axc,vptt,vcct)

PARTITION BY LIST (AGG_LEVEL)
  SUBPARTITION BY RANGE (PERIOD_START_TIME)
(
  PARTITION min15part  VALUES ('15min')
    (
         SUBPARTITION P_FUTURE  START (date '2001-01-01') INCLUSIVE,
         SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                                END (date '2999-12-31') EXCLUSIVE
    ),
  PARTITION hourpart   VALUES ('hour')
    (
          SUBPARTITION P20100622 START (date '2010-06-22') INCLUSIVE,
          SUBPARTITION P_ENDPART START (date '2999-12-30') INCLUSIVE
                                 END (date '2999-12-31') EXCLUSIVE
    )
);

ALTER TABLE mpp10321a
ALTER PARTITION min15part
SPLIT PARTITION  P_FUTURE AT ('2010-06-25')
INTO (PARTITION P20010101, PARTITION P_FUTURE);

DROP TABLE mpp10321a;

-- test for default partition with boundary spec
create table bhagp_range (a int, b int) 
distributed by (a) 
partition by range (b) 
( 
  default partition x 
  start (0) inclusive 
  end (2) exclusive 
  every (1) 
);

create table bhagp_list (a int, b int) 
distributed by (a) 
partition by list (b) 
( 
  default partition x 
  values (1,2)
);

-- more coverage tests

-- bad partition by type
create table cov1 (a int, b int)
distributed by (a)
partition by (b)
(
start (1) end (10) every (1)
);

-- bad partition by type
create table cov1 (a int, b int)
distributed by (a)
partition by funky (b)
(
start (1) end (10) every (1)
);

drop table cov1;

create table cov1 (a int, b int)
distributed by (a)
partition by range (b)
(
start (1) end (10) every (1)
);

-- syntax error
alter table cov1 drop partition for (funky(1));

-- no rank for default
alter table cov1 drop default partition for (1);

-- no default
alter table cov1 split default partition at (9);
alter table cov1 drop default partition;

-- cannot add except by name
alter table cov1 add partition for (1);

-- bad template
alter table cov1 set subpartition template (values (1,2) (values (2,3)));

-- create and drop default partition in one statement!
alter table cov1 add default partition def1, drop default partition;

drop table cov1;

-- every 5 (1) now disallowed...
create table cov1 (a int, b int)
distributed by (a)
partition by range (b)
(
start (1) end(20) every 5 (1)
);

drop table if exists cov1;

create table cov1 (a int, b int)
distributed by (a)
partition by list (b)
(
partition p1 values (1,2,3,4,5,6,7,8)
);

-- bad split
alter table cov1 split partition p1 at (5,50);

-- good split
alter table cov1 split partition p1 at (5,6,7) 
into (partition p1, partition p2);

select relname, pg_get_expr(relpartbound, oid, false) from pg_class where relname like 'cov1%';

drop table cov1;

-- MPP-11120
--  ADD PARTITION didn't explicitly specify the distribution policy in the
-- CreateStmt distributedBy field and as such we followed the behaviour encoded
-- in transformDistributedBy(). Unfortunately, it chooses to set the
-- distribution policy to that of the primary key if the distribution policy
-- is not explicitly set.
create table tbl11120 (
	a	int,
	b	int,
	c	int,
	primary key (a,b,c)
)
distributed by (a)
partition by range (b)
(
	default partition default_partition,
	partition p1 start (1) end (2)
);

insert into tbl11120 values(1,2,3);

select * from tbl11120; -- expected: (1,2,3)

delete from tbl11120 where a=1 and b=2 and c=3; -- this should delete the row in tbl11120

select * from tbl11120; -- expected, no rows

insert into tbl11120 values(1,2,3); -- reinsert data

-- all partitions should have same distribution policy
select relname, distkey as distribution_attributes from
gp_distribution_policy p, pg_class c
where p.localoid = c.oid and relname like 'tbl11120%' order by p.localoid;

alter table tbl11120 split default partition
        start (3)
	end (4)
	into (partition p2, default partition);


select relname, distkey as distribution_attributes from
gp_distribution_policy p, pg_class c where p.localoid = c.oid and 
relname like 'tbl11120%' order by p.localoid;

delete from tbl11120 where a=1 and b=2 and c=3; -- this should delete the row in tbl11120

select * from tbl11120; -- expected, no rows! But we see the row. Wrong results!

alter table tbl11120 drop partition default_partition;

alter table tbl11120 add partition foo start(10) end(20);

select relname, distkey as distribution_attributes from
gp_distribution_policy p, pg_class c where p.localoid = c.oid and
relname like 'tbl11120%' order by p.localoid;

drop table tbl11120;

-- MPP-6979: EXCHANGE partitions - fix namespaces if they differ

-- new schema
create schema mpp6979dummy;

create table mpp6979part(a int, b int) 
partition by range(b) 
(
start (1) end (10) every (1)
);

-- append-only table in new schema 
create table mpp6979dummy.mpp6979tab(like mpp6979part) with (appendonly=true);

-- check that table and all parts in public schema
select relname, nspname, relispartition from pg_class c, pg_namespace n
where c.relnamespace = n.oid and relname like ('mpp6979%');

-- note that we have heap partitions in public, and ao table in mpp6979dummy
select nspname, relname, amname
from pg_class pc
inner join pg_namespace ns on pc.relnamespace=ns.oid
left join pg_am am on am.oid = pc.relam
where relname like ('mpp6979%');

-- exchange the partition with the ao table.  
-- Now we have an ao partition and mpp6979tab is heap!
alter table mpp6979part exchange partition for (1) 
with table mpp6979dummy.mpp6979tab;

-- after the exchange, all partitions are still in public
select relname, nspname, relispartition from pg_class c, pg_namespace n
where c.relnamespace = n.oid and relname like ('mpp6979%');

-- the rank 1 partition is ao, but still in public, and 
-- table mpp6979tab is now heap, but still in mpp6979dummy
select relname, nspname, relispartition, amname
from pg_class c inner join pg_namespace n on c.relnamespace = n.oid
left join pg_am am on am.oid = c.relam
where relname like ('mpp6979%');

drop table mpp6979part;
drop table mpp6979dummy.mpp6979tab;

drop schema mpp6979dummy;

-- MPP-7898:

create table parent_s
    (a int, b text) 
    distributed by (a);
    
insert into parent_s values
    (1, 'one');

-- Try to create a table that mixes inheritance and partitioning.
-- Correct behavior: ERROR

create table child_r
    ( c int, d int) 
    inherits (parent_s)
    partition by range(d) 
    (
        start (0) 
        end (2) 
        every (1)
    );

 -- If (incorrectly) the previous statement works, the next one is 
 -- likely to fail with in unexpected internal error.  This is residual 
 -- issue MPP-7898.
insert into child_r values
    (0, 'from r', 0, 0);

drop table if exists parent_s cascade; --ignore
drop table if exists child_r cascade; --ignore

create table parent_r
    ( a int, b text, c int, d int ) 
    distributed by (a)
    partition by range(d) 
    (
        start (0) 
        end (2) 
        every (1)
    );
 
insert into parent_r values
    (0, 'from r', 0, 0);

create table parent_s
    ( a int, b text, c int, d int ) 
    distributed by (a);
    
insert into parent_s values
    (1, 'from s', 555, 555);

create table child_t
    ( )
    inherits (parent_s)
    distributed by (a);

insert into child_t values
    (0, 'from t', 666, 666);

-- Try to exchange in the child and parent.  
-- Correct behavior: ERROR in both cases.

alter table parent_r exchange partition for (1) with table child_t;
alter table parent_r exchange partition for (1) with table parent_s;

drop table child_t cascade; --ignore
drop table parent_s cascade; --ignore
drop table parent_r cascade; --ignore

-- MPP-7898 end.

-- ( MPP-13750 

CREATE TABLE s (id int, date date, amt decimal(10,2), units int) 
DISTRIBUTED BY (id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2008-01-02') EXCLUSIVE 
   EVERY (INTERVAL '1 day') );

create index s_i on s(amt) 
  where (id > 1)
  ;

create index s_j on s(units)
  where (id <= 1)
  ;

create index s_i_expr on s(log(units));

alter table s add partition s_test 
    start(date '2008-01-03') end (date '2008-01-05');

alter table s split partition for (date '2008-01-03') at (date '2008-01-04')
  into (partition s_test, partition s_test2);

select 
    relname, 
    (select count(distinct content) - 1 
     from gp_segment_configuration) - count(*) as missing, 
    count(distinct relid) oid_count 
from (
    select gp_execution_segment(), oid, relname 
    from gp_dist_random('pg_class') 
    ) seg_class(segid, relid, relname) 
where relname ~ '^s_' 
group by relname; 

drop table s cascade;

--   MPP-13750 )

-- MPP-13806 start
drop table if exists mpp13806;
 CREATE TABLE mpp13806 (id int, date date, amt decimal(10,2))
 DISTRIBUTED BY (id)
 PARTITION BY RANGE (date)
 ( START (date '2008-01-01') INCLUSIVE
	END (date '2008-01-05') EXCLUSIVE
	EVERY (INTERVAL '1 day') );
 
-- Adding unbound partition right before the start  used to fail
alter table mpp13806 add partition test end (date '2008-01-01') exclusive;
 
drop table if exists mpp13806;
 CREATE TABLE mpp13806 (id int, date date, amt decimal(10,2))
 DISTRIBUTED BY (id)
 PARTITION BY RANGE (date)
 ( START (date '2008-01-01') EXCLUSIVE
	END (date '2008-01-05') EXCLUSIVE
	EVERY (INTERVAL '1 day') );
-- For good measure, test the opposite case
alter table mpp13806 add partition test end (date '2008-01-01') inclusive;
drop table mpp13806;
-- MPP-13806 end

-- MPP-14471 start
-- No unenforceable PK/UK constraints!  (UNIQUE INDEXes still allowed; tested above)
drop table if exists tc cascade;
drop table if exists cc cascade;
drop table if exists at cascade;

create table tc
    (a int, b int, c int, primary key(a) )
    distributed by (a)
    partition by range (b)
    ( 
        default partition d,
        start (0) inclusive end(100) inclusive every (50)
    );

create table cc
    (a int primary key, b int, c int)
    distributed by (a)
    partition by range (b)
    ( 
        default partition d,
        start (0) inclusive end(100) inclusive every (50)
    );

create table at
    (a int, b int, c int)
    distributed by (a)
    partition by range (b)
    ( 
        default partition d,
        start (0) inclusive end(100) inclusive every (50)
    );

alter table at
    add primary key (a);

-- MPP-14471 end
-- MPP-17606 (using table "at" from above)

alter table at
    alter column b
	type numeric;
	
-- MPP-17606 end
-- MPP-17707 start
create table mpp17707
( d int, p int ,x text)
with (appendonly = true)
distributed by (d)
partition by range (p)
(start (0) end (3) every (2));

-- Create a expression index on the partitioned table

create index idx_abc on mpp17707(upper(x));

-- split partition 1 of table

alter table mpp17707 split partition for (0) at (1)
	into (partition x1, partition x2);
-- MPP-17707 end
-- MPP-17814 start
drop table if exists plst2 cascade;
-- positive; bug was that it failed whereas it should succeed
create type plst2_partkey as (a integer, c integer);
create table plst2
    (            
        b integer not null,
	d plst2_partkey
    )                                                                                                                   
    distributed by (b) 
    partition by list (d)
    (
        partition p1 values ( CAST('(1,2)' as plst2_partkey), CAST('(3,4)' as plst2_partkey) ),
        partition p2 values ( CAST('(5,6)' as plst2_partkey) ),
        partition p3 values ( CAST('(2,1)' as plst2_partkey) )
    );
\d+ plst2
drop table if exists plst2 cascade;
--negative; test legitimate failure
create table plst2
    (            
        b integer not null,
	d plst2_partkey
    )                                                                                                                   
    distributed by (b) 
    partition by list (d)
    (
        partition p1 values ( CAST('(1,2)' as plst2_partkey), CAST('(3,4)' as plst2_partkey) ),
        partition p2 values ( CAST('(5,6)' as plst2_partkey) ),
        partition p3 values ( CAST('(1,2)' as plst2_partkey) )
    );

-- postive; make sure inner part duplicates are accepted and quietly removed.
drop table if exists plst2;
drop type plst2_partkey;
create type plst2_partkey as (a int, b int);
create table plst2
    ( d int, c plst2_partkey)
    distributed by (d)
    partition by list (c) 
        (
        partition p0 values ( CAST('(1,2)' as plst2_partkey), CAST('(3,4)' as plst2_partkey) ),
        partition p1 values ( CAST('(4,3)' as plst2_partkey), CAST('(2,1)' as plst2_partkey) ),
        partition p2 values ( CAST('(4,4)' as plst2_partkey), CAST('(5,5)' as plst2_partkey), CAST('(4,4)' as plst2_partkey), CAST('(5,5)' as plst2_partkey), CAST('(4,4)' as plst2_partkey), CAST('(5,5)' as plst2_partkey)),
	partition p3 values (CAST('(4,5)' as plst2_partkey), CAST('(5,6)' as plst2_partkey))
        );

-- positive; make sure legitimate alters work.
alter table plst2 add partition p4 values (CAST('(5,4)' as plst2_partkey), CAST('(6,5)' as plst2_partkey));
alter table plst2 add partition p5 values (CAST('(7,8)' as plst2_partkey), CAST('(7,8)' as plst2_partkey));
\d+ plst2

-- negative; make sure conflicting alters fail.
alter table plst2 add partition p6 values (CAST('(7,8)' as plst2_partkey), CAST('(2,1)' as plst2_partkey));

drop table if exists plst2;


-- MPP-17814 end

-- MPP-18441
create table s_heap (i1 int, t1 text, t2 text, i2 int, i3 int, n1 numeric, b1 bool)
partition by list (t1)
     (partition abc values('abc0', 'abc1', 'abc2'));
insert into s_heap (t1, i1, i2, i3, n1, b1) select 'abc0', 1, 1, 1, 2.3, true
    from generate_series(1, 5);
alter table s_heap drop column t2;
alter table s_heap drop column i3;
-- create co table for exchange
create table s_heap_ex_abc (i1 int, t1 text, f1 float, i2 int, n1 numeric, b1 bool)
    WITH (appendonly=true, orientation=column, compresstype=zlib);
alter table s_heap_ex_abc drop column f1;
insert into s_heap_ex_abc select 1, 'abc1', 2, 2, true from generate_series(1, 5);
-- exchange partition
alter table s_heap exchange partition abc with table s_heap_ex_abc;
alter table s_heap exchange partition abc with table s_heap_ex_abc;
drop table s_heap, s_heap_ex_abc;
-- MPP-18441 end

-- MPP-18443
create table s_heap (i1 int, t1 text, i2 int , i3 int, n1 numeric,b1 bool)
partition by list (t1)
    (partition def values('def0', 'def1', 'def2', 'def3', 'def4', 'def5', 'def6', 'def7', 'def8', 'def9'));
insert into s_heap(t1, i1, i2, i3, n1, b1)
    select 'def0', 1, 1, 1, 2.3 , true from generate_series(1, 5);
alter table s_heap drop column i3;
create index s_heap_index on s_heap (i2);
alter table s_heap split partition def
    at ('def0', 'def1', 'def2', 'def3', 'def4') into (partition def5, partition def0);
select * from s_heap_1_prt_def0;
drop table s_heap;
-- MPP-18443 end

-- MPP-18445
create table s_heap_ao ( i1 int, t1 text, i2 int , i3 int, n1 numeric,b1 bool)
partition by list (t1)
    (partition def values('def0', 'def1', 'def2', 'def3', 'def4', 'def5', 'def6', 'def7', 'def8', 'def9')
        with (appendonly=true, orientation=row));
insert into s_heap_ao(t1, i1, i2, i3, n1, b1)
    select 'def4', 1, 1, 1, 2.3, true from generate_series(1, 2);
insert into s_heap_ao(t1, i1, i2, i3, n1, b1)
    select 'def5', 1, 1, 1, 2.3, true from generate_series(1, 2);
alter table s_heap_ao drop column i3;
create index s_heap_ao_index on s_heap_ao (i2);
alter table s_heap_ao split partition def
    at ('def0', 'def1', 'def2', 'def3', 'def4') into (partition def5, partition def0);
select * from s_heap_ao_1_prt_def0;
drop table s_heap_ao;
-- MPP-18445 end

-- MPP-18456
create table s_heap_co (i1 int, t1 text, i2 int, i3 int, n1 numeric, b1 bool)
partition by list (t1)
    (partition def values('def0', 'def1', 'def2', 'def3', 'def4', 'def5', 'def6', 'def7', 'def8', 'def9')
        with (appendonly=true, orientation=column));
insert into s_heap_co(t1, i1, i2, i3, n1, b1)
    select 'def4', 1,1, 1, 2.3, true from generate_series(1, 2);
insert into s_heap_co(t1, i1, i2, i3, n1, b1)
    select 'def5', 1,1, 1, 2.3, true from generate_series(1, 2);
alter table s_heap_co drop column i3;
create index s_heap_co_index on s_heap_co (i2);
alter table s_heap_co split partition def
    at ('def0', 'def1', 'def2', 'def3', 'def4') into (partition def5, partition def0);
select * from s_heap_co_1_prt_def0;
drop table s_heap_co;
-- MPP-18456 end

-- MPP-18457, MPP-18415
CREATE TABLE non_ws_phone_leads (
    lead_key integer NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    source_system_lead_id character varying(60) NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_event_type_key smallint NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_site_key integer NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_date_key integer NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_time_key integer NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_phone_number_key integer NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    duration_second smallint NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_program_key smallint NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_call_status_key integer NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_phone_department_set_key smallint NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_phone_channel_set_key smallint NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_phone_provider_key smallint NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
    dim_phone_ad_set_key smallint NOT NULL ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
)
WITH (appendonly=true, compresstype=zlib, orientation=column) DISTRIBUTED BY (dim_site_key ,dim_date_key) PARTITION BY RANGE(dim_date_key) 
          (
          PARTITION p_max START (2451545) END (9999999) WITH (tablename='non_ws_phone_leads_1_prt_p_max', orientation=column, appendonly=true ) 
                    COLUMN lead_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN source_system_lead_id ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_event_type_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_site_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_date_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_time_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_phone_number_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN duration_second ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_program_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_call_status_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_phone_department_set_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_phone_channel_set_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_phone_provider_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768) 
                    COLUMN dim_phone_ad_set_key ENCODING (compresstype=zlib, compresslevel=1, blocksize=32768)
          );

INSERT INTO non_ws_phone_leads VALUES (63962490, 'CA6qOEyxOmNJUQC7', 5058, 999901, 2455441, 40435, 999904, 207, 79, 2, 9901, 9901, 1, 9901);

CREATE TABLE dim_phone_numbers (
    dim_phone_number_key integer NOT NULL,
    media_tracker_description character varying(40) NOT NULL,
    formatted_phone_number character varying(20) NOT NULL,
    source_system_phone_number_id character varying(100) NOT NULL,
    last_modified_date timestamp without time zone NOT NULL
) DISTRIBUTED BY (dim_phone_number_key);

ALTER TABLE ONLY dim_phone_numbers
    ADD CONSTRAINT dim_phone_numbers_pk1 PRIMARY KEY (dim_phone_number_key);

INSERT INTO dim_phone_numbers VALUES (999902, 'test', '800-123-4568', '8001234568', '2012-09-25 13:34:35.037637');
INSERT INTO dim_phone_numbers VALUES (999904, 'test', '(800) 123-4570', '8001234570', '2012-09-25 13:34:35.148104');
INSERT INTO dim_phone_numbers VALUES (999903, 'test', '(800) 123-4569', '8001234569', '2012-09-25 13:34:35.093523');
INSERT INTO dim_phone_numbers VALUES (999901, 'test', '(800)123-4567', '8001234567', '2012-09-25 13:34:34.781042');

INSERT INTO dim_phone_numbers SELECT gs.*, dim_phone_numbers.media_tracker_description, dim_phone_numbers.formatted_phone_number, dim_phone_numbers.source_system_phone_number_id, dim_phone_numbers.last_modified_date FROM dim_phone_numbers, generate_series(1,100000) gs WHERE dim_phone_numbers.dim_phone_number_key = 999901;

ANALYZE dim_phone_numbers;  

-- Table NON_WS_PHONE_LEADS has two distribution keys
-- Equality condition with constant on one distribution key
-- Redistribute over Append
SELECT pl.duration_Second , pl.dim_program_Key, PL.DIM_SITE_KEY, PL.DIM_DATE_KEY
FROM NON_WS_PHONE_LEADS PL
LEFT outer JOIN DIM_PHONE_NUMBERS DPN
ON PL.DIM_PHONE_NUMBER_KEY = DPN.DIM_PHONE_NUMBER_KEY
WHERE pl.SOURCE_SYSTEM_LEAD_ID = 'CA6qOEyxOmNJUQC7'
AND PL.DIM_DATE_KEY = 2455441;

-- Table NON_WS_PHONE_LEADS has two distribution keys
-- Equality conditions with constants on all distribution keys
-- Redistribute over Append
SELECT pl.duration_Second , pl.dim_program_Key, PL.DIM_SITE_KEY, PL.DIM_DATE_KEY
FROM NON_WS_PHONE_LEADS PL
LEFT outer JOIN DIM_PHONE_NUMBERS DPN
ON PL.DIM_PHONE_NUMBER_KEY = DPN.DIM_PHONE_NUMBER_KEY
WHERE pl.SOURCE_SYSTEM_LEAD_ID = 'CA6qOEyxOmNJUQC7'
AND PL.DIM_DATE_KEY = 2455441
AND PL.dim_site_key = 999901;

-- Table NON_WS_PHONE_LEADS has two distribution keys
-- Broadcast over Append
SELECT pl.duration_Second , pl.dim_program_Key, PL.DIM_SITE_KEY, PL.DIM_DATE_KEY
FROM NON_WS_PHONE_LEADS PL
JOIN DIM_PHONE_NUMBERS DPN
ON PL.DIM_PHONE_NUMBER_KEY = DPN.DIM_PHONE_NUMBER_KEY
WHERE pl.SOURCE_SYSTEM_LEAD_ID = 'CA6qOEyxOmNJUQC7'
AND PL.DIM_DATE_KEY = 2455441
AND PL.dim_site_key = 999901;

-- Join condition uses functions
-- Broadcast over Append
SELECT pl.duration_Second , pl.dim_program_Key, PL.DIM_SITE_KEY, PL.DIM_DATE_KEY
FROM NON_WS_PHONE_LEADS PL
JOIN DIM_PHONE_NUMBERS DPN
ON PL.DIM_PHONE_NUMBER_KEY + 1 = DPN.DIM_PHONE_NUMBER_KEY + 1
WHERE pl.SOURCE_SYSTEM_LEAD_ID = 'CA6qOEyxOmNJUQC7'
AND PL.DIM_DATE_KEY = 2455441
AND PL.dim_site_key = 999901;

-- Equality condition with constant on one distribution key
-- Redistribute over Append
-- Accessing a varchar in the SELECT clause should cause a SIGSEGV
SELECT pl.duration_Second , pl.dim_program_Key, PL.DIM_SITE_KEY, PL.DIM_DATE_KEY, source_system_lead_id
FROM NON_WS_PHONE_LEADS PL
LEFT outer JOIN DIM_PHONE_NUMBERS DPN
ON PL.DIM_PHONE_NUMBER_KEY = DPN.DIM_PHONE_NUMBER_KEY
WHERE pl.SOURCE_SYSTEM_LEAD_ID = 'CA6qOEyxOmNJUQC7'
AND PL.DIM_DATE_KEY = 2455441;

DROP TABLE non_ws_phone_leads;
DROP TABLE dim_phone_numbers;

-- Equality condition with a constant expression on one distribution key
drop table if exists foo_p;
drop table if exists bar;
create table foo_p( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);
create table bar( a int, b int, k int, t text, p int) distributed by (a);

insert into foo_p select i, i % 10, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;

analyze foo_p;
analyze bar;
set optimizer_segments = 3;
set optimizer_nestloop_factor = 1.0;
explain select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = (array[1])[1];
reset optimizer_segments;
drop table if exists foo_p;
drop table if exists bar;
-- MPP-18457, MPP-18415 end

drop table if exists pnx;

create table pnx 
    (x int , y text)
    distributed randomly
    partition by list (y)
        ( 
            partition a values ('x1', 'x2'),
            partition c values ('x3', 'x4')
        );

insert into pnx values
    (1,'x1'),
    (2,'x2'),
    (3,'x3'), 
    (4,'x4');

select tableoid::regclass, * 
from pnx;

alter table pnx
    split partition a at ('x1')
    into (partition b, partition c);

select tableoid::regclass, * 
from pnx;

select tableoid::regclass, *
from pnx
where y = 'x1';

select tableoid::regclass, *
from pnx
where x = 1;


drop table if exists pxn;

create table pxn 
    (x int , y text)
    distributed randomly
    partition by list (y)
        ( 
            partition a values ('x1', 'x2'),
            partition c values ('x3', 'x4')
        );

insert into pxn values
    (1,'x1'),
    (2,'x2'),
    (3,'x3'), 
    (4,'x4');

select tableoid::regclass, * 
from pxn;

alter table pxn
    split partition a at ('x1')
    into (partition c, partition b);

select tableoid::regclass, * 
from pxn;

select tableoid::regclass, *
from pxn
where y = 'x2';

select tableoid::regclass, *
from pxn
where x = 2;

drop table if exists pxn;

create table pxn 
    (x int , y int)
    distributed randomly
    partition by range (y)
        ( 
            partition a start (0) end (10),
            partition c start (11) end (20)
        );

insert into pxn values
    (4,4),
    (9,9),
    (14,14), 
    (19,19);

select tableoid::regclass, * 
from pxn;

alter table pxn
    split partition a at (5)
    into (partition b, partition c);

select tableoid::regclass, * 
from pxn;

select tableoid::regclass, *
from pxn
where y = 4;

select tableoid::regclass, *
from pxn
where x = 4;

drop table if exists pxn;

create table pxn 
    (x int , y int)
    distributed randomly
    partition by range (y)
        ( 
            partition a start (0) end (10),
            partition c start (11) end (20)
        );

insert into pxn values
    (4,4),
    (9,9),
    (14,14), 
    (19,19);

select tableoid::regclass, * 
from pxn;

alter table pxn
    split partition a at (5)
    into (partition c, partition b);

select tableoid::regclass, * 
from pxn;

select tableoid::regclass, *
from pxn
where y = 9;

select tableoid::regclass, *
from pxn
where x = 9;

-- MPP-18359 end

-- MPP-19105
-- Base partitions with trailing dropped columns
create table parttest_t (
	a int,
	b int,
	c char,
	d varchar(50)
) distributed by (c) 
partition by range (a) 
( 
	partition p1 start(1) end(5),
	partition p2 start(5)
);

-- Drop column
alter table parttest_t drop column d;

-- Alter table split partition
alter table parttest_t split partition for(1) at (2) into (partition p11, partition p22);

insert into  parttest_t values(1,2,'a');
select * from parttest_t;
-- END MPP-19105
reset optimizer_nestloop_factor;

-- Sub-partition insertion with checking if the user provided correct leaf part
create table part_tab ( i int, j int) distributed by (i) partition by range(j) (start(0) end(10) every(2));
-- Wrong part
insert into part_tab_1_prt_1 values(5,5);
select * from part_tab;
select * from part_tab_1_prt_1;

insert into part_tab_1_prt_2 values(5,5);
select * from part_tab;
select * from part_tab_1_prt_2;

-- Right part
insert into part_tab_1_prt_3 values(5,5);
analyze part_tab;
select * from part_tab;
select * from part_tab_1_prt_3;

-- Root part
insert into part_tab values(5,5);
select * from part_tab;

drop table if exists input1;
create table input1 (x int, y int);
insert into input1 select i, i from (select generate_series(1,10) as i) as t;
drop table if exists input2;
create table input2 (x int, y int);
insert into input2 select i, i from (select generate_series(1,10) as i) as t;

-- Multiple range table entries in the plan
insert into part_tab_1_prt_1 select i1.x, i2.y from input1 as i1 join input2 as i2 on i1.x = i2.x where i2.y = 5;
analyze part_tab;
select * from part_tab;
select * from part_tab_1_prt_1;

insert into part_tab_1_prt_2 select i1.x, i2.y from input1 as i1 join input2 as i2 on i1.x = i2.x where i2.y = 5;
select * from part_tab;
select * from part_tab_1_prt_2;

-- Right part
insert into part_tab_1_prt_3 select i1.x, i2.y from input1 as i1 join input2 as i2 on i1.x = i2.x where i1.x between 4 and 5;
select * from part_tab;
select * from part_tab_1_prt_3;

-- Root part but no matching part for i2.y == 10
insert into part_tab select i1.x, i2.y from input1 as i1 join input2 as i2 on i1.x = i2.x;
select * from part_tab;

-- Root part
insert into part_tab select i1.x, i2.y from input1 as i1 join input2 as i2 on i1.x = i2.x where i2.y < 10;
select * from part_tab;

-- Multi-level partitioning
create table deep_part ( i int, j int, k int, s char(5)) 
distributed by (i) 
partition by list(s)
subpartition by range (j) subpartition template (start(1)  end(10) every(2))
subpartition by range (k) subpartition template (start(1)  end(10) every(2))
(partition female values('F'), partition male values('M'))
;

-- Intermediate partition insert is allowed
insert into deep_part_1_prt_male_2_prt_2 values(1,3,1,'M');
-- but only if it's the right partition.
insert into deep_part_1_prt_male_2_prt_2 values(1,1,1,'M');

-- Wrong sub-partition (inserting a female value in male partition)
insert into deep_part_1_prt_male_2_prt_2_3_prt_2 values (1, 1, 1, 'F');
select * from deep_part;

-- Correct leaf part
insert into deep_part_1_prt_male_2_prt_1_3_prt_1 values (1, 1, 1, 'M');
analyze deep_part;
select * from deep_part;
select * from deep_part_1_prt_male_2_prt_1_3_prt_1;

-- Root part of a multi-level partitioned table
insert into deep_part values (1, 1, 1, 'M');
select * from deep_part;
select * from deep_part_1_prt_male_2_prt_1_3_prt_1;

insert into deep_part values (1, 1, 1, 'F');
select * from deep_part;
select * from deep_part_1_prt_female_2_prt_1_3_prt_1;

insert into deep_part values (5, 5, 5, 'M');
select * from deep_part;
select * from deep_part_1_prt_male_2_prt_3_3_prt_3;

insert into deep_part values (9, 9, 9, 'F');
select * from deep_part;
select * from deep_part_1_prt_female_2_prt_5_3_prt_5;

-- Out of range partition
insert into deep_part values (9, 9, 10, 'F');
select * from deep_part;

drop table input2;
drop table input1;

-- Avoid TupleDesc leak when COPY partition table from files
-- This also covers the bug reported in MPP-9548 where insertion
-- into a dropped/added column yielded incorrect results
drop table if exists pt_td_leak;
CREATE TABLE pt_td_leak
(
col1 int,
col2 int,
col3 int
)
distributed by (col1)
partition by range(col2)
(
    partition part1 start(1) end(5),
    partition part2 start(5) end(10)
);

insert into pt_td_leak select i,i,i from generate_series(1,9) i;
copy pt_td_leak to '/tmp/pt_td_leak.out' csv;

alter table pt_td_leak drop column col3;
alter table pt_td_leak add column col3 int default 7;

drop table if exists pt_td_leak_exchange;
CREATE TABLE pt_td_leak_exchange ( col1 int, col2 int, col3 int) distributed by (col1);
alter table pt_td_leak exchange partition part2 with table pt_td_leak_exchange;

insert into pt_td_leak values(1,8,1);
copy pt_td_leak from '/tmp/pt_td_leak.out' with delimiter ',';

select * from pt_td_leak where col1 = 5;
-- Check that data inserted into dropped/added column is correct
select * from pt_td_leak where col3 = 1;

drop table pt_td_leak;
drop table pt_td_leak_exchange;


--
-- Test COPY, when distribution keys have different attribute numbers,
-- because of dropped columns
--
CREATE TABLE pt_dropped_col_distkey (i int, to_be_dropped text, t text)
DISTRIBUTED BY (t) PARTITION BY RANGE (i) (START (1) END(10) EVERY (5));
INSERT INTO pt_dropped_col_distkey SELECT g, 'dropped' || g, 'before drop ' || g FROM generate_series(1, 7) g;

ALTER TABLE pt_dropped_col_distkey DROP COLUMN to_be_dropped;

-- This new partition won't have the dropped column. Because the distribution
-- key was after the dropped column, the attribute number of the distribution
-- key column will be different in this partition and the parent.
ALTER TABLE pt_dropped_col_distkey ADD PARTITION pt_dropped_col_distkey_new_part START (10) END (100);

INSERT INTO pt_dropped_col_distkey SELECT g, 'after drop ' || g FROM generate_series(8, 15) g;
SELECT * FROM pt_dropped_col_distkey ORDER BY i;
COPY pt_dropped_col_distkey TO '/tmp/pt_dropped_col_distkey.out';

DELETE FROM pt_dropped_col_distkey;

COPY pt_dropped_col_distkey FROM '/tmp/pt_dropped_col_distkey.out';

SELECT * FROM pt_dropped_col_distkey ORDER BY i;

-- don't drop the table, so that we have a partitioned table like this still
-- in the database, when we test pg_upgrade later.


--
-- Test split default partition while per tuple memory context is reset
--
drop table if exists test_split_part cascade;

CREATE TABLE test_split_part ( log_id int NOT NULL, f_array int[] NOT NULL)
DISTRIBUTED BY (log_id)
PARTITION BY RANGE(log_id)
(
    START (1::int) END (100::int) EVERY (5) WITH (appendonly=false),
    PARTITION "Old" START (101::int) END (201::int) WITH (appendonly=false),
    DEFAULT PARTITION other_log_ids  WITH (appendonly=false)
);

insert into test_split_part (log_id , f_array) select id, '{10}' from generate_series(1,1000) id;

ALTER TABLE test_split_part SPLIT DEFAULT PARTITION START (201) INCLUSIVE END (301) EXCLUSIVE INTO (PARTITION "New", DEFAULT PARTITION);

-- In GPDB 6 and below, an automatic array type was only created for the root
-- partition. Nowadays we rely on upstream partitioning code, which creates
-- an array type for all partition.
select typname, typtype, typcategory from pg_type where typname like '%test_split_part%' and typcategory = 'A';
select array_agg(test_split_part) from test_split_part where log_id = 500;
select array_agg(test_split_part_1_prt_other_log_ids) from test_split_part_1_prt_other_log_ids where log_id = 500;

-- Originally reported in MPP-7232
create table mpp7232a (a int, b int) distributed by (a) partition by range (b) (start (1) end (3) every (1));
\d+ mpp7232a
alter table mpp7232a rename partition for (1) to alpha;
alter table mpp7232a rename partition for (2) to bravo;
\d+ mpp7232a

create table mpp7232b (a int, b int) distributed by (a) partition by range (b) (partition alpha start (1) end (3) every (1));
alter table mpp7232b rename partition for (1) to foo;
\d+ mpp7232b

-- Test .. WITH (tablename = <foo> ..) syntax.
create table mpp17740 (a integer, b integer, e date) with (appendonly = true, orientation = column)
distributed by (a)
partition by range(e)
(
    partition mpp17740_20120523 start ('2012-05-23'::date) inclusive end ('2012-05-24'::date) exclusive with (tablename = 'mpp17740_20120523', appendonly = true),
    partition mpp17740_20120524 start ('2012-05-24'::date) inclusive end ('2012-05-25'::date) exclusive with (tablename = 'mpp17740_20120524', appendonly = true)
);
select relname, pg_get_expr(relpartbound, oid) from pg_class where relname like 'mpp17740%';

alter table mpp17740 add partition mpp17740_20120520 start ('2012-05-20'::date) inclusive end ('2012-05-21'::date) exclusive with (tablename = 'mpp17740_20120520', appendonly=true);
select relname, pg_get_expr(relpartbound, oid) from pg_class where relname like 'mpp17740%';

-- Test mix of add and drop various column before split, and exchange partition at the end
create table sales (pkid serial, option1 int, option2 int, option3 int, constraint partable_pkey primary key(pkid, option3))
distributed by (pkid) partition by range (option3)
(
	partition aa start(1) end(100),
	partition bb start(101) end(200),
	partition cc start(201) end (300)
);

-- should error out since no subpartition in sales.
alter table sales add partition dd start(301) end(300)
	( subpartition opt1_1 VALUES (1),
	  subpartition opt1_2 VALUES (2) );

-- root partition (and only root) should have relfrozenxid as 0
select relname, relkind from pg_class where relkind in ('r', 'p') and relname like 'sales%' and relfrozenxid=0;
select gp_segment_id, relname, relkind from gp_dist_random('pg_class') where relkind in ('r', 'p') and relname like 'sales%' and relfrozenxid=0;

alter table sales add column tax float;
-- root partition (and only root) continues to have relfrozenxid as 0
select relname, relkind from pg_class where relkind in ('r', 'p') and relname like 'sales%' and relfrozenxid=0;
select gp_segment_id, relname, relkind from gp_dist_random('pg_class') where relkind in ('r', 'p') and relname like 'sales%' and relfrozenxid=0;

alter table sales drop column tax;

create table newpart(like sales);
alter table newpart add constraint newpart_pkey primary key(pkid, option3);
alter table sales split partition for(1) at (50) into (partition aa1, partition aa2);

select table_schema, table_name, constraint_name, constraint_type
	from information_schema.table_constraints
	where table_name in ('sales', 'newpart')
	and constraint_name in ('partable_pkey', 'newpart_pkey')
	order by table_name desc;

alter table sales exchange partition for (101) with table newpart;

select * from sales order by pkid;

-- Create exchange table before drop column, make sure the consistency check still exist
create table newpart2(like sales);
alter table sales drop column option2;

alter table sales exchange partition for (101) with table newpart2;

select * from sales order by pkid;
drop table sales cascade;

-- Exchage partition table with a table having dropped column
create table exchange_part(a int, b int) partition by range(b) (start (0) end (10) every (5));
create table exchange1(a int, c int, b int);
alter table exchange1 drop column c;
alter table exchange_part exchange partition for (1) with table exchange1;
copy exchange_part from STDIN DELIMITER as '|';
9794 | 1
9795 | 2
9797 | 3
9799 | 4
9801 | 5
9802 | 6
9803 | 7
9806 | 8
9807 | 9
9808 | 1
9810 | 2
9814 | 3
9815 | 4
9817 | 5
9818 | 6
9822 | 7
9824 | 8
9825 | 9
9827 | 1
9828 | 2
9831 | 3
9832 | 4
9836 | 5
9840 | 6
9843 | 7
9844 | 8
\.
select * from exchange_part;
drop table exchange_part;
drop table exchange1;
-- Ensure that new partitions get the correct attributes (MPP17110)
CREATE TABLE pt_tab_encode (a int, b text)
with (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1)
distributed by (a)
partition by list(b) (partition s_abc values ('abc') with (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1));

alter table pt_tab_encode add partition "s_xyz" values ('xyz') WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1);

select relname, pg_get_expr(relpartbound, oid) from pg_class where relname like 'pt_tab_encode%';

select gp_segment_id, attrelid::regclass, attnum, filenum, attoptions from pg_attribute_encoding where attrelid = 'pt_tab_encode_1_prt_s_abc'::regclass;
select gp_segment_id, attrelid::regclass, attnum, filenum, attoptions from gp_dist_random('pg_attribute_encoding') where attrelid = 'pt_tab_encode_1_prt_s_abc'::regclass order by 1,3 limit 5;

select gp_segment_id, attrelid::regclass, attnum, filenum, attoptions from pg_attribute_encoding where attrelid = 'pt_tab_encode_1_prt_s_xyz'::regclass;
select gp_segment_id, attrelid::regclass, attnum, filenum, attoptions from gp_dist_random('pg_attribute_encoding') where attrelid = 'pt_tab_encode_1_prt_s_xyz'::regclass order by 1,3 limit 5;

select c.oid::regclass, relkind, amname, reloptions from pg_class c left join pg_am am on am.oid = relam where c.oid = 'pt_tab_encode_1_prt_s_abc'::regclass;
select c.oid::regclass, relkind, amname, reloptions from pg_class c left join pg_am am on am.oid = relam where c.oid = 'pt_tab_encode_1_prt_s_xyz'::regclass;

-- Ensure that only the correct type of partitions can be added
create table at_range (a int) partition by range (a) (start(1) end(5));
create table at_list (i int) partition by list(i) (partition p1 values(1));

alter table at_list add partition foo2 start(6) end (10);
alter table at_range add partition test values(5);

-- Ensure array type for the non-partition table is there after partition exchange.
CREATE TABLE pt_xchg(a int) PARTITION BY RANGE(a) (START(1) END(4) EVERY (2));
create table xchg_tab1(a int);
CREATE SCHEMA xchg_schema;
create table xchg_schema.xchg_tab2(a int);
alter table pt_xchg exchange partition for (1) with table xchg_tab1;
alter table pt_xchg exchange partition for (3) with table xchg_schema.xchg_tab2;

select a.typowner=b.typowner from pg_type a join pg_type b on true where a.typname = 'xchg_tab1' and b.typname = '_xchg_tab1';

select nspname from pg_namespace join pg_type on pg_namespace.oid = pg_type.typnamespace where pg_type.typname = 'xchg_tab1' or pg_type.typname = '_xchg_tab1';
select nspname from pg_namespace join pg_type on pg_namespace.oid = pg_type.typnamespace where pg_type.typname = 'xchg_tab2' or pg_type.typname = '_xchg_tab2';

select typname from pg_type where typelem = 'xchg_tab1'::regtype;
select typname from pg_type where typelem = 'xchg_schema.xchg_tab2'::regtype;

select typname from pg_type where typarray = '_xchg_tab1'::regtype;
select typname from pg_type where typarray = 'xchg_schema._xchg_tab2'::regtype;

alter table pt_xchg exchange partition for (1) with table xchg_tab1;
select a.typowner=b.typowner from pg_type a join pg_type b on true where a.typname = 'xchg_tab1' and b.typname = '_xchg_tab1';
select nspname from pg_namespace join pg_type on pg_namespace.oid = pg_type.typnamespace where pg_type.typname = 'xchg_tab1' or pg_type.typname = '_xchg_tab1';
select typname from pg_type where typelem = 'xchg_tab1'::regtype;
select typname from pg_type where typarray = '_xchg_tab1'::regtype;


-- Test with an incomplete operator class. Create a custom operator class and
-- only define equality on it. You can't do much with that.
--
-- Before GPDB 7, Greenplum used to allow creating the table, but you got an
-- error when inserting to it. Nowadays we rely on PostgreSQL partitioning
-- code, which rejects it at CREATE TABLE already.
create type employee_type as (empid int, empname text);
create function emp_equal(employee_type, employee_type) returns boolean
  as 'select $1.empid = $2.empid;'
  language sql
  immutable
  returns null on null input;
create operator = (
        leftarg = employee_type,
        rightarg = employee_type,
        procedure = emp_equal
);
create operator class employee_incomplete_op_class default for type employee_type
  using btree as
  operator 3 =;
create table employee_table(timest date, user_id numeric(16,0) not null, tag1 char(5), emp employee_type)
  distributed by (user_id)
  partition by list(emp)
  (partition part1 values('(1, ''foo'')'::employee_type), partition part2 values('(2, ''foo'')'::employee_type));


-- Test partition table with ACL.
-- We grant default SELECT permission to a new user, this new user should be
-- able to SELECT from any partition table we create later.
-- (https://github.com/greenplum-db/gpdb/issues/9524)
DROP TABLE IF EXISTS user_prt_acl.t_part_acl;
DROP SCHEMA IF EXISTS user_prt_acl;
DROP ROLE IF EXISTS user_prt_acl;

CREATE ROLE user_prt_acl;
CREATE SCHEMA schema_part_acl;
GRANT USAGE ON SCHEMA schema_part_acl TO user_prt_acl;
ALTER DEFAULT PRIVILEGES IN SCHEMA schema_part_acl GRANT SELECT ON TABLES TO user_prt_acl;

CREATE TABLE schema_part_acl.t_part_acl (dt date)
PARTITION BY RANGE (dt)
(
    START (date '2019-12-01') INCLUSIVE
    END (date '2020-02-01') EXCLUSIVE
    EVERY (INTERVAL '1 month')
);
INSERT INTO schema_part_acl.t_part_acl VALUES (date '2019-12-01'), (date '2020-01-31');

-- check if parent and child table have same relacl
SELECT relname FROM pg_class
WHERE relname LIKE 't_part_acl%'
  AND relacl = (SELECT relacl FROM pg_class WHERE relname = 't_part_acl');

-- check if new user can SELECT all data
SET ROLE user_prt_acl;
SELECT * FROM schema_part_acl.t_part_acl;

RESET ROLE;
-- we don't drop the table, schema and role here in order to test upgrade


-- test on commit behavior used on partition table

-- test on commit delete rows
begin;
create temp table temp_parent (a int) partition by range(a) (start(1) end(10) every(10)) on commit delete rows;
insert into temp_parent select i from generate_series(1, 5) i;
select count(*) from temp_parent;
commit;
-- DELETE ROWS will not cascaded to its partitions when we use DELETE ROWS behavior
select count(*) from temp_parent;
drop table temp_parent;

-- test on commit drop
begin;
create temp table temp_parent (a int) partition by range(a) (start(1) end(10) every(1)) on commit drop;
insert into temp_parent select i from generate_series(1, 5) i;
select count(*) from pg_class where relname like 'temp_parent_%';
commit;
-- no relations remain in this case.
select count(*) from pg_class where relname like 'temp_parent_%';

-- check ATTACH PARTITION on parent table with different distribution policy
CREATE EXTENSION IF NOT EXISTS gp_debug_numsegments;
SELECT gp_debug_set_create_table_default_numsegments(1);

CREATE TABLE expanded_parent(a int, b int) PARTITION BY RANGE(b) (START (0) END (6) EVERY (3));
CREATE TABLE expanded_parent2(a int, b int) PARTITION BY RANGE(b) (START (0) END (6) EVERY (3));
CREATE TABLE unexpanded_parent(a int, b int) PARTITION BY RANGE(b) (START (0) END (6) EVERY (3));
CREATE TABLE unexpanded_attach(a int, b int);
CREATE TABLE unexpanded_attach2(a int, b int);
CREATE TABLE expanded_attach(a int, b int);
CREATE TABLE expanded_attach2(a int, b int);

SELECT gp_debug_reset_create_table_default_numsegments();

-- attaching unexpanded partition to expanded table should fail
ALTER TABLE expanded_parent EXPAND TABLE;
ALTER TABLE expanded_parent ATTACH PARTITION unexpanded_attach FOR VALUES FROM (6) TO (9);

-- attaching unexpanded partition to expanded table with partition prepare should fail
ALTER TABLE expanded_parent2 EXPAND PARTITION PREPARE;
ALTER TABLE expanded_parent2 ATTACH PARTITION unexpanded_attach2 FOR VALUES FROM (6) TO (9);

-- attaching expanded partition to unexpanded table should fail
ALTER TABLE expanded_attach EXPAND TABLE;
ALTER TABLE unexpanded_parent ATTACH PARTITION expanded_attach FOR VALUES FROM (6) TO (9);

-- attaching expanded partition to expanded table should succeed
ALTER TABLE expanded_attach2 EXPAND TABLE;
ALTER TABLE expanded_parent2 ATTACH PARTITION expanded_attach2 FOR VALUES FROM (6) TO (9);
ALTER TABLE expanded_parent ATTACH PARTITION expanded_attach FOR VALUES FROM (6) TO (9);

-- cleanup
DROP TABLE expanded_parent;
DROP TABLE expanded_parent2;
DROP TABLE unexpanded_parent;
DROP TABLE unexpanded_attach;
DROP TABLE unexpanded_attach2;

--
-- Verify inheritance behavior of new partition child using various syntax
--
-- set owner for partition root (should be inherited except for ATTACH, EXCHANGE)
CREATE ROLE part_inherit_role CREATEROLE;
CREATE ROLE part_inherit_other_role IN ROLE part_inherit_role;
CREATE ROLE part_inherit_priv_role;
CREATE ROLE part_inherit_attach_priv_role;
CREATE ROLE part_inherit_exchange_out_priv_role;
SET ROLE part_inherit_role;

CREATE TABLE part_inherit (
    a int,
    b int,
-- non-partition constraint should be inherited except for ATTACH, EXCHANGE
    CONSTRAINT con1 CHECK (a >= 0)
)
PARTITION BY RANGE (a)
SUBPARTITION BY RANGE (b)
SUBPARTITION TEMPLATE
(SUBPARTITION l2_child START(10100) END(10200))
(DEFAULT PARTITION l1_default,
       PARTITION l1_child1 START (0) END (100),
       PARTITION l1_to_split START (100) END (200),
       PARTITION l1_to_exchange START (200) END (300))
--AM and reloption should be inherited except for ATTACH, EXCHANGE
WITH (appendonly = TRUE, compresslevel = 7);

-- Set more properties for the root.
-- index are inherited 
CREATE INDEX part_inherit_i on part_inherit(a);
-- privileges are inherited except for ATTACH, EXCHANGE
GRANT UPDATE ON part_inherit TO part_inherit_priv_role;
-- triggers are inherited except for subpartitions 
-- FIXME: In 6X we used to not inherit triggers at all, in 7X we start to inherit
-- trigger like the upstream, but the subpartitions created from the subpartition 
-- template still don't inherit the triggers. We should fix that.
CREATE FUNCTION part_inherit_trig() RETURNS TRIGGER LANGUAGE plpgsql
  AS $$ BEGIN RETURN NULL; END $$;
CREATE TRIGGER part_inherit_tg AFTER INSERT ON part_inherit
  FOR EACH ROW EXECUTE FUNCTION part_inherit_trig();
-- rules are not inherited
CREATE RULE part_inherit_rule AS ON UPDATE TO part_inherit DO INSERT INTO part_inherit values(1);
-- row-level security policies are not inherited
ALTER TABLE part_inherit ENABLE ROW LEVEL SECURITY;

-- Check the current status 
SELECT c.relname,
        c.reloptions,
        c.relkind,
        a.amname as am,
        c.relhasindex as hasindex,
        r.rolname as owner,
        c.relacl as acl,
        c.relchecks as numchecks,
        c.relhasrules as hasrules,
        c.relhastriggers as hastriggers,
        c.relrowsecurity as rowsecurity
FROM pg_class c join pg_roles r on c.relowner = r.oid
        join pg_am a on c.relam = a.oid
WHERE c.relname LIKE 'part_inherit%' AND 
        c.relkind NOT IN ('i', 'I');

-- Now create child partitions in various forms 

-- set an alternative role
SET ROLE part_inherit_other_role;

-- CREATE TABLE PARTITION OF
CREATE TABLE part_inherit_partof PARTITION OF part_inherit FOR VALUES FROM (300) TO (400);

SELECT c.relname,
        c.reloptions,
        c.relkind,
        a.amname as am,
        c.relhasindex as hasindex,
        r.rolname as owner,
        c.relacl as acl,
        c.relchecks as numchecks,
        c.relhasrules as hasrules,
        c.relhastriggers as hastriggers,
        c.relrowsecurity as rowsecurity
FROM pg_class c join pg_roles r on c.relowner = r.oid
        join pg_am a on c.relam = a.oid
WHERE c.relname = 'part_inherit_partof';

-- Now create child partitions in various forms 
-- ATTACH PARTITION
-- error if the partition-to-be doesn't have the same constraint as the parent.
CREATE TABLE part_inherit_attach(a int, b int);
ALTER TABLE part_inherit ATTACH PARTITION part_inherit_attach FOR VALUES FROM (400) TO (500);
DROP TABLE part_inherit_attach;
-- good case: have the same constraint as parent. Can have other constraint too.
CREATE TABLE part_inherit_attach(a int, b int, CONSTRAINT con1 CHECK (a>=0), CONSTRAINT con2 CHECK (b>=0));
-- reloptions and AM ('heap' which is different than the future parent) will be kept
ALTER TABLE part_inherit_attach SET (fillfactor=30);
-- privilege will be kept
GRANT UPDATE ON part_inherit_attach TO part_inherit_attach_priv_role;
-- do the attach
ALTER TABLE part_inherit ATTACH PARTITION part_inherit_attach FOR VALUES FROM (400) TO (500);

SELECT c.relname,
        c.reloptions,
        c.relkind,
        a.amname as am,
        c.relhasindex as hasindex,
        r.rolname as owner,
        c.relacl as acl,
        c.relchecks as numchecks,
        c.relhasrules as hasrules,
        c.relhastriggers as hastriggers,
        c.relrowsecurity as rowsecurity
FROM pg_class c join pg_roles r on c.relowner = r.oid
        join pg_am a on c.relam = a.oid
WHERE c.relname = 'part_inherit_attach';


-- ADD PARTITION
ALTER TABLE part_inherit ADD PARTITION added START(500) END(600);

SELECT c.relname,
        c.reloptions,
        c.relkind,
        a.amname as am,
        c.relhasindex as hasindex,
        r.rolname as owner,
        c.relacl as acl,
        c.relchecks as numchecks,
        c.relhasrules as hasrules,
        c.relhastriggers as hastriggers,
        c.relrowsecurity as rowsecurity
FROM pg_class c join pg_roles r on c.relowner = r.oid
        join pg_am a on c.relam = a.oid
WHERE c.relname LIKE 'part_inherit_1_prt_added%' AND 
        c.relkind NOT IN ('i', 'I');

-- EXCHANGE PARTITION - same behavior as ATTACH PARTITION
-- error if the partition-to-be doesn't have the same constraint as the parent.
CREATE TABLE part_inherit_exchange_out(a int, b int);
ALTER TABLE part_inherit_1_prt_l1_to_exchange EXCHANGE PARTITION l2_child WITH TABLE part_inherit_exchange_out;
DROP TABLE part_inherit_exchange_out;
-- good case: have the same constraint as parent. Can have other constraint too.
CREATE TABLE part_inherit_exchange_out(a int, b int, CONSTRAINT con1 CHECK (a>=0), CONSTRAINT con2 CHECK (b>=0));
-- reloptions and AM ('heap' which is different than the future parent) will be kept
ALTER TABLE part_inherit_exchange_out SET (fillfactor=30);
-- privilege will be kept
GRANT UPDATE ON part_inherit_exchange_out TO part_inherit_exchange_out_priv_role;
-- do the exchange
ALTER TABLE part_inherit_1_prt_l1_to_exchange EXCHANGE PARTITION l2_child WITH TABLE part_inherit_exchange_out;

SELECT c.relname,
        c.reloptions,
        c.relkind,
        a.amname as am,
        c.relhasindex as hasindex,
        r.rolname as owner,
        c.relacl as acl,
        c.relchecks as numchecks,
        c.relhasrules as hasrules,
        c.relhastriggers as hastriggers,
        c.relrowsecurity as rowsecurity
FROM pg_class c join pg_roles r on c.relowner = r.oid
        join pg_am a on c.relam = a.oid
WHERE (c.relname LIKE 'part_inherit_1_prt_l1_to_exchange%' OR c.relname = 'part_inherit_exchange_out')
	AND c.relkind NOT IN ('i', 'I');

-- SPLIT PARTITION
ALTER TABLE part_inherit_1_prt_l1_to_split SPLIT PARTITION l2_child AT (10150) INTO (PARTITION split1, PARTITION split2);

SELECT c.relname,
        c.reloptions,
        c.relkind,
        a.amname as am,
        c.relhasindex as hasindex,
        r.rolname as owner,
        c.relacl as acl,
        c.relchecks as numchecks,
        c.relhasrules as hasrules,
        c.relhastriggers as hastriggers,
        c.relrowsecurity as rowsecurity
FROM pg_class c join pg_roles r on c.relowner = r.oid
        join pg_am a on c.relam = a.oid
WHERE c.relname LIKE 'part_inherit_1_prt_l1_to_split%' AND 
        c.relkind NOT IN ('i', 'I');

-- Now print everything for comparison
SELECT c.relname,
        c.reloptions,
        c.relkind,
        a.amname as am,
        c.relhasindex as hasindex,
        r.rolname as owner,
        c.relacl as acl,
        c.relchecks as numchecks,
        c.relhasrules as hasrules,
        c.relhastriggers as hastriggers,
        c.relrowsecurity as rowsecurity
FROM pg_class c join pg_roles r on c.relowner = r.oid
        join pg_am a on c.relam = a.oid
WHERE c.relname LIKE 'part_inherit%' AND 
        c.relkind NOT IN ('i', 'I');

RESET ROLE;

DROP TABLE part_inherit;
DROP FUNCTION part_inherit_trig CASCADE;
DROP TABLE part_inherit_exchange_out;
DROP ROLE part_inherit_role;
DROP ROLE part_inherit_other_role;
DROP ROLE part_inherit_priv_role;
DROP ROLE part_inherit_attach_priv_role;
DROP ROLE part_inherit_exchange_out_priv_role;

--Test cases for data selection from range partitioned tables with predicate on date or timestamp type-------------
drop table if exists test_rangePartition;
create table public.test_rangePartition
(datedday date)
    WITH (
        appendonly=false
        )
    PARTITION BY RANGE(datedday)
(
    PARTITION pn_20221022 START ('2022-10-22'::date) END ('2022-10-23'::date),
    PARTITION pn_20221023 START ('2022-10-23'::date) END ('2022-10-24'::date),
    DEFAULT PARTITION pdefault
    );

insert into public.test_rangePartition(datedday)
select ('2022-10-22'::date)
union
select ('2022-10-23'::date);

--Test case with condition on date and timestamp
explain (costs off) select max(datedday) from public.test_rangePartition where datedday='2022-10-23' or datedday=('2022-10-23'::date -interval '1 day');
select max(datedday) from public.test_rangePartition where datedday='2022-10-23' or datedday=('2022-10-23'::date -interval '1 day');

--Test case with condition on date and timestamp
explain (costs off) select max(datedday) from public.test_rangePartition where datedday='2022-10-23' or datedday='2022-10-22';
select max(datedday) from public.test_rangePartition where datedday='2022-10-23' or datedday='2022-10-22';

--Test case with condition on timestamp and timestamp
explain (costs off) select max(datedday) from public.test_rangePartition where datedday=('2022-10-23'::date -interval '0 day') or datedday=('2022-10-23'::date -interval '1 day');
select max(datedday) from public.test_rangePartition where datedday=('2022-10-23'::date -interval '0 day') or datedday=('2022-10-23'::date -interval '1 day');

--Test case with condition on date and timestamp
explain (costs off) select datedday from public.test_rangePartition where datedday='2022-10-23' or datedday=('2022-10-23'::date -interval '1 day');
select datedday from public.test_rangePartition where datedday='2022-10-23' or datedday=('2022-10-23'::date -interval '1 day');

drop table test_rangePartition;

--Test cases for data selection from List partitioned tables with predicate on date or timestamp type-------------
drop table if exists test_listPartition;
create table test_listPartition (i int, d date)
    partition by list(d)
 (partition p1 values('2022-10-22'), partition p2 values('2022-10-23'),
 default partition pdefault  );


insert into test_listPartition values(1,'2022-10-22');
insert into test_listPartition values(2,'2022-10-23');
insert into test_listPartition values(3,'2022-10-24');


--Test case with condition on date and timestamp
explain (costs off) select max(d) from test_listPartition where d='2022-10-23' or d=('2022-10-23'::date -interval '1 day');
select max(d) from test_listPartition where d='2022-10-23' or d=('2022-10-23'::date -interval '1 day');

--Test case with condition on date and date
explain (costs off) select max(d) from test_listPartition where d='2022-10-23' or d='2022-10-22';
select max(d) from test_listPartition where d='2022-10-23' or d='2022-10-22';

--Test case with condition on timestamp and timestamp
explain (costs off) select max(d) from test_listPartition where d=('2022-10-23'::date -interval '0 day') or d=('2022-10-23'::date -interval '1 day');
select max(d) from test_listPartition where d=('2022-10-23'::date -interval '0 day') or d=('2022-10-23'::date -interval '1 day');

--Test case with condition on timestamp and timestamp
explain (costs off) select d from test_listPartition where d='2022-10-23' or d=('2022-10-23'::date -interval '1 day');
select d from test_listPartition where d='2022-10-23' or d=('2022-10-23'::date -interval '1 day');

drop table test_listPartition;