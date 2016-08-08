-- @Description Test the basic bahavior of vacuum full
-- 

DELETE FROM foo WHERE a < 4;
SELECT COUNT(*) FROM foo;
VACUUM FULL foo;
-- check if we get the same result afterwards
SELECT COUNT(*) FROM foo;
-- check if we can still insert into the relation
INSERT INTO foo VALUES (11, 11);
SELECT COUNT(*) FROM foo;
