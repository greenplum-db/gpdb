-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC1 HEAP TABLE 1
--
CREATE TABLE sync1_heap_alter_part_rn1 (id int, rank int, year date, gender
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
insert into sync1_heap_alter_part_rn1 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn1;

--
-- SYNC1 HEAP TABLE 2
--
CREATE TABLE sync1_heap_alter_part_rn2 (id int, rank int, year date, gender
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
insert into sync1_heap_alter_part_rn2 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn2;


--
-- SYNC1 HEAP TABLE 3
--
CREATE TABLE sync1_heap_alter_part_rn3 (id int, rank int, year date, gender
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
insert into sync1_heap_alter_part_rn3 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn3;


--
-- SYNC1 HEAP TABLE 4
--
CREATE TABLE sync1_heap_alter_part_rn4 (id int, rank int, year date, gender
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
insert into sync1_heap_alter_part_rn4 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn4;


--
-- SYNC1 HEAP TABLE 5
--
CREATE TABLE sync1_heap_alter_part_rn5 (id int, rank int, year date, gender
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
insert into sync1_heap_alter_part_rn5 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn5;


--
-- SYNC1 HEAP TABLE 6
--
CREATE TABLE sync1_heap_alter_part_rn6 (id int, rank int, year date, gender
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
insert into sync1_heap_alter_part_rn6 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn6;


--
-- SYNC1 HEAP TABLE 7
--
CREATE TABLE sync1_heap_alter_part_rn7 (id int, rank int, year date, gender
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
insert into sync1_heap_alter_part_rn7 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn7;


--
-- SYNC1 HEAP TABLE 8
--
CREATE TABLE sync1_heap_alter_part_rn8 (id int, rank int, year date, gender
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
insert into sync1_heap_alter_part_rn8 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn8;
--
-- ALTER SYNC1 HEAP
--

--
-- Add default Partition
--
alter table sync1_heap_alter_part_rn1 add default partition default_part;
--
-- Rename Default Partition
--
alter table sync1_heap_alter_part_rn1 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_rn1 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn1;

--
-- Rename Table
--
alter table sync1_heap_alter_part_rn1 rename to  sync1_heap_alter_part_rn1_0;

--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_rn1_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_rn1_0;
