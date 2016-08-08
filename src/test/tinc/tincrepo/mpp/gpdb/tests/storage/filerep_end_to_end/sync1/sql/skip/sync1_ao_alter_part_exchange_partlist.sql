-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC1 AO TABLE 1
--

CREATE TABLE sync1_ao_alter_part_exchange_partlist1 (
        unique1         int4,
        unique2         int4)   with ( appendonly='true') partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE sync1_ao_alter_part_exchange_partlist1_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true')  ;


--
-- SYNC1 AO TABLE 2
--

CREATE TABLE sync1_ao_alter_part_exchange_partlist2 (
        unique1         int4,
        unique2         int4)   with ( appendonly='true') partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE sync1_ao_alter_part_exchange_partlist2_A (
        unique1         int4,
        unique2         int4)  with ( appendonly='true') ;


--
-- SYNC1 AO TABLE 3
--

CREATE TABLE sync1_ao_alter_part_exchange_partlist3 (
        unique1         int4,
        unique2         int4)   with ( appendonly='true') partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE sync1_ao_alter_part_exchange_partlist3_A (
        unique1         int4,
        unique2         int4)  with ( appendonly='true') ;


--
-- SYNC1 AO TABLE 4
--

CREATE TABLE sync1_ao_alter_part_exchange_partlist4 (
        unique1         int4,
        unique2         int4)   with ( appendonly='true') partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE sync1_ao_alter_part_exchange_partlist4_A (
        unique1         int4,
        unique2         int4)  with ( appendonly='true') ;



--
-- SYNC1 AO TABLE 5
--

CREATE TABLE sync1_ao_alter_part_exchange_partlist5 (
        unique1         int4,
        unique2         int4)   with ( appendonly='true') partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE sync1_ao_alter_part_exchange_partlist5_A (
        unique1         int4,
        unique2         int4)  with ( appendonly='true') ;



--
-- SYNC1 AO TABLE 6
--

CREATE TABLE sync1_ao_alter_part_exchange_partlist6 (
        unique1         int4,
        unique2         int4)   with ( appendonly='true') partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE sync1_ao_alter_part_exchange_partlist6_A (
        unique1         int4,
        unique2         int4)  with ( appendonly='true') ;


--
-- SYNC1 AO TABLE 7
--

CREATE TABLE sync1_ao_alter_part_exchange_partlist7 (
        unique1         int4,
        unique2         int4)   with ( appendonly='true') partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE sync1_ao_alter_part_exchange_partlist7_A (
        unique1         int4,
        unique2         int4)  with ( appendonly='true') ;


--
-- SYNC1 AO TABLE 8
--

CREATE TABLE sync1_ao_alter_part_exchange_partlist8 (
        unique1         int4,
        unique2         int4)   with ( appendonly='true') partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE sync1_ao_alter_part_exchange_partlist8_A (
        unique1         int4,
        unique2         int4) ;



--
-- ALTER SYNC1 AO TABLE
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION LIST
--
alter table sync1_ao_alter_part_exchange_partlist1 exchange partition aa with table sync1_ao_alter_part_exchange_partlist1_A;

--
-- INSERT ROWS
--
insert into sync1_ao_alter_part_exchange_partlist1 values ( generate_series(5,50),generate_series(15,60));

-- This insert statement fails due to new constraint rules in 4.2 :the exchanged out table (the ex-part) carries along any constraints that  
-- existed on the part
insert into sync1_ao_alter_part_exchange_partlist1_A values ( generate_series(1,10),generate_series(21,30));

-- Alternative 1
insert into sync1_ao_alter_part_exchange_partlist1_A values ( generate_series(1,5),generate_series(21,30));

-- Alternative 2
alter table sync1_ao_alter_part_exchange_partlist1_a drop constraint sync1_ao_alter_part_exchange_partlist1_1_prt_aa_check;
insert into sync1_ao_alter_part_exchange_partlist1_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from sync1_ao_alter_part_exchange_partlist1;
