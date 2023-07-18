-- verify that resource group gucs all exist,
-- in case any of them are removed by accident.
-- do not care about the values / ranges / types

-- start_ignore
\! gpconfig -s gp_resource_group_cpu_priority;
-- end_ignore
\! echo $?;

-- start_ignore
\! gpconfig -s gp_resource_group_cpu_limit;
-- end_ignore
\! echo $?;
