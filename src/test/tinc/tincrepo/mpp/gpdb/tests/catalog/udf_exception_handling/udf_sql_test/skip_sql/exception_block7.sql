-- JUST placeholder, GPDB doesn't support TRIGGER functions, so not testing the sme right now

DROP FUNCTION test_plpgsql() CASCADE;

CREATE OR REPLACE FUNCTION test_plpgsql() RETURNS TRIGGER AS 
$$
BEGIN
    BEGIN
        INSERT INTO public.test_exception_error(a) VALUES(1), (2), (3);
        EXCEPTION
       	    WHEN OTHERS THEN
            BEGIN
			    RAISE NOTICE 'catching the exception ...';
            END;
	END;
	RETURN NULL;
END;
$$
LANGUAGE plpgsql;

CREATE CONSTRAINT TRIGGER emp_audit AFTER INSERT OR UPDATE OR DELETE ON abcd DEFERRABLE FOR EACH ROW EXECUTE PROCEDURE test_plpgsql();
--CREATE TRIGGER emp_audit AFTER INSERT OR UPDATE OR DELETE ON abcd EXECUTE PROCEDURE test_plpgsql();
