-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
SELECT * FROM gp_toolkit.__gp_aoseg_name('foo');
VACUUM foo;
SELECT * FROM gp_toolkit.__gp_aoseg_name('foo');
