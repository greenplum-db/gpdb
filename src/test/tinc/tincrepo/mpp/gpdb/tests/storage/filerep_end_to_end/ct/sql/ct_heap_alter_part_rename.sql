-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CT HEAP TABLE 1
--
CREATE TABLE ct_heap_alter_part_rn1 (id int, rank int, year date, gender
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
insert into ct_heap_alter_part_rn1 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_rn1;

--
-- CT HEAP TABLE 2
--
CREATE TABLE ct_heap_alter_part_rn2 (id int, rank int, year date, gender
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
insert into ct_heap_alter_part_rn2 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_rn2;


--
-- CT HEAP TABLE 3
--
CREATE TABLE ct_heap_alter_part_rn3 (id int, rank int, year date, gender
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
insert into ct_heap_alter_part_rn3 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_rn3;


--
-- CT HEAP TABLE 4
--
CREATE TABLE ct_heap_alter_part_rn4 (id int, rank int, year date, gender
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
insert into ct_heap_alter_part_rn4 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_rn4;


--
-- CT HEAP TABLE 5
--
CREATE TABLE ct_heap_alter_part_rn5 (id int, rank int, year date, gender
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
insert into ct_heap_alter_part_rn5 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_rn5;



--
-- ALTER SYNC1 HEAP
--

--
-- Add default Partition
--
alter table sync1_heap_alter_part_rn4 add default partition default_part;
--
-- Rename Default Partition
--
alter table sync1_heap_alter_part_rn4 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_rn4 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn4;

--
-- Rename Table
--
alter table sync1_heap_alter_part_rn4 rename to  sync1_heap_alter_part_rn4_0;

--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_rn4_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn4_0;
--
-- ALTER CK_SYNC1 HEAP
--

--
-- Add default Partition
--
alter table ck_sync1_heap_alter_part_rn3 add default partition default_part;
--
-- Rename Default Partition
--
alter table ck_sync1_heap_alter_part_rn3 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into ck_sync1_heap_alter_part_rn3 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_rn3;

--
-- Rename Table
--
alter table ck_sync1_heap_alter_part_rn3 rename to  ck_sync1_heap_alter_part_rn3_0;

--
-- Insert few records into the table
--
insert into ck_sync1_heap_alter_part_rn3_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_rn3_0;


--
-- ALTER CT HEAP
--

--
-- Add default Partition
--
alter table ct_heap_alter_part_rn1 add default partition default_part;
--
-- Rename Default Partition
--
alter table ct_heap_alter_part_rn1 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into ct_heap_alter_part_rn1 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_rn1;

--
-- Rename Table
--
alter table ct_heap_alter_part_rn1 rename to  ct_heap_alter_part_rn1_0;

--
-- Insert few records into the table
--
insert into ct_heap_alter_part_rn1_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_heap_alter_part_rn1_0;
