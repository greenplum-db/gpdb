-- ---------------------------------------------------------------------
-- caqlcov
--
-- This test aims to cover syscache calls that are missed in the rest of
-- the installcheck-good schedule.
-- ---------------------------------------------------------------------

-- start_ignore
drop schema if exists tschema;
drop table if exists ttable;
drop table if exists ttable1;
drop table if exists ttable2;
drop table if exists ttable_seq;
drop resource queue myqueue;
drop function if exists trig();
drop user if exists cm_user;
drop view if exists tview;
drop database if exists ctestdb;
-- end_ignore

create user caql_luser;
create user caql_luser_beta;
-- ---------------------------------------------------------------------
-- coverage for comment.c
-- ---------------------------------------------------------------------

create schema tschema;
comment on schema tschema is 'this is to test comment on schema';

create table ttable (a int, b int, constraint testcheck check(b>0));
comment on constraint testcheck on ttable is 'this is to test comment on constraint';


create user cm_user;
set session authorization cm_user;
comment on operator class pg_catalog.abstime_ops USING btree IS '4 byte integer operators for btree';
reset session authorization;

create resource queue myqueue with (active_statements=10);
comment on resource queue myqueue is 'only 10 active statements';


-- ---------------------------------------------------------------------
-- coverage for trigger.c
-- ---------------------------------------------------------------------

create function trig() returns trigger as
$$
begin
raise notice 'testing trigger';
end;
$$ language plpgsql NO SQL;

create trigger btrig before insert on ttable for each row execute procedure trig();
alter trigger btrig on ttable rename to btrig_2;

alter table ttable disable trigger btrig_2;
alter table ttable enable trigger all;

-- ---------------------------------------------------------------------
-- coverage for ruleutils.c
-- ---------------------------------------------------------------------
\d+ ttable

create view tview as select a,~b from ttable;
select pg_get_viewdef('tview');

create table ttable_seq (a int, b serial);
select pg_get_serial_sequence('ttable_seq', 'b');

-- ---------------------------------------------------------------------
-- coverage for tablecmd.c
-- ---------------------------------------------------------------------

alter table ttable reset (fillfactor);

create index indtest on ttable using btree(b);
alter table ttable rename column b to c;

begin;
set transaction read only;
drop table ttable;
rollback;

-- ---------------------------------------------------------------------
-- coverage for parse_func.c
-- ---------------------------------------------------------------------

create table ttable1 (a int, b int);
create table ttable2() inherits (ttable1);
select (ttable2.*)::ttable1 from ttable2;

-- ---------------------------------------------------------------------
-- coverage for aggregatecmds.c
-- ---------------------------------------------------------------------

create function caql_cube_fn(numeric, numeric) RETURNS numeric
AS 'select $1 + $2 * $2 * $2'
language sql CONTAINS SQL
immutable
returns NULL ON NULL INPUT;

create aggregate caqlcube(numeric) (
SFUNC = caql_cube_fn,
STYPE = numeric,
INITCOND = 0 );

create table caql_tab(a int);
insert into caql_tab values (1), (2), (3);

select caqlcube(a) from caql_tab;

alter aggregate caqlcube(numeric) rename to caql_cube;

select caql_cube(a) from caql_tab;

drop table caql_tab;

-- ---------------------------------------------------------------------
-- coverage for indexcmds.c
-- ---------------------------------------------------------------------

create schema caql_schema;

create function caql_schema.int4_array_lt(_int4, _int4) returns bool as 'array_lt' language internal;
create function caql_schema.int4_array_le(_int4, _int4) returns bool as 'array_le' language internal;
create function caql_schema.int4_array_eq(_int4, _int4) returns bool as 'array_eq' language internal;
create function caql_schema.int4_array_ge(_int4, _int4) returns bool as 'array_ge' language internal;
create function caql_schema.int4_array_gt(_int4, _int4) returns bool as 'array_gt' language internal;
create function caql_schema.int4_array_cmp(_int4, _int4) returns int as 'btarraycmp' language internal;

