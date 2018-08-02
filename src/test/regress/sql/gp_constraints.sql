--
-- CONSTRAINTS
-- As of Postgres 9.2, the executor provides details in errors for offending
-- tuples when constraints are violated during an INSERT / UPDATE. However, we
-- are generally masking out these details (using matchsubs) in upstream tests
-- because failing tuples might land on multiple segments, and the the precise
-- error becomes time-sensitive and less predictable.
-- To preserve coverage, we test those error details here (with greater care).
--

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

--
-- Primary keys
--

CREATE TABLE PRIMARY_TBL (i int PRIMARY KEY, t text);
CREATE TEMP TABLE tmp (i int, t text);

INSERT INTO PRIMARY_TBL VALUES (1, 'one');
INSERT INTO PRIMARY_TBL VALUES (2, 'two');

INSERT INTO tmp VALUES (1, 'three');
INSERT INTO PRIMARY_TBL SELECT * FROM tmp;

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
