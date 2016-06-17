
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema create_table_default_distribution_on;
set search_path to create_table_default_distribution_on;
set gp_create_table_random_default_distribution=on;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: sql/create_table_as.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS distpol;
create table distpol as select random(), 1 as a, 2 as b;

DROP TABLE IF EXISTS hobbies_r;
CREATE TABLE hobbies_r (
	name		text,
	person 		text
);

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT * FROM hobbies_r;
-- end_ignore

set optimizer=on;

-- Make sure random default distribution works for CTAS
select attrnums from gp_distribution_policy where
  localoid = 'distpol'::regclass;

-- Test RANDOM default distribution with AS clause containing a SELECT block
select attrnums from gp_distribution_policy where localoid='bar'::regclass;

-- Test RANDOM distribution with ON COMMIT option
begin;

drop table if exists r3_1;
create temp table r3_1 on commit preserve rows as select 10 as a1, 20 as b1, 30 as c1, 40 as d1;
select attrnums from gp_distribution_policy where localoid='r3_1'::regclass;

drop table if exists r3_2;
create temp table r3_2 on commit delete rows as select 10 as a2, 20 as b2, 30 as c2, 40 as d2;
select attrnums from gp_distribution_policy where localoid='r3_2'::regclass;

drop table if exists r3_3;
create temp table r3_3 on commit drop as select 10 as a3, 20 as b3, 30 as c3, 40 as d3;
select attrnums from gp_distribution_policy where localoid='r3_3'::regclass;

end;


set optimizer=off;

-- Make sure random default distribution works for CTAS
select attrnums from gp_distribution_policy where
  localoid = 'distpol'::regclass;

-- Test RANDOM default distribution with AS clause containing a SELECT block
select attrnums from gp_distribution_policy where localoid='bar'::regclass;

-- Test RANDOM distribution with ON COMMIT option
begin;

drop table if exists r3_1;
create temp table r3_1 on commit preserve rows as select 10 as a1, 20 as b1, 30 as c1, 40 as d1;
select attrnums from gp_distribution_policy where localoid='r3_1'::regclass;

drop table if exists r3_2;
create temp table r3_2 on commit delete rows as select 10 as a2, 20 as b2, 30 as c2, 40 as d2;
select attrnums from gp_distribution_policy where localoid='r3_2'::regclass;

drop table if exists r3_3;
create temp table r3_3 on commit drop as select 10 as a3, 20 as b3, 30 as c3, 40 as d3;
select attrnums from gp_distribution_policy where localoid='r3_3'::regclass;

end;

-- ----------------------------------------------------------------------
-- Test: sql/create_table_inherits.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS person CASCADE;
CREATE TABLE person (
	name 		text,
	age			int4,
	location 	point
) DISTRIBUTED BY (name);

CREATE TABLE staff_member (
	salary 		int4,
	manager 	name
) INHERITS (person) WITH OIDS;

CREATE TABLE student (
	gpa 		float8
) INHERITS (person);

CREATE TABLE stud_emp (
	percent 	int4
) INHERITS (staff_member, student);
-- end_ignore

select attrnums from gp_distribution_policy where
  localoid = 'staff_member'::regclass;

select attrnums from gp_distribution_policy where
  localoid = 'student'::regclass;

select attrnums from gp_distribution_policy where
  localoid = 'stud_emp'::regclass;

-- ----------------------------------------------------------------------
-- Test: sql/create_table_like.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS person CASCADE;
CREATE TABLE person (
	name 		text,
	age			int4,
	location 	point
) DISTRIBUTED BY (name);

DROP TABLE IF EXISTS person_copy;
CREATE TABLE person_copy (LIKE person);
-- end_ignore

select attrnums from gp_distribution_policy where
  localoid = 'person'::regclass;

-- test that LIKE clause does not affect default distribution
select attrnums from gp_distribution_policy where
  localoid = 'person_copy'::regclass;

-- ----------------------------------------------------------------------
-- Test: sql_gpdb/create_table_unique.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop table if exists distpol;

create table distpol(c1 int, c2 text) distributed by (c1);
-- end_ignore

-- UNIQUE default behavior
DROP TABLE IF EXISTS dupconstr;
create table dupconstr (
						i int,
						j int constraint test CHECK (j > 10),
						CONSTRAINT test1 UNIQUE (i,j));

-- UNIQUE default behavior
create unique index distpol_uidx on distpol(c1);


-- ----------------------------------------------------------------------
-- Test: sql_default_distribution_sensitive/create_table_basic.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS hobbies_r;
CREATE TABLE hobbies_r (
	name		text,
	person 		text
);

DROP TABLE IF EXISTS tenk1;
CREATE TABLE tenk1 (
	unique1		int4,
	unique2		int4,
	two			int4,
	four		int4,
	ten			int4,
	twenty		int4,
	hundred		int4,
	thousand	int4,
	twothousand	int4,
	fivethous	int4,
	tenthous	int4,
	odd			int4,
	even		int4,
	stringu1	name,
	stringu2	name,
	string4		name
) WITH OIDS;

-- end_ignore

select attrnums from gp_distribution_policy where
  localoid = 'hobbies_r'::regclass;

select attrnums from gp_distribution_policy where
  localoid = 'tenk1'::regclass;

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema create_table_default_distribution_on cascade;
-- end_ignore