create operator caql_schema.<  (leftarg = _int4, rightarg = _int4, procedure = caql_schema.int4_array_lt);
create operator caql_schema.<= (leftarg = _int4, rightarg = _int4, procedure = caql_schema.int4_array_le);
create operator caql_schema.=  (leftarg = _int4, rightarg = _int4, procedure = caql_schema.int4_array_eq);
create operator caql_schema.>= (leftarg = _int4, rightarg = _int4, procedure = caql_schema.int4_array_ge);
create operator caql_schema.>  (leftarg = _int4, rightarg = _int4, procedure = caql_schema.int4_array_gt);

create operator class caql_schema.caql_opclass default for type _int4 using btree as
operator 1 caql_schema.<,
operator 2 caql_schema.<=,
operator 3 caql_schema.=,
operator 4 caql_schema.>=,
operator 5 caql_schema.>,
function 1 caql_schema.int4_array_cmp(_int4, _int4);

create table caql_schema.caql_tab(a int[]);
create index caql_index on caql_schema.caql_tab(a caql_schema.caql_opclass);

create table caql_rtree(a int, b box);
create index caql_rtree_idx on caql_rtree using rtree(b);
drop table caql_rtree;

reindex table caql_schema.caql_tab;

create database caql_db;

\c caql_db;
set client_min_messages to WARNING;
reindex database caql_db;
analyze;
vacuum freeze;
reset client_min_messages;
\c regression;

reindex table pg_class;

drop table caql_schema.caql_tab;

-- ---------------------------------------------------------------------
-- coverage for opclasscmds.c
-- ---------------------------------------------------------------------

alter operator class caql_schema.caql_opclass using btree rename to caql_opclass2;
alter operator class caql_schema.caql_opclass2 using btree owner to caql_luser;
drop operator class caql_schema.caql_opclass2 using btree;

-- there is no way to rename to another schema..., so re-create it
create operator class caql_opclass default for type _int4 using btree as
operator 1 caql_schema.<,
operator 2 caql_schema.<=,
operator 3 caql_schema.=,
operator 4 caql_schema.>=,
operator 5 caql_schema.>,
function 1 caql_schema.int4_array_cmp(_int4, _int4);
alter operator class caql_opclass using btree rename to caql_opclass2;
alter operator class caql_opclass2 using btree owner to caql_luser;
drop operator class caql_opclass2 using btree;

-- ---------------------------------------------------------------------
-- coverage for operatorcmds.c
-- ---------------------------------------------------------------------

alter operator caql_schema.<(_int4, _int4) owner to caql_luser;

-- clean up
drop operator caql_schema.<  (_int4, _int4);
drop operator caql_schema.<= (_int4, _int4);
drop operator caql_schema.=  (_int4, _int4);
drop operator caql_schema.>= (_int4, _int4);
drop operator caql_schema.>  (_int4, _int4);

drop function caql_schema.int4_array_cmp(_int4, _int4);
drop function caql_schema.int4_array_lt(_int4, _int4);
drop function caql_schema.int4_array_le(_int4, _int4);
drop function caql_schema.int4_array_eq(_int4, _int4);
drop function caql_schema.int4_array_ge(_int4, _int4);
drop function caql_schema.int4_array_gt(_int4, _int4);

-- ---------------------------------------------------------------------
-- coverage for namespace.c
-- ---------------------------------------------------------------------

create type caql_type as (id int, grade int);
-- start_ignore
-- for now, we just want have coverage for caql caller and ignore the result
\dT;

\do;
-- end_ignore

select * from pg_opclass_is_visible(403);

-- ---------------------------------------------------------------------
-- coverage for dbcmds.c
-- ---------------------------------------------------------------------

alter database caql_db rename to caql_database;

alter database caql_database with connection limit 200;

create role caql_user with login nosuperuser nocreatedb;
alter database caql_database owner to caql_user;

set role caql_user;
alter database caql_database rename to caql_db;
drop database caql_database;
reset role;

-- ---------------------------------------------------------------------
-- coverage for functioncmds.c
-- ---------------------------------------------------------------------

create function caql_fn(int, int) returns int
AS 'select $1 + $2'
language sql CONTAINS SQL
immutable
returns NULL ON NULL INPUT;

