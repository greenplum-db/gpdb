DROP TABLE IF EXISTS x;
DROP TABLE IF EXISTS y;
CREATE TABLE x AS SELECT generate_series(1,10);
CREATE TABLE y AS SELECT generate_series(1,10);

