-- -----------------------------------------------------------------
-- Test function variadic parameters
--
-- PostgreSQL has tests for variadic parameters in 'polymorphism'
-- and 'plpgsql' tests. This file contains a few extra ones.
-- -----------------------------------------------------------------
-- deparse view
create or replace function concat(text, variadic anyarray) returns text as $$
  select array_to_string($2, $1);
$$ language sql immutable strict;

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

-- PLPGSQL
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
