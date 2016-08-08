-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
select * from uao_visimap_test05;
select * from gp_dist_random('pg_aoseg.pg_aovisimap_550321');
insert into uao_visimap_test05 select i,'aa'||i,i+10 from generate_series(1,5) as i;
delete from uao_visimap_test05 where i=3;
select * from uao_visimap_test05;
select * from gp_dist_random('pg_aoseg.pg_aovisimap_550321');
truncate table uao_visimap_test05;
select * from uao_visimap_test05;
select * from gp_dist_random('pg_aoseg.pg_aovisimap_550321');
