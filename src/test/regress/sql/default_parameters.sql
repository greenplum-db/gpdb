-- -----------------------------------------------------------------
-- Test function default paramters
-- -----------------------------------------------------------------
-- test default polymorphic functions
-- test basic functionality
create function dfunc(a int = 1, int = 2) returns int as $$ select $1 + $2; $$ language sql;
select dfunc();
select dfunc(10);
select dfunc(10, 20);
select dfunc(10, 20, 30);  -- fail
drop function dfunc();  -- fail
drop function dfunc(int);  -- fail
drop function dfunc(int, int);  -- ok

-- fail: defaults must be at end of argument list
create function dfunc(a int = 1, b int) returns int as $$ select $1 + $2; $$ language sql;

-- however, this should work:
create function dfunc(a int = 1, out sum int, b int = 2) as $$ select $1 + $2; $$ language sql;
select dfunc();

-- verify it lists properly
\df dfunc

-- inout argmode test:
drop function dfunc(int, int);
create function dfunc(a inout int default 10, b int = 2) as $$ select $1 + $2; $$ language sql;
select dfunc();
select dfunc(4);
select dfunc(4,5);

\df dfunc
drop function dfunc(int, int);

-- check implicit coercion
create function dfunc(a int DEFAULT 1.0, int DEFAULT '-1') returns int as $$ select $1 + $2; $$ language sql;
select dfunc();

create function dfunc(a text DEFAULT 'Hello', b text DEFAULT 'World') returns text as $$ select $1 || ', ' || $2; $$ language sql;
select dfunc();  -- fail: which dfunc should be called? int or text
select dfunc('Hi');  -- ok
select dfunc('Hi', 'City');  -- ok
select dfunc(0);  -- ok
select dfunc(10, 20);  -- ok
drop function dfunc(int, int);
drop function dfunc(text, text);

-- ambiguity should be reported only if there's not a better match available
create function dfunc(text) returns text as $$ select $1; $$ language sql;
create function dfunc(int = 1, int = 2) returns int as $$ select 2; $$ language sql;
create function dfunc(int = 1, int = 2, int = 3, int = 4) returns int as $$ select 4; $$ language sql;

-- now, dfunc(nargs = 2) and dfunc(nargs = 4) are ambiguous when called
-- with 0 to 2 arguments with input type int
select dfunc('Hi');	-- ok, input type text
select dfunc();  -- fail
select dfunc(1);  -- fail
select dfunc(1, 2);  -- fail
select dfunc(1, 2, 3);  -- ok
select dfunc(1, 2, 3, 4);  -- ok
drop function dfunc(text);
drop function dfunc(int, int);
drop function dfunc(int, int, int, int);

-- default values are not allowed for output parameters
create function dfunc(out int = 20) returns int as $$ select 1; $$ language sql;

-- polymorphic parameter test
create function dfunc(anyelement = 'World'::text) returns text as $$ select 'Hello, ' || $1::text; $$ language sql;
select dfunc();

select dfunc(0);
select dfunc(to_date('20081215','YYYYMMDD'));
select dfunc('City'::text);
drop function dfunc(anyelement);

-- check defaults for variadics
create function dfunc(a variadic int[]) returns int as
$$ select array_upper($1, 1) $$ language sql;
select dfunc();  -- fail
select dfunc(10);
select dfunc(10,20);

create or replace function dfunc(a variadic int[] default '{44,88,99}'::int[]) returns int as $$ select array_upper($1, 1) $$ language sql;
select dfunc();  -- now ok
select dfunc(10);
select dfunc(10,20);

-- can change default value
create or replace function dfunc(a variadic int[] default '{44,55,88,99}'::int[]) returns int as $$ select array_upper($1, 1) $$ language sql;
select dfunc();

-- can't remove the default once it exists
create or replace function dfunc(a variadic int[]) returns int as $$ select array_upper($1, 1) $$ language sql;
\df dfunc
drop function dfunc(a variadic int[]);

-- test expression
create function dfunc(a int, b double precision default random()*10) returns double precision as $$ select $1+$2; $$ language sql;
\df dfunc

create view dview as select random()*10, dfunc(4);
select pg_get_viewdef('dview',true);

-- need drop cascade
drop function dfunc(int, double precision);
drop function dfunc(int, double precision) cascade;
