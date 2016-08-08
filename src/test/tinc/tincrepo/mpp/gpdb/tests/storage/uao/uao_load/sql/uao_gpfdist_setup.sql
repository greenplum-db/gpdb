-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP EXTERNAL TABLE IF EXISTS ext_customer CASCADE;
DROP TABLE IF EXISTS customer CASCADE;

CREATE TABLE customer (id int, name text, sponsor text) WITH (appendonly=true);
