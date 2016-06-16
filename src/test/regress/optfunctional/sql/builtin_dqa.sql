-- start_ignore
create schema builtin_dqa;
set search_path to builtin_dqa;
CREATE TABLE foo (a text, b int, c text, d text, e text);
INSERT INTO foo VALUES ('foo', 12000, 'pg_default', 'ao_tab1', 'testdb');
INSERT INTO foo VALUES ('bar', 4045, 'pg_default', 'ao_tab2', 'testdb');
INSERT INTO foo VALUES ('ao_tab1', 573, 'pg_default', 'ao_tab1', 'testdb');
INSERT INTO foo VALUES ('ao_tab2', 25730, 'pg_default', 'ao_tab2', 'testdb');
-- end_ignore

-- @description varying_with_generate_series_clock_timestamp.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode ORCA_PLANNER_DIFF
-- @tags functionPropertiesBuiltin HAWQ
SELECT count( distinct i ) FROM (SELECT clock_timestamp() i, generate_series(1, 5) j) t;
-- @description varying_with_generate_series_random.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode ORCA_PLANNER_DIFF
-- @tags functionPropertiesBuiltin HAWQ
SELECT count( distinct i ) FROM (SELECT random() i, generate_series(1, 5) j) t;
-- @description varying_with_generate_series_timeofday.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode ORCA_PLANNER_DIFF
-- @tags functionPropertiesBuiltin HAWQ
SELECT count( distinct i ) FROM (SELECT timeofday() i, generate_series(1, 5) j) t;
-- @description varying_fromtable_clock_timestamp.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode gt1
-- @tags functionPropertiesBuiltin HAWQ
SELECT count( distinct i ) > 1 AS gt1 FROM (SELECT clock_timestamp() i from foo) t;
-- @description varying_fromtable_random.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode gt1
-- @tags functionPropertiesBuiltin HAWQ
SELECT count( distinct i ) > 1 AS gt1 FROM (SELECT random() i from foo) t;
-- @description varying_fromtable_timeofday.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode gt1
-- @tags functionPropertiesBuiltin HAWQ
SELECT count( distinct i ) > 1 AS gt1 FROM (SELECT timeofday() i from foo) t;

-- start_ignore
drop schema builtin_dqa cascade;
-- end_ignore
