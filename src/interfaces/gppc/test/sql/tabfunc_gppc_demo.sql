set timezone=UTC;
-- Create test tables which will be used for table function testing
DROP TABLE IF EXISTS t1 CASCADE;
CREATE TABLE t1 (a int, b int, c int, d int, e text)
DISTRIBUTED BY (a);

INSERT INTO t1 SELECT i, i/3, i%2, 100-i, 'text'||i 
FROM generate_series(1,100) i;

select count(*) from t1;

DROP FUNCTION IF EXISTS sessionize(anytable, interval);
DROP FUNCTION IF EXISTS ud_project(anytable, text);
DROP FUNCTION IF EXISTS ud_project(anytable);
DROP FUNCTION IF EXISTS ud_project2(anytable, text);
DROP FUNCTION IF EXISTS ud_project2(anytable);
DROP FUNCTION IF EXISTS ud_describe(internal);
DROP FUNCTION IF EXISTS ud_describe2(internal);

DROP TYPE IF EXISTS outcomp CASCADE;
DROP TABLE IF EXISTS outtable CASCADE;
DROP TABLE IF EXISTS intable CASCADE;
DROP TABLE IF EXISTS randtable;

CREATE TABLE intable(id int, value text) distributed by (id);
CREATE TABLE outtable(a text, b int) distributed randomly;
CREATE TABLE randtable(id int, value text) distributed randomly;

INSERT INTO intable   SELECT id::int, ('value_'||id)::text FROM generate_series(1, 10) id;
INSERT INTO randtable SELECT id::int, ('value_'||id)::text FROM generate_series(1, 10) id;

\d intable
\d outtable
\d randtable

select * from intable order by id,value;

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

SELECT * FROM history order  by id, time;
    -- Create Enhanced Table Function (ETF) using input syntax: 
    -- CREATE FUNCTION tabfunc(t anytable) RETURNS ...

    -- MPP-16614, the SELECT query would fail
    DROP FUNCTION IF EXISTS transform(anytable);
    CREATE OR REPLACE FUNCTION transform(a anytable)
    RETURNS SETOF outtable
    AS '$libdir/gppc_test', 'mytransform'
    LANGUAGE C;

    \df transform

    SELECT * FROM transform( 
        TABLE( SELECT * FROM randtable ORDER BY id, value SCATTER BY id) 
    ) order by b;

    -- The 1st workaround of MPP-16614
    DROP FUNCTION IF EXISTS transform(anytable);
    CREATE OR REPLACE FUNCTION transform(a anytable)
    RETURNS SETOF RECORD
    AS '$libdir/gppc_test', 'mytransform'
    LANGUAGE C;

    SELECT * FROM transform( 
        TABLE( SELECT * FROM randtable ORDER BY id, value SCATTER BY id) 
    )  AS t (a text, b int) order by b;

    -- The 2nd workaround of MPP-16614
    DROP FUNCTION IF EXISTS transform(anytable);
    CREATE OR REPLACE FUNCTION transform(a anytable)
    RETURNS TABLE (a text, b int)
    AS '$libdir/gppc_test', 'mytransform'
    LANGUAGE C;

    SELECT * FROM transform(
        TABLE( SELECT * FROM randtable ORDER BY id, value SCATTER BY id)
    ) order by b;

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer) 
    RETURNS setof record
    AS '$libdir/gppc_test', 'project' 
    LANGUAGE C
    WITH (describe = project_desc);

    -- Check callback function project_desc is registerred in pg_proc_callback
    select * from pg_proc_callback 
    where profnoid='project'::regproc 
    and procallback='project_desc'::regproc;

    -- ETF can only be created when using anytable as input type. 
    -- Negative: CREATE FUNCTION tabfunc_bad1 (x SETOF targettable) RETURNS ...
    CREATE OR REPLACE FUNCTION transform3(a SETOF intable)
      RETURNS SETOF outtable
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

    -- Negative: CREATE FUNCTION tabfunc_bad2 (x TABLE(a int) ) RETURNS ...
    CREATE OR REPLACE FUNCTION transform3(x TABLE(a int, b text))
      RETURNS SETOF outtable
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;
    -- Create ETF using TABLE return syntax.
    DROP FUNCTION IF EXISTS transform(anytable);
    CREATE OR REPLACE FUNCTION transform(a anytable)
    RETURNS TABLE (a text, b int)
    AS '$libdir/gppc_test', 'mytransform'
    LANGUAGE C;

SELECT * FROM transform( 
    TABLE( SELECT * FROM randtable ORDER BY id, value SCATTER BY id) 
) order by b;

    -- create ETF with output table "hai10" does not exist at the moment
    CREATE OR REPLACE FUNCTION transform_new(a anytable)
      RETURNS SETOF hai10
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

    \df transform_new

    drop function if exists transform_new(a anytable);
-- Create ETF using return syntax: TABLE LIKE table, however the "liked" table is non-existing
    DROP FUNCTION IF EXISTS transform(anytable);
    CREATE OR REPLACE FUNCTION transform (a anytable)
      RETURNS TABLE (LIKE outtable_nonexist)
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

-- Create ETF using return syntax: TABLE LIKE table
    CREATE OR REPLACE FUNCTION transform(a anytable)
      RETURNS TABLE (LIKE outtable)
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

-- Verify ETF can be called successfully
select * from transform(TABLE(select * from intable where id<3));

-- Add a new column to output table, then drop the new added column.
alter table outtable add column newcol int;
alter table outtable drop column newcol;
\d outtable;

-- Calling ETF again, get ERROR: invalid output tuple
-- MPP-14231
select * from transform(TABLE(select * from intable where id<3));

-- Recreate outtable
DROP TABLE IF EXISTS outtable cascade;
CREATE TABLE outtable(a text, b int) distributed randomly;

-- Recreate transform function
CREATE OR REPLACE FUNCTION transform (a anytable)
      RETURNS TABLE (a text, b int)
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;
-- Create ETF using return syntax: TABLE (argname argtype).
--    CREATE FUNCTION tabfunc2(t anytable) 
--    RETURNS TABLE (x int, y text) ...
    CREATE OR REPLACE FUNCTION transform3(a anytable)
      RETURNS TABLE (b text, a int)
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

    select * from transform3(TABLE(select * from intable where id<3));
-- Currently ETF can take one only one anytable type input.
    CREATE OR REPLACE FUNCTION transform3(a anytable, b anytable)
      RETURNS TABLE (b text, a int)
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;
-- Missing SETOF keyword in RETURNS
-- Note this is stll valid.
-- The result is one row of record is returned per each segment.
-- For a cluster of 2 segments, total 2 rows will be returned.
DROP FUNCTION IF EXISTS transform_outtable(anytable);
CREATE OR REPLACE FUNCTION transform_outtable(a anytable)
RETURNS TABLE (a text, b int)
AS '$libdir/gppc_test', 'mytransform'
LANGUAGE C;

select * from transform_outtable(TABLE(select * from intable)) order by b;

drop function if exists transform_outtable(anytable);

-- Missing TABLE keyword in RETURNS
CREATE OR REPLACE FUNCTION transform(a anytable)
RETURNS (a text, b int)
AS '$libdir/gppc_test', 'mytransform'
LANGUAGE C;

-- Negative: create ETF with distribution and/or ordering defined at function create time.
-- The followings should not be allowed.

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      scatter randomly
      RETURNS SETOF outtable
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      RETURNS SETOF outtable
      scatter randomly
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      order by a
      RETURNS SETOF outtable
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      RETURNS SETOF outtable
      order by a
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      partition by a
      RETURNS SETOF outtable
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      RETURNS SETOF outtable
      partition by a
      AS '$libdir/gppc_test', 'mytransform'
      LANGUAGE C;

