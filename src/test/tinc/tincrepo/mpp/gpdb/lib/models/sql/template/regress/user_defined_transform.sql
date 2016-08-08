-- 
-- @description test template sql test
-- @created 2012-07-23 12:00:00
-- @modified 2012-07-23 12:00:02
-- @tags basic
select pg_sleep(%sleep_interval%);
-- Pass in pg_default
select rsqname,rsqcountlimit,rsqcountvalue,rsqmemorylimit,rsqmemoryvalue,rsqwaiters,rsqholders from gp_toolkit.gp_resqueue_status where rsqname='%resource_queue%';
