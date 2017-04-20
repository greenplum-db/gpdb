--
-- Clean up for resource_group.sql tests
--

-- start_ignore
\! gpconfig -r gp_resource_manager
\! PGDATESTYLE="" gpstop -rai
-- end_ignore