-- TABLE value expression can only be used inside ETF call
-- Cannot be used in any other places.

    -- TVE after select
    SELECT TABLE(select a from t1);
    -- ERROR:  invalid use of TABLE value expression

    -- TVE after select
    SELECT TABLE(select a from t1) + 1;
    -- ERROR:  operator does not exist: anytable + integer

    -- TVE in FROM clause
    SELECT * FROM TABLE(select a from t1);
    -- ERROR:  syntax error at or near "TABLE"

    -- ETF in IN clause
    SELECT * FROM t1 WHERE a IN ( 
        transform( TABLE(select a,e from t1 where a < 5) )
    );
    -- ERROR:  operator does not exist: integer = outtable

    -- TVE in IN clause
    SELECT * FROM t1 WHERE a IN ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  operator does not exist: integer = anytable

    -- TVE in  NOT IN clause
    SELECT * FROM t1 WHERE a NOT IN ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  operator does not exist: integer <> anytable

    -- TVE in EXIST clause
    SELECT * FROM t1 WHERE EXISTS ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  syntax error at or near "TABLE"

    -- TVE in NOT EXIST clause
    SELECT * FROM t1 WHERE NOT EXISTS ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  syntax error at or near "TABLE"

    -- TVE in ANY/SOME clause
    SELECT * FROM t1 WHERE a < ANY ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  op ANY/ALL (array) requires array on right side

    -- TVE in ALL clause
    SELECT * FROM t1 WHERE a > ALL ( 
        TABLE(select a from t1 where a < 5) 
    );
    -- ERROR:  op ANY/ALL (array) requires array on right side

    -- TVE in GROUP BY clause
    SELECT avg(a) FROM t1 GROUP BY TABLE(select c from t1);
    -- ERROR:  could not identify an ordering operator for type anytable

    -- TVE in HAVING clause
    SELECT avg(a) FROM t1 GROUP BY c HAVING TABLE(select 51) > 50;
    -- ERROR:  operator does not exist: anytable > integer

    -- TVE in window function
    select a, c, TABLE (SELECT a from t1) over(partition by c) from t1 where a < 10;
    -- ERROR:  syntax error at or near "over"

    -- TVE in window function
    select a, c, avg(a) over(partition by TABLE (SELECT a from t1)) from t1 where a < 10;
    -- ERROR:  could not identify an ordering operator for type anytable

    -- TVE in ORDER BY clause
    SELECT a FROM t1 ORDER BY TABLE(select a from t1);
    -- ERROR:  could not identify an ordering operator for type anytable

    -- TVE in LIMIT clause
    SELECT a FROM t1 LIMIT TABLE(select 4);
    -- ERROR:  argument of LIMIT must be type bigint, not type anytable

    -- nested TABLE() expression
    SELECT a FROM TABLE(TABLE(select 4,'haha'::text));
    -- ERROR:  syntax error at or near "TABLE"
-- TABLE() value expressions can never be involved in an expression. 
    SELECT TABLE(select a from t1) + 1;
    -- ERROR:  operator does not exist: anytable + integer

    SELECT * FROM t1 WHERE a IN ( 
        TABLE(select a from t1 where a < 5) * 2
    );
    -- ERROR:  operator does not exist: anytable * integer
-- Negative: test cases of using anytable as output of function in create time
    -- Create output table outComp
    drop table if exists outComp cascade;
    create table outComp (b1 int, b2 text);

    -- Create a non-enhanced table function
    CREATE OR REPLACE FUNCTION tf_int2char(max integer) 
    RETURNS SETOF outComp AS $$
    DECLARE f outComp%ROWTYPE;
    BEGIN
      FOR i IN 1..max 
      LOOP
        f.b1 := CAST(i AS varchar(10));
        f.b2 := 'tf_test '||CAST(i AS varchar(10));
        RETURN NEXT f;
      END LOOP;
      RETURN;
    END;
    $$ LANGUAGE plpgsql;
    -- This should succeed and function can be created successfully.

    -- Using **anytable** as return type
    CREATE OR REPLACE FUNCTION tf_int2char_bad1(max integer) 
    RETURNS anytable AS $$
    DECLARE f outComp%ROWTYPE;
    BEGIN
      FOR i IN 1..max 
      LOOP
        f.b1 := CAST(i AS varchar(10));
        f.b2 := 'tf_test '||CAST(i AS varchar(10));
        RETURN NEXT f;
      END LOOP;
      RETURN;
    END;
    $$ LANGUAGE plpgsql;
    -- ERROR:  PL/pgSQL functions cannot return type anytable

    -- Using **anytable** as OUT parameter
    CREATE OR REPLACE FUNCTION tf_int2char_bad2(IN max integer, OUT a anytable) 
    AS $$
    DECLARE f outComp%ROWTYPE;
    BEGIN
      FOR i IN 1..max 
      LOOP
        f.b1 := CAST(i AS varchar(10));
        f.b2 := 'tf_test '||CAST(i AS varchar(10));
        RETURN NEXT f;
      END LOOP;
      RETURN;
    END;
    $$ LANGUAGE plpgsql;
    -- ERROR:  functions cannot return "anytable" arguments

    -- Using **anytable** as INOUT parameter
    CREATE OR REPLACE FUNCTION tf_int2char_bad3(INOUT a anytable) 
    AS $$
    DECLARE f outComp%ROWTYPE;
    BEGIN
      FOR i IN 1..max 
      LOOP
        f.b1 := CAST(i AS varchar(10));
        f.b2 := 'tf_test '||CAST(i AS varchar(10));
        RETURN NEXT f;
      END LOOP;
      RETURN;
    END;
    $$ LANGUAGE plpgsql;
    -- ERROR:  functions cannot return "anytable" arguments

    -- Negative: can't pass anytable as prepare argument
    PREPARE neg_p(anytable) AS SELECT * FROM transform( 
    TABLE(SELECT * FROM intable ));
    -- ERROR:  type "anytable" is not a valid parameter for PREPARE

    drop function tf_int2char(max integer);
    drop table if exists outComp cascade;
-- Negative: using anytable as general data type should fail
-- NOTICE:  Table doesn't have 'DISTRIBUTED BY' clause, 
--          and no column type is suitable for a distribution key. 
--          Creating a NULL policy entry.
    CREATE TABLE tmpTable1 (a anytable);

    select * from transform( TABLE(
        select * from intable 
        order by value 
        scatter by id) ) 
    order by b limit 2;

    -- Verify TVE inside ETF can be casted
    -- to anytable type and query still works fine
    select * from transform( TABLE(
        select * from intable 
        order by value 
        scatter by id)::anytable) 
    order by b limit 2;

    -- Verify TVE inside ETF cannot be casted
    -- to any type (such as anyelement) other than anytable
    select * from transform( TABLE(
        select * from intable 
        order by value 
        scatter by id)::anyelement) 
    order by b limit 2;

    select * from transform( TABLE(
        select * from intable 
        order by value 
        scatter by id)::anyarray) 
    order by b limit 2;

    -- Verify array_append(anyarray, anyelement)
    select array_append(array['value1','value2']::anyarray, 'value3'::anyelement);

    -- Verify anyarray cannot be casted to anytable
    select array_append(array['value1','value2']::anytable, 'value3');

    -- Verify anyelement cannot be casted to anytable
    select array_append(array['value1','value2'], 'value3'::anytable);

    -- Verify pseudo types anytable and anyelement cannot be used for PREPARE
    PREPARE p1(anyelement) AS SELECT $1;
    -- ERROR:  type "anyelement" is not a valid parameter for PREPARE

    PREPARE p2(anytable) AS SELECT $1;
    -- ERROR:  type "anytable" is not a valid parameter for PREPARE
-- Negative: using SCATTER BY outside of sub-query of ETF call.
-- The followings should fail
    SELECT * FROM transform( TABLE(select * from intable) scatter randomly );

    SELECT * FROM transform( TABLE(select * from intable) ) scatter randomly;

    SELECT * FROM transform( TABLE(select * from intable) scatter by a );

    SELECT * FROM transform( TABLE(select * from intable) ) scatter by a, b;

    SELECT * FROM transform( TABLE(select * from intable) distributed randomly );

    SELECT * FROM transform( TABLE(select * from intable) ) distributed randomly;

    SELECT * FROM transform( TABLE(select * from intable) distributed by (a) );

    SELECT * FROM transform( TABLE(select * from intable) ) distributed by (a,b);

    SELECT id,value FROM intable scatter by id;

    SELECT id,value FROM intable scatter by (id);

    SELECT id,value FROM intable scatter randomly;

    SELECT id,value FROM intable scatter by id,value;

    SELECT id,value FROM intable order by id scatter by id;

    SELECT id,value FROM intable where id < 5 and scatter by id;

    SELECT avg(a) FROM t1 scatter by c;

    SELECT avg(a) FROM t1 group by c scatter by c;

    SELECT avg(a) FROM t1 group by c having avg(a)>50 scatter by c;

    SELECT a, c, avg(a) over(scatter by c scatter by c) FROM t1 where a <10;

    SELECT a, c, avg(a) over(partition by c scatter by c) FROM t1 where a <10;
