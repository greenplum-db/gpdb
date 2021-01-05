
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

create schema qp_gist_indexes2;
set search_path to qp_gist_indexes2;

 -- start_ignore
create language plpython3u;
-- end_ignore

create or replace function count_index_scans(explain_query text) returns int as
$$
rv = plpy.execute(explain_query)
search_text = 'Index Scan'
result = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = result+1
return result
$$
language plpython3u;

-- ----------------------------------------------------------------------
-- Test: test01create_table.sql
-- ----------------------------------------------------------------------

CREATE TABLE GistTable1 ( id INTEGER, owner VARCHAR, description VARCHAR, property BOX, poli POLYGON, bullseye CIRCLE, v VARCHAR, t TEXT, f FLOAT, p POINT, c CIRCLE, filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 DISTRIBUTED BY (id);

COPY GistTable1 FROM
'@abs_srcdir@/data/PropertyInfo.txt'
 CSV
 ;
ANALYZE GistTable1;
-- ----------------------------------------------------------------------
-- Test: test03IndexScan.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
--     This does some simple queries that should use the index.  
--     We can't see directly whether the index was used, but for each query 
--     we can run "EXPLAIN" and see whether the query used the index.
-- ----------------------------------------------------------------------------

CREATE INDEX propertyBoxIndex ON GistTable1 USING Gist (property);

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (66, 'Miller', 'Lubbock or leave it', '((3, 1300), (33, 1330))',
   '( (66,660), (67, 650), (68, 660) )', '( (66, 66), 66)' );

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
SELECT owner, property FROM GistTable1 
 WHERE property ~= '((7052,250),(6050,20))';
SELECT owner, property FROM GistTable1 
 WHERE property ~= ' ( ( 7052, 250 ) , (6050, 20) )';
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';
EXPLAIN (COSTS OFF)
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';

SELECT id, property FROM GistTable1 
 WHERE property IS NULL 
 ORDER BY id;

-- Alter the table and see if we get the same results.
ALTER TABLE GistTable1 CLUSTER ON propertyBoxIndex;

SELECT owner, property FROM GistTable1 
 WHERE property ~= '((7052,250),(6050,20))';
SELECT owner, property FROM GistTable1 
 WHERE property ~= ' ( ( 7052, 250 ) , (6050, 20) )';
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';
EXPLAIN (COSTS OFF)
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';

SELECT id, property FROM GistTable1 
 WHERE property IS NULL 
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test04Insert.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Insert more data.
-- ----------------------------------------------------------------------------

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (76, 'James McMurtry', 'Levelland', '((1500, 1500), (1700, 1900))', 
   '( (76, 77), (76, 75), (75, 77) )', '( (76, 76), 76)' );


-- ----------------------------------------------------------------------
-- Test: test05Select.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test06IllegalonAO.sql
-- ----------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

ALTER TABLE GistTable1 CLUSTER ON propertyBoxIndex;

ALTER INDEX propertyBoxIndex RENAME TO propIndex;

UPDATE GistTable1
 SET description = 'Where''s Johnny?', bullseye = NULL
 WHERE owner = 'James McMurtry';

-- We should no longer be able to find "Levelland".
SELECT property FROM GistTable1 
 WHERE description = 'Levelland';

ALTER INDEX propIndex SET (FILLFACTOR=50);

SELECT owner FROM GistTable1 
 WHERE property ~= '( (40, 20), (42, 25) )';

REINDEX INDEX propIndex;

SELECT owner FROM GistTable1 
 WHERE property ~= '( (40, 20), (42, 25) )';

-- Delete Theodore Turner
DELETE from GistTable1 WHERE property ~= '((2000,2000), (2100, 2100))';

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

-- Update James McMurtry.
UPDATE GistTable1 
 SET owner = 'Record Company', description = 'fat profit', 
   property = '((100,100), (200, 200))'
 WHERE property ~= '( (1700,1900),(1500,1500) )';

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

ALTER INDEX propIndex RENAME TO propertyBoxIndex;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

DELETE FROM GistTable1 WHERE bullseye = '( (76, 76), 76)';


-- ----------------------------------------------------------------------
-- Test: test07select.sql
-- ----------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test10MultipleColumns.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: 
--     Test multi-column indexes.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- Insert 2 more records, but insert them with the same value for BULLSEYE.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (212, 'Fahrenheit', 'Slightly north of Hades', '( (212, 212), (32, 32) )', '( (212, 212), (600, 600), (70, 70) )', '( (100,100), 212 )' );
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (100, 'Celsius', 'Barely north of Hades', '( (100, 100), (0, 0) )', '( (100, 100), (600, 600), (70, 70) )', '( (100,100), 212 )' );

-- This should create an index that has duplicate entries for at least one 
--  value of bullseye.
CREATE INDEX i2 ON GistTable1 USING GIST(bullseye);

CREATE INDEX i3 ON GistTable1 USING GIST(poli, bullseye);

-- This should return 2 rows.
SELECT id FROM GistTable1
 WHERE bullseye ~= '( (100,100), 212 )';

-- This should return 1 row.
SELECT id FROM GistTable1
 WHERE property ~= '( (212, 212), (32, 32) )';



-- ----------------------------------------------------------------------
-- Test: test11WherePredicate.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Test GiST indexes with the WHERE predicate in the CREATE INDEX 
--     statement.  This does some simple queries that should use the index.  
--     We can't see directly whether the index was used, but for each query 
--     we can run "EXPLAIN" and see whether the query used the index.
-- ----------------------------------------------------------------------------

-- Add another record that has NULL in the property field.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (77, 'S. T. "Rip" Sunset', 'Lost Vegas',NULL,
   '( (77, 77), (76, 78), (78, 76) )', '( (77, 77), 77)' );

CREATE INDEX propertyIsNullIndex ON GistTable1 USING Gist (property)
 WHERE property IS NULL;

-- Add two more records that have NULL in the property field.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (86, 'A. Gent', 'Washingtoon D.C.',NULL,
   '( (86, 86), (85, 87), (87, 85) )', '( (86, 86), 86)' );
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (99, 'FelDon Adams', 'Washingtoon D.C.',NULL,
   '( (99, 99), (97, 98), (98, 97) )', '( (99, 99), 99)' );

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
set optimizer_trace_fallback = TRUE;
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL;
SELECT count_index_scans('EXPLAIN SELECT owner, property FROM GistTable1 WHERE property IS NULL ORDER BY id;');
--start_ignore
EXPLAIN 
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL
 ORDER BY id
 ;
--end_ignore

-- Alter the table and see if we get the same results.
ALTER TABLE GistTable1 CLUSTER ON propertyBoxIndex;

SELECT id, property FROM GistTable1 
 WHERE property IS NULL 
 ORDER BY id
 ;
SELECT count_index_scans('EXPLAIN SELECT id, property FROM GistTable1 WHERE property IS NULL ORDER BY id;');
--start_ignore
EXPLAIN
SELECT id, property FROM GistTable1 
 WHERE property IS NULL 
 ORDER BY id
 ;
--end_ignore
reset optimizer_trace_fallback;


-- ----------------------------------------------------------------------
-- Test: test13Vacuum.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     Test VACUUM on GiST indexes.
--     Also test somewhat larger data sets than most of my other GiST index 
--     tests.
--
--         This test suite is for AO (Append-Only) and CO (Column-Oriented) 
--         tables as well as heap tables, so I removed statement(s) such as 
--         DELETE that can't be executed on AO and CO tables.
-- ----------------------------------------------------------------------------

CREATE TABLE GistTable13 (
 id INTEGER,
 property BOX,
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?' 
 )
 DISTRIBUTED BY (id);

-- Register a function that converts TEXT to BOX data type.
CREATE FUNCTION TO_BOX(TEXT) RETURNS BOX AS
  $$
    SELECT box_in(textout($1))
  $$ LANGUAGE SQL;

CREATE FUNCTION insertIntoGistTable13 (seed INTEGER) RETURNS VOID
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
   INSERT INTO GistTable13(id, property) VALUES (seed, TO_BOX(CAST(str1 AS TEXT)) );
END;
$$
LANGUAGE PLPGSQL
;

CREATE FUNCTION insertManyIntoGistTable13 (startValue INTEGER, endValue INTEGER) RETURNS VOID
AS
$$
DECLARE 
   i INTEGER;
BEGIN
   i = startValue;
   WHILE i <= endValue LOOP
       PERFORM insertIntoGistTable13(i);
       i = i + 1;
   END LOOP;
END;
$$
LANGUAGE PLPGSQL
;

-- Add some rows before we create the index.
SELECT insertManyIntoGistTable13(1, 1000);

-- Create the index.
CREATE INDEX GistIndex13 ON GistTable13 USING GiST (property);
SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- Add more rows after we create the index.
SELECT insertManyIntoGistTable13(1001, 2000);

ANALYZE GistTable13;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values (coordinates), not just the 
-- AREA, are the same.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';

VACUUM GistTable13;

ANALYZE GistTable13;

SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';

TRUNCATE TABLE GistTable13;

-- Add some rows.
SELECT insertManyIntoGistTable13(1, 1000);

ANALYZE GistTable13;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values are the same, not just the 
-- same AREA.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';


-- ----------------------------------------------------------------------
-- Test: test15ReindexDropIndex.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
--     REINDEX
--     DROP INDEX
-- NOTES:
--     Although we seemingly ignore the output of the EXPLAIN statements, 
--     elsewhere in this test we look for "Index Scan on propertyBoxIndex" 
--     or something similar in order to verify that the index was used.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;

REINDEX INDEX propertyBoxIndex;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;

DROP INDEX propertyBoxIndex;
-- Obviously, this shouldn't use the index now that the index is gone.

set optimizer_enable_tablescan = TRUE;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_gist_indexes2 cascade;
-- end_ignore


-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_gist_indexes2;
set search_path to qp_gist_indexes2;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: test01create_table.sql
-- ----------------------------------------------------------------------

CREATE TABLE GistTable1 ( id INTEGER, owner VARCHAR, description VARCHAR, property BOX, poli POLYGON, bullseye CIRCLE, v VARCHAR, t TEXT, f FLOAT, p POINT, c CIRCLE, filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 WITH (APPENDONLY=True, COMPRESSTYPE=ZLIB, COMPRESSLEVEL=1)
 DISTRIBUTED BY (id);

COPY GistTable1 FROM
'@abs_srcdir@/data/PropertyInfo.txt'
 CSV
 ;
-- ----------------------------------------------------------------------
-- Test: test03IndexScan.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
--     This does some simple queries that should use the index.  
--     We can't see directly whether the index was used, but for each query 
--     we can run "EXPLAIN" and see whether the query used the index.
-- ----------------------------------------------------------------------------

CREATE INDEX propertyBoxIndex ON GistTable1 USING Gist (property);

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (66, 'Miller', 'Lubbock or leave it', '((3, 1300), (33, 1330))',
   '( (66,660), (67, 650), (68, 660) )', '( (66, 66), 66)' );

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
SELECT owner, property FROM GistTable1 
 WHERE property ~= '((7052,250),(6050,20))';
SELECT owner, property FROM GistTable1 
 WHERE property ~= ' ( ( 7052, 250 ) , (6050, 20) )';
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';
EXPLAIN (COSTS OFF)
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';

SELECT id, property FROM GistTable1 
 WHERE property IS NULL 
 ORDER BY id;

-- ----------------------------------------------------------------------
-- Test: test04Insert.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Insert more data.
-- ----------------------------------------------------------------------------

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (76, 'James McMurtry', 'Levelland', '((1500, 1500), (1700, 1900))', 
   '( (76, 77), (76, 75), (75, 77) )', '( (76, 76), 76)' );


-- ----------------------------------------------------------------------
-- Test: test05Select.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test06IllegalonAO.sql
-- ----------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

ALTER INDEX propertyBoxIndex RENAME TO propIndex;

UPDATE GistTable1
 SET description = 'Where''s Johnny?', bullseye = NULL
 WHERE owner = 'James McMurtry';

-- We should no longer be able to find "Levelland".
SELECT property FROM GistTable1 
 WHERE description = 'Levelland';

ALTER INDEX propIndex SET (FILLFACTOR=50);

SELECT owner FROM GistTable1 
 WHERE property ~= '( (40, 20), (42, 25) )';

REINDEX INDEX propIndex;

SELECT owner FROM GistTable1 
 WHERE property ~= '( (40, 20), (42, 25) )';

-- Delete Theodore Turner
DELETE from GistTable1 WHERE property ~= '((2000,2000), (2100, 2100))';

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

-- Update James McMurtry.
UPDATE GistTable1 
 SET owner = 'Record Company', description = 'fat profit', 
   property = '((100,100), (200, 200))'
 WHERE property ~= '( (1700,1900),(1500,1500) )';

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

ALTER INDEX propIndex RENAME TO propertyBoxIndex;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

DELETE FROM GistTable1 WHERE bullseye = '( (76, 76), 76)';


-- ----------------------------------------------------------------------
-- Test: test07select.sql
-- ----------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test10MultipleColumns.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: 
--     Test multi-column indexes.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- Insert 2 more records, but insert them with the same value for BULLSEYE.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (212, 'Fahrenheit', 'Slightly north of Hades', '( (212, 212), (32, 32) )', '( (212, 212), (600, 600), (70, 70) )', '( (100,100), 212 )' );
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (100, 'Celsius', 'Barely north of Hades', '( (100, 100), (0, 0) )', '( (100, 100), (600, 600), (70, 70) )', '( (100,100), 212 )' );

-- This should create an index that has duplicate entries for at least one 
--  value of bullseye.
CREATE INDEX i2 ON GistTable1 USING GIST(bullseye);

CREATE INDEX i3 ON GistTable1 USING GIST(poli, bullseye);

-- This should return 2 rows.
SELECT id FROM GistTable1
 WHERE bullseye ~= '( (100,100), 212 )';

-- This should return 1 row.
SELECT id FROM GistTable1
 WHERE property ~= '( (212, 212), (32, 32) )';



-- ----------------------------------------------------------------------
-- Test: test11WherePredicate.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Test GiST indexes with the WHERE predicate in the CREATE INDEX 
--     statement.  This does some simple queries that should use the index.  
--     We can't see directly whether the index was used, but for each query 
--     we can run "EXPLAIN" and see whether the query used the index.
-- ----------------------------------------------------------------------------

-- Add another record that has NULL in the property field.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (77, 'S. T. "Rip" Sunset', 'Lost Vegas',NULL,
   '( (77, 77), (76, 78), (78, 76) )', '( (77, 77), 77)' );

CREATE INDEX propertyIsNullIndex ON GistTable1 USING Gist (property)
 WHERE property IS NULL;

-- Add two more records that have NULL in the property field.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (86, 'A. Gent', 'Washingtoon D.C.',NULL,
   '( (86, 86), (85, 87), (87, 85) )', '( (86, 86), 86)' );
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (99, 'FelDon Adams', 'Washingtoon D.C.',NULL,
   '( (99, 99), (97, 98), (98, 97) )', '( (99, 99), 99)' );

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL;
EXPLAIN (COSTS OFF)
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL
 ORDER BY id
 ;

-- ----------------------------------------------------------------------
-- Test: test13Vacuum.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     Test VACUUM on GiST indexes.
--     Also test somewhat larger data sets than most of my other GiST index 
--     tests.
--
--         This test suite is for AO (Append-Only) and CO (Column-Oriented) 
--         tables as well as heap tables, so I removed statement(s) such as 
--         DELETE that can't be executed on AO and CO tables.
-- ----------------------------------------------------------------------------

CREATE TABLE GistTable13 (
 id INTEGER,
 property BOX,
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?' 
 )
 WITH (APPENDONLY=True, COMPRESSTYPE=ZLIB, COMPRESSLEVEL=1)
 DISTRIBUTED BY (id);

-- Register a function that converts TEXT to BOX data type.
CREATE FUNCTION TO_BOX(TEXT) RETURNS BOX AS
  $$
    SELECT box_in(textout($1))
  $$ LANGUAGE SQL;

CREATE FUNCTION insertIntoGistTable13 (seed INTEGER) RETURNS VOID
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
   INSERT INTO GistTable13(id, property) VALUES (seed, TO_BOX(CAST(str1 AS TEXT)) );
END;
$$
LANGUAGE PLPGSQL
;

CREATE FUNCTION insertManyIntoGistTable13 (startValue INTEGER, endValue INTEGER) RETURNS VOID
AS
$$
DECLARE 
   i INTEGER;
BEGIN
   i = startValue;
   WHILE i <= endValue LOOP
       PERFORM insertIntoGistTable13(i);
       i = i + 1;
   END LOOP;
END;
$$
LANGUAGE PLPGSQL
;

-- Add some rows before we create the index.
SELECT insertManyIntoGistTable13(1, 1000);

-- Create the index.
CREATE INDEX GistIndex13 ON GistTable13 USING GiST (property);
SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- Add more rows after we create the index.
SELECT insertManyIntoGistTable13(1001, 2000);

ANALYZE GistTable13;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values (coordinates), not just the 
-- AREA, are the same.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';

VACUUM GistTable13;

ANALYZE GistTable13;

SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';

TRUNCATE TABLE GistTable13;

-- Add some rows.
SELECT insertManyIntoGistTable13(1, 1000);

ANALYZE GistTable13;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values are the same, not just the 
-- same AREA.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';


-- ----------------------------------------------------------------------
-- Test: test15ReindexDropIndex.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
--     REINDEX
--     DROP INDEX
-- NOTES:
--     Although we seemingly ignore the output of the EXPLAIN statements, 
--     elsewhere in this test we look for "Index Scan on propertyBoxIndex" 
--     or something similar in order to verify that the index was used.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;

REINDEX INDEX propertyBoxIndex;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;

DROP INDEX propertyBoxIndex;
-- Obviously, this shouldn't use the index now that the index is gone.

SET optimizer_enable_tablescan = TRUE; 

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_gist_indexes2 cascade;
-- end_ignore



-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_gist_indexes2;
set search_path to qp_gist_indexes2;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: test01create_table.sql
-- ----------------------------------------------------------------------

CREATE TABLE GistTable1 ( id INTEGER, owner VARCHAR, description VARCHAR, property BOX, poli POLYGON, bullseye CIRCLE, v VARCHAR, t TEXT, f FLOAT, p POINT, c CIRCLE, filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 WITH (APPENDONLY=True)
 DISTRIBUTED BY (id);

COPY GistTable1 FROM
'@abs_srcdir@/data/PropertyInfo.txt'
 CSV
 ;
-- ----------------------------------------------------------------------
-- Test: test03IndexScan.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
--     This does some simple queries that should use the index.  
--     We can't see directly whether the index was used, but for each query 
--     we can run "EXPLAIN" and see whether the query used the index.
-- ----------------------------------------------------------------------------

CREATE INDEX propertyBoxIndex ON GistTable1 USING Gist (property);

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (66, 'Miller', 'Lubbock or leave it', '((3, 1300), (33, 1330))',
   '( (66,660), (67, 650), (68, 660) )', '( (66, 66), 66)' );

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
SELECT owner, property FROM GistTable1 
 WHERE property ~= '((7052,250),(6050,20))';
SELECT owner, property FROM GistTable1 
 WHERE property ~= ' ( ( 7052, 250 ) , (6050, 20) )';
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';
EXPLAIN (COSTS OFF)
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';

SELECT id, property FROM GistTable1 
 WHERE property IS NULL 
 ORDER BY id;

-- ----------------------------------------------------------------------
-- Test: test04Insert.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Insert more data.
-- ----------------------------------------------------------------------------

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (76, 'James McMurtry', 'Levelland', '((1500, 1500), (1700, 1900))', 
   '( (76, 77), (76, 75), (75, 77) )', '( (76, 76), 76)' );



-- ----------------------------------------------------------------------
-- Test: test05Select.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test06IllegalonAO.sql
-- ----------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

ALTER INDEX propertyBoxIndex RENAME TO propIndex;

UPDATE GistTable1
 SET description = 'Where''s Johnny?', bullseye = NULL
 WHERE owner = 'James McMurtry';

-- We should no longer be able to find "Levelland".
SELECT property FROM GistTable1 
 WHERE description = 'Levelland';

ALTER INDEX propIndex SET (FILLFACTOR=50);

SELECT owner FROM GistTable1 
 WHERE property ~= '( (40, 20), (42, 25) )';

REINDEX INDEX propIndex;

SELECT owner FROM GistTable1 
 WHERE property ~= '( (40, 20), (42, 25) )';

-- Delete Theodore Turner
DELETE from GistTable1 WHERE property ~= '((2000,2000), (2100, 2100))';

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

-- Update James McMurtry.
UPDATE GistTable1 
 SET owner = 'Record Company', description = 'fat profit', 
   property = '((100,100), (200, 200))'
 WHERE property ~= '( (1700,1900),(1500,1500) )';

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

ALTER INDEX propIndex RENAME TO propertyBoxIndex;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

DELETE FROM GistTable1 WHERE bullseye = '( (76, 76), 76)';


-- ----------------------------------------------------------------------
-- Test: test07select.sql
-- ----------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test10MultipleColumns.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: 
--     Test multi-column indexes.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- Insert 2 more records, but insert them with the same value for BULLSEYE.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (212, 'Fahrenheit', 'Slightly north of Hades', '( (212, 212), (32, 32) )', '( (212, 212), (600, 600), (70, 70) )', '( (100,100), 212 )' );
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (100, 'Celsius', 'Barely north of Hades', '( (100, 100), (0, 0) )', '( (100, 100), (600, 600), (70, 70) )', '( (100,100), 212 )' );

-- This should create an index that has duplicate entries for at least one 
--  value of bullseye.
CREATE INDEX i2 ON GistTable1 USING GIST(bullseye);

CREATE INDEX i3 ON GistTable1 USING GIST(poli, bullseye);

-- This should return 2 rows.
SELECT id FROM GistTable1
 WHERE bullseye ~= '( (100,100), 212 )';

-- This should return 1 row.
SELECT id FROM GistTable1
 WHERE property ~= '( (212, 212), (32, 32) )';



-- ----------------------------------------------------------------------
-- Test: test11WherePredicate.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Test GiST indexes with the WHERE predicate in the CREATE INDEX 
--     statement.  This does some simple queries that should use the index.  
--     We can't see directly whether the index was used, but for each query 
--     we can run "EXPLAIN" and see whether the query used the index.
-- ----------------------------------------------------------------------------

-- Add another record that has NULL in the property field.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (77, 'S. T. "Rip" Sunset', 'Lost Vegas',NULL,
   '( (77, 77), (76, 78), (78, 76) )', '( (77, 77), 77)' );

CREATE INDEX propertyIsNullIndex ON GistTable1 USING Gist (property)
 WHERE property IS NULL;

-- Add two more records that have NULL in the property field.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (86, 'A. Gent', 'Washingtoon D.C.',NULL,
   '( (86, 86), (85, 87), (87, 85) )', '( (86, 86), 86)' );
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (99, 'FelDon Adams', 'Washingtoon D.C.',NULL,
   '( (99, 99), (97, 98), (98, 97) )', '( (99, 99), 99)' );

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL;
EXPLAIN (COSTS OFF)
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL
 ORDER BY id
 ;

-- ----------------------------------------------------------------------
-- Test: test13Vacuum.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     Test VACUUM on GiST indexes.
--     Also test somewhat larger data sets than most of my other GiST index 
--     tests.
--
--         This test suite is for AO (Append-Only) and CO (Column-Oriented) 
--         tables as well as heap tables, so I removed statement(s) such as 
--         DELETE that can't be executed on AO and CO tables.
-- ----------------------------------------------------------------------------

CREATE TABLE GistTable13 (
 id INTEGER,
 property BOX,
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?' 
 )
 WITH (APPENDONLY=True)
 DISTRIBUTED BY (id);

-- Register a function that converts TEXT to BOX data type.
CREATE FUNCTION TO_BOX(TEXT) RETURNS BOX AS
  $$
    SELECT box_in(textout($1))
  $$ LANGUAGE SQL;

CREATE FUNCTION insertIntoGistTable13 (seed INTEGER) RETURNS VOID
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
   INSERT INTO GistTable13(id, property) VALUES (seed, TO_BOX(CAST(str1 AS TEXT)) );
END;
$$
LANGUAGE PLPGSQL
;

CREATE FUNCTION insertManyIntoGistTable13 (startValue INTEGER, endValue INTEGER) RETURNS VOID
AS
$$
DECLARE 
   i INTEGER;
BEGIN
   i = startValue;
   WHILE i <= endValue LOOP
       PERFORM insertIntoGistTable13(i);
       i = i + 1;
   END LOOP;
END;
$$
LANGUAGE PLPGSQL
;

-- Add some rows before we create the index.
SELECT insertManyIntoGistTable13(1, 1000);

-- Create the index.
CREATE INDEX GistIndex13 ON GistTable13 USING GiST (property);
SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- Add more rows after we create the index.
SELECT insertManyIntoGistTable13(1001, 2000);

ANALYZE GistTable13;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values (coordinates), not just the 
-- AREA, are the same.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';

VACUUM GistTable13;

ANALYZE GistTable13;

SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';

TRUNCATE TABLE GistTable13;

-- Add some rows.
SELECT insertManyIntoGistTable13(1, 1000);

ANALYZE GistTable13;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values are the same, not just the 
-- same AREA.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';


-- ----------------------------------------------------------------------
-- Test: test15ReindexDropIndex.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
--     REINDEX
--     DROP INDEX
-- NOTES:
--     Although we seemingly ignore the output of the EXPLAIN statements, 
--     elsewhere in this test we look for "Index Scan on propertyBoxIndex" 
--     or something similar in order to verify that the index was used.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;

REINDEX INDEX propertyBoxIndex;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;

DROP INDEX propertyBoxIndex;
-- Obviously, this shouldn't use the index now that the index is gone.

set optimizer_enable_tablescan = TRUE;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_gist_indexes2 cascade;
-- end_ignore



-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_gist_indexes2;
set search_path to qp_gist_indexes2;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: test01create_table.sql
-- ----------------------------------------------------------------------

CREATE TABLE GistTable1 ( id INTEGER, owner VARCHAR, description VARCHAR, property BOX, poli POLYGON, bullseye CIRCLE, v VARCHAR, t TEXT, f FLOAT, p POINT, c CIRCLE, filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 WITH (APPENDONLY=True, ORIENTATION='column', COMPRESSTYPE=ZLIB, COMPRESSLEVEL=1)
 DISTRIBUTED BY (id);

COPY GistTable1 FROM
'@abs_srcdir@/data/PropertyInfo.txt'
 CSV
 ;
-- ----------------------------------------------------------------------
-- Test: test03IndexScan.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
--     This does some simple queries that should use the index.  
--     We can't see directly whether the index was used, but for each query 
--     we can run "EXPLAIN" and see whether the query used the index.
-- ----------------------------------------------------------------------------

CREATE INDEX propertyBoxIndex ON GistTable1 USING Gist (property);

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (66, 'Miller', 'Lubbock or leave it', '((3, 1300), (33, 1330))',
   '( (66,660), (67, 650), (68, 660) )', '( (66, 66), 66)' );

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
SELECT owner, property FROM GistTable1 
 WHERE property ~= '((7052,250),(6050,20))';
SELECT owner, property FROM GistTable1 
 WHERE property ~= ' ( ( 7052, 250 ) , (6050, 20) )';
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';
EXPLAIN (COSTS OFF)
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';

SELECT id, property FROM GistTable1 
 WHERE property IS NULL 
 ORDER BY id;

-- ----------------------------------------------------------------------
-- Test: test04Insert.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Insert more data.
-- ----------------------------------------------------------------------------

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (76, 'James McMurtry', 'Levelland', '((1500, 1500), (1700, 1900))', 
   '( (76, 77), (76, 75), (75, 77) )', '( (76, 76), 76)' );



-- ----------------------------------------------------------------------
-- Test: test05Select.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test06IllegalonAO.sql
-- ----------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

ALTER INDEX propertyBoxIndex RENAME TO propIndex;

UPDATE GistTable1
 SET description = 'Where''s Johnny?', bullseye = NULL
 WHERE owner = 'James McMurtry';

-- We should no longer be able to find "Levelland".
SELECT property FROM GistTable1 
 WHERE description = 'Levelland';

ALTER INDEX propIndex SET (FILLFACTOR=50);

SELECT owner FROM GistTable1 
 WHERE property ~= '( (40, 20), (42, 25) )';

REINDEX INDEX propIndex;

SELECT owner FROM GistTable1 
 WHERE property ~= '( (40, 20), (42, 25) )';

-- Delete Theodore Turner
DELETE from GistTable1 WHERE property ~= '((2000,2000), (2100, 2100))';

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

-- Update James McMurtry.
UPDATE GistTable1 
 SET owner = 'Record Company', description = 'fat profit', 
   property = '((100,100), (200, 200))'
 WHERE property ~= '( (1700,1900),(1500,1500) )';

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

ALTER INDEX propIndex RENAME TO propertyBoxIndex;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

DELETE FROM GistTable1 WHERE bullseye = '( (76, 76), 76)';


-- ----------------------------------------------------------------------
-- Test: test07select.sql
-- ----------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test10MultipleColumns.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: 
--     Test multi-column indexes.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- Insert 2 more records, but insert them with the same value for BULLSEYE.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (212, 'Fahrenheit', 'Slightly north of Hades', '( (212, 212), (32, 32) )', '( (212, 212), (600, 600), (70, 70) )', '( (100,100), 212 )' );
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (100, 'Celsius', 'Barely north of Hades', '( (100, 100), (0, 0) )', '( (100, 100), (600, 600), (70, 70) )', '( (100,100), 212 )' );

-- This should create an index that has duplicate entries for at least one 
--  value of bullseye.
CREATE INDEX i2 ON GistTable1 USING GIST(bullseye);

CREATE INDEX i3 ON GistTable1 USING GIST(poli, bullseye);

-- This should return 2 rows.
SELECT id FROM GistTable1
 WHERE bullseye ~= '( (100,100), 212 )';

-- This should return 1 row.
SELECT id FROM GistTable1
 WHERE property ~= '( (212, 212), (32, 32) )';



-- ----------------------------------------------------------------------
-- Test: test11WherePredicate.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Test GiST indexes with the WHERE predicate in the CREATE INDEX 
--     statement.  This does some simple queries that should use the index.  
--     We can't see directly whether the index was used, but for each query 
--     we can run "EXPLAIN" and see whether the query used the index.
-- ----------------------------------------------------------------------------

-- Add another record that has NULL in the property field.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (77, 'S. T. "Rip" Sunset', 'Lost Vegas',NULL,
   '( (77, 77), (76, 78), (78, 76) )', '( (77, 77), 77)' );

CREATE INDEX propertyIsNullIndex ON GistTable1 USING Gist (property)
 WHERE property IS NULL;

-- Add two more records that have NULL in the property field.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (86, 'A. Gent', 'Washingtoon D.C.',NULL,
   '( (86, 86), (85, 87), (87, 85) )', '( (86, 86), 86)' );
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (99, 'FelDon Adams', 'Washingtoon D.C.',NULL,
   '( (99, 99), (97, 98), (98, 97) )', '( (99, 99), 99)' );

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL;
EXPLAIN (COSTS OFF)
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL
 ORDER BY id
 ;

-- ----------------------------------------------------------------------
-- Test: test13Vacuum.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     Test VACUUM on GiST indexes.
--     Also test somewhat larger data sets than most of my other GiST index 
--     tests.
--
--         This test suite is for AO (Append-Only) and CO (Column-Oriented) 
--         tables as well as heap tables, so I removed statement(s) such as 
--         DELETE that can't be executed on AO and CO tables.
-- ----------------------------------------------------------------------------

CREATE TABLE GistTable13 (
 id INTEGER,
 property BOX,
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?' 
 )
 WITH (APPENDONLY=True, ORIENTATION='column', COMPRESSTYPE=ZLIB, COMPRESSLEVEL=1)
 DISTRIBUTED BY (id);

-- Register a function that converts TEXT to BOX data type.
CREATE FUNCTION TO_BOX(TEXT) RETURNS BOX AS
  $$
    SELECT box_in(textout($1))
  $$ LANGUAGE SQL;

CREATE FUNCTION insertIntoGistTable13 (seed INTEGER) RETURNS VOID
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
   INSERT INTO GistTable13(id, property) VALUES (seed, TO_BOX(CAST(str1 AS TEXT)) );
END;
$$
LANGUAGE PLPGSQL
;

CREATE FUNCTION insertManyIntoGistTable13 (startValue INTEGER, endValue INTEGER) RETURNS VOID
AS
$$
DECLARE 
   i INTEGER;
BEGIN
   i = startValue;
   WHILE i <= endValue LOOP
       PERFORM insertIntoGistTable13(i);
       i = i + 1;
   END LOOP;
END;
$$
LANGUAGE PLPGSQL
;

-- Add some rows before we create the index.
SELECT insertManyIntoGistTable13(1, 1000);

-- Create the index.
CREATE INDEX GistIndex13 ON GistTable13 USING GiST (property);
SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- Add more rows after we create the index.
SELECT insertManyIntoGistTable13(1001, 2000);

ANALYZE GistTable13;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values (coordinates), not just the 
-- AREA, are the same.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';

VACUUM GistTable13;

ANALYZE GistTable13;

SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';

TRUNCATE TABLE GistTable13;

-- Add some rows.
SELECT insertManyIntoGistTable13(1, 1000);

ANALYZE GistTable13;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values are the same, not just the 
-- same AREA.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';


-- ----------------------------------------------------------------------
-- Test: test15ReindexDropIndex.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
--     REINDEX
--     DROP INDEX
-- NOTES:
--     Although we seemingly ignore the output of the EXPLAIN statements, 
--     elsewhere in this test we look for "Index Scan on propertyBoxIndex" 
--     or something similar in order to verify that the index was used.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;

REINDEX INDEX propertyBoxIndex;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;

DROP INDEX propertyBoxIndex;
-- Obviously, this shouldn't use the index now that the index is gone.

set optimizer_enable_tablescan = TRUE;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_gist_indexes2 cascade;
-- end_ignore




-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_gist_indexes2;
set search_path to qp_gist_indexes2;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: test01create_table.sql
-- ----------------------------------------------------------------------

CREATE TABLE GistTable1 ( id INTEGER, owner VARCHAR, description VARCHAR, property BOX, poli POLYGON, bullseye CIRCLE, v VARCHAR, t TEXT, f FLOAT, p POINT, c CIRCLE, filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 WITH (APPENDONLY=True, ORIENTATION='column')
 DISTRIBUTED BY (id);

COPY GistTable1 FROM
'@abs_srcdir@/data/PropertyInfo.txt'
 CSV
 ;
-- ----------------------------------------------------------------------
-- Test: test03IndexScan.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
--     This does some simple queries that should use the index.  
--     We can't see directly whether the index was used, but for each query 
--     we can run "EXPLAIN" and see whether the query used the index.
-- ----------------------------------------------------------------------------

CREATE INDEX propertyBoxIndex ON GistTable1 USING Gist (property);

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (66, 'Miller', 'Lubbock or leave it', '((3, 1300), (33, 1330))',
   '( (66,660), (67, 650), (68, 660) )', '( (66, 66), 66)' );

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
SELECT owner, property FROM GistTable1 
 WHERE property ~= '((7052,250),(6050,20))';
SELECT owner, property FROM GistTable1 
 WHERE property ~= ' ( ( 7052, 250 ) , (6050, 20) )';
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';
EXPLAIN (COSTS OFF)
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';

SELECT id, property FROM GistTable1 
 WHERE property IS NULL 
 ORDER BY id;

-- ----------------------------------------------------------------------
-- Test: test04Insert.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Insert more data.
-- ----------------------------------------------------------------------------

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (76, 'James McMurtry', 'Levelland', '((1500, 1500), (1700, 1900))', 
   '( (76, 77), (76, 75), (75, 77) )', '( (76, 76), 76)' );



-- ----------------------------------------------------------------------
-- Test: test05Select.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test06IllegalonAO.sql
-- ----------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

ALTER INDEX propertyBoxIndex RENAME TO propIndex;

UPDATE GistTable1
 SET description = 'Where''s Johnny?', bullseye = NULL
 WHERE owner = 'James McMurtry';

-- We should no longer be able to find "Levelland".
SELECT property FROM GistTable1 
 WHERE description = 'Levelland';

ALTER INDEX propIndex SET (FILLFACTOR=50);

SELECT owner FROM GistTable1 
 WHERE property ~= '( (40, 20), (42, 25) )';

REINDEX INDEX propIndex;

SELECT owner FROM GistTable1 
 WHERE property ~= '( (40, 20), (42, 25) )';

-- Delete Theodore Turner
DELETE from GistTable1 WHERE property ~= '((2000,2000), (2100, 2100))';

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

-- Update James McMurtry.
UPDATE GistTable1 
 SET owner = 'Record Company', description = 'fat profit', 
   property = '((100,100), (200, 200))'
 WHERE property ~= '( (1700,1900),(1500,1500) )';

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

ALTER INDEX propIndex RENAME TO propertyBoxIndex;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 
 ORDER BY id;

DELETE FROM GistTable1 WHERE bullseye = '( (76, 76), 76)';


-- ----------------------------------------------------------------------
-- Test: test07select.sql
-- ----------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: test10MultipleColumns.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: 
--     Test multi-column indexes.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- Insert 2 more records, but insert them with the same value for BULLSEYE.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (212, 'Fahrenheit', 'Slightly north of Hades', '( (212, 212), (32, 32) )', '( (212, 212), (600, 600), (70, 70) )', '( (100,100), 212 )' );
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (100, 'Celsius', 'Barely north of Hades', '( (100, 100), (0, 0) )', '( (100, 100), (600, 600), (70, 70) )', '( (100,100), 212 )' );

-- This should create an index that has duplicate entries for at least one 
--  value of bullseye.
CREATE INDEX i2 ON GistTable1 USING GIST(bullseye);

CREATE INDEX i3 ON GistTable1 USING GIST(poli, bullseye);

-- This should return 2 rows.
SELECT id FROM GistTable1
 WHERE bullseye ~= '( (100,100), 212 )';

-- This should return 1 row.
SELECT id FROM GistTable1
 WHERE property ~= '( (212, 212), (32, 32) )';



-- ----------------------------------------------------------------------
-- Test: test11WherePredicate.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Test GiST indexes with the WHERE predicate in the CREATE INDEX 
--     statement.  This does some simple queries that should use the index.  
--     We can't see directly whether the index was used, but for each query 
--     we can run "EXPLAIN" and see whether the query used the index.
-- ----------------------------------------------------------------------------

-- Add another record that has NULL in the property field.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (77, 'S. T. "Rip" Sunset', 'Lost Vegas',NULL,
   '( (77, 77), (76, 78), (78, 76) )', '( (77, 77), 77)' );

CREATE INDEX propertyIsNullIndex ON GistTable1 USING Gist (property)
 WHERE property IS NULL;

-- Add two more records that have NULL in the property field.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (86, 'A. Gent', 'Washingtoon D.C.',NULL,
   '( (86, 86), (85, 87), (87, 85) )', '( (86, 86), 86)' );
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye)
 VALUES (99, 'FelDon Adams', 'Washingtoon D.C.',NULL,
   '( (99, 99), (97, 98), (98, 97) )', '( (99, 99), 99)' );

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL;
EXPLAIN (COSTS OFF)
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL
 ORDER BY id
 ;

-- ----------------------------------------------------------------------
-- Test: test13Vacuum.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE:
--     Test VACUUM on GiST indexes.
--     Also test somewhat larger data sets than most of my other GiST index 
--     tests.
--
--         This test suite is for AO (Append-Only) and CO (Column-Oriented) 
--         tables as well as heap tables, so I removed statement(s) such as 
--         DELETE that can't be executed on AO and CO tables.
-- ----------------------------------------------------------------------------

CREATE TABLE GistTable13 (
 id INTEGER,
 property BOX,
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?' 
 )
 WITH (APPENDONLY=True, ORIENTATION='column')
 DISTRIBUTED BY (id);

-- Register a function that converts TEXT to BOX data type.
CREATE FUNCTION TO_BOX(TEXT) RETURNS BOX AS
  $$
    SELECT box_in(textout($1))
  $$ LANGUAGE SQL;

CREATE FUNCTION insertIntoGistTable13 (seed INTEGER) RETURNS VOID
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
   INSERT INTO GistTable13(id, property) VALUES (seed, TO_BOX(CAST(str1 AS TEXT)) );
END;
$$
LANGUAGE PLPGSQL
;

CREATE FUNCTION insertManyIntoGistTable13 (startValue INTEGER, endValue INTEGER) RETURNS VOID
AS
$$
DECLARE 
   i INTEGER;
BEGIN
   i = startValue;
   WHILE i <= endValue LOOP
       PERFORM insertIntoGistTable13(i);
       i = i + 1;
   END LOOP;
END;
$$
LANGUAGE PLPGSQL
;

-- Add some rows before we create the index.
SELECT insertManyIntoGistTable13(1, 1000);

-- Create the index.
CREATE INDEX GistIndex13 ON GistTable13 USING GiST (property);
SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

-- Add more rows after we create the index.
SELECT insertManyIntoGistTable13(1001, 2000);

ANALYZE GistTable13;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values (coordinates), not just the 
-- AREA, are the same.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';

VACUUM GistTable13;

ANALYZE GistTable13;

SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';

TRUNCATE TABLE GistTable13;

-- Add some rows.
SELECT insertManyIntoGistTable13(1, 1000);

ANALYZE GistTable13;

-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values are the same, not just the 
-- same AREA.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
EXPLAIN (COSTS OFF) SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';


-- ----------------------------------------------------------------------
-- Test: test15ReindexDropIndex.sql
-- ----------------------------------------------------------------------

-- ----------------------------------------------------------------------------
-- PURPOSE: Sanity test GiST indexes.
--     REINDEX
--     DROP INDEX
-- NOTES:
--     Although we seemingly ignore the output of the EXPLAIN statements, 
--     elsewhere in this test we look for "Index Scan on propertyBoxIndex" 
--     or something similar in order to verify that the index was used.
-- ----------------------------------------------------------------------------

SET enable_seqscan = FALSE;
set optimizer_enable_tablescan = FALSE;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;

REINDEX INDEX propertyBoxIndex;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;

DROP INDEX propertyBoxIndex;
-- Obviously, this shouldn't use the index now that the index is gone.

set optimizer_enable_tablescan = TRUE;

SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;
EXPLAIN (COSTS OFF) SELECT id FROM GistTable1 
 WHERE property ~= '( (1,2), (3,4) )'
 ORDER BY id;


-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_gist_indexes2 cascade;
-- end_ignore




