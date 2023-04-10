
drop table if exists d;
drop table if exists c;
drop table if exists b;
drop table if exists a;

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

-- Expressions are allowed
create table ggg (a char(1), b numeric, d numeric)
distributed by (a)
partition by range (b)
(
partition aa start (2007) end (2007+1),
partition bb start (2008) end (2009)
);

drop table ggg cascade;

-- Even volatile expressions are OK. They are evaluted immediately.
create table ggg (a char(1), b numeric, d numeric)
distributed by (a)
partition by range (b)
(
partition aa start (2007) end (2008+(random()*9)::integer),
partition bb start (2018) end (2019)
);

drop table ggg cascade;

-- too many columns for RANGE partition
create table ggg (a char(1), b numeric, d numeric)
distributed by (a)
partition by range (b,d)
(
partition aa start (2007,1) end (2008,2),
partition bb start (2008,2) end (2009,3)
);

drop table ggg cascade;

-- Mismatch between number of columns in PARTITION BY and in the START/END/EVERY/VALUES clauses
create table pby_mismatch (a char(1), b numeric, d numeric)
distributed by (a)
partition by range (b)
(
  partition aa start (2007,1) end (2008),
  partition bb start (2008,2) end (2009)
);

create table pby_mismatch (a char(1), b numeric, d numeric)
distributed by (a)
partition by range (b)
(
  partition aa start (2007) end (2008,1),
  partition bb start (2008) end (2009,1)
);

create table pby_mismatch (a char(1), b numeric, d numeric)
    distributed by (a)
partition by range (b)
(
  partition aa start (2007) end (2008) every (1),
  partition bb start (2008) end (2009) every (1,2)
);

create table pby_mismatch (a char(1), b numeric, d numeric)
    distributed by (a)
partition by list (b)
(
  partition cc values ((1,2))
);

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



-- range list combo
create table ggg (a char(1), b date, d char(3), e numeric)
distributed by (a)
partition by range (b)
subpartition by list(d)
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
CREATE TYPE rank_partkey AS (rank int, gender char(1));
CREATE TABLE rank (id int, rankgender rank_partkey, year date)
DISTRIBUTED BY (id, year)
partition by list (rankgender)
(
 values (CAST ('(1,M)' AS rank_partkey)),
 values (CAST ('(2,M)' AS rank_partkey)),
 values (CAST ('(3,M)' AS rank_partkey)),
 values (CAST ('(1,F)' AS rank_partkey)),
 partition ff values (CAST ('(4,M)' AS rank_partkey)),
 partition bb values (CAST ('(1,M)' AS rank_partkey))
);


-- RANGE validation

