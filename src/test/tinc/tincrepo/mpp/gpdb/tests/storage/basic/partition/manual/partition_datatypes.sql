set enable_partition_fast_insert = on;
set enable_partition_rules = off;
set gp_enable_hash_partitioned_tables = true;

-- Test Partition with different objects

-- Time (WORKS)
CREATE TABLE TIME_TBL (f1 time(2));
insert into time_tbl values (time '00:00');
insert into time_tbl values (time '01:00');
insert into time_tbl values (time '02:00');
insert into time_tbl values (time '03:00');
insert into time_tbl values (time '04:00');
insert into time_tbl values (time '05:00');
insert into time_tbl values (time '06:00');
insert into time_tbl values (time '07:00');
insert into time_tbl values (time '08:00');
insert into time_tbl values (time '09:00');
insert into time_tbl values (time '10:00');
insert into time_tbl values (time '11:00');
insert into time_tbl values (time '12:00');
insert into time_tbl values (time '13:00');
insert into time_tbl values (time '14:00');
insert into time_tbl values (time '15:00');
insert into time_tbl values (time '16:00');
insert into time_tbl values (time '17:00');
insert into time_tbl values (time '18:00');
insert into time_tbl values (time '19:00');
insert into time_tbl values (time '20:00');
insert into time_tbl values (time '21:00');
insert into time_tbl values (time '22:00');
insert into time_tbl values (time '23:00');
insert into time_tbl values (time '24:00');


CREATE TABLE TIME_TBL_HOUR1_rangep (f1 time(2))
partition by range (f1)
(
partition a01 start (time '00:00') end (time '01:00'),
partition a02 start (time '01:00') end (time '02:00'),
partition a03 start (time '02:00') end (time '03:00'),
partition a04 start (time '03:00') end (time '04:00'),
partition a05 start (time '04:00') end (time '05:00'),
partition a06 start (time '05:00') end (time '06:00'),
partition a07 start (time '06:00') end (time '07:00'),
partition a08 start (time '07:00') end (time '08:00'),
partition a09 start (time '08:00') end (time '09:00'),
partition a10 start (time '09:00') end (time '10:00'),
partition a11 start (time '10:00') end (time '11:00'),
partition a12 start (time '11:00') end (time '12:00'),
partition a13 start (time '12:00') end (time '13:00'),
partition a14 start (time '13:00') end (time '14:00'),
partition a15 start (time '14:00') end (time '15:00'),
partition a16 start (time '15:00') end (time '16:00'),
partition a17 start (time '16:00') end (time '17:00'),
partition a18 start (time '17:00') end (time '18:00'),
partition a19 start (time '18:00') end (time '19:00'),
partition a20 start (time '19:00') end (time '20:00'),
partition a21 start (time '20:00') end (time '21:00'),
partition a22 start (time '21:00') end (time '22:00'),
partition a23 start (time '22:00') end (time '23:00'),
partition a24 start (time '23:00') end (time '24:00')
);
insert into time_tbl_hour1_rangep select * from time_tbl;

-- This fail
CREATE TABLE TIME_TBL_HOUR2_rangep (f1 time(2))
partition by range (f1)
(
  start (time '00:00') end (time '24:00') EVERY (INTERVAL '1 hour')
);
insert into time_tbl_hour2_rangep select * from time_tbl;

CREATE TABLE TIME_TBL_HOUR3_rangep (f1 time(2))
partition by range (f1)
(
  start (time '09:00') end (time '17:00') EVERY (INTERVAL '1 hour'),
  default partition default_part
);
insert into time_tbl_hour3_rangep select * from time_tbl;

CREATE TABLE TIME_TBL_HOUR4_rangep (f1 time(2))
partition by range (f1)
(
  start (time '00:00') end (time '08:00') EVERY (INTERVAL '1 hour'),
  default partition default_part
);
insert into time_tbl_hour4_rangep select * from time_tbl;

CREATE TABLE TIME_TBL_HOUR5_rangep (f1 time(2)) -- Error, overlapping
partition by range (f1)
(
  start (time '00:00') end (time '08:00') inclusive EVERY (INTERVAL '1 hour'),
  default partition default_part
);

CREATE TABLE TIME_TBL_HOUR5_rangep (f1 time(2)) -- Syntax Error
partition by range (f1)
(
  start (time '00:00') end (time '08:00') EVERY (INTERVAL '1 hour') inclusive,
  default partition default_part
);

CREATE TABLE TIME_TBL_HOUR6_rangep (f1 time(2))
partition by range (f1)
(
  start (time '00:00') end (time '08:00') exclusive EVERY (INTERVAL '1 hour'),
  default partition default_part
);

CREATE TABLE TIME_TBL_HOUR_listp (f1 time(2))
partition by list (f1)
(
  partition aa values (time '00:00', time '09:00'),
  partition bb values (time '10:00', time '17:00'),
  default partition default_part
);
insert into time_tbl_hour_listp select * from time_tbl;

CREATE TABLE TIME_TBL_HOUR_hashp (f1 time(2))
partition by hash (f1)
partitions 4
(
  partition aa, partition bb, partition cc, partition dd
);

insert into time_tbl_hour_hashp select * from time_tbl;


-- TIMEZ

CREATE TABLE TIMETZ_TBL (f1 time(2) with time zone);

INSERT INTO TIMETZ_TBL VALUES ('00:01 PDT');
INSERT INTO TIMETZ_TBL VALUES ('01:00 PDT');
INSERT INTO TIMETZ_TBL VALUES ('02:03 PDT');
INSERT INTO TIMETZ_TBL VALUES ('07:07 PST');
INSERT INTO TIMETZ_TBL VALUES ('08:08 EDT');
INSERT INTO TIMETZ_TBL VALUES ('11:59 PDT');
INSERT INTO TIMETZ_TBL VALUES ('12:00 PDT');
INSERT INTO TIMETZ_TBL VALUES ('12:01 PDT');
INSERT INTO TIMETZ_TBL VALUES ('23:59 PDT');
INSERT INTO TIMETZ_TBL VALUES ('11:59:59.99 PM PDT');

INSERT INTO TIMETZ_TBL VALUES ('2003-03-07 15:36:39 America/New_York');
INSERT INTO TIMETZ_TBL VALUES ('2003-07-07 15:36:39 America/New_York');

