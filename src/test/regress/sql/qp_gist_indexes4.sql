-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

create schema qp_gist_indexes4;
set search_path to qp_gist_indexes4;

-- ----------------------------------------------------------------------
-- Test: test02_createSeedToMangledIntegerFunctions.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
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
-- ----------------------------------------------------------------------------

CREATE FUNCTION SeedToMangledInteger(seed INTEGER, v1 bytea, v2 bytea)
RETURNS INTEGER
AS
$$
-- Compose a number.
DECLARE 
    result INTEGER;
    len1 INTEGER;
    len2 INTEGER;
    idx INTEGER;
    firstDigits INTEGER;
    lastDigits INTEGER;
    res INTEGER;
BEGIN
    len1 = octet_length(v1);
    len2 = octet_length(v2);
    idx = seed % len1;
    firstDigits = get_byte(v1, idx);
    idx = seed % len2;
    lastDigits = get_byte(v2, idx);
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
  SELECT SeedToMangledInteger($1,
          E'\\000Oh\\000I\\000live\\000in\\000the\\000mid-S.F.\\000Bay,\\000now\\000a\\000suburb\\000of\\000Northern\\000L.A.,\\000which\\000extends\\000from\\000the\\000south\\000Baja\\000coast,\\000to\\000the\\000point\\000of\\000the\\000snows\\000northernmost'::bytea,
	  E'\\000;lkwjeqroiuoiu2rThe-8Quick90uBrown4-89Fox43yJumpedt-19Over27theLazyt4f[g9yghDoghy3-948yrASDFnvcGHJn,oqKLPwqelBVNCMXZ;foqwfpoqiuwepfhgnvpown;ONZJNI&*(^$*(@#$@##OWEU'::bytea);
$$
LANGUAGE SQL
IMMUTABLE
;

CREATE FUNCTION f2(seed INTEGER) RETURNS INTEGER
AS
$$
  SELECT SeedToMangledInteger($1,
           E'\\000And\\000the\\000mountains\\000from\\000which\\000we\\000can\\000see\\000are\\000just\\000piles\\000of\\000debris'::bytea,
           E'\\000;lkwjeqroiuoiu2rThe-8Quick90uBrown4-89Fox43yJumpedt-19Over27theLazyt4f[g9yghDoghy3-948yrASDFnvcGHJn,oqKLPwqelBVNCMXZ;foqwfpoqiuwepfhgnvpown;ONZJNI&*(^$*(@#$@##OWEU'::bytea);
$$
LANGUAGE SQL
IMMUTABLE
;


-- This one always returns a positive number.
-- Use this one for the radius of a circle.
CREATE FUNCTION f3(seed INTEGER) RETURNS INTEGER
AS
$$
  SELECT ABS(SeedToMangledInteger($1,
               E'\\000Oh\\000I\\000live\\000in\\000the\\000mid-S.F.\\000Bay,\\000now\\000a\\000suburb\\000of\\000Northern\\000L.A.,\\000which\\000extends\\000from\\000the\\000south\\000Baja\\000coast,\\000to\\000the\\000point\\000of\\000the\\000snows\\000northernmost'::bytea,
               E'\\000rewqjL:KJkl;vzxc*)(_uoipnm,.7890fa@#$%sd4321n,m.7403-sdfoxc;,ew8vwer;oiuxcvlkqwer98vkpjn;lkqwer;ADOFIPUQWERLKNASDF\\000[8QUREQFOI\\000JQWRE8PRJ;GOVN;WEJRP98EURJNVM.ipigunvpjsdpr'::bytea));
$$
LANGUAGE SQL
IMMUTABLE
;

CREATE FUNCTION f4(seed INTEGER) RETURNS INTEGER
AS
$$
  SELECT SeedToMangledInteger($1,
           E'\\000The\\000mountains\\000from\\000which\\000we\\000oft\\000preach,\\000are\\000generally\\000far\\000out\\000of\\000reach;\\000the\\000examples\\000we\\000set,\\000we\\000quickly\\000regret;\\000so\\000we\\000hide\\000alone\\000at\\000the\\000beac'::bytea,
           E'\\000ChiangMaiBangkokMoscowPhiledelphiaColoradoFujiIcelandSaskatchwanManitobaVancouverAlbertaAustraliazenoGREECEisTHAWURDBritishColumbiaFrenchQuarte'::bytea);
