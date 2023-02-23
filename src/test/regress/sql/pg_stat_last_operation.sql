CREATE SCHEMA mdt_test;
SET search_path = mdt_test;

-- CREATE VIEW
CREATE VIEW
	pg_stat_last_operation_testview AS
SELECT
	c.relname AS objname, staactionname AS actionname, stasubtype AS subtype
FROM
	pg_stat_last_operation lo
		JOIN pg_catalog.pg_class c ON (
			c.oid = lo.objid
		)
		JOIN pg_catalog.pg_namespace nsp ON (
			c.relnamespace = nsp.oid AND nsp.nspname = 'mdt_test'
		)
ORDER BY
	statime;  

CREATE VIEW
	pg_stat_last_operation_testview_schema AS
SELECT lo.staactionname,
       lo.stasubtype,
       ns.nspname
FROM   pg_stat_last_operation lo
       join pg_class c
         ON lo.classid = c.oid
       join pg_namespace ns
         ON c.relname = 'pg_namespace'
            AND lo.objid = ns.oid;

SELECT * FROM pg_stat_last_operation_testview WHERE actionname = 'CREATE';
 
-- CREATE TABLE
CREATE TABLE pg_stat_last_operation_test (foo int) DISTRIBUTED BY (foo);

CREATE TABLE mdt_test_part1 (
  id int,
  rank int,
  year date,
  gender char(1))
DISTRIBUTED BY (id, gender, year)
PARTITION BY list (gender)
SUBPARTITION BY range (year)
SUBPARTITION TEMPLATE (start (date '2001-01-01'))
(VALUES ('M'), VALUES ('F'));
ALTER TABLE mdt_test_part1 ADD DEFAULT PARTITION default_part;

CREATE TABLE mdt_all_types (
  a int,
  col001 char DEFAULT 'z',
  col002 numeric,
  col003 boolean DEFAULT false,
  col004 bit(3) DEFAULT '111',
  col005 text DEFAULT 'pookie',
  col006 integer[] DEFAULT '{5, 4, 3, 2, 1}',
  col007 character varying(512) DEFAULT 'Now is the time',
  col008 character varying DEFAULT 'Now is the time',
  col009 character varying(512)[],
  col010 numeric(8),
  col011 int,
  col012 double precision,
  col013 bigint,
  col014 char(8),
  col015 bytea,
  col016 timestamp with time zone,
  col017 interval,
  col018 cidr,
  col019 inet,
  col020 macaddr,
  col021 serial,
  col022 money,
  col023 bigserial,
  col024 timetz,
  col025 circle,
  col026 box,
  col027 name,
  col028 path,
  col029 int2,
  col031 bit varying(256),
  col032 date,
  col034 lseg,
  col035 point,
  col036 polygon,
  col037 real,
  col039 time,
  col040 timestamp
)
WITH (appendonly=true);

SELECT * FROM pg_stat_last_operation_testview WHERE actionname = 'CREATE';

-- CREATE TABLE .. PARTITION OF
CREATE TABLE mdt_test_newpart PARTITION OF mdt_test_part1 FOR VALUES IN ('X');

-- ATTACH PARTITION
CREATE TABLE mdt_test_newpart2 (
  id int,
  rank int,
  year date,
  gender char(1))
DISTRIBUTED BY (id, gender, year);
ALTER TABLE mdt_test_part1 ATTACH PARTITION mdt_test_newpart2 FOR VALUES IN ('Y');

-- DETACH PARTITION
CREATE TABLE mdt_test_detach PARTITION OF mdt_test_part1 FOR VALUES IN ('Z');
ALTER TABLE mdt_test_part1 DETACH PARTITION mdt_test_detach;

-- DROP PARTITION
ALTER TABLE mdt_test_part1 DROP PARTITION FOR ('Y');

-- GRANT
GRANT ALL ON mdt_all_types TO public;
SELECT * FROM pg_stat_last_operation_testview WHERE actionname = 'PRIVILEGE';

-- VACUUM
VACUUM ANALYZE mdt_all_types ;
SELECT * FROM pg_stat_last_operation_testview WHERE actionname in ('VACUUM','ANALYZE');
SELECT * FROM pg_stat_last_operation_testview WHERE objname like ('mdt_all_%');

-- TRUNCATE
SELECT * FROM pg_stat_last_operation_testview WHERE actionname = 'TRUNCATE';
INSERT INTO pg_stat_last_operation_test SELECT generate_series(1, 5);
TRUNCATE pg_stat_last_operation_test;
SELECT * FROM pg_stat_last_operation_testview WHERE actionname = 'TRUNCATE';

-- DROP TABLE
SELECT * FROM pg_stat_last_operation_testview WHERE actionname = 'DROP';
SELECT * FROM pg_stat_last_operation_testview WHERE objname like ('mdt_%');
DROP TABLE mdt_all_types;
SELECT * FROM pg_stat_last_operation_testview WHERE objname like ('mdt_%');
DROP TABLE mdt_test_part1;
SELECT * FROM pg_stat_last_operation_testview WHERE objname like ('mdt_%');

-- SCHEMA
CREATE SCHEMA mdt_schema;
SELECT * FROM pg_stat_last_operation_testview_schema WHERE nspname LIKE 'mdt_%';
DROP SCHEMA mdt_schema;
SELECT * FROM pg_stat_last_operation_testview_schema WHERE nspname LIKE 'mdt_%';
