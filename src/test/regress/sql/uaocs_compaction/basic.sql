-- @Description Basic lazy vacuum
CREATE TABLE uaocs_basic (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column);

INSERT INTO uaocs_basic SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 10) AS i;
INSERT INTO uaocs_basic SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 10) AS i;
ANALYZE uaocs_basic;

DELETE FROM uaocs_basic WHERE a < 4;
SELECT COUNT(*) FROM uaocs_basic;
VACUUM uaocs_basic;
-- check if we get the same result afterwards
SELECT COUNT(*) FROM uaocs_basic;
-- check if we can still insert into the relation
INSERT INTO uaocs_basic VALUES (11, 11);
SELECT COUNT(*) FROM uaocs_basic;
