DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;

DROP TABLE IF EXISTS v;
CREATE TABLE v as SELECT generate_series(1,10)a;
