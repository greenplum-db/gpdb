create or replace function junkloop(rel text, numiter int) returns int as $$
declare
  sql text;
begin
  for i in 0..numiter loop
    sql := $sql$ insert into $sql$ || rel || $sql$ select 1, $sql$ || i::text || $sql$, repeat('x', 1000) $sql$;
    execute sql;
  end loop;
  return numiter;
end;
$$ language plpgsql;

drop table if exists vfao;
create table vfao (a, b, c) with (appendonly=true, orientation=column) as
select 1, i, repeat('x', 1000) from generate_series(1, 100)i distributed by (a);
create index ivfao on vfao(b, c);

-- insert many times to populate invisible tuples in pg_aoseg
select junkloop('vfao', 300);

select pg_relation_size((select segrelid from pg_appendonly where relid = 'vfao'::regclass)) from gp_dist_random('gp_id') where gp_segment_id = 1;

vacuum full vfao;
select pg_relation_size((select segrelid from pg_appendonly where relid = 'vfao'::regclass)) from gp_dist_random('gp_id') where gp_segment_id = 1;

-- github issue https://github.com/greenplum-db/gpdb/issues/7050

drop table if exists ao;
create table ao (a int, b box) with(appendonly=true) distributed by (a);
create index gist_ao on ao using gist (b);
insert into ao select i, ('(0,'||i||', 1,'||i+1||')')::box from generate_series(1,5)i;
update ao set b = '((1,1),(4,4))' where a = 1;
vacuum ao;
drop table ao;