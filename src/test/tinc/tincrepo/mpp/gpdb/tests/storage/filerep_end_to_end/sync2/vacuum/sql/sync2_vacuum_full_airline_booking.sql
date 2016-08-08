-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
create table sync2_airline_booking_vacuum_full_heap_1 (c1 int, c2 char(100)) distributed by (c1); 
insert into sync2_airline_booking_vacuum_full_heap_1 values (112232, 'xyz'); 
insert into sync2_airline_booking_vacuum_full_heap_1 select generate_series(1,100000), 'abcdefg'; 
delete from sync2_airline_booking_vacuum_full_heap_1 where c1 not in (1, 12, 45, 46, 112232);

create table sync2_airline_booking_vacuum_full_heap_2 (c1 int, c2 char(100)) distributed by (c1);
insert into sync2_airline_booking_vacuum_full_heap_2 values (112232, 'xyz');
insert into sync2_airline_booking_vacuum_full_heap_2 select generate_series(1,100000), 'abcdefg';
delete from sync2_airline_booking_vacuum_full_heap_2 where c1 not in (1, 12, 45, 46, 112232);


vacuum full sync2_airline_booking_vacuum_full_heap_1;
vacuum full resync_airline_booking_vacuum_full_heap_2;
vacuum full ct_airline_booking_vacuum_full_heap_4;
vacuum full ck_sync1_airline_booking_vacuum_full_heap_6;
vacuum full sync1_airline_booking_vacuum_full_heap_7;