CREATE TABLE TIMETZ_TBL1_listp (f1 time(2) with time zone)
partition by list (f1)
(
  partition aa values (time with time zone '00:00 PDT', time with time zone '01:00 PDT'),
  partition bb values (time with time zone '00:00 EST', time with time zone '01:00 EST')
);

-- This does not work, based on regress/timetz.sql
CREATE TABLE TIMETZ_TBL2_listp (f1 time(2) with time zone)
partition by list (f1)
(
  partition aa values (time with time zone '00:00 America/New_York', time with time zone '01:00 America/New_York'),
  partition bb values (time with time zone '00:00 America/Los_Angeles', time with time zone '01:00 America/Los_Angeles')
);

-- The DATE is ignored
CREATE TABLE TIMETZ_TBL3_listp (f1 time(2) with time zone)
partition by list (f1)
(
  partition aa values (time with time zone '2001-01-01 00:00 America/New_York', time with time zone '2001-01-01 01:00 America/New_York'),
  partition bb values (time with time zone '2001-01-01 00:00 America/Los_Angeles', time with time zone '2001-01-01 01:00 America/Los_Angeles')
);

-- This does not work, based on regress/timetz.sql
CREATE TABLE TIMETZ_TBL4_listp (f1 time(2) with time zone)
partition by list (f1)
(
  partition aa values (time with time zone 'America/New_York'),
  partition bb values (time with time zone 'America/Los_Angeles')
);


CREATE TABLE TIMETZ_TBL_sub (f1 time(2) with time zone)
partition by list (f1)
subpartition by range (f1)
subpartition template (
start (time '00:00'),
start (time '01:00')
)
( partition pst values ('PST','America/Los_Angeles'),
  partition est values ('EST','America/New_York')
);

CREATE TABLE TIMETZ_TBL_sub (f1 time(2) with time zone, f2 char(4))
partition by list (f2)
subpartition by range (f1)
subpartition template (
start (time '00:00'),
start (time '01:00')
)
( partition pst values ('PST'),
  partition est values ('EST'),
);

CREATE TABLE TIME_TBL_sub (f1 time(2), f2 char(4))
partition by list (f2)
subpartition by range (f1)
subpartition template (
start (time '00:00'),
start (time '01:00')
)
( partition pst values ('PST'),
  partition est values ('EST')
);

CREATE TABLE TIME_TBL_sub (f1 time(2), f2 varchar(20))
partition by list (f2)
subpartition by range (f1)
subpartition template (
start (time '00:00'),
start (time '01:00')
)
( partition pst values ('PST','America/Los_Angeles'),
  partition est values ('EST','America/New_York')
);

CREATE TABLE TIMETZ_TBL1_rangep (f1 time(2) with time zone)
partition by range (f1)
(
  partition pst start (time with time zone '00:00 PST') end (time with time zone '23:00 PST') EVERY (INTERVAL '1 hour'),
  partition est start (time with time zone '00:00 EST') end (time with time zone '23:00 EST') EVERY (INTERVAL '1 hour')
);

CREATE TABLE TIMETZ_TBL1_hash (f1 time(2) with time zone)
partition by hash (f1)
partitions 5;

CREATE TABLE TIMETZ_TBL2_hash (f1 time(2) with time zone)
partition by hash (f1)
partitions 5
(
  partition aa, partition bb, partition cc, partition dd, partition ee
);

CREATE TABLE TIMETZ_TBL3_hash (f1 time(2) with time zone)
partition by hash (f1)
partitions 2
(
  partition "AA BB", partition "CC DD"
);

\d timetz_tbl3_hash*

alter table timetz_tbl3_hash rename partition "AA BB" to "AA";
alter table timetz_tbl3_hash rename partition "CC DD" to "BB";

\d timetz_tbl3_hash*

CREATE TABLE TIMETZ_TBL3_list (f1 time(2) with time zone)
partition by list (f1)
(
  partition "Los Angeles" values ('00:00 PST'),
  partition "New York" values ('00:00 EST')
);

\d timetz_tbl3_list*

insert into timetz_tbl3_list values ('00:00 PST');
insert into timetz_tbl3_list values ('00:00 EST');

select * from "timetz_tbl3_list_1_prt_Los Angeles" ;
select * from "timetz_tbl3_list_1_prt_New York" ;

alter table timetz_tbl3_list rename partition "Los Angeles" to "LA";
alter table timetz_tbl3_list rename partition "New York" to "NY";

\d timetz_tbl3_list*

insert into timetz_tbl3_list values ('00:00 PST');
insert into timetz_tbl3_list values ('00:00 EST');

select * from "timetz_tbl3_list_1_prt_LA" ;
select * from "timetz_tbl3_list_1_prt_NY" ;

CREATE TABLE TIMETZ_TBL4_range (f1 time(2) with time zone)
partition by range (f1)
(
  partition "Los Angeles" start (time with time zone '00:00 PST') end (time with time zone '23:00 PST') EVERY (INTERVAL '1 hour'),
  partition "New York" start (time with time zone '00:00 EST') end (time with time zone '23:00 EST') EVERY (INTERVAL '1 hour')
);


-- Date (WORKS)
CREATE TABLE DATE_TABLE (f1 date);
INSERT INTO DATE_TABLE VALUES ('2007-01-01');
INSERT INTO DATE_TABLE VALUES ('2007-02-02');
INSERT INTO DATE_TABLE VALUES ('2007-02-03');
INSERT INTO DATE_TABLE VALUES ('2007-02-04');
INSERT INTO DATE_TABLE VALUES ('2007-02-08');
INSERT INTO DATE_TABLE VALUES ('2007-05-02');
INSERT INTO DATE_TABLE VALUES ('2007-05-03');
INSERT INTO DATE_TABLE VALUES ('2007-05-04');
INSERT INTO DATE_TABLE VALUES ('2007-05-05');
INSERT INTO DATE_TABLE VALUES ('2007-05-06');
INSERT INTO DATE_TABLE VALUES ('2007-07-02');
INSERT INTO DATE_TABLE VALUES ('2007-07-03');
INSERT INTO DATE_TABLE VALUES ('2007-07-04');
INSERT INTO DATE_TABLE VALUES ('2007-07-05');
INSERT INTO DATE_TABLE VALUES ('2007-07-06');
INSERT INTO DATE_TABLE VALUES ('2007-12-02');
INSERT INTO DATE_TABLE VALUES ('2007-12-03');
INSERT INTO DATE_TABLE VALUES ('2007-12-04');
INSERT INTO DATE_TABLE VALUES ('2007-12-05');
INSERT INTO DATE_TABLE VALUES ('2007-12-06');
INSERT INTO DATE_TABLE VALUES ('2008-01-01');
INSERT INTO DATE_TABLE VALUES ('2008-02-02');
INSERT INTO DATE_TABLE VALUES ('2008-05-02');
INSERT INTO DATE_TABLE VALUES ('2008-07-02');
INSERT INTO DATE_TABLE VALUES ('2008-12-02');
INSERT INTO DATE_TABLE VALUES ('2009-01-01');
INSERT INTO DATE_TABLE VALUES ('2009-02-02');
INSERT INTO DATE_TABLE VALUES ('2009-05-02');
INSERT INTO DATE_TABLE VALUES ('2009-07-02');
INSERT INTO DATE_TABLE VALUES ('2009-12-02');
INSERT INTO DATE_TABLE VALUES ('2010-01-01');
INSERT INTO DATE_TABLE VALUES ('2010-02-02');
INSERT INTO DATE_TABLE VALUES ('2006-02-02');

