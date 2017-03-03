\c gptest;

--start_ignore
drop table if exists ddl_plpgsql_t2;
--end_ignore
create table ddl_plpgsql_t2( a int );
BEGIN;
select ddl_insertData(1);

DROP FUNCTION ddl_insertData(integer);
