-- start_matchsubs
-- # Remove all successful
-- m/INFO:  Success.*\.c:\d+/
-- s/\d+/###/
-- end_matchsubs

--start_ignore
CREATE EXTENSION IF NOT EXISTS test_planner;
--end_ignore

SELECT test_planner();
DROP EXTENSION test_planner;
