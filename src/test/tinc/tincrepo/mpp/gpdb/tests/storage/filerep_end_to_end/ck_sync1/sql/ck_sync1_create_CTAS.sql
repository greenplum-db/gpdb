-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE  ck_sync1_ctas_heap2 AS SELECT * FROM sync1_heap_table;
CREATE TABLE  ck_sync1_ctas_heap1 AS SELECT * FROM ck_sync1_heap_table;

CREATE TABLE  ck_sync1_ctas_ao2   with ( appendonly='true') AS SELECT * FROM sync1_ao_table;
CREATE TABLE  ck_sync1_ctas_ao1   with ( appendonly='true') AS SELECT * FROM ck_sync1_ao_table;

CREATE TABLE  ck_sync1_ctas_co2  with ( appendonly='true', orientation='column') AS SELECT * FROM sync1_co_table;
CREATE TABLE  ck_sync1_ctas_co1  with ( appendonly='true', orientation='column') AS SELECT * FROM ck_sync1_co_table;
