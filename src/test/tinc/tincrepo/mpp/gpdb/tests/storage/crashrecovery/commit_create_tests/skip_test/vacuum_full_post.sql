set statement_mem = '200MB';
select count(*) from gpt_airline_booking_test2;
select count(*) from gpt_airline_booking_test3;
select count(*) from gpt_airline_booking_t1heap;
drop table gpt_airline_booking_test2;
drop table gpt_airline_booking_test3;
drop table gpt_airline_booking_t1heap;
