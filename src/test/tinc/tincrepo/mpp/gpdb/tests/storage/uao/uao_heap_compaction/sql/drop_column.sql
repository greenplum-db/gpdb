-- @Description Tests dropping a column after a compaction
-- 

DELETE FROM foo WHERE a < 4;
SELECT COUNT(*) FROM foo;
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo';
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo_index';
VACUUM foo;
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo';
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo_index';
ALTER TABLE foo DROP COLUMN c;
SELECT * FROM foo;
INSERT INTO foo VALUES (42, 42);
SELECT * FROM foo;
