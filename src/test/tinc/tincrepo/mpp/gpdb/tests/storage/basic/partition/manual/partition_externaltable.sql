set enable_partition_fast_insert = on;
set enable_partition_rules = off;
set gp_enable_hash_partitioned_tables = true;

\timing

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

copy onek from '/home/jsoedomo/data/onek.data';

CREATE EXTERNAL TABLE ext_onek (
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
) location ('file://apollo3/home/jsoedomo/data/onek.data' ) FORMAT 'text';

CREATE EXTERNAL WEB TABLE gpfdist_start (x text)
execute E'(($GPHOME/bin/gpfdist -p 8080 -d /home/jsoedomo/data  </dev/null >/dev/null 2>&1 &); sleep 5; echo "starting...") '
on SEGMENT 0
FORMAT 'text';

CREATE EXTERNAL WEB TABLE gpfdist_stop (x text)
execute E'(pkill gpfdist || killall gpfdist) > /dev/null 2>&1; echo "stopping..."'
on SEGMENT 0
FORMAT 'text' (delimiter '|');

select * from gpfdist_stop;
select * from gpfdist_start;

CREATE EXTERNAL TABLE gpfdist_onek (
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
) location ('gpfdist://apollo3:8080/onek.data' )
FORMAT 'text';

select count(*) from onek;
select count(*) from ext_onek;
select count(*) from gpfdist_onek;


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

truncate table onek_partlist;
insert into onek_partlist select * from ext_onek;

truncate table onek_partlist;
insert into onek_partlist select * from gpfdist_onek;

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

truncate table onek_partrange;
truncate table onek2_partrange;
insert into onek_partrange select * from ext_onek;
insert into onek2_partrange select * from ext_onek;

truncate table onek_partrange;
truncate table onek2_partrange;
insert into onek_partrange select * from gpfdist_onek;
insert into onek2_partrange select * from gpfdist_onek;


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

truncate table onek_parthash;
truncate table onek2_parthash;
insert into onek_parthash select * from ext_onek;
insert into onek2_parthash select * from ext_onek;

truncate table onek_parthash;
truncate table onek2_parthash;
insert into onek_parthash select * from gpfdist_onek;
insert into onek2_parthash select * from gpfdist_onek;