CREATE TABLE DATE_TBL_YEAR1_rangep (f1 date)
partition by range (f1)
(
partition aa start (date '2007-01-01') end (date '2008-01-01'),
partition bb start (date '2008-01-01') end (date '2009-01-01'),
partition cc start (date '2009-01-01') end (date '2010-01-01'),
default partition default_part
);
insert into date_tbl_year1_rangep select * from date_table;
copy date_tbl_year1_rangep from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/date.data';

CREATE TABLE DATE_TBL_YEAR2_rangep (f1 date)
partition by range (f1)
(
start (date '2007-01-01')
end (date '2010-01-01') every (interval '1 year'),
default partition default_part
);
insert into date_tbl_year2_rangep select * from date_table;

CREATE TABLE DATE_TBL_MONTH1_rangep (f1 date)
partition by range (f1)
(
partition aa start (date '2007-01-01') end (date '2007-02-01'),
partition bb start (date '2007-02-01') end (date '2007-03-01'),
partition cc start (date '2007-03-01') end (date '2007-04-01'),
partition dd start (date '2007-04-01') end (date '2007-05-01'),
partition ee start (date '2007-05-01') end (date '2007-06-01'),
partition ff start (date '2007-06-01') end (date '2007-07-01'),
partition gg start (date '2007-07-01') end (date '2007-08-01'),
partition hh start (date '2007-08-01') end (date '2007-09-01'),
partition ii start (date '2007-09-01') end (date '2007-10-01'),
partition jj start (date '2007-10-01') end (date '2007-11-01'),
partition kk start (date '2007-11-01') end (date '2007-12-01'),
partition ll start (date '2007-12-01') end (date '2008-01-01'),
default partition default_part
);
insert into date_tbl_month1_rangep select * from date_tbl;

CREATE TABLE DATE_TBL_MONTH2_rangep (f1 date)
partition by range (f1)
(
start (date '2007-01-01')
end (date '2008-01-01') every (interval '1 month')
);
insert into date_tbl_month2_rangep select * from date_tbl;

CREATE TABLE DATE_TBL_DAY_rangep (f1 date)
partition by range (f1)
(
start (date '2007-01-01')
end (date '2008-01-01') every (interval '1 day')
);
insert into date_tbl_day_rangep select * from date_tbl;

CREATE TABLE DATE_TBL_hash (f1 date)
partition by hash(f1)
partitions 4
( partition aa, partition bb, partition cc, partition dd );
insert into date_tbl_hash select * from date_tbl;
copy date_tbl_hash from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/date.data';

CREATE TABLE DATE_TBL_list (f1 date)
partition by list(f1)
( partition aa values (date '2007-01-01'),
  partition bb values (date '2008-01-01'),
  partition cc values (date '2009-01-01'),
  partition dd values (date '2010-01-01'),
  default partition default_part
);
insert into date_tbl_list select * from date_tbl;

-- Timestamp (WORKS)

CREATE TABLE TIMESTAMP_TBL ( d1 timestamp(2) without time zone);
-- Special values
INSERT INTO TIMESTAMP_TBL VALUES ('-infinity');
INSERT INTO TIMESTAMP_TBL VALUES ('infinity');
INSERT INTO TIMESTAMP_TBL VALUES ('epoch');
-- Obsolete special values
INSERT INTO TIMESTAMP_TBL VALUES ('invalid');
INSERT INTO TIMESTAMP_TBL VALUES ('current');

-- Postgres v6.0 standard output format
INSERT INTO TIMESTAMP_TBL VALUES ('Mon Feb 10 17:32:01 1997 PST');
INSERT INTO TIMESTAMP_TBL VALUES ('Invalid Abstime');
INSERT INTO TIMESTAMP_TBL VALUES ('Undefined Abstime');

-- Variations on Postgres v6.1 standard output format
INSERT INTO TIMESTAMP_TBL VALUES ('Mon Feb 10 17:32:01.000001 1997 PST');
INSERT INTO TIMESTAMP_TBL VALUES ('Mon Feb 10 17:32:01.999999 1997 PST');
INSERT INTO TIMESTAMP_TBL VALUES ('Mon Feb 10 17:32:01.4 1997 PST');
INSERT INTO TIMESTAMP_TBL VALUES ('Mon Feb 10 17:32:01.5 1997 PST');
INSERT INTO TIMESTAMP_TBL VALUES ('Mon Feb 10 17:32:01.6 1997 PST');

-- ISO 8601 format
INSERT INTO TIMESTAMP_TBL VALUES ('1997-01-02');
INSERT INTO TIMESTAMP_TBL VALUES ('1997-01-02 03:04:05');
INSERT INTO TIMESTAMP_TBL VALUES ('1997-02-10 17:32:01-08');
INSERT INTO TIMESTAMP_TBL VALUES ('1997-02-10 17:32:01-0800');
INSERT INTO TIMESTAMP_TBL VALUES ('1997-02-10 17:32:01 -08:00');
INSERT INTO TIMESTAMP_TBL VALUES ('19970210 173201 -0800');
INSERT INTO TIMESTAMP_TBL VALUES ('1997-06-10 17:32:01 -07:00');
INSERT INTO TIMESTAMP_TBL VALUES ('2001-09-22T18:19:20');

