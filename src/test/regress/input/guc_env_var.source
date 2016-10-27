--
-- test environment variables on QD would not effect GUCs on segments
--
drop table if exists guc_env_tbl;
create table guc_env_tbl (d date);
insert into guc_env_tbl values ('1401-01-01');

-- pg_regress framework would set this to 'Postgres'
show datestyle;

select CASE WHEN d::text < 10::text THEN 1 ELSE 2 END from guc_env_tbl;

-- ensure no Gang is reused
set gp_vmem_idle_resource_timeout = 1;

create or replace function udf_setenv(cstring, cstring) returns bool as
'@abs_builddir@/regress@DLSUFFIX@', 'udf_setenv' LANGUAGE C;

create or replace function udf_unsetenv(cstring) returns bool as
'@abs_builddir@/regress@DLSUFFIX@', 'udf_unsetenv' LANGUAGE C;

-- set QD environment variable
select udf_setenv('PGDATESTYLE', 'ISO, YMD');

-- sleep to ensure the existing Gang has been destroyed
\! sleep 0.5

select CASE WHEN d::text < 10::text THEN 1 ELSE 2 END from guc_env_tbl;

select udf_unsetenv('PGDATESTYLE');
