-- @Description Tests the udf for vacuum
-- And also: It tests vacuum using the user-defined functions.
-- 

DELETE FROM foo WHERE a IN (1, 2, 3, 5, 8, 13, 21);
VACUUM foo;
2U: SELECT * FROM gp_aovisimap_name('foo');
2U: SELECT * FROM gp_aovisimap_entry_name('foo');
2U: SELECT * FROM gp_aovisimap_hidden_info_name('foo');
