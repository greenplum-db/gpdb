-- @Description Tests the behavior when the index of an ao table
-- has not been cleaned in combination with aborted inserts.

CREATE TABLE uaocs_outdatedindex_abort (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column) DISTRIBUTED BY (a);
CREATE INDEX uaocs_outdatedindex_abort_index ON uaocs_outdatedindex_abort(b);
INSERT INTO uaocs_outdatedindex_abort SELECT i as a, i as b, 'hello world' as c FROM generate_series(1, 50) AS i;
INSERT INTO uaocs_outdatedindex_abort SELECT i as a, i as b, 'hello world' as c FROM generate_series(51, 100) AS i;
ANALYZE uaocs_outdatedindex_abort;

SET enable_seqscan=false;
DELETE FROM uaocs_outdatedindex_abort WHERE a < 16;
VACUUM uaocs_outdatedindex_abort;
SELECT * FROM uaocs_outdatedindex_abort WHERE b = 20;
SELECT * FROM uaocs_outdatedindex_abort WHERE b = 10;
INSERT INTO uaocs_outdatedindex_abort SELECT i as a, i as b, 'Good morning' as c FROM generate_series(1, 4) AS i;
BEGIN;
INSERT INTO uaocs_outdatedindex_abort SELECT i as a, i as b, 'Good morning' as c FROM generate_series(5, 8) AS i;
INSERT INTO uaocs_outdatedindex_abort SELECT i as a, i as b, 'Good morning' as c FROM generate_series(9, 12) AS i;
ROLLBACK;
SELECT * FROM uaocs_outdatedindex_abort WHERE b < 16;
