
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_gist_indexes4;
set search_path to qp_gist_indexes4;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: test01_createConversionFunctions.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
--     These functions help convert TEXT data to geometric data types (box, 
--     circle, and polygon).
-- AUTHOR: mgilkey
-- end_ignore
------------------------------------------------------------------------------

-- start_ignore
DROP FUNCTION IF EXISTS TO_BOX(TEXT) CASCADE;
DROP FUNCTION IF EXISTS TO_CIRCLE(TEXT) CASCADE;
DROP FUNCTION IF EXISTS TO_POLY(TEXT) CASCADE;
-- end_ignore


CREATE FUNCTION TO_BOX(TEXT) RETURNS BOX AS
  $$
    SELECT box_in(textout($1))
  $$ LANGUAGE SQL
  IMMUTABLE;

CREATE FUNCTION TO_CIRCLE(TEXT) RETURNS CIRCLE AS
  $$
    SELECT circle_in(textout($1))
  $$ LANGUAGE SQL
  IMMUTABLE;

CREATE FUNCTION TO_POLY(TEXT) RETURNS POLYGON AS
  $$
    SELECT poly_in(textout($1))
  $$ LANGUAGE SQL
  IMMUTABLE;


-- ----------------------------------------------------------------------
-- Test: test02_createSeedToMangledIntegerFunctions.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
--     These functions generate data that "jumps around a lot", i.e. 
--         is not in ascending or descending order; 
--         is reasonably, although nowhere near perfectly, dispersed;  
--         and yet is non-volatile (given a specific input, they will always 
--             return the same output) 
--     The results are not very close to random, and in fact are "biased" 
--     (unintentionally) to increase as the input seed value increases, but 
--     they jump around enough to give an index operation some real work to 
--     do, which wouldn't be the case if we simply generated an 
--     ascending sequence like 1, 2, 3, ...
-- AUTHOR: mgilkey
-- end_ignore
------------------------------------------------------------------------------

-- start_ignore
DROP FUNCTION IF EXISTS SeedToMangledInteger(INTEGER, VARCHAR, VARCHAR);
DROP FUNCTION IF EXISTS f1(INTEGER);
DROP FUNCTION IF EXISTS f2(INTEGER);
DROP FUNCTION IF EXISTS f3(INTEGER);
DROP FUNCTION IF EXISTS f4(INTEGER);
-- end_ignore


CREATE FUNCTION SeedToMangledInteger(seed INTEGER, v1 VARCHAR, v2 VARCHAR) 
RETURNS INTEGER
AS
$$
-- Compose a number.
DECLARE 
    result INTEGER;
    len1 INTEGER;
    len2 INTEGER;
    char1 CHAR(1);
    char2 CHAR(1);
    idx INTEGER;
    firstDigits INTEGER;
    lastDigits INTEGER;
    res INTEGER;
BEGIN
    len1 = char_length(v1);
    len2 = char_length(v2);
    idx = seed % len1;
    char1 = substring(v1 from idx for 1);
    idx = seed % len2;
    char2 = substring(v2 from idx for 1);
    firstDigits = ascii(char1);
    lastDigits = ascii(char2);
    res = (firstDigits - 32) * 100 + (lastDigits - 32);
    RETURN res;
END;
$$
LANGUAGE PLPGSQL
IMMUTABLE
;

CREATE FUNCTION f1(seed INTEGER) RETURNS INTEGER
AS
$$
DECLARE 
    string1 VARCHAR;
    string2 VARCHAR;
BEGIN
    string1 = 'Oh I live in the mid-S.F. Bay, now a suburb of Northern L.A., which extends from the south Baja coast, to the point of the snows northernmost.';
    string2 = ';lkwjeqroiuoiu2rThe-8Quick90uBrown4-89Fox43yJumpedt-19Over27theLazyt4f[g9yghDoghy3-948yrASDFnvcGHJn,oqKLPwqelBVNCMXZ;foqwfpoqiuwepfhgnvpown;ONZJNI&*(^$*(@#$@##OWEUR';
    RETURN SeedToMangledInteger(seed, string1, string2);
END;
$$
LANGUAGE PLPGSQL
IMMUTABLE
;

CREATE FUNCTION f2(seed INTEGER) RETURNS INTEGER
AS
$$
DECLARE 
    string1 VARCHAR;
    string2 VARCHAR;
