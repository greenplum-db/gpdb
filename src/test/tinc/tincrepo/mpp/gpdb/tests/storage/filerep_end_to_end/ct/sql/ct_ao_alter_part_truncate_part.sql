-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CT AO TABLE 1
--

CREATE TABLE ct_ao_alter_part_truncate_part1 (
        unique1         int4,
        unique2         int4
)  with ( appendonly='true') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE ct_ao_alter_part_truncate_part1_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true');
--
-- Insert few records into the table
--
insert into ct_ao_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into ct_ao_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_truncate_part1;


--
-- CT AO TABLE 2
--

CREATE TABLE ct_ao_alter_part_truncate_part2 (
        unique1         int4,
        unique2         int4
)  with ( appendonly='true') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE ct_ao_alter_part_truncate_part2_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true');
--
-- Insert few records into the table
--
insert into ct_ao_alter_part_truncate_part2 values ( generate_series(5,50),generate_series(15,60));
insert into ct_ao_alter_part_truncate_part2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_truncate_part2;


--
-- CT AO TABLE 3
--

CREATE TABLE ct_ao_alter_part_truncate_part3 (
        unique1         int4,
        unique2         int4
)  with ( appendonly='true') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE ct_ao_alter_part_truncate_part3_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true');
--
-- Insert few records into the table
--
insert into ct_ao_alter_part_truncate_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ct_ao_alter_part_truncate_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_truncate_part3;



--
-- CT AO TABLE 4
--

CREATE TABLE ct_ao_alter_part_truncate_part4 (
        unique1         int4,
        unique2         int4
)  with ( appendonly='true') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE ct_ao_alter_part_truncate_part4_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true');
--
-- Insert few records into the table
--
insert into ct_ao_alter_part_truncate_part4 values ( generate_series(5,50),generate_series(15,60));
insert into ct_ao_alter_part_truncate_part4_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_truncate_part4;


--
-- CT AO TABLE 5
--

CREATE TABLE ct_ao_alter_part_truncate_part5 (
        unique1         int4,
        unique2         int4
)  with ( appendonly='true') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE ct_ao_alter_part_truncate_part5_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true');
--
-- Insert few records into the table
--
insert into ct_ao_alter_part_truncate_part5 values ( generate_series(5,50),generate_series(15,60));
insert into ct_ao_alter_part_truncate_part5_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_truncate_part5;



--
-- ALTER SYNC1 AO
--

--
-- Truncate Partition
--
alter table sync1_ao_alter_part_truncate_part4 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into sync1_ao_alter_part_truncate_part4 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_ao_alter_part_truncate_part4_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table sync1_ao_alter_part_truncate_part4 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from sync1_ao_alter_part_truncate_part4;
--
-- Truncate default partition
--
alter table sync1_ao_alter_part_truncate_part4 truncate default partition;
--
-- Insert few records into the table
--
insert into sync1_ao_alter_part_truncate_part4 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_ao_alter_part_truncate_part4_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_ao_alter_part_truncate_part4;

--
-- ALTER CK_SYNC1 AO
--

--
-- Truncate Partition
--
alter table ck_sync1_ao_alter_part_truncate_part3 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_truncate_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_ao_alter_part_truncate_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table ck_sync1_ao_alter_part_truncate_part3 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_truncate_part3;
--
-- Truncate default partition
--
alter table ck_sync1_ao_alter_part_truncate_part3 truncate default partition;
--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_truncate_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_ao_alter_part_truncate_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_truncate_part3;

--
-- ALTER CT AO
--

--
-- Truncate Partition
--
alter table ct_ao_alter_part_truncate_part1 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into ct_ao_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into ct_ao_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table ct_ao_alter_part_truncate_part1 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_truncate_part1;
--
-- Truncate default partition
--
alter table ct_ao_alter_part_truncate_part1 truncate default partition;
--
-- Insert few records into the table
--
insert into ct_ao_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into ct_ao_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_truncate_part1;

