-- @author prabhd
-- @created 2014-03-10 12:00:00
-- @modified 2014-03-10 12:00:00
-- @tags dml MPP-21622
-- @db_name dmldb
-- @product_version gpdb: [4.3.0.0-] 
-- @optimizer_mode on
-- @description MPP-21622 Update with primary key: only sort if the primary key is updated
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

--start_ignore
explain update r set b = 5;
--end_ignore
update r set b = 5;
select * from r order by 1,2;

--start_ignore
explain update r set a = 5;
--end_ignore
update r set a = 5;
select * from r order by 1,2;
