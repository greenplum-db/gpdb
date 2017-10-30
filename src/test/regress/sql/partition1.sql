set enable_partition_rules = false;
set gp_enable_hash_partitioned_tables = true;

-- missing subpartition by
create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash (b)
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);

-- missing subpartition spec
create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash (b)
subpartition by hash (d) 
(
partition aa ,
partition bb 
);

-- subpart spec conflict
create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash (b) 
subpartition by hash (d) subpartition template (subpartition jjj)
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);

-- missing subpartition by
create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash (b)
subpartition by hash (d)
(
partition aa (subpartition cc, subpartition dd (subpartition iii)),
partition bb (subpartition cc, subpartition dd)
);

-- Test column lookup works
create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash(doesnotexist)
partitions 3;

create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash(b)
partitions 3
subpartition by list(alsodoesntexist)
subpartition template (
subpartition aa values(1)
);

-- should work
create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash (b)
subpartition by hash (d) 
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);

drop table ggg cascade;

-- disable hash partitions
set gp_enable_hash_partitioned_tables = false;

create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash (b)
subpartition by hash (d) 
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);

drop table ggg cascade;

set gp_enable_hash_partitioned_tables = true;

-- should work
create table ggg (a char(1), b char(2), d char(3), e int)
distributed by (a)
partition by hash (b)
subpartition by hash (d) 
subpartition template ( 
subpartition cc,
subpartition dd
), 
subpartition by hash (e) 
subpartition template ( 
subpartition ee,
subpartition ff
) 
(
partition aa,
partition bb
);

drop table ggg cascade;

-- should work
create table ggg (a char(1), b char(2), d char(3), e int)
distributed by (a)
partition by hash (b)
subpartition by hash (d),
subpartition by hash (e)
subpartition template ( 
subpartition ee,
subpartition ff
) 
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);

drop table ggg cascade;

-- doesn't work because cannot have nested declaration in template
create table ggg (a char(1), b char(2), d char(3), e int)
distributed by (a)
partition by hash (b)
subpartition by hash (d) 
subpartition template ( 
subpartition cc (subpartition ee, subpartition ff),
subpartition dd (subpartition ee, subpartition ff)
), 
subpartition by hash (e) 
(
partition aa,
partition bb
);

drop table ggg cascade;

--ERROR: Missing boundary specification in partition 'aa' of type LIST 
create table fff (a char(1), b char(2), d char(3)) distributed by
(a) partition by list (b) (partition aa ); 


-- ERROR: Invalid use of RANGE boundary specification in partition
--   number 1 of type LIST
create table fff (a char(1), b char(2), d char(3)) distributed by (a)
partition by list (b) (start ('a') );


-- should work
create table fff (a char(1), b char(2), d char(3)) distributed by (a)
partition by list (b) (partition aa values ('2'));

drop table fff cascade;

-- Invalid use of RANGE boundary specification in partition "cc" of 
-- type HASH (at depth 2)
create table ggg (a char(1), b char(2), d char(3), e int) distributed by (a)
partition by hash (b) subpartition by hash (d),
subpartition by hash (e) 
subpartition template ( subpartition ee, subpartition ff ) (
partition aa (subpartition cc, subpartition dd), partition bb
(subpartition cc start ('a') , subpartition dd) );

-- this is subtly wrong -- it defines 4 partitions
-- the problem is the comma before "end", which causes us to
-- generate 2 anonymous partitions.
-- This is an error: 
-- ERROR:  invalid use of mixed named and unnamed RANGE boundary specifications
create table ggg (a char(1), b int, d char(3))
distributed by (a)
partition by range (b)
(
partition aa start ('2007'), end ('2008'),
partition bb start ('2008'), end ('2009')
);

create table ggg (a char(1), b int)
distributed by (a)
partition by range(b)
(
partition aa start ('2007'), end ('2008')
);

drop table ggg cascade;

create table ggg (a char(1), b date, d char(3))
distributed by (a)
partition by range (b)
(
partition aa start (date '2007-01-01') end (date '2008-01-01'),
partition bb start (date '2008-01-01') end (date '2009-01-01')
);


drop table ggg cascade;

-- don't allow nonconstant expressions, even simple ones...
create table ggg (a char(1), b numeric, d numeric)
distributed by (a)
partition by range (b,d)
(
partition aa start (2007,1) end (2008,2+2),
partition bb start (2008,2) end (2009,3)
);

-- too many columns for RANGE partition
create table ggg (a char(1), b numeric, d numeric)
distributed by (a)
partition by range (b,d)
(
partition aa start (2007,1) end (2008,2),
partition bb start (2008,2) end (2009,3)
);


drop table ggg cascade;

-- demo starts here

-- nested subpartitions
create table ggg
 (a char(1),   b date,
  d char(3),  e numeric,
  f numeric,  g numeric,
  h numeric)
distributed by (a)
partition by hash(b)
partitions 2
subpartition by hash(d)
subpartitions 2,
subpartition by hash(e) subpartitions 2,
subpartition by hash(f) subpartitions 2,
subpartition by hash(g) subpartitions 2,
subpartition by hash(h) subpartitions 2;

drop table ggg cascade;


-- named, inline subpartitions
create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash (b)
subpartition by hash (d) 
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);

drop table ggg cascade;


-- subpartitions with templates
create table ggg (a char(1), b char(2), d char(3), e numeric)
distributed by (a)
partition by hash (b)
subpartition by hash (d) 
subpartition template ( 
subpartition cc,
subpartition dd
), 
subpartition by hash (e) 
subpartition template ( 
subpartition ee,
subpartition ff
) 
(
partition aa,
partition bb
);

drop table ggg cascade;


-- mixed inline subpartition declarations with templates
create table ggg (a char(1), b char(2), d char(3), e numeric)
distributed by (a)
partition by hash (b)
subpartition by hash (d) , 
subpartition by hash (e) 
subpartition template ( 
subpartition ee,
subpartition ff
) 
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);

drop table ggg cascade;


-- basic list partition
create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by LIST (b)
(
partition aa values ('a', 'b', 'c', 'd'),
partition bb values ('e', 'f', 'g')
);

insert into ggg values ('x', 'a');
insert into ggg values ('x', 'b');
insert into ggg values ('x', 'c');
insert into ggg values ('x', 'd');
insert into ggg values ('x', 'e');
insert into ggg values ('x', 'f');
insert into ggg values ('x', 'g');
insert into ggg values ('x', 'a');
insert into ggg values ('x', 'b');
insert into ggg values ('x', 'c');
insert into ggg values ('x', 'd');
insert into ggg values ('x', 'e');
insert into ggg values ('x', 'f');
insert into ggg values ('x', 'g');

select * from ggg order by 1, 2;

-- ok
select * from ggg_1_prt_aa order by 1, 2;
select * from ggg_1_prt_bb order by 1, 2;

drop table ggg cascade;

