--
-- Setup for resource_group.sql tests
--

-- We are setting gp_resource_manager to be 'group' to enable resource group.

-- start_ignore
\! gpconfig -c gp_resource_manager -v group
\! PGDATESTYLE="" gpstop -rai
-- end_ignore
