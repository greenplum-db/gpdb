Create Language plpythonu;
DROP FUNCTION subt_queryexec(text);
CREATE OR REPLACE FUNCTION subt_queryexec( query text )
        returns boolean
AS $$

        try:
                plan = plpy.prepare( query )
                rv = plpy.execute( plan )
        except:
                plpy.notice( "Error Trapped: Not a valid input value" )
                return 'false'

        for r in rv:
                plpy.notice( str( r ) )

        return 'true'

$$ LANGUAGE plpythonu;


DROP FUNCTION subt_insertWrongValue(integer,integer);
CREATE OR REPLACE FUNCTION subt_insertWrongValue (startValue INTEGER, endValue INTEGER) RETURNS VOID
AS
$$
DECLARE
   i INTEGER;
BEGIN
   i = startValue;
   WHILE i <= endValue LOOP
       PERFORM subt_queryexec('insert into subt_plpython_t1 values(4545)');
       i = i + 1;
   END LOOP;
END;
$$
LANGUAGE PLPGSQL
;
Drop table if exists subt_plpython_t1;
Create table subt_plpython_t1(a char);

-- Insert more than 100 records.
SELECT subt_insertWrongValue(3, 250);
