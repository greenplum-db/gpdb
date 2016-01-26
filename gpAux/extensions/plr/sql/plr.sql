\set ECHO all
\i plr.sql

-- make typenames available in the global namespace
select load_r_typenames();


CREATE TABLE plr_modules (
  modseq int4,
  modsrc text
);

INSERT INTO plr_modules VALUES (0, 'pg.test.module.load <-function(msg) {print(msg)}');
select reload_plr_modules();

--
-- user defined R function test
--
select install_rcmd('pg.test.install <-function(msg) {print(msg)}');
create or replace function pg_test_install(text) returns text as 'pg.test.install(arg1)' language 'plr';
select pg_test_install('hello world');

--
-- a variety of plr functions
--
create or replace function throw_notice(text) returns text as 'pg.thrownotice(arg1)' language 'plr';
select throw_notice('hello');

create or replace function paste(_text,_text,text) returns text[] as 'paste(arg1,arg2, sep = arg3)' language 'plr';
select paste('{hello, happy}','{world, birthday}',' ');

create or replace function vec(_float8) returns _float8 as 'arg1' language 'plr';
select vec('{1.23, 1.32}'::float8[]);

create or replace function vec(float, float) returns _float8 as 'c(arg1,arg2)' language 'plr';
select vec(1.23, 1.32);

create or replace function echo(text) returns text as 'print(arg1)' language 'plr';
select echo('hello');

create or replace function reval(text) returns text as 'eval(parse(text = arg1))' language 'plr';
select reval('a <- sd(c(1,2,3)); b <- mean(c(1,2,3)); a + b');

create or replace function "commandArgs"() returns text[] as '' language 'plr';
select "commandArgs"();

create or replace function vec(float) returns text as 'c(arg1)' language 'plr';
select vec(1.23);

create or replace function reval(_text) returns text as 'eval(parse(text = arg1))' language 'plr';
select round(reval('{"sd(c(1.12,1.23,1.18,1.34))"}'::text[])::numeric,8);

create or replace function print(text) returns text as '' language 'plr';
select print('hello');

create or replace function rcube(int) returns float as 'sq <- function(x) {return(x * x)}; return(arg1 * sq(arg1))' language 'plr';
select rcube(3);

create or replace function sd(_float8) returns float as 'sd(arg1)' language 'plr';
select round(sd('{1.23,1.31,1.42,1.27}'::_float8)::numeric,8);

create or replace function sd(_float8) returns float as '' language 'plr';
select round(sd('{1.23,1.31,1.42,1.27}'::_float8)::numeric,8);

create or replace function mean(_float8) returns float as '' language 'plr';
select mean('{1.23,1.31,1.42,1.27}'::_float8);

create or replace function sprintf(text,text,text) returns text as 'sprintf(arg1,arg2,arg3)' language 'plr';
select sprintf('%s is %s feet tall', 'Sven', '7');

--
-- test aggregates
--

create table foo(f0 int, f1 text, f2 float8) with oids;
insert into foo values(1,'cat1',1.21);
insert into foo values(2,'cat1',1.24);
insert into foo values(3,'cat1',1.18);
insert into foo values(4,'cat1',1.26);
insert into foo values(5,'cat1',1.15);
insert into foo values(6,'cat2',1.15);
insert into foo values(7,'cat2',1.26);
insert into foo values(8,'cat2',1.32);
insert into foo values(9,'cat2',1.30);

create or replace function r_median(_float8) returns float as 'median(arg1)' language 'plr';
select r_median('{1.23,1.31,1.42,1.27}'::_float8);
CREATE AGGREGATE "median" (sfunc = plr_array_accum, basetype = float8, stype = _float8, finalfunc = r_median);
select f1, "median"(f2) from foo group by f1 order by f1;

--create or replace function r_gamma(_float8) returns float as 'gamma(arg1)' language 'plr';
--select round(r_gamma('{1.23,1.31,1.42,1.27}'::_float8)::numeric,8);
--CREATE AGGREGATE gamma (sfunc = plr_array_accum, basetype = float8, stype = _float8, finalfunc = r_gamma);
--select f1, round(gamma(f2)::numeric,8) from foo group by f1 order by f1;

--
-- test returning vectors, arrays, matricies, and dataframes
-- as scalars, arrays, and records
--
create or replace function test_vt() returns text as 'array(1:10,c(2,5))' language 'plr';
select test_vt();

create or replace function test_vi() returns int as 'array(1:10,c(2,5))' language 'plr';
select test_vi();

create or replace function test_mt() returns text as 'as.matrix(array(1:10,c(2,5)))' language 'plr';
select test_mt();

