-- @Description Tests the behavior while compacting is disabled

CREATE TABLE full_eof_truncate (a INT, b INT, c CHAR(128)) WITH (appendonly=true);
CREATE INDEX full_eof_truncate_index ON full_eof_truncate(b);
BEGIN;
INSERT INTO full_eof_truncate SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1,1000) AS i;
ANALYZE full_eof_truncate;
COMMIT;
BEGIN;
INSERT INTO full_eof_truncate SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1000,2000) AS i;
ROLLBACK;

SET gp_appendonly_compaction=false;
SELECT COUNT(*) FROM full_eof_truncate;
VACUUM FULL full_eof_truncate;
SELECT COUNT(*) FROM full_eof_truncate;
-- Insert afterwards
INSERT INTO full_eof_truncate SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1,10) AS i;
SELECT COUNT(*) FROM full_eof_truncate;