-- Negative: using SCATTER BY in create table DML
-- The following should fail

    CREATE TABLE scatter_test1 (a int, b int)
    SCATTER BY a;

    CREATE TABLE scatter_test1 (a int, b int)
    SCATTER BY a,b;

    CREATE TABLE scatter_test1 (a int, b int)
    SCATTER BY (a);

    CREATE TABLE scatter_test1 (a int, b int)
    SCATTER BY (a,b);


-- The input of ETF can only be TABLE value expression,
-- which only takes a subquery as an input
    SELECT * FROM transform( TABLE(select * from intable) );

    SELECT * FROM transform( TABLE(select distinct b,c from t1) );

    -- CONSTANT input
    SELECT * FROM transform( TABLE( SELECT 2 as id,'haha'::text as value FROM intable) );

    SELECT * FROM transform( TABLE(select a,e from t1 scatter by a) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 scatter by a, d) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 scatter by a+d) ) order by b;
    -- a: will be 1, 2, 3 ... 100
    -- d: will be 100, 99, 98 ... 1

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 scatter randomly) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a desc, b) ) order by a,b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a + d) ) order by b;
    -- a: will be 1, 2, 3 ... 100
    -- d: will be 100, 99, 98 ... 1

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a::text || b::text ) ) order by b desc;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a asc, b desc) )
    order by a desc, b asc;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a scatter by a) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 order by a scatter by e) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10 
                           order by a,e scatter by a,e) ) order by b;

    SELECT * FROM transform( TABLE(select b, e from t1  where a<=10 
                           order by a scatter by a) ) order by b,a;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10
                           order by a scatter by b,c,d,e) ) order by b;

    SELECT * FROM transform( TABLE(select a,e from t1 where a<=10
                           order by b,c,d,e scatter by b,c,d,e) ) order by b;
-- Test query using ETF with filters
-- MPP-14250
    select a,b from transform( TABLE(
        select id,value from intable 
            where id<8 )) 
    where b <3 order by b;

-- ETF call returns empty result set

    -- ETF sub-query on an empty table: intable2
    create table intable2 (id int, value text) distributed by (id);
    SELECT * from transform( TABLE(select * from intable2) );

    -- ETF sub-query input returns 0 row via filter
    SELECT * from transform( TABLE(select a, e from t1 where a > 1000) );

    SELECT * from transform( TABLE(select a, e from t1 where a is null) );

    SELECT * from transform( TABLE(select a, e from t1 where a::text = e) );

    -- Also checking outer layer 
    SELECT * from transform( TABLE(select a, e from t1 where a > 10) )
    where a < 10::text;

    drop table intable2;
-- ETF call returns duplicate rows

    -- Have source table t1 contain some duplicated rows.

    INSERT INTO t1 SELECT i/100*100, i/100*100, i/100*100, i/100*100, 'text' 
    FROM generate_series(101,110) i;

    SELECT * FROM transform( TABLE (select a,e from t1 where a = 100 order by b scatter by a) ) order by b,a;

    DELETE FROM t1 where e='text';
-- ETF call returns rows contain null values in column e
-- Check scatter by null value column
-- Check order by null value column

-- Have source table t1 contain some null values

    INSERT INTO t1 SELECT i/200*200, i/200*200, i/200*200, i/200*200, null 
    FROM generate_series(200,210) i;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200) ) order by b,a;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200 
                                  order by a scatter by a ) ) order by b,a;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200 
                                  scatter by e) ) order by b,a;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200 
                                  order by e) ) order by b,a;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200 
                                  order by e scatter by a ) ) order by b,a;

    SELECT * FROM transform( TABLE (select a, e from t1 where a=200 
                                  order by e scatter by e ) ) order by b,a;

    DELETE FROM t1 WHERE e is null;
-- ETF call returns rows contain null values in all columns

-- Have source table t1 contain some rows with all null values
INSERT INTO t1 SELECT null, null, null, null, null 
FROM generate_series(200,210) i;

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null) );

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null 
                                  order by a scatter by a) );

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null 
                                  scatter by e) );

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null 
                                  order by e) );

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null  
                                  order by e scatter by a) );

    SELECT * FROM transform( TABLE (select a, e from t1 where a is null  
                                  order by e scatter by e) );

    DELETE FROM t1 where a is null;
-- ETF sub-query using view
    DROP VIEW IF EXISTS t1_view;
    CREATE VIEW t1_view as (
        SELECT a, b, c, d ,e from t1 
        WHERE a <10 ORDER BY d);

    SELECT * FROM transform( TABLE(select a, e from t1_view) ) ORDER BY b;

    SELECT * FROM transform( TABLE(select a, e from t1_view 
                           order by b scatter by a) ) ORDER BY b;

    SELECT * FROM transform( TABLE(select a, e from t1_view 
                           order by a) ) ORDER BY b;

    SELECT * FROM transform( TABLE(select a, e from t1_view
                           where a < 6) ) ORDER BY b;

    SELECT * FROM transform( TABLE(select a, e from t1_view) )
    WHERE a < 6::text ORDER BY b;

    SELECT * FROM transform( TABLE(select a, e from t1_view
                           where a > 10) ) ORDER BY b;
    -- empty result set

    SELECT * FROM transform( TABLE(select a, e from t1_view) )
    WHERE b > 10 ORDER BY b;
    -- empty result set
-- ETF sub-query contains correlated sub-query

select avg(a)::int,'haha'::text from t1;

SELECT * FROM transform( 
    TABLE( select avg(a)::int,'haha'::text from t1 ));

SELECT * FROM transform(
Table(select a,e from t1 t1 where d >
(select avg(a) from t1 t2 where t2.a=t1.a)
)) order by b;

-- This query should fail with following error message:
select a,e from t1 t1 where a >
(SELECT b FROM transform(
TABLE( select avg(a)::int,'haha'::text from t1 t2 where t2.a=t1.d)
)) ;
-- ERROR:  subquery in TABLE value expression may not refer to relation of another query level
-- LINE 3: TABLE( select avg(a)::int,'haha'::text from t1 t2 where t2.a...

-- The following internal correlation sub-query works correctly:
SELECT * FROM transform(
Table(select a,e from t1 t1 where a >
(select avg(a) from t1 t2 where t2.a=t1.d)
)) order by b;

select avg(a) from t1 
group by (select b from transform(TABLE(select a,e from t1 where a=10 )) );

SELECT min(b) FROM
transform( TABLE(select a,e from t1 where a <5) );

------------------------------
-- This query works correctly:
SELECT * FROM t1 WHERE a IN (
SELECT b FROM transform(
TABLE(select a,e from t1
order by a
scatter randomly))
) AND a < 10 order by d;

-- The following query should also work.
-- The only different between this one the one above is this uses "NOT IN"
SELECT * FROM t1 WHERE a NOT IN (
SELECT b FROM transform(
TABLE(select a,e from t1
order by a
scatter randomly))
) AND a < 10 order by a;

-------------------------------
-- For EXISTS and NOT EXISTS
SELECT * FROM t1 WHERE EXISTS (
SELECT 1 FROM transform(
TABLE(select a,e from t1
order by a
scatter randomly))
) AND a < 10 ORDER BY a;

SELECT * FROM t1 WHERE NOT EXISTS (
SELECT 1 FROM transform(
TABLE(select a,e from t1
order by a
scatter randomly))
) AND a < 10 ORDER BY a;

--------------------------------
-- ETF in ALL sub query expression
SELECT * FROM t1 WHERE a>ALL (
SELECT b FROM transform(
TABLE(select a,e from t1
where a>90 and a<98
order by a
scatter randomly))
) ORDER BY a;

--------------------------------
-- Calling ETF from WITH clause (CTE)
WITH first_5 AS (
SELECT b,a FROM transform( TABLE(
select a,e from t1 where a <= 5
))
)
select * from first_5 order by b;

---------------------------------
-- Test ETF as argument
-- The following query should fail with error as shown:
SELECT * FROM ARRAY_APPEND(array['value0','value1'],
(select a from transform( TABLE(select * from intable))));
-- ERROR:  more than one row returned by a subquery used as an expression

-- The following query should succeed
SELECT * FROM ARRAY_APPEND(array['value0','value1'],
(select a from transform( TABLE(select * from intable where id=2))));
-- ETF sub-query contains join table / view

    -- Create another table t2 so to test join tables t1 and t2.
    DROP TABLE IF EXISTS t2;
    CREATE TABLE t2 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a);

    INSERT INTO t2 SELECT i, i/3, i%2, 100-i, 'text'||i 
    FROM generate_series(1,100) i;

