-- verify that resource group gucs all exist,
-- in case any of them are removed by accident.
-- do not care about the values / ranges / types

-- start_ignore
\! gpconfig -s gp_resgroup_print_operator_memory_limits;
-- end_ignore
\! echo $?;

-- start_ignore
\! gpconfig -s gp_resgroup_memory_policy_auto_fixed_mem;
-- end_ignore
\! echo $?;

-- start_ignore
\! gpconfig -s gp_resgroup_memory_policy;
-- end_ignore
\! echo $?;

-- start_ignore
\! gpconfig -s gp_resource_group_cpu_priority;
-- end_ignore
\! echo $?;

-- start_ignore
\! gpconfig -s gp_resource_group_cpu_limit;
-- end_ignore
\! echo $?;

-- start_ignore
\! gpconfig -s gp_resource_group_memory_limit;
-- end_ignore
\! echo $?;
