-- It will occur subtransaction overflow when insert data to segments 1000 times.
-- All segments occur overflow.
DROP TABLE IF EXISTS t_zjpedu_1;
CREATE TABLE t_zjpedu_1(c1 int) DISTRIBUTED BY (c1);
CREATE OR REPLACE FUNCTION transaction_test0() /*in func*/
RETURNS void AS $$ /*in func*/
DECLARE /*in func*/
    i int; /*in func*/
BEGIN /*in func*/
	FOR i in 0..1000 /*in func*/
	LOOP /*in func*/
		BEGIN /*in func*/
			INSERT INTO t_zjpedu_1 VALUES(i); /*in func*/
		EXCEPTION /*in func*/
		WHEN UNIQUE_VIOLATION THEN /*in func*/
			NULL; /*in func*/
		END; /*in func*/
	END LOOP; /*in func*/
END; /*in func*/
$$ /*in func*/
LANGUAGE plpgsql;

1: BEGIN;
1: SELECT transaction_test0();
1: SELECT count(*) FROM (SELECT (i).segid, (i).query, (i).subxact_overflowed, (i).sess_id, (i).pid FROM (SELECT gp_suboverflowed_backends())a(i) ORDER BY (i).segid) AS test;
2: SELECT count(*) FROM (SELECT (i).segid, (i).query, (i).subxact_overflowed, (i).sess_id, (i).pid FROM (SELECT gp_suboverflowed_backends())a(i) ORDER BY (i).segid) AS test;
1: ABORT;
2: SELECT count(*) FROM (SELECT (i).segid, (i).query, (i).subxact_overflowed, (i).sess_id, (i).pid FROM (SELECT gp_suboverflowed_backends())a(i) ORDER BY (i).segid) AS test;

1: BEGIN;
1: SELECT transaction_test0();
1: SELECT count(*) FROM (SELECT (i).segid, (i).query, (i).subxact_overflowed, (i).sess_id, (i).pid FROM (SELECT gp_suboverflowed_backends())a(i) ORDER BY (i).segid) AS test;
2: SELECT count(*) FROM (SELECT (i).segid, (i).query, (i).subxact_overflowed, (i).sess_id, (i).pid FROM (SELECT gp_suboverflowed_backends())a(i) ORDER BY (i).segid) AS test;
1: COMMIT;
2: SELECT count(*) FROM (SELECT (i).segid, (i).query, (i).subxact_overflowed, (i).sess_id, (i).pid FROM (SELECT gp_suboverflowed_backends())a(i) ORDER BY (i).segid) AS test;