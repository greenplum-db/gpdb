-- @Description Tests the behavior when the index of an ao table
-- has not been cleaned in combination with a partial index.
CREATE TABLE uaocs_outdated_partialindex (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column) DISTRIBUTED BY (a);
CREATE INDEX uaocs_outdated_partialindex_index ON uaocs_outdated_partialindex(b) WHERE b < 20;
INSERT INTO uaocs_outdated_partialindex SELECT i as a, i as b, 'hello world' as c FROM generate_series(1, 50) AS i;
INSERT INTO uaocs_outdated_partialindex SELECT i as a, i as b, 'hello world' as c FROM generate_series(51, 100) AS i;
ANALYZE uaocs_outdated_partialindex;

SET enable_seqscan=false;
DELETE FROM uaocs_outdated_partialindex WHERE a < 16;
VACUUM uaocs_outdated_partialindex;
SELECT * FROM uaocs_outdated_partialindex WHERE b = 20;
SELECT * FROM uaocs_outdated_partialindex WHERE b = 10;
INSERT INTO uaocs_outdated_partialindex SELECT i as a, i as b, 'Good morning' as c FROM generate_series(101, 110) AS i;
SELECT * FROM uaocs_outdated_partialindex WHERE b = 10;
SELECT * FROM uaocs_outdated_partialindex WHERE b = 102;
