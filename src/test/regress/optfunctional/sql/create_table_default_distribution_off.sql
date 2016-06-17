
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema create_table_default_distribution_off;
set search_path to create_table_default_distribution_off;
set gp_create_table_random_default_distribution=off;
-- end_ignore

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
drop schema create_table_default_distribution_off cascade;
-- end_ignore
