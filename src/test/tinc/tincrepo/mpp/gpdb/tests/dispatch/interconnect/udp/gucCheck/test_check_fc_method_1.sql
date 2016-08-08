-- 
-- @description In LOSS mode, interconnect connections shared send buffers with others, so for redistribute motion or broadcast motion, a connection can send more than one packets even though gp_interconnect_snd_queue_depth is set to 1. if one packet is dropped by ickm, an disorder packet will be detected.
-- @created 2012-11-06
-- @modified 2016-02-24
-- @tags executor
-- @gpdb_version [4.2.3.0,main]

-- Set GUC
SET gp_interconnect_snd_queue_depth = 1;

-- Create a table
CREATE TABLE small_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz') DISTRIBUTED BY (dkey);

-- Generate some data
INSERT INTO small_table VALUES(generate_series(1, 50000), generate_series(50001, 100000), sqrt(generate_series(50001, 100000)));

-- Functional tests
-- Skew with gather+redistribute
SELECT count(*) FROM small_table AS s1, small_table AS s2 where s1.jkey = s2.dkey;

-- drop table testemp
DROP TABLE small_table;