-- documentation example - partition by list and range
CREATE TABLE rank (id int, rank int, year date, gender 
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

insert into rank values (1, 1, date '2001-01-15', 'M');
insert into rank values (2, 1, date '2002-02-15', 'M');
insert into rank values (3, 1, date '2003-03-15', 'M');
insert into rank values (4, 1, date '2004-04-15', 'M');
insert into rank values (5, 1, date '2005-05-15', 'M');
insert into rank values (6, 1, date '2001-01-15', 'F');
insert into rank values (7, 1, date '2002-02-15', 'F');
insert into rank values (8, 1, date '2003-03-15', 'F');
insert into rank values (9, 1, date '2004-04-15', 'F');
insert into rank values (10, 1, date '2005-05-15', 'F');

select * from rank order by 1, 2, 3, 4;
select * from rank_1_prt_boys order by 1, 2, 3, 4;
select * from rank_1_prt_girls order by 1, 2, 3, 4;
select * from rank_1_prt_girls_2_prt_1 order by 1, 2, 3, 4;
select * from rank_1_prt_girls_2_prt_2 order by 1, 2, 3, 4;


drop table rank cascade;



-- range list hash combo
create table ggg (a char(1), b date, d char(3), e numeric)
distributed by (a)
partition by range (b)
subpartition by list(d),
subpartition by hash(e) subpartitions 3
(
partition aa 
start  (date '2007-01-01') 
end (date '2008-01-01') 
       (subpartition dd values ('1', '2', '3'), 
	    subpartition ee values ('4', '5', '6')),
partition bb
start  (date '2008-01-01') 
end (date '2009-01-01') 
       (subpartition dd values ('1', '2', '3'),
	    subpartition ee values ('4', '5', '6'))
);

drop table ggg cascade;


-- demo ends here


-- LIST validation

-- duplicate partition name
CREATE TABLE rank (id int, rank int, year date, gender
char(1)) DISTRIBUTED BY (id, gender, year)
partition by list (gender)
(
  partition boys values ('M'),
  partition girls values ('a'),
  partition girls values ('b'),
  partition girls values ('c'),
  partition girls values ('d'),
  partition girls values ('e'),
  partition bob values ('M')
);

-- duplicate values
CREATE TABLE rank (id int, rank int, year date, gender
char(1)) DISTRIBUTED BY (id, gender, year)
partition by list (rank,gender)
(
 values ((1, 'M')),
 values ((2, 'M')),
 values ((3, 'M')),
 values ((1, 'F')),
 partition ff values ((4, 'M')),
 partition bb values ((1, 'M'))
);


-- RANGE validation

-- legal if end of aa not inclusive
create table ggg (a char(1), b date, d char(3)) 
distributed by (a)
partition by range (b)
(
partition aa start (date '2007-01-01') end (date '2008-01-01'),
partition bb start (date '2008-01-01') end (date '2009-01-01') 
every (interval '10 days'));

drop table ggg cascade;


-- bad - legal if end of aa not inclusive
create table ggg (a char(1), b date, d char(3)) 
distributed by (a)
partition by range (b)
(
partition aa start (date '2007-01-01') end (date '2008-01-01') inclusive,
partition bb start (date '2008-01-01') end (date '2009-01-01') 
every (interval '10 days'));

drop table ggg cascade;

-- legal because start of bb not inclusive
create table ggg (a char(1), b date, d char(3)) 
distributed by (a)
partition by range (b)
(
partition aa start (date '2007-01-01') end (date '2008-01-01') inclusive,
partition bb start (date '2008-01-01') exclusive end (date '2009-01-01') 
every (interval '10 days'));

drop table ggg cascade;

-- legal if end of aa not inclusive
create table ggg (a char(1), b date, d char(3)) 
distributed by (a)
partition by range (b)
(
partition bb start (date '2008-01-01') end (date '2009-01-01'),
partition aa start (date '2007-01-01') end (date '2008-01-01')
);

drop table ggg cascade;

-- bad - legal if end of aa not inclusive
create table ggg (a char(1), b date, d char(3)) 
distributed by (a)
partition by range (b)
(
partition bb start (date '2008-01-01') end (date '2009-01-01'),
partition aa start (date '2007-01-01') end (date '2008-01-01') inclusive
);

drop table ggg cascade;

-- legal because start of bb not inclusive
create table ggg (a char(1), b date, d char(3)) 
distributed by (a)
partition by range (b)
(
partition bb start (date '2008-01-01') exclusive end (date '2009-01-01'),
partition aa start (date '2007-01-01') end (date '2008-01-01') inclusive
);

drop table ggg cascade;

-- validate aa - start greater than end
create table ggg (a char(1), b date, d char(3)) 
distributed by (a)
partition by range (b)
(
partition bb start (date '2008-01-01') end (date '2009-01-01'),
partition aa start (date '2007-01-01') end (date '2006-01-01')
);

drop table ggg cascade;

-- formerly we could not set end of first partition because next is before
-- but we can sort them now so this is legal.
create table ggg (a char(1), b date, d char(3)) 
distributed by (a)
partition by range (b)
(
partition bb start (date '2008-01-01') ,
partition aa start (date '2007-01-01') 
);

drop table ggg cascade;

-- test cross type coercion
-- int -> char(N)
create table ggg (i int, a char(1))
distributed by (i)
partition by list(a)
(partition aa values(1, 2));

drop table ggg cascade;

-- int -> numeric
create table ggg (i int, n numeric(20, 2))
distributed by (i)
partition by list(n)
(partition aa values(1.22, 4.1));
drop table ggg cascade;

-- EVERY

--  the documentation example, rewritten with EVERY in a template
CREATE TABLE rank (id int,
rank int, year date, gender char(1))
DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01')
end (date '2006-01-01') every (interval '1 year')) (
partition boys values ('M'),
partition girls values ('F')
);


insert into rank values (1, 1, date '2001-01-15', 'M');
insert into rank values (2, 1, date '2002-02-15', 'M');
insert into rank values (3, 1, date '2003-03-15', 'M');
insert into rank values (4, 1, date '2004-04-15', 'M');
insert into rank values (5, 1, date '2005-05-15', 'M');
insert into rank values (6, 1, date '2001-01-15', 'F');
insert into rank values (7, 1, date '2002-02-15', 'F');
insert into rank values (8, 1, date '2003-03-15', 'F');
insert into rank values (9, 1, date '2004-04-15', 'F');
insert into rank values (10, 1, date '2005-05-15', 'F');


select * from rank order by 1, 2, 3, 4;
select * from rank_1_prt_boys order by 1, 2, 3, 4;
select * from rank_1_prt_girls order by 1, 2, 3, 4;
select * from rank_1_prt_girls_2_prt_1 order by 1, 2, 3, 4;
select * from rank_1_prt_girls_2_prt_2 order by 1, 2, 3, 4;

drop table rank cascade;

-- integer ranges work too
create table ggg (id integer, a integer)
distributed by (id)
partition by range (a)
(start (1) end (10) every (1));

insert into ggg values (1, 1);
insert into ggg values (2, 2);
insert into ggg values (3, 3);
insert into ggg values (4, 4);
insert into ggg values (5, 5);
insert into ggg values (6, 6);
insert into ggg values (7, 7);
insert into ggg values (8, 8);
insert into ggg values (9, 9);
insert into ggg values (10, 10);

select * from ggg order by 1, 2;

select * from ggg_1_prt_1 order by 1, 2;
select * from ggg_1_prt_2 order by 1, 2;
select * from ggg_1_prt_3 order by 1, 2;
select * from ggg_1_prt_4 order by 1, 2;

drop table ggg cascade;

-- hash tests

create table ggg (a char(1), b varchar(2), d varchar(2))
distributed by (a)
partition by hash(b)
partitions 3
(partition a, partition b, partition c);

insert into ggg values (1,1,1);
insert into ggg values (2,2,1);
insert into ggg values (1,3,1);
insert into ggg values (2,2,3);
insert into ggg values (1,4,5);
insert into ggg values (2,2,4);
insert into ggg values (1,5,6);
insert into ggg values (2,7,3);
insert into ggg values (1,'a','b');
insert into ggg values (2,'c','c');

select * from ggg order by 1, 2, 3;