-- legal if end of aa not inclusive
create table ggg (a char(1), b date, d char(3))
distributed by (a)
partition by range (b)
(
partition aa start (date '2007-08-01') end (date '2008-01-01'),
partition bb start (date '2008-01-01') end (date '2008-03-01')
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
partition aa start (date '2007-08-01') end (date '2008-01-01') inclusive,
partition bb start (date '2008-01-01') exclusive end (date '2008-03-01')
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

-- EVERY works by invoking the + operator. We haven't explictly documented
-- that user can create his own, but better still test it.
create domain funnytext as text;
create function funnytext_plus (funnytext, integer) returns funnytext
as $$ select (chr(ascii($1) + $2))::funnytext $$ language sql;

create operator pg_catalog.+ (function=funnytext_plus, leftarg=funnytext, rightarg=integer);

create table ggg (a char(1), t funnytext)
distributed by (a)
partition by range (t)
(
  start ('aaa') end ('foobar') every (1)
);
\d+ ggg

drop table ggg cascade;

-- What if the + operator returns NULL?
create or replace function funnytext_plus (funnytext, integer) returns funnytext
as $$ select NULL::funnytext $$ language sql;
create table ggg (a char(1), t funnytext)
distributed by (a)
partition by range (t)
(
  start ('aaa') end ('foobar') every (1)
);

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

create table ggg (a char(1), b date, d char(3))
distributed by (a)
partition by range (b)
(
partition bb start (date '2008-01-01') end (date '2009-01-01'),
partition aa start (date '2007-01-01') end (date '2006-01-01')
);

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
end (date '2003-01-01') every (interval '1 year')) (
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
alter table hhh add partition aa start ('2010-01-01') end ('2011-01-01');

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

select relname, pg_get_expr(relpartbound, oid) from pg_class where relname like 'no_start%' or relname like 'no_end%';

drop table no_end1;
drop table no_start1;

-- default partitions cannot have boundary specifications
create table jjj (aa int, bb date)
partition by range(bb)
(partition j1 end (date '2008-01-01'),
partition j2 end (date '2009-01-01'));

-- must have a name
alter table jjj add default partition;
alter table jjj add default partition for ('2008-01-01');
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

alter table jjj add partition j5 end (date '2010-01-01');

drop table jjj cascade;

alter table hhh alter partition cc set tablespace foo_p;

alter table hhh alter partition aa set tablespace foo_p;

alter table hhh drop partition cc;

alter table hhh drop partition cc cascade;

alter table hhh drop partition cc restrict;

alter table hhh drop partition if exists cc;

-- fail (mpp-3265)
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

-- must have name or value for partition
alter table hhh_l1 drop partition;
alter table hhh_l1 drop partition aa;
alter table hhh_l1 drop partition for ('2008-01-01');

-- same with range partitioning
alter table hhh_r1 drop partition;
alter table hhh_r1 drop partition for ('2007-04-01');
alter table hhh_r1 drop partition for ('2007-01-01');
alter table hhh_r1 drop partition aa_2;
alter table hhh_r1 drop partition aa_3;
alter table hhh_r1 drop partition aa_5;
alter table hhh_r1 drop partition aa_6;

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

alter table hhh exchange partition cc with table nosuchtable;
alter table hhh exchange partition aa with table nosuchtable;

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
\d+ d
select * from d;
select * from d_1_prt_1;
select * from d_1_prt_2;
select * from d_1_prt_3;
drop table d cascade;

-- check for NULL support
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
alter table d split default partition start (21,1) end (30) into (partition c, default partition);
alter table d split default partition start (21) end (30,1) into (partition c, default partition);
drop table  d cascade;

-- multicolumn list support
create type d_partkey as (b int, c int);
create table d (a int, k d_partkey) distributed by (a)
partition by list(k)
(partition a values(CAST('(1,2)' as d_partkey), CAST('(3,4)' as d_partkey)),
 partition b values(CAST('(100,20)' as d_partkey)),
 partition c values(CAST('(1000,1001)' as d_partkey), CAST('(1001,1002)' as d_partkey), CAST('(1003,1004)' as d_partkey)));
\d+ d
insert into d values(1, (1, 2));
insert into d values(1, (3, 4));
insert into d values(1, (100, 20));
insert into d values(1, (100, 2000));
insert into d values(1, ('1000', '1001')), (1, ('1001', '1002')), (1, ('1003', '1004'));
insert into d values(1, (100, NULL));
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

-- MPP-3988: allow same column in multiple partitioning keys at
-- different levels -- so this is legal again...
drop table if exists a;

-- TEST: make sure GPOPT (aka pivotal query optimizer) fall back to Postgres query optimizer
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
-- expect GPOPT fall back to Postgres query optimizer
-- since GPOPT don't support partition elimination through full outer joins
select * from s1 full outer join s2 on s1.d1 = s2.d2 and s1.p1 = s2.p2 where s1.p1 = 1;

-- CLEANUP
-- start_ignore
drop table if exists s1;
drop table if exists s2;
-- end_ignore

-- the following case is to test when we have a template
-- we can correct add new subpartition with relation options.
create table test_part_relops_tmpl (id int,  p1 text, p2 text, count int)
distributed by (id)
partition by list (p1)
subpartition by list (p2)
(
  partition m1 values ('m1')
  (subpartition l1 values ('l1'),
   subpartition l2 values ('l2')),
  partition m2 values ('m2')
  (subpartition l1 values ('l1'),
   subpartition l2 values ('l2'))
);

alter table test_part_relops_tmpl
set subpartition template
(
   subpartition l1 values('l1')
);

-- previously, we do wrong in the function of `add_partition_rule`
-- which invokes `transformRelOptions`, and transformRelOptions
-- may return NULL in some cases. For example, the invokation of
-- transformRelOptions in add_partition_rule set ignoreOids = true,
-- so the following statement creates such senario by passing oids options,
-- then transformRelOptions return NULL and we should correctly handle
-- null pointers.
alter table test_part_relops_tmpl alter partition for ('m1') add partition l3 values ('l3')
with (oids=false);

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
        variable_20             varchar(1024) null
)
distributed by (message_create_date)
partition by range (message_create_date)
(
    START (timestamp '2011-09-01') END (timestamp '2011-09-10') EVERY (interval '1 day'),
    DEFAULT PARTITION outlying_dates
);
-- partial index
create index dcl_messaging_test_index13 on dcl_messaging_test(variable_13) where message_create_date > '2011-09-02';
-- expression index
create index dcl_messaging_test_index16 on dcl_messaging_test(upper(variable_16));
alter table dcl_messaging_test drop default partition;