BEGIN
    string1 = 'And the mountains from which we can see are just piles of debris.';
    string2 = ';lkwjeqroiuoiu2rThe-8Quick90uBrown4-89Fox43yJumpedt-19Over27theLazyt4f[g9yghDoghy3-948yrASDFnvcGHJn,oqKLPwqelBVNCMXZ;foqwfpoqiuwepfhgnvpown;ONZJNI&*(^$*(@#$@##OWEUR';
    RETURN SeedToMangledInteger(seed, string1, string2);
END;
$$
LANGUAGE PLPGSQL
IMMUTABLE
;


-- This one always returns a positive number.
-- Use this one for the radius of a circle.
CREATE FUNCTION f3(seed INTEGER) RETURNS INTEGER
AS
$$
DECLARE 
    string1 VARCHAR;
    string2 VARCHAR;
BEGIN
    string1 = 'Oh I live in the mid-S.F. Bay, now a suburb of Northern L.A., which extends from the south Baja coast, to the point of the snows northernmost.';
    string2 = 'rewqjL:KJkl;vzxc*)(_uoipnm,.7890fa@#$%sd4321n,m.7403-sdfoxc;,ew8vwer;oiuxcvlkqwer98vkpjn;lkqwer;ADOFIPUQWERLKNASDF [8QUREQFOI JQWRE8PRJ;GOVN;WEJRP98EURJNVM.ipigunvpjsdpry';
    RETURN ABS(SeedToMangledInteger(seed, string1, string2));
END;
$$
LANGUAGE PLPGSQL
IMMUTABLE
;

CREATE FUNCTION f4(seed INTEGER) RETURNS INTEGER
AS
$$
DECLARE 
    string1 VARCHAR;
    string2 VARCHAR;
BEGIN
    string1 = 'The mountains from which we oft preach, are generally far out of reach; the examples we set, we quickly regret; so we hide alone at the beach';
    string2 = 'ChiangMaiBangkokMoscowPhiledelphiaColoradoFujiIcelandSaskatchwanManitobaVancouverAlbertaAustraliazenoGREECEisTHAWURDBritishColumbiaFrenchQuarter';
    RETURN SeedToMangledInteger(seed, string1, string2);
END;
$$
LANGUAGE PLPGSQL
IMMUTABLE
;



-- ----------------------------------------------------------------------
-- Test: test03_createSeedToGeometricDataTypes.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
--     These functions generate geometric data types given an integer "seed" 
--     value as a starting point.
-- AUTHOR: mgilkey
-- end_ignore
------------------------------------------------------------------------------

-- start_ignore
DROP FUNCTION IF EXISTS SeedToBoxAsText(INTEGER);
DROP FUNCTION IF EXISTS SeedToBox(INTEGER);
DROP FUNCTION IF EXISTS SeedToCircle(INTEGER);
DROP FUNCTION IF EXISTS SeedToPolygon(INTEGER);
-- end_ignore





-- A box is defined by a pair of points.
-- A box looks like:
--    ( (x1, y1), (x2, y2) )
CREATE FUNCTION SeedToBoxAsText(seed INTEGER) RETURNS TEXT
AS
$$
DECLARE 
    s TEXT;
    x1 INTEGER;
    y1 INTEGER;
    x2 INTEGER;
    y2 INTEGER;
BEGIN
   x1 = f1(seed);
   y1 = f2(seed);
   x2 = f3(seed);
   y2 = f4(seed);
   s = '((' || x1 || ', ' || y1 || '), (' || x2 || ', ' || y2 || '))';
   RETURN s;
END;
$$
LANGUAGE PLPGSQL
IMMUTABLE
;

-- A box is defined by a pair of points.
-- A box looks like:
--    ( (x1, y1), (x2, y2) )
CREATE FUNCTION SeedToBox(seed INTEGER) RETURNS BOX
AS
$$
DECLARE 
   s TEXT;
BEGIN
   s = SeedToBoxAsText(seed);
   RETURN TO_BOX(s);
END;
$$
LANGUAGE PLPGSQL
IMMUTABLE
;

-- A circle is defined by a center point and a radius.
-- The radius should be a positive number.
-- A circle looks like:
--    ( (x1, y1), r )
CREATE FUNCTION SeedToCircle(seed INTEGER) RETURNS CIRCLE
AS
$$
DECLARE 
    s TEXT;
    x1 INTEGER;
    y1 INTEGER;
    r INTEGER;
BEGIN
   x1 = f1(seed);
   y1 = f2(seed);
   r  = f3(seed);
   s = '((' || x1 || ', ' || y1 || '), ' || r || ')';
   RETURN TO_CIRCLE(s);
END;
$$
LANGUAGE PLPGSQL
IMMUTABLE
;

