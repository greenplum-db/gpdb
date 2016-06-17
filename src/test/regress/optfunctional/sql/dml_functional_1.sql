
-- start_ignore
create schema functional_1_75;
set search_path to functional_1_75;
-- end_ignore
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
DROP TABLE IF EXISTS dml_pt_tab;
CREATE TABLE dml_pt_tab (
	i int,
	x text,
	c char,
	v varchar,
	d date,
	n numeric,
	t timestamp without time zone,
	tz time with time zone
) distributed by (c)
partition by range (i)
(
 partition p1 start(1) end(5),
 partition p2 start(5) end(15),
 partition p3 start(15) end(25)
);
INSERT INTO dml_pt_tab VALUES(generate_series(1,10),'dml_pt_tab','a','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Alter table add/drop partition



-- ALTER TABLE ADD PARTITION
SELECT COUNT(*) FROM dml_pt_tab;
ALTER TABLE dml_pt_tab ADD PARTITION padd start(61) end(71) ;
INSERT INTO dml_pt_tab VALUES(generate_series(61,70),'dml_pt_tab','u','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab;
UPDATE dml_pt_tab SET x = 'split partition';
SELECT DISTINCT(x) FROM dml_pt_tab;

-- DROP PARTITION( DML should fail )
ALTER TABLE dml_pt_tab DROP PARTITION padd;
INSERT INTO dml_pt_tab VALUES(generate_series(61,71),'dml_pt_tab','u','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_pt_tab;
CREATE TABLE dml_pt_tab (
	i int,
	x text,
	c char,
	v varchar,
	d date,
	n numeric,
	t timestamp without time zone,
	tz time with time zone
) distributed by (c)
partition by range (i)
(
 partition p1 start(1) end(5),
 partition p2 start(5) end(15),
 partition p3 start(15) end(25)
);
INSERT INTO dml_pt_tab VALUES(generate_series(1,10),'dml_pt_tab','a','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Alter table alter column




-- ADD COLUMN
SELECT COUNT(*) FROM dml_pt_tab;
ALTER TABLE dml_pt_tab add column addcol1 decimal default 10.00 ;
SELECT COUNT(*) FROM dml_pt_tab WHERE addcol1 = 10.00;
UPDATE dml_pt_tab SET addcol1 = 1.00;
SELECT COUNT(*) FROM dml_pt_tab WHERE addcol1 = 1.00;
INSERT INTO dml_pt_tab VALUES(generate_series(11,12),'dml_pt_tab','b','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab WHERE addcol1 = 1.00;

-- CHANGE COLUMN TYPE
SELECT COUNT(*) FROM dml_pt_tab;
ALTER TABLE dml_pt_tab ALTER COLUMN addcol1 type numeric ;
INSERT INTO dml_pt_tab VALUES(generate_series(1,12),'dml_pt_tab','c','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02', 10.10);
SELECT COUNT(*) FROM dml_pt_tab;

-- RENAME COLUMN
ALTER TABLE dml_pt_tab RENAME COLUMN addcol1 to newaddcol1 ;
UPDATE dml_pt_tab SET newaddcol1 = 1.11 , i = 1;
SELECT COUNT(*) FROM dml_pt_tab WHERE i = 1;

-- DROP COLUMN
ALTER TABLE dml_pt_tab DROP COLUMN newaddcol1;
SELECT COUNT(*) FROM dml_pt_tab;
INSERT INTO dml_pt_tab VALUES(generate_series(1,12),'dml_pt_tab','e','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab;


-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_pt_tab;
CREATE TABLE dml_pt_tab (
	i int,
	x text,
	c char,
	v varchar,
	d date,
	n numeric,
	t timestamp without time zone,
	tz time with time zone
) distributed by (c)
partition by range (i)
(
 partition p1 start(1) end(5),
 partition p2 start(5) end(15),
 partition p3 start(15) end(25)
);
INSERT INTO dml_pt_tab VALUES(generate_series(1,10),'dml_pt_tab','a','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Alter table set/drop default values




-- SET DEFAULT
SELECT COUNT(*) FROM dml_pt_tab WHERE x = 'abcdefghijklmnopqrstuvwxyz';
ALTER TABLE dml_pt_tab ALTER COLUMN x SET DEFAULT 'abcdefghijklmnopqrstuvwxyz';
INSERT INTO dml_pt_tab(i,c,v,d,n,t,tz) VALUES(13,'d','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab WHERE x = 'abcdefghijklmnopqrstuvwxyz';

-- DROP DEFAULT
SELECT COUNT(*) FROM dml_pt_tab;
ALTER TABLE dml_pt_tab ALTER COLUMN x DROP DEFAULT;
INSERT INTO dml_pt_tab(i,c,v,d,n,t,tz) VALUES(13,'i','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab;


-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_pt_tab;
CREATE TABLE dml_pt_tab (
	i int,
	x text,
	c char,
	v varchar,
	d date,
	n numeric,
	t timestamp without time zone,
	tz time with time zone
) distributed by (c)
partition by range (i)
(
 partition p1 start(1) end(5),
 partition p2 start(5) end(15),
 partition p3 start(15) end(25)
);
INSERT INTO dml_pt_tab VALUES(generate_series(1,10),'dml_pt_tab','a','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Alter default partition




-- ADD DEFAULT PARTITION
SELECT COUNT(*) FROM dml_pt_tab;
ALTER TABLE dml_pt_tab ADD DEFAULT PARTITION def_part;
INSERT INTO dml_pt_tab VALUES(NULL,'dml_pt_tab','q','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab;
UPDATE dml_pt_tab SET i = NULL WHERE c = 'a';
SELECT DISTINCT(i) FROM dml_pt_tab WHERE c = 'a';
SELECT COUNT(*) FROM dml_pt_tab_1_prt_def_part;

-- SPLIT DEFAULT PARTITION
ALTER TABLE dml_pt_tab SPLIT DEFAULT PARTITION start(41) end(51) into (partition p5, partition def_part);
INSERT INTO dml_pt_tab VALUES(NULL,'dml_pt_tab','r','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
INSERT INTO dml_pt_tab VALUES(generate_series(40,50),'dml_pt_tab','s','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab_1_prt_def_part;
SELECT COUNT(*) FROM dml_pt_tab_1_prt_p5;
SELECT COUNT(*) FROM dml_pt_tab;

-- DROP DEFAULT PARTITION
ALTER TABLE dml_pt_tab DROP DEFAULT PARTITION ;
UPDATE dml_pt_tab SET i = NULL;
SELECT DISTINCT(i) FROM dml_pt_tab;
SELECT COUNT(*) FROM dml_pt_tab WHERE i is NULL;
DELETE FROM dml_pt_tab WHERE i is NULL;
SELECT COUNT(*) FROM dml_pt_tab WHERE i is NULL;


-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_pt_tab;
CREATE TABLE dml_pt_tab (
	i int,
	x text,
	c char,
	v varchar,
	d date,
	n numeric,
	t timestamp without time zone,
	tz time with time zone
) distributed by (c)
partition by range (i)
(
 partition p1 start(1) end(5),
 partition p2 start(5) end(15),
 partition p3 start(15) end(25)
);
INSERT INTO dml_pt_tab VALUES(generate_series(1,10),'dml_pt_tab','a','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Alter table exchange partition




-- ALTER TABLE EXCHANGE PARTITION
SELECT COUNT(*) FROM dml_pt_tab;
-- start_ignore
DROP TABLE IF EXISTS dml_pt_can_tab;
CREATE TABLE dml_pt_can_tab( like dml_pt_tab) distributed by (c);
INSERT INTO dml_pt_can_tab VALUES(generate_series(1,4),'dml_pt_can_tab','t','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
-- end_ignore
SELECT COUNT(*) FROM dml_pt_can_tab;

ALTER TABLE dml_pt_tab exchange partition p1 with table dml_pt_can_tab;
SELECT COUNT(*) FROM dml_pt_can_tab;
SELECT COUNT(*) FROM dml_pt_tab;

INSERT INTO dml_pt_can_tab VALUES(generate_series(1,4),'dml_pt_can_tab','t','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
INSERT INTO dml_pt_tab VALUES(generate_series(1,15),'dml_pt_tab','u','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');

SELECT COUNT(*) FROM dml_pt_can_tab;
SELECT COUNT(*) FROM dml_pt_tab;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_pt_tab;
CREATE TABLE dml_pt_tab (
	i int,
	x text,
	c char,
	v varchar,
	d date,
	n numeric,
	t timestamp without time zone,
	tz time with time zone
) distributed by (c)
partition by range (i)
(
 partition p1 start(1) end(5),
 partition p2 start(5) end(15),
 partition p3 start(15) end(25)
);
INSERT INTO dml_pt_tab VALUES(generate_series(1,10),'dml_pt_tab','a','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Alter table alter index




-- CREATE INDEX
SELECT COUNT(*) FROM dml_pt_tab;
CREATE INDEX dml_pt_tab_idx1 on dml_pt_tab (i);
INSERT INTO dml_pt_tab VALUES(generate_series(12,22),'dml_pt_tab','j','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab;

CREATE INDEX dml_pt_tab_idx2 on dml_pt_tab using bitmap (n);
INSERT INTO dml_pt_tab VALUES(generate_series(12,22),'dml_pt_tab','k','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
INSERT INTO dml_pt_tab VALUES(generate_series(12,22),'dml_pt_tab','l','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab;

DELETE FROM dml_pt_tab;
SELECT COUNT(*) FROM dml_pt_tab;
ALTER INDEX dml_pt_tab_idx1 RENAME TO dml_pt_tab_idx1_new;
ALTER INDEX dml_pt_tab_idx2 RENAME TO dml_pt_tab_idx2_new;

INSERT INTO dml_pt_tab VALUES(generate_series(12,22),'dml_pt_tab','m','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_pt_tab;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_tab;
CREATE TABLE dml_tab (
	i int,
	x text,
	c char,
	v varchar,
	d date,
	n numeric,
	t timestamp without time zone,
	tz time with time zone
) distributed by (c);
INSERT INTO dml_tab VALUES(generate_series(1,10),'dml_tab','a','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Alter table alter column




-- ADD COLUMN
SELECT COUNT(*) FROM dml_tab;
ALTER TABLE dml_tab add column addcol1 decimal default 10.00 ;
SELECT COUNT(*) FROM dml_tab WHERE addcol1 = 10.00;
UPDATE dml_tab SET addcol1 = 1.00;
SELECT COUNT(*) FROM dml_tab WHERE addcol1 = 1.00;
INSERT INTO dml_tab VALUES(generate_series(11,12),'dml_tab','b','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_tab;

-- CHANGE COLUMN DATA TYPE
SELECT COUNT(*) FROM dml_tab;
ALTER TABLE dml_tab ALTER COLUMN addcol1 type numeric ;
INSERT INTO dml_tab VALUES(generate_series(1,12),'dml_tab','c','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02', 10.10);
SELECT COUNT(*) FROM dml_tab;

-- RENAME COLUMN

ALTER TABLE dml_tab RENAME COLUMN addcol1 to newaddcol1 ;
UPDATE dml_tab SET newaddcol1 = 1.11 , i = 1;
SELECT COUNT(*) FROM dml_tab WHERE i = 1;

-- DROP COLUMN
DELETE FROM dml_tab;
SELECT COUNT(*) FROM dml_tab;
ALTER TABLE dml_tab DROP COLUMN newaddcol1;
INSERT INTO dml_tab VALUES(generate_series(1,12),'dml_tab','e','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_tab;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_tab;
CREATE TABLE dml_tab (
        i int,
        x text,
        c char,
        v varchar,
        d date,
        n numeric,
        t timestamp without time zone,
        tz time with time zone
) distributed by (c);
INSERT INTO dml_tab VALUES(generate_series(1,10),'dml_tab','a','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Alter table set/drop default values




-- SET DEFAULT
SELECT COUNT(*) FROM dml_tab WHERE x = 'abcdefghijklmnopqrstuvwxyz';
ALTER TABLE dml_tab ALTER COLUMN x SET DEFAULT 'abcdefghijklmnopqrstuvwxyz';
INSERT INTO dml_tab(i,c,v,d,n,t,tz) VALUES(13,'d','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_tab WHERE x = 'abcdefghijklmnopqrstuvwxyz';

-- DROP DEFAULT
SELECT COUNT(*) FROM dml_tab;
ALTER TABLE dml_tab ALTER COLUMN x DROP DEFAULT;
INSERT INTO dml_tab(i,c,v,d,n,t,tz) VALUES(13,'i','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_tab;


-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_tab;
CREATE TABLE dml_tab (
        i int,
        x text,
        c char,
        v varchar,
        d date,
        n numeric,
        t timestamp without time zone,
        tz time with time zone
) distributed by (c);
INSERT INTO dml_tab VALUES(generate_series(1,10),'dml_tab','a','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');

-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Alter table alter column




-- CREATE INDEX
SELECT COUNT(*) FROM dml_tab;
CREATE INDEX dml_tab_idx1 on dml_tab (i);
INSERT INTO dml_tab VALUES(generate_series(12,22),'dml_tab','j','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_tab;

CREATE INDEX dml_tab_idx2 on dml_tab using bitmap (n);
INSERT INTO dml_tab VALUES(generate_series(12,22),'dml_tab','k','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_tab;
UPDATE dml_tab SET i = 1 WHERE c ='k';
SELECT COUNT(DISTINCT(i)) FROM dml_tab WHERE c='k';


-- ALTER INDEX
DELETE FROM dml_tab;
SELECT COUNT(*) FROM dml_tab;
ALTER INDEX dml_tab_idx1 RENAME TO dml_tab_idx1_new;
ALTER INDEX dml_tab_idx2 RENAME TO dml_tab_idx2_new;
INSERT INTO dml_tab VALUES(generate_series(12,22),'dml_tab','m','alter operations','01-01-2013',1,'01-01-2013 10:10:00', '01-01-2013 10:10:54+02');
SELECT COUNT(*) FROM dml_tab;
DELETE FROM dml_tab;
SELECT COUNT(*) FROM dml_tab;

-- start_ignore

DROP TABLE IF EXISTS dml_ao;
CREATE TABLE dml_ao (a int , b int default -1, c text) WITH (appendonly = true, oids = true) DISTRIBUTED BY (a);
INSERT INTO dml_ao VALUES(generate_series(1,2),generate_series(1,2),'r');
SELECT SUM(a),SUM(b) FROM dml_ao;
SELECT COUNT(*) FROM dml_ao;

INSERT INTO dml_ao VALUES(NULL,NULL,NULL);
SELECT SUM(a),SUM(b) FROM dml_ao;
SELECT COUNT(*) FROM dml_ao;

-- end_ignore
-- @author prabhd
-- @created 2013-07-08 12:00:00
-- @modified 2013-07-08 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description DDL on AO/CO tables with OIDS(Negative Test)


-- start_ignore
DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,a FROM dml_ao ORDER BY 1;
-- end_ignore
UPDATE dml_ao SET a = 100;
SELECT * FROM ( (SELECT COUNT(*) FROM dml_ao) UNION (SELECT COUNT(*) FROM tempoid, dml_ao WHERE tempoid.oid = dml_ao.oid AND tempoid.gp_segment_id = dml_ao.gp_segment_id))foo;

-- start_ignore
DROP TABLE IF EXISTS dml_heap_check_r;
CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105), 
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text, 
	d numeric NOT NULL) 
WITH OIDS DISTRIBUTED BY (a);
-- end_ignore

-- start_ignore
DROP TABLE IF EXISTS dml_heap_r;
CREATE TABLE dml_heap_r (a int , b int default -1, c text) WITH OIDS DISTRIBUTED BY (a);
INSERT INTO dml_heap_r VALUES(generate_series(1,2),generate_series(1,2),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,NULL);
SELECT COUNT(*) FROM dml_heap_r;INSERT INTO dml_heap_r VALUES(generate_series(1,2),generate_series(1,2),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,NULL);
SELECT COUNT(*) FROM dml_heap_r;
-- end_ignore

-- start_ignore

DROP TABLE IF EXISTS dml_heap_r;
CREATE TABLE dml_heap_r (col1 serial, a int , b int default -1, c text) WITH OIDS DISTRIBUTED BY (a);
INSERT INTO dml_heap_r(a,b,c) VALUES(generate_series(1,2),generate_series(1,2),'r');
INSERT INTO dml_heap_r(a,b,c) VALUES(NULL,NULL,NULL);
SELECT COUNT(*) FROM dml_heap_r;
-- end_ignore
-- @author prabhd
-- @created 2013-07-08 12:00:00
-- @modified 2013-07-08 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description UPDATE to constant value on table with OIDS




SELECT SUM(a) FROM dml_heap_r;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,a FROM dml_heap_r ORDER BY 1;

UPDATE dml_heap_r SET a = 1;

SELECT SUM(a) FROM dml_heap_r;
-- The query checks that the tuple oids remain the remain pre and post update .
-- SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1 is a join on the tuple oids before update and after update. If the oids remain the same the below query should return 1 row which is equivalent to the number of rows in the table
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_r) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1))foo;

-- start_ignore

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

-- end_ignore
-- @author prabhd
-- @created 2013-07-08 12:00:00
-- @modified 2013-07-08 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description UPDATE with SELECT on table with OIDS




SELECT SUM(a), SUM(b) FROM dml_heap_p;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,a,b FROM dml_heap_p ORDER BY 1;

UPDATE dml_heap_p SET a = (SELECT a FROM dml_heap_r ORDER BY 1 LIMIT 1), b = ((SELECT b FROM dml_heap_r ORDER BY 1 LIMIT 1));

-- The query checks that the tuple oids remain the remain pre and post update .
-- SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1 is a join on the tuple oids before update and after update. If the oids remain the same the below query should return 1 row which is equivalent to the number of rows in the table
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_p) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_p WHERE tempoid.oid = dml_heap_p.oid AND tempoid.col1 = dml_heap_p.col1))foo;

SELECT SUM(a), SUM(b) FROM dml_heap_p;
-- start_ignore

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

-- end_ignore
-- @author prabhd
-- @created 2013-07-08 12:00:00
-- @modified 2013-07-08 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description UPDATE with joins on table with OIDS




SELECT SUM(a) FROM dml_heap_r;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,a FROM dml_heap_r ORDER BY 1;

SELECT SUM(dml_heap_r.a) FROM dml_heap_p, dml_heap_r WHERE dml_heap_r.b = dml_heap_p.a;

UPDATE dml_heap_r SET a = dml_heap_r.a FROM dml_heap_p WHERE dml_heap_r.b = dml_heap_p.a;

-- The query checks that the tuple oids remain the remain pre and post update .
-- SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1 is a join on the tuple oids before update and after update. If the oids remain the same the below query should return 1 row which is equivalent to the number of rows in the table
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_r) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1))foo;

SELECT SUM(a) FROM dml_heap_r;
-- start_ignore
DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int check (a>0) , b int, c int, d int, CHECK (a+b>5)) DISTRIBUTED BY (a);
-- end_ignore
-- @author prabhd
-- @created 2014-04-01 12:00:00
-- @tags dml ORCA
-- @product_version gpdb: [4.3-]
-- @gpopt 1.524 
-- @gucs client_min_messages='log'
-- @optimizer_mode on
-- @description GUC to disable DML in Orca in the presence of check or not null constraints

-- start_ignore
set optimizer_dml_constraints=off;
explain insert into constr_tab values (1,2,3);
-- end_ignore

\!grep Planner %MYD%/output/fallback_dml_constraint_1_orca.out|grep -v grep | uniq
-- start_ignore
DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int check (a>0) , b int, c int, d int, CHECK (a+b>5)) DISTRIBUTED BY (a);
-- end_ignore
-- @author prabhd
-- @created 2014-04-01 12:00:00
-- @tags dml ORCA
-- @product_version gpdb: [4.3-]
-- @gucs client_min_messages='log'
-- @gpopt 1.524 
-- @optimizer_mode on
-- @description GUC to disable DML in Orca in the presence of check or not null constraints

-- start_ignore
set optimizer_dml_constraints=on;
explain insert into constr_tab values (1,2,3);
-- end_ignore

\!grep Planner %MYD%/output/fallback_dml_constraint_2_orca.out|grep -v grep | uniq
-- start_ignore
DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int check (a>0) , b int, c int, d int, CHECK (a+b>5)) DISTRIBUTED BY (a);
INSERT INTO constr_tab VALUES(1,5,3,4);
-- end_ignore
-- @author prabhd
-- @created 2014-04-01 12:00:00
-- @tags dml ORCA
-- @product_version gpdb: [4.3-]
-- @gpopt 1.524 
-- @gucs client_min_messages='log'
-- @optimizer_mode on
-- @description GUC to disable DML in Orca in the presence of check or not null constraints

-- start_ignore
set optimizer_dml_constraints=off;
explain update constr_tab set a = 10;
-- end_ignore

\!grep Planner %MYD%/output/fallback_dml_constraint_3_orca.out|grep -v grep | uniq
-- start_ignore
DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int check (a>0) , b int, c int, d int, CHECK (a+b>5)) DISTRIBUTED BY (a);
INSERT INTO constr_tab VALUES(1,5,3,4);
-- end_ignore
-- @author prabhd
-- @created 2014-04-01 12:00:00
-- @tags dml ORCA
-- @product_version gpdb: [4.3-]
-- @gpopt 1.524 
-- @gucs client_min_messages='log'
-- @optimizer_mode on
-- @description GUC to disable DML in Orca in the presence of check or not null constraints

-- start_ignore
set optimizer_dml_constraints=off;
explain update constr_tab set b = 10;
-- end_ignore

\!grep Planner %MYD%/output/fallback_dml_constraint_4_orca.out|grep -v grep | uniq
-- start_ignore
DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int check (a>0) , b int, c int, d int, CHECK (a+b>5)) DISTRIBUTED BY (a);
INSERT INTO constr_tab VALUES(1,5,3,4);
-- end_ignore
-- @author prabhd
-- @created 2014-04-01 12:00:00
-- @tags dml ORCA
-- @product_version gpdb: [4.3-]
-- @gpopt 1.524 
-- @gucs client_min_messages='log'
-- @optimizer_mode on
-- @description GUC to disable DML in Orca in the presence of check or not null constraints

-- start_ignore
set optimizer_dml_constraints=on;
explain update constr_tab set b = 10;
-- end_ignore

\!grep Planner %MYD%/output/fallback_dml_constraint_5_orca.out|grep -v grep | uniq
-- start_ignore
DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int NOT NULL, b int, c int, d int, CHECK (a+b>5)) DISTRIBUTED BY (a);
INSERT INTO constr_tab VALUES(1,5,3,4);
-- end_ignore
-- @author prabhd
-- @created 2014-04-01 12:00:00
-- @tags dml ORCA
-- @product_version gpdb: [4.3-]
-- @gpopt 1.524 
-- @gucs client_min_messages='log'
-- @optimizer_mode on
-- @description GUC to disable DML in Orca in the presence of check or not null constraints

-- start_ignore
set optimizer_dml_constraints=off;
explain update constr_tab set a = 10;
-- end_ignore

\!grep Planner %MYD%/output/fallback_dml_constraint_6_orca.out|grep -v grep | uniq
-- start_ignore
DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int NOT NULL, b int NOT NULL, c int NOT NULL, d int NOT NULL) DISTRIBUTED BY (a,b);
INSERT INTO constr_tab VALUES(1,5,3,4);
INSERT INTO constr_tab VALUES(1,5,3,4);
-- end_ignore
-- @author prabhd
-- @created 2014-04-01 12:00:00
-- @tags dml ORCA
-- @product_version gpdb: [4.3-]
-- @gpopt 1.524 
-- @gucs client_min_messages='log'
-- @optimizer_mode on
-- @description GUC to disable DML in Orca in the presence of check or not null constraints

-- start_ignore
set optimizer_dml_constraints=off;
explain update constr_tab set b = 10;
-- end_ignore

\!grep Planner %MYD%/output/fallback_dml_constraint_7_orca.out|grep -v grep | uniq
-- start_ignore
DROP TABLE IF EXISTS constr_tab;
CREATE TABLE constr_tab ( a int, b int, c int, d int) DISTRIBUTED BY (a);
INSERT INTO constr_tab VALUES(1,5,3,4);
INSERT INTO constr_tab VALUES(1,5,3,4);
-- end_ignore
-- @author prabhd
-- @created 2014-04-01 12:00:00
-- @tags dml ORCA
-- @product_version gpdb: [4.3-]
-- @gpopt 1.524 
-- @gucs client_min_messages='log'
-- @optimizer_mode on
-- @description GUC to disable DML in Orca in the presence of check or not null constraints

-- start_ignore
set optimizer_dml_constraints=off;
explain update constr_tab set a = 10;
-- end_ignore

\!grep Planner %MYD%/output/fallback_dml_constraint_8_orca.out|grep -v grep | uniq
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb


DROP TABLE IF EXISTS dml_ao_check_p;
DROP TABLE IF EXISTS dml_ao_check_r;
DROP TABLE IF EXISTS dml_ao_check_s;

CREATE TABLE dml_ao_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);


CREATE TABLE dml_ao_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_ao_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);


INSERT INTO dml_ao_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_ao_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_ao_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb
-- @execute_all_plans True
-- @description test1: Insert data that satisfy the check constraints 



SELECT COUNT(*) FROM dml_ao_check_s;
SELECT COUNT(*) FROM (SELECT dml_ao_check_s.a, dml_ao_check_s.b, 'text', dml_ao_check_s.d FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.b)foo;
INSERT INTO dml_ao_check_s SELECT dml_ao_check_s.a, dml_ao_check_s.b, 'text', dml_ao_check_s.d FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.b ;
SELECT COUNT(*) FROM dml_ao_check_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb


DROP TABLE IF EXISTS dml_ao_check_p;
DROP TABLE IF EXISTS dml_ao_check_r;
DROP TABLE IF EXISTS dml_ao_check_s;

CREATE TABLE dml_ao_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);


CREATE TABLE dml_ao_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_ao_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);


INSERT INTO dml_ao_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_ao_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_ao_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test2: Negative test - Insert default value violates check constraint



SELECT COUNT(*) FROM dml_ao_check_p;
INSERT INTO dml_ao_check_p select generate_series(1,100),'p', generate_series(1,100);
SELECT COUNT(*) FROM dml_ao_check_p;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb


DROP TABLE IF EXISTS dml_ao_check_p;
DROP TABLE IF EXISTS dml_ao_check_r;
DROP TABLE IF EXISTS dml_ao_check_s;

CREATE TABLE dml_ao_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);


CREATE TABLE dml_ao_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_ao_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);


INSERT INTO dml_ao_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_ao_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_ao_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb
-- @description test3: Negative test - Insert default value violates NOT NULL constraint



SELECT COUNT(*) FROM dml_ao_check_s;
INSERT INTO dml_ao_check_s values(default,1,'nn',1.0000);
SELECT COUNT(*) FROM dml_ao_check_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb


DROP TABLE IF EXISTS dml_ao_check_p;
DROP TABLE IF EXISTS dml_ao_check_r;
DROP TABLE IF EXISTS dml_ao_check_s;

CREATE TABLE dml_ao_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);


CREATE TABLE dml_ao_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_ao_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);


INSERT INTO dml_ao_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_ao_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_ao_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test4: Negative test - Insert with joins where the result tuples violate the user defined check constraint



SELECT COUNT(*) FROM dml_ao_check_r;
SELECT COUNT(*) FROM (SELECT dml_ao_check_r.a + 110 , dml_ao_check_r.b, dml_ao_check_r.c, dml_ao_check_r.d FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.a)foo;
INSERT INTO dml_ao_check_r SELECT dml_ao_check_r.a + 110 , dml_ao_check_r.b, dml_ao_check_r.c, dml_ao_check_r.d FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.a;
SELECT COUNT(*) FROM dml_ao_check_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb


DROP TABLE IF EXISTS dml_ao_check_p;
DROP TABLE IF EXISTS dml_ao_check_r;
DROP TABLE IF EXISTS dml_ao_check_s;

CREATE TABLE dml_ao_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);


CREATE TABLE dml_ao_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_ao_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);


INSERT INTO dml_ao_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_ao_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_ao_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_ao_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test5: Insert with joins where the result tuples violate violates multiple check constraints



SELECT COUNT(*) FROM dml_ao_check_r;
SELECT COUNT(*) FROM (SELECT dml_ao_check_r.a + 110 , 0, dml_ao_check_r.c, NULL FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.a)foo;
INSERT INTO dml_ao_check_r SELECT dml_ao_check_r.a + 110 , 0, dml_ao_check_r.c, NULL FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.a;
SELECT COUNT(*) FROM dml_ao_check_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test1: Negative test - Constant tuple Inserts( no partition for partitioning key )



SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r values(10);
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test10: Join on the distribution key of target table



SELECT COUNT(*) FROM dml_ao_pt_s;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_s.* FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_s.a = dml_ao_pt_r.a)foo;
INSERT INTO dml_ao_pt_s SELECT dml_ao_pt_s.* FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_s.a = dml_ao_pt_r.a;
SELECT COUNT(*) FROM dml_ao_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test11: Join on distribution key of target table



SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_r.a = dml_ao_pt_s.a)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_r.a = dml_ao_pt_s.a ;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test12: Join on the distribution key of target table. Insert Large number of rows



SELECT COUNT(*) FROM dml_ao_pt_s;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_s SELECT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_r.b = dml_ao_pt_s.b ;
SELECT COUNT(*) FROM dml_ao_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test13: Join with different column order 



SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_s.a,dml_ao_pt_r.b,'text' FROM dml_ao_pt_r,dml_ao_pt_s  WHERE dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r(b,a,c) SELECT dml_ao_pt_s.a,dml_ao_pt_r.b,'text' FROM dml_ao_pt_r,dml_ao_pt_s  WHERE dml_ao_pt_r.b = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test14: Join with Aggregate



SELECT COUNT(*) FROM dml_ao_pt_r;
ALTER TABLE dml_ao_pt_r ADD DEFAULT partition def;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_ao_pt_s.a, dml_ao_pt_r.b + dml_ao_pt_r.a ,'text' FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b GROUP BY dml_ao_pt_s.a,dml_ao_pt_r.b,dml_ao_pt_r.a)foo;
INSERT INTO dml_ao_pt_r SELECT COUNT(*) + dml_ao_pt_s.a, dml_ao_pt_r.b + dml_ao_pt_r.a ,'text' FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b GROUP BY dml_ao_pt_s.a,dml_ao_pt_r.b,dml_ao_pt_r.a ;
SELECT COUNT(*) FROM dml_ao_pt_r;
ALTER TABLE dml_ao_pt_r DROP DEFAULT partition;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test15: Join with DISTINCT



