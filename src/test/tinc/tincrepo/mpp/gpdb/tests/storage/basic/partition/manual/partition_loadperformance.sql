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

copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';

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
) location ('file://localhost//home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data' ) FORMAT 'text';

CREATE EXTERNAL TABLE ext_mediumonek (
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
) location ('file://localhost//home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data' ) FORMAT 'text';

CREATE EXTERNAL TABLE ext_largeonek (
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
) location ('file://localhost//home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data' ) FORMAT 'text';

CREATE EXTERNAL WEB TABLE gpfdist_start (x text)
execute E'(($GPHOME/bin/gpfdist -p 8080 -d /home/jsoedomo/perforce/cdbfast/main/partition/_data  </dev/null >/dev/null 2>&1 &); sleep 5; echo "starting...") '
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
) location ('gpfdist://localhost:8080/onek.data' )
FORMAT 'text';

CREATE EXTERNAL TABLE gpfdist_mediumonek (
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
) location ('gpfdist://localhost:8080/5mb.data' )
FORMAT 'text';

CREATE EXTERNAL TABLE gpfdist_largeonek (
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
) location ('gpfdist://localhost:8080/54mb.data' )
FORMAT 'text';

select count(*) from onek;
select count(*) from ext_onek;
select count(*) from gpfdist_onek;
select count(*) from gpfdist_mediumonek;
select count(*) from gpfdist_largeonek;


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

-- insert into onek2_partlist select * from onek; -- No default partition, cannot insert because no partition key

truncate table onek;
truncate table onek_partlist;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';
copy onek_partlist from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';
truncate table onek;
truncate table onek_partlist;
insert into onek select * from ext_onek;
insert into onek_partlist select * from ext_onek;
truncate table onek;
truncate table onek_partlist;
insert into onek select * from gpfdist_onek;
insert into onek_partlist select * from gpfdist_onek;

truncate table onek;
truncate table onek_partlist;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data';
copy onek_partlist from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data';
truncate table onek;
truncate table onek_partlist;
insert into onek select * from ext_mediumonek;
insert into onek_partlist select * from ext_mediumonek;
truncate table onek;
truncate table onek_partlist;
insert into onek select * from gpfdist_largeonek;
insert into onek_partlist select * from gpfdist_mediumonek;

truncate table onek;
truncate table onek_partlist;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data';
copy onek_partlist from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data';
truncate table onek;
truncate table onek_partlist;
insert into onek select * from ext_largeonek;
insert into onek_partlist select * from ext_largeonek;
truncate table onek;
truncate table onek_partlist;
insert into onek select * from gpfdist_largeonek;
insert into onek_partlist select * from gpfdist_largeonek;

drop table onek_partlist;
drop table onek2_partlist;

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
-- Issue with inserting subpartition with default partition
-- alter table onek2_partrange add default partition default_part;

truncate table onek;
truncate table onek_partrange;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';
copy onek_partrange from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';
truncate table onek;
truncate table onek_partrange;
insert into onek select * from ext_onek;
insert into onek_partrange select * from ext_onek;
truncate table onek;
truncate table onek_partrange;
insert into onek select * from gpfdist_onek;
insert into onek_partrange select * from gpfdist_onek;

truncate table onek;
truncate table onek_partrange;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data';
copy onek_partrange from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data';
truncate table onek;
truncate table onek_partrange;
insert into onek select * from ext_mediumonek;
insert into onek_partrange select * from ext_mediumonek;
truncate table onek;
truncate table onek_partrange;
insert into onek select * from gpfdist_largeonek;
insert into onek_partrange select * from gpfdist_mediumonek;

truncate table onek;
truncate table onek_partrange;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data';
copy onek_partrange from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data';
truncate table onek;
truncate table onek_partrange;
insert into onek select * from ext_largeonek;
insert into onek_partrange select * from ext_largeonek;
truncate table onek;
truncate table onek_partrange;
insert into onek select * from gpfdist_largeonek;
insert into onek_partrange select * from gpfdist_largeonek;