--select * from ggg_1_prt_a order by 1, 2, 3;
--select * from ggg_1_prt_b order by 1, 2, 3;
--select * from ggg_1_prt_c order by 1, 2, 3;

drop table ggg cascade;

-- use multiple cols
create table ggg (a char(1), b varchar(2), d varchar(2))
distributed by (a)
partition by hash(b,d)
partitions 3
(partition a, partition b, partition c);

insert into ggg values (1,1,1);
insert into ggg values (2,2,1);
insert into ggg values (1,3,1);
insert into ggg values (2,2,3);
insert into ggg values (1,4,5);
insert into ggg values (2,2,4);
insert into ggg values (1,5,6);
insert into ggg values (2,7,3);
insert into ggg values (1,'a','b');
insert into ggg values (2,'c','c');

select * from ggg order by 1, 2, 3;

--select * from ggg_1_prt_a order by 1, 2, 3;
--select * from ggg_1_prt_b order by 1, 2, 3;
--select * from ggg_1_prt_c order by 1, 2, 3;

drop table ggg cascade;

-- use multiple cols of different types and without a partition spec
create table ggg (a char(1), b varchar(2), d integer, e date)
distributed by (a)
partition by hash(b,d,e)
partitions 3;

insert into ggg values (1,1,1,date '2001-01-15');
insert into ggg values (2,2,1,date '2001-01-15');
insert into ggg values (1,3,1,date '2001-01-15');
insert into ggg values (2,2,3,date '2001-01-15');
insert into ggg values (1,4,5,date '2001-01-15');
insert into ggg values (2,2,4,date '2001-01-15');
insert into ggg values (1,5,6,date '2001-01-15');
insert into ggg values (2,7,3,date '2001-01-15');
insert into ggg values (1,'a',33,date '2001-01-15');
insert into ggg values (2,'c',44,date '2001-01-15');

select * from ggg order by 1, 2, 3, 4;

--select * from ggg_1_prt_1 order by 1, 2, 3, 4;
--select * from ggg_1_prt_2 order by 1, 2, 3, 4;
--select * from ggg_1_prt_3 order by 1, 2, 3, 4;

drop table ggg cascade;


create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partItion by hash(b)
partitions 3;

drop table ggg cascade;

create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash (b)
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);

drop table ggg cascade;

create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by hash (b)
subpartition by hash (d) 
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);

drop table ggg cascade;

create table fff (a char(1), b char(2), d char(3)) distributed by (a)
partition by list (b) (partition aa values ('2'));

drop table fff cascade;

create table ggg (a char(1), b numeric, d numeric)
distributed by (a)
partition by range (b,d)
(
partition aa start (2007,1) end (2008,2),
partition bb start (2008,2) end (2009,3)
);


drop table ggg cascade;

create table ggg (a char(1), b date, d char(3), e numeric)
distributed by (a)
partition by range (b)
subpartition by list(d),
subpartition by hash(e) subpartitions 3
(
partition aa 
start  (date '2007-01-01') 
end (date '2008-01-01') 
       (subpartition dd values (1,2,3), subpartition ee values (4,5,6)),
partition bb
start  (date '2008-01-01') 
end (date '2009-01-01') 
       (subpartition dd values (1,2,3), subpartition ee values (4,5,6))

);

drop table ggg cascade;

create table ggg (a char(1), b date, d char(3)) 
distributed by (a)
partition by range (b)
(
partition bb start (date '2008-01-01') end (date '2009-01-01'),
partition aa start (date '2007-01-01') end (date '2006-01-01')
);

drop table ggg cascade;

create table ggg (a char(1), b varchar(2), d varchar(2))
distributed by (a)
partition by hash(b)
partitions 3;

insert into ggg values (1,1,1);
insert into ggg values (2,2,1);
insert into ggg values (1,3,1);
insert into ggg values (2,2,3);
insert into ggg values (1,4,5);
insert into ggg values (2,2,4);
insert into ggg values (1,5,6);
insert into ggg values (2,7,3);
insert into ggg values (1,'a','b');
insert into ggg values (2,'c','c');

select * from ggg;

select * from ggg_1_prt_1;
select * from ggg_1_prt_2;
select * from ggg_1_prt_3;

drop table ggg cascade;



-- append only tests
create table foz (i int, d date) with (appendonly = true) distributed by (i)
partition by range (d) (start (date '2001-01-01') end (date '2005-01-01')
every(interval '1 year'));
insert into foz select i, '2001-01-01'::date + ('1 day'::interval * i) from
generate_series(1, 1000) i;
select count(*) from foz;
select count(*) from foz_1_prt_1;

select min(d), max(d) from foz;
select min(d), max(d) from foz_1_prt_1;
select min(d), max(d) from foz_1_prt_2;
select min(d), max(d) from foz_1_prt_3;
select min(d), max(d) from foz_1_prt_4;


drop table foz cascade;

-- copy test
create table foz (i int, d date) distributed by (i)
partition by range (d) (start (date '2001-01-01') end (date '2005-01-01')
every(interval '1 year'));
COPY foz FROM stdin DELIMITER '|';
1|2001-01-2
2|2001-10-10
3|2002-10-30
4|2003-01-01
5|2004-05-05
\.
select * from foz_1_prt_1;
select * from foz_1_prt_2;
select * from foz_1_prt_3;
select * from foz_1_prt_4;

-- Check behaviour of key for which there is no partition
COPY foz FROM stdin DELIMITER '|';
6|2010-01-01
\.
drop table foz cascade;
-- Same test with append only
create table foz (i int, d date) with (appendonly = true) distributed by (i)
partition by range (d) (start (date '2001-01-01') end (date '2005-01-01')
every(interval '1 year'));
COPY foz FROM stdin DELIMITER '|';
1|2001-01-2
2|2001-10-10
3|2002-10-30
4|2003-01-01
5|2004-05-05
\.
select * from foz_1_prt_1;
select * from foz_1_prt_2;
select * from foz_1_prt_3;
select * from foz_1_prt_4;

-- Check behaviour of key for which there is no partition
COPY foz FROM stdin DELIMITER '|';
6|2010-01-01
\.
drop table foz cascade;


-- complain if create table as select (CTAS)

CREATE TABLE rank1 (id int,
rank int, year date, gender char(1));

create table rank2 as select * from rank1
DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01')
end (date '2006-01-01') every (interval '1 year')) (
partition boys values ('M'),
partition girls values ('F')
);

-- like is ok

create table rank2 (like rank1)
DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01')
end (date '2006-01-01') every (interval '1 year')) (
partition boys values ('M'),
partition girls values ('F')
);

drop table rank1 cascade;
drop table rank2 cascade;


-- alter table testing

create table hhh (a char(1), b date, d char(3))
distributed by (a)
partition by range (b)
(
partition aa start (date '2007-01-01') end (date '2008-01-01') 
    with (appendonly=true),
partition bb start (date '2008-01-01') end (date '2009-01-01')
    with (appendonly=false)
);

-- already exists
alter table hhh add partition aa;

-- no partition spec
alter table hhh add partition cc;

-- overlaps
alter table hhh add partition cc start ('2008-01-01') end ('2010-01-01');
alter table hhh add partition cc end ('2008-01-01');

-- reversed (start > end)
alter table hhh add partition cc start ('2010-01-01') end ('2009-01-01');

-- works
--alter table hhh add partition cc start ('2009-01-01') end ('2010-01-01');
alter table hhh add partition cc end ('2010-01-01');

-- works - anonymous partition MPP-3350
alter table hhh add partition end ('2010-02-01');

-- MPP-3607 - ADD PARTITION with open intervals
create table no_end1 (aa int, bb int) partition by range (bb)
(partition foo start(3));

