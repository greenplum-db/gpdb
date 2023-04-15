set optimizer to off;
drop table  if exists pg_stat_test;
create table pg_stat_test(a int);

select
    schemaname, relname, seq_scan, seq_tup_read, idx_scan, idx_tup_fetch, n_tup_ins, n_tup_upd,
    n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup
from gp_stat_all_tables_summary where relname = 'pg_stat_test';
select
    schemaname, relname, seq_scan, seq_tup_read, idx_scan, idx_tup_fetch, n_tup_ins, n_tup_upd,
    n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup
from gp_stat_user_tables_summary where relname = 'pg_stat_test';
select
    schemaname, relname, indexrelname, idx_scan, idx_tup_read, idx_tup_fetch
from gp_stat_all_indexes_summary where relname = 'pg_stat_test';
select
    schemaname, relname, indexrelname, idx_scan, idx_tup_read, idx_tup_fetch
from gp_stat_user_indexes_summary where relname = 'pg_stat_test';

begin; -- make analyze same transcation with insert to avoid double the pgstat causes by unorder message read.
insert into pg_stat_test select * from generate_series(1, 100);
analyze pg_stat_test;
commit;

create index pg_stat_user_table_index on pg_stat_test(a);

select count(*) from pg_stat_test;

delete from pg_stat_test where a < 10;

update pg_stat_test set a = 1000 where a > 90;

set enable_seqscan to off;

select pg_sleep(10);

select * from pg_stat_test where a = 1;

reset enable_seqscan;

select
    schemaname, relname, seq_scan, seq_tup_read, idx_scan, idx_tup_fetch, n_tup_ins, n_tup_upd,
    n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze
from gp_stat_all_tables_summary where relname = 'pg_stat_test';
select
    schemaname, relname, seq_scan, seq_tup_read, idx_scan, idx_tup_fetch, n_tup_ins, n_tup_upd,
    n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze
from gp_stat_user_tables_summary where relname = 'pg_stat_test';
select
    schemaname, relname, indexrelname, idx_scan, idx_tup_read, idx_tup_fetch
from gp_stat_all_indexes_summary where relname = 'pg_stat_test';
select
    schemaname, relname, indexrelname, idx_scan, idx_tup_read, idx_tup_fetch
from gp_stat_user_indexes_summary where relname = 'pg_stat_test';

reset optimizer;

---------------------------------------------------------------------------
-- "user" views should exclude AO/CO auxiliary tables, and "sys" views should include them
---------------------------------------------------------------------------
create table pg_stat_ao(a int) using ao_row;
create table pg_stat_co(a int) using ao_column;
create index pg_stat_ao_ind on pg_stat_ao(a);
create index pg_stat_co_ind on pg_stat_co(a);

-- "user" views:
select c.relname from pg_stat_user_tables s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from gp_stat_user_tables s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from gp_stat_user_tables_summary s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from pg_stat_xact_user_tables s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from gp_stat_xact_user_tables s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from pg_stat_user_indexes s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from gp_stat_user_indexes s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from gp_stat_user_indexes_summary s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from pg_statio_user_indexes s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from gp_statio_user_indexes s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
-- no AO/CO aux tables are sequence, but test it anyway 
select c.relname from pg_statio_user_sequences s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from gp_statio_user_sequences s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');

-- "sys" views:
-- for cluster-wide views, just test output of one segment.
-- also take a fixed part of the aux table names to avoid different column length because of OIDs.
select c.relname as aorelname, substring(s.relname, 1, 9) as aoauxrelname 
from pg_stat_sys_tables s 
join pg_appendonly a on s.relid = a.segrelid or s.relid = a.blkdirrelid or s.relid = a.visimaprelid 
join pg_class c on a.relid = c.oid 
where c.relname like 'pg_stat_%';

select c.relname as aorelname, substring(s.relname, 1, 9) as aoauxrelname 
from gp_stat_sys_tables s 
join pg_appendonly a on s.relid = a.segrelid or s.relid = a.blkdirrelid or s.relid = a.visimaprelid 
join pg_class c on a.relid = c.oid 
where c.relname like 'pg_stat_%' and gp_segment_id = 0;

select c.relname as aorelname, substring(s.relname, 1, 9) as aoauxrelname 
from pg_stat_xact_sys_tables s 
join pg_appendonly a on s.relid = a.segrelid or s.relid = a.blkdirrelid or s.relid = a.visimaprelid 
join pg_class c on a.relid = c.oid 
where c.relname like 'pg_stat_%';

select c.relname as aorelname, substring(s.relname, 1, 9) as aoauxrelname 
from gp_stat_xact_sys_tables s 
join pg_appendonly a on s.relid = a.segrelid or s.relid = a.blkdirrelid or s.relid = a.visimaprelid 
join pg_class c on a.relid = c.oid 
where c.relname like 'pg_stat_%' and gp_segment_id = 0;

select c.relname as aorelname, substring(s.relname, 1, 9) as aoauxrelname 
from pg_statio_sys_tables s 
join pg_appendonly a on s.relid = a.segrelid or s.relid = a.blkdirrelid or s.relid = a.visimaprelid 
join pg_class c on a.relid = c.oid 
where c.relname like 'pg_stat_%';

select c.relname as aorelname, substring(s.relname, 1, 9) as aoauxrelname 
from gp_statio_sys_tables s 
join pg_appendonly a on s.relid = a.segrelid or s.relid = a.blkdirrelid or s.relid = a.visimaprelid 
join pg_class c on a.relid = c.oid 
where c.relname like 'pg_stat_%' and gp_segment_id = 0;

select c.relname as aorelname, substring(s.relname, 1, 9) as aoauxrelname 
from pg_stat_sys_indexes s 
join pg_appendonly a on s.relid = a.segrelid or s.relid = a.blkdirrelid or s.relid = a.visimaprelid 
join pg_class c on a.relid = c.oid 
where c.relname like 'pg_stat_%';

select c.relname as aorelname, substring(s.relname, 1, 9) as aoauxrelname 
from gp_stat_sys_indexes s 
join pg_appendonly a on s.relid = a.segrelid or s.relid = a.blkdirrelid or s.relid = a.visimaprelid 
join pg_class c on a.relid = c.oid 
where c.relname like 'pg_stat_%' and gp_segment_id = 0;

select c.relname as aorelname, substring(s.relname, 1, 9) as aoauxrelname 
from pg_statio_sys_indexes s 
join pg_appendonly a on s.relid = a.segrelid or s.relid = a.blkdirrelid or s.relid = a.visimaprelid 
join pg_class c on a.relid = c.oid 
where c.relname like 'pg_stat_%';

select c.relname as aorelname, substring(s.relname, 1, 9) as aoauxrelname 
from gp_statio_sys_indexes s 
join pg_appendonly a on s.relid = a.segrelid or s.relid = a.blkdirrelid or s.relid = a.visimaprelid 
join pg_class c on a.relid = c.oid 
where c.relname like 'pg_stat_%' and gp_segment_id = 0;

-- no AO/CO aux tables are sequence, but test it anyway 
select c.relname from pg_statio_sys_sequences s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
select c.relname from gp_statio_sys_sequences s join pg_class c on s.relid = c.oid where c.relkind in ('o', 'b', 'M');
