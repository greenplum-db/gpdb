-- @description subqry_in_pg_total_relation_size.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode ORCA_PLANNER_DIFF
-- @tags functionPropertiesBuiltin
SELECT * FROM bar WHERE x IN (SELECT * FROM pg_total_relation_size('bar'));
