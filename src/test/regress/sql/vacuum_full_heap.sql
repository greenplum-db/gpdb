drop table if exists vfheap;
create table vfheap (a, b, c) as
select 1, i, repeat('x', 1000) from generate_series(1, 100)i distributed by (a);
analyze vfheap;
create index ivfheap on vfheap(b, c);

-- delete half of table
delete from vfheap where b between 0 and (select count(*) / 2 from vfheap);
select pg_relation_size('vfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;
select pg_relation_size('ivfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;

-- show pages are truncated
-- GPDB-specific: VACUUM FULL on heap gives proper relpages and reltuples
select relname, relpages, reltuples, gp_segment_id from gp_dist_random('pg_class') where oid = 'vfheap'::regclass;
vacuum full vfheap;
select relname, relpages, reltuples, gp_segment_id from gp_dist_random('pg_class') where oid = 'vfheap'::regclass;
select pg_relation_size('vfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;
select pg_relation_size('ivfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;

select max(b), min(length(c)) from vfheap;

-- check relpages and reltuples
select relname, relpages, reltuples from gp_dist_random('pg_class') where (oid = 'vfheap'::regclass or oid = 'ivfheap'::regclass) and gp_segment_id = 1;

-- just for the sake of code coverage, one more shot of vacuum full
vacuum full vfheap;

-- delete completely, size becomes 0, reducing index too
delete from vfheap;
vacuum full vfheap;
select pg_relation_size('vfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;
select pg_relation_size('ivfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;

-- check relpages and reltuples (VACUUM FULL clears them)
select relname, relpages, reltuples from gp_dist_random('pg_class') where (oid = 'vfheap'::regclass or oid = 'ivfheap'::regclass) and gp_segment_id = 1;

-- again, but delete second half
insert into vfheap select 1, i, repeat('x', 1000) from generate_series(1, 100)i;
delete from vfheap where b between (select count(*) / 2 from vfheap) and (select count(*) from vfheap);
select pg_relation_size('vfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;
select pg_relation_size('ivfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;

-- show pages are truncated
vacuum full vfheap;
select pg_relation_size('vfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;
select pg_relation_size('ivfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;

select max(b), min(length(c)) from vfheap;

-- also make sure toast is truncated
drop table if exists vfheaptoast;
create table vfheaptoast (a, b, c) as
select 1, i, array(select generate_series(1, 10000)) from generate_series(1, 100)i;
analyze vfheaptoast;

select pg_relation_size((select reltoastrelid from pg_class where oid = 'vfheaptoast'::regclass)) from gp_dist_random('gp_id') where gp_segment_id = 1;

delete from vfheaptoast where b between 0 and (select count(*) / 2 from vfheaptoast);
vacuum full vfheaptoast;

select pg_relation_size((select reltoastrelid from pg_class where oid = 'vfheaptoast'::regclass)) from gp_dist_random('gp_id') where gp_segment_id = 1;

select max(b), min(length(array_to_string(c, ','))) from vfheaptoast;

delete from vfheaptoast;
vacuum full vfheaptoast;

select pg_relation_size((select reltoastrelid from pg_class where oid = 'vfheaptoast'::regclass)) from gp_dist_random('gp_id') where gp_segment_id = 1;

