-- @Description Tests dropping a column after a compaction

CREATE TABLE uao_drop_col (a INT, b INT, c CHAR(128)) WITH (appendonly=true) DISTRIBUTED BY (a);
CREATE INDEX uao_drop_col_index ON uao_drop_col(b);
INSERT INTO uao_drop_col SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 10) AS i;

DELETE FROM uao_drop_col WHERE a < 4;
SELECT COUNT(*) FROM uao_drop_col;
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_drop_col';
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_drop_col_index';
VACUUM uao_drop_col;
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_drop_col';
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_drop_col_index';
ALTER TABLE uao_drop_col DROP COLUMN c;
SELECT * FROM uao_drop_col;
INSERT INTO uao_drop_col VALUES (42, 42);
SELECT * FROM uao_drop_col;