-- fail overlap
alter table no_end1 add partition baz end (4);

-- fail overlap (because prior partition has no end)
alter table no_end1 add partition baz start (5);

-- ok (terminates on foo start)
alter table no_end1 add partition baz start (2);

-- ok (because ends before baz start)
alter table no_end1 add partition baz2 end (1);

create table no_start1 (aa int, bb int) partition by range (bb)
(partition foo end(3));

-- fail overlap (because next partition has no start)
alter table no_start1 add partition baz start (2);

-- fail overlap (because next partition has no start)
alter table no_start1 add partition baz end (1);

-- ok (starts on foo end)
alter table no_start1 add partition baz end (4);

-- ok (because starts after baz end)
alter table no_start1 add partition baz2 start (5);

select tablename, partitionlevel, parentpartitiontablename,
partitionname, partitionrank, partitionboundary from pg_partitions
where tablename = 'no_start1' or tablename = 'no_end1' 
order by tablename, partitionrank;

drop table no_end1;
drop table no_start1;

-- hash partitions cannot have default partitions
create table jjj (aa int, bb int) 
partition by hash(bb) 
(partition j1, partition j2);

alter table jjj add default partition;

drop table jjj cascade;

-- default partitions cannot have boundary specifications
create table jjj (aa int, bb date) 
partition by range(bb) 
(partition j1 end (date '2008-01-01'), 
partition j2 end (date '2009-01-01'));

-- must have a name
alter table jjj add default partition;
alter table jjj add default partition for (rank(1));
-- cannot have boundary spec
alter table jjj add default partition j3 end (date '2010-01-01');

drop table jjj cascade;

-- only one default partition
create table jjj (aa int, bb date) 
partition by range(bb) 
(partition j1 end (date '2008-01-01'), 
partition j2 end (date '2009-01-01'),
default partition j3);

alter table jjj add default partition j3 ;
alter table jjj add default partition j4 ;

-- cannot add if have default, must split
alter table jjj add partition j5 end (date '2010-01-01');

drop table jjj cascade;

alter table hhh alter partition cc set tablespace foo_p;

alter table hhh alter partition aa set tablespace foo_p;

alter table hhh coalesce partition cc;

alter table hhh coalesce partition aa;

alter table hhh drop partition cc;

alter table hhh drop partition cc cascade;

alter table hhh drop partition cc restrict;

alter table hhh drop partition if exists cc;

-- fail (mpp-3265)
alter table hhh drop partition for (rank(0));
alter table hhh drop partition for (rank(-55));
alter table hhh drop partition for ('2001-01-01');


create table hhh_r1 (a char(1), b date, d char(3))
distributed by (a)
partition by range (b)
(
partition aa start (date '2007-01-01') end (date '2008-01-01') 
             every (interval '1 month')
);

create table hhh_l1 (a char(1), b date, d char(3))
distributed by (a)
partition by list (b)
(
partition aa values ('2007-01-01'),
partition bb values ('2008-01-01'),
partition cc values ('2009-01-01') 
);

-- must have name or value for list partition
alter table hhh_l1 drop partition;
alter table hhh_l1 drop partition aa;
alter table hhh_l1 drop partition for ('2008-01-01');

-- if not specified, drop first range partition...
alter table hhh_r1 drop partition for ('2007-04-01');
alter table hhh_r1 drop partition;
alter table hhh_r1 drop partition;
alter table hhh_r1 drop partition;
alter table hhh_r1 drop partition;
alter table hhh_r1 drop partition;

-- more add partition tests

-- start before first partition (fail because start equal end)
alter table hhh_r1 add partition zaa start ('2007-07-01');
-- start before first partition (ok)
alter table hhh_r1 add partition zaa start ('2007-06-01');
-- start > last (fail because start equal end)
alter table hhh_r1 add partition bb start ('2008-01-01') end ('2008-01-01') ;
-- start > last (ok)
alter table hhh_r1 add partition bb start ('2008-01-01') 
end ('2008-02-01') inclusive;
-- start > last (fail because start == last end inclusive)
alter table hhh_r1 add partition cc start ('2008-02-01') end ('2008-03-01') ;
-- start > last (ok [and leave a gap])
alter table hhh_r1 add partition cc start ('2008-04-01') end ('2008-05-01') ;
-- overlap (fail)
alter table hhh_r1 add partition dd start ('2008-01-01') end ('2008-05-01') ;
-- new partition in "gap" (ok)
alter table hhh_r1 add partition dd start ('2008-03-01') end ('2008-04-01') ;
-- overlap all partitions (fail)
alter table hhh_r1 add partition ee start ('2006-01-01') end ('2009-01-01') ;
-- start before first partition (fail because end in "gap" [and overlaps])
alter table hhh_r1 add partition yaa start ('2007-05-01') end ('2007-07-01');
-- start before first partition (fail )
alter table hhh_r1 add partition yaa start ('2007-05-01') 
end ('2007-10-01') inclusive;
-- start before first partition (fail because end overlaps)
alter table hhh_r1 add partition yaa start ('2007-05-01') 
end ('2007-10-01') exclusive;


drop table hhh_r1 cascade;
drop table hhh_l1 cascade;


--  the documentation example, rewritten with EVERY in a template
--  and also with a default partition
CREATE TABLE rank (id int,
rank int, year date, gender char(1))
DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01')
end (date '2006-01-01') every (interval '1 year')) (
partition boys values ('M'),
partition girls values ('F'),
default partition neuter
);

insert into rank values (1, 1, date '2001-01-15', 'M');
insert into rank values (2, 1, date '2002-02-15', 'M');
insert into rank values (3, 1, date '2003-03-15', 'M');
insert into rank values (4, 1, date '2004-04-15', 'M');
insert into rank values (5, 1, date '2005-05-15', 'M');
insert into rank values (6, 1, date '2001-01-15', 'F');
insert into rank values (7, 1, date '2002-02-15', 'F');
insert into rank values (8, 1, date '2003-03-15', 'F');
insert into rank values (9, 1, date '2004-04-15', 'F');
insert into rank values (10, 1, date '2005-05-15', 'F');


select * from rank ;

alter table rank DROP partition boys restrict;

select * from rank ;

-- MPP-3722: complain if for(value) matches the default partition 
alter table rank truncate partition for('N');
alter table rank DROP partition for('N');
alter table rank DROP partition if exists for('N');

alter table rank DROP default partition if exists ;

-- can't drop the final partition - must drop the table
alter table rank DROP partition girls;

-- MPP-4011: make FOR(value) work
alter table rank alter partition for ('F') add default partition def1;
alter table rank alter partition for ('F') 
truncate partition for ('2010-10-10');
alter table rank truncate partition for ('F');

drop table rank cascade;

alter table hhh exchange partition cc with table nosuchtable with validation;

alter table hhh exchange partition cc with table nosuchtable without validation;

alter table hhh exchange partition aa with table nosuchtable with validation;

alter table hhh exchange partition aa with table nosuchtable without validation;

alter table hhh merge partition cc, partition dd;

alter table hhh merge partition cc, partition dd into partition ee;

alter table hhh merge partition aa, partition dd into partition ee;

alter table hhh modify partition cc add values ('a');
alter table hhh modify partition cc drop values ('a');
alter table hhh modify partition aa add values ('a');
alter table hhh modify partition aa drop values ('a');

create table mmm_r1 (a char(1), b date, d char(3))
distributed by (a)
partition by range (b)
(
partition aa start (date '2007-01-01') end (date '2008-01-01')
             every (interval '1 month')
);

