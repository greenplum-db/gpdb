-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC HEAP TABLE 1
--
CREATE TABLE resync_heap_alter_part_exchange_default_part1 (
        unique1         int4,
        unique2         int4
)    partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );



CREATE TABLE resync_heap_alter_part_exchange_default_part1_A (
        unique1         int4,
        unique2         int4) ;
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into resync_heap_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));



--
-- RESYNC HEAP TABLE 2
--
CREATE TABLE resync_heap_alter_part_exchange_default_part2 (
        unique1         int4,
        unique2         int4
)    partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );



CREATE TABLE resync_heap_alter_part_exchange_default_part2_A (
        unique1         int4,
        unique2         int4) ;
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_exchange_default_part2 values ( generate_series(5,50),generate_series(15,60));
insert into  resync_heap_alter_part_exchange_default_part2_A values ( generate_series(1,10),generate_series(21,30));

--
-- RESYNC HEAP TABLE 3
--
CREATE TABLE resync_heap_alter_part_exchange_default_part3 (
        unique1         int4,
        unique2         int4
)    partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );



CREATE TABLE  resync_heap_alter_part_exchange_default_part3_A (
        unique1         int4,
        unique2         int4) ;
--
-- Insert few records into the table
--
insert into  resync_heap_alter_part_exchange_default_part3 values ( generate_series(5,50),generate_series(15,60));
insert into  resync_heap_alter_part_exchange_default_part3_A values ( generate_series(1,10),generate_series(21,30));


--
--
-- ALTER SYNC1 HEAP EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table sync1_heap_alter_part_exchange_default_part6 exchange default partition with table sync1_heap_alter_part_exchange_default_part6_A;
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_exchange_default_part6 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_heap_alter_part_exchange_default_part6_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_exchange_default_part6;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table sync1_heap_alter_part_exchange_default_part6 exchange default partition with table sync1_heap_alter_part_exchange_default_part6_A with validation;
--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_exchange_default_part6 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_heap_alter_part_exchange_default_part6_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_exchange_default_part6;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table sync1_heap_alter_part_exchange_default_part6 exchange default partition with table sync1_heap_alter_part_exchange_default_part6_A without validation;

--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_exchange_default_part6 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_heap_alter_part_exchange_default_part6_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_exchange_default_part6;


--
--
-- ALTER CK_SYNC1 HEAP EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ck_sync1_heap_alter_part_exchange_default_part5 exchange default partition with table ck_sync1_heap_alter_part_exchange_default_part5_A;
--
-- Insert few records into the table
--
insert into ck_sync1_heap_alter_part_exchange_default_part5 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_heap_alter_part_exchange_default_part5_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_exchange_default_part5;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table ck_sync1_heap_alter_part_exchange_default_part5 exchange default partition with table ck_sync1_heap_alter_part_exchange_default_part5_A with validation;
--
-- Insert few records into the table
--

insert into ck_sync1_heap_alter_part_exchange_default_part5 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_heap_alter_part_exchange_default_part5_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_exchange_default_part5;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ck_sync1_heap_alter_part_exchange_default_part5 exchange default partition with table ck_sync1_heap_alter_part_exchange_default_part5_A without validation;

--
-- Insert few records into the table
--

insert into ck_sync1_heap_alter_part_exchange_default_part5 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_heap_alter_part_exchange_default_part5_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_exchange_default_part5;


--
--
-- ALTER CT HEAP EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ct_heap_alter_part_exchange_default_part3 exchange default partition with table ct_heap_alter_part_exchange_default_part3_A;
--
-- Insert few records into the table
--
insert into ct_heap_alter_part_exchange_default_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ct_heap_alter_part_exchange_default_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_exchange_default_part3;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table ct_heap_alter_part_exchange_default_part3 exchange default partition with table ct_heap_alter_part_exchange_default_part3_A with validation;
--
-- Insert few records into the table
--

insert into ct_heap_alter_part_exchange_default_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ct_heap_alter_part_exchange_default_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_exchange_default_part3;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ct_heap_alter_part_exchange_default_part3 exchange default partition with table ct_heap_alter_part_exchange_default_part3_A without validation;

--
-- Insert few records into the table
--

insert into ct_heap_alter_part_exchange_default_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ct_heap_alter_part_exchange_default_part3_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_exchange_default_part3;


--
--
-- ALTER RESYNC HEAP EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table resync_heap_alter_part_exchange_default_part1 exchange default partition with table resync_heap_alter_part_exchange_default_part1_A;
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into resync_heap_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_heap_alter_part_exchange_default_part1;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table resync_heap_alter_part_exchange_default_part1 exchange default partition with table resync_heap_alter_part_exchange_default_part1_A with validation;
--
-- Insert few records into the table
--

insert into resync_heap_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into resync_heap_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_heap_alter_part_exchange_default_part1;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table resync_heap_alter_part_exchange_default_part1 exchange default partition with table resync_heap_alter_part_exchange_default_part1_A without validation;

--
-- Insert few records into the table
--

insert into resync_heap_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into resync_heap_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from resync_heap_alter_part_exchange_default_part1;


