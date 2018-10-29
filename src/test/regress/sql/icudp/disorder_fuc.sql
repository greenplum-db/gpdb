-- 
-- @description interconnect tests for packet control 
-- @created 2012-11-08 09:00:00
-- @modified 2012-11-08 09:00:00
-- @tags executor
-- @gpdb_version [4.2.3.0,main]

SET gp_interconnect_queue_depth=4;
SET gp_interconnect_snd_queue_depth=2;
-- Create a table
CREATE TEMP TABLE small_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'abcdefghijklmnopqrstuvwxyz') DISTRIBUTED BY (dkey);

-- Generate some data
INSERT INTO small_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));

-- Functional tests
-- Skew with gather+redistribute
-- seq = 20
SELECT ROUND(foo.rval * foo.rval)::INT % 30 AS rval2, COUNT(*) AS count, SUM(length(foo.tval)) AS sum_len_tval
  FROM (SELECT 5001 AS jkey, rval, tval FROM small_table ORDER BY dkey LIMIT 3000) foo
    JOIN small_table USING(jkey)
  GROUP BY rval2
  ORDER BY rval2;