-- POSIX format (note that the timezone abbrev is just decoration here)
INSERT INTO TIMESTAMP_TBL VALUES ('2000-03-15 08:14:01 GMT+8');
INSERT INTO TIMESTAMP_TBL VALUES ('2000-03-15 13:14:02 GMT-1');
INSERT INTO TIMESTAMP_TBL VALUES ('2000-03-15 12:14:03 GMT-2');
INSERT INTO TIMESTAMP_TBL VALUES ('2000-03-15 03:14:04 PST+8');
INSERT INTO TIMESTAMP_TBL VALUES ('2000-03-15 02:14:05 MST+7:00');

-- Variations for acceptable input formats
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 10 17:32:01 1997 -0800');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 10 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 10 5:32PM 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('1997/02/10 17:32:01-0800');
INSERT INTO TIMESTAMP_TBL VALUES ('1997-02-10 17:32:01 PST');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb-10-1997 17:32:01 PST');
INSERT INTO TIMESTAMP_TBL VALUES ('02-10-1997 17:32:01 PST');
INSERT INTO TIMESTAMP_TBL VALUES ('19970210 173201 PST');
set datestyle to ymd;
INSERT INTO TIMESTAMP_TBL VALUES ('97FEB10 5:32:01PM UTC');
INSERT INTO TIMESTAMP_TBL VALUES ('97/02/10 17:32:01 UTC');
reset datestyle;
INSERT INTO TIMESTAMP_TBL VALUES ('1997.041 17:32:01 UTC');
INSERT INTO TIMESTAMP_TBL VALUES ('19970210 173201 America/New_York');
-- this fails (even though TZ is a no-op, we still look it up)
INSERT INTO TIMESTAMP_TBL VALUES ('19970710 173201 America/Does_not_exist');

-- Check date conversion and date arithmetic
INSERT INTO TIMESTAMP_TBL VALUES ('1997-06-10 18:32:01 PDT');

INSERT INTO TIMESTAMP_TBL VALUES ('Feb 10 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 11 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 12 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 13 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 14 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 15 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 16 17:32:01 1997');

INSERT INTO TIMESTAMP_TBL VALUES ('Feb 16 17:32:01 0097 BC');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 16 17:32:01 0097');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 16 17:32:01 0597');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 16 17:32:01 1097');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 16 17:32:01 1697');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 16 17:32:01 1797');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 16 17:32:01 1897');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 16 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 16 17:32:01 2097');

INSERT INTO TIMESTAMP_TBL VALUES ('Feb 28 17:32:01 1996');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 29 17:32:01 1996');
INSERT INTO TIMESTAMP_TBL VALUES ('Mar 01 17:32:01 1996');
INSERT INTO TIMESTAMP_TBL VALUES ('Dec 30 17:32:01 1996');
INSERT INTO TIMESTAMP_TBL VALUES ('Dec 31 17:32:01 1996');
INSERT INTO TIMESTAMP_TBL VALUES ('Jan 01 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 28 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Feb 29 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Mar 01 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Dec 30 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Dec 31 17:32:01 1997');
INSERT INTO TIMESTAMP_TBL VALUES ('Dec 31 17:32:01 1999');
INSERT INTO TIMESTAMP_TBL VALUES ('Jan 01 17:32:01 2000');
INSERT INTO TIMESTAMP_TBL VALUES ('Dec 31 17:32:01 2000');
INSERT INTO TIMESTAMP_TBL VALUES ('Jan 01 17:32:01 2001');

INSERT INTO TIMESTAMP_TBL VALUES (NULL);


CREATE TABLE TIMESTAMP_TBL_YEAR ( d1 timestamp(2) without time zone);
CREATE TABLE TIMESTAMP_TBL_YEAR1_rangep (f1 date)
partition by range (f1)
(
start (date '2007-01-01')
end (date '2008-01-01') every (interval '1 month'),
default partition default_part
);
-- MPP-3079
CREATE TABLE TIMESTAMP_TBL_YEAR2_rangep (f1 timestamp)
partition by range (f1)
(
start (timestamp '2007-01-01 00:00:00 PST+8')
end (timestamp '2010-01-01 00:00:00 PST+8') every (interval '1 year'),
default partition default_part
);

CREATE TABLE TIMESTAMP_TBL_MONTH2_rangep (f1 timestamp)
partition by range (f1)
(
start (timestamp '2007-01-01 00:00:00 PST+8')
end (timestamp '2010-01-01 00:00:00 PST+8') every (interval '1 month'),
default partition default_part
);

-- Issue (Out-of-Memory)
CREATE TABLE TIMESTAMP_TBL_DAY2_rangep (f1 timestamp)
partition by range (f1)
(
start (timestamp '2007-01-01 00:00:00 PST+8')
end (timestamp '2010-01-01 00:00:00 PST+8') every (interval '1 day'),
default partition default_part
);

CREATE TABLE TIMESTAMP_TBL_DAY2_rangep (f1 timestamp)
partition by range (f1)
(
start (timestamp '2007-01-01 00:00:00 PST+8')
end (timestamp '2010-01-01 00:00:00 PST+8') every (interval '1 hour'),
default partition default_part
);


CREATE TABLE TIMESTAMP_TBL_YEAR3_rangep (f1 timestamp)
partition by range (f1)
(
start (timestamp '2007-01-01')
end (timestamp '2010-01-01') every (interval '1 year'),
default partition default_part
);

CREATE TABLE TIMESTAMP_TBL_YEAR3_hash (f1 date)
partition by hash (f1)
partitions 4;

CREATE TABLE TIMESTAMP_TBL_YEAR1_list (f1 date)
partition by list (f1)
(
  partition aa values (date '2001-01-01'),
  partition bb values (date '2002-02-02'),
  default partition default_part
);

CREATE TABLE TIMESTAMP_TBL_YEAR2_list (f1 date)
partition by list (f1)
(
  partition aa values (timestamp '2001-01-01'),
  partition bb values (timestamp '2002-02-02'),
  default partition default_part
);

CREATE TABLE TIMESTAMP_TBL_YEAR3_list (f1 timestamp(2) without time zone)
partition by list (f1)
(
partition aa values (timestamp '2007-01-01 00:00:00 PST+8'),
partition bb values (timestamp '2008-01-01 00:00:00 PST+8'),
default partition default_part
);

