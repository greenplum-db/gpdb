-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_crtab_ao_bitmap;

CREATE TABLE reindex_crtab_ao_bitmap (a INT) WITH (appendonly=true);
insert into reindex_crtab_ao_bitmap select generate_series(1,1000);
insert into reindex_crtab_ao_bitmap select generate_series(1,1000);
create index idx_reindex_crtab_ao_bitmap on reindex_crtab_ao_bitmap USING bitmap(a);
select 1 as oid_same_on_all_segs from gp_dist_random('pg_class')   where relname = 'idx_reindex_crtab_ao_bitmap' group by oid having count(*) = (select count(*) from gp_segment_configuration where role='p' and content > -1);