-- ETF sub-query: join table works correctly when using format:
-- SELECT FROM t1 JOIN t2 ON t1.a=t2.a
select * from transform(
    TABLE(select t1.a,t2.e from t1
          join t2 on t1.a=t2.a
          where t1.a <10 order by t1.a scatter by t2.c) ) 
order by b;

-- ETF sub-query joins table and putting join condition in where cause
select * from transform(
    TABLE(select t1.a,t2.e from t1,t2 where t1.a=t2.a
          and t1.a < 10 order by t1.a scatter by t2.c) )
order by b;

drop table t2;
-- ETF sub-query contains aggregation
    SELECT * FROM transform( TABLE(select count(*)::int,'haha'::text from t1) ) ORDER BY b;

    SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1
                               where a < 51 
                               group by c 
                               order by avg(a)) ) ORDER BY b;

    -- This query should fail since "scatter by a"
    SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1
                               where a < 51 
                               group by c 
                               order by avg(a)
                               scatter by a) ) ORDER BY b;

    -- This query should succeed since "scatter by c"
    SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1
                               where a < 51
                               group by c
                               order by avg(a)
                               scatter by c) ) ORDER BY b;

SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1 group by c scatter by c) ) ORDER BY b;

SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1 group by c scatter by avg(a)::int) ) ORDER BY b;

SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1 group by c scatter by 'haha'::text) ) ORDER BY b;

SELECT * FROM transform( TABLE(select avg(a)::int,'haha'::text from t1 
                               where a < 51 group by c order by avg(a) scatter by c) ) ORDER BY b;

SELECT sum(b) FROM transform( 
    TABLE(select (rank() over (order by a))::int,'haha'::text 
          from t1 scatter by (rank() over (order by a))::int) );

-- ETF sub-query contains WINDOW function
    select * from transform( TABLE(
        select avg(a) over(partition by c)::int,e from t1
        order by d
        scatter by d
    ) )
    order by a limit 5;

-- ETF call should fail for when input is not a TABLE value expression (TVE)
select * from transform( intable);

select * from transform( select id, value from intable);

select * from transform( t1_view);

select * from transform( select id, value from t1_view);
-- TABLE value expression only takes a select subquery as input.
-- Directly using a table (or view) as input to TVE should fail.

select * from transform( TABLE(intable));

select * from transform( TABLE(t1_view order by id scatter by id));
-- Negative: Some invalid usages of ETF
-- All following queries should fail

    -- Using **DISTRIBUTED** keyword in sub-query
    SELECT * FROM tabfunc( TABLE(select * from t1 
        DISTRIBUTED RANDOMLY
    ) );
    -- ERROR:  syntax error at or near "distributed"

    -- Using **PARTITION** keyword in sub-query
    SELECT * FROM tabfunc( TABLE(select * from t1 
        PARTITION BY a
    ) );
    -- ERROR:  syntax error at or near "partition"

    -- Sub-query ending with semi-colon ';'
    SELECT * FROM tabfunc( TABLE(select * from t1 
        SCATTER RANDOMLY ;
    ) );
    -- ERROR:  syntax error at or near ";"

    -- source table does not exist
    SELECT * FROM tabfunc( TABLE(select * from non_exist) );
    -- ERROR:  relation "non_exist" does not exist

    -- sub-query is not a select query
    SELECT * FROM tabfunc( TABLE(
        update t1 set e='test_new' where a=200
    ) );
    -- ERROR:  syntax error at or near "update"

    -- using multiple TABLE keyword
    SELECT * FROM tabfunc( TABLE TABLE(select * from t1) );
    -- ERROR:  syntax error at or near "TABLE"

    SELECT * FROM tabfunc( TABLE(select a from t1) 
                           TABLE(select b from t1) );
    -- ERROR:  syntax error at or near "TABLE"

    -- using multiple SCATTER keyword
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATTER BY a
                           SCATTER RANDOMLY) );
    -- ERROR:  syntax error at or near "scatter"

    -- Using scatter before order by
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATTER BY a
                           ORDER BY a) );
    -- ERROR:  syntax error at or near "order"

    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATTER RANDOMLY
                           ORDER BY a) );
    -- ERROR:  syntax error at or near "order"

    -- using multiple ORDER keyword
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           ORDER BY a
                           ORDER BY b) );
    -- ERROR:  syntax error at or near "order"

    SELECT * FROM tabfunc( TABLE(select * from t1 
                           ORDER BY a
                           SCATTER RANDOMLY
                           ORDER BY b) );
    -- ERROR:  syntax error at or near "order"

    -- using incorrect keyword **SCATER**, **SCATTERED** instead of SCATTER
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATER RANDOMLY) );
    -- ERROR:  syntax error at or near "scater"

    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATTERED BY a) );
    -- ERROR:  syntax error at or near "scattered"

    -- using incorrect keyword **ORDERED** instead of ORDER
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           ORDERED BY a) );
    -- ERROR:  syntax error at or near "by"

    -- using incorrect parentheses for scatter by 
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATTER BY (a,b)) );
    -- ERROR:  no equality operator for typid 2249 (cdbmutate.c:1177)

    -- using incorrect parentheses for order by 
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           ORDER BY (a,b)) );
    -- ERROR:  could not identify an ordering operator for type record
-- Negative: using SCATTER outside of ETF sub-query

     SELECT * FROM tabfunc( TABLE(select * from t1)
         SCATTER BY a
     );
     -- ERROR:  syntax error at or near "scatter"

     SELECT * FROM tabfunc( TABLE(select * from t1) )
     SCATTER BY a;
     -- ERROR:  syntax error at or near "scatter"

     -- using ORDER BY outside of sub-query
     SELECT * FROM tabfunc( TABLE(select * from t1)
         ORDER BY a
     );
     -- ERROR:  ORDER BY specified, but transform is not an ordered aggregate function
-- Negative: ETF function call missing TABLE keyword,
-- this effectively make the sub-query a value expression

     SELECT * FROM tabfunc( (select count(*) from t1) );

     SELECT * FROM tabfunc( (select id, value from t1) );
-- Negative: ETF function call missing or extra parentheses
    SELECT * FROM tabfunc( TABLE select * from t1 );
    -- ERROR: syntax error at or near "SELECT"

    SELECT * FROM tabfunc TABLE (select * from t1);
    -- ERROR:  syntax error at or near "TABLE"

    SELECT * FROM tabfunc TABLE select * from t1;
    -- ERROR:  syntax error at or near "TABLE"

    SELECT * FROM tabfunc (TABLE) select * from t1;
    -- ERROR:  syntax error at or near ")"

    -- cases of extra parentheses
    SELECT * FROM transform( TABLE( SELECT * FROM intable) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE(( SELECT * FROM intable)) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE((( SELECT * FROM intable))) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE(( SELECT * FROM intable ORDER BY ID)) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE((( SELECT * FROM intable ORDER BY ID))) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE( SELECT * FROM intable SCATTER BY ID) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE(( SELECT * FROM intable SCATTER BY ID)) ); -- ERROR:  syntax error at or near "SCATTER"
    SELECT * FROM transform( TABLE(( SELECT * FROM intable ORDER BY ID SCATTER BY ID)) ); -- ERROR:  syntax error at or near "SCATTER"

    -- Note: SCATTER by applies to the TABLE value expression, it is not part of a normal SelectStmt.
    -- Pushing the SCATTER clause into the select statement is not supported syntax.
    -- The following is allowed syntax:
    SELECT * FROM transform( TABLE( (SELECT * FROM intable) SCATTER BY id) ) order by b;

