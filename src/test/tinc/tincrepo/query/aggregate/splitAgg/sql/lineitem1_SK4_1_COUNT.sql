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
