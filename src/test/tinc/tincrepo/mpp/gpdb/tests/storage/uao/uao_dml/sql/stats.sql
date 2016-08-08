-- @Description Tests that pg_class statistics are up-to-date when deleting and updating.
-- 

SELECT relname, reltuples FROM pg_class WHERE relname = 'foo';
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo_index';
DELETE FROM foo WHERE a < 100;
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo';
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo_index';
UPDATE foo SET b=42 WHERE a < 200;
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo';
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo_index';
set enable_seqscan=false;
SELECT COUNT(*) FROM foo WHERE b=42;