$$
LANGUAGE SQL
IMMUTABLE
;



-- ----------------------------------------------------------------------
-- Test: test03_createSeedToGeometricDataTypes.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     These functions generate geometric data types given an integer "seed" 
--     value as a starting point.
-- ----------------------------------------------------------------------------


CREATE FUNCTION SeedToPoint1(seed INTEGER) RETURNS point
AS
$$
   SELECT point(f1($1), f2($1));
$$
LANGUAGE SQL
IMMUTABLE
;

CREATE FUNCTION SeedToPoint2(seed INTEGER) RETURNS point
AS
$$
   SELECT point(f3($1), f4($1));
$$
LANGUAGE SQL
IMMUTABLE
;

-- A box is defined by a pair of points.
-- A box looks like:
--    ( (x1, y1), (x2, y2) )
CREATE FUNCTION SeedToBoxAsText(seed INTEGER) RETURNS TEXT
AS
$$
   SELECT '((' || f1($1) || ', ' || f2($1) || '), (' || f3($1) || ', ' || f4($1) || '))';
$$
LANGUAGE SQL
IMMUTABLE
;

-- A box is defined by a pair of points.
-- A box looks like:
--    ( (x1, y1), (x2, y2) )
CREATE FUNCTION SeedToBox(seed INTEGER) RETURNS BOX
AS
$$
  SELECT box(SeedToPoint1($1), SeedToPoint2($1));
$$
LANGUAGE SQL
IMMUTABLE
;

-- A circle is defined by a center point and a radius.
-- The radius should be a positive number.
-- A circle looks like:
--    ( (x1, y1), r )
CREATE FUNCTION SeedToCircle(seed INTEGER) RETURNS CIRCLE
AS
$$
   SELECT circle(point(f1($1), f2($1)), f3($1));
$$
LANGUAGE SQL
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
   SELECT ('(' || SeedToPoint1($1)::text || ', ' || SeedToPoint2($1) || ')')::polygon;
$$
LANGUAGE SQL
IMMUTABLE
;


-- ----------------------------------------------------------------------
-- Test: test04_createTableAndData.sql
-- ----------------------------------------------------------------------

CREATE TABLE geometricTypes (seed INTEGER, c CIRCLE, b BOX, p POLYGON) 
 DISTRIBUTED BY (seed);

INSERT INTO geometricTypes (seed, c, b, p) 
 SELECT x, 
   SeedToCircle(x), 
   SeedToBox(x), 
   SeedToPolygon(x)
  FROM generate_series(1, 20000)x
 ;
ANALYZE geometricTypes;


-- ----------------------------------------------------------------------
-- Test: test05_select.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     This does a few SELECT statements as a brief sanity check that the 
--     indexes are working correctly.  Furthermore, we request EXPLAIN info
--     for each SELECT.  In this script, we ignore the output of the EXPLAIN
--     commands, but a later part of the test checks that we used an index 
--     scan rather than a sequential scan when executing the SELECT 
--     statements.
-- ----------------------------------------------------------------------------
SET enable_seqscan = False;

SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);

EXPLAIN SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);

SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);

EXPLAIN SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);

SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);

EXPLAIN SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);


-- ----------------------------------------------------------------------
-- Test: test06_createIndexes.sql
-- ----------------------------------------------------------------------

CREATE INDEX gt_index_c ON geometricTypes USING GIST (c);
CREATE INDEX gt_index_b ON geometricTypes USING GIST (b);
CREATE INDEX gt_index_p ON geometricTypes USING GIST (p);


-- ----------------------------------------------------------------------
-- Test: test07_select.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     This does a few SELECT statements as a brief sanity check that the 
--     indexes are working correctly.  Furthermore, we request EXPLAIN info
--     for each SELECT.  In this script, we ignore the output of the EXPLAIN
--     commands, but a later part of the test checks that we used an index 
--     scan rather than a sequential scan when executing the SELECT 
--     statements.
-- ----------------------------------------------------------------------------
SET enable_seqscan = False;
SET optimizer_enable_tablescan = False;

SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);

EXPLAIN SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);

SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);

EXPLAIN SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);

SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);

EXPLAIN SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);


-- ----------------------------------------------------------------------
-- Test: test08_reindex.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     This can be run manually to give the user the option of 
--     interrupting the REINDEX operation with ctrl-C or another kill method.
-- ----------------------------------------------------------------------------

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

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     This tests ROLLBACK on the following index-related operations
--     with GiST indexes:
--         CREATE INDEX
--         REINDEX
--         ALTER INDEX
-- ----------------------------------------------------------------------------

CREATE TABLE gone (seed INTEGER, already_gone CIRCLE, too_far_gone BOX, 
  paragon POLYGON)
 DISTRIBUTED BY (seed);

INSERT INTO gone (seed, already_gone, too_far_gone, paragon)
 SELECT x, SeedToCircle(x), SeedToBox(x), SeedToPolygon(x)
  FROM generate_series(1, 10000)x
 ;
 

SET enable_seqscan = False;
SET optimizer_enable_tablescan = False;

-- Create an index; use the index; then roll back.
BEGIN WORK;
CREATE INDEX gone_around_the_bend ON gone USING GiST (already_gone);
-- This should use the index that we just created.
SELECT * FROM gone 
 WHERE already_gone ~= SeedToCircle(857);
EXPLAIN SELECT * FROM gone 
 WHERE already_gone ~= SeedToCircle(857);
ROLLBACK WORK;

SET optimizer_enable_tablescan = True;

-- Should not use the index, since we rolled back the statement that created 
-- the index.
SELECT * FROM gone 
 WHERE already_gone ~= SeedToCircle(857);
EXPLAIN SELECT * FROM gone 
 WHERE already_gone ~= SeedToCircle(857);

SET optimizer_enable_tablescan = False;
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

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     This does a few SELECT statements as a brief sanity check that the 
--     indexes are working correctly.  Furthermore, we request EXPLAIN info
--     for each SELECT.  In this script, we ignore the output of the EXPLAIN
--     commands, but a later part of the test checks that we used an index 
--     scan rather than a sequential scan when executing the SELECT 
--     statements.
-- ----------------------------------------------------------------------------
SET enable_seqscan = False;
SET optimizer_enable_tablescan = False;

SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);

EXPLAIN SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(10001);

SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);

EXPLAIN SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001);

SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);

EXPLAIN SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(3456);


-- ----------------------------------------------------------------------
-- Test: test11_multiple_filters.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     This does a few SELECT statements as a brief sanity check that the 
--     indexes are working correctly when there are multple predicates in the
--     where clause.  Furthermore, we request EXPLAIN info for each SELECT.  
--     In this script, we ignore the output of the EXPLAIN commands, but a 
--     later part of the test checks that we used an index scan rather than 
--     a sequential scan when executing the SELECT statements.
-- ----------------------------------------------------------------------------
SET enable_seqscan = False;
SET optimizer_enable_tablescan = False;

SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(19510) AND c << SeedToCircle(100000);
EXPLAIN SELECT * FROM geometricTypes 
 WHERE c ~= SeedToCircle(19510) AND c << SeedToCircle(100000);

SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001) AND b << SeedToBox(101);
EXPLAIN SELECT * FROM geometricTypes 
 WHERE b ~= SeedToBox(1001) AND b << SeedToBox(101);

SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(345) AND p << SeedToPolygon(34);
EXPLAIN SELECT * FROM geometricTypes 
 WHERE p ~= SeedToPolygon(345) AND p << SeedToPolygon(34);


-- ----------------------------------------------------------------------
-- Test: test12_partition.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     This tests partition tables with GiST indexes as a brief sanity check
--     that the indexes are working correctly. Furthermore, we request EXPLAIN info
--     for each SELECT.  In this script, we ignore the output of the EXPLAIN
--     commands, but a later part of the test checks that we used an index 
--     scan rather than a sequential scan when executing the SELECT 
--     statements.
-- ----------------------------------------------------------------------------
CREATE TABLE geometricTypesPartition (seed INTEGER, c CIRCLE, b BOX, p POLYGON) 
PARTITION BY range(seed) (Start(1) end(3) every(1));

