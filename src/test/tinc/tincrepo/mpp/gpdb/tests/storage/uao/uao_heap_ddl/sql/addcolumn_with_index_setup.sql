-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS mytab;
create table mytab (col1 int, name char(20) ) with (appendonly=true);
CREATE INDEX mytab_index ON mytab(name);
insert into mytab values (1,'a') ;
insert into mytab values (2,'b') ;
insert into mytab values (3,'c') ;
