-- @Description Tests that deleting the same tuple twice within the same
-- commands works fine.
-- 

set enable_nestloop=false;
DELETE FROM foo USING BAR WHERE foo.a = bar.a AND (bar.a = 10 or bar.a = 40000 or bar.a = 20000);
