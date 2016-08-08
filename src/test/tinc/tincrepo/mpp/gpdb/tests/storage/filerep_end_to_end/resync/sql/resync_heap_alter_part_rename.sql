-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC HEAP TABLE 1
--
CREATE TABLE resync_heap_alter_part_rn1 (id int, rank int, year date, gender
char(1))  DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_rn1 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from resync_heap_alter_part_rn1;

--
-- RESYNC HEAP TABLE 2
--
CREATE TABLE resync_heap_alter_part_rn2 (id int, rank int, year date, gender
char(1))  DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_rn2 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from resync_heap_alter_part_rn2;


--
-- RESYNC HEAP TABLE 3
--
CREATE TABLE resync_heap_alter_part_rn3 (id int, rank int, year date, gender
char(1))  DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_rn3 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from resync_heap_alter_part_rn3;


--
-- ALTER SYNC1 HEAP
--

--
-- Add default Partition
--
alter table sync1_heap_alter_part_rn6 add default partition default_part;
--
-- Rename Default Partition
--
alter table sync1_heap_alter_part_rn6 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_rn6 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn6;

--
-- Rename Table
--
alter table sync1_heap_alter_part_rn6 rename to  sync1_heap_alter_part_rn6_0;

--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_rn6_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn6_0;
--
-- ALTER CK_SYNC1 HEAP
--

--
-- Add default Partition
--
alter table ck_sync1_heap_alter_part_rn5 add default partition default_part;
--
-- Rename Default Partition
--
alter table ck_sync1_heap_alter_part_rn5 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into ck_sync1_heap_alter_part_rn5 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_rn5;

--
-- Rename Table
--
alter table ck_sync1_heap_alter_part_rn5 rename to  ck_sync1_heap_alter_part_rn5_0;

--
-- Insert few records into the table
--
insert into ck_sync1_heap_alter_part_rn5_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_rn5_0;

--
-- ALTER CT HEAP
--

--
-- Add default Partition
--
alter table ct_heap_alter_part_rn3 add default partition default_part;
--
-- Rename Default Partition
--
alter table ct_heap_alter_part_rn3 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into ct_heap_alter_part_rn3 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_rn3;

--
-- Rename Table
--
alter table ct_heap_alter_part_rn3 rename to  ct_heap_alter_part_rn3_0;

--
-- Insert few records into the table
--
insert into ct_heap_alter_part_rn3_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_rn3_0;

--
-- ALTER RESYNC HEAP
--

--
-- Add default Partition
--
alter table resync_heap_alter_part_rn1 add default partition default_part;
--
-- Rename Default Partition
--
alter table resync_heap_alter_part_rn1 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into resync_heap_alter_part_rn1 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from resync_heap_alter_part_rn1;

--
-- Rename Table
--
alter table resync_heap_alter_part_rn1 rename to  resync_heap_alter_part_rn1_0;

--
-- Insert few records into the table
--
insert into resync_heap_alter_part_rn1_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from resync_heap_alter_part_rn1_0;
