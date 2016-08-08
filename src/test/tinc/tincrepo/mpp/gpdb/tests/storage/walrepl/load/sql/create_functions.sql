-- @Description Functions plpgsql function
--start_ignore
DROP FUNCTION ddl_insertData(integer);
--end_ignore
CREATE OR REPLACE FUNCTION ddl_insertData (startValue INTEGER) RETURNS VOID
AS
$$
DECLARE
   i INTEGER;
BEGIN
   i = startValue;
    EXECUTE 'INSERT INTO ddl_plpgsql_t2(a) VALUES (' || i || ')';
END;
$$
LANGUAGE PLPGSQL
;

--start_ignore
drop table if exists ddl_plpgsql_t2;
--end_ignore
create table ddl_plpgsql_t2( a int );
BEGIN;
select ddl_insertData(1);

DROP FUNCTION ddl_insertData(integer);