create table mmm_l1 (a char(1), b char(1), d char(3))
distributed by (a)
partition by list (b)
(
partition aa values ('a', 'b', 'c'),
partition bb values ('d', 'e', 'f'),
partition cc values ('g', 'h', 'i')
);

alter table mmm_r1 drop partition for ('2007-03-01');
-- ok
alter table mmm_r1 add partition bb START ('2007-03-03') END ('2007-03-20');

-- fail
alter table mmm_r1 modify partition for (rank(-55)) start ('2007-03-02');
alter table mmm_r1 modify partition for ('2001-01-01') start ('2007-03-02');
alter table mmm_r1 modify partition bb start ('2006-03-02');
alter table mmm_r1 modify partition bb start ('2011-03-02');
alter table mmm_r1 modify partition bb end ('2006-03-02');
alter table mmm_r1 modify partition bb end ('2011-03-02');
alter table mmm_r1 modify partition bb add values ('2011-03-02');
alter table mmm_r1 modify partition bb drop values ('2011-03-02');

--ok
alter table mmm_r1 modify partition bb START ('2007-03-02') END ('2007-03-22');
alter table mmm_r1 modify partition bb START ('2007-03-01') END ('2007-03-31');
alter table mmm_r1 modify partition bb START ('2007-03-02') END ('2007-03-22');

-- with default
alter table mmm_r1 add default partition def1;

-- now fail
alter table mmm_r1 modify partition bb START ('2007-03-01') END ('2007-03-31');

-- still ok to reduce range
alter table mmm_r1 modify partition bb START ('2007-03-09') END ('2007-03-10');

-- fail
alter table mmm_l1 modify partition for (rank(1)) drop values ('k');
alter table mmm_l1 modify partition for ('j') drop values ('k');
alter table mmm_l1 modify partition for ('a') drop values ('k');
alter table mmm_l1 modify partition for ('a') drop values ('e');
alter table mmm_l1 modify partition for ('a') add values ('e');

alter table mmm_l1 modify partition for ('a') START ('2007-03-09') ;


--ok
alter table mmm_l1 modify partition for ('a') drop values ('b');
alter table mmm_l1 modify partition for ('a') add values ('z');

-- with default
alter table mmm_l1 add default partition def1;
-- ok
alter table mmm_l1 modify partition for ('a') drop values ('c');
-- now fail
alter table mmm_l1 modify partition for ('a') add values ('y');

-- XXX XXX: add some data 

drop table mmm_r1 cascade;
drop table mmm_l1 cascade;



alter table hhh rename partition cc to aa;
alter table hhh rename partition bb to aa;
alter table hhh rename partition aa to aa;
alter table hhh rename partition aa to "funky fresh";
alter table hhh rename partition "funky fresh" to aa;

-- use FOR PARTITION VALUE (with implicate date conversion)
alter table hhh rename partition for ('2007-01-01') to "funky fresh";
alter table hhh rename partition for ('2007-01-01') to aa;


alter table hhh set subpartition template ();

alter table hhh split partition cc at ('a');
alter table hhh split partition cc at ('a') into (partition gg, partition hh);
alter table hhh split partition aa at ('a');

alter table hhh truncate partition cc;

alter table hhh truncate partition aa;

insert into hhh values('a', date '2007-01-02', 'b');
insert into hhh values('a', date '2007-02-01', 'b');
insert into hhh values('a', date '2007-03-01', 'b');
insert into hhh values('a', date '2007-04-01', 'b');
insert into hhh values('a', date '2007-05-01', 'b');
insert into hhh values('a', date '2007-06-01', 'b');
insert into hhh values('a', date '2007-07-01', 'b');
insert into hhh values('a', date '2007-08-01', 'b');
insert into hhh values('a', date '2007-09-01', 'b');
insert into hhh values('a', date '2007-10-01', 'b');
insert into hhh values('a', date '2007-11-01', 'b');
insert into hhh values('a', date '2007-12-01', 'b');
insert into hhh values('a', date '2008-01-02', 'b');
insert into hhh values('a', date '2008-02-01', 'b');
insert into hhh values('a', date '2008-03-01', 'b');
insert into hhh values('a', date '2008-04-01', 'b');
insert into hhh values('a', date '2008-05-01', 'b');
insert into hhh values('a', date '2008-06-01', 'b');
insert into hhh values('a', date '2008-07-01', 'b');
insert into hhh values('a', date '2008-08-01', 'b');
insert into hhh values('a', date '2008-09-01', 'b');
insert into hhh values('a', date '2008-10-01', 'b');
insert into hhh values('a', date '2008-11-01', 'b');
insert into hhh values('a', date '2008-12-01', 'b');

select * from hhh;

alter table hhh truncate partition aa;

select * from hhh;

alter table hhh truncate partition bb;

select * from hhh;

insert into hhh values('a', date '2007-01-02', 'b');
insert into hhh values('a', date '2007-02-01', 'b');
insert into hhh values('a', date '2007-03-01', 'b');
insert into hhh values('a', date '2007-04-01', 'b');
insert into hhh values('a', date '2007-05-01', 'b');
insert into hhh values('a', date '2007-06-01', 'b');
insert into hhh values('a', date '2007-07-01', 'b');
insert into hhh values('a', date '2007-08-01', 'b');
insert into hhh values('a', date '2007-09-01', 'b');
insert into hhh values('a', date '2007-10-01', 'b');
insert into hhh values('a', date '2007-11-01', 'b');
insert into hhh values('a', date '2007-12-01', 'b');
insert into hhh values('a', date '2008-01-02', 'b');
insert into hhh values('a', date '2008-02-01', 'b');
insert into hhh values('a', date '2008-03-01', 'b');
insert into hhh values('a', date '2008-04-01', 'b');
insert into hhh values('a', date '2008-05-01', 'b');
insert into hhh values('a', date '2008-06-01', 'b');
insert into hhh values('a', date '2008-07-01', 'b');
insert into hhh values('a', date '2008-08-01', 'b');
insert into hhh values('a', date '2008-09-01', 'b');
insert into hhh values('a', date '2008-10-01', 'b');
insert into hhh values('a', date '2008-11-01', 'b');
insert into hhh values('a', date '2008-12-01', 'b');

select * from hhh;

-- truncate child partitions recursively
truncate table hhh;

select * from hhh;


drop table hhh cascade;

-- default partitions

-- hash partitions cannot have default partitions
create table jjj (aa int, bb int) 
partition by hash(bb) 
(partition j1, partition j2, default partition j3);

-- default partitions cannot have boundary specifications
create table jjj (aa int, bb date) 
partition by range(bb) 
(partition j1 end (date '2008-01-01'), 
partition j2 end (date '2009-01-01'), 
default partition j3 end (date '2010-01-01'));

-- more than one default partition
create table jjj (aa int, bb date) 
partition by range(bb) 
(partition j1 end (date '2008-01-01'), 
partition j2 end (date '2009-01-01'), 
default partition j3,
default partition j4);


-- check default
create table foz (i int, d date) distributed by (i)
partition by range (d) 
(
 default partition dsf,
 partition foo start (date '2001-01-01') end (date '2005-01-01')
               every(interval '1 year')
);
insert into foz values(1, '2003-04-01');
insert into foz values(2, '2010-04-01');
select * from foz;
select * from foz_1_prt_dsf;
drop table foz cascade;

