-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC1 HEAP TABLE 1
--
CREATE TABLE sync1_heap_alter_part_alter_dist1 (id int, name text,rank int, year date, gender char(1))  DISTRIBUTED randomly
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
insert into sync1_heap_alter_part_alter_dist1 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_alter_dist1;


--
-- SYNC1 HEAP TABLE 2
--
CREATE TABLE sync1_heap_alter_part_alter_dist2 (id int, name text,rank int, year date, gender char(1))  DISTRIBUTED randomly
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
insert into sync1_heap_alter_part_alter_dist2 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_alter_dist2;


--
-- SYNC1 HEAP TABLE 3
--
CREATE TABLE sync1_heap_alter_part_alter_dist3 (id int, name text,rank int, year date, gender char(1))  DISTRIBUTED randomly
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
insert into sync1_heap_alter_part_alter_dist3 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_alter_dist3;


--
-- SYNC1 HEAP TABLE 4
--
CREATE TABLE sync1_heap_alter_part_alter_dist4 (id int, name text,rank int, year date, gender char(1))  DISTRIBUTED randomly
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
insert into sync1_heap_alter_part_alter_dist4 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_alter_dist4;



--
-- SYNC1 HEAP TABLE 5
--
CREATE TABLE sync1_heap_alter_part_alter_dist5 (id int, name text,rank int, year date, gender char(1))  DISTRIBUTED randomly
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
insert into sync1_heap_alter_part_alter_dist5 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_alter_dist5;


--
-- SYNC1 HEAP TABLE 6
--
CREATE TABLE sync1_heap_alter_part_alter_dist6 (id int, name text,rank int, year date, gender char(1))  DISTRIBUTED randomly
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
insert into sync1_heap_alter_part_alter_dist6 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_alter_dist6;




--
-- SYNC1 HEAP TABLE 7
--
CREATE TABLE sync1_heap_alter_part_alter_dist7 (id int, name text,rank int, year date, gender char(1))  DISTRIBUTED randomly
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
insert into sync1_heap_alter_part_alter_dist7 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_alter_dist7;


--
-- SYNC1 HEAP TABLE 8
--
CREATE TABLE sync1_heap_alter_part_alter_dist8 (id int, name text,rank int, year date, gender char(1))  DISTRIBUTED randomly
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
insert into sync1_heap_alter_part_alter_dist8 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_alter_dist8;

--
-- ALTER SYNC1 HEAP TABLE 
--
--
-- ALTER PARTITION TABLE ALTER SET DISTRIBUTED BY
--
alter table sync1_heap_alter_part_alter_dist1 set distributed BY (id, gender, year);
--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_alter_dist1 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_alter_dist1;


-- ALTER PARTITION TABLE ALTER SET DISTRIBUTED RANDOMLY
--
alter table sync1_heap_alter_part_alter_dist1 set distributed randomly;
--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_alter_dist1 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_alter_dist1;


