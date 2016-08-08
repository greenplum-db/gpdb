-- @product_version gpdb: [4.3.5-]
set enable_partition_rules = off;
set gp_enable_hash_partitioned_tables = true;

-- Test partition with DROP
CREATE TABLE mpp3274 (
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

copy mpp3274 from '%PATH%/_data/onek.data';


CREATE TABLE mpp3274a_partlist (
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

CREATE TABLE mpp3274b_partlist (
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

-- No Rules but data is within constraint
CREATE TABLE mpp3274a_partlist_A (
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

-- No Rules but data is not within constraint
CREATE TABLE mpp3274a_partlist_B (
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

-- Table that has the same constraint with partition aa
CREATE TABLE mpp3274a_partlist_C (
    unique1 integer,
    unique2 integer,
    two integer,
    four integer,
    ten integer,
    twenty integer,
    hundred integer,
    thousand integer,
    twothousand integer,
    fivethous integer,
    tenthous integer,
    odd integer,
    even integer,
    stringu1 name,
    stringu2 name,
    string4 name,
    CONSTRAINT mpp3274a_partlist_1_prt_aa_check CHECK ((((((unique1 = 1) OR (unique1 = 2)) OR (unique1 = 3)) OR (unique1 = 4)) OR (unique1 = 5)))
);

insert into mpp3274a_partlist select * from mpp3274;
insert into mpp3274a_partlist_A select * from mpp3274a_partlist_1_prt_aa;
insert into mpp3274a_partlist_A select * from mpp3274a_partlist_1_prt_aa;
insert into mpp3274a_partlist_A select * from mpp3274a_partlist_1_prt_aa;
select count(*) from mpp3274a_partlist_A;
alter table mpp3274a_partlist exchange partition aa with table mpp3274a_partlist_A;
select count(*) from mpp3274a_partlist_1_prt_aa;
select count(*) from mpp3274a_partlist_A;

insert into mpp3274a_partlist_B select * from mpp3274;
select count(*) from mpp3274a_partlist_B;
-- Verified MPP-3274
alter table mpp3274a_partlist exchange partition aa with table mpp3274a_partlist_B;
select count(*) from mpp3274a_partlist_A;
select count(*) from mpp3274a_partlist_1_prt_aa;


insert into mpp3274a_partlist_C select * from mpp3274a_partlist_b;
-- This should fail because of the constraint
insert into mpp3274a_partlist_C select * from mpp3274a_partlist_1_prt_cc;
insert into mpp3274a_partlist_C select * from mpp3274a_partlist_b;
select count(*) from mpp3274a_partlist_C;
alter table mpp3274a_partlist exchange partition aa with table mpp3274a_partlist_C;
select count(*) from mpp3274a_partlist_C;
select count(*) from mpp3274a_partlist_1_prt_aa;
select count(*) from mpp3274a_partlist;

insert into mpp3274b_partlist select * from mpp3274;
truncate table mpp3274a_partlist_A;
insert into mpp3274a_partlist_A select * from mpp3274b_partlist_1_prt_aa;
insert into mpp3274a_partlist_A select * from mpp3274b_partlist_1_prt_aa;
insert into mpp3274a_partlist_A select * from mpp3274b_partlist_1_prt_aa;
alter table mpp3274b_partlist exchange partition aa with table mpp3274a_partlist_A; -- Error MPP3-3670

-- RANGE
CREATE TABLE mpp3274a_partrange (
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

-- Not complete
CREATE TABLE mpp3274b_partrange (
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
alter table mpp3274b_partrange add default partition default_part;

-- No Rules but data is within constraint
CREATE TABLE mpp3274a_partrange_A (
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

-- No Rules but data is not within constraint
CREATE TABLE mpp3274a_partrange_B (
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

-- Table that has the same constraint with partition aa
CREATE TABLE mpp3274a_partrange_C (
    unique1 integer,
    unique2 integer,
    two integer,
    four integer,
    ten integer,
    twenty integer,
    hundred integer,
    thousand integer,
    twothousand integer,
    fivethous integer,
    tenthous integer,
    odd integer,
    even integer,
    stringu1 name,
    stringu2 name,
    string4 name,
    CONSTRAINT mpp3274a_partrange_1_prt_aa1_check CHECK (((unique1 >= 0) AND (unique1 < 100)))
);

-- Exactly the same
CREATE TABLE mpp3274b_partrangeA (
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
alter table mpp3274b_partrangeA add default partition default_part;

-- The same, but no default partition
CREATE TABLE mpp3274b_partrangeB (
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

-- Only partial partition as the original
CREATE TABLE mpp3274b_partrangeC (
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
subpartition by range (unique2) subpartition template ( start (0) end (100) every (100) )
( start (0) end (1000) every (100));

insert into mpp3274a_partrange select * from mpp3274;
insert into mpp3274a_partrange_A select * from mpp3274a_partrange_1_prt_aa_1;
insert into mpp3274a_partrange_A select * from mpp3274a_partrange_1_prt_aa_1;
insert into mpp3274a_partrange_A select * from mpp3274a_partrange_1_prt_aa_1;
select count(*) from mpp3274a_partrange_A;
alter table mpp3274a_partrange exchange partition for (rank(1)) with table mpp3274a_partrange_A;
select count(*) from mpp3274a_partrange_1_prt_aa_1;
select count(*) from mpp3274a_partrange_A;
select count(*) from mpp3274a_partrange;

insert into mpp3274a_partrange_B select * from mpp3274;
select count(*) from mpp3274a_partrange_B;
--Verified MPP-3274
alter table mpp3274a_partrange exchange partition for (rank(1)) with table mpp3274a_partrange_B;
select count(*) from mpp3274a_partrange_B;
select count(*) from mpp3274a_partrange_1_prt_aa_1;

insert into mpp3274a_partlist_C select * from mpp3274a_partlist_b;
-- This should fail because of the constraint
insert into mpp3274a_partrange_C select * from mpp3274a_partrange_1_prt_aa_2;
insert into mpp3274a_partrange_C select * from mpp3274a_partrange_b;
select count(*) from mpp3274a_partrange_C;
alter table mpp3274a_partrange exchange partition for (rank(1)) with table mpp3274a_partrange_C;
select count(*) from mpp3274a_partrange_C;
select count(*) from mpp3274a_partrange_1_prt_aa_2;

------------------------------

insert into mpp3274b_partrange select * from mpp3274;
-- To be continued

-- Not complete, Not necessary for now because we are not releasing HASH partition
CREATE TABLE mpp3274a_parthash (
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


CREATE TABLE mpp3274b_parthash (
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

insert into mpp3274a_parthash select * from mpp3274;
insert into mpp3274b_parthash select * from mpp3274;

checkpoint;

drop table mpp3274;
drop table mpp3274a_partrange_A;
drop table mpp3274a_partrange_B;
drop table mpp3274a_partrange_C;
drop table mpp3274b_partrangeA;
drop table mpp3274b_partrangeB;
drop table mpp3274b_partrangeC;
drop table mpp3274a_partlist_A;
drop table mpp3274a_partlist_B;
drop table mpp3274a_partlist_C;
drop table mpp3274a_parthash;
drop table mpp3274b_parthash;
drop table mpp3274a_partlist;
drop table mpp3274b_partlist;
drop table mpp3274a_partrange;
drop table mpp3274b_partrange;
