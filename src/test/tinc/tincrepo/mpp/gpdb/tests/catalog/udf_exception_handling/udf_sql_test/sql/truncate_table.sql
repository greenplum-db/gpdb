-- @Description Tests truncate table success and failure case in function with exception
-- 


DROP TABLE IF EXISTS public.test_exception_error CASCADE;
DROP FUNCTION IF EXISTS test_plpgsql() CASCADE;

CREATE TABLE public.test_exception_error (a INTEGER NOT NULL);
INSERT INTO public.test_exception_error select * from generate_series(1, 100);

-- SUCCESS case
CREATE OR REPLACE FUNCTION public.test_plpgsql() RETURNS VOID AS
$$
BEGIN
	TRUNCATE TABLE public.test_exception_error;
	INSERT INTO public.test_exception_error select * from generate_series(100, 150);
EXCEPTION WHEN OTHERS THEN
	RAISE NOTICE 'catching the exception ...';
END;
$$
LANGUAGE plpgsql;

SELECT public.test_plpgsql();
SELECT count(*) FROM public.test_exception_error;

INSERT INTO public.test_exception_error SELECT * FROM generate_series(150, 200);

-- FAILURE scenario
CREATE OR REPLACE FUNCTION public.test_plpgsql() RETURNS VOID AS
$$
BEGIN
	TRUNCATE TABLE public.test_exception_error;
	INSERT INTO public.test_exception_error select * from generate_series(200, 250);
	INSERT INTO public.test_exception_error(a) VALUES(NULL);
EXCEPTION WHEN OTHERS THEN
	RAISE NOTICE 'catching the exception ...';
END;
$$
LANGUAGE plpgsql;

-- Raises Exception
SELECT public.test_plpgsql();
SELECT count(*) FROM public.test_exception_error;
INSERT INTO public.test_exception_error select * from generate_series(250, 300);

-- FAILURE scenario
CREATE OR REPLACE FUNCTION public.test_plpgsql() RETURNS VOID AS
$$
BEGIN
	INSERT INTO public.test_exception_error select * from generate_series(300, 350);
	INSERT INTO public.test_exception_error(a) VALUES(NULL);
EXCEPTION WHEN OTHERS THEN
	BEGIN
		TRUNCATE TABLE public.test_exception_error;
		RAISE NOTICE 'catching the exception ...';
	END;
END;
$$
LANGUAGE plpgsql;

-- Raises Exception
SELECT public.test_plpgsql();
SELECT count(*) FROM public.test_exception_error;
INSERT INTO public.test_exception_error select * from generate_series(350, 400);
SELECT count(*) FROM public.test_exception_error;

