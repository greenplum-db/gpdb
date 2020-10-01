--start_ignore
CREATE LANGUAGE plpython3u;
--end_ignore

DROP FUNCTION IF EXISTS public.test_bigint_python();
DROP TABLE IF EXISTS public.spi64bittestpython;

CREATE TABLE public.spi64bittestpython (id BIGSERIAL PRIMARY KEY, data BIGINT);

-- Insert 1 ~ 40000 here can guarantee each segment's processing more than 10000 rows
-- and less then 1000000(under jump hash or old module hash). This is the condition
-- that will trigger the faultinjection.
CREATE FUNCTION public.test_bigint_python()
RETURNS BIGINT
AS $$

res = plpy.execute("INSERT INTO public.spi64bittestpython (data) SELECT g FROM generate_series(1, 40000) g")

return res.nrows()

$$ LANGUAGE plpython3u;


-- insert 30k rows without fault injection framework
SELECT public.test_bigint_python();
SELECT COUNT(*) AS count
  FROM public.spi64bittestpython;


-- activate fault injection framework
SELECT gp_inject_fault('executor_run_high_processed', 'skip', dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';


-- and insert another 30k rows, this time overflowing the 2^32 counter
SELECT public.test_bigint_python();


-- reset fault injection framework
SELECT gp_inject_fault('executor_run_high_processed', 'reset', dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';
SELECT COUNT(*) AS count
  FROM public.spi64bittestpython;


-- drop test table
DROP TABLE public.spi64bittestpython;
