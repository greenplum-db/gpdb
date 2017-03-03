-- @description select_fromtable_gp_update_ao_master_stats.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode ORCA_PLANNER_DIFF
-- @tags functionPropertiesBuiltin HAWQ
SELECT gp_update_ao_master_stats('ao_tab2') FROM foo
