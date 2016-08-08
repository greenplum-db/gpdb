CREATE OR REPLACE FUNCTION test_excep (arg INTEGER) RETURNS INTEGER 
AS $$
    DECLARE res INTEGER;
    BEGIN
        res := 100 / arg;
        RETURN res;
    EXCEPTION
        WHEN division_by_zero 
        THEN  RETURN 999;
    END;
$$
LANGUAGE plpgsql;

