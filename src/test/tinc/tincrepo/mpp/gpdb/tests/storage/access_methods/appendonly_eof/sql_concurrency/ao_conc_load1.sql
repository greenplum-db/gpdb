-- @concurrency 10
-- @iterations 5
INSERT INTO TEST_AO_LOAD1 SELECT * FROM exttab_conc_1;
COPY TEST_AO_LOAD1 FROM '@data_dir@/data.tbl' WITH DELIMITER '|' SEGMENT REJECT LIMIT 100;

SELECT duplicate_check_aoseg('pg_aoseg.pg_aoseg_' || a.relfilenode) FROM 
(SELECT relfilenode FROM pg_class WHERE relname = 'TEST_AO_LOAD1')a;

SELECT duplicate_check_visimap('pg_aoseg.pg_aovisimap_' || a.relfilenode) FROM 
(SELECT relfilenode FROM pg_class WHERE relname = 'TEST_AO_LOAD1')a;

COPY TEST_AO_LOAD1 FROM STDIN WITH DELIMITER '|';
1|'test'
2|'test'
3|'test'
4|'test'
5|'test'
\.
BEGIN;
INSERT INTO TEST_AO_LOAD1 select i, 'test123' from generate_series(1, 10000)i;
ABORT;
INSERT INTO TEST_AO_LOAD1 VALUES (1, 'test');
-- start_ignore
COPY TEST_AO_LOAD1 TO '@data_dir@/output.tbl' WITH DELIMITER '|';
UPDATE TEST_AO_LOAD1 SET j = i || '_update' WHERE i > 103 and i < 545;
-- end_ignore

SELECT duplicate_check_aoseg('pg_aoseg.pg_aoseg_' || a.relfilenode) FROM 
(SELECT relfilenode FROM pg_class WHERE relname = 'TEST_AO_LOAD1')a;

SELECT duplicate_check_visimap('pg_aoseg.pg_aovisimap_' || a.relfilenode) FROM 
(SELECT relfilenode FROM pg_class WHERE relname = 'TEST_AO_LOAD1')a;

INSERT INTO TEST_AO_LOAD1 select i, 'test456' from generate_series(10000, 20000)i;;

BEGIN;
COPY TEST_AO_LOAD1 FROM '@data_dir@/data.tbl' WITH DELIMITER '|' SEGMENT REJECT LIMIT 100;
ABORT;

INSERT INTO TEST_AO_LOAD1 select i, 'test789' from generate_series(20000, 30000)i;;
-- start_ignore
COPY TEST_AO_LOAD1 TO '@data_dir@/output1.tbl' WITH DELIMITER '|';
UPDATE TEST_AO_LOAD1 SET j = i || '_update' WHERE i > 602 and i < 876;
-- end_ignore
INSERT INTO TEST_AO_LOAD1 VALUES (1, 'test');

COPY TEST_AO_LOAD1 FROM '@data_dir@/data.tbl' WITH DELIMITER '|' SEGMENT REJECT LIMIT 100;

SELECT duplicate_check_aoseg('pg_aoseg.pg_aoseg_' || a.relfilenode) FROM 
(SELECT relfilenode FROM pg_class WHERE relname = 'TEST_AO_LOAD1')a;

SELECT duplicate_check_visimap('pg_aoseg.pg_aovisimap_' || a.relfilenode) FROM 
(SELECT relfilenode FROM pg_class WHERE relname = 'TEST_AO_LOAD1')a;

