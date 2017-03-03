CREATE DATABASE testdb;
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a text, b int, c text, d text, e text);
INSERT INTO foo VALUES ('foo', 12000, 'pg_default', 'ao_tab1', 'testdb');
INSERT INTO foo VALUES ('bar', 4045, 'pg_default', 'ao_tab2', 'testdb');
INSERT INTO foo VALUES ('ao_tab1', 573, 'pg_default', 'ao_tab1', 'testdb');
INSERT INTO foo VALUES ('ao_tab2', 25730, 'pg_default', 'ao_tab2', 'testdb');

CREATE TABLE bar (x int, y int);
INSERT INTO bar SELECT i, i+1 FROM generate_series(1,10) i;

CREATE TABLE ao_tab1 (r int, s int) WITH (appendonly=true, compresslevel=6);
INSERT INTO ao_tab1 SELECT i, i+1 FROM generate_series(1,20) i;

CREATE TABLE ao_tab2 (r int, s int) WITH (appendonly=true);
INSERT INTO ao_tab2 SELECT i, i*100 FROM generate_series(1,200) i;

