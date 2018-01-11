set enable_partition_rules = false;

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

create unique index region_pkey on region(r_regionkey);

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

-- policies are different
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p (i int, j int) distributed by (j);
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;
drop table foo_p;
drop table bar_p;

-- random policy vs. hash policy
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p (i int, j int) distributed randomly;
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;
drop table foo_p;
drop table bar_p;

-- different number of columns
create table foo_p (i int, j int, k text) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p (i int, j int) distributed by (i);
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;
drop table foo_p;
drop table bar_p;

-- different types
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p (i int, j int8) distributed by (i);
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;
drop table foo_p;
drop table bar_p;

-- different column names
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p (i int, m int) distributed by (i);
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;
drop table foo_p;
drop table bar_p;

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
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p (i int, j int) distributed by (i);
set session authorization part_role;
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;

-- back to superuser
\c -
alter table bar_p owner to part_role;
set session authorization part_role;
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;
\c -

-- owners should be the same, error out 
alter table foo_p exchange partition for(rank(6)) with table bar_p;
drop table foo_p;
drop table bar_p;

-- should work, and new partition should inherit ownership (mpp-6538)
set role part_role;
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(6) every(3));
reset role;
alter table foo_p split partition for(rank(1)) at (2) into (partition prt_11, partition prt_12);
\dt foo_*
drop table foo_p;

drop role part_role;
-- with and without OIDs
-- MPP-8405: disallow OIDS on partitioned tables 
create table foo_p (i int, j int) with (oids = true) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
-- but disallow exchange if different oid settings
create table foo_p (i int, j int) with (oids = false) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p (i int, j int) with (oids = true) distributed by (i);
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;
drop table foo_p;
drop table bar_p;

-- non-partition table involved in inheritance
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));

create table barparent(i int, j int) distributed by (i);
create table bar_p () inherits(barparent);
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;
drop table foo_p;
drop table bar_p;
drop table barparent;

-- non-partition table involved in inheritance
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));

create table bar_p(i int, j int) distributed by (i);
create table barchild () inherits(bar_p);
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;
drop table foo_p;
drop table barchild;
drop table bar_p;
-- rules on non-partition table
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));

create table bar_p(i int, j int) distributed by (i);
create table baz_p(i int, j int) distributed by (i);
create rule bar_baz as on insert to bar_p do instead insert into baz_p
  values(NEW.i, NEW.j);

alter table foo_p exchange partition for(rank(2)) with table bar_p;
drop table foo_p, bar_p, baz_p;

-- Should fail: A constraint on bar_p isn't shared by all the parts.  
-- Allowing this would make an inconsistent partitioned table.  Note
-- that it is possible to have a constraint that prevents rows from 
-- going into one or more parts.  This isn't a conflict, though prior
-- versions would fail because "a constraint on bar_p conflicts with
-- partitioning rule". 
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(5) every(1));

create table bar_a(i int, j int check (j > 1000)) distributed by (i);
alter table foo_p exchange partition for(rank(2)) with table bar_a;

-- Should fail: A constraint on bar_p isn't shared by all the parts.
-- Allowing this would make an inconsistent partitioned table. 
-- Prior versions allowed this, so parts could have differing constraints
-- as long as they avoided the partition columns.
create table bar_b(i int check (i > 1000), j int) distributed by (i);
alter table foo_p exchange partition for(rank(2)) with table bar_b;

-- like above, but with two contraints, just to check that the error
-- message can print that correctly.
create table bar_c(i int check (i > 1000), j int check (j > 1000)) distributed by (i);
alter table foo_p exchange partition for(rank(2)) with table bar_c;

-- Shouldn't fail: check constraint matches partition rule.
-- Note this test is slightly different from prior versions to get
-- in line with constraint consistency requirement.
create table bar_d(i int, j int check (j >= 2 and j < 3 ))
distributed by (i);
insert into bar_d values(100000, 2);
alter table foo_p exchange partition for(rank(2)) with table bar_d;
insert into bar_d values(200000, 2);
select * from bar_d;

drop table foo_p, bar_a, bar_b, bar_c, bar_d;

-- permissions
create role part_role;
create table foo_p (i int) partition by range(i)
(start(1) end(10) every(1));
create table bar_p (i int);
grant select on foo_p to part_role;
revoke all on bar_p from part_role;
select has_table_privilege('part_role', 'foo_p_1_prt_6'::regclass, 'select');
select has_table_privilege('part_role', 'bar_p'::regclass, 'select');
alter table foo_p exchange partition for(rank(6)) with table bar_p;
select has_table_privilege('part_role', 'foo_p_1_prt_6'::regclass, 'select');
select has_table_privilege('part_role', 'bar_p'::regclass, 'select');
drop table foo_p;
drop table bar_p;
drop role part_role;

-- validation
create table foo_p (i int) partition by range(i)
(start(1) end(10) every(1));
create table bar_p (i int);

insert into bar_p values(6);
insert into bar_p values(100);
-- should fail
alter table foo_p exchange partition for(rank(6)) with table bar_p;
alter table foo_p exchange partition for(rank(6)) with table bar_p without
validation;
select * from foo_p;
drop table foo_p, bar_p;

