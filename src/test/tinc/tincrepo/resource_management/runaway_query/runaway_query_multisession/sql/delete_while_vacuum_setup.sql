DROP TABLE IF EXISTS ao;

CREATE TABLE ao (a INT, b INT) WITH (appendonly=true);
INSERT INTO ao SELECT i as a, i as b FROM generate_series(1, 100) AS i;

