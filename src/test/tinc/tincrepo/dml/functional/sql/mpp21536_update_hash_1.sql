-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on
\echo --start_ignore
set gp_enable_column_oriented_table=on;
set optimizer_enable_master_only_queries=on;
\echo --end_ignore

update zzz set a=m.b from m where m.a=zzz.b;

select * from zzz order by 1, 2;