-- basic test
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p(i int, j int) distributed by (i);

insert into bar_p values(6);
alter table foo_p exchange partition for(rank(6)) with table bar_p;
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
alter table foo_p exchange partition for(rank(6)) with table bar_p;
-- Should fail.  Prior releases didn't convey constraints out via exchange
-- but we do now, so the following tries to insert a value that can't go
-- in part 6.
insert into bar_p values(10, 10);
drop table foo_p;
select * from bar_p;
-- Should succeed.  Conveyed constraint matches.
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
alter table foo_p exchange partition for(rank(6)) with table bar_p;
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
alter table foo_p exchange partition for(rank(6)) with table bar_p;
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
alter table foo_p exchange partition for(rank(6)) with table bar_p;
select * from foo_p;
drop table bar_p;
drop table foo_p;

-- exchange same table more than once
create table foo_p (i int, j int) distributed by (i)
partition by range(j)
(start(1) end(10) every(1));
create table bar_p(i int, j int) distributed by (i);

insert into bar_p values(6, 6);
alter table foo_p exchange partition for(rank(6)) with table bar_p;
select * from foo_p;
select * from bar_p;

alter table foo_p exchange partition for(rank(6)) with table bar_p;
select * from foo_p;
select * from bar_p;

alter table foo_p exchange partition for(rank(6)) with table bar_p;
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
set gp_enable_exchange_default_partition = on;
alter table dex exchange default partition with table exh_abc;
reset gp_enable_exchange_default_partition;

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

-- Exchange default sub-partition, should fail
alter table sto_ao_ao alter partition for (rank(3)) exchange default partition with table exh_ao_ao;
set gp_enable_exchange_default_partition = on;
alter table sto_ao_ao alter partition for (rank(3)) exchange default partition with table exh_ao_ao;
reset gp_enable_exchange_default_partition;

-- Exchange a non-default sub-partition of a default partition, should fail
alter table sto_ao_ao alter default partition exchange partition for ('one') with table exh_ao_ao;

-- Exchange a partition that has sub partitions, should fail.
alter table sto_ao_ao exchange partition for ('2008-01-01') with table exh_ao_ao;

drop table sto_ao_ao;
drop table exh_ao_ao;
-- XXX: not yet: VALIDATE parameter


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
create table hhh_r1 (a char(1), b date, d char(3)) 
distributed by (a) partition by range (b)
(                                                              
partition aa start (date '2007-01-01') end (date '2008-01-01') 
      every (interval '0 days')
);

create table foo_p (i int) distributed by(i)
partition by range(i)
(start (1) end (20) every(0));

-- Check for ambiguous EVERY parameters
-- should fail
create table foo_p (i int) distributed by (i)
partition by range(i)
(start (1) end (20) every (0.6));
-- should fail
create table foo_p (i int) distributed by (i)
partition by range(i)
(start (1) end (20) every (0.3));
-- should fail
create table foo_p (i int) distributed by (i)
partition by range(i)
(start (1) end (20) every (1.3));

-- should fail
create table foo_p (i int) distributed by (i)
partition by range(i)
(start (1) end (20) every (10.9));

-- should fail
create table foo_p (i int, j date) distributed by (i)
partition by range(j)
(start ('2007-01-01') end ('2008-01-01') every (interval '0.5 days'));

-- should fail
create table foo_p (i int, j date) distributed by (i)
partition by range(j)
(start ('2007-01-01') end ('2008-01-01') every (interval '0.5 days'));

-- should fail
create table foo_p (i int, j date) distributed by (i)
partition by range(j)
(start ('2007-01-01') end ('2008-01-01') every (interval '12 hours'));

-- should fail
create table foo_p (i int, j date) distributed by (i)
partition by range(j)
(start ('2007-01-01') end ('2008-01-01') every (interval '1.2 days'));

-- should work
create table foo_p (i int, j timestamp) distributed by (i)
partition by range(j)
(start ('2007-01-01') end ('2007-01-05') every (interval '1.2 days'));

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

-- Make sure they're correctly ordered
select parname, parruleord, pg_get_expr(parrangestart, parchildrelid, false),
parrangestartincl,
pg_get_expr(parrangeend, parchildrelid, false),parrangeendincl 
from pg_partition_rule  where
paroid in (select oid from pg_partition where parrelid = 'supplier2'::regclass)
order by parruleord;

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

select parname, parruleord, pg_get_expr(parrangestart, parchildrelid, false) as
 start, pg_get_expr(parrangeend, parchildrelid, false) as end,
 pg_get_expr(parlistvalues, parchildrelid, false) as list from 
 pg_partition_rule
 r, pg_partition p where r.paroid = p.oid and p.parlevel = 0 and 
 p.parrelid = 'foo_p'::regclass order by 1;

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
select parname, parruleord, pg_get_expr(parrangestart, parchildrelid, false) as
 start, pg_get_expr(parrangeend, parchildrelid, false) as end,
  pg_get_expr(parlistvalues, parchildrelid, false) as list from
   pg_partition_rule
    r, pg_partition p where r.paroid = p.oid and p.parlevel = 0 and
	 p.parrelid = 'foo_p'::regclass order by 1;

