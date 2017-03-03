DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;

DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo (a int, b int);
INSERT INTO newfoo SELECT i as a, i+1 as b from generate_series(1,10)i;