alter function caql_fn(int, int) rename to caql_function;

alter function caql_function(int, int) owner to caql_user;

alter aggregate caql_cube(numeric) owner to caql_user;

create function caql_fn_in(cstring) returns opaque as 'boolin' language internal;

create function caql_fn_out(opaque) returns opaque as 'boolin' language internal;

create type caql_type2(input=caql_fn_in, output=caql_fn_out);

create type caql_type3 as (name int4);
create function caql_type3_cast(caql_type3) returns int as
' select $1.name'
language sql CONTAINS SQL
immutable;

create cast (caql_type3 AS int4) WITH FUNCTION caql_type3_cast(caql_type3);
drop cast (caql_type3 as int4);

-- ---------------------------------------------------------------------
-- coverage for acl.c
-- ---------------------------------------------------------------------

select has_table_privilege(1255, 'SELECT');

select has_table_privilege((select usesysid from pg_user where usename='caql_user'), 1255, 'SELECT');

select has_database_privilege('caql_user', 1, 'CONNECT');

select has_database_privilege(1, 'CONNECT');

select has_database_privilege((select usesysid from pg_user where usename='caql_user'), 1, 'CONNECT');

select has_function_privilege('caql_user', (select oid from pg_proc where proname='boolin'), 'EXECUTE');

select has_function_privilege((select usesysid from pg_user where usename='caql_user'),'EXECUTE');

select has_function_privilege((select usesysid from pg_user where usename='caql_user'),
					(select oid from pg_proc where proname='boolin'), 'EXECUTE');

select has_language_privilege((select oid from pg_language where lanname='plpgsql'), 'USAGE');

select has_language_privilege((select usesysid from pg_user where usename='caql_user'),
				(select oid from pg_language where lanname = 'plpgsql'), 'USAGE');

select has_language_privilege((select usesysid from pg_user where usename='caql_user'), 'plpgsql', 'USAGE');

select has_schema_privilege('caql_user', (select oid from pg_namespace where nspname='caql_schema'), 'USAGE');

select has_schema_privilege((select oid from pg_namespace where nspname='caql_schema'), 'USAGE');

select has_schema_privilege((select usesysid from pg_user where usename='caql_user'),
				(select oid from pg_namespace where nspname='caql_schema'), 'USAGE');

select has_schema_privilege('caql_user', 'caql_schema', 'USAGE');

create role caql_role;

select pg_has_role('caql_user', 'caql_role', 'USAGE WITH ADMIN OPTION');

-- ---------------------------------------------------------------------
-- coverage for cdbpartition.c
-- ---------------------------------------------------------------------

create table caql_part_table
( c1 int,
  c2 int
)
with (appendonly=false)
partition by list (c1) subpartition by list(c2)
(
  partition a values(1)
        (subpartition a1 values (1)
                with (appendonly=true, orientation=column, compresstype=quicklz)
        )
);

drop table caql_part_table_1_prt_a_2_prt_a1;
drop table caql_part_table_1_prt_a;

drop table caql_part_table;

-- cleanup

drop aggregate caql_cube(numeric);
drop function caql_cube_fn(numeric, numeric);

drop type caql_type cascade;
drop function caql_type3_cast(caql_type3);
drop type caql_type3;

drop function caql_function(int, int);
drop schema caql_schema cascade;;
drop user caql_user;

drop function caql_fn_out(caql_type2) cascade;
drop function caql_fn_in(cstring);

drop role caql_role;

-- setup

-- start_ignore
drop schema tschema;
drop resource queue myqueue;
drop table ttable cascade;
drop table ttable1 cascade;
drop table ttable2 cascade;
drop table ttable_seq cascade;
drop function trig();
drop user cm_user;
-- end_ignore
create schema caql_schema;

-- ---------------------------------------------------------------------
-- coverage for typecmds.c
-- ---------------------------------------------------------------------

create domain caql_domain as int;
alter domain caql_domain set default 1;
alter domain caql_domain set not null;
alter domain caql_domain add constraint caql_domain_constraint check (value < 1000);
alter domain caql_domain drop constraint caql_domain_constraint;
alter domain caql_domain owner to caql_luser;
drop domain caql_domain;

