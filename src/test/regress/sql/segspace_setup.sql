--
-- Setup for segspace.sql tests
-- 

-- We are setting gp_workfile_limit_per_segment to be 5.2GB so that all tests in 
-- segspace.sql can run without hitting workfile limit. The tests examine if 
-- 'used_segspace' counter is changed and get reset to 0 at the end of 
-- query execution. If gp_workfile_limit_per_segment == 0 (default value), 
-- then we don't change 'used_segspace' counter.

-- start_ignore 
\! gpconfig -c gp_workfile_limit_per_segment -v 5242880
\! PGDATESTYLE="" gpstop -rai
-- end_ignore