create or replace function test_mi() returns int as 'as.matrix(array(1:10,c(2,5)))' language 'plr';
select test_mi();

create or replace function test_dt() returns text as 'as.data.frame(array(1:10,c(2,5)))[[1]]' language 'plr';
select test_dt();

create or replace function test_di() returns int as 'as.data.frame(array(1:10,c(2,5)))[[1]]' language 'plr';
select test_di() as error;

create or replace function test_vta() returns text[] as 'array(1:10,c(2,5))' language 'plr';
select test_vta();

create or replace function test_via() returns int[] as 'array(1:10,c(2,5))' language 'plr';
select test_via();

create or replace function test_mta() returns text[] as 'as.matrix(array(1:10,c(2,5)))' language 'plr';
select test_mta();

create or replace function test_mia() returns int[] as 'as.matrix(array(1:10,c(2,5)))' language 'plr';
select test_mia();

create or replace function test_dia() returns int[] as 'as.data.frame(array(1:10,c(2,5)))' language 'plr';
select test_dia();

create or replace function test_dta() returns text[] as 'as.data.frame(array(1:10,c(2,5)))' language 'plr';
select test_dta();

create or replace function test_dta1() returns text[] as 'as.data.frame(array(letters[1:10], c(2,5)))' language 'plr';
select test_dta1();

create or replace function test_dta2() returns text[] as 'as.data.frame(data.frame(letters[1:10],1:10))' language 'plr';
select test_dta2();

-- generates expected error
create or replace function test_dia1() returns int[] as 'as.data.frame(array(letters[1:10], c(2,5)))' language 'plr';
select test_dia1() as error;

create or replace function test_dtup() returns setof record as 'data.frame(letters[1:10],1:10)' language 'plr';
select * from test_dtup() as t(f1 text, f2 int);

create or replace function test_mtup() returns setof record as 'as.matrix(array(1:15,c(5,3)))' language 'plr';
select * from test_mtup() as t(f1 int, f2 int, f3 int);

create or replace function test_vtup() returns setof record as 'as.vector(array(1:15,c(5,3)))' language 'plr';
select * from test_vtup() as t(f1 int);

create or replace function test_vint() returns setof int as 'as.vector(array(1:15,c(5,3)))' language 'plr';
select * from test_vint();

--
-- try again with named tuple types
--
CREATE TYPE dtup AS (f1 text, f2 int);
CREATE TYPE mtup AS (f1 int, f2 int, f3 int);
CREATE TYPE vtup AS (f1 int);

create or replace function test_dtup1() returns setof dtup as 'data.frame(letters[1:10],1:10)' language 'plr';
select * from test_dtup1();

create or replace function test_dtup2() returns setof dtup as 'data.frame(c("c","qw","ax","h","k","ax","l","t","b","u"),1:10)' language 'plr';
select * from test_dtup2();

create or replace function test_mtup1() returns setof mtup as 'as.matrix(array(1:15,c(5,3)))' language 'plr';
select * from test_mtup1();

create or replace function test_vtup1() returns setof vtup as 'as.vector(array(1:15,c(5,3)))' language 'plr';
select * from test_vtup1();



--
-- test pg R support functions (e.g. SPI_exec)
--
create or replace function pg_quote_ident(text) returns text as 'pg.quoteident(arg1)' language 'plr';
select pg_quote_ident('Hello World');

create or replace function pg_quote_literal(text) returns text as 'pg.quoteliteral(arg1)' language 'plr';
select pg_quote_literal('Hello''World');

create or replace function test_spi_t(text) returns text as '(pg.spi.exec(arg1))[[1]]' language 'plr';
select test_spi_t('select oid, typname from pg_type where typname = ''oid'' or typname = ''text''');

create or replace function test_spi_ta(text) returns text[] as 'pg.spi.exec(arg1)' language 'plr';
select test_spi_ta('select oid, typname from pg_type where typname = ''oid'' or typname = ''text''');

create or replace function test_spi_tup(text) returns setof record as 'pg.spi.exec(arg1)' language 'plr';
select * from test_spi_tup('select oid, typname from pg_type where typname = ''oid'' or typname = ''text''') as t(typeid oid, typename name);

create or replace function fetch_pgoid(text) returns int as 'pg.reval(arg1)' language 'plr';
select fetch_pgoid('BYTEAOID');

create or replace function test_spi_prep(text) returns text as 'sp <<- pg.spi.prepare(arg1, c(NAMEOID, NAMEOID)); print("OK")' language 'plr';
select test_spi_prep('select oid, typname from pg_type where typname = $1 or typname = $2');

