-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CK_SYNC1 CO Part Table 1
--
CREATE TABLE ck_sync1_co_alter_part_drop_col1 (id int, name text,rank int, year date, gender  char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_drop_col1 values (generate_series(1,10),'ann',1,'2001-01-01','F');

--
-- CK_SYNC1 CO Part Table 2
--
CREATE TABLE ck_sync1_co_alter_part_drop_col2 (id int, name text,rank int, year date, gender  char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_drop_col2 values (generate_series(1,10),'ann',1,'2001-01-01','F');

--
-- CK_SYNC1 CO Part Table 3
--
CREATE TABLE ck_sync1_co_alter_part_drop_col3 (id int, name text,rank int, year date, gender  char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_drop_col3 values (generate_series(1,10),'ann',1,'2001-01-01','F');


--
-- CK_SYNC1 CO Part Table 4
--
CREATE TABLE ck_sync1_co_alter_part_drop_col4 (id int, name text,rank int, year date, gender  char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_drop_col4 values (generate_series(1,10),'ann',1,'2001-01-01','F');


--
-- CK_SYNC1 CO Part Table 5
--
CREATE TABLE ck_sync1_co_alter_part_drop_col5 (id int, name text,rank int, year date, gender  char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_drop_col5 values (generate_series(1,10),'ann',1,'2001-01-01','F');


--
-- CK_SYNC1 CO Part Table 6
--
CREATE TABLE ck_sync1_co_alter_part_drop_col6 (id int, name text,rank int, year date, gender  char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_drop_col6 values (generate_series(1,10),'ann',1,'2001-01-01','F');


--
-- CK_SYNC1 CO Part Table 7
--
CREATE TABLE ck_sync1_co_alter_part_drop_col7 (id int, name text,rank int, year date, gender  char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_drop_col7 values (generate_series(1,10),'ann',1,'2001-01-01','F');


--
--
--ALTER TABLE TO DROP COL
--
--
--
-- ALTER SYNC1 CO Part Table To DROP INT COL
--
alter table sync1_co_alter_part_drop_col2 DROP column rank;
--
-- INSERT ROWS
--
insert into sync1_co_alter_part_drop_col2 values (generate_series(1,10),'ann','2001-01-01','F');
select count(*) from sync1_co_alter_part_drop_col2;
--
-- ALTER SYNC1 CO Part Table  To DROP TEXT COL
--
alter table sync1_co_alter_part_drop_col2 DROP column name;
--
-- INSERT ROWS
--
insert into sync1_co_alter_part_drop_col2 values (generate_series(1,10),'2001-01-01','F');
select count(*) from sync1_co_alter_part_drop_col2;

--
-- ALTER CK_SYNC1 CO Part Table To DROP INT COL
--
alter table ck_sync1_co_alter_part_drop_col1 DROP column rank;
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_drop_col1 values (generate_series(1,10),'ann','2001-01-01','F');
select count(*) from ck_sync1_co_alter_part_drop_col1;
--
-- ALTER CK_SYNC1 CO Part Table  To DROP TEXT COL
--
alter table ck_sync1_co_alter_part_drop_col1 DROP column name;
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_drop_col1 values (generate_series(1,10),'2001-01-01','F');
select count(*) from ck_sync1_co_alter_part_drop_col1;
