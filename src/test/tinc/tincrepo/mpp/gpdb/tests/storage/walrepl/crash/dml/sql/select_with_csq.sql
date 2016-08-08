-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- start_ignore
drop table if exists csq_t1;
drop table if exists csq_t2;
drop table if exists csq_t3;
drop table if exists csq_t4;
drop table if exists csq_t5;
drop table if exists A;
drop table if exists B;
drop table if exists C;
-- end_ignore

create table csq_t1(a int, b int);
insert into csq_t1 values (1,2);
insert into csq_t1 values (3,4);
insert into csq_t1 values (5,6);
insert into csq_t1 values (7,8);

create table csq_t2(x int,y int);
insert into csq_t2 values(1,1);
insert into csq_t2 values(3,9);
insert into csq_t2 values(5,25);
insert into csq_t2 values(7,49);

create table csq_t3(c int, d text);
insert into csq_t3 values(1,'one');
insert into csq_t3 values(3,'three');
insert into csq_t3 values(5,'five');
insert into csq_t3 values(7,'seven');

create table csq_t4(a int, b int) distributed by (b);
insert into csq_t4 values (1,2);
insert into csq_t4 values (3,4);
insert into csq_t4 values (5,6);
insert into csq_t4 values (7,8);

create table csq_t5(x int,y int);
insert into csq_t5 values(1,1);
insert into csq_t5 values(3,9);
insert into csq_t5 values(5,25);
insert into csq_t5 values(7,49);

create table A(i integer, j integer);
insert into A values(1,1);
insert into A values(19,5);
insert into A values(99,62);
insert into A values(1,1);
insert into A values(78,-1);

create table B(i integer, j integer);
insert into B values(1,43);
insert into B values(88,1);
insert into B values(-1,62);
insert into B values(1,1);
insert into B values(32,5);
insert into B values(2,7);

create table C(i integer, j integer);
insert into C values(1,889);
insert into C values(288,1);
insert into C values(-1,625);
insert into C values(32,65);
insert into C values(32,62);
insert into C values(3,-1);
insert into C values(99,7);
insert into C values(78,62);
insert into C values(2,7);

-- -- -- --
-- Basic queries with ANY clause
-- -- -- --
select a, x from csq_t1, csq_t2 where csq_t1.a = any (select x);
select A.i from A where A.i = any (select B.i from B where A.i = B.i) order by A.i;

select * from A where A.j = any (select C.j from C where C.j = A.j) order by 1,2;
select * from A,B where A.j = any (select C.j from C where C.j = A.j and B.i = any (select C.i from C)) order by 1,2,3,4;
select * from A where A.j = any (select C.j from C,B where C.j = A.j and B.i = any (select C.i from C)) order by 1,2;
select * from A where A.j = any (select C.j from C,B where C.j = A.j and B.i = any (select C.i from C where C.i != 10 and C.i = B.i)) order by 1,2;
select * from A,B where A.j = any (select C.j from C where C.j = A.j and B.i = any (select C.i from C where C.i != 10 and C.i = A.i)) order by 1,2,3,4; -- Not supported, should fail

select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i = any (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i = any (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = any ( select C.j from C where not exists(select C.i from C,A where C.i = A.i and C.i =10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = any (select C.j from C where C.j = A.j and not exists (select sum(B.i) from B where C.i = B.i and C.i !=10)) order by A.i, B.i, C.j limit 10;



-- -- -- --
-- Basic CSQ with UPDATE statements
-- -- -- --
select * from csq_t4 order by a;
update csq_t4 set a = (select y from csq_t5 where x=a) where b < 8;
select * from csq_t4 order by a;
update csq_t4 set a = 9999 where csq_t4.a = (select max(x) from csq_t5);
select * from csq_t4 order by a;
update csq_t4 set a = (select max(y) from csq_t5 where x=a) where csq_t4.a = (select min(x) from csq_t5);
select * from csq_t4 order by a;
update csq_t4 set a = 8888 where (select (y*2)>b from csq_t5 where a=x);
select * from csq_t4 order by a;
update csq_t4 set a = 3333 where csq_t4.a in (select x from csq_t5);
select * from csq_t4 order by a;

update A set i = 11111 from C where C.i = A.i and exists (select C.j from C,B where C.j = B.j and A.j < 10);
select * from A order by A.i, A.j;
update A set i = 22222 from C where C.i = A.i and not exists (select C.j from C,B where C.j = B.j and A.j < 10);
select * from A order by A.i, A.j;

-- -- -- --
-- Basic CSQ with DELETE statements
-- -- -- --
select * from csq_t4 order by a;
delete from csq_t4 where a <= (select min(y) from csq_t5 where x=a);
select * from csq_t4 order by a;
delete from csq_t4 where csq_t4.a = (select x from csq_t5);
select * from csq_t4 order by a;
delete from csq_t4 where exists (select (y*2)>b from csq_t5 where a=x);
select * from csq_t4 order by a;
delete from csq_t4  where csq_t4.a = (select x from csq_t5 where a=x);
select * from csq_t4 order by a;

delete from  A TableA where exists (select C.j from C, B where C.j = B.j and TableA.j < 10);
select * from A order by A.i;
delete from A TableA where not exists (select C.j from C,B where C.j = B.j and TableA.j < 10);
select * from A order by A.i;
