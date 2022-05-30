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
