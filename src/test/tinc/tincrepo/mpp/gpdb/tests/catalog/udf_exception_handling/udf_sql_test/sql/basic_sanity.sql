-- @Description Tests classical example of function with exception
-- 

DROP TABLE IF EXISTS public.test_exception_error CASCADE;
DROP FUNCTION IF EXISTS public.test_plpgsql() CASCADE;

CREATE TABLE public.test_exception_error (a INTEGER NOT NULL);

CREATE OR REPLACE FUNCTION public.test_plpgsql() RETURNS VOID AS
$$
BEGIN
    BEGIN
        INSERT INTO public.test_exception_error(a) VALUES(NULL);
        EXCEPTION
       	    WHEN OTHERS THEN
            BEGIN
			    RAISE NOTICE 'catching the exception ...';
            END;
	END;
END;
$$
LANGUAGE plpgsql;

-- Raises unexpected Exception but should not fail the command
SELECT public.test_plpgsql();

CREATE OR REPLACE FUNCTION public.test_plpgsql() RETURNS VOID AS
$$
BEGIN
    BEGIN
        INSERT INTO public.test_exception_error(a) VALUES(1), (2), (3), (NULL), (4), (5), (6), (7), (8), (9);
        EXCEPTION
       	    WHEN OTHERS THEN
            BEGIN
			    RAISE NOTICE 'catching the exception ...';
            END;
	END;
END;
$$
LANGUAGE plpgsql;

-- Raises unexpected Exception but should not fail the command
SELECT public.test_plpgsql();
