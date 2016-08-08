-- @Description Tests that deleting the same tuple twice within the same
-- commands works fine.
-- 

SELECT * FROM foo;
DELETE FROM foo USING bar WHERE foo.a = bar.a;
SELECT * FROM foo;
