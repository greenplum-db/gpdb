set statement_mem = '200MB';
drop table if exists gpt_airline_booking_test1;
drop table if exists gpt_airline_booking_test2;

create table gpt_airline_booking_test1 (i int) partition by range(i) (start(1) end(1000) every(1));
create table gpt_airline_booking_test2 (i int) partition by range(i) (start(1) end(1000) every(1));
alter table gpt_airline_booking_test2 drop partition for (100);
alter table gpt_airline_booking_test2 drop partition for (101);
alter table gpt_airline_booking_test2 drop partition for (102);
alter table gpt_airline_booking_test2 drop partition for (103);
alter table gpt_airline_booking_test2 drop partition for (104);
alter table gpt_airline_booking_test2 drop partition for (105);
drop table gpt_airline_booking_test1;
create table gpt_airline_booking_test3(i int);

drop table if exists gpt_airline_booking_t1heap;
drop table if exists gpt_airline_booking_t2;
create table gpt_airline_booking_t1heap (c1 int, c2 char(100)) distributed by (c1);
insert into gpt_airline_booking_t1heap select generate_series(1,100000), 'abcdefg';
insert into gpt_airline_booking_t1heap values (112232, 'xyz');

delete from gpt_airline_booking_t1heap where c1 not in (1, 12, 45, 46, 112232);
reindex database gptest;
select count(*) from gpt_airline_booking_test2;
select count(*) from gpt_airline_booking_test3;
select count(*) from gpt_airline_booking_t1heap;

