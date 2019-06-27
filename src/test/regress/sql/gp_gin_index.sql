CREATE INDEX jidx ON testjsonb USING gin (j);
SET optimizer_enable_tablescan = off;
SET enable_seqscan = off;
set enable_bitmapscan = on;

EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"wait":null}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"wait":"CC"}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"wait":"CC", "public":true}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"age":25}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"age":25.0}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"array":["foo"]}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"array":["bar"]}';

SELECT count(*) FROM testjsonb WHERE j @> '{"wait":null}';
SELECT count(*) FROM testjsonb WHERE j @> '{"wait":"CC"}';
SELECT count(*) FROM testjsonb WHERE j @> '{"wait":"CC", "public":true}';
SELECT count(*) FROM testjsonb WHERE j @> '{"age":25}';
SELECT count(*) FROM testjsonb WHERE j @> '{"age":25.0}';
SELECT count(*) FROM testjsonb WHERE j @> '{"array":["foo"]}';
SELECT count(*) FROM testjsonb WHERE j @> '{"array":["bar"]}';
-- exercise GIN_SEARCH_MODE_ALL
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j ? 'public';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j ? 'bar';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j ?| ARRAY['public','disabled'];
EXPLAIN SELECT count(*) FROM testjsonb WHERE j ?& ARRAY['public','disabled'];

SELECT count(*) FROM testjsonb WHERE j @> '{}';
SELECT count(*) FROM testjsonb WHERE j ? 'public';
SELECT count(*) FROM testjsonb WHERE j ? 'bar';
SELECT count(*) FROM testjsonb WHERE j ?| ARRAY['public','disabled'];
SELECT count(*) FROM testjsonb WHERE j ?& ARRAY['public','disabled'];

-- array exists - array elements should behave as keys (for GIN index scans too)
CREATE INDEX jidx_array ON testjsonb USING gin((j->'array'));
-- gin index on expression not support for orca
EXPLAIN SELECT count(*) from testjsonb  WHERE j->'array' ? 'bar';
SELECT count(*) from testjsonb  WHERE j->'array' ? 'bar';
-- type sensitive array exists - should return no rows (since "exists" only
-- matches strings that are either object keys or array elements)
-- gin index on expression not support for orca
EXPLAIN SELECT count(*) from testjsonb  WHERE j->'array' ? '5'::text;
SELECT count(*) from testjsonb  WHERE j->'array' ? '5'::text;
-- However, a raw scalar is *contained* within the array
EXPLAIN SELECT count(*) from testjsonb  WHERE j->'array' @> '5'::jsonb;
SELECT count(*) from testjsonb  WHERE j->'array' @> '5'::jsonb;
DROP INDEX jidx_array;

--gin path opclass
DROP INDEX jidx;
CREATE INDEX jidx ON testjsonb USING gin (j jsonb_path_ops);

EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"wait":null}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"wait":"CC"}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"wait":"CC", "public":true}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"age":25}';
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{"age":25.0}';
SELECT count(*) FROM testjsonb WHERE j @> '{"wait":null}';
SELECT count(*) FROM testjsonb WHERE j @> '{"wait":"CC"}';
SELECT count(*) FROM testjsonb WHERE j @> '{"wait":"CC", "public":true}';
SELECT count(*) FROM testjsonb WHERE j @> '{"age":25}';
SELECT count(*) FROM testjsonb WHERE j @> '{"age":25.0}';
-- exercise GIN_SEARCH_MODE_ALL
EXPLAIN SELECT count(*) FROM testjsonb WHERE j @> '{}';
SELECT count(*) FROM testjsonb WHERE j @> '{}';
DROP INDEX jidx;

