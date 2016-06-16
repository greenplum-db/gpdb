
-- start_ignore
create schema builtin_normal;
set search_path to builtin_normal;
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a text, b int, c text, d text, e text);
INSERT INTO foo VALUES ('foo', 12000, 'pg_default', 'ao_tab1', 'testdb');
INSERT INTO foo VALUES ('bar', 4045, 'pg_default', 'ao_tab2', 'testdb');
INSERT INTO foo VALUES ('ao_tab1', 573, 'pg_default', 'ao_tab1', 'testdb');
INSERT INTO foo VALUES ('ao_tab2', 25730, 'pg_default', 'ao_tab2', 'testdb');

CREATE TABLE bar (x int, y int);
INSERT INTO bar SELECT i, i+1 FROM generate_series(1,10) i;

CREATE TABLE ao_tab1 (r int, s int) WITH (appendonly=true, compresslevel=6);
INSERT INTO ao_tab1 SELECT i, i+1 FROM generate_series(1,20) i;

CREATE TABLE ao_tab2 (r int, s int) WITH (appendonly=true);
INSERT INTO ao_tab2 SELECT i, i*100 FROM generate_series(1,200) i;


-- end_ignore
-- @description varying_fromtable_now.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count( distinct i ) FROM (SELECT now() i from foo) t;
-- @description varying_join_clock_timestamp.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count(distinct i ) FROM clock_timestamp() i, foo j;
-- @description varying_join_now.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count(distinct i ) FROM now() i, foo j;
-- @description varying_join_random.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count(distinct i ) FROM random() i, foo j;
-- @description varying_join_subqry_clock_timestamp.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count(distinct i) FROM foo, (SELECT clock_timestamp() i) t;
-- @description varying_join_subqry_now.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count(distinct i) FROM foo, (SELECT now() i) t;
-- @description varying_join_subqry_random.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count(distinct i) FROM foo, (SELECT random() i) t;
-- @description varying_join_subqry_timeofday.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count(distinct i) FROM foo, (SELECT timeofday() i) t;
-- @description varying_join_timeofday.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count(distinct i ) FROM timeofday() i, foo j;
-- @description varying_with_generate_series_now.sql
-- @db_name builtin_functionproperty
-- @author tungs1
-- @modified 2013-04-17 12:00:00
-- @created 2013-04-17 12:00:00
-- @executemode NORMAL
-- @tags functionPropertiesBuiltin HAWQ
SELECT count( distinct i ) FROM (SELECT now() i, generate_series(1, 5) j) t;

-- start_ignore
drop schema builtin_normal cascade;
-- end_ignore
