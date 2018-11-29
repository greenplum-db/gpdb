-- Test concurrent update a table with a varying length type
CREATE TABLE t_concurrent_update(a int, b int, c char(84));
INSERT INTO t_concurrent_update VALUES(1,1,'test');

1: BEGIN;
1: SET optimizer=off;
1: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
2: SET optimizer=off;
2&: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
1: END;
2<:
1: SELECT * FROM t_concurrent_update;

DROP TABLE t_concurrent_update;
