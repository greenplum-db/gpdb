drop table if exists vfheap;
create table vfheap (a, b, c) as
select 1, i, repeat('x', 1000) from generate_series(1, 100)i distributed by (a);
create index ivfheap on vfheap(b, c);
analyze vfheap;

-- delete half of table
delete from vfheap where b between 0 and (select count(*) / 2 from vfheap);
select pg_relation_size('vfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;
select pg_relation_size('ivfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;

-- show pages are truncated
vacuum full freeze vfheap;
select pg_relation_size('vfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;
select pg_relation_size('ivfheap') from gp_dist_random('gp_id') where gp_segment_id = 1;

select max(b), min(substring(c, 10, 1)) from vfheap;
