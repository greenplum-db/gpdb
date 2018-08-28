create schema gpexplain;
set search_path = gpexplain;

-- Helper function, to return the EXPLAIN output of a query as a normal
-- result set, so that you can manipulate it further.
create or replace function get_explain_output(explain_query text) returns setof text as
$$
declare
  explainrow text;
begin
  for explainrow in execute 'EXPLAIN ' || explain_query
  loop
    return next explainrow;
  end loop;
end;
$$ language plpgsql;

-- Same, for EXPLAIN ANALYZE VERBOSE
create or replace function get_explain_analyze_output(explain_query text) returns setof text as
$$
declare
  explainrow text;
begin
  for explainrow in execute 'EXPLAIN (ANALYZE, VERBOSE) ' || explain_query
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
  get_explain_analyze_output($$
    SELECT * FROM explaintest;
  $$) as et
WHERE et like '%Memory: %';

reset explain_memory_verbosity;


-- Verify that the column references are OK. This tests for an old ORCA bug,
-- where the Filter clause in the IndexScan of this query was incorrectly
-- printed as something like:
--
--   Filter: "outer".column2 = mpp22263.*::text

CREATE TABLE mpp22263 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) distributed by (unique1);

create index mpp22263_idx1 on mpp22263 using btree(unique1);

explain select * from mpp22263, (values(147, 'RFAAAA'), (931, 'VJAAAA')) as v (i, j)
WHERE mpp22263.unique1 = v.i and mpp22263.stringu1 = v.j;

-- atmsort.pm masks out differences in the Filter line, so just memorizing
-- the output of the above EXPLAIN isn't enough to catch a faulty Filter line.
-- Extract the Filter explicitly.
SELECT * from
  get_explain_output($$
select * from mpp22263, (values(147, 'RFAAAA'), (931, 'VJAAAA')) as v (i, j)
WHERE mpp22263.unique1 = v.i and mpp22263.stringu1 = v.j;
  $$) as et
WHERE et like '%Filter: %';

--
-- Join condition in explain plan should represent constants with proper
-- variable name
--
create table foo (a int) distributed randomly;
-- "outer", "inner" prefix must also be prefixed to variable name as length of rtable > 1
SELECT trim(et) et from
get_explain_output($$ 
	select * from (values (1)) as f(a) join (values(2)) b(b) on a = b join foo on true join foo as foo2 on true $$) as et
WHERE et like '%Join Filter:%' or et like '%Hash Cond:%';

SELECT trim(et) et from
get_explain_output($$
	select * from (values (1)) as f(a) join (values(2)) b(b) on a = b$$) as et
WHERE et like '%Hash Cond:%';

--
-- Test EXPLAINing of the Partition By in a window function. (PostgreSQL
-- doesn't print it at all.)
--
explain (costs off) select count(*) over (partition by g) from generate_series(1, 10) g;
