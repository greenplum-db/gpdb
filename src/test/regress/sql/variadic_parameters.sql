-- -----------------------------------------------------------------
-- Test function variadic parameters
--
-- Most of these tests were backported from PostgreSQL 8.4's polymorphism and
-- plpgsql tests. We should remove the redundant ones after we've merged with
-- 8.4.
-- -----------------------------------------------------------------

-- test variadic polymorphic functions
create or replace function myleast(variadic anyarray) returns anyelement as $$
  select min(g) from unnest($1) g
$$ language sql immutable strict;

select myleast(10, 1, 20, 33);
select myleast(1.1, 0.22, 0.55);
select myleast('z'::text);
select myleast(); -- fail

-- test with variadic call parameter
select myleast(variadic array[1,2,3,4,-1]);
select myleast(variadic array[1.1, -5.5]);

--test with empty variadic call parameter
select myleast(variadic array[]::int[]);

-- an example with some ordinary arguments too
create or replace function concat(text, variadic anyarray) returns text as $$
  select array_to_string($2, $1);
$$ language sql immutable strict;

select concat('%', 1, 2, 3, 4, 5);
select concat('|', 'a'::text, 'b', 'c');
select concat('|', variadic array[1,2,33]);
select concat('-|-', variadic '{44,NULL,55,NULL,33}'::int[]);

-- deparse view
create table people (id int, fname varchar, lname varchar);
insert into people values (770,'John','Woo');
insert into people values (771,'Jim','Ng');
insert into people values (778,'Jerry','Lau');
insert into people values (790,'Jen','Smith');
create view print_name as select concat(' ', fname, lname) from people where id < 790;

select * from print_name;
select pg_get_viewdef('print_name');
select pg_get_viewdef('print_name', true);

drop view print_name;
drop function concat(text, anyarray);

-- mix variadic with anyelement
create or replace function formarray(anyelement, variadic anyarray) returns anyarray as $$
  select array_prepend($1, $2);
$$ language sql immutable strict;

select formarray(1,2,3,4,5);
select formarray(1.1, variadic array[1.2,55.5]);
select formarray(1.1, array[1.2,55.5]); -- fail without variadic
select formarray(1, 'x'::text); -- fail, type mismatch
select formarray(1, variadic array['x'::text]); -- fail, type mismatch

drop function formarray(anyelement, variadic anyarray);

-- PLPGSQL
-- test variadic functions

create or replace function vari(variadic int[])
returns void as $$
begin
  for i in array_lower($1,1)..array_upper($1,1) loop
    raise notice '%', $1[i];
  end loop; end;
$$ language plpgsql;

select vari(1,2,3,4,5);
select vari(3,4,5);
select vari(variadic array[5,6,7]);

drop function vari(int[]);

-- coercion test
create or replace function pleast(variadic numeric[])
returns numeric as $$
declare aux numeric = $1[array_lower($1,1)];
begin
  for i in array_lower($1,1)+1..array_upper($1,1) loop
    if $1[i] < aux then aux := $1[i]; end if;
  end loop;
  return aux;
end;
$$ language plpgsql immutable strict;

select pleast(10,1,2,3,-16);
select pleast(10.2,2.2,-1.1);
select pleast(10.2,10, -20);
select pleast(10,20, -1.0);

-- in case of conflict, non-variadic version is preferred
create or replace function pleast(numeric)
returns numeric as $$
begin
  raise notice 'non-variadic function called';
  return $1;
end;
$$ language plpgsql immutable strict;

select pleast(10);
select pleast(1,-2,3.3);

drop function pleast(numeric[]);
drop function pleast(numeric);


-- table function
create or replace function tfunc(variadic char[]) returns table (id int, tx char) as 
$$ select id, unnest($1) || ', ' ||  lname || '.' || fname from people order by 2
$$ language sql strict;

select * from tfunc ('hello', 'morning');

drop table people;
drop function tfunc(variadic char[]);

set optimizer_trace_fallback to on;
drop table if exists foo;
create table foo (a int,b int, c_json json);
insert into foo values (1,1,'{"1":"10"}');
insert into foo values (2,2,'{"2":"20"}');
insert into foo values (3,3,'{"3":"30"}');

select *, (json_extract_path_text(c_json, '1'))  AS jsonValues from foo;

select json_build_array(VARIADIC '{a,b,c}'::text[]);

drop table if exists foo;

set optimizer_trace_fallback to off;
