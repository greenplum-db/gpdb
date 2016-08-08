-- @Description Tests invalid schema creation from function with exception
-- 

DROP TABLE IF EXISTS test_exception_block CASCADE;
DROP FUNCTION IF EXISTS test_exception_block_fn() CASCADE;

-- create base table
CREATE TABLE test_exception_block
(
  f1 smallint,
  f2 smallint,
  f3 smallint
)
WITH ( OIDS=FALSE ) DISTRIBUTED BY (f1);

-- create function execution of which post fix avoids catalog corruption
-- without the fix, VIEW v1 used to get commited on segment and not on master
CREATE OR REPLACE FUNCTION test_exception_block_fn() RETURNS integer AS
$BODY$
	begin
		-- this view definition is valid
		CREATE VIEW public.test_exception_block_v1 AS SELECT f1 FROM public.test_exception_block;
		-- this view definition is invalid
		CREATE VIEW public.test_exception_block_v2 AS SELECT f4 FROM public.test_exception_block;
		return 1;
	exception
		WHEN OTHERS THEN
		RAISE NOTICE 'EXCEPTION HIT !!!';
	        return 0;
		RAISE EXCEPTION 'ERROR 0';
	end;
$BODY$
LANGUAGE plpgsql VOLATILE SECURITY DEFINER;

--execute the function
SELECT * FROM test_exception_block_fn();

SELECT relname FROM pg_class WHERE relname IN ('test_exception_block_v1','test_exception_block_v2');
-- no record returned i.e. v1 does not exist in the catalog

--let's try and create v1
CREATE VIEW public.test_exception_block_v1 AS SELECT f1 FROM public.test_exception_block;
  -- Should not thow ERROR:  relation "test_exception_block_v1" already exists

DROP VIEW public.test_exception_block_v1;

-- Function call from savepoint which commits
BEGIN;
	SAVEPOINT SP1;
	SELECT * FROM test_exception_block_fn();
	RELEASE SP1;
COMMIT;

SELECT relname FROM pg_class WHERE relname IN ('test_exception_block_v1','test_exception_block_v2');
-- no record returned i.e. v1 does not exist in the catalog

--let's try and create v1
CREATE VIEW public.test_exception_block_v1 AS SELECT f1 FROM public.test_exception_block;
  -- Should not thow ERROR:  relation "test_exception_block_v1" already exists

DROP VIEW public.test_exception_block_v1;


--  create function execution of which post fix avoids catalog corruption
CREATE OR REPLACE FUNCTION test_exception_block_fn() RETURNS integer AS
$BODY$
	begin
		-- this view definition is valid
		CREATE VIEW public.test_exception_block_v1 AS SELECT f1 FROM public.test_exception_block;
		return 1;
	exception
		WHEN OTHERS THEN
		RAISE NOTICE 'EXCEPTION HIT !!!';
	        return 0;
		RAISE EXCEPTION 'ERROR 0';
	end;
$BODY$
LANGUAGE plpgsql VOLATILE SECURITY DEFINER;


-- Function call from savepoint which aborts
BEGIN;
	SAVEPOINT SP1;
	SELECT * FROM test_exception_block_fn();
	ROLLBACK TO SP1;
COMMIT;

SELECT relname FROM pg_class WHERE relname IN ('test_exception_block_v1','test_exception_block_v2');
-- no record returned i.e. v1 does not exist in the catalog

--let's try and create v1
CREATE VIEW public.test_exception_block_v1 AS SELECT f1 FROM public.test_exception_block;
  -- Should not thow ERROR:  relation "test_exception_block_v1" already exists