-- ADD case
alter table dcl_messaging_test add partition start (timestamp '2011-09-10') inclusive end (timestamp '2011-09-11') exclusive;

-- EXCHANGE case
create table dcl_candidate(like dcl_messaging_test including indexes) with (appendonly=true);
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
select relname, pg_get_expr(relpartbound, oid) from pg_class where relname like 'mpp13806%';

-- Drop the partition
alter TABLE mpp13806 drop partition test;

-- Add unbound partition with a gap succeeds
alter table mpp13806 add partition test end (date '2007-12-31') exclusive;
select relname, pg_get_expr(relpartbound, oid) from pg_class where relname like 'mpp13806%';

-- Fill the gap succeeds/adding immediately before the first partition succeeds
alter table mpp13806 add partition test1 start (date '2007-12-31') inclusive end (date '2008-01-01') exclusive;
select relname, pg_get_expr(relpartbound, oid) from pg_class where relname like 'mpp13806%';


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

-- SPLIT partition
alter table mpp14613_list alter partition others split partition subothers at (10) into (partition b1, partition subothers);
alter table mpp14613_range alter partition others split partition subothers at (10) into (partition b1, partition subothers);

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

alter table foo alter partition others split partition subothers at (10) into (partition b1, partition subothers);
alter table foo alter partition others split partition subothers at (10) into (partition b1, default partition);
alter table foo alter partition others split default partition at (10) into (partition b1, default partition);
drop table foo;

--
-- Drop index on a partitioned table. The indexes on the partitions are removed.
--
create table pt_indx_tab (c1 integer, c2 int, c3 text) partition by range (c1) (partition A start (integer '0') end (integer '5') every (integer '1'));

create unique index pt_indx_drop on pt_indx_tab(c1);

select count(*) from pg_index where indrelid='pt_indx_tab'::regclass;
select count(*) from pg_index where indrelid='pt_indx_tab_1_prt_a_1'::regclass;

drop index pt_indx_drop;

select count(*) from pg_index where indrelid='pt_indx_tab'::regclass;
select count(*) from pg_index where indrelid='pt_indx_tab_1_prt_a_1'::regclass;

--
-- Test changing the datatype of a column in a partitioning key column.
-- (Not supported, throws an error).
--
create table mpp18179 (a int, b int, i int)
distributed by (a)
partition by list (b)
   ( PARTITION ab1 VALUES (1),
     PARTITION ab2 values (2),
     default partition other
   );

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

-- Drop it
DROP INDEX mpp7635_ix3;
select * from pg_indexes where tablename like 'mpp7635%';

-- Create it again.
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


