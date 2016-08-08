-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
BEGIN;
UPDATE foo set b = 0 ;
COMMIT;