CREATE TABLE TIMESTAMP_TBL_MONTH_range (f1 timestamp(2) without time zone)
partition by range (f1)
(
start (timestamp '2007-01-01')
end (timestamp '2010-01-01') every (interval '1 month'),
default partition default_part
);

CREATE TABLE TIMESTAMP_TBL_DAY ( d1 timestamp(2) without time zone);
CREATE TABLE TIMESTAMP_TBL_HOUR ( d1 timestamp(2) without time zone);

CREATE TABLE TIMESTAMPTZ_TBL ( d1 timestamp(2) with time zone);

CREATE TABLE TIMESTAMPTZ_TBL_YEAR ( d1 timestamp(2) with time zone);
partition by list (d1)
(
partition aa values (timestamp '2007-01-01 00:00:00 PST+8'),
partition bb values (timestamp '2008-01-01 00:00:00 PST+8'),
default partition default_part
);

CREATE TABLE TIMESTAMPTZ_TBL_MONTH ( d1 timestamp(2) with time zone);
CREATE TABLE TIMESTAMPTZ_TBL_DAY ( d1 timestamp(2) with time zone);
CREATE TABLE TIMESTAMPTZ_TBL_HOUR ( d1 timestamp(2) with time zone);

-- Bolean (WORKS)
CREATE TABLE BOOL_TBL(b bool)
partition by list (b)
(
  partition aa values ('t'),
  partition bb values ('f')
);

CREATE TABLE BOOL_TBL1(b bool)
partition by list (b)
(
  partition aa values ('1'),
  partition bb values ('0')
);

CREATE TABLE BOOL_TBL2(b bool)
partition by list (b)
(
  partition aa values (1),
  partition bb values (0)
);

CREATE TABLE BOOL_TBL3(b bool)
partition by list (b)
(
  partition aa values (NULL),
  partition bb values (0)
);

CREATE TABLE BOOL_TBL4(b bool)
partition by list (b)
(
  partition aa values (1),
  partition bb values (NULL)
);

CREATE TABLE BOOL_TBL5(b bool)
partition by list (b)
(
  partition aa values ('t'),
  partition bb values ('f'),
  default partition default_part
);

-- This will fail
CREATE TABLE BOOL_TBL_FAIL(b bool)
partition by list (b)
(
  partition aa values ('t','True','true'),
  partition bb values ('f','False','false')
);

CREATE TABLE BOOL_FAIL(b bool)
partition by list (b)
(
  partition aa values ('2'),
  partition bb values ('0')
);

CREATE TABLE BOOL_FAIL(b bool)
partition by list (b)
(
  partition aa values ('1'),
  partition bb values ('-1')
);

CREATE TABLE BOOL_FAIL(b bool)
partition by list (b)
(
  partition aa values (NULL),
  partition bb values (NULL)
);

CREATE TABLE BOOL_FAIL(b bool)
partition by list (b)
(
  partition aa values ('t'),
  partition bb values ('t')
);

CREATE TABLE BOOL_FAIL(b bool)
partition by list (b)
(
  partition aa values ('f'),
  partition bb values ('f')
);

-- Int2 (Assert, MPP-3080)
CREATE TABLE INT2_TBL(f1 int2)
partition by range (f1)
(start (1) end (10) every (1));

CREATE TABLE INT2_TBL_list(f1 int2)
partition by list (f1)
(
  partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  default partition default_part
);

CREATE TABLE INT2_TBL_hash(f1 int2)
partition by hash (f1)
partitions 4;

-- Int4 (WORKS)

CREATE TABLE INT4_TBL(f1 int4)
partition by range (f1)
(start (1) end (10) every (1));

CREATE TABLE INT4_TBL2(f1 int4)
partition by range (f1)
(start (1) end (10) every (1), default partition default_part);

CREATE TABLE INT4_TBL3(f1 int4) -- Test Inclusive and Exclusive
partition by range (f1)
( start(1) end (10) inclusive,
  start(11) exclusive end (20) inclusive );

CREATE TABLE INT4_TBL4(f1 int4)
partition by list (f1)
(
  partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  default partition default_part
);

CREATE TABLE INT4_TBL5(f1 int4)
partition by hash (f1)
partitions 4;

CREATE TABLE INT4_TBL6(f1 int4)
partition by hash (f1)
partitions 4
( partition aa, partition bb, partition cc, partition dd);


-- Int8 (Assert, MPP-3080)

CREATE TABLE INT8_TBL(q1 int8, q2 int8)
partition by range (q1)
(start (1) end (10) every (1));

CREATE TABLE INT8_TBL_list(q1 int8, q2 int8)
partition by list (q1)
(
  partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  default partition default_part
);

CREATE TABLE INT8_TBL_hash(q1 int8)
partition by hash (q1)
partitions 4;

-- Float4 (Assert, MPP-3080)
CREATE TABLE FLOAT4_TBL (f1  float4)
partition by range (f1)
(start (1) end (10) every (1));

-- Float8 (Assert, MPP-3080)
CREATE TABLE FLOAT8_TBL(i INT DEFAULT 1, f1 float8)
partition by range (f1)
(start (1) end (10) every (1));

-- Real,Double Range Partition (Assert, MPP-3080)
CREATE TABLE FLOATREAL_TBL_rangep(i INT DEFAULT 1, f1 float(24))
partition by range (f1)
(start (1) end (10) every (1));

CREATE TABLE FLOATDOUBLE_TBL_rangep(i INT DEFAULT 1, f1 float(53))
partition by range (f1)
(start (1) end (10) every (1));

-- Real,Double Hash Partition
CREATE TABLE FLOATREAL_TBL_hashp(i INT DEFAULT 1, f1 float(24))
partition by hash (f1)
partitions 4
( partition aa, partition bb, partition cc, partition dd );

CREATE TABLE FLOATDOUBLE_TBL_hashp(i INT DEFAULT 1, f1 float(53))
partition by hash (f1)
partitions 4
( partition aa, partition bb, partition cc, partition dd );

-- Numeric, Numeric Big Range Partition (Assert, MPP-3080)
CREATE TABLE num_data_rangep (id int4, val numeric(210,10))
partition by range (val)
(start (1) end (10) every (1));

CREATE TABLE num_data_big_rangep (id int4, val numeric(1000,800))
partition by range (val)
(start (1) end (10) every (1));

-- Numeric, Numeric Big Hash Partition
CREATE TABLE num_data_hashp (id int4, val numeric(210,10))
partition by hash (val)
partitions 4
( partition aa, partition bb, partition cc, partition dd );

