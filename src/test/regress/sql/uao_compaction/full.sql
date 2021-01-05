-- @Description Test the basic bahavior of vacuum full

CREATE TABLE uao_full (a INT, b INT, c CHAR(128)) WITH (appendonly=true);
CREATE INDEX uao_full_index ON uao_full(b);
INSERT INTO uao_full SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1,10) AS i;
INSERT INTO uao_full SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1,10) AS i;
ANALYZE uao_full;

DELETE FROM uao_full WHERE a < 4;
SELECT COUNT(*) FROM uao_full;
VACUUM FULL uao_full;
-- check if we get the same result afterwards
SELECT COUNT(*) FROM uao_full;
-- check if we can still insert into the relation
INSERT INTO uao_full VALUES (11, 11);
SELECT COUNT(*) FROM uao_full;
