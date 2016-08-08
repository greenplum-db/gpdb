-- 
-- @description test sql test case: basic
-- @created 2012-07-10 12:00:00
-- @modified 2012-07-10 12:00:02
-- @tags basic
-- @product_version gpdb: main
-- @dbname gptest


select 'foo', i from generate_series(1,100) i;
