-- @Description Tests master only function execution with exception
-- 

DROP TABLE IF EXISTS public.test_exception_error CASCADE;
DROP FUNCTION IF EXISTS public.test_plpgsql() CASCADE;

CREATE TABLE public.test_exception_error (a INTEGER NOT NULL);

CREATE OR REPLACE FUNCTION public.test_plpgsql() RETURNS VOID AS
$$
BEGIN
	BEGIN
		PERFORM * from pg_class;
	EXCEPTION
		WHEN OTHERS THEN
		BEGIN
			RAISE NOTICE 'catching the exception ...';
		END;
	END;
END;
$$
LANGUAGE plpgsql;

INSERT INTO public.test_exception_error SELECT * FROM generate_series(1, 100);
SELECT public.test_plpgsql();

SELECT * FROM public.test_exception_error;
