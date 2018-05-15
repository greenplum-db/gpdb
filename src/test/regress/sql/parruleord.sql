-- Tests on partition ordering, i.e. pg_partition_rule.parruleord attribute.

drop table if exists pt_range;
create table pt_range (aa int, bb int) partition by range (bb) 
(partition foo1 start(3) inclusive end(6) exclusive, 
 partition foo2 start(6) inclusive end(9) exclusive,
 partition foo3 start(9) inclusive end(12) exclusive,
 partition foo4 start(12) inclusive end(15) exclusive,
 partition foo5 start(15) inclusive end(18) exclusive);

alter table pt_range drop partition foo2;

select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

-- Add partition before first, without start
alter table pt_range add partition foo6 end (0) inclusive;

select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

alter table pt_range drop partition foo6;

-- Add partition before first, end = first start.
alter table pt_range add partition foo7 start(1) inclusive end(3) exclusive;

select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

alter table pt_range drop partition foo1;

-- Add partition before first, end < first start.
alter table pt_range add partition foo8 start (-100) end (-50);
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;
alter table pt_range drop partition foo8;

-- Add in-between, new start = existing end, new end < existing start
alter table pt_range drop partition foo4;
alter table pt_range add partition foo10 start (12) inclusive end (14) exclusive;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;
alter table pt_range drop partition foo10;

-- Add in-between, new start = existing end, new end = existing start
alter table pt_range add partition foo4 start (12) inclusive end (15) exclusive;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;
alter table pt_range drop partition foo4;

-- Add in-between, new start > existing end, new end = existing start
alter table pt_range add partition foo11 start (13) end (15) exclusive;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;
alter table pt_range drop partition foo11;

-- Add in-between, new start > existing end, new end < existing start
alter table pt_range add partition foo9 start (4) end (5);
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;
alter table pt_range drop partition foo9;

drop table pt_range;
create table pt_range (aa int, bb int) partition by range (bb)
(partition foo1 start(3) inclusive end(6) exclusive,
 partition foo2 start(6) inclusive end(9) exclusive,
 partition foo3 start(9) inclusive end(12) exclusive,
 partition foo4 start(12) inclusive end(15) exclusive,
 partition foo5 start(15) inclusive end(18) exclusive,
 partition foo6 start(18) inclusive end(21) exclusive,
 partition foo7 start(21) inclusive end(25) exclusive);

alter table pt_range drop partition foo3;
alter table pt_range drop partition foo5;
alter table pt_range drop partition foo6;

select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

-- Add before first, only start specified
alter table pt_range add partition foo8 start(1);

select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

-- create two holes, make sure that the new partiton plugs the hole
-- that is closest to the end.
alter table pt_range drop partition foo2;

select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

-- Add after last, start and end specified, new start > last end
alter table pt_range add partition foo9
   start(26) inclusive end(30) exclusive;

select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

-- Add after last, new start = last end
alter table pt_range add partition foo10
   start(30) inclusive end(35) exclusive;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

-- this should plug the hole created by dropping foo2.
alter table pt_range add partition foo11 start(35) end(40);

-- ensure that we handle the case when first parruleord is
-- greater than 1.
alter table pt_range drop partition foo8;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

-- Add after last, new first = last end
alter table pt_range add partition foo12 start(40) end(45);
alter table pt_range add partition foo13 start(45) end(50);
alter table pt_range add partition foo14 start(50) end(55);
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

drop table pt_range;
create table pt_range (aa int, bb int) partition by range (bb)
(partition foo1 start(3) inclusive end(6) exclusive,
 partition foo2 start(6) inclusive end(9) exclusive,
 partition foo3 start(9) inclusive end(12) exclusive,
 partition foo4 start(12) inclusive end(15) exclusive,
 partition foo5 start(15) inclusive end(18) exclusive,
 partition foo6 start(18) inclusive end(21) exclusive,
 partition foo7 start(21) inclusive end(25) exclusive);

alter table pt_range drop partition foo3;
alter table pt_range drop partition foo5;
alter table pt_range drop partition foo6;

-- Add after last, only end specified
alter table pt_range add partition foo8 end(30);
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

-- Add after last, only start specified, new start > last end
alter table pt_range add partition foo9 start(31);
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

drop table pt_range;
create table pt_range (aa int, bb int) partition by range (bb)
(partition foo1 start(3) inclusive end(6) exclusive,
 partition foo2 start(9) inclusive end(12) exclusive,
 partition foo3 start(12) inclusive end(15) exclusive,
 partition foo4 start(15) inclusive end(18) exclusive,
 partition foo5 start(21) inclusive end(25) exclusive,
 partition foo6 start(25) inclusive end(28) exclusive,
 partition foo7 start(35) inclusive end(38) exclusive);

alter table pt_range drop partition foo5;
-- New partition falls after foo1, hole exists beyond (after foo4).
alter table pt_range add partition foo8
   start(6) inclusive end(9) exclusive;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

