
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_gist_indexes3;
set search_path to qp_gist_indexes3;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: test01CreateTable.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- Portions Copyright (c) 2010, Greenplum, Inc.  All rights reserved.
-- Portions Copyright (c) 2012-Present Pivotal Software, Inc.
-- PURPOSE:
--     Test VACUUM on GiST indexes.
--     Also test somewhat larger data sets than most of my other GiST index 
--     tests.
-- AUTHOR: mgilkey
-- LAST MODIFIED:
--     2010-04-20 mgilkey
--         This test suite is for AO (Append-Only) and CO (Column-Oriented) 
--         tables as well as heap tables, so I removed statement(s) such as 
--         DELETE that can't be executed on AO and CO tables.
-- end_ignore
------------------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS GistTable3;
DROP FUNCTION IF EXISTS TO_BOX(TEXT) CASCADE;
DROP FUNCTION IF EXISTS insertIntoGistTable3(INTEGER);
DROP FUNCTION IF EXISTS insertManyIntoGistTable3(INTEGER, INTEGER);
-- end_ignore

CREATE TABLE GistTable3 (
 id INTEGER,
 property BOX,
 poli POLYGON,
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?' 
 )
 DISTRIBUTED BY (id);

-- Register a function that converts TEXT to BOX data type.
CREATE FUNCTION TO_BOX(TEXT) RETURNS BOX AS
  $$
    SELECT box_in(textout($1))
  $$ LANGUAGE SQL;

CREATE FUNCTION insertIntoGistTable3 (seed INTEGER) RETURNS VOID
AS
$$
DECLARE 
   str1 VARCHAR;
   ss VARCHAR;
   s2 VARCHAR;
BEGIN
   ss = CAST(seed AS VARCHAR);
   s2 = CAST((seed - 1) AS VARCHAR);
   str1 = '((' || ss || ', ' || ss || '), (' || s2 || ', ' || s2 || '))';
   INSERT INTO GistTable3(id, property) VALUES (seed, TO_BOX(CAST(str1 AS TEXT)) );
END;
$$
LANGUAGE PLPGSQL
;

CREATE FUNCTION insertManyIntoGistTable3 (startValue INTEGER, endValue INTEGER) RETURNS VOID
AS
$$
DECLARE 
   i INTEGER;
BEGIN
   i = startValue;
   WHILE i <= endValue LOOP
       PERFORM insertIntoGistTable3(i);
       i = i + 1;
   END LOOP;
END;
$$
LANGUAGE PLPGSQL
;


-- ----------------------------------------------------------------------
-- Test: test02Insert.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- COPYRIGHT (c) 2010, Greenplum, Inc.  All rights reserved.  
-- PURPOSE:
--     Test VACUUM on GiST indexes.
--     Also test somewhat larger data sets than most of my other GiST index 
--     tests.
-- AUTHOR: mgilkey
-- LAST MODIFIED:
--     2010-04-20 mgilkey
--         This test suite is for AO (Append-Only) and CO (Column-Oriented) 
--         tables as well as heap tables, so I removed statement(s) such as 
--         DELETE that can't be executed on AO and CO tables.
-- end_ignore
------------------------------------------------------------------------------


-- Add some rows before we create the index.
SELECT insertManyIntoGistTable3(1, 2000);

-- Create the index.
CREATE INDEX GistIndex3a ON GistTable3 USING GiST (property);
CREATE INDEX GistIndex3b ON GistTable3 USING GiST (poli);

-- Add more rows after we create the index.
SELECT insertManyIntoGistTable3(2001, 4000);

SELECT id, property AS "ProperTee" FROM GistTable3
 WHERE property ~= '( (999,999), (998,998) )';


-- ----------------------------------------------------------------------
-- Test: test06VacuumFull.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE: Help test VACUUM and REINDEX with GiST indexes. 
-- AUTHOR: mgilkey
-- NOTES:
--     1) We use start_ignore and end_ignore around the EXPLAIN plans.  We use 
--        separate code look at whether the indexes were used by the 
--        optimizer.
-- end_ignore
------------------------------------------------------------------------------

-- Encourage the optimizer to use indexes rather than sequential table scans.
SET enable_seqscan=False;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the coordinate values, not just the area,
-- are the same.
SELECT id, property AS "Property" FROM GistTable3
 WHERE property ~= '( (999,999), (998,998) )';
