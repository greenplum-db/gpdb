-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CK_SYNC1 CO Part Table 1
--
CREATE TABLE ck_sync1_co_alter_part_add_col1 (id int, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_col1 values (1,1,'2001-01-01','F');
insert into ck_sync1_co_alter_part_add_col1 values (2,2,'2002-01-01','M');
insert into ck_sync1_co_alter_part_add_col1 values (3,3,'2003-01-01','F');
insert into ck_sync1_co_alter_part_add_col1 values (4,4,'2003-01-01','M');
--
-- CK_SYNC1 CO Part Table 2
--
CREATE TABLE ck_sync1_co_alter_part_add_col2 (id int, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_col2 values (1,1,'2001-01-01','F');
insert into ck_sync1_co_alter_part_add_col2 values (2,2,'2002-01-01','M');
insert into ck_sync1_co_alter_part_add_col2 values (3,3,'2003-01-01','F');
insert into ck_sync1_co_alter_part_add_col2 values (4,4,'2003-01-01','M');
--
-- CK_SYNC1 CO Part Table 3
--
CREATE TABLE ck_sync1_co_alter_part_add_col3 (id int, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_col3 values (1,1,'2001-01-01','F');
insert into ck_sync1_co_alter_part_add_col3 values (2,2,'2002-01-01','M');
insert into ck_sync1_co_alter_part_add_col3 values (3,3,'2003-01-01','F');
insert into ck_sync1_co_alter_part_add_col3 values (4,4,'2003-01-01','M');
--
-- CK_SYNC1 CO Part Table 4
--
CREATE TABLE ck_sync1_co_alter_part_add_col4 (id int, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_col4 values (1,1,'2001-01-01','F');
insert into ck_sync1_co_alter_part_add_col4 values (2,2,'2002-01-01','M');
insert into ck_sync1_co_alter_part_add_col4 values (3,3,'2003-01-01','F');
insert into ck_sync1_co_alter_part_add_col4 values (4,4,'2003-01-01','M');
--
-- CK_SYNC1 CO Part Table 5
--
CREATE TABLE ck_sync1_co_alter_part_add_col5 (id int, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_col5 values (1,1,'2001-01-01','F');
insert into ck_sync1_co_alter_part_add_col5 values (2,2,'2002-01-01','M');
insert into ck_sync1_co_alter_part_add_col5 values (3,3,'2003-01-01','F');
insert into ck_sync1_co_alter_part_add_col5 values (4,4,'2003-01-01','M');
--
-- CK_SYNC1 CO Part Table 6
--
CREATE TABLE ck_sync1_co_alter_part_add_col6 (id int, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_col6 values (1,1,'2001-01-01','F');
insert into ck_sync1_co_alter_part_add_col6 values (2,2,'2002-01-01','M');
insert into ck_sync1_co_alter_part_add_col6 values (3,3,'2003-01-01','F');
insert into ck_sync1_co_alter_part_add_col6 values (4,4,'2003-01-01','M');
--
-- CK_SYNC1 CO Part Table 7
--
CREATE TABLE ck_sync1_co_alter_part_add_col7 (id int, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_col7 values (1,1,'2001-01-01','F');
insert into ck_sync1_co_alter_part_add_col7 values (2,2,'2002-01-01','M');
insert into ck_sync1_co_alter_part_add_col7 values (3,3,'2003-01-01','F');
insert into ck_sync1_co_alter_part_add_col7 values (4,4,'2003-01-01','M');
--
--
--ALTER TABLE TO ADD COL 
--
--
--
-- ALTER SYNC1 CO Part Table 2 To Add INT COL
--
alter table sync1_co_alter_part_add_col2 add column AAA int default 1;
--
-- INSERT ROWS
--
insert into sync1_co_alter_part_add_col2 values (5,5,'2001-01-01','F',1);
insert into sync1_co_alter_part_add_col2 values (6,6,'2002-01-01','M',2);
insert into sync1_co_alter_part_add_col2 values (7,7,'2003-01-01','F',3);
insert into sync1_co_alter_part_add_col2 values (8,8,'2003-01-01','M',4);
--
--
-- ALTER SYNC1 CO Part Table 2 To Add TEXT COL
--
--
alter table sync1_co_alter_part_add_col2 add column BBB text default 'hello';
--
-- INSERT ROWS
--
insert into sync1_co_alter_part_add_col2 values (5,5,'2001-01-01','F',1,'text1');
insert into sync1_co_alter_part_add_col2 values (6,6,'2002-01-01','M',2,'text1');
insert into sync1_co_alter_part_add_col2 values (7,7,'2003-01-01','F',3,'text1');
insert into sync1_co_alter_part_add_col2 values (8,8,'2003-01-01','M',4,'text1');
--
-- ALTER CK_SYNC1 CO Part Table 1 To Add INT COL
--
alter table ck_sync1_co_alter_part_add_col1 add column AAA int default 1;
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_add_col1 values (5,5,'2001-01-01','F',1);
insert into ck_sync1_co_alter_part_add_col1 values (6,6,'2002-01-01','M',2);
insert into ck_sync1_co_alter_part_add_col1 values (7,7,'2003-01-01','F',3);
insert into ck_sync1_co_alter_part_add_col1 values (8,8,'2003-01-01','M',4);
--
--
-- ALTER CK_SYNC1 CO Part Table 1 To Add TEXT COL
--
--
alter table ck_sync1_co_alter_part_add_col1 add column BBB text default 'hello';
--
-- INSERT ROWS
--
insert into ck_sync1_co_alter_part_add_col1 values (5,5,'2001-01-01','F',1,'text1');
insert into ck_sync1_co_alter_part_add_col1 values (6,6,'2002-01-01','M',2,'text1');
insert into ck_sync1_co_alter_part_add_col1 values (7,7,'2003-01-01','F',3,'text1');
insert into ck_sync1_co_alter_part_add_col1 values (8,8,'2003-01-01','M',4,'text1');
