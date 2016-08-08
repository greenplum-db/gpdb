-- 
-- @description In CAPACITY mode, interconnect connections own their own send buffers, so for redistribute motion or broadcast motion, a connection can only send one packet one time when gp_interconnect_snd_queue_depth is set to 1. if one packet is dropped by ickm, no disorder packet will be detected.
-- @created 2012-11-06
-- @modified 2016-02-24
-- @tags executor
-- @gpdb_version [4.2.3.0,main]

-- Set GUC
SET gp_interconnect_snd_queue_depth = 1;
SET gp_interconnect_fc_method = "capacity";

-- Create a table
CREATE TABLE small_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz') DISTRIBUTED BY (dkey);

-- Generate some data
INSERT INTO small_table VALUES(generate_series(1, 50000), generate_series(50001, 100000), sqrt(generate_series(50001, 100000)));

-- Functional tests
-- Skew with gather+redistribute
SELECT count(*) FROM small_table AS s1, small_table AS s2 where s1.jkey = s2.dkey;

-- drop table testemp
DROP TABLE small_table;


