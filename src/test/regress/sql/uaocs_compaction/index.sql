-- @Description Tests basic index usage behavior after vacuuming
CREATE TABLE uaocs_index (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column);
CREATE INDEX uaocs_index_index ON uaocs_index(b);

INSERT INTO uaocs_index SELECT i as a, i as b, 'hello world' as c FROM generate_series(1,10) AS i;
INSERT INTO uaocs_index SELECT i as a, i as b, 'hello world' as c FROM generate_series(1,10) AS i;
ANALYZE uaocs_index;

VACUUM uaocs_index;
SELECT * FROM uaocs_index WHERE b = 5;
