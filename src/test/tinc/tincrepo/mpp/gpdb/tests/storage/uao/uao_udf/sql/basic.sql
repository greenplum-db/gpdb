-- @Description Tests the udf on basic updates and deletes.
-- 

2U: SELECT * FROM gp_aovisimap_name('foo');
2U: SELECT sego, first_row_num, hiddentupcount FROM gp_aovisimap_entry_name('foo');
2U: SELECT * FROM gp_aovisimap_hidden_info_name('foo');
DELETE FROM foo WHERE a IN (1, 2, 3, 5, 8, 13, 21);
2U: SELECT * FROM gp_aovisimap_name('foo');
2U: SELECT sego, first_row_num, hiddentupcount FROM gp_aovisimap_entry_name('foo');
2U: SELECT * FROM gp_aovisimap_hidden_info_name('foo');
UPDATE FOO SET b=0 WHERE a > 21;
2U: SELECT * FROM gp_aovisimap_name('foo');
2U: SELECT sego, first_row_num, hiddentupcount FROM gp_aovisimap_entry_name('foo');
2U: SELECT * FROM gp_aovisimap_hidden_info_name('foo');