--
-- Test handling of dropped columns in SPLIT PARTITION. (PR #9386)
--
DROP TABLE IF EXISTS users_test;

CREATE TABLE users_test
(
  id          INT,
  dd          TEXT,
  user_name   VARCHAR(40),
  user_email  VARCHAR(60),
  born_time   TIMESTAMP,
  create_time TIMESTAMP
)
DISTRIBUTED BY (id)
PARTITION BY RANGE (create_time)
(
  PARTITION p2019 START ('2019-01-01'::TIMESTAMP) END ('2020-01-01'::TIMESTAMP),
  DEFAULT PARTITION extra
);

-- Drop useless column dd for some reason
ALTER TABLE users_test DROP COLUMN dd;

-- Assume we forgot/failed to split out new partitions beforehand
INSERT INTO users_test VALUES(1, 'A', 'A@abc.com', '1970-01-01', '2019-01-01 12:00:00');
INSERT INTO users_test VALUES(2, 'B', 'B@abc.com', '1980-01-01', '2020-01-01 12:00:00');
INSERT INTO users_test VALUES(3, 'C', 'C@abc.com', '1990-01-01', '2021-01-01 12:00:00');

-- New partition arrives late
ALTER TABLE users_test SPLIT DEFAULT PARTITION START ('2020-01-01'::TIMESTAMP) END ('2021-01-01'::TIMESTAMP)
 INTO (PARTITION p2020, DEFAULT PARTITION);

-- Expect A
SELECT user_name FROM users_test_1_prt_p2019;
-- Expect B
SELECT user_name FROM users_test_1_prt_p2020;
-- Expect C
SELECT user_name FROM users_test_1_prt_extra;

-- Github issue: https://github.com/greenplum-db/gpdb/issues/9460
-- When creating unique or primary key index on Partition table,
-- the cols in index must contain all partition keys.
CREATE TABLE t_idx_col_contain_partkey(a int, b int) DISTRIBUTED BY (a)
PARTITION BY list (b)
(PARTITION t1 values (1),
 PARTITION t2 values (2));

-- the following statement should fail because index cols does not contain part key
CREATE UNIQUE INDEX uidx_t_idx_col_contain_partkey on t_idx_col_contain_partkey(a);
-- the following statement should work
CREATE UNIQUE INDEX uidx_t_idx_col_contain_partkey on t_idx_col_contain_partkey(a, b);
DROP INDEX uidx_t_idx_col_contain_partkey;
DROP TABLE t_idx_col_contain_partkey;

-- test unique index for multi level partition table
CREATE TABLE t_idx_col_contain_partkey
(
        r_regionkey integer not null,
        r_name char(25),
        r_comment varchar(152)
)
DISTRIBUTED BY (r_regionkey)
PARTITION BY RANGE (r_regionkey)
SUBPARTITION BY LIST (r_name) SUBPARTITION TEMPLATE
(
        SUBPARTITION africa VALUES ('AFRICA'),
        SUBPARTITION america VALUES ('AMERICA'),
        SUBPARTITION asia VALUES ('ASIA'),
        SUBPARTITION europe VALUES ('EUROPE'),
        SUBPARTITION mideast VALUES ('MIDDLE EAST'),
        SUBPARTITION australia VALUES ('AUSTRALIA'),
        SUBPARTITION antarctica VALUES ('ANTARCTICA')
)
(
        PARTITION region1 start (0),
        PARTITION region2 start (3),
        PARTITION region3 start (5) end (8)
);

-- should fail, must contain all the partition keys of all levels
CREATE UNIQUE INDEX uidx_t_idx_col_contain_partkey on t_idx_col_contain_partkey(r_regionkey);
-- should work
CREATE UNIQUE INDEX uidx_t_idx_col_contain_partkey on t_idx_col_contain_partkey(r_regionkey, r_name);
DROP INDEX uidx_t_idx_col_contain_partkey;
DROP TABLE t_idx_col_contain_partkey;

--
-- Test EXCHANGE PARTITION, when the new table has different CHECK constraints
--
CREATE TABLE constraint_mismatch_tbl (
    id int,
    date date,
    amt decimal(10,2)
    CONSTRAINT amt_check CHECK (amt > 0)
) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
   (PARTITION Jan08 START (date '2008-01-01'),
    PARTITION Feb08 START (date '2008-02-01'),
    PARTITION Mar08 START (date '2008-03-01') END (date '2008-04-01'));

-- fail: new table doesn't have 'amt_check' constraint
CREATE TABLE mismatch_exchange_tbl (
    id int,
    date date,
    amt decimal(10,2)
) DISTRIBUTED BY (id);
INSERT INTO mismatch_exchange_tbl SELECT i, '2008-03-02', i FROM generate_series(11,15)i;

ALTER TABLE constraint_mismatch_tbl EXCHANGE PARTITION mar08 WITH TABLE mismatch_exchange_tbl;

-- fail: new table has a constraint called 'amt_check', but it's different from the parent's
DROP TABLE mismatch_exchange_tbl;
CREATE TABLE mismatch_exchange_tbl (
    id int,
    date date,
    amt decimal(10,2)
    CONSTRAINT amt_check CHECK (amt <> 0)
) DISTRIBUTED BY (id);
INSERT INTO mismatch_exchange_tbl SELECT i, '2008-03-02', i FROM generate_series(11,15)i;

ALTER TABLE constraint_mismatch_tbl EXCHANGE PARTITION mar08 WITH TABLE mismatch_exchange_tbl;

-- success: new table has compatible 'amt_check' constraint
DROP TABLE mismatch_exchange_tbl;
CREATE TABLE mismatch_exchange_tbl (
    id int,
    date date,
    amt decimal(10,2)
    CONSTRAINT amt_check CHECK (amt > 0)
) DISTRIBUTED BY (id);
INSERT INTO mismatch_exchange_tbl SELECT i, '2008-03-02', i FROM generate_series(11,15)i;

ALTER TABLE constraint_mismatch_tbl EXCHANGE PARTITION mar08 WITH TABLE mismatch_exchange_tbl;


--
-- END INCLUSIVE should work for CREATE, ADD PARTITION, and SPLIT PARTITION for
-- the following data types. The INCLUSIVE END value will be converted to an
-- EXCLUSIVE upper bound during transformation. If the INCLUSIVE END value is
-- smaller than the maximum value of the data type, the exclusive upper bound
-- will be the END INCLUSIVE value + '1', where '1' is the resolution of the
-- data type. Otherwise, MAXVALUE will be stored as the upper bound.
--
-- END INCLUSIVE should work for bigint
CREATE TABLE end_inclusive_bigint (a int, b bigint)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION pmax_create START (9223372036854775805) END (9223372036854775807) INCLUSIVE EVERY (1),
        PARTITION p1 START (1) END (3) INCLUSIVE,
        PARTITION p20 START (20),
        DEFAULT PARTITION other
        );
