-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE  resync_insert_into_select_heap6 AS SELECT * FROM sync1_heap_table;
INSERT INTO resync_insert_into_select_heap6 SELECT * FROM sync1_heap_table;
SELECT count(*) FROM resync_insert_into_select_heap6;

CREATE TABLE  resync_insert_into_select_heap5 AS SELECT * FROM ck_sync1_heap_table;
INSERT INTO resync_insert_into_select_heap5 SELECT * FROM ck_sync1_heap_table;
SELECT count(*) FROM resync_insert_into_select_heap5;

CREATE TABLE  resync_insert_into_select_heap3 AS SELECT * FROM ct_heap_table;
INSERT INTO resync_insert_into_select_heap3 SELECT * FROM ct_heap_table;
SELECT count(*) FROM resync_insert_into_select_heap3;

CREATE TABLE  resync_insert_into_select_heap1 AS SELECT * FROM resync_heap_table;
INSERT INTO resync_insert_into_select_heap1 SELECT * FROM resync_heap_table;
SELECT count(*) FROM resync_insert_into_select_heap1;




CREATE TABLE  resync_insert_into_select_ao6   with ( appendonly='true') AS SELECT * FROM sync1_ao_table;
INSERT INTO resync_insert_into_select_ao6 SELECT * FROM sync1_ao_table;
SELECT count(*) FROM resync_insert_into_select_ao6;

CREATE TABLE  resync_insert_into_select_ao5   with ( appendonly='true') AS SELECT * FROM ck_sync1_ao_table;
INSERT INTO resync_insert_into_select_ao5 SELECT * FROM ck_sync1_ao_table;
SELECT count(*) FROM resync_insert_into_select_ao5;

CREATE TABLE  resync_insert_into_select_ao3   with ( appendonly='true') AS SELECT * FROM ct_ao_table;
INSERT INTO resync_insert_into_select_ao3 SELECT * FROM ct_ao_table;
SELECT count(*) FROM resync_insert_into_select_ao3;


CREATE TABLE  resync_insert_into_select_ao1   with ( appendonly='true') AS SELECT * FROM resync_ao_table;
INSERT INTO resync_insert_into_select_ao1 SELECT * FROM resync_ao_table;
SELECT count(*) FROM resync_insert_into_select_ao1;




CREATE TABLE  resync_insert_into_select_co6  with ( appendonly='true', orientation='column') AS SELECT * FROM sync1_co_table;
INSERT INTO resync_insert_into_select_co6 SELECT * FROM sync1_co_table;
SELECT count(*) FROM resync_insert_into_select_co6;

CREATE TABLE  resync_insert_into_select_co5  with ( appendonly='true', orientation='column') AS SELECT * FROM ck_sync1_co_table;
INSERT INTO resync_insert_into_select_co5 SELECT * FROM ck_sync1_co_table;
SELECT count(*) FROM resync_insert_into_select_co5;

CREATE TABLE  resync_insert_into_select_co3  with ( appendonly='true', orientation='column') AS SELECT * FROM ct_co_table;
INSERT INTO resync_insert_into_select_co3 SELECT * FROM ct_co_table;
SELECT count(*) FROM resync_insert_into_select_co3;

CREATE TABLE  resync_insert_into_select_co1  with ( appendonly='true', orientation='column') AS SELECT * FROM resync_co_table;
INSERT INTO resync_insert_into_select_co1 SELECT * FROM resync_co_table;
SELECT count(*) FROM resync_insert_into_select_co1;


