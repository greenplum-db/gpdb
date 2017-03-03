DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS foobar;
CREATE TABLE foobar (c int, d int);
INSERT INTO foobar select i, i+1 from generate_series(1,10) i;
