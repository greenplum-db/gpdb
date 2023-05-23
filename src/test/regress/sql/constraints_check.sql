-- loose over-eager constraint exclusion
-- please refer to https://github.com/greenplum-db/gpdb/issues/10287
CREATE TABLE t_issue_10287(a INT CHECK(a = 1));
INSERT INTO t_issue_10287 VALUES (NULL);
SELECT * FROM t_issue_10287 WHERE a IS NULL;
DROP TABLE t_issue_10287;

CREATE FUNCTION sum2_issue_10287(int8, int8) RETURNS int8 AS 'select $1 + $2' LANGUAGE SQL;
CREATE TABLE t_issue_10287_func(a INT, b INT, c INT CHECK(sum2_issue_10287(a, b)=3));
INSERT INTO t_issue_10287_func VALUES (1,2,3), (NULL,NULL,3);
SELECT * FROM t_issue_10287_func WHERE a IS NULL;
DROP TABLE t_issue_10287_func;
DROP FUNCTION sum2_issue_10287;
