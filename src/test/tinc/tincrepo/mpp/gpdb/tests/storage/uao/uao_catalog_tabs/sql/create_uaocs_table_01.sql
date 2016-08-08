-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
drop table if exists uaocs_tab_test1;
drop table if exists uaocs_tab_test2;
drop table if exists uaocs_tab_test3;

create table uaocs_tab_test1 (i int, j varchar(20), k int ) with (appendonly=true, orientation=column) DISTRIBUTED BY (i);
create table uaocs_tab_test2 (i int, j varchar(20), k int ) with (appendonly=true, orientation=column) DISTRIBUTED BY (i);
create table uaocs_tab_test3 (i int, j varchar(20) ) with (appendonly=true, orientation=column) DISTRIBUTED BY (i);
insert into uaocs_tab_test1 values(1,'test',2);
insert into uaocs_tab_test2 values(1,'test',4);
