DROP FUNCTION subt_insertValue(integer);
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


DROP FUNCTION subt_inData(integer,integer);
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


drop table if exists subt_plpgsql_t1;
create table subt_plpgsql_t1( a char);
select subt_inData(1,105);
select count(*) from subt_plpgsql_t1;
