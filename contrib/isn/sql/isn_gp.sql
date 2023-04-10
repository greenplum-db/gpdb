--
-- Test ISN extension on GPDB
--

CREATE EXTENSION isn;

--
-- test partition table
--
CREATE TABLE pt(id ISBN) PARTITION BY RANGE (id);
CREATE TABLE pt_1 PARTITION of pt for VALUES FROM ('0-11-000533-3!') TO ('0-14-121930-0!');
CREATE TABLE pt_2 PARTITION of pt for VALUES FROM ('0-14-121930-0!') TO ('0-393-04002-X');
CREATE TABLE pt_3 PARTITION of pt for VALUES FROM ('0-393-04002-X') TO ('2-205-00876-5!');
INSERT INTO pt VALUES ('0-11-000533-3!'), ('0-14-121930-0!'), ('0-393-04002-X');
SELECT * FROM pt ORDER BY id;
\d+ pt

explain (verbose) SELECT * FROM pt WHERE id >= '0-11-000533-3!'::ISBN AND id <= '0-14-121930-0!'::ISBN ORDER BY id;

DROP TABLE pt;

--
-- test distributed by
--
CREATE TABLE dt(id ISBN) DISTRIBUTED BY (id);
INSERT INTO dt VALUES ('0-11-000533-3!'), ('0-14-121930-0!'), ('0-393-04002-X'), ('2-205-00876-5!');
SELECT * FROM dt ORDER BY id;
DROP TABLE dt;

--
-- cleanup
--
DROP EXTENSION isn;
