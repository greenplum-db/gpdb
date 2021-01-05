-- @Description Basic lazy vacuum

CREATE TABLE uao_basic (a INT, b INT, c CHAR(128)) WITH (appendonly=true);

INSERT INTO uao_basic SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 10) AS i;
INSERT INTO uao_basic SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 10) AS i;
ANALYZE uao_basic;

DELETE FROM uao_basic WHERE a < 4;
SELECT COUNT(*) FROM uao_basic;
VACUUM uao_basic;
-- check if we get the same result afterwards
SELECT COUNT(*) FROM uao_basic;
-- check if we can still insert into the relation
INSERT INTO uao_basic VALUES (11, 11);
SELECT COUNT(*) FROM uao_basic;
