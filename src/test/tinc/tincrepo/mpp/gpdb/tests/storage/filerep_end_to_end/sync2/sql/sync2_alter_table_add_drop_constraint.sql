-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC2  - ADD Table_constraint & DROP Constraint
--

--
-- HEAP TABLE
--

 CREATE TABLE sync2_heap_films1 (
 code char(5),
 title varchar(40),
 did integer,
 date_prod date,
 kind varchar(10),
 len interval hour to minute,
 CONSTRAINT sync2_production1 UNIQUE(date_prod)
 );

 insert into sync2_heap_films1 values ('aaa','Heavenly Life',1,'2008-03-03','good','2 hr 30 mins');
 insert into sync2_heap_films1 values ('bbb','Beautiful Mind',2,'2007-05-05','good','1 hr 30 mins');
 insert into sync2_heap_films1 values ('ccc','Wonderful Earth',3,'2009-03-03','good','2 hr 15 mins');
 insert into sync2_heap_films1 values ('ddd','Heavenly Life 2',4,'2005-03-03','good','2 hr 30 mins');
 insert into sync2_heap_films1 values ('eee','Beautiful Mind 3',5,'2006-05-05','good','1 hr 30 mins');

select count(*) from sync2_heap_films1;

 CREATE TABLE sync2_heap_films2 (
 code char(5),
 title varchar(40),
 did integer,
 date_prod date,
 kind varchar(10),
 len interval hour to minute,
 CONSTRAINT sync2_production2 UNIQUE(date_prod)
 );

 insert into sync2_heap_films2 values ('aaa','Heavenly Life',1,'2008-03-03','good','2 hr 30 mins');
 insert into sync2_heap_films2 values ('bbb','Beautiful Mind',2,'2007-05-05','good','1 hr 30 mins');
 insert into sync2_heap_films2 values ('ccc','Wonderful Earth',3,'2009-03-03','good','2 hr 15 mins');
 insert into sync2_heap_films2 values ('ddd','Heavenly Life 2',4,'2005-03-03','good','2 hr 30 mins');
 insert into sync2_heap_films2 values ('eee','Beautiful Mind 3',5,'2006-05-05','good','1 hr 30 mins');

select count(*) from sync2_heap_films2;

--
--
-- ADD Table_constraint & DROP Constraint
--
--

--
--ADD table_constraint
--
ALTER TABLE sync1_heap_films7 ADD UNIQUE(code, date_prod);
select count(*) from sync1_heap_films7;

ALTER TABLE ck_sync1_heap_films6 ADD UNIQUE(code, date_prod);
select count(*) from ck_sync1_heap_films6;

ALTER TABLE ct_heap_films4 ADD UNIQUE(code, date_prod);
select count(*) from ct_heap_films4;

ALTER TABLE resync_heap_films2 ADD UNIQUE(code, date_prod);
select count(*) from resync_heap_films2;

ALTER TABLE sync2_heap_films1 ADD UNIQUE(code, date_prod);
select count(*) from sync2_heap_films1;

--
--DROP CONSTRAINT constraint_name [ RESTRICT | CASCADE ]
--
ALTER TABLE sync1_heap_films7 DROP CONSTRAINT sync1_production7;
select count(*) from sync1_heap_films7;

ALTER TABLE ck_sync1_heap_films6 DROP CONSTRAINT ck_sync1_production6;
select count(*) from ck_sync1_heap_films6;

ALTER TABLE ct_heap_films4 DROP CONSTRAINT ct_production4;
select count(*) from ct_heap_films4;

ALTER TABLE resync_heap_films2 DROP CONSTRAINT resync_production2;
select count(*) from resync_heap_films2;

ALTER TABLE sync2_heap_films1 DROP CONSTRAINT sync2_production1;
select count(*) from sync2_heap_films1;