drop table foo_p;
create table foo_p (i int) 
partition by range(i)
(partition p1 start(1) end(5),
 partition p2 start(10) exclusive,
 partition p3 end(10) inclusive);
select parname, parruleord, pg_get_expr(parrangestart, parchildrelid, false) as
 start, parrangestartincl,
 pg_get_expr(parrangeend, parchildrelid, false) as end,
 parrangeendincl,
  pg_get_expr(parlistvalues, parchildrelid, false) as list from
   pg_partition_rule
    r, pg_partition p where r.paroid = p.oid and p.parlevel = 0 and
	 p.parrelid = 'foo_p'::regclass order by 1;

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
,subpartition by range (ps_supplycost) subpartition template (start('1')
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

select parname, parruleord, pg_get_expr(parrangestart, parchildrelid, false) as
 start, parrangestartincl,
 pg_get_expr(parrangeend, parchildrelid, false) as end,
 parrangeendincl,
  pg_get_expr(parlistvalues, parchildrelid, false) as list from
   pg_partition_rule
    r, pg_partition p where r.paroid = p.oid and p.parlevel = 0 and
	 p.parrelid = 'partlineitem'::regclass order by 1;

drop table partlineitem;

-- Make sure ADD creates dependencies
create table i (i int) partition by range(i) (start (1) end(3) every(1));
alter table i add partition foo2 start(40) end (50);
drop table i;

create table i (i int) partition by range(i) (start (1) end(3) every(1));
alter table i add partition foo2 start(40) end (50);
alter table i drop partition foo2;
drop table i;

-- dumpability of partition info
create table i5 (i int) partition by RANGE(i) (start(1) exclusive end(10)
inclusive);
select tablename, partitiontablename,
partitionboundary from pg_partitions where
tablename = 'i5';
select pg_get_partition_def('i5'::regclass, true);
drop table i5;

CREATE TABLE PARTSUPP (
PS_PARTKEY INTEGER,
PS_SUPPKEY INTEGER,
PS_AVAILQTY integer,
PS_SUPPLYCOST decimal,
PS_COMMENT VARCHAR(199)
)
partition by range (ps_suppkey)
subpartition by range (ps_partkey)
,subpartition by range (ps_supplycost) subpartition template (start('1')
end('1001') every(500))
(
partition p1 start('1') end('10001') every(5000)
(subpartition sp1 start('1') end('200001') every(66666)
)
);

select tablename, partitiontablename,
partitionboundary from pg_partitions where
tablename = 'partsupp';
select pg_get_partition_def('partsupp'::regclass, true);
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
),
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
subpartition by range (year),
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
)
,
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
alter table ataprank alter partition girls truncate partition for (rank(1));
alter table ataprank alter partition girls alter partition 
for (rank(1)) truncate partition mass;

-- don't NOTIFY of children if cascade
alter table ataprank truncate partition girls cascade;

-- fail - no rank 100
alter table ataprank alter partition girls truncate partition for (rank(100));

-- fail - no funky
alter table ataprank alter partition girls alter partition 
for (rank(1)) truncate partition "funky";

-- fail - no funky (drop)
alter table ataprank alter partition girls alter partition 
for (rank(1)) drop partition "funky";

-- fail - missing name
alter table ataprank alter partition girls alter partition 
for (rank(1)) drop partition ;

-- ok
alter table ataprank alter partition girls drop partition 
for (rank(1)) ;

-- ok , skipping
alter table ataprank alter partition girls drop partition if exists jan01;

-- ok until run out of partitions
alter table ataprank alter partition girls drop partition ;
alter table ataprank alter partition girls drop partition ;
alter table ataprank alter partition girls drop partition ;
alter table ataprank alter partition girls drop partition ;
alter table ataprank alter partition girls drop partition ;