-- check for out of order partition definitions. We should order these correctly
-- and determine the appropriate boundaries.
create table d (i int, j int) distributed by (i) partition by range(j)
( start (10), start(5), start(50) end(60));
insert into d values(1, 5);
insert into d values(1, 10);
insert into d values(1, 11);
insert into d values(1, 55);
insert into d values(1, 70);
select * from d;
select * from d_1_prt_1;
select * from d_1_prt_2;
select * from d_1_prt_3;
drop table d cascade;

-- check for NULL support
-- hash
create table d (i int, j int) partition by hash(j) partitions 4;
insert into d values(1, NULL);
insert into d values(NULL, NULL);
drop table d cascade;
-- list
create table d (i int, j int) partition by list(j)
(partition a values(1, 2, NULL),
 partition b values(3, 4)
);
insert into d values(1, 1);
insert into d values(1, 2);
insert into d values(1, NULL);
insert into d values(1, 3);
insert into d values(1, 4);
select * from d_1_prt_a;
select * from d_1_prt_b;
drop table d cascade;
--range
-- Reject NULL values
create table d (i int,  j int) partition by range(j)
(partition a start (1) end(10), partition b start(11) end(20));
insert into d values (1, 1);
insert into d values (1, 2);
insert into d values (1, NULL);
drop table  d cascade;
-- allow NULLs into the default partition
create table d (i int,  j int) partition by range(j)
(partition a start (1) end(10), partition b start(11) end(20),
default partition abc);
insert into d values (1, 1);
insert into d values (1, 2);
insert into d values (1, NULL);
select * from d_1_prt_abc;
drop table  d cascade;

-- multicolumn list support
create table d (a int, b int, c int) distributed by (a) 
partition by list(b, c)
(partition a values(('1', '2'), ('3', '4')),
 partition b values(('100', '20')),
 partition c values(('1000', '1001'), ('1001', '1002'), ('1003', '1004')));

insert into d values(1, 1, 2);
insert into d values(1, 3, 4);
insert into d values(1, 100, 20);
insert into d values(1, 100, 2000);
insert into d values(1, '1000', '1001'), (1, '1001', '1002'), (1, '1003', '1004');
insert into d values(1, 100, NULL);
select * from d_1_prt_a;
select * from d_1_prt_b;
select * from d_1_prt_c;
drop table d cascade;

-- test multi value range partitioning
create table b (i int, j date) distributed by (i)
partition by range (i, j)
(start(1, '2008-01-01') end (10, '2009-01-01'),
 start(1, '2009-01-01') end(15, '2010-01-01'),
 start(15, '2010-01-01') end (30, '2011-01-01'),
 start(1, '2011-01-01') end (100, '2012-01-01')
);
-- should work
insert into b values(1, '2008-06-11');
insert into b values(11, '2009-08-24');
insert into b values(25, '2010-01-22');
insert into b values(90, '2011-05-04');
-- shouldn't work
insert into b values(1, '2019-01-01');
insert into b values(91, '2008-05-05');
 
select * from b_1_prt_1;
select * from b_1_prt_2;
select * from b_1_prt_3;
select * from b_1_prt_4;
drop table b;

-- try some different combinations
create table b (i int, n numeric(20, 2), t timestamp, s text)
distributed by (i)
partition by range(n, t, s)
(
start(2000.99, '2007-01-01 00:00:00', 'AAA')
  end (4000.95, '2007-02-02 15:00:00', 'BBB'),

start(2000.99, '2007-01-01 00:00:00', 'BBB')
  end (4000.95, '2007-02-02 16:00:00', 'CCC'),

start(4000.95, '2007-01-01 00:00:00', 'AAA')
  end (7000.95, '2007-02-02 15:00:00', 'BBB')
);

-- should work
insert into b values(1, 2000.99, '2007-01-01 00:00:00', 'AAA');
insert into b values(2, 2000.99, '2007-01-01 00:00:00', 'BBB');
insert into b values(3, 4000.95, '2007-01-01 00:00:00', 'AAA');
insert into b values(6, 3000, '2007-02-02 15:30:00', 'BBC');
insert into b values(6, 3000, '2007-02-02 15:30:00', 'CC');
insert into b values(6, 3000, '2007-02-02 16:00:00'::timestamp - 
					'1 second'::interval, 'BBZZZZZZZZZZ');

-- should fail
insert into b values(6, 3000, '2007-02-02 15:30:00', 'CCCCCCC');
insert into b values(4, 5000, '2007-01-01 12:00:00', 'BCC');
insert into b values(5, 8000, '2007-01-01 12:00:00', 'ZZZ');
insert into b values(6, 3000, '2007-02-02 16:00:00', 'ABZZZZZZZZZZ');
insert into b values(6, 1000, '2007-02-02 16:00:00', 'ABZZZZZZZZZZ');
insert into b values(6, 3000, '2006-02-02 16:00:00', 'ABZZZZZZZZZZ');
insert into b values(6, 3000, '2007-02-02 00:00:00', 'A');

-- NULL tests
insert into b default values;
insert into b values(6, 3000, '2007-01-01 12:00:00', NULL);
drop table b;

-- check that we detect subpartitions partitioning a column that is already
-- a partitioning target
create table a (i int, b int)
distributed by (i)
partition by range (i)
subpartition by hash(b) subpartitions 3,
subpartition by hash(b) subpartitions 2
(start(1) end(100),
 start(100) end(1000)
);

-- MPP-3988: allow same column in multiple partitioning keys at
-- different levels -- so this is legal again...
drop table if exists a;

-- TEST: make sure GPOPT (aka pivotal query optimizer) fall back to legacy query optimizer 
--       for queries with partition elimination over FULL OUTER JOIN
--       between partitioned tables.

-- SETUP
-- start_ignore
drop table if exists s1;
drop table if exists s2;

-- setup two partitioned tables s1 and s2
create table s1 (d1 int, p1 int)
distributed by (d1)
partition by list (p1)
(
  values (0),
  values (1));

create table s2 (d2 int, p2 int)
distributed by (d2)
partition by list (p2)
(
  values (0),
  values (1));
-- end_ignore

-- VERIFY
-- expect GPOPT fall back to legacy query optimizer
-- since GPOPT don't support partition elimination through full outer joins
select * from s1 full outer join s2 on s1.d1 = s2.d2 and s1.p1 = s2.p2 where s1.p1 = 1;

-- CLEANUP
-- start_ignore
drop table if exists s1;
drop table if exists s2;
-- end_ignore


create table mpp_2914A(id int,  buyDate date, kind char(1))
DISTRIBUTED BY (id)
partition by list (kind) 
subpartition by range(buyDate) 
subpartition template 
(
        start (date '2001-01-01'),
        start (date '2002-01-01'),
        start (date '2003-01-01'),
        start (date '2004-01-01'),
        start (date '2005-01-01')
)
(
        partition auction  values('a','A'),
        partition buyItNow values('b', 'B'),
        default partition catchall
);
select count(*) from mpp_2914A;

\d mpp_2914a*

create table mpp_2914B(id int,  buyDate date, kind char(1))
DISTRIBUTED BY (id)
partition by list (kind)
subpartition by range(buyDate)
(
        partition auction  values('a','A')
        (
                subpartition  y2001 start (date '2001-01-01'),
                subpartition  y2002 start (date '2002-01-01'),
                subpartition  y2003 start (date '2003-01-01'),
                subpartition y2004 start (date '2004-01-01'),
                subpartition y2005 start (date '2005-01-01')
        ),
        partition buyitnow  values('b','B')
        (
                subpartition  y2001 start (date '2001-01-01'),
                subpartition  y2002 start (date '2002-01-01'),
                subpartition  y2003 start (date '2003-01-01'),
                subpartition y2004 start (date '2004-01-01'),
                subpartition y2005 start (date '2005-01-01')
        ),
        default partition catchAll  
        (
                subpartition  y2001 start (date '2001-01-01'),
                subpartition  y2002 start (date '2002-01-01'),
                subpartition  y2003 start (date '2003-01-01'),
                subpartition y2004 start (date '2004-01-01'),
                subpartition y2005 start (date '2005-01-01')
        )
);
select count(*) from mpp_2914B;

