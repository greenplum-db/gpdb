-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_serialize_tab_ao;

CREATE TABLE reindex_serialize_tab_ao (a INT, b text, c date, d numeric, e bigint, f char(10), g float) with (appendonly=True) distributed by (a);
insert into reindex_serialize_tab_ao select 1, 'abc'||i, now(),i*100.43, i*-187, 'a'|| i*-1, i*2.23 from generate_series(1,1000) i;
insert into reindex_serialize_tab_ao select 1, 'abc'||i, now(),i*100.43, i*-187, 'a'|| i*-1, i*2.23 from generate_series(1,1000) i;
insert into reindex_serialize_tab_ao select 1, 'abc'||i, now(),i*100.43, i*-187, 'a'|| i*-1, i*2.23 from generate_series(1,1000) i;
insert into reindex_serialize_tab_ao select 1, 'abc'||i, now(),i*100.43, i*-187, 'a'|| i*-1, i*2.23 from generate_series(1,1000) i;
create index idxa_reindex_serialize_tab_ao on reindex_serialize_tab_ao(a);
create index idxb_reindex_serialize_tab_ao on reindex_serialize_tab_ao(b);
create index idxc_reindex_serialize_tab_ao on reindex_serialize_tab_ao(c);
create index idxd_reindex_serialize_tab_ao on reindex_serialize_tab_ao(d);
create index idxe_reindex_serialize_tab_ao on reindex_serialize_tab_ao(e);
create index idxf_reindex_serialize_tab_ao on reindex_serialize_tab_ao(f);
create index idxg_reindex_serialize_tab_ao on reindex_serialize_tab_ao(g);