-- ok, skipping
alter table ataprank alter partition girls drop partition if exists for (rank(5));

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
create table f (n numeric(20, 2)) partition by range(n) (start(1::bigint)
end(10000::text));
drop table f;
--should fail. bool -> numeric makes no sense
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
CREATE TABLE partsupp (
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
drop table partsupp;

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
drop table orders;

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
alter table k split partition bar at (23) into (partition baz, partition foz);
select partitiontablename,partitionposition,partitionrangestart,
       partitionrangeend from pg_partitions where tablename = 'k'
	   order by partitionposition;
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

-- check that we don't allow updates of tuples such that they would move
-- between partitions
create table v (i int, j int) partition by range(j) (start(1) end(5)
 every(2));
insert into v values(1, 1) ;
-- should work
update v set j = 2;
-- should fail
update v set j = 3;
drop table v;

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
select partitiontablename, partitionboundary from pg_partitions
where tablename = 'k' order by 1;
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
alter table a split partition for(rank(1)) at (date '2006-01-01')
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

-- Check for MPP-3679
create table list_test (a text, b text) partition by list (a) (partition foo
values ('foo'), partition bar values ('bar'), default partition baz);
alter table list_test split default partition at ('baz') into (partition bing,
default partition);
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
select count(*) = 0 as passed from pg_partition_rule pr
 left outer join pg_partition p on pr.paroid = p.oid
 where p.parrelid not in (select oid from pg_class);

-- MPP-4172
-- should fail
create table ggg (a char(1), b int)
distributed by (b)
partition by range(a)
(
partition aa start ('2006') end ('2009'), partition bb start ('2007') end
('2008')
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

-- MPP-5159
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

-- MPP-5185
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
select * from pg_partition_templates where tablename like 'rank_settemp%';

alter table rank_settemp set subpartition template (default subpartition def2);

-- def2 is there
select * from pg_partition_templates where tablename like 'rank_settemp%';

alter table rank_settemp set subpartition template (default subpartition def2);
-- Should still be there
select * from pg_partition_templates where tablename like 'rank_settemp%';


alter table rank_settemp set subpartition template (start (date '2006-01-01') with (appendonly=true));
alter table rank_settemp add partition f1 values ('N');
alter table rank_settemp set subpartition template (start (date '2007-01-01') with (appendonly=true, compresslevel=5));
alter table rank_settemp add partition f2 values ('C');

select * from pg_partition_templates where tablename like 'rank_settemp%';

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
-- MPP-8405: disallow OIDS on partitioned tables 
create table rank_damage (i int, j int) with oids partition by range(j) (start(1) end(10)
every(1));
-- this works
create table rank_damage (i int, j int)  partition by range(j) (start(1) end(10)
every(1));
-- should all fail
alter table rank_damage_1_prt_1 no inherit rank_damage;
create table rank2_damage(like rank_damage);
alter table rank_damage_1_prt_1 inherit rank2_damage;
alter table rank_damage_1_prt_1 alter column i type bigint;
alter table rank_damage_1_prt_1 set without oids;
alter table rank_damage_1_prt_1 drop constraint rank_damage_1_prt_1_check;
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

select pg_get_partition_def('sg_cal_event_silvertail_hour'::regclass, true);

drop table sg_cal_event_silvertail_hour;

-- Make sure we inherit master's storage settings
create table foo_p (i int, j int, k text)
with (appendonly = true, compresslevel = 5)
partition by range(j) (start(1) end(10) every(1), default partition def);
insert into foo_p select i, i+1, repeat('fooo', 9000) from generate_series(1, 100) i;
alter table foo_p split default partition start (10) end(20) 
into (partition p10_20, partition def);
select reloptions from pg_class where relname = 'foo_p_1_prt_p10_20';
select count(distinct k) from foo_p;
drop table foo_p;

create table foo_p (i int, j int, k text)
partition by range(j) (start(1) end(10) every(1), default partition def
with(appendonly = true));
insert into foo_p select i, i+1, repeat('fooo', 9000) from generate_series(1, 100) i;
alter table foo_p split default partition start (10) end(20) 
into (partition p10_20, partition def);
select reloptions from pg_class where relname = 'foo_p_1_prt_p10_20';
select reloptions from pg_class where relname = 'foo_p_1_prt_def';
select count(distinct k) from foo_p;
drop table foo_p;


-- MPP-5878 - display correct partition boundary 

create table mpp5878 (a int, b char, d char)
partition by list (b,d)
(
values (('a','b'),('c','d')),
values (('e','f'),('g','h'))
);

create table mpp5878a (a int, b character(1), d character(1))
partition by list (b,d)
(
values (('a','b'),('c','d')),
values (('e','f'),('g','h'))
);

select tablename, partitionlistvalues from pg_partitions where tablename like 'mpp5878%';
select tablename, partitionboundary from pg_partitions where tablename like 'mpp5878%';

drop table mpp5878, mpp5878a;

-- MPP-5941: work with many levels of templates

CREATE TABLE mpp5941 (a int, b date, c char, 
	   		 		 d char(4), e varchar(20), f timestamp)
partition by range (b)
subpartition by list (a) 
subpartition template ( 
subpartition l1 values (1,2,3,4,5), 
subpartition l2 values (6,7,8,9,10) ),
subpartition by list (e) 
subpartition template ( 
subpartition ll1 values ('Engineering'), 
subpartition ll2 values ('QA') ),
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

select tablename, partitionname, partitionlevel from pg_partition_templates 
where tablename = 'mpp5941';

-- clear level 1

alter table mpp5941 set subpartition template ();

select tablename, partitionname, partitionlevel from pg_partition_templates 
where tablename = 'mpp5941';

-- clear level 2

alter table mpp5941 alter partition for ('2008-01-01') 
set subpartition template ();

select tablename, partitionname, partitionlevel from pg_partition_templates 
where tablename = 'mpp5941';

-- clear level 3

alter table mpp5941 alter partition for ('2008-01-01') 
alter partition for (1)
set subpartition template ();

select tablename, partitionname, partitionlevel from pg_partition_templates 
where tablename = 'mpp5941';

-- no level 4 (error)

alter table mpp5941 alter partition for ('2008-01-01') 
alter partition for (1) alter partition for ('QA')
set subpartition template ();

select tablename, partitionname, partitionlevel from pg_partition_templates 
where tablename = 'mpp5941';

-- no level 5 (error)

alter table mpp5941 alter partition for ('2008-01-01') 
alter partition for (1) alter partition for ('QA')
alter partition for ('M')
set subpartition template ();

select tablename, partitionname, partitionlevel from pg_partition_templates 
where tablename = 'mpp5941';

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

select tablename, partitionname, partitionlevel from pg_partition_templates 
where tablename = 'mpp5941';

-- set level 2

alter table mpp5941 alter partition for ('2008-01-01') 
set subpartition template (
subpartition ll1 values ('Engineering'), 
subpartition ll2 values ('QA') );

select tablename, partitionname, partitionlevel from pg_partition_templates 
where tablename = 'mpp5941';

-- set level 1

alter table mpp5941 set subpartition template (
subpartition l1 values (1,2,3,4,5), 
subpartition l2 values (6,7,8,9,10) );

select tablename, partitionname, partitionlevel from pg_partition_templates 
where tablename = 'mpp5941';


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

--MPP-6240
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
select pg_get_partition_def('supplier_hybrid_part'::regclass, true);
drop table supplier_hybrid_part;

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
create unique index ti_pkey on ti(i);

select * from pg_indexes where schemaname = 'public' and tablename like 'ti%';
create index ti_j_idx on ti using bitmap(j);
select * from pg_indexes where schemaname = 'public' and tablename like 'ti%';
alter table ti add partition p3 start(3) end(10);
select * from pg_indexes where schemaname = 'public' and tablename like 'ti%';
alter table ti split partition p3 at (7) into (partition pnew1, partition pnew2);
select * from pg_indexes where schemaname = 'public' and tablename like 'ti%';
drop table ti;

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

-- should be a single partition def for p1 from 1/31 to 4/30, but
-- shows 4 partitions instead
select partitiontablename, partitionname, 
partitionrangestart, partitionrangeend, partitioneveryclause
from pg_partitions
where tablename like 'mpp6297%' order by partitionrank;

select 
pg_get_partition_def(
(select oid from pg_class 
where relname='mpp6297')::pg_catalog.oid, true);

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

-- should be a single partition def for p1 from 1/31 to 4/30, as intended
select 
pg_get_partition_def(
(select oid from pg_class 
where relname='mpp6297')::pg_catalog.oid, true);

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

-- note that the partition from 10 to 11 is *not* part of every
select 
pg_get_partition_def(
(select oid from pg_class 
where relname='mpp6297')::pg_catalog.oid, true);

alter table mpp6297 drop partition for (rank(3));

-- note that the every clause splits into two parts: 1-3 and 4-10
select
pg_get_partition_def(
(select oid from pg_class
where relname='mpp6297')::pg_catalog.oid, true);

-- this isn't legal (but it would be nice)
alter table mpp6297 add partition start (3) end (4) every (1);

-- this is legal but it doesn't fix the EVERY clause
alter table mpp6297 add partition start (3) end (4) ;

-- note that the every clause is still splits into two parts: 1-3 and
-- 4-10, because the new partition from 3 to 4 doesn't have an EVERY
-- attribute
select
pg_get_partition_def(
(select oid from pg_class
where relname='mpp6297')::pg_catalog.oid, true);

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

-- note that there is only a single every from 1-10
select 
pg_get_partition_def(
(select oid from pg_class 
where relname='mpp6297')::pg_catalog.oid, true);

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

-- two every clauses for this case
select 
pg_get_partition_def(
(select oid from pg_class 
where relname='mpp6297')::pg_catalog.oid, true);

drop table mpp6297;

-- more fun with inclusivity/exclusivity (normal case)
create table mpp6297 
(a int, 
b int) 
partition by range (b)
(
start (1) inclusive end (10) exclusive every (1)
);

-- note that inclusive and exclusive attributes aren't listed here (because
-- default behavior)
select 
pg_get_partition_def(
(select oid from pg_class 
where relname='mpp6297')::pg_catalog.oid, true);

drop table mpp6297;

-- more fun with inclusivity/exclusivity (abnormal case)
create table mpp6297 
(a int, 
b int) 
partition by range (b)
(
start (1) exclusive end (10) inclusive every (1)
);

-- note that inclusive and exclusive attributes are listed here 
select 
pg_get_partition_def(
(select oid from pg_class 
where relname='mpp6297')::pg_catalog.oid, true);

alter table mpp6297 drop partition for (rank(3));

-- note that the every clause splits into two parts: 1-3 and 4-10 (and
-- inclusive/exclusive is listed correctly)
select
pg_get_partition_def(
(select oid from pg_class
where relname='mpp6297')::pg_catalog.oid, true);

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

-- two every clauses for this case
select 
pg_get_partition_def(
(select oid from pg_class 
where relname='mpp6297')::pg_catalog.oid, true);

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

select pg_get_partition_def('mpp6589a'::regclass,true);

-- should work
ALTER TABLE mpp6589a 
SPLIT PARTITION p20090312 AT( '20090310' ) 
INTO( PARTITION p20090309, PARTITION p20090312_tmp);

select pg_get_partition_def('mpp6589a'::regclass,true);

drop table mpp6589a;

CREATE TABLE mpp6589i(a int, b int) 
partition by range (b) (start (1) end (3));
select pg_get_partition_def('mpp6589i'::regclass,true);

-- should fail (overlap)
ALTER TABLE mpp6589i ADD PARTITION start (2);
-- should fail (overlap) (not a real overlap, but a "point" hole)
ALTER TABLE mpp6589i ADD PARTITION start (3) exclusive;

-- should work - make sure can add an open-ended final partition
ALTER TABLE mpp6589i ADD PARTITION start (3);
select pg_get_partition_def('mpp6589i'::regclass,true);

DROP TABLE mpp6589i;

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

select pg_get_partition_def('mpp6589b'::regclass,true);

-- should work
ALTER TABLE mpp6589b 
SPLIT PARTITION p20090312 AT( '20090310' ) 
INTO( PARTITION p20090309, PARTITION p20090312_tmp);

select pg_get_partition_def('mpp6589b'::regclass,true);

drop table mpp6589b;

-- MPP-7191, MPP-7193: partitioned tables - fully-qualify storage type
-- if not specified (and not a template)
CREATE TABLE mpp5992 (a int, b date, c char,
                     d char(4), e varchar(20), f timestamp)
WITH (orientation=column,appendonly=true)
partition by range (b)
subpartition by list (a)
subpartition template (
subpartition l1 values (1,2,3,4,5),
subpartition l2 values (6,7,8,9,10) ),
subpartition by list (e)
subpartition template (
subpartition ll1 values ('Engineering'),
subpartition ll2 values ('QA') ),
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
select * from pg_partition_templates where tablename='mpp5992';

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

select pg_get_partition_def('mpp5992'::regclass,true, true);

drop table mpp5992;

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

select pg_get_partition_def('mpp10223'::regclass,true);

drop table mpp10223;

-- simpler version
create table mpp10223b (a int, b int , d int)
partition by range (b)
subpartition by range (d)
(partition p1 start (1) end (10)
(subpartition sp2 start (20) end (30)));

-- MPP-10421: allow re-use sp2 for non-DEFAULT partition
alter table mpp10223b alter partition p1 
split partition for (rank(1) ) at (25)
into (partition sp2, partition sp3);

select partitiontablename,partitionposition,partitionrangestart,
       partitionrangeend from pg_partitions where tablename = 'mpp10223b'
           order by partitionposition;

select pg_get_partition_def('mpp10223b'::regclass,true);

drop table mpp10223b;

-- MPP-10480: dump templates (but don't use "foo")
create table MPP10480 (a int, b int, d int)
partition by range (b)
subpartition by range(d)
subpartition template (start (1) end (10) every (1))
(start (20) end (30) every (1));

select pg_get_partition_template_def('MPP10480'::regclass, true, true);

drop table MPP10480;

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
alter table cov1 drop default partition for (rank(1));

-- no default
alter table cov1 split default partition at (9);
alter table cov1 drop default partition;

-- cannot add except by name
alter table cov1 add partition for (rank(1));

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

select partitionboundary from pg_partitions where tablename = 'cov1';

drop table cov1;

-- MPP-11120
--  ADD PARTITION didn't explicitly specify the distribution policy in the
-- CreateStmt distributedBy field and as such we followed the behaviour encoded
-- in transformDistributedBy(). Unfortunately, it chooses to set the
-- distribution policy to that of the primary key if the distribution policy
-- is not explicitly set.
create table test_table (
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

insert into test_table values(1,2,3);

select * from test_table; -- expected: (1,2,3)

delete from test_table where a=1 and b=2 and c=3; -- this should delete the row in test_table

select * from test_table; -- expected, no rows

insert into test_table values(1,2,3); -- reinsert data

-- all partitions should have same distribution policy
select relname, attrnums as distribution_attributes from
gp_distribution_policy p, pg_class c
where p.localoid = c.oid and relname like 'test_table%' order by p.localoid;

alter table test_table split default partition
        start (3)
	end (4)
	into (partition p2, partition default_partition);


select relname, attrnums as distribution_attributes from
gp_distribution_policy p, pg_class c where p.localoid = c.oid and 
relname like 'test_table%' order by p.localoid;

delete from test_table where a=1 and b=2 and c=3; -- this should delete the row in test_table

select * from test_table; -- expected, no rows! But we see the row. Wrong results!

alter table test_table drop partition default_partition;

alter table test_table add partition foo start(10) end(20);

select relname, attrnums as distribution_attributes from
gp_distribution_policy p, pg_class c where p.localoid = c.oid and
relname like 'test_table%' order by p.localoid;

drop table test_table;

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
select schemaname, tablename, partitionschemaname, partitiontablename
from pg_partitions 
where tablename like ('mpp6979%');

-- note that we have heap partitions in public, and ao table in mpp6979dummy
select nspname, relname, relstorage from pg_class pc, pg_namespace ns 
where
pc.relnamespace=ns.oid and relname like ('mpp6979%');

-- exchange the partition with the ao table.  
-- Now we have an ao partition and mpp6979tab is heap!
alter table mpp6979part exchange partition for (rank(1)) 
with table mpp6979dummy.mpp6979tab;

-- after the exchange, all partitions are still in public
select schemaname, tablename, partitionschemaname, partitiontablename
from pg_partitions 
where tablename like ('mpp6979%');

-- the rank 1 partition is ao, but still in public, and 
-- table mpp6979tab is now heap, but still in mpp6979dummy
select nspname, relname, relstorage from pg_class pc, pg_namespace ns 
where
pc.relnamespace=ns.oid and relname like ('mpp6979%');

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
create table plst2
    (            
        a integer not null,
        b integer not null,
        c integer
    )                                                                                                                   
    distributed by (b) 
    partition by list (a,c)
    (
        partition p1 values ( (1, 2), (3, 4) ),
        partition p2 values ( (5, 6) ),
        partition p3 values ( (2, 1) )
    );
drop table if exists plst2 cascade;
--negative; test legitimate failure
create table plst2
    (            
        a integer not null,
        b integer not null,
        c integer
    )                                                                                                                   
    distributed by (b) 
    partition by list (a,c)
    (
        partition p1 values ( (1, 2), (3, 4) ),
        partition p2 values ( (5, 6) ),
        partition p3 values ( (1, 2) )
    );

-- postive; make sure inner part duplicates are accepted and quietly removed.
drop table if exists plst2;

create table plst2
    ( a int, b int)
    distributed by (a)
    partition by list (a, b) 
        (
            partition p0 values ((1,2), (3,4)),
            partition p1 values ((4,3), (2,1)),
            partition p2 values ((4,4),(5,5),(4,4),(5,5),(4,4),(5,5)),
            partition p3 values ((4,5),(5,6))
        );

-- positive; make sure legitimate alters work.
alter table plst2 add partition p4 values ((5,4),(6,5));
alter table plst2 add partition p5 values ((7,8),(7,8));

select conrelid::regclass, consrc  
from pg_constraint 
where conrelid in (
    select parchildrelid::regclass
    from pg_partition_rule
    where paroid in (
        select oid 
        from pg_partition 
        where parrelid = 'plst2'::regclass
        )
    );

-- negative; make sure conflicting alters fail.
alter table plst2 add partition p6 values ((7,8),(2,1));

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

-- MPP-18359
drop view if exists redundantly_named_part cascade;

create view redundantly_named_part(tableid, partid, partname) as
	with 
		dups(paroid, partname) as 
		(
			select paroid, parname
			from pg_partition_rule 
			where parname is not null 
			group by paroid, parname 
			having count(*) > 1
		),
		parts(tableid, partid, paroid, partname) as
		(
			select p.parrelid, r.parchildrelid, r.paroid, r.parname
			from pg_partition p, pg_partition_rule r
			where not p.paristemplate and
				p.oid = r.paroid
		)
	select p.tableid::regclass, p.partid::regclass, p.partname
	from parts p, dups d
	where 
		p.paroid = d.paroid and
		p.partname = d.partname;

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

select * from redundantly_named_part where tableid::text like '%pnx%';

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

select * from redundantly_named_part where tableid::text like '%pxn%';

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

select * from redundantly_named_part where tableid::text like '%pxn%';

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

select * from redundantly_named_part where tableid::text like '%pxn%';

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
set dml_ignore_target_partition_check=false;
drop table if exists part_tab;
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
drop table if exists deep_part;
create table deep_part ( i int, j int, k int, s char(5)) 
distributed by (i) 
partition by list(s)
subpartition by range (j) subpartition template (start(1)  end(10) every(2))
subpartition by range (k) subpartition template (start(1)  end(10) every(2))
(partition female values('F'), partition male values('M'))
;

-- Intermediate partition insert is not allowed
insert into deep_part_1_prt_male_2_prt_2 values(1,1,1,'M');

-- Wrong sub-partition (inserting a female value in male partition)
insert into deep_part_1_prt_male_2_prt_2_3_prt_2 values (1, 1, 1, 'F');
select * from deep_part;

-- Correct leaf part
insert into deep_part_1_prt_male_2_prt_1_3_prt_1 values (1, 1, 1, 'M');
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



-- Sub-partition insertion without checking for correct user provided leaf part
set dml_ignore_target_partition_check=true;

drop table if exists part_tab;
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
drop table if exists deep_part;
create table deep_part ( i int, j int, k int, s char(5)) 
distributed by (i) 
partition by list(s)
subpartition by range (j) subpartition template (start(1)  end(10) every(2))
subpartition by range (k) subpartition template (start(1)  end(10) every(2))
(partition female values('F'), partition male values('M'))
;

-- Intermediate partition insert is not allowed
insert into deep_part_1_prt_male_2_prt_2 values(1,1,1,'M');

-- Wrong sub-partition (inserting a female value in male partition)
insert into deep_part_1_prt_male_2_prt_2_3_prt_2 values (1, 1, 1, 'F');
select * from deep_part;

-- Correct leaf part
insert into deep_part_1_prt_male_2_prt_1_3_prt_1 values (1, 1, 1, 'M');
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
drop table part_tab;
drop table deep_part;

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

-- Test split default partition while per tuple memory context is reset
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

-- Test that pg_get_partition_def() correctly dumps the renamed names for
-- partitions. Originally reported in MPP-7232
create table mpp7232a (a int, b int) distributed by (a) partition by range (b) (start (1) end (3) every (1));
select pg_get_partition_def('mpp7232a'::regclass, true);
alter table mpp7232a rename partition for (rank(1)) to alpha;
alter table mpp7232a rename partition for (rank(2)) to bravo;
select partitionname, partitionrank from pg_partitions where tablename like 'mpp7232a' order by 2;
select pg_get_partition_def('mpp7232a'::regclass, true);

create table mpp7232b (a int, b int) distributed by (a) partition by range (b) (partition alpha start (1) end (3) every (1));
select partitionname, partitionrank from pg_partitions where tablename like 'mpp7232b' order by 2;
alter table mpp7232b rename partition for (rank(1)) to foo;
select pg_get_partition_def('mpp7232b'::regclass, true);

-- Test .. WITH (tablename = <foo> ..) syntax.
create table mpp17740 (a integer, b integer, e date) with (appendonly = true, orientation = column)
distributed by (a)
partition by range(e)
(
    partition mpp17740_20120523 start ('2012-05-23'::date) inclusive end ('2012-05-24'::date) exclusive with (tablename = 'mpp17740_20120523', appendonly = true),
    partition mpp17740_20120524 start ('2012-05-24'::date) inclusive end ('2012-05-25'::date) exclusive with (tablename = 'mpp17740_20120524', appendonly = true)
);

select partitiontablename, partitionrangestart, partitionrangeend from pg_partitions where tablename = 'mpp17740' order by partitiontablename;

alter table mpp17740 add partition mpp17740_20120520 start ('2012-05-20'::date) inclusive end ('2012-05-21'::date) exclusive with (tablename = 'mpp17740_20120520', appendonly=true);
select partitiontablename, partitionrangestart, partitionrangeend from pg_partitions where tablename = 'mpp17740' order by partitiontablename;

-- Test mix of add and drop various column before split, and exchange partition at the end
create table sales (pkid serial, option1 int, option2 int, option3 int, constraint partable_pkey primary key(pkid, option3))
distributed by (pkid) partition by range (option3)
(
	partition aa start(1) end(100),
	partition bb start(101) end(200),
	partition cc start(201) end (300)
);

alter table sales add column tax float;
alter table sales drop column tax;

create table newpart(like sales);
alter table newpart add constraint partable_pkey primary key(pkid, option3);
alter table sales split partition for(1) at (50) into (partition aa1, partition aa2);

select table_schema, table_name, constraint_name, constraint_type
	from information_schema.table_constraints
	where table_name in ('sales', 'newpart')
	and constraint_name = 'partable_pkey'
	order by table_name desc;

alter table sales exchange partition for (101) with table newpart;

select * from sales order by pkid;

-- Create exchange table before drop column, make sure the consistency check still exist
create table newpart2(like sales);
alter table sales drop column option2;

alter table sales exchange partition for (101) with table newpart2;

select * from sales order by pkid;
drop table sales cascade;

-- Ensure that new partitions get the correct attributes (MPP17110)
CREATE TABLE pt_tab_encode (a int, b text)
with (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1)
distributed by (a)
partition by list(b) (partition s_abc values ('abc') with (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1));

alter table pt_tab_encode add partition "s_xyz" values ('xyz') WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1);

select tablename, partitiontablename from pg_partitions where tablename = 'pt_tab_encode';

select gp_segment_id, attrelid::regclass, attnum, attoptions from pg_attribute_encoding where attrelid = 'pt_tab_encode_1_prt_s_abc'::regclass;
select gp_segment_id, attrelid::regclass, attnum, attoptions from gp_dist_random('pg_attribute_encoding') where attrelid = 'pt_tab_encode_1_prt_s_abc'::regclass order by 1,3 limit 5;

select gp_segment_id, attrelid::regclass, attnum, attoptions from pg_attribute_encoding where attrelid = 'pt_tab_encode_1_prt_s_xyz'::regclass;
select gp_segment_id, attrelid::regclass, attnum, attoptions from gp_dist_random('pg_attribute_encoding') where attrelid = 'pt_tab_encode_1_prt_s_xyz'::regclass order by 1,3 limit 5;

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid = 'pt_tab_encode_1_prt_s_abc'::regclass;
select oid::regclass, relkind, relstorage, reloptions from pg_class where oid = 'pt_tab_encode_1_prt_s_xyz'::regclass;