truncate table onek;
truncate table onek2_partrange;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';
copy onek2_partrange from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';
truncate table onek;
truncate table onek2_partrange;
insert into onek select * from ext_onek;
insert into onek2_partrange select * from ext_onek;
truncate table onek;
truncate table onek2_partrange;
insert into onek select * from gpfdist_onek;
insert into onek2_partrange select * from gpfdist_onek;

truncate table onek;
truncate table onek2_partrange;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data';
copy onek2_partrange from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data';
truncate table onek;
truncate table onek2_partrange;
insert into onek select * from ext_mediumonek;
insert into onek2_partrange select * from ext_mediumonek;
truncate table onek;
truncate table onek2_partrange;
insert into onek select * from gpfdist_largeonek;
insert into onek2_partrange select * from gpfdist_mediumonek;

truncate table onek;
truncate table onek2_partrange;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data';
copy onek2_partrange from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data';
truncate table onek;
truncate table onek2_partrange;
insert into onek select * from ext_largeonek;
insert into onek2_partrange select * from ext_largeonek;
truncate table onek;
truncate table onek2_partrange;
insert into onek select * from gpfdist_largeonek;
insert into onek2_partrange select * from gpfdist_largeonek;

drop table one2_partrange;
drop table onek2_partrange;


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

truncate table onek;
truncate table onek_parthash;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';
copy onek_parthash from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';
truncate table onek;
truncate table onek_parthash;
insert into onek select * from ext_onek;
insert into onek_parthash select * from ext_onek;
truncate table onek;
truncate table onek_parthash;
insert into onek select * from gpfdist_onek;
insert into onek_parthash select * from gpfdist_onek;

truncate table onek;
truncate table onek_parthash;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data';
copy onek_parthash from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data';
truncate table onek;
truncate table onek_parthash;
insert into onek select * from ext_mediumonek;
insert into onek_parthash select * from ext_mediumonek;
truncate table onek;
truncate table onek_parthash;
insert into onek select * from gpfdist_largeonek;
insert into onek_parthash select * from gpfdist_mediumonek;

truncate table onek;
truncate table onek_parthash;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data';
copy onek_parthash from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data';
truncate table onek;
truncate table onek_parthash;
insert into onek select * from ext_largeonek;
insert into onek_parthash select * from ext_largeonek;
truncate table onek;
truncate table onek_parthash;
insert into onek select * from gpfdist_largeonek;
insert into onek_parthash select * from gpfdist_largeonek;

truncate table onek;
truncate table onek2_parthash;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';
copy onek2_parthash from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/onek.data';
truncate table onek;
truncate table onek2_parthash;
insert into onek select * from ext_onek;
insert into onek2_parthash select * from ext_onek;
truncate table onek;
truncate table onek2_parthash;
insert into onek select * from gpfdist_onek;
insert into onek2_parthash select * from gpfdist_onek;

truncate table onek;
truncate table onek2_parthash;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data';
copy onek2_parthash from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/5mb.data';
truncate table onek;
truncate table onek2_parthash;
insert into onek select * from ext_mediumonek;
insert into onek2_parthash select * from ext_mediumonek;
truncate table onek;
truncate table onek2_parthash;
insert into onek select * from gpfdist_largeonek;
insert into onek2_parthash select * from gpfdist_mediumonek;

truncate table onek;
truncate table onek2_parthash;
copy onek from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data';
copy onek2_parthash from '/home/jsoedomo/perforce/cdbfast/main/partition/_data/54mb.data';
truncate table onek;
truncate table onek2_parthash;
insert into onek select * from ext_largeonek;
insert into onek2_parthash select * from ext_largeonek;
truncate table onek;
truncate table onek2_parthash;
insert into onek select * from gpfdist_largeonek;
insert into onek2_parthash select * from gpfdist_largeonek;

drop table onek_parthash;
drop table onek2_parthash;
