-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CT CO TABLE 1
--
CREATE TABLE ct_co_alter_part_exchange_default_part1 (
        unique1         int4,
        unique2         int4
)     with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );



CREATE TABLE ct_co_alter_part_exchange_default_part1_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true', orientation='column')  ;
--
-- Insert few records into the table
--
insert into ct_co_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));



--
-- CT CO TABLE 2
--
CREATE TABLE ct_co_alter_part_exchange_default_part2 (
        unique1         int4,
        unique2         int4
)     with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );



CREATE TABLE ct_co_alter_part_exchange_default_part2_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true', orientation='column')  ;
--
-- Insert few records into the table
--
insert into ct_co_alter_part_exchange_default_part2 values ( generate_series(5,50),generate_series(15,60));
insert into  ct_co_alter_part_exchange_default_part2_A values ( generate_series(1,10),generate_series(21,30));

--
-- CT CO TABLE 3
--
CREATE TABLE ct_co_alter_part_exchange_default_part3 (
        unique1         int4,
        unique2         int4
)     with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );



CREATE TABLE  ct_co_alter_part_exchange_default_part3_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true', orientation='column')  ;
--
-- Insert few records into the table
--
insert into  ct_co_alter_part_exchange_default_part3 values ( generate_series(5,50),generate_series(15,60));
insert into  ct_co_alter_part_exchange_default_part3_A values ( generate_series(1,10),generate_series(21,30));

--
-- CT CO TABLE 4
--
CREATE TABLE ct_co_alter_part_exchange_default_part4 (
        unique1         int4,
        unique2         int4
)     with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );



CREATE TABLE  ct_co_alter_part_exchange_default_part4_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true', orientation='column')  ;
--
-- Insert few records into the table
--
insert into  ct_co_alter_part_exchange_default_part4 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_default_part4_A values ( generate_series(1,10),generate_series(21,30));

--
-- CT CO TABLE 5
--
CREATE TABLE  ct_co_alter_part_exchange_default_part5 (
        unique1         int4,
        unique2         int4
)     with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );



CREATE TABLE  ct_co_alter_part_exchange_default_part5_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true', orientation='column')  ;
--
-- Insert few records into the table
--
insert into  ct_co_alter_part_exchange_default_part5 values ( generate_series(5,50),generate_series(15,60));
insert into  ct_co_alter_part_exchange_default_part5_A values ( generate_series(1,10),generate_series(21,30));


--
--
-- ALTER SYNC1 CO EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table sync1_co_alter_part_exchange_default_part4 exchange default partition with table sync1_co_alter_part_exchange_default_part4_A;
--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_default_part4 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_default_part4_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_default_part4;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table sync1_co_alter_part_exchange_default_part4 exchange default partition with table sync1_co_alter_part_exchange_default_part4_A with validation;
--
-- Insert few records into the table
--

insert into sync1_co_alter_part_exchange_default_part4 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_default_part4_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_default_part4;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table sync1_co_alter_part_exchange_default_part4 exchange default partition with table sync1_co_alter_part_exchange_default_part4_A without validation;

--
-- Insert few records into the table
--

insert into sync1_co_alter_part_exchange_default_part4 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_default_part4_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_default_part4;


--
--
-- ALTER CK_SYNC1 CO EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ck_sync1_co_alter_part_exchange_default_part3 exchange default partition with table ck_sync1_co_alter_part_exchange_default_part3_A;
--
-- Insert few records into the table
--
insert into ck_sync1_co_alter_part_exchange_default_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_co_alter_part_exchange_default_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_exchange_default_part3;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table ck_sync1_co_alter_part_exchange_default_part3 exchange default partition with table ck_sync1_co_alter_part_exchange_default_part3_A with validation;
--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_exchange_default_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_co_alter_part_exchange_default_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_exchange_default_part3;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ck_sync1_co_alter_part_exchange_default_part3 exchange default partition with table ck_sync1_co_alter_part_exchange_default_part3_A without validation;

--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_exchange_default_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_co_alter_part_exchange_default_part3_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_exchange_default_part3;

--
--
-- ALTER CT CO EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ct_co_alter_part_exchange_default_part1 exchange default partition with table ct_co_alter_part_exchange_default_part1_A;
--
-- Insert few records into the table
--
insert into ct_co_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_default_part1;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table ct_co_alter_part_exchange_default_part1 exchange default partition with table ct_co_alter_part_exchange_default_part1_A with validation;
--
-- Insert few records into the table
--

insert into ct_co_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_default_part1;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ct_co_alter_part_exchange_default_part1 exchange default partition with table ct_co_alter_part_exchange_default_part1_A without validation;

--
-- Insert few records into the table
--

insert into ct_co_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_default_part1;


