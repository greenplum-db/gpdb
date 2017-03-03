DROP TABLE IF EXISTS r;
CREATE TABLE r(a int, b int);
INSERT INTO r SELECT i,i FROM generate_series(1,5)i;
