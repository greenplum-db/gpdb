-- @Description Tests ALTER table success and failure case in function with exception
-- 


DROP TABLE IF EXISTS public.test_exception_error CASCADE;
DROP FUNCTION IF EXISTS test_plpgsql() CASCADE;

CREATE TABLE public.test_exception_error (a INTEGER NOT NULL);
INSERT INTO public.test_exception_error select * from generate_series(1, 100);


-- SUCCESS case
CREATE OR REPLACE FUNCTION public.test_plpgsql() RETURNS VOID AS
$$
BEGIN
    BEGIN
	ALTER TABLE public.test_exception_error set with ( reorganize='true') distributed randomly;
        EXCEPTION
       	    WHEN OTHERS THEN
            BEGIN
			    RAISE NOTICE 'catching the exception ...';
            END;
	END;
END;
$$
LANGUAGE plpgsql;

SELECT public.test_plpgsql();
SELECT count(*) FROM public.test_exception_error;

INSERT INTO public.test_exception_error SELECT * FROM generate_series(101, 200);

-- FAILURE scenario
CREATE OR REPLACE FUNCTION public.test_plpgsql() RETURNS VOID AS
$$
BEGIN
    BEGIN
	ALTER TABLE public.test_exception_error set with ( reorganize='true') distributed randomly;
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

-- Raises unexpected Exception
SELECT public.test_plpgsql();
SELECT count(*) FROM public.test_exception_error;
