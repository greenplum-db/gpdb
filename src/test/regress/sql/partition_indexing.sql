set enable_partition_rules = off;
set gp_enable_hash_partitioned_tables = true;

CREATE TABLE mpp3033 (
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
\copy mpp3033 from 'data/onek.data';

CREATE TABLE mpp3033a (
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
) distributed by (unique1) partition by hash (unique1) partitions 4;

CREATE TABLE mpp3033b (
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
) distributed by (unique1) partition by hash (unique1)
subpartition by hash (unique2)
(
partition aa (subpartition cc, subpartition dd),
partition bb (subpartition cc, subpartition dd)
);

insert into mpp3033a select * from mpp3033;
insert into mpp3033b select * from mpp3033;

CREATE INDEX mpp3033a_unique1 ON mpp3033a USING btree(unique1 int4_ops);
CREATE INDEX mpp3033a_unique2 ON mpp3033a USING btree(unique2 int4_ops);
CREATE INDEX mpp3033a_hundred ON mpp3033a USING btree(hundred int4_ops);
CREATE INDEX mpp3033a_stringu1 ON mpp3033a USING btree(stringu1 name_ops);

CREATE INDEX mpp3033b_unique1 ON mpp3033b USING btree(unique1 int4_ops);
CREATE INDEX mpp3033b_unique2 ON mpp3033b USING btree(unique2 int4_ops);
CREATE INDEX mpp3033b_hundred ON mpp3033b USING btree(hundred int4_ops);
CREATE INDEX mpp3033b_stringu1 ON mpp3033b USING btree(stringu1 name_ops);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;


select count(*) from mpp3033a;
select count(*) from mpp3033b;

drop index mpp3033a_unique1;
drop index mpp3033a_unique2;
drop index mpp3033a_hundred;
drop index mpp3033a_stringu1;

drop index mpp3033b_unique1;
drop index mpp3033b_unique2;
drop index mpp3033b_hundred;
drop index mpp3033b_stringu1;


CREATE INDEX mpp3033a_unique1 ON mpp3033a (unique1);
CREATE INDEX mpp3033a_unique2 ON mpp3033a (unique2);
CREATE INDEX mpp3033a_hundred ON mpp3033a (hundred);
CREATE INDEX mpp3033a_stringu1 ON mpp3033a (stringu1);

CREATE INDEX mpp3033b_unique1 ON mpp3033b (unique1);
CREATE INDEX mpp3033b_unique2 ON mpp3033b (unique2);
CREATE INDEX mpp3033b_hundred ON mpp3033b (hundred);
CREATE INDEX mpp3033b_stringu1 ON mpp3033b (stringu1);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;


select count(*) from mpp3033a;
select count(*) from mpp3033b;

drop index mpp3033a_unique1;
drop index mpp3033a_unique2;
drop index mpp3033a_hundred;
drop index mpp3033a_stringu1;

drop index mpp3033b_unique1;
drop index mpp3033b_unique2;
drop index mpp3033b_hundred;
drop index mpp3033b_stringu1;

CREATE UNIQUE INDEX mpp3033a_unique1 ON mpp3033a (unique1);
CREATE UNIQUE INDEX mpp3033a_unique2 ON mpp3033a (unique2);
CREATE UNIQUE INDEX mpp3033a_hundred ON mpp3033a (hundred);
CREATE UNIQUE INDEX mpp3033a_stringu1 ON mpp3033a (stringu1);

CREATE UNIQUE INDEX mpp3033b_unique1 ON mpp3033b (unique1);
CREATE UNIQUE INDEX mpp3033b_unique2 ON mpp3033b (unique2);
CREATE UNIQUE INDEX mpp3033b_hundred ON mpp3033b (hundred);
CREATE UNIQUE INDEX mpp3033b_stringu1 ON mpp3033b (stringu1);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;


select count(*) from mpp3033a;
select count(*) from mpp3033b;

drop index mpp3033a_unique1;
drop index mpp3033a_unique2;
drop index mpp3033a_hundred;
drop index mpp3033a_stringu1;

drop index mpp3033b_unique1;
drop index mpp3033b_unique2;
drop index mpp3033b_hundred;
drop index mpp3033b_stringu1;

CREATE INDEX mpp3033a_unique1 ON mpp3033a USING bitmap (unique1);
CREATE INDEX mpp3033a_unique2 ON mpp3033a USING bitmap (unique2);
CREATE INDEX mpp3033a_hundred ON mpp3033a USING bitmap (hundred);
CREATE INDEX mpp3033a_stringu1 ON mpp3033a USING bitmap (stringu1);

CREATE INDEX mpp3033b_unique1 ON mpp3033b USING bitmap (unique1);
CREATE INDEX mpp3033b_unique2 ON mpp3033b USING bitmap (unique2);
CREATE INDEX mpp3033b_hundred ON mpp3033b USING bitmap (hundred);
CREATE INDEX mpp3033b_stringu1 ON mpp3033b USING bitmap (stringu1);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;


select count(*) from mpp3033a;
select count(*) from mpp3033b;

-- partition_list_index.sql


-- Test partition with CREATE INDEX
DROP TABLE if exists mpp3033a;
DROP TABLE if exists mpp3033b;

CREATE TABLE mpp3033a (
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
) distributed by (unique1) partition by list (unique1) (
partition aa values (1,2,3,4,5,6,7,8,9,10),
partition bb values (11,12,13,14,15,16,17,18,19,20),
default partition default_part
);

CREATE TABLE mpp3033b (
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
) distributed by (unique1) partition by list (unique1)
subpartition by list (unique2)
(
partition aa values (1,2,3,4,5,6,7,8,9,10) (subpartition cc values (1,2,3), subpartition dd values (4,5,6) ),
partition bb values (11,12,13,14,15,16,17,18,19,20) (subpartition cc values (1,2,3), subpartition dd values (4,5,6) )
);
alter table mpp3033b add default partition default_part;

insert into mpp3033a select * from mpp3033;
insert into mpp3033b select * from mpp3033;

CREATE INDEX mpp3033a_unique1 ON mpp3033a USING btree(unique1 int4_ops);
CREATE INDEX mpp3033a_unique2 ON mpp3033a USING btree(unique2 int4_ops);
CREATE INDEX mpp3033a_hundred ON mpp3033a USING btree(hundred int4_ops);
CREATE INDEX mpp3033a_stringu1 ON mpp3033a USING btree(stringu1 name_ops);

CREATE INDEX mpp3033b_unique1 ON mpp3033b USING btree(unique1 int4_ops);
CREATE INDEX mpp3033b_unique2 ON mpp3033b USING btree(unique2 int4_ops);
CREATE INDEX mpp3033b_hundred ON mpp3033b USING btree(hundred int4_ops);
CREATE INDEX mpp3033b_stringu1 ON mpp3033b USING btree(stringu1 name_ops);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;


select count(*) from mpp3033a;
select count(*) from mpp3033b;

drop index mpp3033a_unique1;
drop index mpp3033a_unique2;
drop index mpp3033a_hundred;
drop index mpp3033a_stringu1;

drop index mpp3033b_unique1;
drop index mpp3033b_unique2;
drop index mpp3033b_hundred;
drop index mpp3033b_stringu1;


CREATE INDEX mpp3033a_unique1 ON mpp3033a (unique1);
CREATE INDEX mpp3033a_unique2 ON mpp3033a (unique2);
CREATE INDEX mpp3033a_hundred ON mpp3033a (hundred);
CREATE INDEX mpp3033a_stringu1 ON mpp3033a (stringu1);

CREATE INDEX mpp3033b_unique1 ON mpp3033b (unique1);
CREATE INDEX mpp3033b_unique2 ON mpp3033b (unique2);
CREATE INDEX mpp3033b_hundred ON mpp3033b (hundred);
CREATE INDEX mpp3033b_stringu1 ON mpp3033b (stringu1);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;

select count(*) from mpp3033a;
select count(*) from mpp3033b;

drop index mpp3033a_unique1;
drop index mpp3033a_unique2;
drop index mpp3033a_hundred;
drop index mpp3033a_stringu1;

drop index mpp3033b_unique1;
drop index mpp3033b_unique2;
drop index mpp3033b_hundred;
drop index mpp3033b_stringu1;

CREATE UNIQUE INDEX mpp3033a_unique1 ON mpp3033a (unique1);
CREATE UNIQUE INDEX mpp3033a_unique2 ON mpp3033a (unique2);
CREATE UNIQUE INDEX mpp3033a_hundred ON mpp3033a (hundred);
CREATE UNIQUE INDEX mpp3033a_stringu1 ON mpp3033a (stringu1);

CREATE UNIQUE INDEX mpp3033b_unique1 ON mpp3033b (unique1);
CREATE UNIQUE INDEX mpp3033b_unique2 ON mpp3033b (unique2);
CREATE UNIQUE INDEX mpp3033b_hundred ON mpp3033b (hundred);
CREATE UNIQUE INDEX mpp3033b_stringu1 ON mpp3033b (stringu1);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;

select count(*) from mpp3033b;

drop index mpp3033a_unique1;
drop index mpp3033a_unique2;
drop index mpp3033a_hundred;
drop index mpp3033a_stringu1;

drop index mpp3033b_unique1;
drop index mpp3033b_unique2;
drop index mpp3033b_hundred;
drop index mpp3033b_stringu1;

CREATE INDEX mpp3033a_unique1 ON mpp3033a USING bitmap (unique1);
CREATE INDEX mpp3033a_unique2 ON mpp3033a USING bitmap (unique2);
CREATE INDEX mpp3033a_hundred ON mpp3033a USING bitmap (hundred);
CREATE INDEX mpp3033a_stringu1 ON mpp3033a USING bitmap (stringu1);

CREATE INDEX mpp3033b_unique1 ON mpp3033b USING bitmap (unique1);
CREATE INDEX mpp3033b_unique2 ON mpp3033b USING bitmap (unique2);
CREATE INDEX mpp3033b_hundred ON mpp3033b USING bitmap (hundred);
CREATE INDEX mpp3033b_stringu1 ON mpp3033b USING bitmap (stringu1);

select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;


select count(*) from mpp3033a;
select count(*) from mpp3033b;

-- partition_range_index.sql
-- Test partition with CREATE INDEX
DROP TABLE if exists mpp3033a;
DROP TABLE if exists mpp3033b;

CREATE TABLE mpp3033a (
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
) distributed by (unique1) partition by range (unique1)
( partition aa start (0) end (1000) every (100), default partition default_part );

CREATE TABLE mpp3033b (
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
) distributed by (unique1) partition by range (unique1)
subpartition by range (unique2) subpartition template ( start (0) end (1000) every (500) )
( start (0) end (1000) every (200));
alter table mpp3033b add default partition default_part;

insert into mpp3033a select * from mpp3033;
insert into mpp3033b select * from mpp3033;

drop index if exists mpp3033a_unique1;
drop index if exists mpp3033a_unique2;
drop index if exists mpp3033a_hundred;
drop index if exists mpp3033a_stringu1;

drop index if exists mpp3033b_unique1;
drop index if exists mpp3033b_unique2;
drop index if exists mpp3033b_hundred;
drop index if exists mpp3033b_stringu1;

CREATE INDEX mpp3033a_unique1 ON mpp3033a USING btree(unique1 int4_ops);
CREATE INDEX mpp3033a_unique2 ON mpp3033a USING btree(unique2 int4_ops);
CREATE INDEX mpp3033a_hundred ON mpp3033a USING btree(hundred int4_ops);
CREATE INDEX mpp3033a_stringu1 ON mpp3033a USING btree(stringu1 name_ops);

CREATE INDEX mpp3033b_unique1 ON mpp3033b USING btree(unique1 int4_ops);
CREATE INDEX mpp3033b_unique2 ON mpp3033b USING btree(unique2 int4_ops);
CREATE INDEX mpp3033b_hundred ON mpp3033b USING btree(hundred int4_ops);
CREATE INDEX mpp3033b_stringu1 ON mpp3033b USING btree(stringu1 name_ops);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;


select count(*) from mpp3033a;
select count(*) from mpp3033b;

drop index mpp3033a_unique1;
drop index mpp3033a_unique2;
drop index mpp3033a_hundred;
drop index mpp3033a_stringu1;

drop index mpp3033b_unique1;
drop index mpp3033b_unique2;
drop index mpp3033b_hundred;
drop index mpp3033b_stringu1;


CREATE INDEX mpp3033a_unique1 ON mpp3033a (unique1);
CREATE INDEX mpp3033a_unique2 ON mpp3033a (unique2);
CREATE INDEX mpp3033a_hundred ON mpp3033a (hundred);
CREATE INDEX mpp3033a_stringu1 ON mpp3033a (stringu1);

CREATE INDEX mpp3033b_unique1 ON mpp3033b (unique1);
CREATE INDEX mpp3033b_unique2 ON mpp3033b (unique2);
CREATE INDEX mpp3033b_hundred ON mpp3033b (hundred);
CREATE INDEX mpp3033b_stringu1 ON mpp3033b (stringu1);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;


select count(*) from mpp3033a;
select count(*) from mpp3033b;

drop index mpp3033a_unique1;
drop index mpp3033a_unique2;
drop index mpp3033a_hundred;
drop index mpp3033a_stringu1;

drop index mpp3033b_unique1;
drop index mpp3033b_unique2;
drop index mpp3033b_hundred;
drop index mpp3033b_stringu1;

CREATE UNIQUE INDEX mpp3033a_unique1 ON mpp3033a (unique1);
CREATE UNIQUE INDEX mpp3033a_unique2 ON mpp3033a (unique2);
CREATE UNIQUE INDEX mpp3033a_hundred ON mpp3033a (hundred);
CREATE UNIQUE INDEX mpp3033a_stringu1 ON mpp3033a (stringu1);

CREATE UNIQUE INDEX mpp3033b_unique1 ON mpp3033b (unique1);
CREATE UNIQUE INDEX mpp3033b_unique2 ON mpp3033b (unique2);
CREATE UNIQUE INDEX mpp3033b_hundred ON mpp3033b (hundred);
CREATE UNIQUE INDEX mpp3033b_stringu1 ON mpp3033b (stringu1);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;


select count(*) from mpp3033a;
select count(*) from mpp3033b;

drop index mpp3033a_unique1;
drop index mpp3033a_unique2;
drop index mpp3033a_hundred;
drop index mpp3033a_stringu1;

drop index mpp3033b_unique1;
drop index mpp3033b_unique2;
drop index mpp3033b_hundred;
drop index mpp3033b_stringu1;

CREATE INDEX mpp3033a_unique1 ON mpp3033a USING bitmap (unique1);
CREATE INDEX mpp3033a_unique2 ON mpp3033a USING bitmap (unique2);
CREATE INDEX mpp3033a_hundred ON mpp3033a USING bitmap (hundred);
CREATE INDEX mpp3033a_stringu1 ON mpp3033a USING bitmap (stringu1);

CREATE INDEX mpp3033b_unique1 ON mpp3033b USING bitmap (unique1);
CREATE INDEX mpp3033b_unique2 ON mpp3033b USING bitmap (unique2);
CREATE INDEX mpp3033b_hundred ON mpp3033b USING bitmap (hundred);
CREATE INDEX mpp3033b_stringu1 ON mpp3033b USING bitmap (stringu1);


select count(*) from mpp3033a;
select count(*) from mpp3033b;

reindex index mpp3033a_unique1;
reindex index mpp3033a_unique2;
reindex index mpp3033a_hundred;
reindex index mpp3033a_stringu1;

reindex index mpp3033b_unique1;
reindex index mpp3033b_unique2;
reindex index mpp3033b_hundred;
reindex index mpp3033b_stringu1;


select count(*) from mpp3033a;
select count(*) from mpp3033b;

create table mpp6379(a int, b date, primary key (a,b)) distributed by (a) partition by range (b) (partition p1 end ('2009-01-02'::date));
insert into mpp6379( a, b ) values( 1, '20090101' );
insert into mpp6379( a, b ) values( 1, '20090101' );
alter table mpp6379 add partition p2 end(date '2009-01-03');
insert into mpp6379( a, b ) values( 2, '20090102' );
insert into mpp6379( a, b ) values( 2, '20090102' );
drop table mpp6379;
