
-- Helper function, to return the EXPLAIN ANALYZE output of a query as a normal
-- result set, so that you can manipulate it further.
create or replace function get_explain_output(explain_query text) returns setof text as
$$
declare
  explainrow text;
begin
  for explainrow in execute 'EXPLAIN ANALYZE ' || explain_query
  loop
    return next explainrow;
  end loop;
end;
$$ language plpgsql;


--
-- Test explain_memory_verbosity option
-- 
CREATE TABLE explaintest (id int4);
INSERT INTO explaintest SELECT generate_series(1, 10);

EXPLAIN ANALYZE SELECT * FROM explaintest;

set explain_memory_verbosity='summary';

-- The plan should consist of a Gather and a Seq Scan, with a
-- "Memory: ..." line on both nodes.
SELECT COUNT(*) from
  get_explain_output($$
    SELECT * FROM explaintest;
  $$) as et
WHERE et like '%Memory: %';