CREATE TABLE num_data_big_hashp (id int4, val numeric(1000,800))
partition by hash (val)
partitions 4
( partition aa, partition bb, partition cc, partition dd );

-- Bit (Not Supported with hash, no default operator class)

CREATE TABLE BIT_TABLE(a int,b BIT(11))
partition by hash (b) partitions 4
(partition a, partition b, partition c, partition d);

CREATE TABLE VARBIT_TABLE(v BIT VARYING(11))
partition by hash (v) partitions 4
(partition a, partition b, partition c, partition d);

-- Char (WORKS)
CREATE TABLE CHAR_TBL(f1 char);
INSERT INTO CHAR_TBL VALUES ('a');
INSERT INTO CHAR_TBL VALUES ('b');
INSERT INTO CHAR_TBL VALUES ('c');
INSERT INTO CHAR_TBL VALUES ('d');
INSERT INTO CHAR_TBL VALUES ('e');
INSERT INTO CHAR_TBL VALUES ('f');
INSERT INTO CHAR_TBL VALUES ('g');
INSERT INTO CHAR_TBL VALUES ('h');
INSERT INTO CHAR_TBL VALUES ('z');

CREATE TABLE CHAR_TBL1(f1 char)
partition by LIST (f1)
(
partition aa values ('a', 'b', 'c', 'd'),
partition bb values ('e', 'f', 'g')
);
INSERT INTO CHAR_TBL1 VALUES ('a');
INSERT INTO CHAR_TBL1 VALUES ('b');
INSERT INTO CHAR_TBL1 VALUES ('c');
INSERT INTO CHAR_TBL1 VALUES ('d');
INSERT INTO CHAR_TBL1 VALUES ('e');
INSERT INTO CHAR_TBL1 VALUES ('f');
INSERT INTO CHAR_TBL1 VALUES ('g');
INSERT INTO CHAR_TBL1 VALUES ('h'); -- failed because no partition
INSERT INTO CHAR_TBL1 VALUES (NULL); -- failed because no partition

CREATE TABLE CHAR_TBL2(f1 char)
partition by LIST (f1)
(
partition aa values ('a', 'b', 'c', 'd'),
partition bb values ('e', 'f', 'g'),
default partition default_part
);
INSERT INTO CHAR_TBL2 SELECT * FROM CHAR_TBL;
INSERT INTO CHAR_TBL2 VALUES ('h'); --  Should go to default partition
INSERT INTO CHAR_TBL2 VALUES (NULL); -- Should go to default partition

CREATE TABLE CHAR_TBL3(f1 char(4))
partition by LIST (f1)
(
partition aa values ('a', 'bb', 'cccc', 'dddd'),
partition bb values ('e', 'ffff', 'g')
);
INSERT INTO CHAR_TBL3 VALUES ('a');
INSERT INTO CHAR_TBL3 VALUES ('bb');
INSERT INTO CHAR_TBL3 VALUES ('cccc');
INSERT INTO CHAR_TBL3 VALUES ('dddd');
INSERT INTO CHAR_TBL3 VALUES ('e');
INSERT INTO CHAR_TBL3 VALUES ('ffff');
INSERT INTO CHAR_TBL3 VALUES ('g');
INSERT INTO CHAR_TBL3 VALUES ('g1'); -- failed because no partition
INSERT INTO CHAR_TBL3 VALUES (NULL); -- failed because no partition

CREATE TABLE CHAR_TBL4(f1 char(4))
partition by LIST (f1)
(
partition aa values ('a', 'bb', 'cccc', 'dddd'),
partition bb values ('e', 'ffff', 'g'),
default partition default_part
);
INSERT INTO CHAR_TBL4 SELECT * FROM CHAR_TBL3;
INSERT INTO CHAR_TBL4 VALUES ('g1'); -- Should go to default partition
INSERT INTO CHAR_TBL4 VALUES (NULL); -- Should go to default parttion
INSERT INTO CHAR_TBL4 VALUES ('zzz'); -- Should go to default partition
INSERT INTO CHAR_TBL4 VALUES ('1111'); -- Should go to default partition

CREATE TABLE CHAR_TBL5(f1 char(4))
partition by HASH (f1)
partitions 4
( partition aa, partition bb, partition cc, partition dd );
INSERT INTO CHAR_TBL5 SELECT * FROM CHAR_TBL4;

CREATE TABLE CHAR_TBL6(f1 char(4))
partition by RANGE (f1)
(
partition aa start ('a') end ('z'),
default partition default_part
);

-- Data type check, more than 4 character
CREATE TABLE CHAR_TBL7(f1 char(4))
partition by RANGE (f1)
(
partition aa start ('aaaaa') end ('z'),
default partition default_part
);

-- Varchar (WORKS)
CREATE TABLE VARCHAR_TBL(a int, b varchar(1),c varchar(4))
partition by hash(b)
partitions 4
(partition a, partition b, partition c, partition d);

