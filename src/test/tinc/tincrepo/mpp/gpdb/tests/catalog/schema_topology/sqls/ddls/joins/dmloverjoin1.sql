
-- start_ignore

-- end_ignore
-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Joins

-- start_ignore
DROP TABLE IF EXISTS update_test;
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS t2;
-- end_ignore

CREATE TABLE update_test (
	e INT DEFAULT 1,
	a INT DEFAULT 10,
	b INT,
	c TEXT
);

INSERT INTO update_test(a,b,c) VALUES (5, 10, 'foo');
INSERT INTO update_test(b,a) VALUES (15, 10);

SELECT a,b,c FROM update_test ORDER BY a,b;
UPDATE update_test SET a = DEFAULT, b = DEFAULT;
SELECT a,b,c FROM update_test ORDER BY a,c;

-- aliases for the UPDATE target table
UPDATE update_test AS t SET b = 10 WHERE t.a = 10;
SELECT a,b,c FROM update_test ORDER BY a,c;

UPDATE update_test t SET b = t.b + 10 WHERE t.a = 10;
SELECT a,b,c FROM update_test ORDER BY a,c;


UPDATE update_test SET a=v.i FROM (VALUES(100, 20)) AS v(i, j)
	WHERE update_test.b = v.j;
SELECT a,b,c FROM update_test ORDER BY a,c;


-- ----------------------------------------------
-- Create 2 tables with the same columns, but distributed differently.
CREATE TABLE t1 (id INTEGER, data1 INTEGER, data2 INTEGER) DISTRIBUTED BY (id);
CREATE TABLE t2 (id INTEGER, data1 INTEGER, data2 INTEGER) DISTRIBUTED BY (data1);

INSERT INTO t1 (id, data1, data2) VALUES (1, 1, 1);
INSERT INTO t1 (id, data1, data2) VALUES (2, 2, 2);
INSERT INTO t1 (id, data1, data2) VALUES (3, 3, 3);
INSERT INTO t1 (id, data1, data2) VALUES (4, 4, 4);

INSERT INTO t2 (id, data1, data2) VALUES (1, 2, 101);
INSERT INTO t2 (id, data1, data2) VALUES (2, 1, 102);
INSERT INTO t2 (id, data1, data2) VALUES (3, 4, 103);
INSERT INTO t2 (id, data1, data2) VALUES (4, 3, 104);

-- Now let's try an update that would require us to move info across segments 
-- (depending upon exactly where the data is stored, which will vary depending 
-- upon the number of segments; in my case, I used only 2 segments).
UPDATE t1 SET data2 = t2.data2 FROM t2 WHERE t1.data1 = t2.data1;

SELECT * from t1;


DROP TABLE IF EXISTS update_test;
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS t2;

