--
-- Test: cpuset cannot be specified when group is disabled.
--

-- start_ignore
DROP RESOURCE GROUP resource_group1;
-- end_ignore

CREATE RESOURCE GROUP resource_group1 WITH (cpuset='0');
CREATE RESOURCE GROUP resource_group1 WITH (cpu_hard_quota_limit=5);
ALTER RESOURCE GROUP resource_group1 SET cpuset '0';

DROP RESOURCE GROUP resource_group1;
