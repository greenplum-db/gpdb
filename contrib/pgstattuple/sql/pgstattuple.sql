CREATE EXTENSION pgstattuple;

--
-- It's difficult to come up with platform-independent test cases for
-- the pgstattuple functions, but the results for empty tables and
-- indexes should be that.
--

create table test (a int primary key, b int[]);

select * from pgstattuple('test');
select * from pgstattuple('test'::text);
select * from pgstattuple('test'::name);
select * from pgstattuple('test'::regclass);
select pgstattuple(oid) from pg_class where relname = 'test';
select pgstattuple(relname) from pg_class where relname = 'test';

select version, tree_level,
    index_size / current_setting('block_size')::int as index_size,
    root_block_no, internal_pages, leaf_pages, empty_pages, deleted_pages,
    avg_leaf_density, leaf_fragmentation
    from pgstatindex('test_pkey');
select version, tree_level,
    index_size / current_setting('block_size')::int as index_size,
    root_block_no, internal_pages, leaf_pages, empty_pages, deleted_pages,
    avg_leaf_density, leaf_fragmentation
    from pgstatindex('test_pkey'::text);
select version, tree_level,
    index_size / current_setting('block_size')::int as index_size,
    root_block_no, internal_pages, leaf_pages, empty_pages, deleted_pages,
    avg_leaf_density, leaf_fragmentation
    from pgstatindex('test_pkey'::name);
select version, tree_level,
    index_size / current_setting('block_size')::int as index_size,
    root_block_no, internal_pages, leaf_pages, empty_pages, deleted_pages,
    avg_leaf_density, leaf_fragmentation
    from pgstatindex('test_pkey'::regclass);

select pg_relpages('test');
select pg_relpages('test_pkey');
select pg_relpages('test_pkey'::text);
select pg_relpages('test_pkey'::name);
select pg_relpages('test_pkey'::regclass);
select pg_relpages(oid) from pg_class where relname = 'test_pkey';
select pg_relpages(relname) from pg_class where relname = 'test_pkey';

create index test_ginidx on test using gin (b);

select * from pgstatginindex('test_ginidx');

--
-- Test cases for auxiliary system relations of appendonly table
--
create table ao_table
(id int,
 fname text,
 lname text,
 address1 text,
 address2 text,
 city text,
 state text,
 zip text)
with (appendonly=true)
distributed by (id);
create index ao_table_fname_idx on ao_table (fname);

select pgstattuple(blkdirrelid) from pg_appendonly where relid = 'ao_table'::regclass;
select pgstattuple(segrelid) from pg_appendonly where relid = 'ao_table'::regclass;
select pgstattuple(visimaprelid) from pg_appendonly where relid = 'ao_table'::regclass;

select pgstattuple_approx(blkdirrelid) from pg_appendonly where relid = 'ao_table'::regclass;
select pgstattuple_approx(segrelid) from pg_appendonly where relid = 'ao_table'::regclass;
select pgstattuple_approx(visimaprelid) from pg_appendonly where relid = 'ao_table'::regclass;
