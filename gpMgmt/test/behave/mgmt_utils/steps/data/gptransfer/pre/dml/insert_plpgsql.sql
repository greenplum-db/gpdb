\c gptest;

-- start_ignore
DROP FUNCTION subt_insertValue(integer);
-- end_ignore
CREATE OR REPLACE FUNCTION subt_insertValue (inp INTEGER) RETURNS VOID
AS
$$
DECLARE
BEGIN
     EXECUTE 'INSERT INTO subt_plpgsql_t1(a) VALUES (%)' , i;
EXCEPTION
    WHEN others THEN
    RAISE NOTICE 'Error in data';

END;
$$
LANGUAGE PLPGSQL
;

-- start_ignore
DROP FUNCTION subt_inData(integer,integer);
-- end_ignore
CREATE OR REPLACE FUNCTION subt_inData (startValue INTEGER, endValue INTEGER) RETURNS VOID
AS
$$
DECLARE
   i INTEGER;
BEGIN
   i = startValue;
   WHILE i <= endValue LOOP
       PERFORM subt_insertValue(100);
       i = i + 1;
   END LOOP;
END;
$$
LANGUAGE PLPGSQL
;

-- start_ignore
drop table if exists subt_plpgsql_t1;
-- end_ignore
create table subt_plpgsql_t1( a char);
