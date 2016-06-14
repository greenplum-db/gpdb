
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema lineitem;
set search_path to lineitem;
\cd optfunctional
\i data/tpch_load_heap.sql
create table lineitem1 as (select * from lineitem ORDER BY l_orderkey, l_partkey, l_suppkey,l_linenumber,l_returnflag,l_linestatus,l_receiptdate, l_shipinstruct limit 500) distributed by (l_orderkey);
create table part1 as (select * from part, partsupp where p_partkey = ps_partkey ORDER BY p_partkey,ps_partkey, ps_availqty, p_size , ps_suppkey, ps_supplycost limit 500) distributed by (p_partkey);
create table part2 as (select * from part, partsupp where p_partkey = ps_partkey ORDER BY p_partkey,ps_partkey, ps_availqty, p_size , ps_suppkey, ps_supplycost limit 500) distributed by (p_partkey, ps_suppkey);
create table lineitem2 as (select * from lineitem ORDER BY l_orderkey, l_partkey, l_suppkey,l_linenumber,l_returnflag,l_linestatus,l_receiptdate, l_shipinstruct limit 500) distributed by (l_orderkey, l_partkey);
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK0_1_1.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK0_1_1.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode enumerate
-- @skipPlanID 1 3 5
--end_ignore
SELECT SUM(DISTINCT l_partkey) FROM lineitem1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK0_1_2.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK0_1_2.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode enumerate
-- @skipPlanID 1 3 5
--end_ignore
SELECT COUNT(DISTINCT l_partkey) FROM lineitem1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK0_2_1.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK0_2_1.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode enumerate
-- @skipPlanID 1 3 5
--end_ignore
SELECT SUM(DISTINCT l_suppkey) FROM lineitem1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK0_2_2.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK0_2_2.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode enumerate
-- @skipPlanID 1 3 5
--end_ignore
SELECT COUNT(DISTINCT l_suppkey) FROM lineitem1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK1_1_COUNT.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK1_1_COUNT.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK1_1_SUM.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK1_1_SUM.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK2_1_COUNT.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK2_1_COUNT.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode enumerate
--end_ignore
SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK2_1_SUM.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK2_1_SUM.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode enumerate
--end_ignore
SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK3_1_COUNT.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK3_1_COUNT.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK3_1_SUM.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK3_1_SUM.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK3_2_COUNT.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK3_2_COUNT.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK3_2_SUM.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK3_2_SUM.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK4_1_COUNT.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK4_1_COUNT.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK4_1_SUM.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK4_1_SUM.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK5_1_0.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK5_1_0.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM part1, (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t where t.DQA1_dqacol1 = part1.p_partkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK5_1_1.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK5_1_1.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM part1, (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t where t.DQA1_dqacol1 = part1.p_partkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK5_1_2.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK5_1_2.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM part1, (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t where t.DQA1_dqacol1 = part1.p_partkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK5_1_3.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK5_1_3.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM part1, (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t where t.DQA1_dqacol1 = part1.p_partkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK6_1_0.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK6_1_0.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM part1, (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t where t.DQA1_dqacol1 = part1.p_size;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK6_1_1.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK6_1_1.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM part1, (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t where t.DQA1_dqacol1 = part1.p_size;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK6_1_2.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK6_1_2.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM part1, (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t where t.DQA1_dqacol1 = part1.p_size;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK6_1_3.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK6_1_3.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM part1, (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t where t.DQA1_dqacol1 = part1.p_size;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_0.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_0.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t1, (SELECT COUNT(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_1.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_1.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t1, (SELECT SUM(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_10.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_10.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_11.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_11.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_12.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_12.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_13.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_13.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_14.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_14.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_15.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_15.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_16.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_16.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_17.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_17.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_18.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_18.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_19.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_19.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_2.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_2.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_20.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_20.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_21.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_21.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_22.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_22.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_23.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_23.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_24.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_24.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_25.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_25.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_26.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_26.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_27.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_27.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_28.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_28.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_29.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_29.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_3.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_3.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_30.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_30.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_31.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_31.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_32.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_32.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_33.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_33.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_34.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_34.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_35.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_35.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_36.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_36.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_37.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_37.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_38.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_38.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_39.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_39.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_4.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_4.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t1, (SELECT COUNT(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_40.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_40.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_41.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_41.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_42.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_42.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_43.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_43.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_44.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_44.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_45.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_45.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_46.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_46.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_47.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_47.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_48.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_48.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_49.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_49.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_5.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_5.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t1, (SELECT SUM(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_50.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_50.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_51.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_51.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_6.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_6.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t1, (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_7.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_7.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t1, (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_8.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_8.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT COUNT(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK7_1_9.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK7_1_9.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t1, (SELECT SUM(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2 WHERE t1.DQA1_dqacol1 = t2.DQA2_dqacol1;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_1_0.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_1_0.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ANY (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_partkey ) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_1_1.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_1_1.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ANY (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_size ) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_1_2.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_1_2.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ANY (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_partkey ) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_1_3.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_1_3.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ANY (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_size ) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_1_4.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_1_4.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ANY (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_partkey  GROUP BY l_suppkey) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_1_5.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_1_5.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ANY (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_size  GROUP BY l_suppkey) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_1_6.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_1_6.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ANY (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_partkey  GROUP BY l_suppkey) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_1_7.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_1_7.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ANY (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_size  GROUP BY l_suppkey) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_2_0.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_2_0.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ALL (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_partkey ) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_2_1.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_2_1.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ALL (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_size ) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_2_2.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_2_2.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ALL (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_partkey ) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_2_3.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_2_3.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ALL (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_size ) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_2_4.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_2_4.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ALL (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_partkey  GROUP BY l_suppkey) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_2_5.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_2_5.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ALL (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_size  GROUP BY l_suppkey) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_2_6.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_2_6.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ALL (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_partkey  GROUP BY l_suppkey) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK8_2_7.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK8_2_7.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT * from part1 WHERE part1.p_partkey = ALL (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  WHERE lineitem1.l_suppkey = part1.p_size  GROUP BY l_suppkey) ;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_1_0.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_1_0.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT part1.p_partkey FROM part1) UNION (SELECT * FROM (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_1_1.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_1_1.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT part1.p_partkey FROM part1) UNION (SELECT * FROM (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_1_2.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_1_2.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT part1.p_partkey FROM part1) UNION (SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_1_3.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_1_3.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT part1.p_partkey FROM part1) UNION (SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_2_0.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_2_0.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t) UNION (SELECT part1.p_partkey FROM part1);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_2_1.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_2_1.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t) UNION (SELECT part1.p_partkey FROM part1);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_2_2.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_2_2.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT part1.p_partkey FROM part1);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_2_3.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_2_3.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT part1.p_partkey FROM part1);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_0.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_0.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_1.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_1.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_10.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_10.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_11.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_11.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_12.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_12.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_13.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_13.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_14.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_14.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_15.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_15.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_16.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_16.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_17.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_17.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_18.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_18.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_19.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_19.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_2.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_2.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_20.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_20.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_21.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_21.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_22.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_22.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_23.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_23.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_24.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_24.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_25.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_25.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_26.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_26.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_27.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_27.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_28.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_28.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_29.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_29.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_3.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_3.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_30.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_30.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_31.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_31.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_32.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_32.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_33.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_33.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_34.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_34.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_35.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_35.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_36.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_36.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_37.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_37.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_38.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_38.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_39.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_39.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(l_suppkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_linenumber) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_4.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_4.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_40.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_40.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_41.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_41.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_42.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_42.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_43.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_43.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_44.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_44.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_45.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_45.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_46.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_46.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_47.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_47.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_48.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_48.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_49.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_49.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(p_size) AS DQA2_dqacol2 FROM part1  GROUP BY ps_availqty) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_5.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_5.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_50.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_50.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_51.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_51.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_partkey) AS DQA1_dqacol1, SUM(DISTINCT l_partkey) AS DQA1_dqacol2 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1, SUM(DISTINCT p_partkey) AS DQA2_dqacol2 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_6.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_6.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_7.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_7.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT SUM(DISTINCT l_suppkey) AS DQA1_dqacol1 FROM lineitem1 ) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_partkey) AS DQA2_dqacol1 FROM part1  GROUP BY p_size) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_8.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_8.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT COUNT(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem1_SK9_3_9.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem1_SK9_3_9.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
(SELECT * FROM (SELECT COUNT(DISTINCT l_partkey) AS DQA1_dqacol1 FROM lineitem1  GROUP BY l_suppkey) as t) UNION (SELECT * FROM (SELECT SUM(DISTINCT p_size) AS DQA2_dqacol1 FROM part1 ) as t2);

-- ----------------------------------------------------------------------
-- Test: sql/lineitem2_SK10_1_COUNT.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem2_SK10_1_COUNT.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT COUNT(DISTINCT l_suppkey) FROM lineitem2 GROUP BY l_orderkey;

-- ----------------------------------------------------------------------
-- Test: sql/lineitem2_SK10_1_SUM.sql
-- ----------------------------------------------------------------------

--start_ignore
-- @author tungs1
-- @modified 2013-07-17 12:00:00
-- @created 2013-07-17 12:00:00
-- @description SplitDQA lineitem2_SK10_1_SUM.sql
-- @db_name splitdqa
-- @tags SplitAgg HAWQ
-- @executemode normal
--end_ignore
SELECT SUM(DISTINCT l_suppkey) FROM lineitem2 GROUP BY l_orderkey;

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema lineitem cascade;
-- end_ignore
