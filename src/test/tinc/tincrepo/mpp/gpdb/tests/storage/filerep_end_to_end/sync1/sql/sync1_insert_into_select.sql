-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE  sync1_insert_into_select_heap1 AS SELECT * FROM sync1_heap_table;
INSERT INTO sync1_insert_into_select_heap1 SELECT * from sync1_heap_table;
SELECT count(*) FROM sync1_insert_into_select_heap1;


CREATE TABLE  sync1_insert_into_select_ao1   with ( appendonly='true') AS SELECT * FROM sync1_ao_table;
INSERT INTO sync1_insert_into_select_ao1 SELECT * from sync1_ao_table;
SELECT count(*) FROM sync1_insert_into_select_ao1;


CREATE TABLE  sync1_insert_into_select_co1  with ( appendonly='true', orientation='column') AS SELECT * FROM sync1_co_table;
INSERT INTO sync1_insert_into_select_co1 SELECT * from sync1_co_table;
SELECT count(*) FROM sync1_insert_into_select_co1;

