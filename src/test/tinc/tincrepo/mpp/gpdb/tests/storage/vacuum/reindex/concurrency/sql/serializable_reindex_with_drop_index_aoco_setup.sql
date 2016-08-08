-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_serialize_tab_aoco;

CREATE TABLE reindex_serialize_tab_aoco (a INT, b text, c date, d numeric, e bigint, f char(10), g float) with (appendonly=True, orientation = column) distributed by (a);
insert into reindex_serialize_tab_aoco select 1, 'abc'||i, now(),i*100.43, i*-187, 'a'|| i*-1, i*2.23 from generate_series(1,1000) i;
insert into reindex_serialize_tab_aoco select 1, 'abc'||i, now(),i*100.43, i*-187, 'a'|| i*-1, i*2.23 from generate_series(1,1000) i;
insert into reindex_serialize_tab_aoco select 1, 'abc'||i, now(),i*100.43, i*-187, 'a'|| i*-1, i*2.23 from generate_series(1,1000) i;
insert into reindex_serialize_tab_aoco select 1, 'abc'||i, now(),i*100.43, i*-187, 'a'|| i*-1, i*2.23 from generate_series(1,1000) i;
create index idxa_reindex_serialize_tab_aoco on reindex_serialize_tab_aoco(a);
create index idxb_reindex_serialize_tab_aoco on reindex_serialize_tab_aoco(b);
create index idxc_reindex_serialize_tab_aoco on reindex_serialize_tab_aoco(c);
create index idxd_reindex_serialize_tab_aoco on reindex_serialize_tab_aoco(d);
create index idxe_reindex_serialize_tab_aoco on reindex_serialize_tab_aoco(e);
create index idxf_reindex_serialize_tab_aoco on reindex_serialize_tab_aoco(f);
create index idxg_reindex_serialize_tab_aoco on reindex_serialize_tab_aoco(g);
select 1 as relfilenode_same_on_all_segs from gp_dist_random('pg_class')   where relname = 'idxa_reindex_serialize_tab_aoco' group by relfilenode having count(*) = (select count(*) from gp_segment_configuration where role='p' and content > -1);
