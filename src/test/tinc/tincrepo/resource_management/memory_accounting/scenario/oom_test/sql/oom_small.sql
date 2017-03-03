-- @author ramans2
-- @created 2014-03-27 12:00:00
-- @modified 2014-03-27 12:00:00
-- @gpdiff False
-- @description Simple queries that do not OOM

select count(*) from partsupp;
select pg_sleep(10);
