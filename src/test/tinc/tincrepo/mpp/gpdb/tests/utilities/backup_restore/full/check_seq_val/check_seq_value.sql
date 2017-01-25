CREATE TABLE company(id int, name text);
CREATE SEQUENCE serial START 1;
CREATE TABLE department ( id int DEFAULT nextval('serial'), name text);

insert into company  values ( 101, 'abc');
insert into department values ( nextval('serial'), 'aa');
insert into department values ( nextval('serial'), 'bb');
