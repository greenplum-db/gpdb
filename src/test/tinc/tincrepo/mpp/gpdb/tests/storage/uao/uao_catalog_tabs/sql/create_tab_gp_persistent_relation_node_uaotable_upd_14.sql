-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
drop table if exists uao_table_test14;
create table uao_table_test14(i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);
insert into uao_table_test14 select i,'aa'||i,i+10 from generate_series(1,10) as i;
