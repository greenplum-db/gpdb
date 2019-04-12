/*
 * 
 * Functional tests
 * Parameter combination tests
 * Improve code coverage tests
 */
-- start_ignore
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
-- end_ignore

CREATE SCHEMA ic_udp_test;
SET search_path = ic_udp_test;

-- Prepare some tables
CREATE TABLE small_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'abcdefghijklmnopqrstuvwxyz') DISTRIBUTED BY (dkey);
INSERT INTO small_table VALUES(generate_series(1, 500), generate_series(501, 1000), sqrt(generate_series(501, 1000)));

-- Functional tests
-- Skew with gather+redistribute
SELECT ROUND(foo.rval * foo.rval)::INT % 30 AS rval2, COUNT(*) AS count, SUM(length(foo.tval)) AS sum_len_tval
  FROM (SELECT 501 AS jkey, rval, tval FROM small_table ORDER BY dkey LIMIT 3000) foo
    JOIN small_table USING(jkey)
  GROUP BY rval2
  ORDER BY rval2;

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
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 50) bar USING(jkey);

-- Gather motion (Window function)
SELECT dkey % 30 AS dkey2, MIN(rank) AS min_rank, AVG(foo.rval) AS avg_rval
  FROM (SELECT RANK() OVER(ORDER BY rval DESC) AS rank, jkey, rval
        FROM small_table) foo
    JOIN small_table USING(jkey)
  GROUP BY dkey2
  ORDER BY dkey2;

-- Broadcast (call genereate_series to multiply result set)
SELECT COUNT(*) AS count
  FROM (SELECT generate_series(501, 530) AS jkey FROM small_table) foo
    JOIN small_table USING(jkey);

-- Subquery
SELECT (SELECT tval FROM small_table bar WHERE bar.dkey + 500 = foo.jkey) AS tval
  FROM (SELECT * FROM small_table ORDER BY jkey LIMIT 200) foo LIMIT 15;

SELECT (SELECT tval FROM small_table bar WHERE bar.dkey = 1) AS tval
  FROM (SELECT * FROM small_table ORDER BY jkey LIMIT 300) foo LIMIT 15;

-- Target dispatch
CREATE TABLE target_table AS SELECT * FROM small_table LIMIT 0 DISTRIBUTED BY (dkey);
INSERT INTO target_table VALUES(1, 1, 1.0, '1');
SELECT * FROM target_table WHERE dkey = 1;
DROP TABLE target_table;

-- CURSOR tests
BEGIN;
DECLARE c1 CURSOR FOR SELECT dkey % 500 AS dkey2
                FROM (SELECT jkey FROM small_table) foo
                  JOIN small_table USING(jkey)
                GROUP BY dkey2
                ORDER BY dkey2;

DECLARE c2 CURSOR FOR SELECT dkey % 500 AS dkey2
                FROM (SELECT jkey FROM small_table) foo
                  JOIN small_table USING(jkey)
                GROUP BY dkey2
                ORDER BY dkey2;

DECLARE c3 CURSOR FOR SELECT dkey % 500 AS dkey2
                FROM (SELECT jkey FROM small_table) foo
                  JOIN small_table USING(jkey)
                GROUP BY dkey2
                ORDER BY dkey2;

DECLARE c4 CURSOR FOR SELECT dkey % 500 AS dkey2
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
  FROM (SELECT jkey, repeat(tval, 10000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- Redistribute all tuples with minimize settings
SET gp_interconnect_snd_queue_depth TO 1;
SET gp_interconnect_queue_depth TO 1;
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 10000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- Redistribute all tuples
SET gp_interconnect_snd_queue_depth TO 4096;
SET gp_interconnect_queue_depth TO 1;
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 10000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- Redistribute all tuples
SET gp_interconnect_snd_queue_depth TO 1;
SET gp_interconnect_queue_depth TO 4096;
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 10000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- Redistribute all tuples
SET gp_interconnect_snd_queue_depth TO 1024;
SET gp_interconnect_queue_depth TO 1024;
SELECT SUM(length(long_tval)) AS sum_len_tval
  FROM (SELECT jkey, repeat(tval, 10000) AS long_tval
          FROM small_table ORDER BY dkey LIMIT 20) foo
            JOIN (SELECT * FROM small_table ORDER BY dkey LIMIT 100) bar USING(jkey);

-- MPP-21916
CREATE TABLE a (i INT, j INT) DISTRIBUTED BY (i);
INSERT INTO a (SELECT i, i * i FROM generate_series(1, 10) as i);
SELECT a.* FROM a WHERE a.j NOT IN (SELECT j FROM a a2 WHERE a2.j = a.j AND a2.i = 1) AND a.i = 1;
SELECT a.* FROM a INNER JOIN a b ON a.i = b.i WHERE a.j NOT IN (SELECT j FROM a a2 WHERE a2.j = b.j) AND a.i = 1;

-- Paramter range
SET gp_interconnect_snd_queue_depth TO -1; -- ERROR
SET gp_interconnect_snd_queue_depth TO 0; -- ERROR
SET gp_interconnect_snd_queue_depth TO 4097; -- ERROR
SET gp_interconnect_queue_depth TO -1; -- ERROR
SET gp_interconnect_queue_depth TO 0; -- ERROR
SET gp_interconnect_queue_depth TO 4097; -- ERROR

-- Cleanup
DROP TABLE small_table;
DROP TABLE a;

RESET search_path;
DROP SCHEMA ic_udp_test CASCADE;

/*
 * If ack packet is lost in doSendStopMessageUDPIFC(), transaction with cursor
 * should still be able to commit.
*/
--start_ignore
drop table if exists ic_test_1;
--end_ignore
create table ic_test_1 as select i as c1, i as c2 from generate_series(1, 100000) i;
begin;
declare ic_test_cursor_c1 cursor for select * from ic_test_1;
select gp_inject_fault('interconnect_stop_ack_is_lost', 'reset', 1);
select gp_inject_fault('interconnect_stop_ack_is_lost', 'skip', 1);
commit;
drop table ic_test_1;

/*
 * If message queue of connection is failed to be allocated in
 * SetupUDPIFCInterconnect_Internal() , it should be handled properly
 * in TeardownUDPIFCInterconnect_Internal().
 */
CREATE TABLE a (i INT, j INT) DISTRIBUTED BY (i);
INSERT INTO a (SELECT i, i * i FROM generate_series(1, 10) as i);
SELECT gp_inject_fault('interconnect_setup_palloc', 'error', 1);
SELECT * FROM a;
DROP TABLE a;
SELECT gp_inject_fault('interconnect_setup_palloc', 'reset', 1);

-- Use WITH RECURSIVE to construct a one-time filter result node that executed
-- on QD, meanwhile, let the result node has a outer node which contain motion.
-- It's used to test that result node on QD can send a stop message to sender in
-- one-time filter case.
RESET gp_interconnect_snd_queue_depth;
RESET gp_interconnect_queue_depth;
CREATE TABLE recursive_table_ic (a INT) DISTRIBUTED BY (a);
-- Insert enough data so interconnect sender don't quit earlier.
INSERT INTO recursive_table_ic SELECT * FROM generate_series(20, 30000);
WITH RECURSIVE
r(i) AS (
       SELECT 1
),
y(i) AS (
       SELECT 1
       UNION ALL
       SELECT i + 1 FROM y, recursive_table_ic WHERE NOT EXISTS (SELECT * FROM r LIMIT 10)
)
SELECT * FROM y LIMIT 10;
DROP TABLE recursive_table_ic;
