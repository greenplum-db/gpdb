CREATE SCHEMA auto_explain_mem_leak_test;
SET search_path=auto_explain_mem_leak_test;

LOAD 'auto_explain';
-- Enable auto_explain. Log all plans
SET auto_explain.log_min_duration = 0;
-- Log statements executed inside a function
SET auto_explain.log_nested_statements=true;
-- Collect data for EXPLAIN ANALYZE output. The data will be collected for every
-- query executed inside a function
SET auto_explain.log_analyze = true;

CREATE OR REPLACE FUNCTION get_executor_mem(calls_num int) returns int 
language plpgsql
as $$
declare
    line text;
    mem int[];
    total int = 0;
begin
    for line in execute(
       'EXPLAIN ANALYZE 
        SELECT information_schema._pg_truetypid(a, t)
        FROM (
            SELECT a as a, t as t
            FROM pg_type t
                JOIN pg_attribute a ON a.atttypid = t.oid
            WHERE typtype = ''d''
            LIMIT 1
        ) at 
            JOIN generate_series(1, $1) gc ON true')
    using calls_num	
    loop
        mem = regexp_matches(line, 'Executor memory: (\d+)K');
        continue when mem is null;
        total = total + mem[1];
    end loop;

    return total;
end
$$;


-- Memory usage should not depend much on how many times a function written in 
-- sql language, that optimizers cannot inline, is called during query execution.
-- The information_schema._pg_truetypid() function is called here.
-- The amount of memory used for 50,000 calls and 1000 calls should not differ
-- by more than 10 MB.
SELECT abs(get_executor_mem(50000) - get_executor_mem(1000)) < 10000;

-- clean
DROP FUNCTION get_executor_mem(calls_num int);
DROP SCHEMA auto_explain_mem_leak_test;
