DROP DATABASE IF EXISTS testdb3;
CREATE DATABASE testdb3 WITH LC_COLLATE='C' LC_CTYPE='C' TEMPLATE=template0;
\c testdb3

CREATE TABLE hi_안녕세계 (a int, 안녕세계1 text, 안녕세계2 text, 안녕세계3 text) DISTRIBUTED BY (a);
-- DROP/ADD/RENAME columns
ALTER TABLE hi_안녕세계 DROP COLUMN 안녕세계2;
ALTER TABLE hi_안녕세계 ADD COLUMN 안녕세계2_ADD_COLUMN text;
ALTER TABLE hi_안녕세계 RENAME COLUMN 안녕세계3 TO 안녕세계3_RENAME;

INSERT INTO hi_안녕세계 VALUES(1, '안녕세계1 first', '안녕세2 first', '안녕세계3 first');
INSERT INTO hi_안녕세계 VALUES(42, '안녕세계1 second', '안녕세2 second', '안녕세계3 second');

SET optimizer_trace_fallback=on;

-- DELETE
EXPLAIN DELETE FROM hi_안녕세계 WHERE a=42;
DELETE FROM hi_안녕세계 WHERE a=42;

-- UPDATE
EXPLAIN UPDATE hi_안녕세계 SET 안녕세계1='안녕세계1 first UPDATE' WHERE 안녕세계1='안녕세계1 first';
UPDATE hi_안녕세계 SET 안녕세계1='안녕세계1 first UPDATE' WHERE 안녕세계1='안녕세계1 first';

-- SELECT
EXPLAIN SELECT * FROM hi_안녕세계;
SELECT * FROM hi_안녕세계;

-- SELECT ALIAS
EXPLAIN SELECT 안녕세계1 AS 안녕세계1_Alias FROM hi_안녕세계;
SELECT 안녕세계1 AS 안녕세계1_Alias FROM hi_안녕세계;

-- SUBQUERY
EXPLAIN SELECT * FROM (SELECT 안녕세계1 FROM hi_안녕세계) t;
SELECT * FROM (SELECT 안녕세계1 FROM hi_안녕세계) t;

-- CTE
EXPLAIN WITH cte AS
(SELECT 안녕세계1, 안녕세계2_ADD_COLUMN FROM hi_안녕세계) SELECT * FROM cte WHERE 안녕세계1 LIKE '안녕세계1%';
WITH cte AS
(SELECT 안녕세계1, 안녕세계2_ADD_COLUMN FROM hi_안녕세계) SELECT * FROM cte WHERE 안녕세계1 LIKE '안녕세계1%';

-- JOIN
EXPLAIN SELECT * FROM hi_안녕세계 hi_안녕세계1, hi_안녕세계 hi_안녕세계2 WHERE hi_안녕세계1.안녕세계1 LIKE '%UPDATE';
SELECT * FROM hi_안녕세계 hi_안녕세계1, hi_안녕세계 hi_안녕세계2 WHERE hi_안녕세계1.안녕세계1 LIKE '%UPDATE';
