DROP TABLE IF EXISTS distpol;
create table distpol as select random(), 1 as a, 2 as b;

DROP TABLE IF EXISTS hobbies_r;
CREATE TABLE hobbies_r (
	name		text,
	person 		text
);

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT * FROM hobbies_r;
