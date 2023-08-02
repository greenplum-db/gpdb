-- gp_enable_query_metrics GUC will enable instrumentation
-- collection for every query in following tests.
-- After all tests finished, the last test check_instr_in_shmem
-- will check for leaks of instrumentation slots in shmem.

-- start_ignore
\! gpconfig -c gp_enable_query_metrics -v on 
\! PGDATESTYLE="" gpstop -raf
-- end_ignore
