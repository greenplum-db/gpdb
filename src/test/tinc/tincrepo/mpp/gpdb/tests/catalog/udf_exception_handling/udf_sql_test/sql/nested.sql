-- @Description Tests nested function blocks with exception
-- 


DROP TABLE IF EXISTS public.test_exception_error,abcd CASCADE;
DROP FUNCTION IF EXISTS public.test_plpgsql() CASCADE;

CREATE TABLE public.test_exception_error (a INTEGER NOT NULL);
CREATE TABLE abcd(a int);

INSERT INTO abcd SELECT * from generate_series(1, 100);
INSERT INTO public.test_exception_error SELECT * from generate_series(1, 100);

CREATE OR REPLACE FUNCTION public.test_plpgsql() RETURNS VOID AS
$$
BEGIN
    BEGIN
        INSERT INTO public.test_exception_error SELECT * from abcd;
	INSERT INTO public.test_exception_error SELECT * from public.test_exception_error;
        BEGIN
		INSERT INTO public.test_exception_error SELECT * from abcd;
		INSERT INTO public.test_exception_error SELECT * from public.test_exception_error;
		INSERT INTO public.test_exception_error(a) VALUES(1000),(NULL),(1001),(1002);
	        EXCEPTION
	       	    WHEN OTHERS THEN
	            BEGIN
			    RAISE NOTICE 'catching the exception ...1';
		    END;
	END;
        BEGIN
		INSERT INTO public.test_exception_error SELECT * from public.test_exception_error;
	        EXCEPTION
	       	    WHEN OTHERS THEN
	            BEGIN
			    RAISE NOTICE 'catching the exception ...2';
		    END;
	END;
        EXCEPTION
       	    WHEN OTHERS THEN
	    BEGIN
		RAISE NOTICE 'catching the exception ...3';
            END;
	END;
END;
$$
LANGUAGE plpgsql;

-- Raises unexpected Exception in function but cmd still should be successful
SELECT public.test_plpgsql();

SELECT count(*) from public.test_exception_error;