alter table pt_range drop partition foo8;
-- New partition falls after foo6, hole exists before foo3.
alter table pt_range add partition foo9 start(29) end (31);
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

alter table pt_range drop partition foo1;
-- Hole before first, new partition falls in the middle.
alter table pt_range add partition foo10
   start(21) inclusive end(25) exclusive;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

-- New partition falls in the middle, no hole exists.
alter table pt_range add partition foo11
   start(32) inclusive end(34) exclusive;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

-- Split a partition in the middle
insert into pt_range values (9, 9), (10, 10), (11, 11), (25, 25),
   (26, 26), (27, 27), (29, 29), (30, 30);
alter table pt_range split partition foo6 at (26) into
   (partition foo6_1, partition foo6_2);
select * from pt_range_1_prt_foo6_1 order by aa;
select * from pt_range_1_prt_foo6_2 order by aa;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;

alter table pt_range add default partition def;
insert into pt_range select i, i from generate_series(1, 40)i;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;
select count(*) from pt_range_1_prt_def;

-- Split default partition
alter table pt_range split default partition
   start(38) inclusive end(45) exclusive into
   (partition def, partition foo12);
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;
select count(*) from pt_range_1_prt_def;
select count(*) from pt_range_1_prt_foo12;

-- Drop all but one partitions
alter table pt_range drop partition foo2;
alter table pt_range drop partition foo3;
alter table pt_range drop partition foo4;
alter table pt_range drop partition foo10;
alter table pt_range drop partition foo6_1;
alter table pt_range drop partition foo6_2;
alter table pt_range drop partition foo9;
alter table pt_range drop partition foo11;
alter table pt_range drop partition foo7;
alter table pt_range drop partition foo12;

-- Split the only existing partition
alter table pt_range split default partition
   start(-10) inclusive end(0) exclusive into
   (partition def, partition foo1);
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_range'::regclass
   order by parruleord;
insert into pt_range values (-5, -5), (20, 20);
select count(*) from pt_range_1_prt_def;
select count(*) from pt_range_1_prt_foo1;

-- Test for list partitions
drop table if exists pt_list;
create table pt_list (a char(1), b char(2), d char(3)) distributed by (a)
partition by list (b) (partition aa values('1'),
	     	       partition bb values('2'),
		       partition cc values('3'),
		       partition dd values('4'));

alter table pt_list drop partition cc;
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_list'::regclass
   order by parruleord;

alter table pt_list add partition cc values('5');
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_list'::regclass
   order by parruleord;

alter table pt_list drop partition aa;

alter table pt_list add partition aa values('1');
select parchildrelid::regclass, parname, parruleord
   from pg_partition_rule r left join pg_partition p
   on p.oid = r.paroid where p.parrelid = 'pt_list'::regclass
   order by parruleord;

drop table if exists pt_range;
drop table if exists pt_list;

-- Check that adding a partition above a gap never decrements the
-- lowest ranked partition so that parruleord < 1. For example,
-- inserting between 4 and 5 in order [1,4,5,6,7] should change the
-- order to [1,3,4,5,6,7] and not [0,3,4,5,6,7].
CREATE TABLE test_partitioned_table_never_decrements_parruleord_to_zero(a int, b int)
PARTITION BY RANGE(a)
(
  PARTITION starter
  START (1) END (2),
  PARTITION part0
  START (2) END (3),
  PARTITION part1
  START (14) END (15),
  PARTITION part4
  START (15) END (16),
  PARTITION part5
  START (16) END (17)
);

-- create gap such that parruleord sequence is [1,4,5,6,7]
ALTER TABLE test_partitioned_table_never_decrements_parruleord_to_zero ADD PARTITION partnew3 START(3) END(4);
ALTER TABLE test_partitioned_table_never_decrements_parruleord_to_zero ADD PARTITION partnew4 START(4) END(5);
ALTER TABLE test_partitioned_table_never_decrements_parruleord_to_zero DROP PARTITION FOR (RANK(2));
ALTER TABLE test_partitioned_table_never_decrements_parruleord_to_zero DROP PARTITION FOR (RANK(2));

SELECT parchildrelid::regclass, parname, parruleord
   FROM pg_partition_rule r LEFT JOIN pg_partition p
   ON p.oid = r.paroid
   WHERE p.parrelid = 'test_partitioned_table_never_decrements_parruleord_to_zero'::regclass
   ORDER BY parruleord;

-- insert a partition that will take existing parruleord 4
-- partition partnew4 should decrement to 3 to close gap and partition starter should stay at 1
ALTER TABLE test_partitioned_table_never_decrements_parruleord_to_zero ADD PARTITION partnew5 START(5) END(6);

SELECT parchildrelid::regclass, parname, parruleord
   FROM pg_partition_rule r LEFT JOIN pg_partition p
   ON p.oid = r.paroid
   WHERE p.parrelid = 'test_partitioned_table_never_decrements_parruleord_to_zero'::regclass
   ORDER BY parruleord;
