-- Negative: test cases of using anytable as output of function in create time
    -- Create output table outComp
    drop table if exists outComp cascade;
    create table outComp (b1 int, b2 text);

    -- Create a non-enhanced table function
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
    -- This should succeed and function can be created successfully.

    -- Using **anytable** as return type
    CREATE OR REPLACE FUNCTION tf_int2char_bad1(max integer) 
    RETURNS anytable AS $$
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
    -- ERROR:  PL/pgSQL functions cannot return type anytable

    -- Using **anytable** as OUT parameter
    CREATE OR REPLACE FUNCTION tf_int2char_bad2(IN max integer, OUT a anytable) 
    AS $$
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
    -- ERROR:  functions cannot return "anytable" arguments

    -- Using **anytable** as INOUT parameter
    CREATE OR REPLACE FUNCTION tf_int2char_bad3(INOUT a anytable) 
    AS $$
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
    -- ERROR:  functions cannot return "anytable" arguments

    -- Negative: can't pass anytable as prepare argument
    PREPARE neg_p(anytable) AS SELECT * FROM transform( 
    TABLE(SELECT * FROM intable ));
    -- ERROR:  type "anytable" is not a valid parameter for PREPARE

    drop function tf_int2char(max integer);
    drop table if exists outComp cascade;
