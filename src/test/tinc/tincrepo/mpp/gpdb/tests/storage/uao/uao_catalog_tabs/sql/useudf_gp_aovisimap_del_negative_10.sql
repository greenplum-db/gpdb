-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
select * from uao_visimap_test10;
 select * from gp_aovisimap(43608);
delete from uao_visimap_test10 where i < 6;
select * from uao_visimap_test10;
 select * from gp_aovisimap(43608);
