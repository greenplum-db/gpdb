-- restore gp_enable_query_metrics to default
-- to avoid conflict with binary swap test

-- start_ignore
\! gpconfig -r gp_enable_query_metrics
\! PGDATESTYLE="" gpstop -rai
-- end_ignore