insert into varchar_tbl values (1,'a','aaaa');
insert into varchar_tbl values (2,'b','aaaa');
insert into varchar_tbl values (3,'c','aaaa');
insert into varchar_tbl values (4,'d','aaaa');
insert into varchar_tbl values (5,'e','aaaa');
insert into varchar_tbl values (6,'f','aaaa');
insert into varchar_tbl values (7,'g','aaaa');
insert into varchar_tbl values (8,'h','aaaa');
insert into varchar_tbl values (9,'i','aaaa');
insert into varchar_tbl values (10,'j','aaaa');
insert into varchar_tbl values (11,'k','aaaa');
insert into varchar_tbl values (12,'l','aaaa');
insert into varchar_tbl values (13,'m','aaaa');
insert into varchar_tbl values (14,'n','aaaa');
insert into varchar_tbl values (15,'o','aaaa');
insert into varchar_tbl values (16,'p','aaaa');
insert into varchar_tbl values (17,'q','aaaa');
insert into varchar_tbl values (18,'r','aaaa');
insert into varchar_tbl values (19,'s','aaaa');
insert into varchar_tbl values (20,'t','aaaa');
insert into varchar_tbl values (21,'u','aaaa');
insert into varchar_tbl values (22,'v','aaaa');
insert into varchar_tbl values (23,'w','aaaa');
insert into varchar_tbl values (24,'x','aaaa');
insert into varchar_tbl values (25,'y','aaaa');
insert into varchar_tbl values (26,'z','aaaa');
insert into varchar_tbl values (1,'A','aaaa');
insert into varchar_tbl values (2,'B','aaaa');
insert into varchar_tbl values (3,'C','aaaa');
insert into varchar_tbl values (4,'D','aaaa');
insert into varchar_tbl values (5,'E','aaaa');
insert into varchar_tbl values (6,'F','aaaa');
insert into varchar_tbl values (7,'G','aaaa');
insert into varchar_tbl values (8,'H','aaaa');
insert into varchar_tbl values (9,'I','aaaa');
insert into varchar_tbl values (10,'J','aaaa');
insert into varchar_tbl values (11,'K','aaaa');
insert into varchar_tbl values (12,'L','aaaa');
insert into varchar_tbl values (13,'M','aaaa');
insert into varchar_tbl values (14,'N','aaaa');
insert into varchar_tbl values (15,'O','aaaa');
insert into varchar_tbl values (16,'P','aaaa');
insert into varchar_tbl values (17,'Q','aaaa');
insert into varchar_tbl values (18,'R','aaaa');
insert into varchar_tbl values (19,'S','aaaa');
insert into varchar_tbl values (20,'T','aaaa');
insert into varchar_tbl values (21,'U','aaaa');
insert into varchar_tbl values (22,'V','aaaa');
insert into varchar_tbl values (23,'W','aaaa');
insert into varchar_tbl values (24,'X','aaaa');
insert into varchar_tbl values (25,'Y','aaaa');
insert into varchar_tbl values (26,'Z','aaaa');

CREATE TABLE VARCHAR_TBL_listp(a int, b varchar(1),c varchar(4))
partition by LIST(b)
( partition aa VALUES ('a','A'), 
  partition bb VALUES ('b','B'),
  default partition default_part
);
INSERT INTO VARCHAR_TBL_listp SELECT * FROM VARCHAR_TBL;

-- MPP-3121 (Resolved)
CREATE TABLE VARCHAR_TBL_rangep(a int, b varchar(1),c varchar(4))
partition by RANGE(b)
( partition az1 start ('a') end ('z'),
  partition az2 start ('A') end ('Z'),
  default partition default_part
);
INSERT INTO VARCHAR_TBL3 SELECT * FROM VARCHAR_TBL;

-- Text (WORKS)
CREATE TABLE TEXT_TBL (f1 text)
partition by hash(f1)
partitions 4
(partition a, partition b, partition c, partition d);

insert into text_tbl values ('This is a test');

CREATE TABLE TEXT_TBL2(f1 text)
partition by RANGE (f1)
(
partition aa start ('a') end ('z'),
default partition default_part
);

CREATE TABLE TEXT_TBL3(f1 text)
partition by LIST(f1)
( partition aa VALUES ('a','A'), 
  partition bb VALUES ('b','B'),
  default partition default_part
);

-- Line (Not Supported with hash, no default operator class)
CREATE TABLE LSEG_TBL (a int,s lseg)
partition by hash(s)
partitions 4
(partition a, partition b, partition c, partition d);


-- Box (Not Supported with hash, no default operator class)
CREATE TABLE BOX_TBL (a int,f1 box)
partition by hash(f1)
partitions 4
(partition a, partition b, partition c, partition d);

-- Circle (Not Supported with hash, no default operator class)
CREATE TABLE CIRCLE_TBL (a int, f1 circle)
partition by hash(f1)
partitions 4
(partition a, partition b, partition c, partition d);

-- Path (Not Supported with hash, no default operator class)
CREATE TABLE PATH_TBL (a int,s serial, f1 path)
partition by hash(f1)
partitions 4
(partition a, partition b, partition c, partition d);

-- Point (Not Supported with hash, no default operator class)
CREATE TABLE POINT_TBL(a int, f1 point)
partition by hash(f1)
partitions 4
(partition a, partition b, partition c, partition d);

-- Polygon (Not Supported with hash, no default operator class)
CREATE TABLE POLYGON_TBL(a int, s serial, f1 polygon)
partition by hash(f1)
partitions 4
(partition a, partition b, partition c, partition d);

-- Inet Hash
CREATE TABLE INET_TBL (a int,c cidr, i inet);

INSERT INTO INET_TBL VALUES (1,'192.168.1', '192.168.1.226/24');
INSERT INTO INET_TBL VALUES (2,'192.168.1.0/26', '192.168.1.226');
INSERT INTO INET_TBL VALUES (3,'192.168.1', '192.168.1.0/24');
INSERT INTO INET_TBL VALUES (4,'192.168.1', '192.168.1.0/25');
INSERT INTO INET_TBL VALUES (5,'192.168.1', '192.168.1.255/24');
INSERT INTO INET_TBL VALUES (6,'192.168.1', '192.168.1.255/25');
INSERT INTO INET_TBL VALUES (7,'10', '10.1.2.3/8');
INSERT INTO INET_TBL VALUES (8,'10.0.0.0', '10.1.2.3/8');
INSERT INTO INET_TBL VALUES (9,'10.1.2.3', '10.1.2.3/32');
INSERT INTO INET_TBL VALUES (10,'10.1.2', '10.1.2.3/24');
INSERT INTO INET_TBL VALUES (11,'10.1', '10.1.2.3/16');
INSERT INTO INET_TBL VALUES (12,'10', '10.1.2.3/8');
INSERT INTO INET_TBL VALUES (13,'10', '11.1.2.3/8');
INSERT INTO INET_TBL VALUES (14,'10', '9.1.2.3/8');
INSERT INTO INET_TBL VALUES (15,'10:23::f1', '10:23::f1/64');
INSERT INTO INET_TBL VALUES (16,'10:23::8000/113', '10:23::ffff');
INSERT INTO INET_TBL VALUES (17,'::ffff:1.2.3.4', '::4.3.2.1/24');


CREATE TABLE INET_TBL1 (a int,c cidr, i inet)
partition by hash(i)
partitions 4
(partition a, partition b, partition c, partition d);

insert into inet_tbl1 select * from inet_tbl;

CREATE TABLE INET_TBL2 (a int,c cidr, i inet)
partition by hash(c)
partitions 4
(partition a, partition b, partition c, partition d);

insert into inet_tbl2 select * from inet_tbl;

