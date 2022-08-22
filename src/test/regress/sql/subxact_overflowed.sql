-- It will occur subtransaction overflow when insert data to segments 1000 times.
-- All segments occur overflow.
DROP TABLE IF EXISTS t_zjp_1;
CREATE TABLE t_zjp_1(c1 int) DISTRIBUTED BY (c1);
CREATE OR REPLACE FUNCTION transaction_test0()
RETURNS void AS $$
DECLARE
    i int;
BEGIN
	FOR i in 0..1000
	LOOP
		BEGIN
			INSERT INTO t_zjp_1 VALUES(i);
		EXCEPTION
		WHEN UNIQUE_VIOLATION THEN
			NULL;
		END;
	END LOOP;
END;
$$
LANGUAGE plpgsql;

-- It will occur subtransaction overflow when insert data to segments 1000 times.
-- All segments occur overflow.
DROP TABLE IF EXISTS t_zjp_2;
CREATE TABLE t_zjp_2(c int PRIMARY KEY);
CREATE OR REPLACE FUNCTION transaction_test1()
RETURNS void AS $$
DECLARE i int;
BEGIN
	for i in 0..1000
	LOOP
		BEGIN
			INSERT INTO t_zjp_2 VALUES(i);
		EXCEPTION
			WHEN UNIQUE_VIOLATION THEN
				NULL;
		END;
	END LOOP;
END;
$$
LANGUAGE plpgsql;

-- It occur subtransaction overflow for coordinator and all segments.
CREATE OR REPLACE FUNCTION transaction_test2()
RETURNS void AS $$
DECLARE
    i int;
BEGIN
	for i in 0..1000
	LOOP
		BEGIN
			CREATE TEMP TABLE tmptab(c int) DISTRIBUTED BY (c);
			DROP TABLE tmptab;
		EXCEPTION
			WHEN others THEN
				NULL;
		END;
	END LOOP;
END;
$$
LANGUAGE plpgsql;

SET gp_log_suboverflow_statement = ON;

BEGIN;
SELECT transaction_test0();
SELECT count(*) FROM (SELECT (i).segid, (i).query, (i).subxact_overflowed, (i).sess_id, (i).pid FROM (SELECT gp_suboverflowed_backends())a(i) ORDER BY (i).segid) AS test;
SELECT DISTINCT logsegment, logmessage FROM gp_toolkit.gp_log_system
	WHERE logdebug = 'INSERT INTO t_zjp_1 VALUES(i)'
	ORDER BY logsegment;
COMMIT;

BEGIN;
SELECT transaction_test1();
SELECT count(*) FROM (SELECT (i).segid, (i).query, (i).subxact_overflowed, (i).sess_id, (i).pid FROM (SELECT gp_suboverflowed_backends())a(i) ORDER BY (i).segid) AS test;
SELECT DISTINCT logsegment, logmessage FROM gp_toolkit.gp_log_system
	WHERE logdebug = 'INSERT INTO t_zjp_2 VALUES(i)'
	ORDER BY logsegment;
COMMIT;

BEGIN;
SELECT transaction_test2();
SELECT count(*) FROM (SELECT (i).segid, (i).query, (i).subxact_overflowed, (i).sess_id, (i).pid FROM (SELECT gp_suboverflowed_backends())a(i) ORDER BY (i).segid) AS test;
SELECT DISTINCT logsegment, logmessage FROM gp_toolkit.gp_log_system
	WHERE logmessage = 'Statement caused suboverflow: SELECT transaction_test2();'
	ORDER BY logsegment;
COMMIT;

SET gp_log_suboverflow_statement = OFF;
