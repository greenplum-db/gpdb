-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_ao;

CREATE TABLE reindex_ao (a INT) WITH (appendonly=true);
insert into reindex_ao select generate_series(1,1000);
insert into reindex_ao select generate_series(1,1000);
create index idx_btree_reindex_ao on reindex_ao(a);
