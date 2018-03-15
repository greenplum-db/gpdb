CREATE TABLESPACE ts1 LOCATION '/tmp/twophase_create_tablespace_test_ts';
create table drop_table_ts_test(a int, b int) tablespace ts1;
insert into drop_table_ts_test select i,i+1 from generate_series(1,1000)i;
drop table drop_table_ts_test;
DROP TABLESPACE ts1;
