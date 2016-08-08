-- @Description Tests exception raised on master instead of segment in function with exception
-- 

DROP TABLE IF EXISTS test_exception_block CASCADE;
DROP FUNCTION IF EXISTS test_exception_block_fn() CASCADE;

SELECT relname FROM pg_class WHERE relname = 'test_exception_block';

SELECT relname FROM gp_dist_random('pg_class') WHERE relname = 'test_exception_block';

CREATE OR REPLACE FUNCTION test_exception_block_fn() RETURNS VOID AS
$$
BEGIN
     CREATE TABLE test_exception_block(a int) DISTRIBUTED BY (a);
     VACUUM pg_type;
EXCEPTION WHEN OTHERS THEN
RAISE NOTICE '%', SQLSTATE;
end;
$$ LANGUAGE plpgsql;

SELECT test_exception_block_fn();

SELECT relname FROM pg_class WHERE relname = 'test_exception_block';

SELECT relname FROM gp_dist_random('pg_class') WHERE relname = 'test_exception_block';

-- Test explicit function call inside SAVEPOINT (sub-transaction)
BEGIN;
	SAVEPOINT SP1;
	SELECT test_exception_block_fn();
	ROLLBACK to SP1;
COMMIT;

SELECT relname FROM pg_class WHERE relname = 'test_exception_block';

SELECT relname FROM gp_dist_random('pg_class') WHERE relname = 'test_exception_block';

-- Test explicit function call inside SAVEPOINT (sub-transaction)
BEGIN;
	SAVEPOINT SP1;
	SELECT test_exception_block_fn();
	RELEASE SP1;
COMMIT;

SELECT relname FROM pg_class WHERE relname = 'test_exception_block';

SELECT relname FROM gp_dist_random('pg_class') WHERE relname = 'test_exception_block';


