CREATE TABLESPACE ts1 FILESPACE filespace_test_a;
create table drop_table_ts_test(a int, b int) tablespace ts1;
insert into drop_table_ts_test select i,i+1 from generate_series(1,1000)i;
drop table drop_table_ts_test;
DROP TABLESPACE ts1;
