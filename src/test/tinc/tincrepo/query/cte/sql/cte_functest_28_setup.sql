DROP LANGUAGE IF EXISTS plpythonu CASCADE;
CREATE LANGUAGE plpythonu;

DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
