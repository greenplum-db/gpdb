-- @Description Tests basic delete feature with two AO blocks
-- 

SELECT COUNT(*) FROM foo;
DELETE FROM foo WHERE a < 4;
SELECT COUNT(*) FROM foo;
INSERT INTO foo SELECT i as a, i as b, 'hello world' as c FROM generate_series(11,20) AS i;
SELECT COUNT(*) FROM foo;
DELETE FROM foo WHERE a = 16;
DELETE FROM foo WHERE a = 17;
SELECT a FROM foo WHERE a > 10;
