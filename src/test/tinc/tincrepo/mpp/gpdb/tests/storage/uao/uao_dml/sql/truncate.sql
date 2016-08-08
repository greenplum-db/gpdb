-- @Description Checks the behavior of TRUNCATE.
-- Note that the contents of the visimap is not checked here.
-- 

SELECT COUNT(*) FROM foo;
DELETE FROM foo;
SELECT COUNT(*) FROM foo;
TRUNCATE foo;
SELECT COUNT(*) FROM foo;
INSERT INTO foo SELECT i as a, i as b, 'hello world' as c FROM generate_series(1,10) AS i;
SELECT COUNT(*) FROM foo;
