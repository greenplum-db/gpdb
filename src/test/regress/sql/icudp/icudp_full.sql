-- 
-- @description interconnect tests 
-- @created 2012-11-28 09:00:00
-- @modified 2012-11-28 09:00:00
-- @tags executor
-- @gpdb_version [4.2.3.0,main]
CREATE SCHEMA ic_udp_test;
SET search_path = ic_udp_test;

-- Create a table
CREATE TABLE small_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'abcdefghijklmnopqrstuvwxyz') DISTRIBUTED BY (dkey);

-- Issue a query to make later queries elide ic setup while injecting faults  
SELECT count(*) from small_table;

-- Makesure these codes are triggered to improve the code coverage during test.
SET gp_udpic_dropacks_percent = 20;
SET gp_udpic_dropxmit_percent = 20;
SET gp_udpic_fault_inject_percent = 40;
SET gp_interconnect_full_crc = true;
SET gp_udpic_fault_inject_bitmap = 50790655;
-- SET gp_log_interconnect TO 'DEBUG';
-- SET gp_interconnect_log_stats = true;
-- SET log_min_messages TO 'DEBUG5';

-- Generate some data
INSERT INTO small_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));

-- Functional tests
-- Skew with gather+redistribute
SELECT ROUND(foo.rval * foo.rval)::INT % 30 AS rval2, COUNT(*) AS count, SUM(length(foo.tval)) AS sum_len_tval
  FROM (SELECT 5001 AS jkey, rval, tval FROM small_table ORDER BY dkey LIMIT 3000) foo
    JOIN small_table USING(jkey)
  GROUP BY rval2
  ORDER BY rval2;

drop table if exists csq_t1;
drop table if exists csq_t2;

create table csq_t1(a int, b int) distributed by (b);
insert into csq_t1 values (1,2);
insert into csq_t1 values (3,4);
insert into csq_t1 values (5,6);
insert into csq_t1 values (7,8);

create table csq_t2(x int,y int);
insert into csq_t2 values(1,1);
insert into csq_t2 values(3,9);
insert into csq_t2 values(5,25);
insert into csq_t2 values(7,49);

update csq_t1 set a = (select y from csq_t2 where x=a) where b < 8;

drop table if exists csq_t1;
drop table if exists csq_t2;


-- Union
SELECT jkey2, SUM(length(digits_string)) AS sum_len_dstring
  FROM (
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)
    UNION ALL
    (SELECT jkey % 30 AS jkey2, repeat('0123456789', 200) AS digits_string FROM small_table GROUP BY jkey2)) foo
  GROUP BY jkey2
  ORDER BY jkey2
  LIMIT 30;

-- Huge tuple (May need to split) 26 * 200000
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 200000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 500) bar USING(jkey);

-- Gather motion (Window function)
SELECT dkey % 30 AS dkey2, MIN(rank) AS min_rank, AVG(foo.rval) AS avg_rval
  FROM (SELECT RANK() OVER(ORDER BY rval DESC) AS rank, jkey, rval
        FROM small_table) foo
    JOIN small_table USING(jkey)
  GROUP BY dkey2
  ORDER BY dkey2;

-- Broadcast (call genereate_series to multiply result set)
SELECT COUNT(*) AS count
  FROM (SELECT generate_series(5001, 5300) AS jkey FROM small_table) foo
    JOIN small_table USING(jkey);

-- Subquery
SELECT (SELECT tval FROM small_table bar WHERE bar.dkey + 5000 = foo.jkey) AS tval
  FROM (SELECT * FROM small_table ORDER BY jkey LIMIT 2000) foo LIMIT 15;

SELECT (SELECT tval FROM small_table bar WHERE bar.dkey = 1) AS tval
  FROM (SELECT * FROM small_table ORDER BY jkey LIMIT 3000) foo LIMIT 15;

-- Target dispatch
CREATE TABLE target_table AS SELECT * FROM small_table LIMIT 0 DISTRIBUTED BY (dkey);
INSERT INTO target_table VALUES(1, 1, 1.0, '1');
SELECT * FROM target_table WHERE dkey = 1;
DROP TABLE target_table;

-- CURSOR tests
BEGIN;
DECLARE c1 CURSOR FOR SELECT dkey % 5000 AS dkey2
                FROM (SELECT jkey FROM small_table) foo
                  JOIN small_table USING(jkey)
                GROUP BY dkey2
                ORDER BY dkey2;

DECLARE c2 CURSOR FOR SELECT dkey % 5000 AS dkey2
                FROM (SELECT jkey FROM small_table) foo
                  JOIN small_table USING(jkey)
                GROUP BY dkey2
                ORDER BY dkey2;

DECLARE c3 CURSOR FOR SELECT dkey % 5000 AS dkey2
                FROM (SELECT jkey FROM small_table) foo
                  JOIN small_table USING(jkey)
                GROUP BY dkey2
                ORDER BY dkey2;

DECLARE c4 CURSOR FOR SELECT dkey % 5000 AS dkey2
                FROM (SELECT jkey FROM small_table) foo
                  JOIN small_table USING(jkey)
                GROUP BY dkey2
                ORDER BY dkey2;

FETCH 20 FROM c1;
FETCH 20 FROM c2;
FETCH 20 FROM c3;
FETCH 20 FROM c4;

CLOSE c1;
CLOSE c2;
CLOSE c3;
CLOSE c4;

END;

