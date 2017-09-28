CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

DROP TABLE IF EXISTS public.spi64bittest;
-- use a sequence as primary key, so we can update the data later on
CREATE TABLE public.spi64bittest (id BIGSERIAL PRIMARY KEY, data BIGINT);

-- general test case first, user test case afterwards


-- Pretend that the INSERT below inserted more than 4 billion rows in a plpgsql function
--
-- Use type 'skip', because we don't want to throw an ERROR or worse. There
-- is special handling at the code that checks for this fault, to bump up
-- the row counter regardless of the fault type.

SELECT gp_inject_fault('executor_run_high_processed', 'reset', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';


-- insert enough rows to trigger the fault injector
SELECT gp_inject_fault('executor_run_high_processed', 'skip', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';
DO $$
declare
  num_rows int8;
begin

  INSERT INTO public.spi64bittest (data)
  SELECT g
    FROM generate_series(1, 30000) g;
  GET DIAGNOSTICS num_rows = ROW_COUNT;

  RAISE NOTICE 'Inserted % rows', num_rows;
end;
$$;
SELECT gp_inject_fault('executor_run_high_processed', 'reset', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';
SELECT COUNT(*) AS count
  FROM public.spi64bittest;


-- update all rows, and trigger the fault injector
SELECT gp_inject_fault('executor_run_high_processed', 'skip', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';
DO $$
declare
  num_rows int8;
begin

  UPDATE public.spi64bittest
     SET data = data + 1;
  GET DIAGNOSTICS num_rows = ROW_COUNT;

  RAISE NOTICE 'Updated % rows', num_rows;
end;
$$;
SELECT gp_inject_fault('executor_run_high_processed', 'reset', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';
SELECT COUNT(*) AS count
  FROM public.spi64bittest;


-- delete all rows, and trigger the fault injector
SELECT gp_inject_fault('executor_run_high_processed', 'skip', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';
DO $$
declare
  num_rows int8;
begin

  DELETE FROM public.spi64bittest;
  GET DIAGNOSTICS num_rows = ROW_COUNT;

  RAISE NOTICE 'Deleted % rows', num_rows;
end;
$$;
SELECT gp_inject_fault('executor_run_high_processed', 'reset', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';
SELECT COUNT(*) AS count
  FROM public.spi64bittest;

DROP TABLE public.spi64bittest;



-- user test case

-- create a function which executes SQL statements and processes the number of touched rows
CREATE OR REPLACE FUNCTION sql_exec_stmt(sql_stmt TEXT)
    RETURNS BIGINT AS $$
DECLARE
    num_rows BIGINT;
BEGIN

    EXECUTE sql_stmt;

    GET DIAGNOSTICS num_rows := ROW_COUNT;

    RETURN (num_rows);
END
$$
LANGUAGE 'plpgsql' VOLATILE;
SELECT sql_exec_stmt('SELECT 1');


DROP TABLE IF EXISTS public.spi64bittest_2;
CREATE TABLE public.spi64bittest_2 (id BIGINT);


-- insert some data
SELECT sql_exec_stmt('INSERT INTO public.spi64bittest_2 (id) SELECT generate_series(1,5000)');

-- activate fault injector
SELECT gp_inject_fault('executor_run_high_processed', 'skip', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';

-- double the data
SELECT sql_exec_stmt('INSERT INTO public.spi64bittest_2 (id) SELECT id FROM public.spi64bittest_2');
SELECT sql_exec_stmt('INSERT INTO public.spi64bittest_2 (id) SELECT id FROM public.spi64bittest_2');
SELECT sql_exec_stmt('INSERT INTO public.spi64bittest_2 (id) SELECT id FROM public.spi64bittest_2');
SELECT sql_exec_stmt('INSERT INTO public.spi64bittest_2 (id) SELECT id FROM public.spi64bittest_2');

SELECT gp_inject_fault('executor_run_high_processed', 'reset', '', '', '', 0, 0, dbid)
  FROM pg_catalog.gp_segment_configuration
 WHERE role = 'p';
SELECT COUNT(*) AS count
  FROM public.spi64bittest_2;

DROP TABLE public.spi64bittest_2;
