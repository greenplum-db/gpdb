SET CLIENT_MIN_MESSAGES = LOG;
LOAD 'auto_explain';
SET auto_explain.log_analyze = TRUE;
SET auto_explain.log_min_duration = 0;
SET auto_explain.log_buffers = FALSE;
SET auto_explain.log_triggers = TRUE;
SET auto_explain.log_nested_statements = FALSE;
SET auto_explain.log_timing = FALSE;
SET auto_explain.log_verbose = FALSE;

SELECT relname FROM pg_class WHERE relname='pg_class';
SELECT count(*)>0 FROM pg_class, pg_index WHERE oid = indrelid AND indisunique;

SET auto_explain.log_min_duration = 1;
SET auto_explain.log_triggers = FALSE;
SET auto_explain.log_verbose = TRUE;

SELECT relname FROM pg_class WHERE relname='pg_class';
SELECT count(*)>0 FROM pg_class, pg_index WHERE oid = indrelid AND indisunique;