-- calling undefined ETF function

    SELECT * FROM tabfunc_non( TABLE(select * from t1) );

    SELECT * FROM transform( TABLE_non(select * from t1) );
-- Negative: ETF must be called via FROM
-- The following queries should fail

-- ETF after SELECT
select transform( TABLE(select * from intable));

-- ETF in ORDER BY
select * from t1 order by transform( TABLE(select * from intable));

-- ETF in LIMIT
select * from t1 LIMIT transform (TABLE(select * from intable));

-- ETF in GROUP BY
select avg(a) from t1 group by 
                  (transform(TABLE(select * from t1 where a<10 )));

-- ETF in IN clause
select * from t1 where a in 
              (transform(TABLE(select a,e from t1 where a<10)));

-- Positive: ETF can be used for table JOIN operation
-- The followings should succeed

    -- join table t1 with ETF
    SELECT t1.* from t1, transform( 
        TABLE(select a,e from t1 where a <10 order by a scatter by a) ) t2
    WHERE t1.a = t2.b order by t1.a;

    -- join table t1 with ETF, a different format
    SELECT t1.* from t1
    JOIN transform( TABLE(select a,e from t1 
                          where a <10 order by a scatter by a) ) t2
    ON t1.a = t2.b 
    ORDER BY t1.a;

-- Create view using ETF function

    CREATE VIEW t1viewetf AS
    SELECT * FROM transform( 
        TABLE(select a,e from t1 
              where a < 10
              order by a
              scatter by a
        )
    );
    -- This should succeed

    -- Describe the created view
    \d+ t1viewetf

    -- directly using transform to create view
    CREATE VIEW t1viewetf AS
    transform( 
        TABLE(select a,e from t1 
              where a < 10
              order by a
              scatter by a
        )
    );
    -- This should fail since ETF is not call via FROM

    -- create view using ETF, where ETF itself is using another view
    create view t1_etf_view as (
        select * from transform( 
            table(select a,e from t1_view order by b scatter randomly) 
    ) ) order by a;
    -- This should succeed
  
    \d+ t1_etf_view

    -- Create temp table (CTAS) using ETF 
    create temp table tmp_t2 as select * from transform( 
        table(select a,e from t1 where a<=10) );
    -- This should succeed

    select * from tmp_t2 order by b;

    drop view t1viewetf;
    drop view t1_etf_view;

-- ETF can be used in the subquery

    -- ETF in InitPlan
    select array (select a from transform 
    ( TABLE(select * from intable order by id scatter by value) ) order by a);
    -- This works correctly

    -- Use ETF as Qual
    SELECT * FROM t1 WHERE a < (
        SELECT max(b) FROM 
            transform( TABLE(select a,e from t1 where a <5) )
    ) ORDER BY a, b;

-- ETF can be called within following sub query expression: 
-- IN/NOT IN, EXISTS / NOT EXISTS, ANY/SOME, ALL

    -- ETF called in IN
    SELECT * FROM t1 WHERE a IN (
    SELECT b FROM transform(
        TABLE(select a,e from t1
              order by a
              scatter randomly))
    ) AND a < 10 ORDER BY a, b;

    -- ETF called in IN
    SELECT * FROM t1 WHERE a NOT IN ( -- using NOT IN here
    SELECT b FROM transform(
        TABLE(select a,e from t1
              order by a
              scatter randomly))
    ) AND a < 10 ;

    -- For EXISTS and NOT EXISTS
    SELECT * FROM t1 WHERE EXISTS (
    SELECT 1 FROM transform(
        TABLE(select a,e from t1
              order by a
              scatter randomly))
    ) AND a < 10 ORDER BY a, b;

    SELECT * FROM t1 WHERE NOT EXISTS (
        SELECT 1 FROM transform(
            TABLE(select a,e from t1
                  order by a
                  scatter randomly))
    ) AND a < 10 ;

    -- For ANY/SOME
    SELECT * FROM t1 WHERE a> ANY (
        SELECT b FROM transform(
            TABLE(select a,e from t1
                  where a>98
                  order by a
                  scatter randomly))
    );

    SELECT * FROM t1 WHERE a < SOME (
        SELECT b FROM transform(
            TABLE(select a,e from t1
                  where a<3
                  order by a
                  scatter randomly))
    );

    -- For ALL
    SELECT * FROM t1 WHERE a > ALL (
        SELECT b FROM transform(
            TABLE(select a,e from t1
                  where a<98
                  order by a
                  scatter randomly))
    ) ORDER BY a;

-- ETF sub-query calling from CTE, i.e. WITH clause
      WITH first_5 AS (
          SELECT b,a FROM transform( TABLE(
              select a,e from t1 where a <= 5
          )) 
      )
      select * from first_5 order by b;
-- ETF sub-query recursive call
      SELECT * FROM transform( TABLE(
          select b,a from transform (TABLE(
              select a,e from t1 where a < 5
          ) )
      ) ) order by a;

      SELECT * FROM transform( TABLE(
          select b,a from transform (TABLE(
              select a,e from t1 where a < 5
              order by d
              scatter by c
          ) )
          order by b
          scatter by a
      ) )
      order by b desc;

      SELECT * FROM transform( TABLE(
      SELECT b,a FROM transform( TABLE(
          select b,a from transform (TABLE(
              select a,e from t1 where a < 5
              order by d
              scatter randomly
          ) )
          order by b
          scatter by a
      ) )
      order by b desc
      scatter by b)) order by a;

    -- ETF can be used for PREPARE statement
    PREPARE pretransform (int) AS SELECT * FROM transform( 
    TABLE(SELECT * FROM intable WHERE ID <=$1 ORDER BY ID SCATTER BY value)) 
    ORDER BY b;
    EXECUTE pretransform(5);
    EXECUTE pretransform(5);
    EXECUTE pretransform(5);
    EXECUTE pretransform(5);
    DEALLOCATE pretransform;

-- ETF distribution is the same as underlying source table
-- Should not have redistribution motion, except for SCATTER RANDOMLY

-- Table t1 is distributed by column a
-- Table t3 is distributed by columns a and e
-- Table t4 is distributed randomly
    DROP TABLE IF EXISTS t3;
    CREATE TABLE t3 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a, e);

    INSERT INTO t3 SELECT i, i/3, i%2, 100-i, 'text'||i
    FROM generate_series(1,100) i;

    DROP TABLE IF EXISTS t4;
    CREATE TABLE t4 (a int, b int, c int, d int, e text)
    DISTRIBUTED RANDOMLY;

    INSERT INTO t4 SELECT i, i/3, i%2, 100-i, 'text'||i 
    FROM generate_series(1,100) i;

-- ETF sub-query distribution is different than source table

-- Create t1 distributed by a
-- Create t3 distributed by a, e
    DROP TABLE IF EXISTS t3;
    CREATE TABLE t3 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a, e);

-- start_ignore
create language plpython3u;
-- end_ignore

create or replace function find_operator(query text, operator_name text) returns text as
$$
rv = plpy.execute(query)
search_text = operator_name
result = ['false']
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = ['true']
        break
return result
$$
language plpython3u;

