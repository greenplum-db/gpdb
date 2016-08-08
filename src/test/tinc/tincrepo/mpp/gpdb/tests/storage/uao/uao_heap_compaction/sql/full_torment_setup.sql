-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
INSERT INTO foo SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1,10) AS i;


