-- Suppress NOTICE messages when schema doesn't exist
SET client_min_messages TO 'warning';
DROP SCHEMA IF EXISTS test_util CASCADE;
SET client_min_messages TO 'notice';
CREATE SCHEMA test_util;

-- Helper function, to return the EXPLAIN output of a query as a normal
-- result set, so that you can manipulate it further.
create or replace function test_util.get_explain_output(explain_query text) returns setof text as
$$
declare
  explainrow text;
begin
  for explainrow in execute 'EXPLAIN analyze ' || explain_query
  loop
    return next explainrow;
  end loop;
end;
$$ language plpgsql;

create or replace function test_util.extract_plan_stats(explain_query text)
  returns table (executor_mem_lines bigint,
                 workmem_wanted_lines bigint)
as
$$
begin
return query
  WITH query_plan (et) AS
(
  SELECT test_util.get_explain_output(explain_query)
)
SELECT
  (SELECT COUNT(*) FROM query_plan WHERE et like '%Executor Memory: %') as executor_mem_lines,
  (SELECT COUNT(*) FROM query_plan WHERE et like '%Work_mem wanted: %') as workmem_wanted_lines
;
end;
$$ language plpgsql;
