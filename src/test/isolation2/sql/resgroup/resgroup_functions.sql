-- start_ignore
SELECT s.groupid, s.num_running, s.num_queueing, s.num_queued, s.num_executed
FROM pg_resgroup_get_status(NULL::oid) s(groupid, num_running, num_queueing, num_queued, num_executed, total_queue_duration);
-- end_ignore
CREATE TEMP TABLE resgroup_function_test(LIKE gp_toolkit.gp_resgroup_status);

INSERT INTO resgroup_function_test(groupid, num_running, num_queueing, num_queued, num_executed)
SELECT s.groupid, s.num_running, s.num_queueing, s.num_queued, s.num_executed
FROM pg_resgroup_get_status(NULL::oid) s(groupid, num_running, num_queueing, num_queued, num_executed, total_queue_duration) LIMIT 1;

SELECT count(num_executed)>0 FROM resgroup_function_test WHERE num_executed IS NOT NULL;

-- Check that the contents of the cpu_usage field are valid JSON
ANALYZE resgroup_function_test;
