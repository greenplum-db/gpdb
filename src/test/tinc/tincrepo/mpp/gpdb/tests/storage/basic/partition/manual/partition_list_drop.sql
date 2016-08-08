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

copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';


CREATE TABLE onek_part (
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

CREATE TABLE onek2_part (
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
partition aa values (1,2,3,4,5,6,7,8,9,10) (subpartition cc values (1,2,3), subpartition dd values (4,5,6), default subpartition aa_def ),
partition bb values (11,12,13,14,15,16,17,18,19,20) (subpartition cc values (1,2,3), subpartition dd values (4,5,6), default subpartition bb_def),
default partition default_part (default subpartition foo)
);

insert into onek_part select * from onek;
insert into onek2_part select * from onek;

-- ALTER DROP

alter table onek_part drop partition default_part;
alter table onek2_part drop partition default_part;

alter table onek_part add default partition default_part;
alter table onek2_part add default partition default_part;

-- FIRT LEVEL
-- Try to drop all partition, should only remain one
-- Have to drop default partition first
-- Position 0 does not exist
alter table onek_part drop partition for (position(0));
alter table onek_part drop partition for (position(NULL));
alter table onek_part drop partition for (position());

alter table onek_part drop partition default_part;
alter table onek_part drop partition;
alter table onek_part drop partition;
alter table onek_part drop partition;
alter table onek_part drop partition;
alter table onek_part drop partition;
alter table onek_part drop partition;
alter table onek_part drop partition;
alter table onek_part drop partition;
alter table onek_part drop partition;
-- Last partition, cannot be dropped
alter table onek_part drop partition;

insert into onek_part (unique1) values (999);

alter table onek_part add partition aa start (0) end (100);

-- SECOND LEVEL
-- Try to drop all partition, should only remain one
-- Position 0 does not exist
alter table onek2_part drop partition for (position(0));
alter table onek2_part drop partition for (position(NULL));
alter table onek2_part drop partition for (position());

alter table onek2_part drop partition;
alter table onek2_part drop partition;
alter table onek2_part drop partition;
alter table onek2_part drop partition;
alter table onek2_part drop partition;
alter table onek2_part drop partition;
alter table onek2_part drop partition;
alter table onek2_part drop partition;
alter table onek2_part drop partition;
-- Last subpartition, cannot be dropped
alter table onek2_part drop partition;

-- ADD, DROP, ALTER column
CREATE TABLE onek3_part (
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
( partition aa start (0) end (1000) every (500), default partition default_part );
insert into onek3_part select * from onek;

alter table onek3_part add column AAA varchar(20);
alter table onek3_part add column BBB int ;
alter table onek3_part drop column BBB;
alter table onek3_part alter column AAA type varchar(30);
alter table onek3_part rename column AAA to BBB;
alter table onek3_part rename column BBB to AAA;

alter table onek3_part drop partition;

-- ERROR
alter table onek3_part add column CCC int;
alter table onek3_part alter column AAA type varchar(30);
-- This is OK
alter table onek3_part rename column AAA to BBB;
alter table onek3_part rename column BBB to AAA;

drop table onek3_part;

-- DROP PARTITION with Position
CREATE TABLE onek3_part (
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
( partition aa start (0) end (500),
  partition bb start (500) end (1000),
  partition cc start (1000) end (1500),
  partition dd start (1500) end (2000),
  default partition default_part );

insert into onek3_part select * from onek;
-- ERROR, Out of range
alter table onek3_part drop partition for (position(-5));
-- Valid
alter table onek3_part drop partition for (position(1));
-- Doesn't work if there is default partition, need SPLIT
alter table onek3_part add partition aa start (0) end (500);
alter table onek3_part drop partition default_part;
alter table onek3_part add partition aa start (0) end (500);

alter table onek3_part drop partition for (position(100));
alter table onek3_part drop partition for (position(1000));
alter table onek3_part drop partition ff;
alter table onek3_part drop partition for (position(-1));

alter table onek3_part drop partition;
alter table onek3_part drop partition;
drop table onek3_part;

-- DROP with COMBINED KEY
create table b (i int, j date) distributed by (i)
partition by range (i, j)
(start(1, '2008-01-01') end (10, '2009-01-01'),
 start(1, '2009-01-01') end(15, '2010-01-01'),
 start(15, '2010-01-01') end (30, '2011-01-01'),
 start(1, '2011-01-01') end (100, '2012-01-01')
);
insert into b values(1, '2008-06-11');
insert into b values(11, '2009-08-24');
insert into b values(25, '2010-01-22');
insert into b values(90, '2011-05-04');

alter table b drop partition;
alter table b drop partition for (position(-1));
-- Does not exist
alter table b drop partition ee;
drop table b;

create table b (i int, j date) distributed by (i)
partition by range (i, j)
( partition aa start(1, '2008-01-01') end (10, '2009-01-01'),
 partition bb start(1, '2009-01-01') end(15, '2010-01-01'),
 partition cc start(15, '2010-01-01') end (30, '2011-01-01'),
 partition dd start(1, '2011-01-01') end (100, '2012-01-01')
);
insert into b values(1, '2008-06-11');
insert into b values(11, '2009-08-24');
insert into b values(25, '2010-01-22');
insert into b values(90, '2011-05-04');

alter table b drop partition;
alter table b drop partition for (position(-1));
alter table b drop partition bb;
-- Does not exist
alter table b drop partition ee;
-- Last partition
alter table b drop partition dd;

