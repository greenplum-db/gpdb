-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Set returning functions

--Drop tables and functions
Drop table if exists srf_t1;
Drop table if exists srf_t2;
Drop table if exists srf_t3;
Drop function if exists srf_vect();

--Set Returning Functions

create table srf_t1 (i int, t text);
insert into srf_t1 select i % 10, (i % 10)::text  from generate_series(1, 100) i;
create index srf_t1_idx on srf_t1 using bitmap (i);
select count(*) from srf_t1 where i=1;


create table srf_t2 (i int, j int, k int) distributed by (k);
create index srf_t2_i_idx on srf_t2 using bitmap(i);
insert into srf_t2 select 1,
case when (i % (16 * 16 + 8)) = 0 then 2  else 1 end, 1
from generate_series(1, 16 * 16 * 16) i;
select count(*) from srf_t2 where i = 1;
select count(*) from srf_t2 where j = 2;


create table srf_t3 (i int, j int, k int) distributed by (k);
create index srf_t3_i_idx on srf_t3 using bitmap(i);
insert into srf_t3 select i, 1, 1 from
generate_series(1, 8250 * 8) g, generate_series(1, 2) as i;

insert into srf_t3 select 17, 1, 1 from generate_series(1, 16 * 16) i;
insert into srf_t3 values(17, 2, 1);

insert into srf_t3 select 17, 1, 1 from generate_series(1, 16 * 16) i;

insert into srf_t3 select i, 1, 1 from
generate_series(1, 8250 * 8) g, generate_series(1, 2) i;
select count(*) from srf_t3 where i = 1;
select count(*) from srf_t3 where i = 17;


create function srf_vect() returns void as $proc$
<<lbl>>declare a integer; b varchar; c varchar; r record;
begin
  -- fori
  for i in 1 .. 3 loop
    raise notice '%', i;
  end loop;
  -- fore with record var
  for r in select gs as aa, 'BB' as bb, 'CC' as cc from generate_series(1,4) gs loop
    raise notice '% % %', r.aa, r.bb, r.cc;
  end loop;
  -- fore with single scalar
  for a in select gs from generate_series(1,4) gs loop
    raise notice '%', a;
  end loop;
  -- fore with multiple scalars
  for a,b,c in select gs, 'BB','CC' from generate_series(1,4) gs loop
    raise notice '% % %', a, b, c;
  end loop;
  -- using qualified names in fors, fore is enabled, disabled only for fori
  for lbl.a, lbl.b, lbl.c in execute $$select gs, 'bb','cc' from generate_series(1,4) gs$$ loop
    raise notice '% % %', a, b, c;
  end loop;
end;
$proc$ language plpgsql;

select srf_vect();

