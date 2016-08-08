-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE  ct_insert_into_select_heap4 AS SELECT * FROM sync1_heap_table;
INSERT INTO ct_insert_into_select_heap4 SELECT * FROM sync1_heap_table;
SELECT count(*) from ct_insert_into_select_heap4;

CREATE TABLE  ct_insert_into_select_heap3 AS SELECT * FROM ck_sync1_heap_table;
INSERT INTO ct_insert_into_select_heap3 SELECT * FROM ck_sync1_heap_table;
SELECT count(*) from ct_insert_into_select_heap3;

CREATE TABLE  ct_insert_into_select_heap1 AS SELECT * FROM ct_heap_table;
INSERT INTO ct_insert_into_select_heap1 SELECT * FROM ct_heap_table;
SELECT count(*) from ct_insert_into_select_heap1;



CREATE TABLE  ct_insert_into_select_ao4   with ( appendonly='true') AS SELECT * FROM sync1_ao_table;
INSERT INTO ct_insert_into_select_ao4 SELECT * FROM sync1_ao_table;
SELECT count(*) from ct_insert_into_select_ao4;

CREATE TABLE  ct_insert_into_select_ao3   with ( appendonly='true') AS SELECT * FROM ck_sync1_ao_table;
INSERT INTO ct_insert_into_select_ao3 SELECT * FROM ck_sync1_ao_table;
SELECT count(*) from ct_insert_into_select_ao3;

CREATE TABLE  ct_insert_into_select_ao1   with ( appendonly='true') AS SELECT * FROM ct_ao_table;
INSERT INTO ct_insert_into_select_ao1 SELECT * FROM ct_ao_table;
SELECT count(*) from ct_insert_into_select_ao1;



CREATE TABLE  ct_insert_into_select_co4  with ( appendonly='true', orientation='column') AS SELECT * FROM sync1_co_table;
INSERT INTO ct_insert_into_select_co4 SELECT * FROM sync1_co_table;
SELECT count(*) from ct_insert_into_select_co4;

CREATE TABLE  ct_insert_into_select_co3  with ( appendonly='true', orientation='column') AS SELECT * FROM ck_sync1_co_table;
INSERT INTO ct_insert_into_select_co3 SELECT * FROM ck_sync1_co_table;
SELECT count(*) from ct_insert_into_select_co3;

CREATE TABLE  ct_insert_into_select_co1  with ( appendonly='true', orientation='column') AS SELECT * FROM ct_co_table;
INSERT INTO ct_insert_into_select_co1 SELECT * FROM ct_co_table;
SELECT count(*) from ct_insert_into_select_co1;

