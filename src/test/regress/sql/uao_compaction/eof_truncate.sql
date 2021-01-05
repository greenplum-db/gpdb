-- @Description Tests the behavior while compacting is disabled

CREATE TABLE uao_eof_truncate (a INT, b INT, c CHAR(128)) WITH (appendonly=true);
CREATE INDEX uao_eof_truncate_index ON uao_eof_truncate(b);
BEGIN;
INSERT INTO uao_eof_truncate SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1,1000) AS i;
ANALYZE uao_eof_truncate;
COMMIT;
BEGIN;
INSERT INTO uao_eof_truncate SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1000,2000) AS i;
ROLLBACK;

SET gp_appendonly_compaction=false;
SELECT COUNT(*) FROM uao_eof_truncate;
VACUUM uao_eof_truncate;
SELECT COUNT(*) FROM uao_eof_truncate;
-- Insert afterwards
INSERT INTO uao_eof_truncate SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1,10) AS i;
SELECT COUNT(*) FROM uao_eof_truncate;