-- Inet with LIST (WORKS)
CREATE TABLE INET_TBL3 (a int,c cidr, i inet)
partition by list(i)
(
  partition c1921 values ('192.168.1.0/24'),
  partition c10123 values ('10.1.2.3/8'),
  default partition default_partition

);
insert into inet_tbl3 select * from inet_tbl;

CREATE TABLE INET_TBL4 (a int,c cidr, i inet)
partition by list(c)
(
  partition c192 values ('192.168.1'),
  partition c10 values ('10'),
  default partition default_partition

);
insert into inet_tbl4 select * from inet_tbl;

CREATE TABLE INET_TBL5 (a int,c cidr, i inet)
partition by range(c)
(
  partition c100 start (cidr '192'),
  default partition default_partition

);

insert into inet_tbl5 select * from inet_tbl;

CREATE TABLE INET_TBL6 (a int,c cidr, i inet)
partition by range(i)
(
  partition c100 start (inet '192.168.1.0/24'),
  default partition default_partition

);

insert into inet_tbl6 select * from inet_tbl;

CREATE TABLE INET_TBL7 (a int,c cidr, i inet)
partition by range(i)
(
  partition c100 start (inet '192.168.1.0/32') end (inet '192.168.2.0/32'),
  default partition default_partition

);

insert into inet_tbl7 select * from inet_tbl;

CREATE TABLE INET_TBL8 (a int,c cidr, i inet)
partition by range(i)
(
  partition c100 start (inet '192.168.1.0/32'),
  partition c200 end (inet '192.168.2.0/32'),
  default partition default_partition

);

insert into inet_tbl8 select * from inet_tbl;

-- Invalid INET range
CREATE TABLE INET_TBL9 (a int,c cidr, i inet)
partition by range(i)
(
  partition c100 start (inet '192.168.1.0/32'), end (inet '192.168.2.0/32'),
  default partition default_partition

);

CREATE TABLE INET_TBL9 (a int,c cidr, i inet)
partition by LIST(c)
(
partition aa values ('10.1'),
partition bb values ('10.2'),
partition cc values ('192.168'),
partition dd values ('10:23::f1')
);

CREATE TABLE INET_TBL10 (a int,c cidr, i inet)
partition by LIST(i)
(
partition aa values ('10.1.2.3/8'),
partition cc values ('192.168.1.226')
);

-- Interval (Not Supported with hash, no default operator class)
CREATE TABLE TINTERVAL_TBL (a int,f1  tinterval)
partition by hash(f1)
partitions 4
(partition a, partition b, partition c, partition d);

-- Interval LIST (WORKS)
CREATE TABLE TINTERVAL_TBL2 (a int,f1  tinterval)
partition by LIST(f1)
(
partition aa values ('["Jan 1, 2000 00:00:00" "Jan 1, 2005 00:00:00"]'),
partition bb values ('["Jan 1, 2005 00:00:00" "Jan 1, 2010 00:00:00"]')
);

-- Arrays (Not Supported with hash, no default operator class)
CREATE TABLE arrtest1 (
        a                       int2[],
        b                       int4[][][],
        c                       name[],
        d                       text[][],
        e                       float8[],
        f                       char(5)[],
        g                       varchar(5)[]
)
partition by hash(b)
partitions 4
(partition a, partition b, partition c, partition d);

-- Arrays int4 (WORKS)
CREATE TABLE arrtest2 (
        a                       int2[],
        b                       int4[][][],
        c                       name[],
        d                       text[][],
        e                       float8[],
        f                       char(5)[],
        g                       varchar(5)[]
)
partition by LIST(b)
(
partition aa values ('{1,2,3}'),
partition bb values ('{4,5,6}'),
partition cc values ('{7,8,9}')
);

insert into arrtest2 (b) values ('{1,2,3}');
insert into arrtest2 (b) values ('{4,5,6}');
insert into arrtest2 (b) values ('{7,8,9}');
insert into arrtest2 (b) values ('{1,1,1}');

-- Arrays char(5) (WORKS)
CREATE TABLE arrtest3 (
        a                       int2[],
        b                       int4[][][],
        c                       name[],
        d                       text[][],
        e                       float8[],
        f                       char(5)[],
        g                       varchar(5)[]
)
partition by LIST(f)
(
partition aa values ('{"aaaaa","bbbbb"}'),
partition bb values ('{"ccccc","ddddd"}'),
partition cc values ('{"eeeee","fffff"}')
);

insert into arrtest3 (f) values ('{"aaaaa","bbbbb"}');
insert into arrtest3 (f) values ('{"ccccc","ddddd"}');
insert into arrtest3 (f) values ('{"eeeee","fffff"}');
insert into arrtest3 (f) values ('{"aaaa1","bbbb1"}');

CREATE TABLE arrtest4 (
        a                       int2[],
        b                       int4[][][],
        c                       name[],
        d                       text[][],
        e                       float8[],
        f                       char(5)[],
        g                       varchar(5)[]
)
partition by RANGE(d)
(
  partition aa start ('{"aaaaa","bbbbb"}'),
  partition bb start ('{"bbbbb","ccccc"}'),
  default partition default_part
);


-- Test tables using domains (MPP-3080)
create domain domainvarchar varchar(5);
create domain domainnumeric numeric(8,2);
create domain domainint4 int4;
create domain domaintext text;

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

create table basictest2
           ( testint4 domainint4
           , testtext domaintext
           , testvarchar domainvarchar
           , testnumeric domainnumeric
           )
partition by RANGE(testint4)
(start (1) end (10) every (1));

-- Name (WORKS)
CREATE TABLE NAME_TBL(a int, f1 name)
partition by LIST(f1)
(
partition aa values ('aaaaa'),
partition bb values ('bbbbb'),
partition cc values ('ccccc'),
default partition default_part
);

insert into name_tbl values (1,'aaaaa');
insert into name_tbl values (1,'bbbbb');
insert into name_tbl values (1,'ccccc');
insert into name_tbl values (1,'ddddd');

CREATE TABLE NAME_TBL1(a int, f1 name)
partition by HASH(f1)
partitions 4
(
partition aa, partition bb, partition cc, partition dd
insert into name_tbl1 select * from name_tbl;
);

CREATE TABLE NAME_TBL2(a int, f1 name)
partition by RANGE(f1)
(
partition aa start ('aaaaa'),
partition bb start ('bbbbb'),
partition cc start ('ccccc'),
default partition default_part
);

