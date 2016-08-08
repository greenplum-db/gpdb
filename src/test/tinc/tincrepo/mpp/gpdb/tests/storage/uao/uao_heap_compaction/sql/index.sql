-- @Description Tests basic index usage behavior after vacuuming
-- 

VACUUM foo;
SELECT * FROM foo WHERE b = 5;
