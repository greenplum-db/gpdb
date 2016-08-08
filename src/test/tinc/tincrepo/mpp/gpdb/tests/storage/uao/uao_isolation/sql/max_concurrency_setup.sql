-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS ao;

CREATE TABLE ao (a INT, b INT) WITH (appendonly=true);
