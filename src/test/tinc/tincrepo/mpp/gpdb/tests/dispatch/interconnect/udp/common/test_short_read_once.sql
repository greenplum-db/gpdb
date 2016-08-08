-- 
-- @description interconnect tests for MPP 17970 short read 
-- @created 2012-10-15 09:00:00
-- @modified 2012-10-15 09:00:00
-- @tags executor
-- @gpdb_version [4.2.3.0,4.2]
-- Test MPP-17970 
-- Create a table
CREATE TABLE ic_short_read_table(dkey INT, jkey INT, rval REAL, tval TEXT default 'abcdefghijklmnopqrstuvwxyz') DISTRIBUTED BY (dkey);

-- Issue a query to make later queries elide ic setup while injecting faults
SELECT count(*) from ic_short_read_table;

-- Generate some data
INSERT INTO ic_short_read_table VALUES(1, generate_series(1, 6000), sqrt(generate_series(1, 6000)));

-- set GUC
SET gp_udpic_fault_inject_percent = 25;

SELECT count(*) from ic_short_read_table ic1 join ic_short_read_table ic2 on ic1.dkey = ic2.jkey;

-- reset GUC
SET gp_udpic_fault_inject_percent = 0;

-- drop table testemp
DROP TABLE ic_short_read_table;