-- ---------------------------------------------------------------------
-- coverage for user.c
-- ---------------------------------------------------------------------

alter user caql_luser rename to caql_user;
alter user caql_user rename to caql_luser;

-- ---------------------------------------------------------------------
-- coverage for proclang.c
-- ---------------------------------------------------------------------

create language caql_plpgsql handler plpgsql_call_handler;
drop language caql_plpgsql;

-- ---------------------------------------------------------------------
-- coverage for schemacmds.c
-- ---------------------------------------------------------------------

alter schema caql_schema owner to caql_luser;
reassign owned by caql_luser to caql_luser_beta;

-- ---------------------------------------------------------------------
-- coverage for sequence.c
-- ---------------------------------------------------------------------

create external web table cmd(a text)
  execute E'PGOPTIONS="-c gp_session_role=utility" \\
    psql -p $GP_MASTER_PORT $GP_DATABASE $GP_USER -c \\
      "create temporary sequence caql_sequence; \\
      select nextval(''caql_sequence''); select lastval();"'
  on master format 'text';
select * from cmd;
drop external web table cmd;

-- ---------------------------------------------------------------------
-- coverage for dbsize.c
-- ---------------------------------------------------------------------

select pg_total_relation_size('pg_class'::regclass) -
  pg_total_relation_size('pg_class'::regclass);

-- ---------------------------------------------------------------------
-- coverage for regproc.c
-- ---------------------------------------------------------------------

select oid::regoper from pg_operator order by oid limit 1;

-- ---------------------------------------------------------------------
-- coverage for dependency.c
-- ---------------------------------------------------------------------

create table caql_depend(a int,
  b regproc default 'int4pl',
  c regoperator default '=(bool, bool)',
  d regclass default 'pg_class',
  e regtype default 'bool');

-- ---------------------------------------------------------------------
-- coverage for aclchk.c
-- ---------------------------------------------------------------------

grant all on tablespace pg_default to caql_luser;
grant all on function random() to caql_luser;
set role to caql_luser;
select has_database_privilege('postgres', 'create');
select has_tablespace_privilege('pg_default', 'create');
comment on tablespace pg_default is 'pg_default';
comment on filespace pg_system is 'pg_system';
comment on operator class abstime_ops using btree is 'abstime_ops';
reset role;

create database ctestdb;
grant create on database ctestdb to caql_luser;
\c ctestdb
set session authorization caql_luser;
create schema tschema;
drop schema tschema;
reset session authorization;
\c regression

-- ---------------------------------------------------------------------
-- coverage for pg_depend.c
-- ---------------------------------------------------------------------

create sequence caql_sequence;
alter sequence caql_sequence set schema caql_schema;

revoke all privileges on tablespace pg_default from caql_luser;
revoke all privileges on function random() from caql_luser;
revoke all privileges on database ctestdb from caql_luser;

-- ---------------------------------------------------------------------
-- coverage for pg_operator.c
-- ---------------------------------------------------------------------

create operator @@ (procedure = int4pl, leftarg = int, rightarg = int, negator = !!);
create operator !! (procedure = int4pl, leftarg = int, rightarg = int);

-- ---------------------------------------------------------------------
-- coverage for planagg.c
-- ---------------------------------------------------------------------

create external web table cmd(a text)
  execute E'PGOPTIONS="-c gp_session_role=utility" \\
    psql -p $GP_MASTER_PORT $GP_DATABASE $GP_USER -c \\
      "select min(oid) from pg_class"'
  on master format 'text';
select * from cmd;
drop external web table cmd;

-- start_ignore
drop database ctestdb;
-- end_ignore

-- ---------------------------------------------------------------------
-- coverage for fmgr.c
-- ---------------------------------------------------------------------

create function security_definer_test() returns void as
$$
begin
    perform * from pg_class;
end;
$$
language plpgsql READS SQL DATA security definer;

select security_definer_test();

drop function security_definer_test();

-- clean up
drop schema caql_schema cascade;
drop role caql_luser;
drop role caql_luser_beta;
