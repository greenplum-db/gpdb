-- @Description Tests the behavior when the index of an ao table
-- has not been cleaned (e.g. because of a crash).

CREATE TABLE uao_outdated (a INT, b INT, c CHAR(128)) WITH (appendonly=true) DISTRIBUTED BY (a);
CREATE INDEX uao_outdated_index ON uao_outdated(b);
INSERT INTO uao_outdated SELECT i as a, i as b, 'hello world' as c FROM generate_series(1, 50) AS i;
INSERT INTO uao_outdated SELECT i as a, i as b, 'hello world' as c FROM generate_series(51, 100) AS i;
ANALYZE uao_outdated;

SET enable_seqscan=false;
DELETE FROM uao_outdated WHERE a < 16;
VACUUM uao_outdated;
SELECT * FROM uao_outdated WHERE b = 20;
SELECT * FROM uao_outdated WHERE b = 10;
INSERT INTO uao_outdated SELECT i as a, i as b, 'Good morning' as c FROM generate_series(1, 10) AS i;
SELECT * FROM uao_outdated WHERE b = 10;
