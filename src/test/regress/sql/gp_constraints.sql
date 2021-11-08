--
-- CONSTRAINTS
-- As of Postgres 9.2, the executor provides details in errors for offending
-- tuples when constraints are violated during an INSERT / UPDATE. However, we
-- are generally masking out these details (using matchsubs) in upstream tests
-- because failing tuples might land on multiple segments, and the precise
-- error becomes time-sensitive and less predictable.
-- To preserve coverage, we test those error details here (with greater care).
--
-- start_matchsubs
-- m/^LOCATION:.*:\d+/
-- s/:\d+/:/
-- end_matchsubs

CREATE SCHEMA gpdb_insert;
SET search_path TO gpdb_insert;

--
-- CHECK syntax
--

CREATE TABLE CHECK_TBL (x int,
	CONSTRAINT CHECK_CON CHECK (x > 3));

INSERT INTO CHECK_TBL VALUES (5);
INSERT INTO CHECK_TBL VALUES (4);
INSERT INTO CHECK_TBL VALUES (3);
INSERT INTO CHECK_TBL VALUES (2);
INSERT INTO CHECK_TBL VALUES (6);
INSERT INTO CHECK_TBL VALUES (1);

SELECT '' AS three, * FROM CHECK_TBL;


CREATE TABLE CHECK_TBL1 (x int,
	CONSTRAINT CHECK_CON CHECK ((x > 3) IS TRUE));

INSERT INTO CHECK_TBL1 VALUES (5);
INSERT INTO CHECK_TBL1 VALUES (4);
INSERT INTO CHECK_TBL1 VALUES (3);
INSERT INTO CHECK_TBL1 VALUES (2);
INSERT INTO CHECK_TBL1 VALUES (6);
INSERT INTO CHECK_TBL1 VALUES (1);

SELECT '' AS three, * FROM CHECK_TBL1;

--
-- Primary keys
--

CREATE TABLE PRIMARY_TBL (i int PRIMARY KEY, t text);
CREATE TEMP TABLE tmp (i int, t text);

INSERT INTO PRIMARY_TBL VALUES (1, 'one');
INSERT INTO PRIMARY_TBL VALUES (2, 'two');

INSERT INTO tmp VALUES (1, 'three');
INSERT INTO PRIMARY_TBL SELECT * FROM tmp;
-- database object names as separate fields in error messages can be shown when VERBOSITY set to verbose
\set VERBOSITY verbose
INSERT INTO PRIMARY_TBL SELECT * FROM tmp;
\set VERBOSITY default

SELECT '' AS four, * FROM PRIMARY_TBL;

INSERT INTO PRIMARY_TBL VALUES (4, 'four');
INSERT INTO PRIMARY_TBL VALUES (5, 'five');

DELETE FROM tmp;
INSERT INTO tmp (t) VALUES ('six');
INSERT INTO PRIMARY_TBL SELECT * FROM tmp;

SELECT '' AS four, * FROM PRIMARY_TBL;

DROP TABLE PRIMARY_TBL;

--
-- composite unique keys
--

CREATE TABLE UNIQUE_TBL (i int, t text,
	UNIQUE(i,t));

INSERT INTO UNIQUE_TBL VALUES (1, 'one');
INSERT INTO UNIQUE_TBL VALUES (2, 'two');
INSERT INTO UNIQUE_TBL VALUES (1, 'three');
INSERT INTO UNIQUE_TBL VALUES (1, 'one');
INSERT INTO UNIQUE_TBL VALUES (5, 'one');
INSERT INTO UNIQUE_TBL (t) VALUES ('six');

SELECT '' AS five, * FROM UNIQUE_TBL;

DROP TABLE UNIQUE_TBL;

--
-- Test foreign key constraints
--
BEGIN;
-- Test with two heap tables
CREATE TABLE fkc_primary_table1(a int PRIMARY KEY, b text) DISTRIBUTED BY (a);
CREATE TABLE fkc_foreign_table1(a int REFERENCES fkc_primary_table1 ON DELETE RESTRICT ON UPDATE RESTRICT, b text) DISTRIBUTED BY (a);
-- the following should succeed
INSERT INTO fkc_primary_table1 VALUES (1, 'bar');
INSERT INTO fkc_primary_table1 VALUES (2, 'bar');
INSERT INTO fkc_foreign_table1 VALUES (1, 'bar');
INSERT INTO fkc_foreign_table1 VALUES (2, 'bar');
UPDATE fkc_foreign_table1 SET b = 'foo';
DELETE FROM fkc_primary_table1 WHERE a = 1;
COMMIT;

BEGIN;
-- Test with an ao table and heap table
CREATE TABLE fkc_primary_table2(a int PRIMARY KEY, b text) DISTRIBUTED BY (a);
CREATE TABLE fkc_foreign_table2(a int REFERENCES fkc_primary_table2 ON DELETE RESTRICT ON UPDATE RESTRICT,
                                b text) WITH (APPENDONLY=TRUE) DISTRIBUTED BY (a);
-- the following should succeed
INSERT INTO fkc_primary_table2 VALUES (1, 'bar');
INSERT INTO fkc_primary_table2 VALUES (2, 'bar');
INSERT INTO fkc_foreign_table2 VALUES (1, 'bar');
INSERT INTO fkc_foreign_table2 VALUES (2, 'bar');
UPDATE fkc_foreign_table2 SET b = 'foo';
DELETE FROM fkc_primary_table2 WHERE a = 1;
COMMIT;
