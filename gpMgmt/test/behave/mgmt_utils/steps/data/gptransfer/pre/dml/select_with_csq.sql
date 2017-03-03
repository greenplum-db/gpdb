\c gptest;

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
