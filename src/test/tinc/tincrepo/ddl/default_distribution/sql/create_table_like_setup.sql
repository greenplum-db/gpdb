DROP TABLE IF EXISTS person CASCADE;
CREATE TABLE person (
	name 		text,
	age			int4,
	location 	point
) DISTRIBUTED BY (name);

DROP TABLE IF EXISTS person_copy;
CREATE TABLE person_copy (LIKE person);
