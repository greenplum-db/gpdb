-- After ETF is created and executed, verified regular table function can be created and executed (no regression)
    DROP TYPE IF EXISTS outComp cascade;
    CREATE TYPE outComp AS (b1 varchar(10), b2 varchar(10));

    CREATE OR REPLACE FUNCTION tf_int2char(max integer) 
    RETURNS SETOF outComp AS $$
    DECLARE f outComp%ROWTYPE;
    BEGIN
      FOR i IN 1..max 
      LOOP
        f.b1 := CAST(i AS varchar(10));
        f.b2 := 'tf_test '||CAST(i AS varchar(10));
        RETURN NEXT f;
      END LOOP;
      RETURN;
    END;
    $$ LANGUAGE plpgsql;

    SELECT t1.*, t2.* 
       FROM tf_int2char(5) t1
            JOIN 
            tf_int2char(3) t2
            ON t1.b1 = t2.b1;

