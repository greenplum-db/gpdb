-- 
-- @description Interconncet flow control test case: single guc value
-- @created 2012-11-22
-- @modified 2012-11-22
-- @tags executor
-- @gpdb_version [4.2.3.0,main]

-- Create tables
CREATE TABLE small_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'abcdefghijklmnopqrstuvwxyz') DISTRIBUTED BY (dkey);
CREATE TABLE media_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'i like travelling') DISTRIBUTED BY (jkey);
CREATE TABLE big_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'can you tell me a joke') DISTRIBUTED BY (rval);

-- Generate some data
INSERT INTO small_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));
INSERT INTO media_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));
INSERT INTO big_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));

-- Functional tests
-- Skew with gather+redistribute
SELECT ROUND(rval * rval)::INT % 30 AS rval2, SUM(dkey) AS sum_dkey, AVG(jkey) AS ave_jkey,
                                SUM(length(tval)) AS sum_len_tval, SUM(length(tval2)) AS sum_len_tval2, SUM(length(tval3)) AS sum_len_tval3 
FROM
 (SELECT dkey, big_table.jkey, big_table.rval, foo2.tval, foo2.tval2, big_table.tval AS tval3  FROM
  (SELECT rval, media_table.jkey, media_table.dkey, media_table.tval AS tval2, foo1.tval  FROM
   (SELECT jkey, dkey,  small_table.rval, small_table.tval  FROM
     (SELECT  jkey, rval, tval  FROM
       small_table ORDER BY dkey ) foo
        JOIN small_table USING(jkey) ORDER BY dkey) foo1
          JOIN media_table USING(rval) ORDER BY jkey) foo2
             JOIN big_table USING(dkey) ORDER BY rval) foo3
GROUP BY rval2
ORDER BY rval2;

-- drop table testemp
DROP TABLE small_table;
DROP TABLE media_table;
DROP TABLE big_table;

-- Create tables
CREATE TABLE small_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'abcdefghijklmnopqrstuvwxyz') DISTRIBUTED BY (dkey);
CREATE TABLE media_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'i like travelling') DISTRIBUTED BY (jkey);
CREATE TABLE big_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'can you tell me a joke') DISTRIBUTED BY (rval);

-- Generate some data
INSERT INTO small_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));
INSERT INTO media_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));
INSERT INTO big_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));

-- Set GUC value to its min value 
set gp_interconnect_queue_depth = 1;
show gp_interconnect_queue_depth;
SELECT ROUND(rval * rval)::INT % 30 AS rval2, SUM(dkey) AS sum_dkey, AVG(jkey) AS ave_jkey,
                                SUM(length(tval)) AS sum_len_tval, SUM(length(tval2)) AS sum_len_tval2, SUM(length(tval3)) AS sum_len_tval3
FROM
 (SELECT dkey, big_table.jkey, big_table.rval, foo2.tval, foo2.tval2, big_table.tval AS tval3  FROM
  (SELECT rval, media_table.jkey, media_table.dkey, media_table.tval AS tval2, foo1.tval  FROM
   (SELECT jkey, dkey,  small_table.rval, small_table.tval  FROM
     (SELECT  jkey, rval, tval  FROM
       small_table ORDER BY dkey ) foo
        JOIN small_table USING(jkey) ORDER BY dkey) foo1
          JOIN media_table USING(rval) ORDER BY jkey) foo2
             JOIN big_table USING(dkey) ORDER BY rval) foo3
GROUP BY rval2
ORDER BY rval2;

-- drop table testemp
DROP TABLE small_table;
DROP TABLE media_table;
DROP TABLE big_table;

-- Create tables
CREATE TABLE small_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'abcdefghijklmnopqrstuvwxyz') DISTRIBUTED BY (dkey);
CREATE TABLE media_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'i like travelling') DISTRIBUTED BY (jkey);
CREATE TABLE big_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'can you tell me a joke') DISTRIBUTED BY (rval);

-- Generate some data
INSERT INTO small_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));
INSERT INTO media_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));
INSERT INTO big_table VALUES(generate_series(1, 5000), generate_series(5001, 10000), sqrt(generate_series(5001, 10000)));

-- Set GUC value to its max value
set gp_interconnect_queue_depth = 4096;
show gp_interconnect_queue_depth;
SELECT ROUND(rval * rval)::INT % 30 AS rval2, SUM(dkey) AS sum_dkey, AVG(jkey) AS ave_jkey,
                                SUM(length(tval)) AS sum_len_tval, SUM(length(tval2)) AS sum_len_tval2, SUM(length(tval3)) AS sum_len_tval3
FROM
 (SELECT dkey, big_table.jkey, big_table.rval, foo2.tval, foo2.tval2, big_table.tval AS tval3  FROM
  (SELECT rval, media_table.jkey, media_table.dkey, media_table.tval AS tval2, foo1.tval  FROM
   (SELECT jkey, dkey,  small_table.rval, small_table.tval  FROM
     (SELECT  jkey, rval, tval  FROM
       small_table ORDER BY dkey ) foo
        JOIN small_table USING(jkey) ORDER BY dkey) foo1
          JOIN media_table USING(rval) ORDER BY jkey) foo2
             JOIN big_table USING(dkey) ORDER BY rval) foo3
GROUP BY rval2
ORDER BY rval2;

-- drop table testemp
DROP TABLE small_table;
DROP TABLE media_table;
DROP TABLE big_table;
