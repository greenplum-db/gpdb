-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC HEAP TABLE 1
--

CREATE TABLE resync_heap_alter_part_exchange_partlist1 (
        unique1         int4,
        unique2         int4)  partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE resync_heap_alter_part_exchange_partlist1_A (
        unique1         int4,
        unique2         int4) ;


--
-- RESYNC HEAP TABLE 2
--

CREATE TABLE resync_heap_alter_part_exchange_partlist2 (
        unique1         int4,
        unique2         int4)  partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE resync_heap_alter_part_exchange_partlist2_A (
        unique1         int4,
        unique2         int4) ;


--
-- RESYNC HEAP TABLE 3
--

CREATE TABLE resync_heap_alter_part_exchange_partlist3 (
        unique1         int4,
        unique2         int4)  partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE resync_heap_alter_part_exchange_partlist3_A (
        unique1         int4,
        unique2         int4) ;




--
-- ALTER SYNC1 HEAP TABLE
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION LIST
--
alter table sync1_heap_alter_part_exchange_partlist6 exchange partition aa with table sync1_heap_alter_part_exchange_partlist6_A;

--
-- INSERT ROWS
--
insert into sync1_heap_alter_part_exchange_partlist6 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_heap_alter_part_exchange_partlist6_A values ( generate_series(1,5),generate_series(21,30));

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_exchange_partlist6;

--
-- ALTER CK_SYNC1 HEAP TABLE
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION LIST
--
alter table ck_sync1_heap_alter_part_exchange_partlist5 exchange partition aa with table ck_sync1_heap_alter_part_exchange_partlist5_A;

--
-- INSERT ROWS
--
insert into ck_sync1_heap_alter_part_exchange_partlist5 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_heap_alter_part_exchange_partlist5_A values ( generate_series(1,5),generate_series(21,30));

--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_exchange_partlist5;

--
-- ALTER CT HEAP TABLE
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION LIST
--
alter table ct_heap_alter_part_exchange_partlist3 exchange partition aa with table ct_heap_alter_part_exchange_partlist3_A;

--
-- INSERT ROWS
--
insert into ct_heap_alter_part_exchange_partlist3 values ( generate_series(5,50),generate_series(15,60));
insert into ct_heap_alter_part_exchange_partlist3_A values ( generate_series(1,5),generate_series(21,30));

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_exchange_partlist3;


--
-- ALTER RESYNC HEAP TABLE
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION LIST
--
alter table resync_heap_alter_part_exchange_partlist1 exchange partition aa with table resync_heap_alter_part_exchange_partlist1_A;

--
-- INSERT ROWS
--
insert into resync_heap_alter_part_exchange_partlist1 values ( generate_series(5,50),generate_series(15,60));

-- This insert statement fails due to new constraint rules in 4.2 :the exchanged out table (the ex-part) carries along any constraints that 
-- existed on the part
insert into resync_heap_alter_part_exchange_partlist1_A values ( generate_series(1,10),generate_series(21,30));

-- Alternative 1
insert into resync_heap_alter_part_exchange_partlist1_A values ( generate_series(1,5),generate_series(21,30));

-- Alternative 2
alter table resync_heap_alter_part_exchange_partlist1_a drop constraint resync_heap_alter_part_exchange_partlist1_1_prt_aa_check;
insert into resync_heap_alter_part_exchange_partlist1_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from resync_heap_alter_part_exchange_partlist1;

