-- GISTINDEXES

-- test01CreateTable.sql

-- start_ignore
DROP TABLE IF EXISTS pre_GistTable1;
DROP TABLE IF EXISTS GistTable1;
-- end_ignore

CREATE TABLE pre_GistTable1 (
 id INTEGER,
 owner VARCHAR,
 description VARCHAR,
 property BOX, -- Assumes that all properties are rectangles whose sides are 
                -- aligned North/South, East/West.
 poli POLYGON,
 bullseye CIRCLE,
 v VARCHAR,
 t TEXT,
 f FLOAT, 
 p POINT,
 c CIRCLE,
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 DISTRIBUTED BY (id)
 PARTITION BY RANGE (id)
  (
  PARTITION p_one START('1') INCLUSIVE END ('10') EXCLUSIVE,
  DEFAULT PARTITION de_fault
  )
;
CREATE TABLE GistTable1 AS SELECT * FROM pre_GistTable1;

\COPY GistTable1 FROM 'bugbuster/data/PropertyInfo.txt' CSV;

-- test03IndexScan.sql

CREATE INDEX propertyBoxIndex ON GistTable1 USING Gist (property);

-- INSERT some more data.
INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) 
 VALUES (66, 'Miller', 'Lubbock or leave it', '((3, 1300), (33, 1330))',
   '( (66,660), (67, 650), (68, 660) )', '( (66, 66), 66)' );

SET enable_seqscan = FALSE;

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
-- start_ignore
EXPLAIN 
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';
-- end_ignore

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
-- start_ignore
EXPLAIN
SELECT owner, property FROM GistTable1
 WHERE property ~= '( (6050, 20), (7052, 250) )';
-- end_ignore

SELECT id, property FROM GistTable1 
 WHERE property IS NULL 
 ORDER BY id;

--test04insert.sql

INSERT INTO GistTable1 (id, owner, description, property, poli, bullseye) VALUES (76, 'James McMurtry', 'Levelland', '((1500, 1500), (1700, 1900))', '( (76, 77), (76, 75), (75, 77) )', '( (76, 76), 76)' );

-- test05Select.sql

SET enable_seqscan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1 WHERE property IS NOT NULL ORDER BY id;

-- test06IllegalonAO.sql

SET enable_seqscan = FALSE;

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

-- test07select.sql

SET enable_seqscan = FALSE;

SELECT id, owner, description, property, poli, bullseye FROM GistTable1
 WHERE property IS NOT NULL
 ORDER BY id;


--test08UniqueandPKey.sql

ALTER TABLE GistTable1 ADD CONSTRAINT pkey PRIMARY KEY (property); 
ALTER TABLE GistTable1 ADD CONSTRAINT pkey PRIMARY KEY (poli); 
ALTER TABLE GistTable1 ADD CONSTRAINT pkey PRIMARY KEY (bullseye); 

ALTER TABLE GistTable1 ADD CONSTRAINT uniq UNIQUE (property); 
ALTER TABLE GistTable1 ADD CONSTRAINT uniq UNIQUE (poli); 
ALTER TABLE GistTable1 ADD CONSTRAINT uniq UNIQUE (bullseye); 

CREATE UNIQUE INDEX ShouldNotExist ON GistTable1 USING GiST (property);
CREATE UNIQUE INDEX ShouldNotExist ON GistTable1 USING GiST (poli);
CREATE UNIQUE INDEX ShouldNotExist ON GistTable1 USING GiST (bullseye);

-- start_ignore
DROP TABLE IF EXISTS GistTable2;
-- end_ignore

-- Test whether geometric types can be part of a primary key.
CREATE TABLE GistTable2 (id INTEGER, property BOX)
 DISTRIBUTED BY (property);
CREATE TABLE GistTable2 (id INTEGER, poli POLYGON)
 DISTRIBUTED BY (poli);
CREATE TABLE GistTable2 (id INTEGER, bullseye CIRCLE)
 DISTRIBUTED BY (bullseye);

-- start_ignore
DROP TABLE IF EXISTS GistTable2;
-- end_ignore


-- test09NegativeTests.sql

CREATE INDEX ShouldNotExist ON GistTable1 USING GiST (id);
CREATE INDEX ShouldNotExist ON GistTable1 USING GiST (v);
CREATE INDEX ShouldNotExist ON GistTable1 USING GiST (t);
CREATE INDEX ShouldNotExist ON GistTable1 USING GiST (f);
CREATE INDEX ShouldNotExist ON GistTable1 USING GiST (p);
-- Try to create GiST indexes on a mix of geometric and 
-- non-geometric types.
CREATE INDEX ShouldNotExist ON GistTable1 USING GiST (c, f);

-- test10MultipleColumns.sql

SET enable_seqscan = FALSE;

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

-- test11WherePredicate.sql
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

-- We should be able to search the column that uses a geometric data type, 
-- and of course we should find the right rows.  We should be able to search 
-- using different "formats" (e.g. spacing) of the data, and in some cases 
-- even different "order" of the data (if the data is converted to a 
-- canonical form, as it is for the BOX data type and perhaps some other 
-- data types), as long as data in all of those formats should be converted 
-- to the same internal representation.
SELECT owner, property FROM GistTable1 
 WHERE property IS NULL;
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
--start_ignore
EXPLAIN
SELECT id, property FROM GistTable1 
 WHERE property IS NULL 
 ORDER BY id
 ;