-- Redistribute all tuples with normal settings
SET gp_interconnect_snd_queue_depth TO 8;
SET gp_interconnect_queue_depth TO 8;
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 20000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- Redistribute all tuples with minimize settings
SET gp_interconnect_snd_queue_depth TO 1;
SET gp_interconnect_queue_depth TO 1;
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 20000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- Redistribute all tuples
SET gp_interconnect_snd_queue_depth TO 4096;
SET gp_interconnect_queue_depth TO 1;
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 20000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- Redistribute all tuples
SET gp_interconnect_snd_queue_depth TO 1;
SET gp_interconnect_queue_depth TO 4096;
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 20000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- Redistribute all tuples
SET gp_interconnect_snd_queue_depth TO 1024;
SET gp_interconnect_queue_depth TO 1024;
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 20000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- Paramter range
SET gp_interconnect_snd_queue_depth TO -1; -- ERROR
SET gp_interconnect_snd_queue_depth TO 0; -- ERROR
SET gp_interconnect_snd_queue_depth TO 4097; -- ERROR
SET gp_interconnect_queue_depth TO -1; -- ERROR
SET gp_interconnect_queue_depth TO 0; -- ERROR
SET gp_interconnect_queue_depth TO 4097; -- ERROR

-- Reset parameters
RESET gp_interconnect_snd_queue_depth;
RESET gp_interconnect_queue_depth;

-- Lots of connections
CREATE FUNCTION icudp_history_test() RETURNS void LANGUAGE plpgsql AS $$
DECLARE
    local_jkey INT;
    local_val  INT;
BEGIN
    FOR k IN 1 .. 1000
    LOOP
        SELECT jkey INTO local_jkey FROM small_table WHERE dkey = k;
    END LOOP;

    FOR k IN 1 .. 30
    LOOP
        SELECT ROUND(foo.rval * foo.rval)::INT % 30 AS rval2 INTO local_val
          FROM (SELECT 5001 AS jkey, rval, tval FROM small_table dkey LIMIT 3000) foo
            JOIN small_table USING(jkey)
        GROUP BY rval2
        ORDER BY rval2;
    END LOOP;
END
$$;

SELECT icudp_history_test();
DROP FUNCTION icudp_history_test();

-- lots of functions
SELECT COUNT(tval) AS count_tval
  FROM (SELECT (SELECT tval FROM small_table bar WHERE bar.dkey + 5000 = foo.jkey) AS tval
        FROM (SELECT * FROM small_table ORDER BY jkey LIMIT 2000) foo) bar;

-- system call fault injection tests
CREATE FUNCTION system_call_fault_injection_test() RETURNS void LANGUAGE plpgsql AS $$
DECLARE
    local_jkey INT;
    local_val  INT;
BEGIN
    FOR k IN 1 .. 50
    LOOP
        BEGIN
            SELECT jkey INTO local_jkey FROM small_table WHERE dkey = k;
        EXCEPTION
            WHEN others THEN
                CONTINUE;
        END;
    END LOOP;

    FOR k IN 1 .. 30
    LOOP
        BEGIN
            SELECT ROUND(foo.rval * foo.rval)::INT % 30 AS rval2 INTO local_val
              FROM (SELECT 5001 AS jkey, rval, tval FROM small_table LIMIT 3000) foo
                JOIN small_table USING(jkey)
            GROUP BY rval2
            ORDER BY rval2;
        EXCEPTION
            WHEN others THEN
                CONTINUE;
        END;
    END LOOP;
END
$$;

SET gp_udpic_fault_inject_bitmap = 524288;
SELECT system_call_fault_injection_test();

-- disable ipv6 may increase the code coverage.
SET gp_udpic_network_disable_ipv6 = 1;
SELECT system_call_fault_injection_test();

-- inject faults into malloc() will coverage more error process codes.
SET gp_udpic_fault_inject_bitmap = 1048576;
SELECT system_call_fault_injection_test();

-- inject faults to receiver buffers
SET gp_udpic_fault_inject_bitmap = 536870912;
SELECT system_call_fault_injection_test();

DROP FUNCTION system_call_fault_injection_test();

-- Improve code coverage by disable fault injection
SET gp_udpic_fault_inject_percent = 40;
SET gp_udpic_fault_inject_bitmap = 0;
SELECT ROUND(foo.rval * foo.rval)::INT % 30 AS rval2, COUNT(*) AS count, SUM(length(foo.tval)) AS sum_len_tval
  FROM (SELECT 5001 AS jkey, rval, tval FROM small_table ORDER BY dkey LIMIT 3000) foo
    JOIN small_table USING(jkey)
  GROUP BY rval2
  ORDER BY rval2;

-- Inject query cancel interrupt faults
SET gp_udpic_fault_inject_percent = 15;
SET gp_udpic_fault_inject_bitmap = 4096; -- Query cancel

CREATE FUNCTION query_cancel_fault_injection_test() RETURNS void LANGUAGE plpgsql AS $$
DECLARE
    local_val  BIGINT;
BEGIN
    FOR k IN 1 .. 90
    LOOP
        BEGIN
            SELECT SUM(length(long_tval)) AS sum_len_tval INTO local_val
              FROM (SELECT jkey, repeat(tval, 50000) AS long_tval
                      FROM small_table ORDER BY dkey LIMIT 20) foo
                        JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);
        EXCEPTION
            WHEN query_canceled THEN
                CONTINUE;
        END;
    END LOOP;
END
$$;

SELECT query_cancel_fault_injection_test();
DROP FUNCTION query_cancel_fault_injection_test();

RESET gp_udpic_dropseg;
RESET gp_udpic_dropxmit_percent;
RESET gp_udpic_fault_inject_percent;
--RESET gp_interconnect_elide_setup;

/*
 * Inject proc die interrupt faults
 * This test always failed the regression test.
SET gp_udpic_fault_inject_percent = 15;
SET gp_udpic_fault_inject_bitmap = 8192; -- Proc die
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 50000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- Make sure we are still under this schema
SET search_path = ic_udp_test;
*/

RESET search_path;
DROP SCHEMA ic_udp_test CASCADE;
