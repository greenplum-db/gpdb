set enable_partition_fast_insert = on;
set enable_partition_rules = off;
set gp_enable_hash_partitioned_tables = true;

-- Test partition with DROP
CREATE TABLE onek (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
);

copy onek from '/home/jsoedomo/onek.data';


CREATE TABLE onek_partlist (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );

CREATE TABLE onek2_partlist (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by list (unique1)
subpartition by list (unique2)
(
partition aa values (1,2,3,4,5,6,7,8,9,10) (subpartition cc values (1,2,3), subpartition dd values (4,5,6) ),
partition bb values (11,12,13,14,15,16,17,18,19,20) (subpartition cc values (1,2,3), subpartition dd values (4,5,6) )
);
insert into onek_partlist select * from onek;
insert into onek2_partlist select * from onek; -- No default partition, cannot insert because no partition key


CREATE TABLE onek_partrange (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
( partition aa start (0) end (1000) every (100), default partition default_part );

CREATE TABLE onek2_partrange (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
subpartition by range (unique2) subpartition template ( start (0) end (1000) every (100) )
( start (0) end (1000) every (100));
alter table onek2_part add default partition default_part;

insert into onek_partrange select * from onek;
insert into onek2_partrange select * from onek;

CREATE TABLE onek_parthash (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by hash (unique1)
( partition aa, partition bb, partition cc, partition dd );

CREATE TABLE onek2_parthash (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by hash (unique1)
subpartition by hash (unique2)
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);


insert into onek_parthash select * from onek;
insert into onek2_parthash select * from onek;


select count(*) from onek_partrange;
select count(*) from onek2_partrange;
select count(*) from onek_partlist;
select count(*) from onek2_partlist; -- NO DATA
select count(*) from onek_parthash;
select count(*) from onek2_parthash;

-- Truncate ALL
truncate table onek_partrange;
truncate table onek2_partrange;
truncate table onek_partlist;
truncate table onek2_partlist;
truncate table onek_parthash;
truncate table onek2_parthash;

insert into onek_parthash select * from onek;
insert into onek2_parthash select * from onek;
insert into onek_partrange select * from onek;
insert into onek2_partrange select * from onek;
insert into onek_partlist select * from onek;
insert into onek2_partlist select * from onek; -- No default partition, cannot insert because no partition key

-- Truncate child partition LIST
select unique1 from onek_partlist_1_prt_aa order by 1;
select unique1 from onek_partlist_1_prt_bb order by 1;
select count(*) from onek_partlist_1_prt_aa;
alter table onek_partlist truncate partition aa;
select count(*) from onek_partlist_1_prt_aa;
select count(*) from onek_partlist;
select count(*) from onek_partlist_1_prt_default_part;
alter table onek_partlist truncate partition default_part;
select count(*) from onek_partlist_1_prt_default_part;
select count(*) from onek_partlist;
select count(*) from onek_partlist_1_prt_bb;
truncate table onek_partlist_1_prt_bb;
select count(*) from onek_partlist_1_prt_bb;
select count(*) from onek_partlist;

-- Issue with LIST subpartition, NO DATA FOR NOW until Default Partition is fixed
select unique1 from onek2_partlist_1_prt_aa order by 1;
select unique1 from onek2_partlist_1_prt_bb order by 1;
select count(*) from onek2_partlist_1_prt_aa;
alter table onek2_partlist truncate partition aa;
select count(*) from onek2_partlist_1_prt_aa;
select count(*) from onek2_partlist;
select count(*) from onek2_partlist_1_prt_default_part;
alter table onek2_partlist truncate partition default_part;
select count(*) from onek2_partlist_1_prt_default_part;
select count(*) from onek2_partlist;
select count(*) from onek2_partlist_1_prt_bb;
truncate table onek2_partlist_1_prt_bb;
select count(*) from onek2_partlist_1_prt_bb;
select count(*) from onek2_partlist;

-- Truncate child partition HASH
select count(*) from onek_parthash_1_prt_aa;
alter table onek_parthash truncate partition aa;
select count(*) from onek_parthash_1_prt_aa;
select count(*) from onek_parthash;
select count(*) from onek_parthash_1_prt_bb;
truncate table onek_parthash_1_prt_bb;
select count(*) from onek_parthash_1_prt_bb;
select count(*) from onek_parthash;

select count(*) from onek2_parthash_1_prt_aa;
alter table onek2_parthash truncate partition aa;
select count(*) from onek2_parthash_1_prt_aa;
select count(*) from onek2_parthash;
select count(*) from onek2_parthash_1_prt_bb_2_prt_cc;
truncate table onek2_parthash_1_prt_bb_2_prt_cc;
select count(*) from onek2_parthash_1_prt_bb_2_prt_cc;
select count(*) from onek2_parthash;


-- Truncate child partition RANGE
select count(*) from onek_partrange_1_prt_aa1;
alter table onek_partrange truncate partition for (position(1));
select count(*) from onek_partrange_1_prt_aa1;
select count(*) from onek_partrange;
select count(*) from onek_partrange_1_prt_aa10;
alter table onek_partrange truncate partition for (position(-1));
select count(*) from onek_partrange_1_prt_aa10;
select count(*) from onek_partrange;
alter table onek_partrange truncate partition for (position(100));
alter table onek_partrange truncate partition for (position(NULL));
alter table onek_partrange truncate partition for (position(-100));
select count(*) from onek_partrange;
select count(*) from onek_partrange_1_prt_aa9;
truncate table onek_partrange_1_prt_aa9;
select count(*) from onek_partrange_1_prt_aa9;
select count(*) from onek_partrange;

select count(*) from onek2_partrange_1_prt_1;
alter table onek2_partrange truncate partition for (position(1));
select count(*) from onek2_partrange_1_prt_1;
select count(*) from onek2_partrange;
select count(*) from onek2_partrange_1_prt_10;
alter table onek2_partrange truncate partition for (position(-1));
select count(*) from onek2_partrange_1_prt_10;
select count(*) from onek2_partrange;
alter table onek2_partrange truncate partition for (position(100));
alter table onek2_partrange truncate partition for (position(NULL));
alter table onek2_partrange truncate partition for (position(-100));
select count(*) from onek2_partrange;
select count(*) from onek2_partrange_1_prt_9;
truncate table onek2_partrange_1_prt_9;
select count(*) from onek2_partrange_1_prt_9;
select count(*) from onek2_partrange;


-- DROP TABLE
drop table onek_partrange;
drop table onek2_partrange;
drop table onek_partlist;
drop table onek2_partlist;
drop table onek_parthash;
drop table onek2_parthash;
drop table onek;