-- start_ignore
EXPLAIN SELECT id, property AS "Property" FROM GistTable3
 WHERE property ~= '( (999,999), (998,998) )';
-- end_ignore

VACUUM ANALYZE GistTable3;

SELECT id, property AS "ProperTee" FROM GistTable3
 WHERE property ~= '( (999,999), (998,998) )';
-- start_ignore
EXPLAIN SELECT id, property AS "ProperTee" FROM GistTable3
 WHERE property ~= '( (999,999), (998,998) )';
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: test07Reindex.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE: Help test VACUUM and REINDEX with GiST indexes. 
-- AUTHOR: mgilkey
-- NOTES:
--     1) We use start_ignore and end_ignore around the EXPLAIN plans.  We use 
--        separate code look at whether the indexes were used by the 
--        optimizer.
-- end_ignore
------------------------------------------------------------------------------

-- Encourage the optimizer to use indexes rather than sequential table scans.
SET enable_seqscan=False;

REINDEX INDEX GistIndex3a;
REINDEX TABLE GistTable3;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the coordinate values, not just the area,
-- are the same.
SELECT id, property AS "Property" FROM GistTable3
 WHERE property ~= '( (999,999), (998,998) )';
-- start_ignore
EXPLAIN SELECT id, property AS "Property" FROM GistTable3
 WHERE property ~= '( (999,999), (998,998) )';
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: test08UniqueAndPKey.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- PURPOSE:
--     Test UNIQUE indexes and primary keys on geometric data types.
--     It turns out that columns with the geometric data types (at least
--     box, polygon, and circle, and probably any others) can't be part of
--     a distribution key.  And since Greenplum allows unique indexes only on
--     columns that are part of the distribution key, GiST indexes cannot
--     be unique.  And of course since primary keys rely on unique indexes,
--     if we can't have unique GiST indexes, then we can't have primary
--     keys on geometric data types.  So this script is basically a negative
--     test that verifies that we get reasonable error messages when we try
--     to create unique GiST indexes or pimary keys on gemoetric data types.
------------------------------------------------------------------------------

CREATE TABLE gisttable_pktest (id integer, property box, poli polygon, bullseye point);

CREATE UNIQUE INDEX ShouldNotExist ON gisttable_pktest USING GiST (property);
CREATE UNIQUE INDEX ShouldNotExist ON gisttable_pktest USING GiST (poli);
CREATE UNIQUE INDEX ShouldNotExist ON gisttable_pktest USING GiST (bullseye);


-- Test whether geometric types can be part of a primary key.
CREATE TABLE GistTable2 (id INTEGER, property BOX) DISTRIBUTED BY (property);
CREATE TABLE GistTable2 (id INTEGER, poli POLYGON) DISTRIBUTED BY (poli);
CREATE TABLE GistTable2 (id INTEGER, bullseye CIRCLE) DISTRIBUTED BY (bullseye);

-- ----------------------------------------------------------------------
-- Test: test09NegativeTests.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- PURPOSE:
--     "Negative" tests.  Verify that we get reasonable error messages when
--     we try to do unreasonable things, such as create indexes on types that
--     do not support GiST (non-geometric types).
------------------------------------------------------------------------------

-- Try to create GiST indexes on non-geometric data types.
CREATE INDEX ShouldNotExist ON gisttable_pktest USING GiST (id);
-- Try to create GiST indexes on a mix of geometric and
-- non-geometric types.
CREATE INDEX ShouldNotExist ON gisttable_pktest USING GiST (id, property);


-- ----------------------------------------------------------------------
-- Test: test14Hash.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- PURPOSE:
--     Test that you get a reasonable error message when you try to create a
--     HASH index (we no longer support those).
------------------------------------------------------------------------------

CREATE TABLE GistTable14 (
 id INTEGER,
 property BOX
 )
 DISTRIBUTED BY (id);

-- Try to create a hash index.
CREATE INDEX GistIndex14a ON GistTable14 USING HASH (id);
CREATE INDEX GistIndex14b ON GistTable14 USING HASH (property);


-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_gist_indexes3 cascade;
-- end_ignore