create or replace function test_spi_execp(text, text, text) returns setof record as 'pg.spi.execp(pg.reval(arg1), list(arg2,arg3))' language 'plr';
select * from test_spi_execp('sp','oid','text') as t(typeid oid, typename name);

create or replace function test_spi_lastoid(text) returns text as 'pg.spi.exec(arg1); pg.spi.lastoid()/pg.spi.lastoid()' language 'plr';
select test_spi_lastoid('insert into foo values(10,''cat3'',3.333)') as "ONE";

--
-- test NULL handling
--
CREATE OR REPLACE FUNCTION r_test (float8) RETURNS float8 AS 'arg1' LANGUAGE 'plr';
select r_test(null) is null as "NULL";

CREATE OR REPLACE FUNCTION r_max (integer, integer) RETURNS integer AS 'if (is.null(arg1) && is.null(arg2)) return(NA);if (is.null(arg1)) return(arg2);if (is.null(arg2)) return(arg1);if (arg1 > arg2) return(arg1);arg2' LANGUAGE 'plr';
select r_max(1,2) as "TWO";
select r_max(null,2) as "TWO";
select r_max(1,null) as "ONE";
select r_max(null,null) is null as "NULL";

--
-- test tuple arguments
--
create or replace function get_foo(int) returns foo as 'select * from foo where f0 = $1' language 'sql';
create or replace function test_foo(foo) returns foo as 'return(arg1)' language 'plr';
select * from test_foo(get_foo(1));

--
-- test 2D array argument
--
create or replace function test_in_m_tup(_int4) returns setof record as 'arg1' language 'plr';
select * from test_in_m_tup('{{1,3,5},{2,4,6}}') as t(f1 int, f2 int, f3 int);

--
-- test 3D array argument
--
create or replace function arr3d(_int4,int4,int4,int4) returns int4 as '
if (arg2 < 1 || arg3 < 1 || arg4 < 1)
  return(NA)
if (arg2 > dim(arg1)[1] || arg3 > dim(arg1)[2] || arg4 > dim(arg1)[3])
  return(NA)
return(arg1[arg2,arg3,arg4])
' language 'plr' WITH (isstrict);

select arr3d('{{{111,112},{121,122},{131,132}},{{211,212},{221,222},{231,232}}}',2,3,1) as "231";
-- for sake of comparison, see what normal pgsql array operations produces
select f1[2][3][1] as "231" from (select '{{{111,112},{121,122},{131,132}},{{211,212},{221,222},{231,232}}}'::int4[] as f1) as t;

-- out-of-bounds, returns null
select arr3d('{{{111,112},{121,122},{131,132}},{{211,212},{221,222},{231,232}}}',1,4,1) is null as "NULL";
select f1[1][4][1] is null as "NULL" from (select '{{{111,112},{121,122},{131,132}},{{211,212},{221,222},{231,232}}}'::int4[] as f1) as t;
select arr3d('{{{111,112},{121,122},{131,132}},{{211,212},{221,222},{231,232}}}',0,1,1) is null as "NULL";
select f1[0][1][1] is null as "NULL" from (select '{{{111,112},{121,122},{131,132}},{{211,212},{221,222},{231,232}}}'::int4[] as f1) as t;

--
-- test 3D array return value
--
create or replace function arr3d(_int4) returns int4[] as 'return(arg1)' language 'plr' WITH (isstrict);
select arr3d('{{{111,112},{121,122},{131,132}},{{211,212},{221,222},{231,232}}}');

--
-- Trigger support tests
--

--
-- test that NULL return value suppresses the change
--
create or replace function rejectfoo() returns trigger as 'return(NULL)' language plr;
create trigger footrig before insert or update or delete on foo for each row execute procedure rejectfoo();
select count(*) from foo;
insert into foo values(11,'cat99',1.89);
select count(*) from foo;
update foo set f1 = 'zzz';
select count(*) from foo;
delete from foo;
select count(*) from foo;
drop trigger footrig on foo;

--
-- test that returning OLD/NEW as appropriate allow the change unmodified
--
create or replace function acceptfoo() returns trigger as '
switch (pg.tg.op, INSERT = return(pg.tg.new), UPDATE = return(pg.tg.new), DELETE = return(pg.tg.old))
' language plr;
create trigger footrig before insert or update or delete on foo for each row execute procedure acceptfoo();
select count(*) from foo;
insert into foo values(11,'cat99',1.89);
select count(*) from foo;
update foo set f1 = 'zzz' where f0 = 11;
select * from foo where f0 = 11;
delete from foo where f0 = 11;
select count(*) from foo;
drop trigger footrig on foo;