-- A polygon is defined by a sequence of points.
-- A polygon looks like:
--    ( (x1, y1), (x2, y2) [, ...] )
-- To save time, we "cheat" and call the SeedToBoxAsText() function, 
-- which returns a sequence of 2 points (as TEXT) and then convert that 
-- to a POLYGON.
CREATE FUNCTION SeedToPolygon(seed INTEGER) RETURNS POLYGON
AS
$$
DECLARE
   s TEXT;
BEGIN
   s = SeedToBoxAsText(seed);
   RETURN TO_POLY(s);
END;
$$
LANGUAGE PLPGSQL
IMMUTABLE
;


-- ----------------------------------------------------------------------
-- Test: test04_createTableAndData.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
-- AUTHOR: mgilkey
-- end_ignore
------------------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS geometricTypes;
-- end_ignore

CREATE TABLE geometricTypes (seed INTEGER, c CIRCLE, b BOX, p POLYGON) 
 DISTRIBUTED BY (seed);

INSERT INTO geometricTypes (seed, c, b, p) 
 SELECT x, 
   SeedToCircle(x), 
   SeedToBox(x), 
   SeedToPolygon(x)
  FROM generate_series(1, 300000)x
 ;



-- ----------------------------------------------------------------------
-- Test: test05_select.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
--     This does a few SELECT statements as a brief sanity check that the 
--     indexes are working correctly.  Furthermore, we request EXPLAIN info
--     for each SELECT.  In this script, we ignore the output of the EXPLAIN
--     commands, but a later part of the test checks that we used an index 
--     scan rather than a sequential scan when executing the SELECT 
--     statements.
-- end_ignore
------------------------------------------------------------------------------
SET enable_seqscan = False;

SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);
-- start_ignore
EXPLAIN SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);
-- end_ignore

SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);
-- start_ignore
EXPLAIN SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);
-- end_ignore

SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);
-- start_ignore
EXPLAIN SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);
-- end_ignore


-- ----------------------------------------------------------------------
-- Test: test06_createIndexes.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
-- AUTHOR: mgilkey
-- end_ignore
------------------------------------------------------------------------------

-- start_ignore
DROP INDEX IF EXISTS gt_index_c;
DROP INDEX IF EXISTS gt_index_b;
DROP INDEX IF EXISTS gt_index_p;
-- end_ignore

CREATE INDEX gt_index_c ON geometricTypes USING GIST (c);
CREATE INDEX gt_index_b ON geometricTypes USING GIST (b);
CREATE INDEX gt_index_p ON geometricTypes USING GIST (p);


-- ----------------------------------------------------------------------
-- Test: test07_select.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
--     This does a few SELECT statements as a brief sanity check that the 
--     indexes are working correctly.  Furthermore, we request EXPLAIN info
--     for each SELECT.  In this script, we ignore the output of the EXPLAIN
--     commands, but a later part of the test checks that we used an index 
--     scan rather than a sequential scan when executing the SELECT 
--     statements.
-- end_ignore
------------------------------------------------------------------------------
SET enable_seqscan = False;

SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);
-- start_ignore
EXPLAIN SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);
-- end_ignore

SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);
-- start_ignore
EXPLAIN SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);
-- end_ignore

SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);
-- start_ignore
EXPLAIN SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);
-- end_ignore


-- ----------------------------------------------------------------------
-- Test: test08_reindex.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
--     This can be run manually to give the user the option of 
--     interrupting the REINDEX operation with ctrl-C or another kill method.
-- AUTHOR: mgilkey
-- end_ignore
------------------------------------------------------------------------------

ALTER INDEX gt_index_c SET (FILLFACTOR = 20);
ALTER INDEX gt_index_b SET (FILLFACTOR = 20);
ALTER INDEX gt_index_p SET (FILLFACTOR = 20);

REINDEX INDEX gt_index_c;
REINDEX INDEX gt_index_b;
REINDEX INDEX gt_index_p;

ALTER INDEX gt_index_c SET (FILLFACTOR = 13);
ALTER INDEX gt_index_b SET (FILLFACTOR = 13);
ALTER INDEX gt_index_p SET (FILLFACTOR = 13);

REINDEX TABLE geometricTypes; 



-- ----------------------------------------------------------------------
-- Test: test10_rollback.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
--     This tests ROLLBACK on the following index-related operations
--     with GiST indexes:
--         CREATE INDEX
--         REINDEX
--         ALTER INDEX
-- AUTHOR: mgilkey
-- end_ignore
------------------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS gone;
-- end_ignore

