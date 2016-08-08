-- @Description Tests that out-of-order update works fine.
-- 

set enable_nestloop=false;
UPDATE foo SET b = 0 FROM bar WHERE foo.a = bar.a AND (bar.a = 10 or bar.a = 40000 or bar.a = 20000);
