-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE  sync2_insert_into_select_heap7 AS SELECT * FROM sync1_heap_table;
INSERT INTO sync2_insert_into_select_heap7 SELECT * FROM sync1_heap_table;
SELECT count(*) FROM sync2_insert_into_select_heap7;

CREATE TABLE  sync2_insert_into_select_heap6 AS SELECT * FROM ck_sync1_heap_table;
INSERT INTO sync2_insert_into_select_heap6 SELECT * FROM ck_sync1_heap_table;
SELECT count(*) FROM sync2_insert_into_select_heap6;

CREATE TABLE  sync2_insert_into_select_heap4 AS SELECT * FROM ct_heap_table;
INSERT INTO sync2_insert_into_select_heap4 SELECT * FROM ct_heap_table;
SELECT count(*) FROM sync2_insert_into_select_heap4;

CREATE TABLE  sync2_insert_into_select_heap2 AS SELECT * FROM resync_heap_table;
INSERT INTO sync2_insert_into_select_heap2 SELECT * FROM resync_heap_table;
SELECT count(*) FROM sync2_insert_into_select_heap2;

CREATE TABLE  sync2_insert_into_select_heap1 AS SELECT * FROM sync2_heap_table;
INSERT INTO sync2_insert_into_select_heap1 SELECT * FROM sync2_heap_table;
SELECT count(*) FROM sync2_insert_into_select_heap1;




CREATE TABLE  sync2_insert_into_select_ao7   with ( appendonly='true') AS SELECT * FROM sync1_ao_table;
INSERT INTO sync2_insert_into_select_ao7 SELECT * FROM sync1_ao_table;
SELECT count(*) FROM sync2_insert_into_select_ao7;

CREATE TABLE  sync2_insert_into_select_ao6   with ( appendonly='true') AS SELECT * FROM ck_sync1_ao_table;
INSERT INTO sync2_insert_into_select_ao6 SELECT * FROM ck_sync1_ao_table;
SELECT count(*) FROM sync2_insert_into_select_ao6;

CREATE TABLE  sync2_insert_into_select_ao4   with ( appendonly='true') AS SELECT * FROM ct_ao_table;
INSERT INTO sync2_insert_into_select_ao4 SELECT * FROM ct_ao_table;
SELECT count(*) FROM sync2_insert_into_select_ao4;

CREATE TABLE  sync2_insert_into_select_ao2   with ( appendonly='true') AS SELECT * FROM resync_ao_table;
INSERT INTO sync2_insert_into_select_ao2 SELECT * FROM resync_ao_table;
SELECT count(*) FROM sync2_insert_into_select_ao2;

CREATE TABLE  sync2_insert_into_select_ao1   with ( appendonly='true') AS SELECT * FROM sync2_ao_table;
INSERT INTO sync2_insert_into_select_ao1 SELECT * FROM sync2_ao_table;
SELECT count(*) FROM sync2_insert_into_select_ao1;




CREATE TABLE  sync2_insert_into_select_co7  with ( appendonly='true', orientation='column') AS SELECT * FROM sync1_co_table;
INSERT INTO sync2_insert_into_select_co7 SELECT * FROM sync1_co_table;
SELECT count(*) FROM sync2_insert_into_select_co7;

CREATE TABLE  sync2_insert_into_select_co6  with ( appendonly='true', orientation='column') AS SELECT * FROM ck_sync1_co_table;
INSERT INTO sync2_insert_into_select_co6 SELECT * FROM ck_sync1_co_table;
SELECT count(*) FROM sync2_insert_into_select_co6;

CREATE TABLE  sync2_insert_into_select_co4  with ( appendonly='true', orientation='column') AS SELECT * FROM ct_co_table;
INSERT INTO sync2_insert_into_select_co4 SELECT * FROM ct_co_table;
SELECT count(*) FROM sync2_insert_into_select_co4;

CREATE TABLE  sync2_insert_into_select_co2  with ( appendonly='true', orientation='column') AS SELECT * FROM resync_co_table;
INSERT INTO sync2_insert_into_select_co2 SELECT * FROM resync_co_table;
SELECT count(*) FROM sync2_insert_into_select_co2;

CREATE TABLE  sync2_insert_into_select_co1  with ( appendonly='true', orientation='column') AS SELECT * FROM sync2_co_table;
INSERT INTO sync2_insert_into_select_co1 SELECT * FROM sync2_co_table;
SELECT count(*) FROM sync2_insert_into_select_co1;