-- check some corner cases for indexed nested containment (bug #13756)
create temp table nestjsonb (j jsonb);
insert into nestjsonb (j) values ('{"a":[["b",{"x":1}],["b",{"x":2}]],"c":3}');
insert into nestjsonb (j) values ('[[14,2,3]]');
insert into nestjsonb (j) values ('[1,[14,2,3]]');
create index on nestjsonb using gin(j jsonb_path_ops);

explain select * from nestjsonb where j @> '{"a":[[{"x":2}]]}'::jsonb;
explain select * from nestjsonb where j @> '{"c":3}';
explain select * from nestjsonb where j @> '[[14]]';

select * from nestjsonb where j @> '{"a":[[{"x":2}]]}'::jsonb;
select * from nestjsonb where j @> '{"c":3}';
select * from nestjsonb where j @> '[[14]]';

CREATE INDEX wowidx ON test_tsvector USING gin (a);
-- GIN only supports bitmapscan, so no need to test plain indexscan
explain (costs off) SELECT count(*) FROM test_tsvector WHERE a @@ 'wr|qh';
EXPLAIN SELECT count(*) FROM test_tsvector WHERE a @@ 'wr|qh';
EXPLAIN SELECT count(*) FROM test_tsvector WHERE a @@ 'wr&qh';
EXPLAIN SELECT count(*) FROM test_tsvector WHERE a @@ 'eq&yt';
EXPLAIN SELECT count(*) FROM test_tsvector WHERE a @@ 'eq|yt';
EXPLAIN SELECT count(*) FROM test_tsvector WHERE a @@ '(eq&yt)|(wr&qh)';
EXPLAIN SELECT count(*) FROM test_tsvector WHERE a @@ '(eq|yt)&(wr|qh)';
EXPLAIN SELECT count(*) FROM test_tsvector WHERE a @@ 'w:*|q:*';
-- For orca, ScalarArrayOpExpr condition on index scan not supported
EXPLAIN SELECT count(*) FROM test_tsvector WHERE a @@ any ('{wr,qh}');
EXPLAIN SELECT count(*) FROM test_tsvector WHERE a @@ 'no_such_lexeme';
EXPLAIN SELECT count(*) FROM test_tsvector WHERE a @@ '!no_such_lexeme';

SELECT count(*) FROM test_tsvector WHERE a @@ 'wr|qh';
SELECT count(*) FROM test_tsvector WHERE a @@ 'wr&qh';
SELECT count(*) FROM test_tsvector WHERE a @@ 'eq&yt';
SELECT count(*) FROM test_tsvector WHERE a @@ 'eq|yt';
SELECT count(*) FROM test_tsvector WHERE a @@ '(eq&yt)|(wr&qh)';
SELECT count(*) FROM test_tsvector WHERE a @@ '(eq|yt)&(wr|qh)';
SELECT count(*) FROM test_tsvector WHERE a @@ 'w:*|q:*';
-- For orca, ScalarArrayOpExpr condition on index scan not supported
SELECT count(*) FROM test_tsvector WHERE a @@ any ('{wr,qh}');
SELECT count(*) FROM test_tsvector WHERE a @@ 'no_such_lexeme';
SELECT count(*) FROM test_tsvector WHERE a @@ '!no_such_lexeme';

DROP INDEX wowidx;

-- GIN index on complex array
CREATE TABLE complex_array_table (complex_arr complex[]);
CREATE INDEX ON complex_array_table USING gin (complex_arr);

INSERT INTO complex_array_table VALUES (ARRAY[COMPLEX(1,3), COMPLEX(5,7)]);
INSERT INTO complex_array_table VALUES (ARRAY[COMPLEX(2,4), COMPLEX(6,8)]);

EXPLAIN SELECT * FROM complex_array_table WHERE complex_arr @> ARRAY[COMPLEX(2,4)];
SELECT * FROM complex_array_table WHERE complex_arr @> ARRAY[COMPLEX(2,4)];
-- with orca bitmap table scan off and table scan off, orca should fallback to
-- planner to use bitmap index scan, as btree index plans are not supported with gin
set optimizer_enable_tablescan=off;
EXPLAIN SELECT * FROM complex_array_table WHERE complex_arr @> ARRAY[COMPLEX(2,4)];
DROP TABLE complex_array_table;
RESET enable_seqscan;
RESET enable_bitmapscan;
RESET optimizer_enable_tablescan;
