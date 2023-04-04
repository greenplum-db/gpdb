SELECT name, setting FROM pg_settings WHERE name LIKE 'enable%';
-- start_ignore
create schema rangefuncs_cdb;
set search_path to rangefuncs_cdb, public;
-- end_ignore

DROP TABLE IF EXISTS foo;

CREATE TABLE foo(
  fooid int, foosubid int, fooname text, primary key(fooid,foosubid)
  ) DISTRIBUTED BY (fooid, foosubid);
INSERT INTO foo VALUES
  (1,1,'Joe'),
  (1,2,'Ed'),
  (2,1,'Mary');

CREATE TABLE foo2(fooid int, f2 int) DISTRIBUTED BY (fooid);
INSERT INTO foo2 VALUES
  (1, 11),
  (2, 22),
  (1, 111);

-- In Greenplum we do not support functions which call SQL from the segments
-- for this reason we have rewritten this test to use plpgsql functions rather
-- than SQL language functions.
set optimizer_segments = 3;
--
-- RETURNS SETOF foo2
--
CREATE OR REPLACE FUNCTION foost(int) returns setof foo2 as
$$
DECLARE
r foo2%rowtype;
BEGIN
  r.fooid := $1; r.f2 := 11;
  RETURN NEXT r;
  r.fooid := $1; r.f2 := 33;
  RETURN NEXT r;
  RETURN;
END
$$ language plpgsql;

-- function in select clause
select foost(1);

-- expanding columns in the select list
select (foost(1)).*;

-- function in from clause
select * from foost(1);

-- function over function (executed on coordinator)
select foost(fooid), * from foost(3);

-- function over table (executed on segments)
select foost(fooid), * from foo2;

-- Joining with a table
select * from foo2, foost(3) z where foo2.f2 = z.f2;