SELECT COUNT(*) FROM dml_ao_pt_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_s.a = dml_ao_pt_r.a)foo;
INSERT INTO dml_ao_pt_s SELECT DISTINCT dml_ao_pt_r.a,dml_ao_pt_r.b,dml_ao_pt_s.c FROM dml_ao_pt_s INNER JOIN dml_ao_pt_r on dml_ao_pt_s.a = dml_ao_pt_r.a ;
SELECT COUNT(*) FROM dml_ao_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test16: Insert NULL with Joins



SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_ao_pt_r.b,'text' FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r SELECT NULL,dml_ao_pt_r.b,'text' FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test18: Insert 0 rows



SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.* FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test19: Insert 0 rows



SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.* FROM dml_ao_pt_r,dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.a and false;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test2: Negative Test - Constant tuple Inserts( no partition )



SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r values(NULL,NULL);
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test20: Negative tests Insert column of different data type



SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT ('a')::int, dml_ao_pt_r.b,10 FROM dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r SELECT ('a')::int, dml_ao_pt_r.b,10 FROM dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test21: Negative test case. INSERT has more expressions than target columns



SELECT COUNT(*) FROM dml_ao_pt_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_ao_pt_r.b, dml_ao_pt_r.c, dml_ao_pt_r.d FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_s.a = dml_ao_pt_r.a GROUP BY dml_ao_pt_r.a, dml_ao_pt_r.b, dml_ao_pt_r.c, dml_ao_pt_r.d)foo;
INSERT INTO dml_ao_pt_s SELECT COUNT(*) as a, dml_ao_pt_r.b, dml_ao_pt_r.c, dml_ao_pt_r.d FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_s.a = dml_ao_pt_r.a GROUP BY dml_ao_pt_r.a, dml_ao_pt_r.b, dml_ao_pt_r.c, dml_ao_pt_r.d;
SELECT COUNT(*) FROM dml_ao_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test22: Insert with join on the partition key



SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.* FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.* FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test24: Negative test - Insert NULL value to a table without default partition



SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.b, NULL, dml_ao_pt_r.c FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.b, NULL, dml_ao_pt_r.c FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.b = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test25: Negative test - Insert out of partition range values for table without default partition



SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.b ,dml_ao_pt_r.a + dml_ao_pt_s.a + 100, dml_ao_pt_r.c FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.a = dml_ao_pt_s.b)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.b ,dml_ao_pt_r.a + dml_ao_pt_s.a + 100, dml_ao_pt_r.c FROM dml_ao_pt_r, dml_ao_pt_s WHERE dml_ao_pt_r.a = dml_ao_pt_s.b;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test3: Negative Test - Multiple constant tuple Inserts



SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r values(NULL,NULL,NULL),(10,10,'text');
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test4: Insert with generate_series



SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_ao_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test5: Insert with generate_series and NULL



SELECT COUNT(*) FROM dml_ao_pt_r;
ALTER TABLE dml_ao_pt_r ADD DEFAULT partition def;
INSERT INTO dml_ao_pt_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_ao_pt_r WHERE c ='text' ORDER BY 1;
ALTER TABLE dml_ao_pt_r DROP DEFAULT partition;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test6: Insert with generate_series



SELECT COUNT(*) FROM dml_ao_pt_r;
TRUNCATE TABLE dml_ao_pt_r;
INSERT INTO dml_ao_pt_r SELECT generate_series(1,10),1;
SELECT * FROM dml_ao_pt_r ORDER BY 1;
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test7: Insert with generate_series



SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_ao_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_ao_pt_r;
	
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test8: Insert with generate_series



SELECT COUNT(*) FROM dml_ao_pt_r;
INSERT INTO dml_ao_pt_r SELECT *,1 from generate_series(1,10);
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_pt_r;
DROP TABLE IF EXISTS dml_ao_pt_s;
DROP TABLE IF EXISTS dml_ao_pt_p;

CREATE TABLE dml_ao_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_ao_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(100),
	default partition def);

CREATE TABLE dml_ao_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_ao_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_ao_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_ao_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_ao_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_ao_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test9: Join on the non-distribution key of target table



SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.* FROM dml_ao_pt_r,dml_ao_pt_s  WHERE dml_ao_pt_r.b = dml_ao_pt_s.a)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.* FROM dml_ao_pt_r,dml_ao_pt_s  WHERE dml_ao_pt_r.b = dml_ao_pt_s.a;
SELECT COUNT(*) FROM dml_ao_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test1: Constant tuple Inserts



SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r values(10);
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test10: Join on the distribution key of target table



SELECT COUNT(*) FROM dml_ao_s;
SELECT COUNT(*) FROM (SELECT dml_ao_s.* FROM dml_ao_r,dml_ao_s WHERE dml_ao_s.a = dml_ao_r.a)foo;
INSERT INTO dml_ao_s SELECT dml_ao_s.* FROM dml_ao_r,dml_ao_s WHERE dml_ao_s.a = dml_ao_r.a;
SELECT COUNT(*) FROM dml_ao_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test11: Join on distribution key of target table



SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_r.a = dml_ao_s.a)foo;
INSERT INTO dml_ao_r SELECT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_r.a = dml_ao_s.a ;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test12: Join on the distribution key of target table. Insert Large number of rows



SELECT COUNT(*) FROM dml_ao_s;
SELECT COUNT(*) FROM (SELECT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_r.b <> dml_ao_s.b )foo;
INSERT INTO dml_ao_s SELECT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_r.b <> dml_ao_s.b ;
SELECT COUNT(*) FROM dml_ao_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test13: Join with different column order 



SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT dml_ao_s.a,dml_ao_r.b,'text' FROM dml_ao_r,dml_ao_s  WHERE dml_ao_r.b = dml_ao_s.b)foo;
INSERT INTO dml_ao_r(b,a,c) SELECT dml_ao_s.a,dml_ao_r.b,'text' FROM dml_ao_r,dml_ao_s  WHERE dml_ao_r.b = dml_ao_s.b;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test14: Join with Aggregate



SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_ao_s.a, dml_ao_r.b + dml_ao_r.a ,'text' FROM dml_ao_r, dml_ao_s WHERE dml_ao_r.b = dml_ao_s.b GROUP BY dml_ao_s.a,dml_ao_r.b,dml_ao_r.a)foo;
INSERT INTO dml_ao_r SELECT COUNT(*) + dml_ao_s.a, dml_ao_r.b + dml_ao_r.a ,'text' FROM dml_ao_r, dml_ao_s WHERE dml_ao_r.b = dml_ao_s.b GROUP BY dml_ao_s.a,dml_ao_r.b,dml_ao_r.a ;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test15: Join with DISTINCT



