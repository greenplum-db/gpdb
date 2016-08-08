\c airline_booking_db
set statement_mem = '200MB';
select count(*) from airline_booking_test2;
select count(*) from airline_booking_test3;
select count(*) from airline_booking_t1heap;
\c template1
select pg_sleep(5);
drop database airline_booking_db;