CREATE TABLE gone (seed INTEGER, already_gone CIRCLE, too_far_gone BOX, 
  paragon POLYGON)
 DISTRIBUTED BY (seed);

INSERT INTO gone (seed, already_gone, too_far_gone, paragon)
 SELECT x, SeedToCircle(x), SeedToBox(x), SeedToPolygon(x)
  FROM generate_series(1, 10000)x
 ;
 

SET enable_seqscan = False;

-- Create an index; use the index; then roll back.
BEGIN WORK;
CREATE INDEX gone_around_the_bend ON gone USING GiST (already_gone);
-- This should use the index that we just created.
SELECT * FROM gone 
 WHERE already_gone ~= SeedToCircle(857);
EXPLAIN SELECT * FROM gone 
 WHERE already_gone ~= SeedToCircle(857);
ROLLBACK WORK;

-- Should not use the index, since we rolled back the statement that created 
-- the index.
SELECT * FROM gone 
 WHERE already_gone ~= SeedToCircle(857);
EXPLAIN SELECT * FROM gone 
 WHERE already_gone ~= SeedToCircle(857);

CREATE INDEX polly_gone ON gone USING GiST (paragon);

BEGIN WORK;
ALTER INDEX polly_gone RENAME TO polly_wanna_cracker;
-- This should use the index, and the EXPLAIN should use the new name.
SELECT * FROM gone 
 WHERE paragon ~= SeedToPolygon(858);
EXPLAIN SELECT * FROM gone 
 WHERE paragon ~= SeedToPolygon(858);
ROLLBACK WORK;

-- Explain should show the original name of the index.
SELECT * FROM gone 
 WHERE paragon ~= SeedToPolygon(858);
EXPLAIN SELECT * FROM gone 
 WHERE paragon ~= SeedToPolygon(858);

CREATE INDEX box_of_rain ON gone USING GiST (too_far_gone) 
 WITH (FILLFACTOR = 100);

-- I'm not sure what should happen when we roll back a REINDEX operation.
-- Let's try it and see.  ;-)  
BEGIN WORK;
REINDEX INDEX box_of_rain;
SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
EXPLAIN SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
ROLLBACK WORK;

SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
EXPLAIN SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);

BEGIN WORK;
ALTER INDEX box_of_rain SET (FILLFACTOR = 40);
SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
EXPLAIN SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
ROLLBACK WORK;

BEGIN WORK;
ALTER INDEX box_of_rain SET (FILLFACTOR = 40);
REINDEX TABLE gone;
SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
EXPLAIN SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
ROLLBACK WORK;

BEGIN WORK;
ALTER INDEX box_of_rain RESET (FILLFACTOR);  -- Sets fillfactor back to 100??
SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
EXPLAIN SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
REINDEX TABLE gone;
SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
EXPLAIN SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
ROLLBACK WORK;

SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);
EXPLAIN SELECT * FROM gone 
 WHERE too_far_gone ~= SeedToBox(859);

DROP TABLE IF EXISTS gone;


-- ----------------------------------------------------------------------
-- Test: test09_select.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
--     This does a few SELECT statements as a brief sanity check that the 
--     indexes are working correctly.  Furthermore, we request EXPLAIN info
--     for each SELECT.  In this script, we ignore the output of the EXPLAIN
--     commands, but a later part of the test checks that we used an index 
--     scan rather than a sequential scan when executing the SELECT 
--     statements.
-- end_ignore
------------------------------------------------------------------------------
SET enable_seqscan = False;

SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);
-- start_ignore
EXPLAIN SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);
-- end_ignore

SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);
-- start_ignore
EXPLAIN SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);
-- end_ignore

SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);
-- start_ignore
EXPLAIN SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);
-- end_ignore


-- ----------------------------------------------------------------------
-- Test: test99_cleanup.sql
-- ----------------------------------------------------------------------

------------------------------------------------------------------------------
-- start_ignore
-- PURPOSE:
-- AUTHOR: mgilkey
-- end_ignore
------------------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS geometricTypes;
DROP FUNCTION IF EXISTS SeedToMangledInteger(INTEGER, VARCHAR, VARCHAR);
DROP FUNCTION IF EXISTS f1(INTEGER);
DROP FUNCTION IF EXISTS f2(INTEGER);
DROP FUNCTION IF EXISTS f3(INTEGER);
DROP FUNCTION IF EXISTS f4(INTEGER);
-- end_ignore


-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_gist_indexes4 cascade;
-- end_ignore
