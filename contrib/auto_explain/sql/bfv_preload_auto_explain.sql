-- start_ignore
\! gpconfig -c shared_preload_libraries -v "$(psql -At -c "SELECT array_to_string(array_append(string_to_array(current_setting('shared_preload_libraries'), ','), 'auto_explain'), ',')" postgres)"
\! gpconfig -c auto_explain.log_min_duration -v 0 --skipvalidation;
\! gpconfig -c auto_explain.log_analyze -v true --skipvalidation;
\! gpstop -raiq;
\c
-- end_ignore

SET CLIENT_MIN_MESSAGES = LOG;

-- check that auto_explain doesn't work on coordinator with Gp_role is not a GP_ROLE_DISPATCH
-- Query 'SELECT count(1) from (select i from t1 limit 10) t join t2 using (i)' generate executor's slice on coordinator:
--             ->  Redistribute Motion 1:3  (slice2)
--                   Output: t1.i
--                   Hash Key: t1.i
--                   ->  Limit
--                         Output: t1.i
--                         ->  Gather Motion 3:1  (slice1; segments: 3)
-- IMPORTANT: ./configure with --enable-orca

CREATE TABLE t1(i int);
CREATE TABLE t2(i int);
SELECT count(1) from (select i from t1 limit 10) t join t2 using (i);
DROP TABLE t1;
DROP TABLE t2;

-- start_ignore
\! gpconfig -r auto_explain.log_min_duration;
\! gpconfig -r auto_explain.log_analyze;
\! gpconfig -c shared_preload_libraries -v "$(psql -At -c "SELECT array_to_string(array_remove(string_to_array(current_setting('shared_preload_libraries'), ','), 'auto_explain'), ',')" postgres)"
\! gpstop -raiq;
-- end_ignore
