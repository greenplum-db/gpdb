-- test distributed by
CREATE TABLE t1 (str oracle.varchar2(30)) DISTRIBUTED BY (str);
INSERT INTO t1 VALUES ('abc');
SELECT * FROM t1;

CREATE TABLE t2 (str oracle.nvarchar2(30)) DISTRIBUTED BY (str);
INSERT INTO t2 VALUES ('abc');
SELECT * FROM t2;

DROP TABLE t1;
DROP TABLE t2;