ALTER TABLE end_inclusive_bigint SPLIT DEFAULT PARTITION START (7) END (10) INCLUSIVE INTO (PARTITION p7, DEFAULT PARTITION);
\d+ end_inclusive_bigint

ALTER TABLE end_inclusive_bigint DROP PARTITION pmax_create_1;
ALTER TABLE end_inclusive_bigint DROP PARTITION pmax_create_2;
ALTER TABLE end_inclusive_bigint ADD PARTITION pmax_add START (9223372036854775805) END (9223372036854775807) INCLUSIVE;
\d+ end_inclusive_bigint

ALTER TABLE end_inclusive_bigint DROP PARTITION pmax_add;
ALTER TABLE end_inclusive_bigint SPLIT DEFAULT PARTITION START (9223372036854775805) END (9223372036854775807) INCLUSIVE INTO (PARTITION pmax_split, DEFAULT PARTITION);
\d+ end_inclusive_bigint

-- END INCLUSIVE should work for int
CREATE TABLE end_inclusive_int (a int, b int)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 END (3) INCLUSIVE,
        PARTITION pmax END (2147483647) INCLUSIVE
        );
\d+ end_inclusive_int

-- END INCLUSIVE should work for smallint
CREATE TABLE end_inclusive_smallint (a int, b smallint)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START (1) END (3) INCLUSIVE,
        PARTITION pmax START (4) END (32767) INCLUSIVE
        );
\d+ end_inclusive_smallint

-- END INCLUSIVE should work for date
CREATE TABLE end_inclusive_date (a int, b date)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START ('2020-06-16') END ('2020-06-17') INCLUSIVE,
        PARTITION pmax START ('2020-06-18') END ('infinity') INCLUSIVE
        );
\d+ end_inclusive_date

-- END INCLUSIVE should work for time without time zone
CREATE TABLE end_inclusive_time (a int, b time)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START ('00:00:00.000001') END ('01:00:00') INCLUSIVE,
        PARTITION pmax START ('23:00:00') END ('24:00:00') INCLUSIVE
        );
\d+ end_inclusive_time

-- END INCLUSIVE should work for time with time zone
CREATE TABLE end_inclusive_timetz (a int, b time with time zone)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START ('00:00:00 EST') END ('01:00:00 PST') INCLUSIVE,
        PARTITION pmax START ('23:00:00 EST') END ('24:00:00 PST') INCLUSIVE
        );
\d+ end_inclusive_timetz

-- END INCLUSIVE should work for timestamp without time zone
CREATE TABLE end_inclusive_timestamp (a int, b timestamp)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START ('2020-06-16 00:00:00') END ('2020-06-16 01:00:00') INCLUSIVE,
        PARTITION pmax START ('2020-06-16 23:00:00') END ('infinity') INCLUSIVE
        );
\d+ end_inclusive_timestamp

-- END INCLUSIVE should work for timestamp with time zone
CREATE TABLE end_inclusive_timestamptz (a int, b timestamp with time zone)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START ('2020-06-16 00:00:00 PST') END ('2020-06-16 01:00:00 PST') INCLUSIVE,
        PARTITION pmax START ('2020-06-16 23:00:00 EST') END ('infinity') INCLUSIVE
        );
