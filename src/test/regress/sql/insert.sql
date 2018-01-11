--
-- insert with DEFAULT in the target_list
--
create table inserttest (col1 int4, col2 int4 NOT NULL, col3 text default 'testing');
insert into inserttest (col1, col2, col3) values (DEFAULT, DEFAULT, DEFAULT);
insert into inserttest (col2, col3) values (3, DEFAULT);
insert into inserttest (col1, col2, col3) values (DEFAULT, 5, DEFAULT);
insert into inserttest values (DEFAULT, 5, 'test');
insert into inserttest values (DEFAULT, 7);

select * from inserttest;

--
-- insert with similar expression / target_list values (all fail)
--
insert into inserttest (col1, col2, col3) values (DEFAULT, DEFAULT);
insert into inserttest (col1, col2, col3) values (1, 2);
insert into inserttest (col1) values (1, 2);
insert into inserttest (col1) values (DEFAULT, DEFAULT);

select * from inserttest;

--
-- VALUES test
--
insert into inserttest values(10, 20, '40'), (-1, 2, DEFAULT),
    ((select 2), (select i from (values(3)) as foo (i)), 'values are fun!');

select * from inserttest;

drop table inserttest;

-- MPP-6775 : Adding and dropping a column. Then perform an insert.
 
create table bar(x int) distributed randomly;        
create table foo(like bar) distributed randomly;

alter table foo add column y int;
alter table foo drop column y;

insert into bar values(1);
insert into bar values(2);

insert into foo(x) select  t1.x from    bar t1 join bar t2 on t1.x=t2.x;
insert into foo(x) select  t1.x from    bar t1;
insert into foo(x) select  t1.x from    bar t1 group by t1.x;

drop table if exists foo;
drop table if exists bar;

-- MPP-6775 : Dropping a column. Then perform an insert.

create table bar(x int, y int) distributed randomly;        
create table foo(like bar) distributed randomly;

alter table foo drop column y;

insert into bar values(1,1);
insert into bar values(2,2);

insert into foo(x) select  t1.x from    bar t1 join bar t2 on t1.x=t2.x;
insert into foo(x) select  t1.x from    bar t1;
insert into foo(x) select  t1.x from    bar t1 group by t1.x;

drop table if exists foo;
drop table if exists bar;
