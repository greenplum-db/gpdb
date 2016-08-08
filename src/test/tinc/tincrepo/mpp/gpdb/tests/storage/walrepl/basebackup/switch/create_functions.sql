-- @Description Functions plpgsql function

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

drop table if exists ddl_plpgsql_t2;
create table ddl_plpgsql_t2( a int );
BEGIN;
select ddl_insertData(1);

DROP FUNCTION ddl_insertData(integer);