\d+ end_inclusive_timestamptz

-- END INCLUSIVE should work for interval
CREATE TABLE end_inclusive_interval (a int, b interval)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START ('1 year') END ('2 years') INCLUSIVE
        );
\d+ end_inclusive_interval

-- END INCLUSIVE with MAXVALUE should work with implicit START/END
DROP TABLE end_inclusive_int;
CREATE TABLE end_inclusive_int (a int, b int)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START (1),
        PARTITION pmax END (2147483647) INCLUSIVE,
        PARTITION p2 START (2) END (5) INCLUSIVE
        );
\d+ end_inclusive_int

DROP TABLE end_inclusive_int;
CREATE TABLE end_inclusive_int (a int, b int)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION pmax END (2147483647) INCLUSIVE,
        PARTITION p1 START (1),
        PARTITION p2 START (2) END (5) INCLUSIVE
        );
\d+ end_inclusive_int

-- END INCLUSIVE should fail when precision is specified
CREATE TABLE end_inclusive_time_with_precision (a int, b time(5))
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START ('00:00:00') END ('01:00:00') INCLUSIVE
        );

-- END INCLUSIVE should fail for unsupported data types
CREATE TABLE end_inclusive_numeric (a int, b numeric)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START (1) END (3) INCLUSIVE
        );

-- Also check START EXCLUSIVE
CREATE TABLE start_exclusive_smallint (a int, b smallint)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START (0) EXCLUSIVE END (3) INCLUSIVE,
        PARTITION pmax START (4) EXCLUSIVE
        );
\d+ start_exclusive_smallint

-- If the START EXCLUSIVE value + 1 would overflow, you get an error
CREATE TABLE start_exclusive_smallint_overflow (a int, b smallint)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (
        PARTITION p1 START (0) EXCLUSIVE END (3) INCLUSIVE,
        PARTITION pmax START (32767) EXCLUSIVE
        );

-- Test for ALTER TABLE WITH/WITHOUT VALIDATION.
-- It doesn't do anything anymore, but check that the syntax is accepted.
CREATE TABLE validation_syntax_tbl (a int)
    DISTRIBUTED BY (a)
    PARTITION BY RANGE (a)
        (
        PARTITION p1 START (1) END (3)
        );
CREATE TABLE exchange_tbl (a int4);
INSERT INTO exchange_tbl VALUES (100);
ALTER TABLE validation_syntax_tbl EXCHANGE PARTITION p1 WITH TABLE exchange_tbl WITH VALIDATION;
ALTER TABLE validation_syntax_tbl EXCHANGE PARTITION p1 WITH TABLE exchange_tbl WITHOUT VALIDATION;
DROP TABLE exchange_tbl;
DROP TABLE validation_syntax_tbl;


--
-- Test a case where the automatically created partition name clashes with
-- another table or partition.
-- Before GPDB 7, the automatic table name generation used check if the name is
-- in use, and pick another name to avoid the clash. It's not as smart anymore.
-- It's more tricky now, because e.g. the ALTER TABLE ALTER/DROP/ADD PARTITION
-- commands rely on the deterministic naming of the partitions. If a user runs
-- into this, the work around is to use different table/partition names, or
-- use the upstream syntax and name each partition explicitly.
--
CREATE TABLE partitioned_table_with_very_long_name_123456789x
(
    col1 int4,
    col2 int4
)
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)
  (partition partone start(1) end(100000001),
   partition parttwo start(100000001) end(200000001),
   partition partthree start(200000001) end(300000001));

CREATE TABLE partitioned_table_with_very_long_name_123456789y
(
    col1 int4,
    col2 int4
)
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)
  (partition partone start(1) end(100000001),
   partition parttwo start(100000001) end(200000001),
   partition partthree start(200000001) end(300000001));

-- Test not supported partition strategy with legacy GPDB syntax
create table hashpart_gpspec (a int, b int) partition by hash (b) (partition p1 start (1) end (2));

