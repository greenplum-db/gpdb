-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @db_name world_db
-- @description Mpp-19991

with v1 as (select * from x), v2 as (select * from y) select * from v1;
