-- @Description Test the basic bahavior of vacuum full
CREATE TABLE uaocs_full (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column);
CREATE INDEX uaocs_full_index ON uaocs_full(b);

INSERT INTO uaocs_full SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1,10) AS i;
INSERT INTO uaocs_full SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1,10) AS i;
ANALYZE uaocs_full;

DELETE FROM uaocs_full WHERE a < 4;
SELECT COUNT(*) FROM uaocs_full;
VACUUM FULL uaocs_full;
-- check if we get the same result afterwards
SELECT COUNT(*) FROM uaocs_full;
-- check if we can still insert into the relation
INSERT INTO uaocs_full VALUES (11, 11);
SELECT COUNT(*) FROM uaocs_full;
