-- @description function_in_subqry_column_withfunc2_50.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_nosql_vol(a)) from foo) r order by 1,2,3; 
