-- start_ignore
drop table if exists sales cascade;
drop table if exists newpart cascade;
-- end_ignore
create table sales (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id) PARTITION BY RANGE (date) ( START (date '2011-01-01')
INCLUSIVE END (date '2011-01-03') EXCLUSIVE EVERY (INTERVAL '1 day') );
alter table sales drop column amt;
create table newpart(like sales);

alter table sales exchange partition for ('2011-01-01') with table newpart;
select * from sales order by id;

-- add column before exchange partition
drop table sales;
drop table newpart;
create table sales (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id) PARTITION BY RANGE (date) ( START (date '2011-01-01')
INCLUSIVE END (date '2011-01-03') EXCLUSIVE EVERY (INTERVAL '1 day') );

alter table sales add column tax float;

create table newpart(like sales);

alter table sales exchange partition for ('2011-01-01') with table newpart;
select * from sales order by id;


-- drop column before exchange partition
drop table sales;
drop table newpart;
create table sales (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id) PARTITION BY RANGE (date) ( START (date '2011-01-01')
INCLUSIVE END (date '2011-01-03') EXCLUSIVE EVERY (INTERVAL '1 day') );

alter table sales add column tax float;
alter table sales drop column tax ;

create table newpart(like sales);

alter table sales exchange partition for ('2011-01-01') with table newpart;
select * from sales order by id;


-- rename column before exchange partition
drop table sales;
drop table newpart;
create table sales (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id) PARTITION BY RANGE (date) ( START (date '2011-01-01')
INCLUSIVE END (date '2011-01-03') EXCLUSIVE EVERY (INTERVAL '1 day') );

alter table sales rename COLUMN id to id_change;

create table newpart(like sales);

alter table sales exchange partition for ('2011-01-01') with table newpart;
select * from sales order by id_change;


-- retype column before exchange partition

drop table sales;
drop table newpart;
create table sales (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id) PARTITION BY RANGE (date) ( START (date '2011-01-01')
INCLUSIVE END (date '2011-01-03') EXCLUSIVE EVERY (INTERVAL '1 day') );

alter table sales alter COLUMN id TYPE numeric(10,2);

create table newpart(like sales);

alter table sales exchange partition for ('2011-01-01') with table newpart;
select * from sales order by id;
\d sales;

-- mix of add and drop column before exchange 
drop table sales;
drop table newpart;
create table sales (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id) PARTITION BY RANGE (date) ( START (date '2011-01-01')
INCLUSIVE END (date '2011-01-03') EXCLUSIVE EVERY (INTERVAL '1 day') );

alter table sales add column tax float;
alter table sales drop column tax ;

create table newpart(like sales);

alter table sales exchange partition for ('2011-01-01') with table newpart;
select * from sales order by id;


-- add column before split partition 
drop table sales;
drop table newpart;

create table sales (pkid serial, option1 int, option2 int, option3 int, primary key(pkid,option3))                           
distributed by (pkid) partition by range (option3)                                                                             
(                                                                                                                              
partition aa start(1) end(100),                                                                                                
partition bb start(101) end(200), 
partition cc start(201) end (300)                                                            
);

alter table sales add column tax float;

create table newpart(like sales);
alter table sales split partition for(1) at (50) into (partition aa1, partition aa2);

select * from sales order by pkid;
\d sales;


-- drop column before split partition
drop table sales cascade;
drop table newpart;

create table sales (pkid serial, option1 int, option2 int, option3 int, primary key(pkid, option3))
distributed by (pkid) partition by range (option3)
(
partition aa start(1) end(100),
partition bb start(101) end(200),
partition cc start(201) end (300)
);

alter table sales drop column option2;

create table newpart(like sales);
alter table sales split partition for(1) at (50) into (partition aa1, partition aa2);

select * from sales order by pkid;
\d sales;



-- rename column before split partition
drop table sales cascade;
drop table newpart;

create table sales (pkid serial, option1 int, option2 int, option3 int, primary key(pkid,option3))
distributed by (pkid) partition by range (option3)
(
partition aa start(1) end(100),
partition bb start(101) end(200),
partition cc start(201) end (300)
);

alter table sales rename COLUMN pkid to id_change;

create table newpart(like sales);
alter table sales split partition for(1) at (50) into (partition aa1, partition aa2);

select * from sales order by id_change;
\d sales;



-- retype column before split partition
drop table sales cascade;
drop table newpart;

create table sales (pkid serial, option1 int, option2 int, option3 int, primary key(pkid, option3))
distributed by (pkid) partition by range (option3)
(
partition aa start(1) end(100),
partition bb start(101) end(200),
partition cc start(201) end (300)
);

alter table sales alter COLUMN option1 TYPE numeric(10,2);

create table newpart(like sales);
alter table sales split partition for(1) at (50) into (partition aa1, partition aa2);

select * from sales order by pkid;
\d sales;



-- mix of add and drop various column before split 
drop table sales cascade;
drop table newpart;

create table sales (pkid serial, option1 int, option2 int, option3 int, primary key(pkid, option3))
distributed by (pkid) partition by range (option3)
(
partition aa start(1) end(100),
partition bb start(101) end(200),
partition cc start(201) end (300)
);

alter table sales add column tax float;
alter table sales drop column tax;

alter table sales split partition for(1) at (50) into (partition aa1, partition aa2);

select * from sales order by pkid;
\d sales;




-- mix of add and drop various column before split, and exchange parition at the end 
drop table sales cascade;
drop table newpart;

create table sales (pkid serial, option1 int, option2 int, option3 int, constraint partable_pkey primary key(pkid, option3))
distributed by (pkid) partition by range (option3)
(
partition aa start(1) end(100),
partition bb start(101) end(200),
partition cc start(201) end (300)
);

alter table sales add column tax float;
alter table sales drop column tax;

create table newpart(like sales);
alter table newpart add constraint partable_pkey primary key(pkid, option3);
alter table sales split partition for(1) at (50) into (partition aa1, partition aa2);

select table_schema, table_name, constraint_name, constraint_type
from information_schema.table_constraints
where table_name in ('sales', 'newpart')
and constraint_name = 'partable_pkey'
order by table_name desc;

alter table sales exchange partition for (101) with table newpart;

select * from sales order by pkid;
\d sales;



-- create exchange table before drop column, make sure the consistency check still exist 
drop table sales cascade;
drop table newpart cascade;

create table sales (pkid serial, option1 int, option2 int, option3 int, primary key(pkid, option3))
distributed by (pkid) partition by range (option3)
(
partition aa start(1) end(100),
partition bb start(101) end(200),
partition cc start(201) end (300)
);

create table newpart(like sales);
alter table sales drop column option2;

alter table sales exchange partition for (101) with table newpart;

select * from sales order by pkid;
\d sales;

