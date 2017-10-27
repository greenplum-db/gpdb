create schema qp_dml_oids;
set search_path='qp_dml_oids';

DROP TABLE IF EXISTS dml_ao;
CREATE TABLE dml_ao (a int , b int default -1, c text) WITH (appendonly = true, oids = true) DISTRIBUTED BY (a);
INSERT INTO dml_ao VALUES(generate_series(1,2),generate_series(1,2),'r');

INSERT INTO dml_ao VALUES(NULL,NULL,NULL);

--
-- DDL on AO/CO tables with OIDS(Negative Test)
--
DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,a FROM dml_ao ORDER BY 1;
UPDATE dml_ao SET a = 100;
SELECT * FROM ( (SELECT COUNT(*) FROM dml_ao) UNION (SELECT COUNT(*) FROM tempoid, dml_ao WHERE tempoid.oid = dml_ao.oid AND tempoid.gp_segment_id = dml_ao.gp_segment_id))foo;

DROP TABLE IF EXISTS dml_heap_check_r;
CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105), 
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text, 
	d numeric NOT NULL) 
WITH OIDS DISTRIBUTED BY (a);

--
-- DML on table with constraints and OIDS(Negative Test)
--
INSERT INTO dml_heap_check_r SELECT i, i ,'r', i FROM generate_series(1,2)i;
SELECT SUM(a),SUM(b) FROM dml_heap_check_r;
SELECT COUNT(*) FROM dml_heap_check_r;

INSERT INTO dml_heap_check_r VALUES(DEFAULT,DEFAULT,'rn',0);
SELECT SUM(a),SUM(b) FROM dml_heap_check_r;
SELECT COUNT(*) FROM dml_heap_check_r;

INSERT INTO dml_heap_check_r VALUES(110,NULL,'rn',0);
SELECT SUM(a),SUM(b) FROM dml_heap_check_r;
SELECT COUNT(*) FROM dml_heap_check_r;

SELECT SUM(a) FROM dml_heap_check_r;
DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,a FROM dml_heap_check_r DISTRIBUTED BY (a);

UPDATE dml_heap_check_r set a = 110;
SELECT SUM(a) FROM dml_heap_check_r;
-- THIS SQL CONFIRMS THAT POST UPDATE THE OID OF THE TUPLE REMAINS THE SAME
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_check_r) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_check_r WHERE tempoid.oid = dml_heap_check_r.oid AND tempoid.gp_segment_id = dml_heap_check_r.gp_segment_id))foo;

DROP TABLE IF EXISTS dml_heap_r;
CREATE TABLE dml_heap_r (a int , b int default -1, c text) WITH OIDS DISTRIBUTED BY (a);
INSERT INTO dml_heap_r VALUES(generate_series(1,2),generate_series(1,2),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,NULL);
SELECT COUNT(*) FROM dml_heap_r;

--
-- DELETE on table with OIDS
--
SELECT SUM(a) FROM dml_heap_r;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,a FROM dml_heap_r DISTRIBUTED BY (a);

DELETE FROM dml_heap_r WHERE a is NULL;

SELECT SUM(a) FROM dml_heap_r;
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_r) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.gp_segment_id = dml_heap_r.gp_segment_id AND tempoid.a is NOT NULL))foo;


DROP TABLE IF EXISTS dml_heap_r;
CREATE TABLE dml_heap_r (col1 serial, a int , b int default -1, c text) WITH OIDS DISTRIBUTED BY (a);
INSERT INTO dml_heap_r(a,b,c) VALUES(generate_series(1,2),generate_series(1,2),'r');
INSERT INTO dml_heap_r(a,b,c) VALUES(NULL,NULL,NULL);
SELECT COUNT(*) FROM dml_heap_r;

--
-- UPDATE to constant value on table with OIDS
--
SELECT SUM(a) FROM dml_heap_r;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,a FROM dml_heap_r ORDER BY 1;

UPDATE dml_heap_r SET a = 1;

SELECT SUM(a) FROM dml_heap_r;
-- The query checks that the tuple oids remain the remain pre and post update .
-- SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1 is a join on the tuple oids before update and after update. If the oids remain the same the below query should return 1 row which is equivalent to the number of rows in the table
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_r) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1))foo;

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) WITH OIDS DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (col1 serial, a numeric, b decimal) WITH OIDS DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_p(a,b) SELECT id as a, id as b FROM (SELECT * FROM generate_series(1,2) as id) AS x;
INSERT INTO dml_heap_p(a,b) VALUES(NULL,NULL);

INSERT INTO dml_heap_r VALUES(generate_series(1,2),generate_series(1,2),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,NULL);

SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM dml_heap_p;

--
-- UPDATE with SELECT on table with OIDS
--
SELECT SUM(a), SUM(b) FROM dml_heap_p;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,a,b FROM dml_heap_p ORDER BY 1;

UPDATE dml_heap_p SET a = (SELECT a FROM dml_heap_r ORDER BY 1 LIMIT 1), b = ((SELECT b FROM dml_heap_r ORDER BY 1 LIMIT 1));

-- The query checks that the tuple oids remain the remain pre and post update .
-- SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1 is a join on the tuple oids before update and after update. If the oids remain the same the below query should return 1 row which is equivalent to the number of rows in the table
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_p) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_p WHERE tempoid.oid = dml_heap_p.oid AND tempoid.col1 = dml_heap_p.col1))foo;

SELECT SUM(a), SUM(b) FROM dml_heap_p;


DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (col1 serial,a int , b int default -1, c text) WITH OIDS DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (col1 serial, a numeric, b decimal) WITH OIDS DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_p(a,b) SELECT id as a, id as b FROM (SELECT * FROM generate_series(1,2) as id) AS x;
INSERT INTO dml_heap_p(a,b) VALUES(NULL,NULL);

INSERT INTO dml_heap_r(a,b,c) VALUES(generate_series(1,2),generate_series(1,2),'r');
INSERT INTO dml_heap_r(a,b,c) VALUES(NULL,NULL,NULL);

SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM dml_heap_p;

--
-- UPDATE with joins on table with OIDS
--
SELECT SUM(a) FROM dml_heap_r;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,a FROM dml_heap_r ORDER BY 1;

SELECT SUM(dml_heap_r.a) FROM dml_heap_p, dml_heap_r WHERE dml_heap_r.b = dml_heap_p.a;

UPDATE dml_heap_r SET a = dml_heap_r.a FROM dml_heap_p WHERE dml_heap_r.b = dml_heap_p.a;

-- The query checks that the tuple oids remain the remain pre and post update .
-- SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1 is a join on the tuple oids before update and after update. If the oids remain the same the below query should return 1 row which is equivalent to the number of rows in the table
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_r) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1))foo;

SELECT SUM(a) FROM dml_heap_r;

--
-- Check that a tuple gets an OID, even if it's toasted (there used to
-- be a bug, where toasting a tuple cleared its just-assigned OID)
--
INSERT INTO dml_ao (a, b, c) VALUES (10, 1, repeat('x', 50000));
INSERT INTO dml_ao (a, b, c) VALUES (10, 2, repeat('x', 50000));

SELECT COUNT(distinct oid) FROM dml_ao where a = 10;