-- ETF sub-query contains join table / view
-- Table t1, t2 are distributed by column a
-- Table t3, t5 are distributed by columns a and e
-- Table t4, t6 are distributed randomly
    DROP TABLE IF EXISTS t2;
    DROP TABLE IF EXISTS t5;
    DROP TABLE IF EXISTS t6;
    DROP TABLE IF EXISTS t4;
    DROP TABLE IF EXISTS t3;

    CREATE TABLE t3 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a, e);

    CREATE TABLE t2 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a);

    INSERT INTO t2 SELECT i, i/3, i%2, 100-i, 'text'||i
    FROM generate_series(1,100) i;

    DROP TABLE IF EXISTS t4;
    CREATE TABLE t4 (a int, b int, c int, d int, e text)
    DISTRIBUTED RANDOMLY;

    CREATE TABLE t5 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a, e);

    INSERT INTO t5 SELECT i, i/3, i%2, 100-i, 'text'||i
    FROM generate_series(1,100) i;

    CREATE TABLE t6 (a int, b int, c int, d int, e text)
    DISTRIBUTED RANDOMLY;

    INSERT INTO t6 SELECT i, i/3, i%2, 100-i, 'text'||i
    FROM generate_series(1,100) i;

    -- Join t1 and t2 that are both distributed by single column a
    -- SCATTER BY the same single distribution key a
    -- of the source tables t1 and t2
    -- No redistribution should be involved
    SELECT find_operator('EXPLAIN SELECT * FROM transform( TABLE(select t1.a, t2.e from t1 join t2 on t1.a = t2.a scatter by t1.a) );','Redistribute Motion');

    -- Join t3 and t5 that are both distributed by composite key columns a, e
    -- SCATTER BY the same composite key a, e
    -- of the source tables t3 and t5
    -- No redistribution should be involved
    SELECT find_operator('EXPLAIN SELECT * FROM transform( TABLE(select t3.a, t5.e from t3 join t5 on (t3.a = t5.a and t3.e = t5.e) scatter by t3.a, t3.e) );','Redistribute Motion');

    -- Both source tables are DISTRIBUTED RANDOMLY
    -- Redistribution is needed
    SELECT find_operator('EXPLAIN SELECT * FROM transform( TABLE(select t4.a, t6.e from t4 join t6 on (t4.a = t6.a and t4.e = t6.e) scatter by t4.a, t6.e) );','Redistribute Motion');

-- start_ignore
drop function if exists find_operator(query text, operator_name text);
-- end_ignore

-- ETF query plan of projection
-- Note: Projection does not show up in plan
-- So needs to run queries

    SELECT b FROM transform( TABLE(
        select a, e from t1
        scatter by a
    ) ) order by b limit 5;

    SELECT b FROM transform( TABLE(
        select a, e from t1
        scatter randomly
    ) ) ORDER BY b desc limit 5;
-- ETF query plan for correlated sub-query

    SELECT * FROM transform( TABLE(
        select a,e from t1 where a > 
            (select avg(a) from t1)
    ) ) ORDER BY b;

-- Check catalog table pg_type for new type anytable
    \x

    select * from pg_type where typname='anytable';


-- Verify pg_proc catalog table for specific columns:
-- # prorettype
-- # proargtypes
-- # proallargtypes
-- # proargmodes
-- # proargnames

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

SELECT prorettype, proargtypes, proallargtypes
       proargmodes, proargnames
FROM pg_proc 
WHERE oid='project_desc'::regproc
    or oid='project'::regproc
ORDER BY oid;

\x
-- After ETF is created and executed, verified regular table function can be created and executed (no regression)
    DROP TYPE IF EXISTS outComp cascade;
    CREATE TYPE outComp AS (b1 varchar(10), b2 varchar(10));

    CREATE OR REPLACE FUNCTION tf_int2char(max integer) 
    RETURNS SETOF outComp AS $$
    DECLARE f outComp%ROWTYPE;
    BEGIN
      FOR i IN 1..max 
      LOOP
        f.b1 := CAST(i AS varchar(10));
        f.b2 := 'tf_test '||CAST(i AS varchar(10));
        RETURN NEXT f;
      END LOOP;
      RETURN;
    END;
    $$ LANGUAGE plpgsql;

    SELECT t1.*, t2.* 
       FROM tf_int2char(5) t1
            JOIN 
            tf_int2char(3) t2
            ON t1.b1 = t2.b1;

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- explicit return type not suitable for dynamic type resolution
    CREATE FUNCTION x() returns int
      AS '$libdir/gppc_test', 'project'
      LANGUAGE C 
      WITH (describe = project_desc);
    -- ERROR:  DESCRIBE only supported for functions returning "record"

    -- explicit return type (setof) not suitable for dynamic type resolution
    CREATE FUNCTION x() returns setof int
      AS '$libdir/gppc_test', 'project'
      LANGUAGE C 
      WITH (describe = project_desc);
    -- ERROR:  DESCRIBE only supported for functions returning "record"

    -- explicit return type (TABLE) not suitable for dynamic type resolution
    CREATE FUNCTION x() returns TABLE(id integer, "time" timestamp, sessionnum integer)
      AS '$libdir/gppc_test', 'project'
      LANGUAGE C 
      WITH (describe = project_desc);
    -- ERROR:  DESCRIBE is not supported for functions that return TABLE

    -- explicit return type (OUT PARAMS) not suitable for dynamic type resolution
    CREATE FUNCTION x(OUT id integer, OUT "time" timestamp, OUT sessionnum integer)
      AS '$libdir/gppc_test', 'project'
      LANGUAGE C 
      WITH (describe = project_desc);
    -- ERROR:  DESCRIBE is not supported for functions with OUT parameters
    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

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

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

    -- Query returns empty result set because ETF returns empty result set
    SELECT * FROM project( TABLE( SELECT id FROM history 
        where id > 4
        order by id scatter by id, time), 1) order by 1 desc;

    -- Query resturns empty result because of filter outside of ETF 
    SELECT * FROM project( TABLE( SELECT id FROM history 
        order by id scatter by id, time), 1) where id > 4;

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

   -- Using constant input to ETF
    SELECT * FROM project( TABLE( SELECT 'col_1','col_2','col_3' ), 2);

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);


    -- Rearrange column sequence of ETF input, order by multiple columns inside ETF
    SELECT * FROM project( TABLE( SELECT time,id FROM history order by id,time scatter by id), 1) order by 1;

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

    -- Valid operations on results
    -- Using expression in scatter by and projected column
    SELECT id+1 FROM project( TABLE( SELECT * FROM history where id >2 scatter by id+1), 1) order by 1;

    -- Avg function
    SELECT avg(id) FROM project( TABLE( SELECT * FROM history ), 1);

    -- extract function, which takes timestamp type as input
    SELECT extract(day from "time") FROM project( TABLE( SELECT * FROM history where time >'2011-08-21'), 2) order by 1;

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

    -- Source table history contains null values
    insert into history values (null,'2011-08-24'), (4,null), (null, null);

    SELECT id FROM project( TABLE( SELECT id FROM history 
    where id is null
    order by id scatter by id, time), 1) order by 1 desc;

    delete from history where id is null or time is null;
    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

    -- ETF subquery using view
    drop view if exists history_v;
    create view history_v as (
    select * from history order by id);

    SELECT * FROM project( 
        TABLE( SELECT * FROM history_v order by id scatter by id), 1) 
    where id < 3 order by 1;

    DROP FUNCTION project(anytable, integer);
    DROP FUNCTION project_desc(internal);
    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

    -- ETF recursive call
    SELECT * FROM project( TABLE( SELECT * FROM (
        SELECT * FROM project (
            TABLE (SELECT * FROM history where time is not null 
            order by id scatter by time), 2) as project_alias1
        ) as project_alias2 
        order by time scatter by time), 1) 
    order by 1;

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

    drop view if exists history_v;
    create view history_v as (
    select * from history order by id);

    -- History table self-join
    SELECT * FROM project( TABLE(SELECT * FROM history h1, history h2
        WHERE h1.id = h2.id and h1.time=h2.time 
        ORDER BY h1.id SCATTER BY h2.time), 3)
    WHERE id <3 ORDER BY 1;

    -- Join history table with history_v view
    SELECT * FROM project( TABLE(SELECT * FROM history h1, history_v h2
        WHERE h1.id = h2.id and h1.time=h2.time 
        ORDER BY h1.id SCATTER BY h2.time), 3)
    WHERE id <3 ORDER BY 1;

  -- Join history table with ETF, using join format
    SELECT * from history h1 join project( TABLE(SELECT * FROM history h1, history_v h2
        WHERE h1.id = h2.id and h1.time=h2.time 
        ORDER BY h1.id SCATTER BY h2.time), 3) h3
    ON h1.id = h3.id
    order by h1.time limit 5;

    -- Join history table with ETF, put join condition in where clause
    SELECT * from history h1, project( TABLE(SELECT * FROM history h1, history_v h2
        WHERE h1.id = h2.id and h1.time=h2.time 
        ORDER BY h1.id SCATTER BY h2.time), 3) h3
    WHERE h1.id = h3.id
    order by h1.time desc limit 5;

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);