-- Lateral function. (If it was a subquery, this would require the LATERAL
-- keyword, but for a function, we're more lenient.)
select * from foo2, foost(foo2.fooid) z where foo2.f2 = z.f2;

-- function in subselect, without correlation
select * from foo2 
where f2 in (select f2 from foost(1) z where z.fooid = foo2.fooid) 
ORDER BY 1,2;

-- function in subselect, with correlation
select * from foo2 
where f2 in (select f2 from foost(foo2.fooid) z where z.fooid = foo2.fooid) 
ORDER BY 1,2;

-- function in subselect, with correlation
select * from foo2 
where f2 in (select f2 from foost(foo2.fooid) z where z.fooid = 1) 
ORDER BY 1,2;

-- function in subselect, with correlation - coordinator only
select * from foost(4) x 
where f2 in (select f2 from foost(x.fooid) z where x.fooid = z.fooid) 
ORDER BY 1,2;

-- nested functions
select z.fooid, z.f2 from foost(sin(pi()/2)::int) z ORDER BY 1,2;

DROP FUNCTION foost(int);

--
-- RETURNS SETOF record
--
CREATE OR REPLACE FUNCTION foor(int) RETURNS setof record AS
$$
DECLARE
rec record;
BEGIN
   rec:= ($1, 11);
   RETURN NEXT rec;
   rec:= ($1, 33);
   RETURN NEXT rec;
   return;
END
$$  LANGUAGE plpgsql;

-- function in select clause
--   Fails: plpgsql does not support SFRM_Materialize
select foor(1);

-- expanding columns in the select list
--    Fails: record type not registered
select (foor(1)).*;

-- function in from clause 
--    Fails: column definition list needed for "record"
select * from foor(1);

-- function in from clause, explicit typing
select * from foor(1) as (fooid int, f2 int);

-- function over function (executed on coordinator)
--    Fails: plpgsql does not support SFRM_Materialize
select foor(fooid), * from foor(3) as (fooid int, f2 int);

-- Joining with a table
select * from foo2, foor(3) z(fooid int, f2 int) where foo2.f2 = z.f2;

-- Lateral function. (If it was a subquery, this would require the LATERAL
-- keyword, but for a function, we're more lenient.)
select * from foo2, foor(foo2.fooid) z(fooid int, f2 int) 
where foo2.f2 = z.f2;

-- function in subselect, without correlation
select * from foo2 
where f2 in (select f2 from foor(1) z(fooid int, f2 int) 
             where z.fooid = foo2.fooid) 
ORDER BY 1,2;

-- function in subselect, with correlation
select * from foo2 
where f2 in (select f2 from foor(foo2.fooid) z(fooid int, f2 int) 
             where z.fooid = foo2.fooid) 
ORDER BY 1,2;

-- function in subselect, with correlation
select * from foo2 
where f2 in (select f2 from foor(foo2.fooid) z(fooid int, f2 int) 
             where z.fooid = 1) 
ORDER BY 1,2;

-- function in subselect, with correlation - coordinator only
select * from foor(4) x(fooid int, f2 int)
where f2 in (select f2 from foor(x.fooid) z(fooid int, f2 int)
             where x.fooid = z.fooid) 
ORDER BY 1,2;

-- nested functions
select z.fooid, z.f2 
from foor(sin(pi()/2)::int) z(fooid int, f2 int)
ORDER BY 1,2;

DROP FUNCTION foor(int);

--
-- RETURNS SETOF record, with OUT parameters
--
CREATE OR REPLACE FUNCTION fooro(int, out fooid int, out f2 int) RETURNS SETOF RECORD AS
$$
BEGIN
  fooid := $1; f2 := 11;
  RETURN NEXT;
  fooid := $1; f2 := 33;
  RETURN NEXT;
  RETURN;
END;
$$ LANGUAGE plpgsql;

-- function in select clause
select fooro(1);

-- expanding columns in the select list
select (fooro(1)).*;

-- function in from clause
select * from fooro(1);

-- function over function (executed on coordinator)
select fooro(fooid), * from fooro(3);

-- Joining with a table
select * from foo2, fooro(3) z where foo2.f2 = z.f2;

-- Lateral function. (If it was a subquery, this would require the LATERAL
-- keyword, but for a function, we're more lenient.)
select * from foo2, fooro(foo2.fooid) z where foo2.f2 = z.f2;

-- function in subselect, without correlation
select * from foo2 
where f2 in (select f2 from fooro(1) z where z.fooid = foo2.fooid) 
ORDER BY 1,2;

-- function in subselect, with correlation
select * from foo2 
where f2 in (select f2 from fooro(foo2.fooid) z where z.fooid = foo2.fooid) 
ORDER BY 1,2;

-- function in subselect, with correlation
select * from foo2 
where f2 in (select f2 from fooro(foo2.fooid) z where z.fooid = 1) 
ORDER BY 1,2;

-- function in subselect, with correlation - coordinator only
select * from fooro(4) x 
where f2 in (select f2 from fooro(x.fooid) z where x.fooid = z.fooid) 
ORDER BY 1,2;

-- nested functions
select z.fooid, z.f2 from fooro(sin(pi()/2)::int) z ORDER BY 1,2;

DROP FUNCTION fooro(int);

--
-- RETURNS TABLE
--
CREATE OR REPLACE FUNCTION foot(int) returns TABLE(fooid int, f2 int) as
$$
BEGIN
  fooid := $1; f2 := 11;
  RETURN NEXT;
  fooid := $1; f2 := 33;
  RETURN NEXT;
  RETURN;
END
$$ language plpgsql;

-- function in select clause
select foot(1);

-- expanding columns in the select list
select (foot(1)).*;

-- function in from clause
select * from foot(1);

-- function over function (executed on coordinator)
select foot(fooid), * from foot(3);

-- Joining with a table
select * from foo2, foot(3) z where foo2.f2 = z.f2;

-- Lateral function. (If it was a subquery, this would require the LATERAL
-- keyword, but for a function, we're more lenient.)
select * from foo2, foot(foo2.fooid) z where foo2.f2 = z.f2;

-- function in subselect, without correlation
select * from foo2 
where f2 in (select f2 from foot(1) z where z.fooid = foo2.fooid) 
ORDER BY 1,2;

-- function in subselect, with correlation
select * from foo2 
where f2 in (select f2 from foot(foo2.fooid) z where z.fooid = foo2.fooid) 
ORDER BY 1,2;

-- function in subselect, with correlation
select * from foo2 
where f2 in (select f2 from foot(foo2.fooid) z where z.fooid = 1) 
ORDER BY 1,2;

-- function in subselect, with correlation - coordinator only
select * from foot(4) x 
where f2 in (select f2 from foot(x.fooid) z where x.fooid = z.fooid) 
ORDER BY 1,2;

-- nested functions
select z.fooid, z.f2 from foot(sin(pi()/2)::int) z ORDER BY 1,2;

DROP FUNCTION foot(int);

-- sql, proretset = f, prorettype = b
CREATE FUNCTION getfoo(int) RETURNS int AS 'SELECT $1;' LANGUAGE SQL CONTAINS SQL;
SELECT * FROM getfoo(1) AS t1;
CREATE VIEW vw_getfoo AS SELECT * FROM getfoo(1);
SELECT * FROM vw_getfoo;

-- sql, proretset = t, prorettype = b
DROP VIEW vw_getfoo;
DROP FUNCTION getfoo(int);
CREATE FUNCTION getfoo(int) RETURNS setof int AS 'SELECT fooid FROM foo WHERE fooid = $1;' LANGUAGE SQL READS SQL DATA;
SELECT * FROM getfoo(1) AS t1;
CREATE VIEW vw_getfoo AS SELECT * FROM getfoo(1);
SELECT * FROM vw_getfoo;

-- sql, proretset = t, prorettype = b
DROP VIEW vw_getfoo;
DROP FUNCTION getfoo(int);
CREATE FUNCTION getfoo(int) RETURNS setof text AS 'SELECT fooname FROM foo WHERE fooid = $1;' LANGUAGE SQL READS SQL DATA;
SELECT * FROM getfoo(1) AS t1;
CREATE VIEW vw_getfoo AS SELECT * FROM getfoo(1);
SELECT * FROM vw_getfoo;

-- sql, proretset = f, prorettype = c
DROP VIEW vw_getfoo;
DROP FUNCTION getfoo(int);
CREATE FUNCTION getfoo(int) RETURNS foo AS 'SELECT * FROM foo WHERE fooid = $1 ORDER BY 1,2,3;' LANGUAGE SQL READS SQL DATA;
SELECT * FROM getfoo(1) AS t1;
CREATE VIEW vw_getfoo AS SELECT * FROM getfoo(1);
SELECT * FROM vw_getfoo;

-- sql, proretset = t, prorettype = c
DROP VIEW vw_getfoo;
DROP FUNCTION getfoo(int);
CREATE FUNCTION getfoo(int) RETURNS setof foo AS 'SELECT * FROM foo WHERE fooid = $1 ORDER BY 1,2,3;' LANGUAGE SQL READS SQL DATA;
SELECT * FROM getfoo(1) AS t1;
CREATE VIEW vw_getfoo AS SELECT * FROM getfoo(1);
SELECT * FROM vw_getfoo;

-- sql, proretset = f, prorettype = record
DROP VIEW vw_getfoo;
DROP FUNCTION getfoo(int);
CREATE FUNCTION getfoo(int) RETURNS RECORD AS 'SELECT * FROM foo WHERE fooid = $1 ORDER BY 1,2,3;' LANGUAGE SQL READS SQL DATA;
SELECT * FROM getfoo(1) AS t1(fooid int, foosubid int, fooname text);
CREATE VIEW vw_getfoo AS SELECT * FROM getfoo(1) AS 
(fooid int, foosubid int, fooname text);
SELECT * FROM vw_getfoo;

-- sql, proretset = t, prorettype = record
DROP VIEW vw_getfoo;
DROP FUNCTION getfoo(int);
CREATE FUNCTION getfoo(int) RETURNS setof record AS 'SELECT * FROM foo WHERE fooid = $1 ORDER BY 1,2,3;' LANGUAGE SQL READS SQL DATA;
SELECT * FROM getfoo(1) AS t1(fooid int, foosubid int, fooname text);
CREATE VIEW vw_getfoo AS SELECT * FROM getfoo(1) AS
(fooid int, foosubid int, fooname text);
SELECT * FROM vw_getfoo;

-- plpgsql, proretset = f, prorettype = b
DROP VIEW vw_getfoo;
DROP FUNCTION getfoo(int);
CREATE FUNCTION getfoo(int) RETURNS int AS 'DECLARE fooint int; BEGIN SELECT fooid into fooint FROM foo WHERE fooid = $1; RETURN fooint; END;' LANGUAGE plpgsql READS SQL DATA;
SELECT * FROM getfoo(1) AS t1;
CREATE VIEW vw_getfoo AS SELECT * FROM getfoo(1);
SELECT * FROM vw_getfoo;

-- plpgsql, proretset = f, prorettype = c
DROP VIEW vw_getfoo;
DROP FUNCTION getfoo(int);
CREATE FUNCTION getfoo(int) RETURNS foo AS 'DECLARE footup foo%ROWTYPE; BEGIN SELECT * into footup FROM foo WHERE fooid = $1 ORDER BY 1,2,3; RETURN footup; END;' LANGUAGE plpgsql READS SQL DATA;
SELECT * FROM getfoo(1) AS t1;
CREATE VIEW vw_getfoo AS SELECT * FROM getfoo(1);
SELECT * FROM vw_getfoo;

DROP VIEW vw_getfoo;
DROP FUNCTION getfoo(int);
DROP TABLE foo2;
DROP TABLE foo;

-- Rescan tests --
--   see rangefuncs.sql
--   Removed in greenplum, can't execute sql on segments

--
-- Test cases involving OUT parameters
--

CREATE FUNCTION foo(in f1 int, out f2 int)
AS 'select $1+1' LANGUAGE sql;
SELECT foo(42);
SELECT * FROM foo(42);
SELECT * FROM foo(42) AS p(x);

-- explicit spec of return type is OK
CREATE OR REPLACE FUNCTION foo(in f1 int, out f2 int) RETURNS int
AS 'select $1+1' LANGUAGE sql CONTAINS SQL;

-- error, wrong result type
CREATE OR REPLACE FUNCTION foo(in f1 int, out f2 int) RETURNS float
AS 'select $1+1' LANGUAGE sql CONTAINS SQL;

-- Can switch from output parmeters to non-ouput parameters
CREATE OR REPLACE FUNCTION foo(in f1 int) RETURNS int
AS 'select $1+1' LANGUAGE sql CONTAINS SQL;

-- Can't change the output type with CREATE OR REPLACE
CREATE OR REPLACE FUNCTION foo(in f1 int) RETURNS record
AS 'select $1+1' LANGUAGE sql CONTAINS SQL;

-- with multiple OUT params you must get a RECORD result
CREATE OR REPLACE FUNCTION foo(in f1 int, out f2 int, out f3 text) RETURNS int
AS 'select $1+1' LANGUAGE sql CONTAINS SQL;

DROP FUNCTION foo(int);
CREATE OR REPLACE FUNCTION foo(in f1 int) RETURNS record
AS 'select $1+1' LANGUAGE sql CONTAINS SQL;

-- Can't change the result type, previously record now typed record
CREATE OR REPLACE FUNCTION foo(in f1 int, out f2 int, out f3 text)
RETURNS record
AS 'select $1+1' LANGUAGE sql CONTAINS SQL;

-- Can't change the result type, previously record now typed record (implied)
CREATE OR REPLACE FUNCTION foo(in f1 int, out f2 int, out f3 text)
AS 'select $1+1' LANGUAGE sql;

-- okay - output type is still record
CREATE OR REPLACE FUNCTION foo(in f1 int, out x record)
AS 'select $1+1' LANGUAGE sql;

-- Can't change the result type, previously record, now setof record
CREATE OR REPLACE FUNCTION foo(in f1 int) RETURNS setof record
AS 'select $1+1' LANGUAGE sql CONTAINS SQL;

DROP FUNCTION foo(int);
CREATE OR REPLACE FUNCTION foo(in f1 int) RETURNS setof record
AS 'select $1+1' LANGUAGE sql CONTAINS SQL;

-- Can't convert between setof record and setof (typed) record.
CREATE OR REPLACE FUNCTION foo(in f1 int, out f2 int, out f3 text) 
RETURNS setof record
AS 'select $1+1, null' LANGUAGE sql CONTAINS SQL;

-- Can't convert between setof record and TABLE
CREATE OR REPLACE FUNCTION foo(in f1 int)
RETURNS TABLE(f2 int, f3 text)
AS 'select $1+1' LANGUAGE sql CONTAINS SQL;

DROP FUNCTION foo(int);
CREATE OR REPLACE FUNCTION foo(in f1 int, out f2 int, out f3 text) 
RETURNS setof record
AS 'select $1+1, null::text' LANGUAGE sql CONTAINS SQL;

-- CAN convert between setof (typed) record and TABLE
CREATE OR REPLACE FUNCTION foo(in f1 int)
RETURNS TABLE(f2 int, f3 text)
AS 'select $1+1, null::text' LANGUAGE sql CONTAINS SQL;

-- Can't change the definition of a (typed) record or table
CREATE OR REPLACE FUNCTION foo(in f1 int)
RETURNS TABLE(f2 int, different text)
AS 'select $1+1, null::text' LANGUAGE sql CONTAINS SQL;

CREATE OR REPLACE FUNCTION foor(in f1 int, out f2 int, out text)
AS $$select $1-1, $1::text || 'z'$$ LANGUAGE sql;
SELECT f1, foor(f1) FROM int4_tbl;
SELECT * FROM foor(42);
SELECT * FROM foor(42) AS p(a,b);

CREATE OR REPLACE FUNCTION foob(in f1 int, inout f2 int, out text)
AS $$select $2-1, $1::text || 'z'$$ LANGUAGE sql;
SELECT f1, foob(f1, f1/2) FROM int4_tbl;
SELECT * FROM foob(42, 99);
SELECT * FROM foob(42, 99) AS p(a,b);

-- Can reference function with or without OUT params for DROP, etc
DROP FUNCTION foo(int);
DROP FUNCTION foor(in f2 int, out f1 int, out text);
DROP FUNCTION foob(in f1 int, inout f2 int);

--
-- For my next trick, polymorphic OUT parameters
--

CREATE FUNCTION dup (f1 anyelement, f2 out anyelement, f3 out anyarray)
AS 'select $1, array[$1,$1]' LANGUAGE sql;
SELECT dup(22);
SELECT dup('xyz');	-- fails
SELECT dup('xyz'::text);
SELECT * FROM dup('xyz'::text);

-- equivalent specification
CREATE OR REPLACE FUNCTION dup (inout f2 anyelement, out f3 anyarray)
AS 'select $1, array[$1,$1]' LANGUAGE sql;
SELECT dup(22);

DROP FUNCTION dup(anyelement);

-- fails, no way to deduce outputs
CREATE FUNCTION bad (f1 int, out f2 anyelement, out f3 anyarray)
AS 'select $1, array[$1,$1]' LANGUAGE sql;

--
-- Test that a set-returning function is not called unnecessarily.
--
-- The planner could legitimately call an immutable function as many times it
-- wishes, but there's no need to call it more than once. (ORCA used to create
-- plans where the FunctionScan was executed on every segment, but a Result
-- on top of it filtered all the rows, except on one segment.
--
CREATE FUNCTION notice_srf() RETURNS SETOF text AS $$
begin
   RAISE NOTICE 'notice_srf called in segment %', gp_execution_segment();

   RETURN NEXT 'foo';
   RETURN NEXT 'bar';
end;
$$ LANGUAGE plpgsql IMMUTABLE;

-- gpdiff suppresses identical NOTICEs coming from multiple segments. But we
-- specifically want to check that we get the NOTICE only from one segment.
-- To defeat gpdiff's duplicate-elimination, the NOTICE includes the segment
-- number in the message, so that the message is different on every segment.
-- But we don't actually don't care which segment it executes on, so filter
-- out the segment number for comparison.
--
-- start_matchsubs
-- m/NOTICE:  notice_srf called in segment (\d+)/
-- s/in segment (\d+)/in segment ###/
-- end_matchsubs

CREATE TEMPORARY TABLE srfdest (t text) DISTRIBUTED RANDOMLY;
INSERT INTO srfdest select * FROM notice_srf();

reset optimizer_segments;