INSERT INTO geometricTypesPartition (seed, c, b, p) 
 SELECT x%2 + 1, 
   SeedToCircle(x), 
   SeedToBox(x), 
   SeedToPolygon(x)
  FROM generate_series(1, 3000)x
 ;

CREATE INDEX gt_index_c_part ON geometricTypesPartition USING GIST (c);

SET enable_seqscan = False;
SET optimizer_enable_tablescan = False;

SELECT * FROM geometricTypesPartition 
 WHERE c ~= SeedToCircle(101);
EXPLAIN SELECT * FROM geometricTypesPartition 
 WHERE c ~= SeedToCircle(101);

DROP TABLE IF EXISTS geometricTypesPartition;


-- ----------------------------------------------------------------------
-- Test: test13_textsearch.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     This tests full text search with GiST indexes as a brief sanity check
--     that the indexes are working correctly. Furthermore, we request EXPLAIN info
--     for each SELECT.  In this script, we ignore the output of the EXPLAIN
--     commands, but a later part of the test checks that we used an index 
--     scan rather than a sequential scan when executing the SELECT 
--     statements.
-- ----------------------------------------------------------------------------
CREATE TABLE textSearch (seed INTEGER, t tsvector);

CREATE INDEX text_index ON textSearch USING GIST (t);

INSERT INTO textSearch VALUES (1, 'test');
INSERT INTO textSearch VALUES (2, 'test');
INSERT INTO textSearch VALUES (2, 't');
INSERT INTO textSearch VALUES (1, 'est');
INSERT INTO textSearch VALUES (2, 'te');
INSERT INTO textSearch VALUES (1, 'st');
INSERT INTO textSearch VALUES (2, 'tt');
INSERT INTO textSearch VALUES (1, 'hello');
INSERT INTO textSearch VALUES (3, 'world');
INSERT INTO textSearch VALUES (4, 'orca');
INSERT INTO textSearch VALUES (3, 'gpdb');
INSERT INTO textSearch VALUES (4, 'gist');
INSERT INTO textSearch VALUES (3, 'cool');
ANALYZE textSearch;

SELECT * FROM textSearch 
 WHERE t @@ to_tsquery('test'); 
EXPLAIN SELECT * FROM textSearch 
 WHERE t @@ to_tsquery('test'); 

DROP TABLE IF EXISTS textSearch;


-- ----------------------------------------------------------------------
-- Test: test14_performance.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     This tests performance with GiST indexes as a brief sanity check
--     that the indexes are working correctly. Furthermore, we request EXPLAIN info
--     for each SELECT.  In this script, we ignore the output of the EXPLAIN
--     commands, but a later part of the test checks that we used an index 
--     scan rather than a sequential scan when executing the SELECT 
--     statements. This test should not be taking longer than a couple of 
--     seconds. If it goes for a seq scan, then this query will take
--     at least 70x times longer. 
-- ----------------------------------------------------------------------------
SET optimizer_enable_tablescan = TRUE;

CREATE TABLE gist_tbl (a int, p polygon);
CREATE TABLE gist_tbl2 (b int, p polygon);
CREATE INDEX poly_index ON gist_tbl USING gist(p);

INSERT INTO gist_tbl SELECT i, polygon(box(point(i, i+2),point(i+4,
i+6))) FROM generate_series(1,50000)i;
INSERT INTO gist_tbl2 SELECT i, polygon(box(point(i+1, i+3),point(i+5,
i+7))) FROM generate_series(1,50000)i;

ANALYZE gist_tbl;
ANALYZE gist_tbl2;

SELECT count(*) FROM gist_tbl, gist_tbl2 
 WHERE gist_tbl.p <@ gist_tbl2.p;
EXPLAIN SELECT count(*) FROM gist_tbl, gist_tbl2 
 WHERE gist_tbl.p <@ gist_tbl2.p;

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_gist_indexes4 cascade;
-- end_ignore