SELECT COUNT(*) FROM dml_ao_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_s.a = dml_ao_r.a)foo;
INSERT INTO dml_ao_s SELECT DISTINCT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_s.a = dml_ao_r.a ;
SELECT COUNT(*) FROM dml_ao_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test16: Insert NULL with Joins



SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_ao_r.b,'text' FROM dml_ao_r,dml_ao_s WHERE dml_ao_r.b = dml_ao_s.b)foo;
INSERT INTO dml_ao_r SELECT NULL,dml_ao_r.b,'text' FROM dml_ao_r,dml_ao_s WHERE dml_ao_r.b = dml_ao_s.b;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test17: Insert and CASE



SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_ao_p.a) as A, dml_ao_p.a as B, dml_ao_s.c as C FROM dml_ao_p, dml_ao_s WHERE dml_ao_p.a = dml_ao_s.a GROUP BY dml_ao_p.a,dml_ao_s.c)as x GROUP BY A,B,C)foo;
INSERT INTO dml_ao_r SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_ao_p.a) as A, dml_ao_p.a as B, dml_ao_s.c as C FROM dml_ao_p, dml_ao_s WHERE dml_ao_p.a = dml_ao_s.a GROUP BY dml_ao_p.a,dml_ao_s.c)as x GROUP BY A,B,C;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test18: Insert 0 rows



SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r SELECT dml_ao_r.* FROM dml_ao_r,dml_ao_s WHERE dml_ao_r.b = dml_ao_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test19: Insert 0 rows



SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r SELECT dml_ao_r.* FROM dml_ao_r,dml_ao_s WHERE dml_ao_r.b = dml_ao_s.a and false;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test2: Constant tuple Inserts



SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r values(NULL);
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test20: Negative tests Insert column of different data type



SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT ('a')::int, dml_ao_r.b,10 FROM dml_ao_s WHERE dml_ao_r.b = dml_ao_s.b)foo;
INSERT INTO dml_ao_r SELECT ('a')::int, dml_ao_r.b,10 FROM dml_ao_s WHERE dml_ao_r.b = dml_ao_s.b;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test21: Negative test case. INSERT has more expressions than target columns



SELECT COUNT(*) FROM dml_ao_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_ao_r.* FROM dml_ao_r, dml_ao_s WHERE dml_ao_s.a = dml_ao_r.a GROUP BY dml_ao_r.a, dml_ao_r.b, dml_ao_r.c)foo;
INSERT INTO dml_ao_s SELECT COUNT(*) as a, dml_ao_r.* FROM dml_ao_r, dml_ao_s WHERE dml_ao_s.a = dml_ao_r.a GROUP BY dml_ao_r.a, dml_ao_r.b, dml_ao_r.c;
SELECT COUNT(*) FROM dml_ao_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test3: Multiple constant tuple Inserts



SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r values(NULL,NULL,NULL),(10,10,'text');
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test4: Insert with generate_series



SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_ao_r WHERE c ='text';
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test5: Insert with generate_series and NULL



SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_ao_r WHERE c ='text' ORDER BY 1;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test6: Insert with generate_series



SELECT COUNT(*) FROM dml_ao_r;
TRUNCATE TABLE dml_ao_r;
INSERT INTO dml_ao_r SELECT generate_series(1,10);
SELECT * FROM dml_ao_r ORDER BY 1;
SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test7: Insert with generate_series



SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_ao_r WHERE c ='text';
SELECT COUNT(*) FROM dml_ao_r;
	
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test8: Insert with generate_series



SELECT COUNT(*) FROM dml_ao_r;
INSERT INTO dml_ao_r SELECT * from generate_series(1,10);
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_ao_r;
DROP TABLE IF EXISTS dml_ao_s;
DROP TABLE IF EXISTS dml_ao_p;

CREATE TABLE dml_ao_r (a int , b int default -1, c text) WITH (appendonly = true) DISTRIBUTED BY (a);
CREATE TABLE dml_ao_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true) DISTRIBUTED BY (a,b);
CREATE TABLE dml_ao_s as select dml_ao_r.b, dml_ao_r.a, dml_ao_r.c from dml_ao_r, dml_ao_p WHERE dml_ao_r.a = dml_ao_p.a;
ALTER TABLE dml_ao_s SET DISTRIBUTED BY (b);

INSERT INTO dml_ao_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_ao_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_ao_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_ao_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_ao_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_ao_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_ao_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_ao_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_ao_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test9: Join on the non-distribution key of target table



SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT dml_ao_r.* FROM dml_ao_r,dml_ao_s  WHERE dml_ao_r.b = dml_ao_s.b)foo;
INSERT INTO dml_ao_r SELECT dml_ao_r.* FROM dml_ao_r,dml_ao_s  WHERE dml_ao_r.b = dml_ao_s.b;
SELECT COUNT(*) FROM dml_ao_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_co_check_p;
DROP TABLE IF EXISTS dml_co_check_r;
DROP TABLE IF EXISTS dml_co_check_s;

CREATE TABLE dml_co_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_co_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_co_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_co_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_co_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_co_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_co_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test1: Insert data that satisfy the check constraints 



SELECT COUNT(*) FROM dml_co_check_s;
SELECT COUNT(*) FROM (SELECT dml_co_check_s.a, dml_co_check_s.b, 'text', dml_co_check_s.d FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.b)foo;
INSERT INTO dml_co_check_s SELECT dml_co_check_s.a, dml_co_check_s.b, 'text', dml_co_check_s.d FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.b ;
SELECT COUNT(*) FROM dml_co_check_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_co_check_p;
DROP TABLE IF EXISTS dml_co_check_r;
DROP TABLE IF EXISTS dml_co_check_s;

CREATE TABLE dml_co_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_co_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_co_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_co_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_co_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_co_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_co_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test2: Negative test - Insert default value violates check constraint


SELECT COUNT(*) FROM dml_co_check_p;
INSERT INTO dml_co_check_p select generate_series(1,100),'p', generate_series(1,100);
SELECT COUNT(*) FROM dml_co_check_p;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_co_check_p;
DROP TABLE IF EXISTS dml_co_check_r;
DROP TABLE IF EXISTS dml_co_check_s;

CREATE TABLE dml_co_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_co_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_co_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_co_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_co_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_co_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_co_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test3: Negative test - Insert default value violates NOT NULL constraint

SELECT COUNT(*) FROM dml_co_check_s;
INSERT INTO dml_co_check_s values(default,1,'nn',1.0000);
SELECT COUNT(*) FROM dml_co_check_s;

-- start_ignore
drop schema functional_1_75 cascade;
-- end_ignore
