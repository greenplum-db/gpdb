DROP TABLE IF EXISTS foo CASCADE;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar CASCADE;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
