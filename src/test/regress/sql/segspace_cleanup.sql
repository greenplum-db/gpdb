--
-- Clean up for segspace.sql tests
-- 

-- start_ignore
\! gpconfig -r gp_workfile_limit_per_segment
\! PGDATESTYLE="" gpstop -rai
-- end_ignore
