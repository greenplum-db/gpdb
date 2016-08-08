-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE  ct_ctas_heap4 AS SELECT * FROM sync1_heap_table;
CREATE TABLE  ct_ctas_heap3 AS SELECT * FROM ck_sync1_heap_table;
CREATE TABLE  ct_ctas_heap1 AS SELECT * FROM ct_heap_table;

CREATE TABLE  ct_ctas_ao4   with ( appendonly='true') AS SELECT * FROM sync1_ao_table;
CREATE TABLE  ct_ctas_ao3   with ( appendonly='true') AS SELECT * FROM ck_sync1_ao_table;
CREATE TABLE  ct_ctas_ao1   with ( appendonly='true') AS SELECT * FROM ct_ao_table;

CREATE TABLE  ct_ctas_co4  with ( appendonly='true', orientation='column') AS SELECT * FROM sync1_co_table;
CREATE TABLE  ct_ctas_co3  with ( appendonly='true', orientation='column') AS SELECT * FROM ck_sync1_co_table;
CREATE TABLE  ct_ctas_co1  with ( appendonly='true', orientation='column') AS SELECT * FROM ct_co_table;
