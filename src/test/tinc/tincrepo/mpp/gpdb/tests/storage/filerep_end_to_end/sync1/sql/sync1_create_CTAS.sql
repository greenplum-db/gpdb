-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE  sync1_ctas_heap1 AS SELECT * FROM sync1_heap_table;

CREATE TABLE  sync1_ctas_ao1  with ( appendonly='true')  AS SELECT * FROM sync1_ao_table;

CREATE TABLE  sync1_ctas_co1  with ( appendonly='true', orientation='column') AS SELECT * FROM sync1_co_table;
