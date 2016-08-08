-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE  ck_sync1_insert_into_select_heap2 AS SELECT * FROM sync1_heap_table;
INSERT INTO ck_sync1_insert_into_select_heap2 SELECT * from sync1_heap_table;
SELECT count(*) FROM ck_sync1_insert_into_select_heap2;

CREATE TABLE  ck_sync1_insert_into_select_heap1 AS SELECT * FROM ck_sync1_heap_table;
INSERT INTO ck_sync1_insert_into_select_heap1 SELECT * from ck_sync1_heap_table;
SELECT count(*) FROM ck_sync1_insert_into_select_heap1;



CREATE TABLE  ck_sync1_insert_into_select_ao2   with ( appendonly='true') AS SELECT * FROM sync1_ao_table;
INSERT INTO ck_sync1_insert_into_select_ao2 SELECT * from sync1_ao_table;
SELECT count(*) FROM ck_sync1_insert_into_select_ao2;

CREATE TABLE  ck_sync1_insert_into_select_ao1   with ( appendonly='true') AS SELECT * FROM ck_sync1_ao_table;
INSERT INTO ck_sync1_insert_into_select_ao1 SELECT * from ck_sync1_ao_table;
SELECT count(*) FROM ck_sync1_insert_into_select_ao1;



CREATE TABLE  ck_sync1_insert_into_select_co2  with ( appendonly='true', orientation='column') AS SELECT * FROM sync1_co_table;
INSERT INTO ck_sync1_insert_into_select_co2 SELECT * from sync1_co_table;
SELECT count(*) FROM ck_sync1_insert_into_select_co2;

CREATE TABLE  ck_sync1_insert_into_select_co1  with ( appendonly='true', orientation='column') AS SELECT * FROM ck_sync1_co_table;
INSERT INTO ck_sync1_insert_into_select_co1 SELECT * from ck_sync1_co_table;
SELECT count(*) FROM ck_sync1_insert_into_select_co1;
