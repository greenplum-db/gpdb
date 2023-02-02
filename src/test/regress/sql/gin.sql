--
-- Test GIN indexes.
--
-- There are other tests to test different GIN opclasses. This is for testing
-- GIN itself.

-- Create and populate a test table with a GIN index.
create table gin_test_tbl(i int4[]) with (autovacuum_enabled = off);
create index gin_test_idx on gin_test_tbl using gin (i)
  with (fastupdate = on, gin_pending_list_limit = 4096);
insert into gin_test_tbl select array[1, 2, g] from generate_series(1, 20000) g;
insert into gin_test_tbl select array[1, 3, g] from generate_series(1, 1000) g;
analyze gin_test_tbl;

select gin_clean_pending_list('gin_test_idx')>10 as many; -- flush the fastupdate buffers

insert into gin_test_tbl select array[3, 1, g] from generate_series(1, 1000) g;

vacuum gin_test_tbl; -- flush the fastupdate buffers

select gin_clean_pending_list('gin_test_idx'); -- nothing to flush

-- Test vacuuming
delete from gin_test_tbl where i @> array[2];
vacuum gin_test_tbl;

-- Disable fastupdate, and do more insertions. With fastupdate enabled, most
-- insertions (by flushing the list pages) cause page splits. Without
-- fastupdate, we get more churn in the GIN data leaf pages, and exercise the
-- recompression codepaths.
alter index gin_test_idx set (fastupdate = off);

insert into gin_test_tbl select array[1, 2, g] from generate_series(1, 1000) g;
insert into gin_test_tbl select array[1, 3, g] from generate_series(1, 1000) g;

delete from gin_test_tbl where i @> array[2];
vacuum gin_test_tbl;

-- Test for "rare && frequent" searches
explain (costs off)
select count(*) from gin_test_tbl where i @> array[1, 999];

select count(*) from gin_test_tbl where i @> array[1, 999];

-- Very weak test for gin_fuzzy_search_limit
set gin_fuzzy_search_limit = 1000;

explain (costs off)
select count(*) > 0 as ok from gin_test_tbl where i @> array[1];

select count(*) > 0 as ok from gin_test_tbl where i @> array[1];

reset gin_fuzzy_search_limit;
