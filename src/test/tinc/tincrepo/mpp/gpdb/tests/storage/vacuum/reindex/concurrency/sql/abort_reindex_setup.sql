-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_abort_ao;

CREATE TABLE reindex_abort_ao (a INT) WITH (appendonly=true, orientation=column);
insert into reindex_abort_ao select generate_series(1,1000);
create index idx_btree_reindex_abort_ao on reindex_abort_ao(a);

-- start_ignore
drop table if exists reindex_abort_ao_old;
create table reindex_abort_ao_old as
  (select oid as c_oid, gp_segment_id as c_gp_segment_id, relfilenode as c_relfilenode from pg_class where relname = 'idx_btree_reindex_abort_ao' union all
   select oid as c_oid, gp_segment_id as c_gp_segment_id, relfilenode as c_relfilenode from gp_dist_random('pg_class') where relname = 'idx_btree_reindex_abort_ao');
-- end_ignore

select 1 as have_same_number_of_rows from reindex_abort_ao_old where c_gp_segment_id > -1 group by c_oid having count(*) = (select count(*) from gp_segment_configuration where role = 'p' and content > -1);
