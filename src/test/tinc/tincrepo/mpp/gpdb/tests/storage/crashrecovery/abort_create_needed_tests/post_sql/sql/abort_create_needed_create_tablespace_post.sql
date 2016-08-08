CREATE TABLESPACE abort_create_needed_ts1 filespace filespace_test_a;
create table ct_ts_test(a int, b int) tablespace abort_create_needed_ts1;
insert into ct_ts_test select i,i+1 from generate_series(1,1000)i;
drop table ct_ts_test;
DROP TABLESPACE abort_create_needed_ts1;