-- Negative: Invalid column reference
SELECT time FROM project( TABLE( SELECT * FROM history ), 1);

SELECT id FROM project( TABLE( SELECT time,id FROM history ), 1);

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

-- Invalid projection position 

    SELECT * FROM project( TABLE( SELECT * FROM history ), NULL);
    SELECT * FROM project( TABLE( SELECT * FROM history ), -1);
    SELECT * FROM project( TABLE( SELECT * FROM history ), 0);
    SELECT * FROM project( TABLE( SELECT * FROM history ), 25);
    SELECT * FROM project( TABLE( SELECT time FROM history ), 2);
    SELECT * FROM project( TABLE( SELECT * FROM t1 ), (ARRAY[2,3])[1]);

-- The following queries should work
    SELECT * FROM project( TABLE( SELECT * FROM history ), 1+1) order by time;

    SELECT * FROM project( TABLE( SELECT * FROM t1 ),
      CASE 1 WHEN 2 THEN 1 ELSE GREATEST(1, COALESCE(1+1)) END) 
      order by b limit 10;

    SELECT * FROM project( TABLE( SELECT * FROM t1 ),
      CASE WHEN 3 IS NOT NULL AND 1 IN (1, 2) THEN floor(NULLIF(2, 3))::int END)
      order by b limit 10;

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

   -- DRT table function cannot be used to create view
    CREATE VIEW project_v
    AS SELECT * 
    FROM project ( TABLE (SELECT * FROM history ORDER BY id, time SCATTER BY id ), 1);
    -- ERROR:  CREATE VIEW statements cannot include calls to dynamically typed function

    CREATE VIEW project_v
    AS SELECT * FROM (SELECT *
    FROM project ( TABLE (SELECT * FROM history ORDER BY id, time SCATTER BY id ), 1) as project_alias) as project_alias2;
    -- ERROR:  CREATE VIEW statements cannot include calls to dynamically typed function

