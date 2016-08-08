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

-- Cannot create hash partition with default partition
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
) partition by hash (unique1) ( partition aa, partition bb, partition cc, partition dd, default partition default_part );


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
) partition by hash (unique1) partitions 4;

-- Expected, Cannot create default partition for 2-level
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
) partition by hash (unique1)
subpartition by hash (unique2)
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd),
default partition default_partition
);

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
) partition by hash (unique1)
subpartition by hash (unique2)
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);


insert into onek_part select * from onek;
insert into onek2_part select * from onek;

-- Expected, Cannot add default partition for hash
alter table onek_part add default partition default_part;
alter table onek2_part add default partition default_part;

-- FIRT LEVEL
-- Cannot DROP partition using hash
alter table onek_part drop partition;
alter table onek_part drop partition for (position(1));
alter table onek_part drop partition for (position(100));
alter table onek_part drop partition for (position(1000));
alter table onek_part drop partition for (position(-1));
alter table onek_part drop partition for (position(-6));
alter table onek_part drop partition for (position());
alter table onek_part drop partition for (position(0));
alter table onek_part drop partition for (position(NULL));

alter table onek_part add partition aa;

-- SECOND LEVEL
-- Try to drop all partition, should only remain one
alter table onek2_part drop partition;
alter table onek2_part drop partition for (position(1));
alter table onek2_part drop partition for (position(100));
alter table onek2_part drop partition for (position(1000));
alter table onek2_part drop partition for (position(-1));
alter table onek2_part drop partition for (position(-6));
alter table onek2_part drop partition for (position());
alter table onek2_part drop partition for (position(0));
alter table onek2_part drop partition for (position(NULL));

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
) partition by hash (unique1)
subpartition by hash (unique2)
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);
insert into onek3_part select * from onek;

alter table onek3_part add column AAA varchar(20);
alter table onek3_part add column BBB int ;
alter table onek3_part drop column BBB;
alter table onek3_part alter column AAA type varchar(30);
alter table onek3_part rename column AAA to BBB;
alter table onek3_part rename column BBB to AAA;

-- Cannot drop for hash partition
alter table onek3_part drop partition;

alter table onek3_part add column CCC int;
alter table onek3_part alter column AAA type varchar(30);
alter table onek3_part rename column AAA to BBB;
alter table onek3_part rename column BBB to AAA;

drop table onek3_part;
