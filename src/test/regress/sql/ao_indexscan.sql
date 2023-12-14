-- Test index scans on append-optimized tables. We mainly test the plans being
-- generated, in addition to smoke testing the output if Index Scan is exercised.

-- Create a test ao_row table
create table aoindexscantab (i int4, j int4) with (appendonly=true, compresstype=zstd);
insert into aoindexscantab select g, g % 10000 from generate_series(1, 100000) g;
create index on aoindexscantab(j);
create index on aoindexscantab(i);
analyze aoindexscantab;

-- A simple key-value lookup query. Should use an Index scan.
explain (costs off) select i, j from aoindexscantab where i = 90;
select i, j from aoindexscantab where i = 90;

-- IndexOnlyScan should still be preferred when only the index key is involved.
explain (costs off) select i from aoindexscantab where i = 90;

-- BitmapScan should still be preferred when selectivity is higher.
explain (costs off) select * from aoindexscantab where i < 1000;

-- Should use an Index Scan as an ordering operator when limit is specified.
explain (costs off) select * from aoindexscantab order by i limit 5;
select * from aoindexscantab order by i limit 5;

-- IndexOnlyScan should still be preferred when only the index key is involved.
explain (costs off) select j from aoindexscantab order by j limit 15;

-- Create a test ao_column table
create table aocsindexscantab (i int4, j int4) with (appendonly=true, orientation=column, compresstype=zstd);
insert into aocsindexscantab select g, g % 10000 from generate_series(1, 100000) g;
create index on aocsindexscantab(j);
create index on aocsindexscantab(i);
analyze aocsindexscantab;

-- IndexOnlyScan should still be preferred when only the index key is involved.
explain (costs off) select i from aocsindexscantab where i = 90;

-- Should use an Index Scan as an ordering operator when limit is specified.
explain (costs off) select * from aocsindexscantab order by i limit 5;
select * from aocsindexscantab order by i limit 5;

-- IndexOnlyScan should still be preferred when only the index key is involved.
explain (costs off) select j from aocsindexscantab order by j limit 15;

-- BitmapScan should still be preferred when selectivity is higher.
explain (costs off) select * from aocsindexscantab where i < 1000;
