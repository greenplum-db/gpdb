-- @Description Tests the behavior when the index of an ao table
-- has not been cleaned (e.g. because of a crash) in combination
-- with aborted inserts.

CREATE TABLE uao_index_abort_test (a INT, b INT, c CHAR(128)) WITH (appendonly=true) DISTRIBUTED BY (a);
CREATE INDEX uao_index_abort_test_index ON uao_index_abort_test(b);
INSERT INTO uao_index_abort_test SELECT i as a, i as b, 'hello world' as c FROM generate_series(1, 50) AS i;
INSERT INTO uao_index_abort_test SELECT i as a, i as b, 'hello world' as c FROM generate_series(51, 100) AS i;
ANALYZE uao_index_abort_test;

SET enable_seqscan=false;
DELETE FROM uao_index_abort_test WHERE a < 16;
VACUUM uao_index_abort_test;
SELECT * FROM uao_index_abort_test WHERE b = 20;
SELECT * FROM uao_index_abort_test WHERE b = 10;
INSERT INTO uao_index_abort_test SELECT i as a, i as b, 'Good morning' as c FROM generate_series(1, 4) AS i;
BEGIN;
INSERT INTO uao_index_abort_test SELECT i as a, i as b, 'Good morning' as c FROM generate_series(5, 8) AS i;
INSERT INTO uao_index_abort_test SELECT i as a, i as b, 'Good morning' as c FROM generate_series(9, 12) AS i;
ROLLBACK;
SELECT * FROM uao_index_abort_test WHERE b < 16;
