-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Joins

-- start_ignore
drop table if exists r;
drop table if exists m;
drop table if exists purchase_par cascade;
-- 3 tables: heap, master-only and partitioned table
-- end_ignore

create table r (a int, b int) distributed by (a);
create table m ();
	alter table m add column a int;
	alter table m add column b int;
CREATE TABLE purchase_par (id int, year int, month int, day int, region text)
   DISTRIBUTED BY (id)
   PARTITION BY LIST (region)
   (	PARTITION usa VALUES ('south'),
	PARTITION europe VALUES ('north'),
	PARTITION asia VALUES ('east'),
	DEFAULT PARTITION other_regions)
   ;
insert into r select generate_series(1, 10000), generate_series(1, 10000) * 3;
insert into m select generate_series(1, 1000), generate_series(1, 1000) * 4;


Insert into purchase_par values(1,2009,13,29,'east');
Insert into purchase_par values(2,2009,13,29,'west');
Insert into purchase_par values(3,2009,13,29,'north');
Insert into purchase_par values(4,2009,13,29,'south');
Insert into purchase_par values(5,2009,13,29,'east');
Insert into purchase_par values(6,2009,13,29,'west');
Insert into purchase_par values(7,2002,13,29,'north');
Insert into purchase_par values(8,2003,13,29,'south');
Insert into purchase_par values(9,2004,13,29,'east');
Insert into purchase_par values(10,2005,13,29,'west');
Insert into purchase_par values(11,2006,13,29,'north');
Insert into purchase_par values(12,2007,01,29,'south');
Insert into purchase_par values(13,2008,02,29,'east');
Insert into purchase_par values(14,2002,03,29,'west');
Insert into purchase_par values(15,2003,04,29,'north');
Insert into purchase_par values(16,2004,05,29,'south');
Insert into purchase_par values(17,2005,06,29,'east');
Insert into purchase_par values(18,2006,07,29,'west');
Insert into purchase_par values(19,2007,08,29,'north');
Insert into purchase_par values(20,2008,09,29,'south');

-- partitioned table: update --
select purchase_par.* from purchase_par where id in (select m.b from m, r where m.a = r.b) and day in (select a from r);
update purchase_par set region = 'new_region' where id in (select m.b from m, r where m.a = r.b) and day in (select a from r);
select purchase_par.* from purchase_par where id in (select m.b from m, r where m.a = r.b) and day in (select a from r);

select purchase_par.* from purchase_par,m,r where purchase_par.id = m.b and purchase_par.month = r.b;
update purchase_par set month = month+1 from r,m where purchase_par.id = m.b and purchase_par.month = r.b;
select purchase_par.* from purchase_par,m,r where purchase_par.id = m.b and purchase_par.month = r.b+1;

-- heap table: delete --
select * from r where b in (select month-1 from purchase_par,m where purchase_par.id = m.b);
delete from r where b in (select month-1 from purchase_par,m where purchase_par.id = m.b);
select * from r where b in (select month-1 from purchase_par,m where purchase_par.id = m.b);

-- master-only table: update
select m.* from m,r,purchase_par where m.a = r.b and m.b = purchase_par.id;
delete from m using r,purchase_par where m.a = r.b and m.b = purchase_par.id;
select m.* from m,r,purchase_par where m.a = r.b and m.b = purchase_par.id;

select m.* from m,r,purchase_par where m.a = r.a and m.b = purchase_par.id;
update m set b = m.b + 1 from r,purchase_par where m.a = r.a and m.b = purchase_par.id;
select m.* from m,r,purchase_par where m.a = r.a and m.b = purchase_par.id + 1;
--Drop tables

drop table r;
drop table m;
drop table purchase_par cascade;
