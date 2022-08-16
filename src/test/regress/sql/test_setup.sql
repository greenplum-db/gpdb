-- Suppress NOTICE messages when schema doesn't exist
SET client_min_messages TO 'warning';
DROP SCHEMA IF EXISTS test_util CASCADE;
SET client_min_messages TO 'notice';
CREATE SCHEMA test_util;

-- Helper function, to return the EXPLAIN output of a query as a normal
-- result set, so that you can manipulate it further.
create or replace function test_util.get_explain_output(explain_query text, is_verbose boolean)
returns setof text as
$$
declare
  explainrow text;
  sqltext text;
begin
  sqltext = 'EXPLAIN analyze ';
  if is_verbose then sqltext = sqltext || ' verbose ';
  end if;
  sqltext = sqltext || explain_query;
  for explainrow in execute sqltext
  loop
    return next explainrow;
  end loop;
end;
$$ language plpgsql;

create or replace function test_util.extract_plan_stats(explain_query text, is_verbose boolean)
  returns table (stats_name  text,
                 stats_value bigint) as
$$
begin
return query
  WITH query_plan (et) AS
(
  SELECT test_util.get_explain_output(explain_query, is_verbose)
)
SELECT
  'executor_mem_lines', (SELECT COUNT(*) FROM query_plan WHERE et like '%Executor Memory: %')
UNION
SELECT
  'workmem_wanted_lines', (SELECT COUNT(*) FROM query_plan WHERE et like '%Work_mem wanted: %');
end;
$$ language plpgsql;