\d mpp_2914b*

drop table mpp_2914a cascade;
drop table mpp_2914b cascade;

create table mpp10847_pkeyconstraints(
  pkid serial,
  option1 int,
  option2 int,
  option3 int,
  primary key(pkid, option3))
distributed by (pkid) partition by range (option3)
(
partition aa start(1) end(100) inclusive,
partition bb start(101) end(200) inclusive,
partition cc start(201) end (300) inclusive
);

insert into mpp10847_pkeyconstraints values (10000, 50, 50, 102);
-- This is supposed to fail as you're not supposed to be able to use the same
-- primary key in the same table. But GPDB cannot currently enforce that.
insert into mpp10847_pkeyconstraints values (10000, 50, 50, 5);

select * from mpp10847_pkeyconstraints;

drop table mpp10847_pkeyconstraints;


-- Test that ADD/EXCHANGE/SPLIT PARTITION works, even when there are partial or expression
-- indexes on the table. (MPP-13750)
create table dcl_messaging_test
(
        message_create_date     timestamp(3) not null,
        trace_socket            varchar(1024) null,
        trace_count             varchar(1024) null,
        variable_01             varchar(1024) null,
        variable_02             varchar(1024) null,
        variable_03             varchar(1024) null,
        variable_04             varchar(1024) null,
        variable_05             varchar(1024) null,
        variable_06             varchar(1024) null,
        variable_07             varchar(1024) null,
        variable_08             varchar(1024) null,
        variable_09             varchar(1024) null,
        variable_10             varchar(1024) null,
        variable_11             varchar(1024) null,
        variable_12             varchar(1024) null,
        variable_13             varchar(1024) default('-1'),
        variable_14             varchar(1024) null,
        variable_15             varchar(1024) null,
        variable_16             varchar(1024) null,
        variable_17             varchar(1024) null,
        variable_18             varchar(1024) null,
        variable_19             varchar(1024) null,
        variable_20             varchar(1024) null,
        variable_21             varchar(1024) null,
        variable_22             varchar(1024) null,
        variable_23             varchar(1024) null,
        variable_24             varchar(1024) null,
        variable_25             varchar(1024) null,
        variable_26             varchar(1024) null,
        variable_27             varchar(1024) null,
        variable_28             varchar(1024) null,
        variable_29             varchar(1024) null,
        variable_30             varchar(1024) null,
        variable_31             varchar(1024) null,
        variable_32             varchar(1024) null,
        variable_33             varchar(1024) null,
        variable_34             varchar(1024) null,
        variable_35             varchar(1024) null,
        variable_36             varchar(1024) null,
        variable_37             varchar(1024) null,
        variable_38             varchar(1024) null,
        variable_39             varchar(1024) null,
        variable_40             varchar(1024) null,
        variable_41             varchar(1024) null,
        variable_42             varchar(1024) null,
        variable_43             varchar(1024) null,
        variable_44             varchar(1024) null,
        variable_45             varchar(1024) null,
        variable_46             varchar(1024) null,
        variable_47             varchar(1024) null,
        variable_48             varchar(1024) null,
        variable_49             varchar(1024) null,
        variable_50             varchar(1024) null,
        variable_51             varchar(1024) null,
        variable_52             varchar(1024) null,
        variable_53             varchar(1024) null,
        variable_54             varchar(1024) null,
        variable_55             varchar(1024) null,
        variable_56             varchar(1024) null,
        variable_57             varchar(1024) null,
        variable_58             varchar(1024) null,
        variable_59             varchar(1024) null,
        variable_60             varchar(1024) null
)
distributed by (message_create_date)
partition by range (message_create_date)
(
    START (timestamp '2011-09-01') END (timestamp '2011-09-15') EVERY (interval '1 day'),
    DEFAULT PARTITION outlying_dates
);
-- partial index
create index dcl_messaging_test_index13 on dcl_messaging_test(variable_13) where message_create_date > '2011-09-02';
-- expression index
create index dcl_messaging_test_index16 on dcl_messaging_test(upper(variable_16));
alter table dcl_messaging_test drop default partition;

-- ADD case
alter table dcl_messaging_test add partition start (timestamp '2011-09-15') inclusive end (timestamp '2011-09-16') exclusive;

-- EXCHANGE case
create table dcl_candidate(like dcl_messaging_test) with (appendonly=true);
insert into dcl_candidate(message_create_date) values (timestamp '2011-09-06');
alter table dcl_messaging_test exchange partition for ('2011-09-06') with table dcl_candidate;

-- SPLIT case
alter table dcl_messaging_test split partition for (timestamp '2011-09-06') at (timestamp '2011-09-06 12:00:00') into (partition x1, partition x2);


