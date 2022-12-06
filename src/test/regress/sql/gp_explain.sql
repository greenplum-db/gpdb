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

EXPLAIN ANALYZE SELECT id FROM 
( SELECT id 
	FROM explaintest
	WHERE id > (
		SELECT avg(id)
		FROM explaintest
	)
) as foo
ORDER BY id
LIMIT 1;


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


--
-- Test non-text format with a few queries that contain GPDB-specific node types.
--

-- The default init_file rules contain a line to mask this out in normal
-- text-format EXPLAIN output, but it doesn't catch these alternative formats.
-- start_matchignore
-- m/Optimizer.*Pivotal Optimizer \(GPORCA\)/
-- end_matchignore

CREATE EXTERNAL WEB TABLE dummy_ext_tab (x text) EXECUTE 'echo foo' FORMAT 'text';

-- External Table Scan
explain (format json, costs off) SELECT * FROM dummy_ext_tab;

-- Seq Scan on an append-only table
CREATE TEMP TABLE dummy_aotab (x int4) WITH (appendonly=true);
explain (format yaml, costs off) SELECT * FROM dummy_aotab;

-- DML node (with ORCA)
explain (format xml, costs off) insert into dummy_aotab values (1);

-- github issues 5795. explain fails previously.
--start_ignore
explain SELECT * from information_schema.key_column_usage;
--end_ignore

-- github issue 5794.
set gp_enable_explain_allstat=on;
explain analyze SELECT * FROM explaintest;
set gp_enable_explain_allstat=DEFAULT;

--
-- Test output of EXPLAIN ANALYZE for Bitmap index scan's actual rows.
--

-- Return EXPLAIN ANALYZE result as xml to manipulate it further.
create or replace function get_explain_analyze_xml_output(explain_query text)
returns xml as
$$
declare
  x xml;
begin
  execute 'EXPLAIN (ANALYZE, VERBOSE, FORMAT XML) ' || explain_query
  into x;
  return x;
end;
$$ language plpgsql;

-- force (Dynamic) Bitmap Index Scan
set optimizer_enable_dynamictablescan=off;
set enable_seqscan=off;

-- Each test case below creates partitioned table with index atop of it.
-- Each test case covers its own index type supported by GPDB and shows actual
-- number of rows processed. This number should be more than 1 after fix
-- (except bitmap, see below).
-- The final call to get_explain_analyze_xml_output finds xml node with
-- "Bitmap Index Scan" value, then goes to parent node (..), and finally
-- extracts "Node-Type" and "Actual-Rows" nodes values. Additionally,
-- "Optimizer" value extracted.

-- btree index test case
create table bitmap_btree_test(dist_col int, idx_col int)
with (appendonly=true, compresslevel=5, compresstype=zlib)
distributed by (dist_col)
partition by range(idx_col) 
(start (0) inclusive end (999) inclusive every (500));

insert into bitmap_btree_test
select i, i % 1000
from generate_series(0,10000) i;

create index bitmap_btree_test_idx on bitmap_btree_test
using btree(idx_col);

