-- @description varying_join_subqry_now.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count(distinct i) FROM foo, (SELECT now() i) t;