--
-- Create table with 4 partitions
CREATE TABLE mpp13806 (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( START (date '2008-01-01') INCLUSIVE
 END (date '2008-01-05') EXCLUSIVE
 EVERY (INTERVAL '1 day') );

-- Add unbound partition right before the start succeeds
alter table mpp13806 add partition test end (date '2008-01-01') exclusive;
select partitiontablename, partitionrangestart, partitionstartinclusive, partitionrangeend, partitionendinclusive from pg_partitions where tablename like 'mpp13806%' order by partitionrank;

-- Drop the partition
alter TABLE mpp13806 drop partition test;

-- Add unbound partition with a gap succeeds
alter table mpp13806 add partition test end (date '2007-12-31') exclusive;
select partitiontablename, partitionrangestart, partitionstartinclusive, partitionrangeend, partitionendinclusive from pg_partitions where tablename like 'mpp13806%' order by partitionrank;

-- Fill the gap succeeds/adding immediately before the first partition succeeds
alter table mpp13806 add partition test1 start (date '2007-12-31') inclusive end (date '2008-01-01') exclusive;
select partitiontablename, partitionrangestart, partitionstartinclusive, partitionrangeend, partitionendinclusive from pg_partitions where tablename like 'mpp13806%' order by partitionrank;


--
-- Create two tables mpp14613_range (range partitioned) and
-- mpp14613_list (list partitioned) with 5 partitions (including default
-- partition) and 3 subpartitions (including default subpartition) each
create table mpp14613_list(
  a int,
  b int,
  c int,
  d int)
  partition by range(b)
  subpartition by list(c)
  subpartition template
 (
    default subpartition subothers,
    subpartition s1 values(1,2,3),
    subpartition s2 values(4,5,6)
 )
 (
    default partition others,
    start(1) end(5) every(1)
 );

create table mpp14613_range(
  a int,
  b int,
  c int,
  d int
 )
  partition by range(b)
  subpartition by range(c)
  subpartition template
 (
     default subpartition subothers,
     start (1) end(7) every (3)
 )
 (
     default partition others,
     start(1) end(5) every(1)
 );

-- Check the partition and subpartition details
select tablename,partitiontablename, partitionname from pg_partitions where tablename in ('mpp14613_list','mpp14613_range');

-- SPLIT partition
alter table mpp14613_list alter partition others split partition subothers at (10) into (partition b1, partition b2);
alter table mpp14613_range alter partition others split partition subothers at (10) into (partition b1, partition b2);

-- ALTER TABLE ... ALTER PARTITION ... SPLIT DEFAULT PARTITION
create table foo(
  a int,
  b int,
  c int,
  d int)
  partition by range(b)
  subpartition by list(c)
  subpartition template
 (
    default subpartition subothers,
    subpartition s1 values(1,2,3),
    subpartition s2 values(4,5,6)
 )
 (
    default partition others,
    start(1) end(5) every(1)
 );

alter table foo alter partition others split partition subothers at (10) into (partition b1, default partition);
alter table foo alter partition others split partition subothers at (10) into (partition b1, partition subothers);
alter table foo alter partition others split default partition at (10) into (partition b1, default partition);
drop table foo;

-- Drop table as gpcheckcat will complaint of not having constraint for newly
-- created tables due to split.
drop table mpp14613_list;

--
-- Drop index on a partitioned table. The indexes on the partitions remain.
--
create table pt_indx_tab (c1 integer, c2 int, c3 text) partition by range (c1) (partition A start (integer '0') end (integer '5') every (integer '1'));

create unique index pt_indx_drop on pt_indx_tab(c1);

select count(*) from pg_index where indrelid='pt_indx_tab'::regclass;
select count(*) from pg_index where indrelid='pt_indx_tab_1_prt_a_1'::regclass;

drop index pt_indx_drop;

select count(*) from pg_index where indrelid='pt_indx_tab'::regclass;
select count(*) from pg_index where indrelid='pt_indx_tab_1_prt_a_1'::regclass;


--
-- MPP-18162 CLONE (4.2.3) - List partitioning for multiple columns gives duplicate values error
--
create table mpp18162a
( i1 int, i2 int)
distributed by (i1)
partition by list (i1, i2) (
  partition pi0 values ( (1,1) ),
  partition pi1 values ( (1,2) ),
  partition pi2 values ( (2,1) )
);

create table mpp18162b
( i1 int, i2 int)
distributed by (i1)
partition by list (i1, i2) (
  partition pi1 values ( (3,1), (1,3) ),
  partition pi2 values ( (4,1), (1,4) )
);

create table mpp18162c
( i1 text, i2 varchar(10))
distributed by (i1)
partition by list (i1, i2) (
  partition pi0 values ( ('1','1') ),
  partition pi1 values ( ('1','2') ),
  partition pi2 values ( ('2','1') )
);

create table mpp18162d
( i1 text, i2 varchar(10))
distributed by (i1)
partition by list (i1, i2) (
  partition pi1 values ( ('3','1'), ('1','3') ),
  partition pi2 values ( ('4','1'), ('1','4') )
);

create table mpp18162e
( i1 date, i2 date)
distributed by (i1)
partition by list (i1, i2) (
  partition pi1 values ( (date '2008-01-01',date '2008-02-01') ),
  partition pi2 values ( (date '2008-02-01',date '2008-01-01') ),
  partition pi3 values ( (date '2008-03-01',date '2008-04-01') ),
  partition pi4 values ( (date '2008-04-01',date '2008-03-01') )
);

create table mpp18162f
( i1 text, i2 varchar(10))
distributed by (i1)
partition by list (i1, i2) (
  partition pi1 values ( (date '2008-01-01',date '2008-02-01'), (date '2008-02-01',date '2008-01-01') ),
  partition pi2 values ( (date '2008-03-01',date '2008-04-01'), (date '2008-04-01',date '2008-03-01') )
);


--
-- Test changing the datatype of a column in a partitioning key column.
-- (Not supported, throws an error).
--
create table mpp18179 (a int, b int, i int)
distributed by (a)
partition by list (a,b)
   ( PARTITION ab1 VALUES ((1,1)),
     PARTITION ab2 values ((1,2)),
     default partition other
   );

alter table mpp18179 alter column a type varchar(20);
alter table mpp18179 alter column b type varchar(20);


--
-- Drop index on partitioned table, and recreate it.
--
CREATE TABLE mpp7635_aoi_table2 (id INTEGER)
 PARTITION BY RANGE (id)
  (START (0) END (200000) EVERY (100000))
;
INSERT INTO mpp7635_aoi_table2(id) VALUES (0);

-- Create index
CREATE INDEX mpp7635_ix3 ON mpp7635_aoi_table2 USING BITMAP (id);
select * from pg_indexes where tablename like 'mpp7635%';

-- Drop it. This only drops it from the root table, not the partitions.
DROP INDEX mpp7635_ix3;
select * from pg_indexes where tablename like 'mpp7635%';

-- Create it again. This creates the index on the partitions, too, so you
-- end up with duplicate indexes on the partitions. It's a bit silly, but
-- should still work, and not throw a "relation already exists" error, for
-- example.
CREATE INDEX mpp7635_ix3 ON mpp7635_aoi_table2 (id);
select * from pg_indexes where tablename like 'mpp7635%';


--
-- Test handling of NULL values in SPLIT PARTITION.
--
CREATE TABLE mpp7863 (id int, dat char(8))
DISTRIBUTED BY (id)
PARTITION BY RANGE (dat)
( PARTITION Oct09 START (200910) INCLUSIVE END (200911) EXCLUSIVE ,
PARTITION Nov09 START (200911) INCLUSIVE END (200912) EXCLUSIVE ,
PARTITION Dec09 START (200912) INCLUSIVE END (201001) EXCLUSIVE ,
DEFAULT PARTITION extra);

insert into mpp7863 values(generate_series(1, 100),'200910');
insert into mpp7863 values(generate_series(101, 200),'200911');
insert into mpp7863 values(generate_series(201, 300),'200912');
insert into mpp7863 values(generate_series(301, 30300),'');
insert into mpp7863 (id) values(generate_series(30301, 60300));
insert into mpp7863 values(generate_series(60301, 60400),'201001');

select count(*) from mpp7863_1_prt_extra;
select count(*) from mpp7863_1_prt_extra where dat is null;
select count(*) from mpp7863_1_prt_extra where dat ='';
select count(*) from mpp7863;

alter table mpp7863 split default partition start (201001) inclusive end (201002) exclusive into (partition jan10,default partition);
select count(*) from mpp7863_1_prt_extra where dat is null;
select count(*) from mpp7863_1_prt_extra where dat ='';
select count(*) from mpp7863_1_prt_extra;

select dat, count(*) from mpp7863 group by 1 order by 2,1;


-- Test that pg_get_expr() can be used on the pg_partition_rule columns that
-- store expressions, even by non-superusers. pg_get_expr() is restricted
-- to specific system catalogs, for security reasons.
create role part_expr_role;

CREATE TABLE part_expr_test_range (id int) DISTRIBUTED BY (id)
PARTITION BY RANGE(id) (START (1::int) END (10::int) EVERY (5));
CREATE TABLE part_expr_test_list (id int) DISTRIBUTED BY (id)
PARTITION BY list(id) (partition p1 values(1, 2, 3));

set session authorization part_expr_role;

-- This should throw a "not allowed" error.
select pg_get_expr('bogus', 'pg_class'::regclass);

-- But this should
select p.parrelid::regclass, pr.parchildrelid::regclass,
       pg_get_expr(parrangestart, pr.parchildrelid),
       pg_get_expr(parrangeend, pr.parchildrelid),
       pg_get_expr(parrangeevery, pr.parchildrelid),
       pg_get_expr(parlistvalues, pr.parchildrelid)
from pg_partition_rule pr, pg_partition p
where pr.paroid = p.oid
and p.parrelid in ('part_expr_test_range'::regclass, 'part_expr_test_list'::regclass);

reset session authorization;

DROP TABLE part_expr_test_range;
DROP TABLE part_expr_test_list;
DROP ROLE part_expr_role;
