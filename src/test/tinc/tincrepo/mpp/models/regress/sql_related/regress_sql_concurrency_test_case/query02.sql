-- 
-- @description test sql test case
-- @created 2012-07-05 12:00:00
-- @modified 2012-07-08 12:00:02
-- @tags orca hashagg executor
-- @product_version gpdb: main
-- @iterations 2
-- @concurrency 2

select 'foo';

select pg_sleep(2);
