-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Tables using enhanced functions

-----------------------------------------
-- Create test tables which will be used for table function testing
DROP TABLE IF EXISTS t1 CASCADE;
CREATE TABLE t1 (a int, b int, c int, d int, e text)
DISTRIBUTED BY (a);

INSERT INTO t1 SELECT i, i/3, i%2, 100-i, 'text'||i 
FROM generate_series(1,100) i;

DROP TABLE IF EXISTS outtable CASCADE;
DROP TABLE IF EXISTS intable CASCADE;
DROP TABLE IF EXISTS randtable;
CREATE TABLE outtable(a text, b int) distributed randomly;
CREATE TABLE intable(id int, value text) distributed by (id);
CREATE TABLE randtable(id int, value text) distributed randomly;

INSERT INTO intable   SELECT id::int, ('value_'||id)::text FROM generate_series(1, 10) id;
INSERT INTO randtable SELECT id::int, ('value_'||id)::text FROM generate_series(1, 10) id;

DROP TABLE IF EXISTS history CASCADE;
CREATE TABLE history(id integer, time timestamp) DISTRIBUTED BY (id);
INSERT INTO history values 
(1,'2011/08/21 10:15:02am'),
(1,'2011/08/21 10:15:30am'),
(1,'2011/08/22 10:15:04am'),
(1,'2011/08/22 10:16:10am'),
(2,'2011/08/21 10:15:02am'),
(2,'2011/08/21 10:15:02am'),
(2,'2011/08/21 10:16:02am'),
(2,'2011/08/21 10:16:02am'),
(3,'2011-08-19 19:05:13'),
(3,'2011-08-19 19:06:50'),
(3,'2011-08-19 19:07:35'),
(3,'2011-08-19 19:08:18'),
(3,'2011-08-19 19:09:07'),
(3,'2011-08-20 10:07:10'),
(3,'2011-08-20 10:07:35'),
(3,'2011-08-20 10:11:29'),
(3,'2011-08-20 10:17:10'),
(3,'2011-08-20 10:17:42');

-----------------------------------------
-- Create Enhanced Table Function (ETF) using input syntax: 
-- CREATE FUNCTION tabfunc(t anytable) RETURNS ...
CREATE OR REPLACE FUNCTION transform(a anytable)
RETURNS SETOF outtable
AS '$libdir/tabfunc_demo', 'mytransform'
LANGUAGE C;

\df transform;

SELECT * FROM transform( 
    TABLE( SELECT * FROM randtable ORDER BY id, value SCATTER BY id) 
) order by b;

-- Functions can have parameters besides the anytable parameter 
-- Create Enhanced Table Function (ETF) using input syntax
-- CREATE FUNCTION tabfunc(t anytable, i integer) ...
CREATE OR REPLACE FUNCTION sessionize(anytable, interval) 
RETURNS TABLE(id integer, "time" timestamp, sessionnum integer)
AS '$libdir/tabfunc_demo', 'sessionize' 
LANGUAGE C;

SELECT *
FROM sessionize( TABLE( SELECT id, time
                        FROM history
                        ORDER BY id, time
                        SCATTER BY id ),
                  '1 minute' )
ORDER BY id, time;

SELECT *
FROM sessionize( TABLE( SELECT id, time
                        FROM history
                        ORDER BY id, time
                        SCATTER BY id ),
                  '1 hour')
ORDER BY id, time;

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/tabfunc_demo', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer) 
    RETURNS setof record
    AS '$libdir/tabfunc_demo', 'project' 
    LANGUAGE C
    WITH (describe = project_desc);

    -- Check callback function project_desc is registerred in pg_proc_callback
    select * from pg_proc_callback 
    where profnoid='project'::regproc 
    and procallback='project_desc'::regproc;

    -- Using order by and scatter by inside ETF, with order by outside ETF
    SELECT * FROM project( 
        TABLE( SELECT * FROM history order by id scatter by id), 1) 
    order by 1;

    -- Using distinct outside ETF, scatter by multiple columns inside ETF
    SELECT distinct id FROM project( 
        TABLE( SELECT id FROM history order by id scatter by id, time), 1) 
    order by 1 desc;

    -- Using distinct filter inside ETF, and filter outside ETF
    SELECT time FROM project( 
        TABLE( SELECT distinct * FROM history scatter by id), 2) 
    where time <'2011-08-20' order by 1;