--
-- test that returning modifed tuple successfully modifies the result
--
create or replace function modfoo() returns trigger as '
if (pg.tg.op == "INSERT")
{
  retval <- pg.tg.new
  retval$f1 <- "xxx"
}
if (pg.tg.op == "UPDATE")
{
  retval <- pg.tg.new
  retval$f1 <- "aaa"
}
if (pg.tg.op == "DELETE")
  retval <- pg.tg.old
return(retval)
' language plr;
create trigger footrig before insert or update or delete on foo for each row execute procedure modfoo();
select count(*) from foo;
insert into foo values(11,'cat99',1.89);
select * from foo where f0 = 11;
update foo set f1 = 'zzz' where f0 = 11;
select * from foo where f0 = 11;
delete from foo where f0 = 11;
select count(*) from foo;
drop trigger footrig on foo;

--
-- test statement level triggers and verify all arguments come
-- across correctly
--
create or replace function foonotice() returns trigger as '
msg <- paste(pg.tg.name,pg.tg.relname,pg.tg.when,pg.tg.level,pg.tg.op,pg.tg.args[1],pg.tg.args[2])
pg.thrownotice(msg)
return(NULL)
' language plr;

create trigger footrig after insert or update or delete on foo for each row execute procedure foonotice();
select count(*) from foo;
insert into foo values(11,'cat99',1.89);
select count(*) from foo;
update foo set f1 = 'zzz' where f0 = 11;
select * from foo where f0 = 11;
delete from foo where f0 = 11;
select count(*) from foo;
drop trigger footrig on foo;

create trigger footrig after insert or update or delete on foo for each statement execute procedure foonotice('hello','world');
select count(*) from foo;
insert into foo values(11,'cat99',1.89);
select count(*) from foo;
update foo set f1 = 'zzz' where f0 = 11;
select * from foo where f0 = 11;
delete from foo where f0 = 11;
select count(*) from foo;
drop trigger footrig on foo;

-- Test cursors: creating, scrolling forward, closing
CREATE OR REPLACE FUNCTION cursor_fetch_test(integer,boolean) RETURNS SETOF integer AS 'plan<-pg.spi.prepare("SELECT * FROM generate_series(1,10)"); cursor<-pg.spi.cursor_open("curs",plan); dat<-pg.spi.cursor_fetch(cursor,arg2,arg1); pg.spi.cursor_close(cursor); return (dat);' language 'plr';
SELECT * FROM cursor_fetch_test(1,true);
SELECT * FROM cursor_fetch_test(2,true);
SELECT * FROM cursor_fetch_test(20,true);

--Test cursors: scrolling backwards
CREATE OR REPLACE FUNCTION cursor_direction_test() RETURNS SETOF integer AS'plan<-pg.spi.prepare("SELECT * FROM generate_series(1,10)"); cursor<-pg.spi.cursor_open("curs",plan); dat<-pg.spi.cursor_fetch(cursor,TRUE,as.integer(3)); dat2<-pg.spi.cursor_fetch(cursor,FALSE,as.integer(3)); pg.spi.cursor_close(cursor); return (dat2);' language 'plr';
SELECT * FROM cursor_direction_test();

--Test cursors: Passing arguments to a plan
CREATE OR REPLACE FUNCTION cursor_fetch_test_arg(integer) RETURNS SETOF integer AS 'plan<-pg.spi.prepare("SELECT * FROM generate_series(1,$1)",c(INT4OID)); cursor<-pg.spi.cursor_open("curs",plan,list(arg1)); dat<-pg.spi.cursor_fetch(cursor,TRUE,arg1); pg.spi.cursor_close(cursor); return (dat);' language 'plr';
SELECT * FROM cursor_fetch_test_arg(3);

--Test bytea arguments and return values: serialize/unserialize
create or replace function test_serialize(text)
returns bytea as '
 mydf <- pg.spi.exec(arg1)
 return (mydf)
' language 'plr';

create or replace function restore_df(bytea)
returns setof record as '
 return (arg1)
' language 'plr';

select * from restore_df((select test_serialize('select oid, typname from pg_type where typname in (''oid'',''name'',''int4'')'))) as t(oid oid, typname name);

--now cleaning 
DROP FUNCTION plr_call_handler() cascade; 
DROP TYPE IF EXISTS plr_environ_type cascade; 
DROP TYPE IF EXISTS r_typename cascade; 
DROP TYPE IF EXISTS r_version_type CASCADE; 
DROP TABLE IF EXISTS plr_modules CASCADE; 
DROP TYPE IF EXISTS dtup CASCADE; 
DROP TYPE IF EXISTS mtup CASCADE; 
DROP TYPE IF EXISTS vtup CASCADE; 
drop table if exists foo cascade; 
