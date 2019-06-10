-- Test effective distribution policy after different variants of CREATE TABLE

-- start_ignore
CREATE SCHEMA create_table_distpol;
SET search_path TO create_table_distpol;
-- end_ignore

-- Make sure random default distribution works for CTAS
SET gp_create_table_random_default_distribution=on;
DROP TABLE IF EXISTS distpol;
create table distpol as select random(), 1 as a, 2 as b;

select distkey from gp_distribution_policy where localoid = 'distpol'::regclass;


-- Test RANDOM default distribution with AS clause containing a SELECT block
CREATE TABLE distpol_hobbies_r (
  name text,
  person text
);

CREATE TABLE distpol_bar AS SELECT * FROM distpol_hobbies_r;

select distkey from gp_distribution_policy where localoid='distpol_bar'::regclass;

-- Test RANDOM distribution with ON COMMIT option
begin;

create temp table r3_1 on commit preserve rows as select 10 as a1, 20 as b1, 30 as c1, 40 as d1;
select distkey from gp_distribution_policy where localoid='r3_1'::regclass;

create temp table r3_2 on commit delete rows as select 10 as a2, 20 as b2, 30 as c2, 40 as d2;
select distkey from gp_distribution_policy where localoid='r3_2'::regclass;

create temp table r3_3 on commit drop as select 10 as a3, 20 as b3, 30 as c3, 40 as d3;
select distkey from gp_distribution_policy where localoid='r3_3'::regclass;

end;

RESET gp_create_table_random_default_distribution;

-- Test that distribution policy is not inherited and it is RANDOM in CREATE TABLE with default distribution set to random
SET gp_create_table_random_default_distribution=on;
CREATE TABLE distpol_person (
  name      text,
  age       int4,
  location  point
) DISTRIBUTED BY (name);

CREATE TABLE distpol_staff_member (
  salary    int4,
  manager   name
) INHERITS (distpol_person) WITH OIDS;
select distkey from gp_distribution_policy where localoid = 'distpol_staff_member'::regclass;

CREATE TABLE distpol_student (
  gpa      float8
) INHERITS (distpol_person);
select distkey from gp_distribution_policy where localoid = 'distpol_student'::regclass;

CREATE TABLE distpol_stud_emp (
  percent  int4
) INHERITS (distpol_staff_member, distpol_student);
select distkey from gp_distribution_policy where localoid = 'distpol_stud_emp'::regclass;

RESET gp_create_table_random_default_distribution;

-- Test that LIKE clause does not affect default distribution
SET gp_create_table_random_default_distribution=on;
set client_min_messages='warning';
DROP TABLE IF EXISTS distpol_person CASCADE;
reset client_min_messages;
CREATE TABLE distpol_person (
  name      text,
  age       int4,
  location  point
) DISTRIBUTED BY (name);
select distkey from gp_distribution_policy where localoid = 'distpol_person'::regclass;

CREATE TABLE distpol_person_copy (LIKE distpol_person);
select distkey from gp_distribution_policy where localoid = 'distpol_person_copy'::regclass;

RESET gp_create_table_random_default_distribution;

-- Test duplicate distribute keys
CREATE TABLE ctas_dup_dk as SELECT distinct age as c1, age as c2 from distpol_person; 
SELECT distinct age c1, age c2 into ctas_dup_dk_1 from distpol_person;


--
-- Test deriving distribution key from the query's distribution in
-- CREATE TABLE AS
--
create temporary table foo (i int) distributed by (i);

-- In both these cases, the query results are distributed by foo.i. In the
-- first case, it becomes a table column, so it's chosen as the distribution
-- key. In the second case, it's not, so we follow the default rule to use
-- the first column. (That's with the Postgres planner. ORCA uses different
-- rules.)
create table distpol_ctas1 as select 1 as col1, i from (select i from foo) x;
create table distpol_ctas2 as select 1 as col1 from (select i from foo) x;

-- Multiple columns. All the query's distribution key columns have to become
-- table columns, otherwise we can't use it.
drop table foo;
create temporary table foo (i int, j int) distributed by (i, j);
create table distpol_ctas3 as select 1 as col1, i from (select i, j from foo) x;
create table distpol_ctas4 as select 1 as col1, i, j from (select i, j from foo) x;

-- Check the results.
select localoid::regclass, distkey from gp_distribution_policy where localoid::regclass::text like 'distpol_ctas%';


--
-- Test using various datatypes as distribution keys.
--

-- Arrays can be used, if the base type is hashable.
create table distpol_text (t text[]) distributed by (t);

-- 'point' doesn't have a hash opclass, so these aren't allowed
create table distpol_point (p point) distributed by (p);
create table distpol_point_array (p point[]) distributed by (p);

-- Same with ranges
create table distpol_intrange (t int4range) distributed by (t);

-- tsvector has a btree opfamily, so you can create a range type on it, but
-- no hash opfamily, so its range type can't be used as distribution key.

create type tsvector_range as range (subtype = tsvector);

create table distpol_tsvector (t tsvector) distributed by (t);
create table distpol_tsvector_range (t tsvector_range) distributed by (t);
