-- @description subqry_get_ao_compression_ratio.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode ORCA_PLANNER_DIFF
-- @tags functionPropertiesBuiltin HAWQ
SELECT * FROM bar, (SELECT get_ao_compression_ratio('ao_tab2') ORDER BY 1) r ORDER BY 1,2,3;
