-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE  sync2_ctas_heap7 AS SELECT * FROM sync1_heap_table;
CREATE TABLE  sync2_ctas_heap6 AS SELECT * FROM ck_sync1_heap_table;
CREATE TABLE  sync2_ctas_heap4 AS SELECT * FROM ct_heap_table;
CREATE TABLE  sync2_ctas_heap2 AS SELECT * FROM resync_heap_table;
CREATE TABLE  sync2_ctas_heap1 AS SELECT * FROM sync2_heap_table;

CREATE TABLE  sync2_ctas_ao7   with ( appendonly='true') AS SELECT * FROM sync1_ao_table;
CREATE TABLE  sync2_ctas_ao6   with ( appendonly='true') AS SELECT * FROM ck_sync1_ao_table;
CREATE TABLE  sync2_ctas_ao4   with ( appendonly='true') AS SELECT * FROM ct_ao_table;
CREATE TABLE  sync2_ctas_ao2   with ( appendonly='true') AS SELECT * FROM resync_ao_table;
CREATE TABLE  sync2_ctas_ao1   with ( appendonly='true') AS SELECT * FROM sync2_ao_table;

CREATE TABLE  sync2_ctas_co7  with ( appendonly='true', orientation='column') AS SELECT * FROM sync1_co_table;
CREATE TABLE  sync2_ctas_co6  with ( appendonly='true', orientation='column') AS SELECT * FROM ck_sync1_co_table;
CREATE TABLE  sync2_ctas_co4  with ( appendonly='true', orientation='column') AS SELECT * FROM ct_co_table;
CREATE TABLE  sync2_ctas_co2  with ( appendonly='true', orientation='column') AS SELECT * FROM resync_co_table;
CREATE TABLE  sync2_ctas_co1  with ( appendonly='true', orientation='column') AS SELECT * FROM sync2_co_table;
