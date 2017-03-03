-- @author ramans2
-- @created 2014-02-05 12:00:00
-- @modified 2014-02-05 12:00:00
-- @description Simple queries that do not OOM

select count(*) from partsupp;
select pg_sleep(60);
