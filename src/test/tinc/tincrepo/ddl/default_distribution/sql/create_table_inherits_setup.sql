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
