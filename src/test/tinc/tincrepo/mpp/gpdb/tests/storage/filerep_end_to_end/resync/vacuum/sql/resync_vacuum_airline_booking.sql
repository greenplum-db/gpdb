-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
create table resync_airline_booking_heap_1 (c1 int, c2 char(100)) distributed by (c1); 
insert into resync_airline_booking_heap_1 values (112232, 'xyz'); 
insert into resync_airline_booking_heap_1 select generate_series(1,100000), 'abcdefg'; 
delete from resync_airline_booking_heap_1 where c1 not in (1, 12, 45, 46, 112232);

create table resync_airline_booking_heap_2 (c1 int, c2 char(100)) distributed by (c1);
insert into resync_airline_booking_heap_2 values (112232, 'xyz');
insert into resync_airline_booking_heap_2 select generate_series(1,100000), 'abcdefg';
delete from resync_airline_booking_heap_2 where c1 not in (1, 12, 45, 46, 112232);

create table resync_airline_booking_heap_3 (c1 int, c2 char(100)) distributed by (c1);
insert into resync_airline_booking_heap_3 values (112232, 'xyz');
insert into resync_airline_booking_heap_3 select generate_series(1,100000), 'abcdefg';
delete from resync_airline_booking_heap_3 where c1 not in (1, 12, 45, 46, 112232);

vacuum resync_airline_booking_heap_1;
vacuum ct_airline_booking_heap_3;
vacuum ck_sync1_airline_booking_heap_5;
vacuum sync1_airline_booking_heap_6;
