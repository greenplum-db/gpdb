-- This case is based on the reproduce steps reported on https://github.com/greenplum-db/gpdb/issues/15149

CREATE TABLE issue_15149(c1 varchar);

SELECT t.c1 FROM (SELECT DISTINCT ON (upper(c1)) c1 COLLATE "C" FROM issue_15149)t;

DROP TABLE issue_15149;