--end_ignore
-- test12expression.sql

INSERT INTO GistTable1 (id, box1) VALUES (300, '( (1, 1), (2, 2) )');
INSERT INTO GistTable1 (id, box1) VALUES (301, '( (3, 4), (4, 5) )');
INSERT INTO GistTable1 (id, box1) VALUES (302, '( (2, 2), (20, 20) )');
INSERT INTO GistTable1 (id, box1) VALUES (304, '( (2, 2), (4, 4) )');

-- You can create an index on an expression:
CREATE INDEX exampleExpressionIndex on GistTable1 ((id * 2));

CREATE INDEX boxIndex1 ON GistTable1 USING Gist ((box1 * POINT '(2.0, 0)'));

SET enable_seqscan = FALSE;

-- We should be able to search the column that uses an expression that uses 
-- a geometric data type, and of course we should find the right rows.  

-- This uses the index, as it should.
SELECT id, box1 FROM GistTable1
 WHERE (box1 * POINT '(2.0, 0)' ) ~= (BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)');
--start_ignore
EXPLAIN SELECT id, box1 FROM GistTable1
 WHERE (box1 * POINT '(2.0, 0)' ) ~= (BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)');
--end_ignore
-- It seems to me that this could use the index, but it doesn't.  
-- The reason that I think this can use the index is that the index expression 
-- evaluates to a box, and the right-hand side of the WHERE clause below 
-- evaluates to a box, so the server should be able to search for the 
-- expression on the right-hand side by using the index.  
-- The fact that this doesn't use an index suggests that there's a bug!!!
SELECT id, box1 FROM GistTable1
 WHERE box1 ~= BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)';
--start_ignore
EXPLAIN SELECT id, box1 FROM GistTable1
 WHERE box1 ~= BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)';
--end_ignore
-- It seems to me that this could use the index, but it doesn't.
SELECT id, box1 FROM GistTable1
 WHERE (box1 * POINT '(2.0, 0)' ) ~= (BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)') 
  OR 
   box1 ~= (BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)')
   ;
--start_ignore
EXPLAIN SELECT id, box1 FROM GistTable1 WHERE (box1 * POINT '(2.0, 0)' ) ~= (BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)') OR box1 ~= (BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)');
--end_ignore
-- test13Vaccum.sql

-- start_ignore
DROP TABLE IF EXISTS GistTable13;
DROP FUNCTION IF EXISTS TO_BOX(TEXT) CASCADE;
DROP FUNCTION IF EXISTS insertIntoGistTable13(INTEGER);
DROP FUNCTION IF EXISTS insertManyIntoGistTable13(INTEGER, INTEGER);
-- end_ignore

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
  $$ LANGUAGE SQL CONTAINS SQL;

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
LANGUAGE PLPGSQL MODIFIES SQL DATA
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
LANGUAGE PLPGSQL MODIFIES SQL DATA
;

-- Add some rows before we create the index.
SELECT insertManyIntoGistTable13(1, 1000);

-- Create the index.
CREATE INDEX GistIndex13 ON GistTable13 USING GiST (property);
SET enable_seqscan = FALSE;

-- Add more rows after we create the index.
SELECT insertManyIntoGistTable13(1001, 2000);

-- Delete half the rows (the rows with even-numbered ids).
DELETE FROM GistTable13 WHERE id %2 = 0;

-- There should be about 1000 rows left.
SELECT COUNT(*) FROM GistTable13;
--start_ignore
ANALYZE GistTable13;
--end_ignore
-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values (coordinates), not just the 
-- AREA, are the same.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
--start_ignore
EXPLAIN SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
--end_ignore
--start_ignore
VACUUM GistTable13;
--end_ignore
--start_ignore
ANALYZE GistTable13;
--end_ignore
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
--start_ignore
EXPLAIN SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
--end_ignore
TRUNCATE TABLE GistTable13;

-- Add some rows.
SELECT insertManyIntoGistTable13(1, 1000);
--start_ignore
ANALYZE GistTable13;
--end_ignore
-- Note that "=" for geometric data types means equal AREA, NOT COORDINATES.
-- The "~=" operator means that the values are the same, not just the 
-- same AREA.
SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
--start_ignore
EXPLAIN SELECT id, property AS "ProperTee" FROM GistTable13 
 WHERE property ~= '( (999,999), (998,998) )';
--end_ignore

-- start_ignore
DROP TABLE IF EXISTS GistTable13;
-- end_ignore

-- test14Hash.sql

-- start_ignore
DROP TABLE IF EXISTS GistTable14;
-- end_ignore

CREATE TABLE GistTable14 (
 id INTEGER,
 property BOX
 )
 DISTRIBUTED BY (id);

-- Try to create a hash index.
CREATE INDEX GistIndex14a ON GistTable14 USING HASH (id);
CREATE INDEX GistIndex14b ON GistTable14 USING HASH (property);

-- start_ignore
DROP TABLE IF EXISTS GistTable14;
-- end_ignore




