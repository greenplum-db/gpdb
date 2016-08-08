-- @Description Tests the udf for truncate
-- And also: It tests truncate using the user-defined functions.
-- 

DELETE FROM foo WHERE a IN (1, 2, 3, 5, 8, 13, 21);
TRUNCATE foo;
2U: SELECT * FROM gp_aovisimap_name('foo');
2U: SELECT * FROM gp_aovisimap_entry_name('foo');
2U: SELECT * FROM gp_aovisimap_hidden_info_name('foo');