--both optimizers should show more than 1 row actually processed
select xpath('//*[contains(text(), "Bitmap Index Scan")]/..
              /*[local-name()="Node-Type" or local-name()="Actual-Rows"]
              /text()', x) type_n_rows,
       xpath('//*[local-name()="Optimizer"]/text()', x) opt
from get_explain_analyze_xml_output($$
    select * from bitmap_btree_test where idx_col = 900;
    $$) as x;

-- bitmap index test case
create table bitmap_bitmap_test(dist_col int, idx_col int)
with (appendonly=true, compresslevel=5, compresstype=zlib)
distributed by (dist_col)
partition by range(idx_col)
(start (0) inclusive end (999) inclusive every (500));

insert into bitmap_bitmap_test
select i, i % 1000
from generate_series(0,10000) i;

create index bitmap_bitmap_test_idx on bitmap_bitmap_test
using bitmap(idx_col);

--both optimizers should show 1 row actually processed, because bitmap index
--doesn't have a precise idea of the number of heap tuples involved
select xpath('//*[contains(text(), "Bitmap Index Scan")]/..
              /*[local-name()="Node-Type" or local-name()="Actual-Rows"]
              /text()', x) type_n_rows,
       xpath('//*[local-name()="Optimizer"]/text()', x) opt
from get_explain_analyze_xml_output($$
    select * from bitmap_bitmap_test where idx_col = 900;
    $$) as x;

-- gist index test case
create table bitmap_gist_test(dist_col int, part_col int, idx_col int4range)
with (appendonly=true, compresslevel=5, compresstype=zlib)
distributed by (dist_col)
partition by range(part_col)
(start (0) inclusive end (999) inclusive every (500));

insert into bitmap_gist_test
select i, i % 1000, int4range(i % 1000, i % 1000, '[]')
from generate_series(0,10000) i;

create index bitmap_gist_test_idx on bitmap_gist_test
using gist(idx_col);

--both optimizers should show more than 1 row actually processed
select xpath('//*[contains(text(), "Bitmap Index Scan")]/..
              /*[local-name()="Node-Type" or local-name()="Actual-Rows"]
              /text()', x) type_n_rows,
       xpath('//*[local-name()="Optimizer"]/text()', x) opt
from get_explain_analyze_xml_output($$
    select * from bitmap_gist_test where idx_col @> 900;
    $$) as x;

-- spgist index test case
create table bitmap_spgist_test(dist_col int, part_col int, idx_col int4range)
with (appendonly=true, compresslevel=5, compresstype=zlib)
distributed by (dist_col)
partition by range(part_col)
(start (0) inclusive end (999) inclusive every (500));

insert into bitmap_spgist_test
select i, i % 1000, int4range(i % 1000, i % 1000, '[]')
from generate_series(0,10000) i;

create index bitmap_spgist_test_idx on bitmap_spgist_test
using spgist(idx_col);

--both optimizers should show more than 1 row actually processed
--spgist index is not supported by ORCA, falling back to Postgres optimizer
select xpath('//*[contains(text(), "Bitmap Index Scan")]/..
              /*[local-name()="Node-Type" or local-name()="Actual-Rows"]
              /text()', x) type_n_rows,
       xpath('//*[local-name()="Optimizer"]/text()', x) opt
from get_explain_analyze_xml_output($$
    select * from bitmap_spgist_test where idx_col @> 900;
    $$) as x;

-- gin index test case

create table bitmap_gin_test(dist_col int, part_col int, idx_col int[])
with (appendonly=true, compresslevel=5, compresstype=zlib)
distributed by (dist_col)
partition by range(part_col)
(start (0) inclusive end (999) inclusive every (500));

insert into bitmap_gin_test
select i, i % 1000, array[(i % 1000)]
from generate_series(0,10000) i;

create index bitmap_gin_test_idx on bitmap_gin_test
using gin(idx_col);

--both optimizers should show more than 1 row actually processed
select xpath('//*[contains(text(), "Bitmap Index Scan")]/..
              /*[local-name()="Node-Type" or local-name()="Actual-Rows"]
              /text()', x) type_n_rows,
       xpath('//*[local-name()="Optimizer"]/text()', x) opt
from get_explain_analyze_xml_output($$
    select * from bitmap_gin_test where idx_col @> array[900];
    $$) as x;

reset optimizer_enable_dynamictablescan;
reset enable_seqscan;
reset search_path;

-- If all QEs hit errors when executing sort, we might not receive stat data for sort.
-- rethrow error before print explain info.
create extension if not exists gp_inject_fault;
create table sort_error_test1(tc1 int, tc2 int);
create table sort_error_test2(tc1 int, tc2 int);
insert into sort_error_test1 select i,i from generate_series(1,20) i;
select gp_inject_fault('explain_analyze_sort_error', 'error', dbid)
    from gp_segment_configuration where role = 'p' and content > -1;
EXPLAIN analyze insert into sort_error_test2 select * from sort_error_test1 order by 1;
select count(*) from sort_error_test2;
select gp_inject_fault('explain_analyze_sort_error', 'reset', dbid)
    from gp_segment_configuration where role = 'p' and content > -1;
drop table sort_error_test1;
drop table sort_error_test2;