-- Test (START (VAL1) END (VAL2) EVERY (INTERVAL_VAL)) syntax against customed types.
CREATE SCHEMA part_op_test;
SET search_path = 'part_op_test';
CREATE TYPE myint;
CREATE FUNCTION myint_in(cstring) RETURNS myint AS 'int4in' LANGUAGE INTERNAL IMMUTABLE STRICT;
CREATE FUNCTION myint_out(myint) RETURNS cstring AS 'int4out' LANGUAGE INTERNAL IMMUTABLE STRICT;
CREATE TYPE myint (INPUT=myint_in, OUTPUT=myint_out, passedbyvalue, internallength=4);
CREATE FUNCTION myint_lt(myint, myint) RETURNS boolean AS 'int4lt' LANGUAGE INTERNAL IMMUTABLE STRICT;
CREATE FUNCTION myint_le(myint, myint) RETURNS boolean AS 'int4le' LANGUAGE INTERNAL IMMUTABLE STRICT;
CREATE FUNCTION myint_gt(myint, myint) RETURNS boolean AS 'int4gt' LANGUAGE INTERNAL IMMUTABLE STRICT;
CREATE FUNCTION myint_ge(myint, myint) RETURNS boolean AS 'int4ge' LANGUAGE INTERNAL IMMUTABLE STRICT;
CREATE FUNCTION myint_eq(myint, myint) RETURNS boolean AS 'int4eq' LANGUAGE INTERNAL IMMUTABLE STRICT;
CREATE FUNCTION myint_ne(myint, myint) RETURNS boolean AS 'int4ne' LANGUAGE INTERNAL IMMUTABLE STRICT;
CREATE FUNCTION myint_pl_myint(myint, myint) RETURNS myint   AS 'int4pl' LANGUAGE INTERNAL IMMUTABLE STRICT;
CREATE OPERATOR < (LEFTARG=myint, RIGHTARG=myint, PROCEDURE=myint_lt, COMMUTATOR= >, NEGATOR= >=, RESTRICT=scalarltsel, JOIN=scalarltjoinsel);
CREATE OPERATOR > (LEFTARG=myint, RIGHTARG=myint, PROCEDURE=myint_gt, COMMUTATOR= <, negator= <=, RESTRICT=scalargtsel, JOIN=scalargtjoinsel);
CREATE OPERATOR <= (LEFTARG=myint, RIGHTARG=myint, PROCEDURE=myint_le, COMMUTATOR= >=, NEGATOR= >, RESTRICT=scalarltsel, JOIN=scalarltjoinsel);
CREATE OPERATOR >= (LEFTARG=myint, RIGHTARG=myint, PROCEDURE=myint_ge, COMMUTATOR= <=, NEGATOR= <, RESTRICT=scalargtsel, JOIN=scalargtjoinsel);
CREATE OPERATOR = (LEFTARG=myint, RIGHTARG=myint, PROCEDURE=myint_eq, COMMUTATOR= =, NEGATOR= <>, RESTRICT=eqsel, JOIN=eqjoinsel, hashes, merges);
CREATE OPERATOR <> (LEFTARG=myint, RIGHTARG=myint, PROCEDURE=myint_ne, COMMUTATOR= <>, NEGATOR= =, RESTRICT=neqsel, JOIN=neqjoinsel, merges);
CREATE FUNCTION bt_myint_cmp (myint, myint) RETURNS int AS 'btint4cmp' LANGUAGE INTERNAL IMMUTABLE STRICT;
CREATE OPERATOR CLASS bt_myint_ops DEFAULT FOR TYPE myint USING btree family integer_ops AS
  operator 1 <,
  operator 2 <=,
  operator 3 =,
  operator 4 >=,
  operator 5 >,
  FUNCTION 1 bt_myint_cmp (myint, myint);
CREATE CAST (int AS myint) WITHOUT FUNCTION AS IMPLICIT;
-- We don't have operator +(myint, myint), failure expected.
CREATE TABLE issue_14956_part_table_with_customed_type
(
  col1 int4,
  col2 myint
)
DISTRIBUTED BY (col1)
PARTITION BY RANGE (col2)
  (START (1) END (10) EVERY (1::myint));

CREATE OPERATOR + (LEFTARG = myint, RIGHTARG = myint, PROCEDURE = myint_pl_myint, COMMUTATOR = + );
-- Now, we can create the partitioned table with Greenplum syntax.
CREATE TABLE issue_14956_part_table_with_customed_type
(
  col1 int4,
  col2 myint
)
DISTRIBUTED BY (col1)
PARTITION BY RANGE (col2)
  (START (1) END (10) EVERY (1::myint));
INSERT INTO issue_14956_part_table_with_customed_type VALUES (1, 2), (2, 3);
-- Clean up.
DROP SCHEMA part_op_test CASCADE;
