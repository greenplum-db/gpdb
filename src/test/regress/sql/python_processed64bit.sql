CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

CREATE LANGUAGE plpythonu;

DROP FUNCTION IF EXISTS public.test_bigint_python();
DROP TABLE IF EXISTS public.spi64bittestpython;

CREATE TABLE public.spi64bittestpython (id BIGSERIAL PRIMARY KEY, data BIGINT);

CREATE FUNCTION public.test_bigint_python()
RETURNS BIGINT
AS $$

res = plpy.execute("INSERT INTO public.spi64bittestpython (data) SELECT g FROM generate_series(1, 30000) g")

return res.nrows()

$$ LANGUAGE plpythonu;


-- insert 30k rows without fault injection framework
SELECT public.test_bigint();
SELECT COUNT(*) AS count
  FROM public.spi64bittestpython;


-- activate fault injection framework
SELECT gp_inject_fault('executor_run_high_processed', 'reset', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';


-- insert enough rows to trigger the fault injector
SELECT gp_inject_fault('executor_run_high_processed', 'skip', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';


-- and insert another 30k rows, this time overflowing the 2^32 counter
SELECT public.test_bigint();


-- reset fault injection framework
SELECT gp_inject_fault('executor_run_high_processed', 'reset', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';
SELECT COUNT(*) AS count
  FROM public.spi64bittestpython;


-- drop test table
DROP TABLE public.spi64bittestpython;
