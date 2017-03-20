create database airline_booking_db template template0;
\c airline_booking_db
set statement_mem = '200MB';
drop table if exists airline_booking_test1;
drop table if exists airline_booking_test2;

create table airline_booking_test1 (i int) distributed by (i) partition by range(i) (start(1) end(1000) every(1));
create table airline_booking_test2 (i int) distributed by (i) partition by range(i) (start(1) end(1000) every(1));
alter table airline_booking_test2 drop partition for (100);
alter table airline_booking_test2 drop partition for (101);
alter table airline_booking_test2 drop partition for (102);
alter table airline_booking_test2 drop partition for (103);
alter table airline_booking_test2 drop partition for (104);
alter table airline_booking_test2 drop partition for (105);
drop table airline_booking_test1;
create table airline_booking_test3(i int) distributed by (i);

drop table if exists airline_booking_t1heap;
drop table if exists airline_booking_t2;
create table airline_booking_t1heap (c1 int, c2 char(100)) distributed by (c1);
insert into airline_booking_t1heap select generate_series(1,100000), 'abcdefg';
insert into airline_booking_t1heap values (112232, 'xyz');

delete from airline_booking_t1heap where c1 not in (1, 12, 45, 46, 112232);
select count(*) from airline_booking_test2;
select count(*) from airline_booking_test3;
select count(*) from airline_booking_t1heap;

