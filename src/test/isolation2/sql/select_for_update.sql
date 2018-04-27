DROP TABLE IF EXISTS t;

CREATE TABLE t (c1 int, c2 int);
 
INSERT INTO t values (1,1), (2,2), (3,3), (4,4);

1: BEGIN;
2: BEGIN;

1: SELECT c2 FROM t WHERE c1 = 3 FOR UPDATE;
2&: UPDATE t SET c2 = 999 where c1 = 3;

1: END;

2<:
2: END;

1q:
2q:

DROP TABLE t;