-- An existing function that has views defined over it can not be allowed to
-- be altered to have a describe function for similar reasons outlined above.
    -- Create table function project_plain
    -- WITHOUT using the callback function
    CREATE OR REPLACE FUNCTION project_plain (anytable, integer) 
    RETURNS setof record
    AS '$libdir/gppc_test', 'project' 
    LANGUAGE C;

    -- Create a view using table function project_plain
    -- which should be allowed
    CREATE VIEW project_plain_v
    AS SELECT * 
    FROM project_plain ( TABLE (SELECT * FROM history ORDER BY id, time SCATTER BY id ), 1)
    AS project(id integer, "time" timestamp, sessionnum integer);

    -- Now try to replace table func project_plain with one 
    -- actually is Dynamic Return Type table func
    -- i.e. using callback function        
    CREATE OR REPLACE FUNCTION project_plain(anytable, integer) 
    RETURNS setof record
    AS '$libdir/gppc_test', 'project' 
    LANGUAGE C
    WITH (describe = project_desc);
    -- ERROR:  cannot add DESCRIBE callback to function used in view(s)

    DROP VIEW project_plain_v;
    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

    -- Negative: $1 is not a constant
    PREPARE p4 AS SELECT * FROM project( TABLE( SELECT * FROM history ), $1);
    -- ERROR:  unable to resolve type for function
    -- LINE 1: PREPARE p4 AS SELECT * FROM project( TABLE( SELECT * FROM hi...

    -- Negative: $1 is not a constant
    PREPARE p5(integer) AS SELECT * FROM project( TABLE( SELECT * FROM history ), $1);
    -- ERROR:  unable to resolve type for function
    -- LINE 1: PREPARE p5(integer) AS SELECT * FROM project( TABLE( SELECT ...

    -- Positive: can prepare with a dynamic result set MPP-16643
    PREPARE p6 AS SELECT * FROM project( TABLE( SELECT * FROM history ), 1) order by id;
    EXECUTE p6;
    EXECUTE p6;
    EXECUTE p6;
    EXECUTE p6;
    DEALLOCATE p6;
    -- Cannot drop describe (callback) function 
    -- while there is dyr table function (project) is using it
    DROP FUNCTION project_desc(internal);

    --start_ignore
    -- Drop dyr table function first
    DROP FUNCTION IF EXISTS project(anytable, integer);
    DROP FUNCTION IF EXISTS project_desc(internal);
    --end_ignore

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/gppc_test', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/gppc_test', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

    -- Check callback function project_desc is removed from pg_proc_callback
            select * from pg_proc_callback 
            where profnoid='project'::regproc 
            and procallback='project_desc'::regproc;

   -- Verify can drop describe (callback) function 
   -- when no other function is using it
    -- Create Dynamic Return Type test table drt_test
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i 
    FROM generate_series(1,10) i;

    select * from drt_test order by a;

    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    CREATE OR REPLACE FUNCTION ud_describe(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION ud_project1(anytable) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project'
      LANGUAGE C
      WITH (describe = ud_describe);

    -- Simple check of DRT_UC_ETF
    select * from ud_project1( table(select * from drt_test) );

    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- Simple check of DRT_UC_ETF
    select * from ud_project2(table(
        select a from drt_test order by a scatter by (a)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;

    -- Check describe (callback) functions are registerred in pg_proc_callback
    select * from pg_proc_callback 
    where ((profnoid='ud_project1'::regproc and procallback='ud_describe'::regproc)
    or (profnoid='ud_project2'::regproc and procallback='ud_describe2'::regproc)); 

    -- explicit return type not suitable for dynamic type resolution
    CREATE FUNCTION x() returns int
      AS '$libdir/gppc_test', 'userdata_project'
      LANGUAGE C 
      WITH (describe = ud_describe);
    -- ERROR:  DESCRIBE only supported for functions returning "record"

    -- explicit return type (setof) not suitable for dynamic type resolution
    CREATE FUNCTION x() returns setof int
      AS '$libdir/gppc_test', 'userdata_project'
      LANGUAGE C 
      WITH (describe = ud_describe);
    -- ERROR:  DESCRIBE only supported for functions returning "record"

    -- explicit return type (TABLE) not suitable for dynamic type resolution
    CREATE FUNCTION x() returns TABLE(id integer, "time" timestamp, sessionnum integer)
      AS '$libdir/gppc_test', 'userdata_project'
      LANGUAGE C 
      WITH (describe = ud_describe);
    -- ERROR:  DESCRIBE is not supported for functions that return TABLE

    -- explicit return type (OUT PARAMS) not suitable for dynamic type resolution
    CREATE FUNCTION x(OUT id integer, OUT "time" timestamp, OUT sessionnum integer)
      AS '$libdir/gppc_test', 'userdata_project'
      LANGUAGE C 
      WITH (describe = ud_desc);
    -- ERROR:  DESCRIBE is not supported for functions with OUT parameters
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;


    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- ETF order by b scatter by (b)
    select * from ud_project2(table(
        select a from drt_test order by b scatter by (b)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;      

    -- ETF has filter, order by b scatter by (c)
    select * from ud_project2(table(
        select a from drt_test where a<5 order by b scatter by (c)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;

    -- ETF has filter, order by a scatter by (a+b)
    -- Also switch column positions
    select message, position from ud_project2(table(
        select a from drt_test where c=1 order by a desc scatter by (a+b)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position desc;

    -- ETF inner filter that causes empty result set
    select * from ud_project2(table(
        select a from drt_test where a<0 order by b scatter by (c)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;

    -- Outter filter that causes empty result set
    select * from ud_project2(table(
        select a from drt_test order by c scatter by (c)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    where position >10
    order by position;

    -- Using constant input
    select * from ud_project2(table(
        select 3), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt'); 
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- insert records with null values
    insert into drt_test values(null, null, 1, 2);

    -- Verify null value can be handled
    select message, position from ud_project2(table(
        select a from drt_test where c=1 order by a desc scatter by (a+b)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position desc;
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- ETF subquery using view
    create view drt_test_v as (
    select * from drt_test order by a);

    select * from ud_project2(table(
        select a from drt_test where a<5 order by b scatter by (c)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;

    drop view if exists drt_test_v;
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- Negative: Create View Using DRT function with User Context
    create view userdata_v as
    select * from ud_project2(table(
        select a from drt_test order by a scatter by (c)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt'); 
    -- ERROR:  CREATE VIEW statements cannot include calls to dynamically typed function
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    CREATE OR REPLACE FUNCTION ud_describe(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION ud_project(anytable) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project'
      LANGUAGE C
      WITH (describe = ud_describe);

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- Positive: Using DRT function with User Context with PREPARE
    -- MPP-16643
    PREPARE userdata_pre as
    select * from ud_project( table(select * from drt_test) );
    EXECUTE userdata_pre;
    EXECUTE userdata_pre;
    EXECUTE userdata_pre;
    EXECUTE userdata_pre;

    PREPARE userdata_pre2 as
    select * from ud_project2(table(
        select a from drt_test order by a scatter by (c)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') order by position; 
    EXECUTE userdata_pre2;
    EXECUTE userdata_pre2;
    EXECUTE userdata_pre2;
    EXECUTE userdata_pre2;

    DEALLOCATE userdata_pre;
    DEALLOCATE userdata_pre2;
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- When sub query has dup rows
    select * from ud_project2(table(
        select b from drt_test order by a scatter by (a)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);


    -- Correlated sub query
    select * from ud_project2(table(
        select a from drt_test dt1 where a > 
            (select avg(a) from drt_test dt2 where dt1.c = dt2.c)),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- Using aggregation avg()
    select * from ud_project2(table(
        select avg(a)::int from drt_test dt1 where a > 
            (select avg(a) from drt_test dt2 where dt1.c = dt2.c)),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;

    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- Window Function
    select * from ud_project2(table(
        select avg(a) over(partition by b)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- Called from subquery
    select array(
    select message from ud_project2(table(
        select avg(a) over(partition by b)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position);

    select * from drt_test where a < (
      select max(position) from
        (select position from ud_project2(table(
          select avg(a) over(partition by c)::int from drt_test 
          order by d scatter by c),
          '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') ) s    
    ) order by a;
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- Delete the null record from DRT_TEST
    delete from drt_test where a is null;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- ETF called in IN/NOT IN        
    select * from drt_test where a in (
      select position from ud_project2(table(
        select avg(a) over(partition by b)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') ) 
    order by a;

     select * from drt_test where a not in (
      select position from ud_project2(table(
        select avg(a) over(partition by b)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') ) 
    order by a;

    -- For EXISTS and NOT EXISTS
    select * from drt_test where exists (
      select position from ud_project2(table(
        select avg(a) over(partition by b)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') ) 
    order by a;

    select * from drt_test where not exists (
      select position from ud_project2(table(
        select avg(a) over(partition by b)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') ) 
    order by a;

    -- For ANY / SOME / ALL
    select * from drt_test where a > any (
      select position from ud_project2(table(
        select avg(a) over(partition by c)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') ) 
    order by a;

    select * from drt_test where a < some (
      select position from ud_project2(table(
        select avg(a) over(partition by c)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') ) 
    order by a;


    select * from drt_test where a < all (
      select position from ud_project2(table(
        select avg(a) over(partition by c)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') ) 
    order by a;


    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- inside CTE
    WITH first_4 AS (
    select * from drt_test where a < all (
      select position from ud_project2(table(
        select avg(a) over(partition by c)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') ) )      
    select * from first_4 order by a;
 
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- Recursive call
    select * from ud_project2(table(
      select position from ud_project2(table(
        select avg(a) over(partition by c)::int from drt_test 
        order by d scatter by c),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') ) ,
    '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt')
    order by position desc;
    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- Query table that only available on coordinator (catalog tables)
    select * from ud_project2(table(
        select dbid::int from gp_segment_configuration 
        where dbid < 3
        order by dbid scatter by dbid),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt')
    order by position desc;
    -- Positive: Using DRT function with User Context with PREPARE
    -- MPP-16643
    -- Change UDF after PREPARED. The expected behavior is the prepared statement 
    -- still using the old UDF definition.

    -- Create Dynamic Return Type test table drt_test
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i 
    FROM generate_series(1,10) i;

    CREATE OR REPLACE FUNCTION ud_describe(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION ud_project(anytable) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project'
      LANGUAGE C
      WITH (describe = ud_describe);

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/gppc_test', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/gppc_test', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    PREPARE userdata_pre as
    select * from ud_project( table(select * from drt_test) );

    EXECUTE userdata_pre;

    -- The PREPARED statement should still work since it is cached
    EXECUTE userdata_pre;
    EXECUTE userdata_pre;

    DEALLOCATE userdata_pre;

    -- PREPARE using new UDF signature, should succeed
    PREPARE userdata_pre as
    select * from ud_project2(table(
        select a from drt_test order by a scatter by (c)),
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') order by position; 

    EXECUTE userdata_pre;
    DEALLOCATE userdata_pre;
-- Calling the error callback function (tfcallback()) from table function (project_errorcallback())

CREATE OR REPLACE FUNCTION project_errorcallback(anytable, OUT int, OUT int) RETURNS SETOF record AS '$libdir/gppc_test' LANGUAGE c;

SELECT * FROM project_errorcallback(TABLE(SELECT CASE WHEN a < 10 THEN a END, a FROM generate_series(1, 10)a SCATTER BY a));

SELECT * FROM project_errorcallback(TABLE(SELECT a, a FROM generate_series(1, 5)a SCATTER BY a)) ORDER BY 1;
-- Calling error callback function errorcallback from ETF describe function (tablefunc_describe)

CREATE OR REPLACE FUNCTION tablefunc_describe(internal) RETURNS internal AS '$libdir/gppc_test' LANGUAGE c;
CREATE OR REPLACE FUNCTION tablefunc_project(anytable, int) RETURNS SETOF record AS '$libdir/gppc_test' LANGUAGE c WITH(describe=tablefunc_describe);
SELECT * FROM tablefunc_project(TABLE(SELECT a, a / 10 FROM generate_series(1, 10)a SCATTER BY a), 2) ORDER BY 1;
--  ETF using SPI with describe function and user context

CREATE OR REPLACE FUNCTION describe_spi(internal) RETURNS internal AS '$libdir/gppc_test' LANGUAGE c;
CREATE OR REPLACE FUNCTION project_spi(anytable, text) RETURNS SETOF record AS '$libdir/gppc_test' LANGUAGE c WITH(describe=describe_spi);
SELECT * FROM project_spi(TABLE(SELECT a::text FROM generate_series(1, 10)a SCATTER BY a), 'SELECT $$foo$$') ORDER BY 1;

-- Negative: Test system behavior and error message when sub query is non-SELECT query
-- such as INSERT, INSERT ... INTO, UPDATE, DELETE, 
-- or even DDL (CREATE/ALTER TABLE, CREATE/ALTER VIEW, CREATE/ALTER INDEX, VACUUM, and etc)

SELECT * FROM transform(
TABLE( INSERT INTO randtable values (11, 'value_11'))
);

SELECT * FROM transform(
TABLE( UPDATE randtable SET value='value_11' WHERE id=11)
);

SELECT * FROM transform(
TABLE( DELETE FROM randtable WHERE id=11;)
);

SELECT * FROM transform(
TABLE( select i, 'foo'||i into randtable2 from generate_series(11,15) i)
);

SELECT * FROM ud_project(
TABLE( CREATE TABLE neg_test (a int, b text)));

SELECT * FROM ud_project(
TABLE( ALTER TABLE randtable RENAME id TO id2));


SELECT * FROM ud_project(
TABLE( vacuum));

