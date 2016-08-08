-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
create table sync1_airline_booking_heap_1 (c1 int, c2 char(100)) distributed by (c1); 
insert into sync1_airline_booking_heap_1 values (112232, 'xyz'); 
insert into sync1_airline_booking_heap_1 select generate_series(1,100000), 'abcdefg'; 
delete from sync1_airline_booking_heap_1 where c1 not in (1, 12, 45, 46, 112232);

create table sync1_airline_booking_heap_2 (c1 int, c2 char(100)) distributed by (c1);
insert into sync1_airline_booking_heap_2 values (112232, 'xyz');
insert into sync1_airline_booking_heap_2 select generate_series(1,100000), 'abcdefg';
delete from sync1_airline_booking_heap_2 where c1 not in (1, 12, 45, 46, 112232);

create table sync1_airline_booking_heap_3 (c1 int, c2 char(100)) distributed by (c1);
insert into sync1_airline_booking_heap_3 values (112232, 'xyz');
insert into sync1_airline_booking_heap_3 select generate_series(1,100000), 'abcdefg';
delete from sync1_airline_booking_heap_3 where c1 not in (1, 12, 45, 46, 112232);

create table sync1_airline_booking_heap_4 (c1 int, c2 char(100)) distributed by (c1);
insert into sync1_airline_booking_heap_4 values (112232, 'xyz');
insert into sync1_airline_booking_heap_4 select generate_series(1,100000), 'abcdefg';
delete from sync1_airline_booking_heap_4 where c1 not in (1, 12, 45, 46, 112232);

create table sync1_airline_booking_heap_5 (c1 int, c2 char(100)) distributed by (c1);
insert into sync1_airline_booking_heap_5 values (112232, 'xyz');
insert into sync1_airline_booking_heap_5 select generate_series(1,100000), 'abcdefg';
delete from sync1_airline_booking_heap_5 where c1 not in (1, 12, 45, 46, 112232);

create table sync1_airline_booking_heap_6 (c1 int, c2 char(100)) distributed by (c1);
insert into sync1_airline_booking_heap_6 values (112232, 'xyz');
insert into sync1_airline_booking_heap_6 select generate_series(1,100000), 'abcdefg';
delete from sync1_airline_booking_heap_6 where c1 not in (1, 12, 45, 46, 112232);

create table sync1_airline_booking_heap_7 (c1 int, c2 char(100)) distributed by (c1);
insert into sync1_airline_booking_heap_7 values (112232, 'xyz');
insert into sync1_airline_booking_heap_7 select generate_series(1,100000), 'abcdefg';
delete from sync1_airline_booking_heap_7 where c1 not in (1, 12, 45, 46, 112232);

create table sync1_airline_booking_heap_8 (c1 int, c2 char(100)) distributed by (c1);
insert into sync1_airline_booking_heap_8 values (112232, 'xyz');
insert into sync1_airline_booking_heap_8 select generate_series(1,100000), 'abcdefg';
delete from sync1_airline_booking_heap_8 where c1 not in (1, 12, 45, 46, 112232);

vacuum sync1_airline_booking_heap_1;
