-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_ao;

CREATE TABLE reindex_ao (a INT) WITH (appendonly=true);
insert into reindex_ao select generate_series(1,1000);
select 1 as reltuples_same_as_count from pg_class where relname = 'reindex_ao'  and reltuples = (select count(*) from reindex_ao);
insert into reindex_ao select generate_series(1,1000);
select 1 as reltuples_same_as_count from pg_class where relname = 'reindex_ao'  and reltuples = (select count(*) from reindex_ao);
create index idx_btree_reindex_vacuum_analyze_ao on reindex_ao(a);
