CREATE OR REPLACE FUNCTION index_info(regclass)
RETURNS TABLE(indexname regclass, parentindex regclass, isleaf boolean, level int, tablename regclass, indpred text, indnatts smallint, indnkeyatts smallint, indkey int2vector)
AS
$$ select t.*, indrelid::regclass as tablename, pg_get_expr(indpred, relid), indnatts, indnkeyatts, indkey
   from pg_partition_tree($1) as t, pg_index as i where relid = indexrelid
$$
LANGUAGE SQL EXECUTE ON COORDINATOR;

--
-- ************************************************************
-- * Scenario 1
-- * 	- single level partition.
-- *	- no default parts, no dropped columns
-- ************************************************************
--
create table part_table1
(
  part_key int,
  content  text
)
partition by range(part_key)
(
	partition part1 start ('1') end ('10'),
	partition part2 start('11') end ('20')
);

-- physical index on all parts
-- output show 1 logical index 
create index rp_i1 on part_table1(part_key);

select * from index_info('rp_i1'::regclass);

-- partial index on all part
-- output shows 1 logical index with partial predicate 
create index parent_partial_ind1 on part_table1(part_key)
where part_key >=1 and part_key < 10;

select * from index_info('parent_partial_ind1'::regclass);

-- add a new part - index automatically created 
alter table part_table1 add partition part4 start('31') end ('40');

select * from index_info('rp_i1'::regclass);
select * from index_info('parent_partial_ind1'::regclass);

-- create index on just 1 part
create index rp_i2 on part_table1_1_prt_part4(part_key, content);
-- no result as not partitioned index
select * from index_info('rp_i2'::regclass);
select relkind from pg_class where relname = 'rp_i2';

-- split a part into 2 parts - index created on both the parts
-- the existing part4 along with indexes is dropped 
-- and 2 new parts are added - NO new indexes create
alter table part_table1 split partition for ('31') at ('35') into (partition part41, partition part42);

select * from index_info('rp_i1'::regclass);
select * from index_info('parent_partial_ind1'::regclass);
select relkind from pg_class where relname = 'rp_i2';

--
-- ************************************************************
-- * Scenario 2
-- * 	- single level partition.
-- *	- testing exchange 
-- ************************************************************
--
create table exchange_tab
(
  part_key int,
  content text
);

-- These indexes must exist on the table before we can exchange, because
-- they exist as a partitioned index on part_table1
create index like_rp_i1 on exchange_tab(part_key);
create index like_parent_partial_ind1 on exchange_tab(part_key)
where part_key >=1 and part_key < 10;

create index e1 on exchange_tab(lower(content));

-- exchanging with part2
-- the 3 indexs on exchange_tab will move along and become part of partition
alter table part_table1 exchange partition for ('11') with table exchange_tab;

select relname, relkind from pg_class where oid in (select indexrelid from pg_index where indrelid = 'exchange_tab'::regclass);
select indexrelid::regclass as indexname, indrelid::regclass as tablename from pg_index where indexrelid = 'e1'::regclass;
select * from index_info('rp_i1'::regclass);
select * from index_info('parent_partial_ind1'::regclass);

--
-- ************************************************************
-- * Scenario 3
-- * 	- multi-level partitions 
-- ************************************************************
--
create table part_table2 (trans_id int, date date, region text)
distributed by (trans_id)
partition by range(date)
subpartition by list(region)
subpartition template
(
 subpartition usa    values ('usa'),
 subpartition asia   values ('asia'),
 subpartition europe values ('europe'),
 default subpartition other_regions)
 (start (date '2008-01-01') end (date '2009-01-01')
 every (interval '6 month'),
 default partition outlying_years);

-- create indexes (will be created on all parts/subparts)
create index first_index on part_table2(trans_id);
select * from index_info('first_index'::regclass);

-- create a partial index on part_table2 (will be created on all parts/subparts)
create index second_index on part_table2(trans_id)  
where trans_id >=1000 and trans_id < 2000;  
select * from index_info('second_index'::regclass);

--
-- ************************************************************
-- * Scenario 3
-- * 	- multi-level partitions
-- * 	- truncate partition
-- * 	- dropped columns
-- ************************************************************
--
create table part_table3
(
  date date,
  region text,
  region1 text,
  amount decimal(10,2)
)
partition by range(date)
subpartition by list(region)
(
 partition part1 start(date '2008-01-01') end(date '2009-01-01')
 	(
	subpartition usa    values ('usa'),
 	subpartition asia   values ('asia'),
	default subpartition def
	),
 partition part2 start(date '2009-01-01') end(date '2010-01-01')
 	(
	subpartition usa    values ('usa'),
 	subpartition asia   values ('asia')
	)
);

-- insert some data
insert into part_table3 values ('2008-02-02', 'usa', 'Texas', 10.05), ('2008-03-03', 'asia', 'China', 1.01);
insert into part_table3 values ('2009-02-02', 'usa', 'Texas', 10.05), ('2009-03-03', 'asia', 'China', 1.01);

-- index on atts 1, 4
create index i1 on part_table3(date, amount); 
select * from index_info('i1'::regclass);

-- truncate partitions until table is empty
select * from part_table3;
truncate part_table3_1_prt_part1_2_prt_asia;
select * from part_table3;
alter table part_table3 truncate partition part1;
select * from part_table3;
alter table part_table3 alter partition part2 truncate partition usa;
select * from part_table3;
alter table part_table3 truncate partition part2;
select * from part_table3;

-- drop column region1
alter table part_table3 drop column region1;

-- the index i1 on this part has atts - 1 3
alter table part_table3 add partition part3 start(date '2010-01-01') end (date '2011-01-01')
(subpartition usa values('usa'));
select * from index_info('i1'::regclass);

-- dropped cols in the ind expr 
-- index expr varno are different
create index i41 on part_table3_1_prt_part3_2_prt_usa(ceil(amount));
create index i42 on part_table3_1_prt_part2_2_prt_usa(ceil(amount));
create index i43 on part_table3_1_prt_part2_2_prt_asia(ceil(amount));
select indrelid::regclass as tablename, indnatts, indnkeyatts, indkey, pg_get_expr(indexprs, indrelid) from pg_index where indexrelid = 'i41'::regclass;
select indrelid::regclass as tablename, indnatts, indnkeyatts, indkey, pg_get_expr(indexprs, indrelid) from pg_index where indexrelid = 'i42'::regclass;
select indrelid::regclass as tablename, indnatts, indnkeyatts, indkey, pg_get_expr(indexprs, indrelid) from pg_index where indexrelid = 'i43'::regclass;

-- ************************************************************
-- * Scenario 8
-- *    - open-ended partitions
-- ************************************************************
-- AO partitioned tables: Orca treats them as bitmap indexes
create table part_table11(a int, b int, c int) with (appendonly=true) partition by list(b) (partition p1 values(1), partition p2 values(2));
create index part_table11_idx on part_table11(c);
select * from index_info('part_table11_idx'::regclass);

-- mixed heap, AO and AOCO tables: Orca treats them as bitmap indexes
create table part_table12(a int, b int, c int) partition by list(b) 
(partition p1 values(1), 
partition p2 values(2) with (appendonly=true, orientation=column),
partition p3 values(3) with (appendonly=true),
partition p4 values(4)
);
create index part_table12_idx on part_table12(c);
select * from index_info('part_table12_idx'::regclass);

--
-- ************************************************************
-- * Scenario 10
-- * 	- multi-levels partition exchange with external table
-- *
-- ************************************************************
--
create table exchange_sub_partition (a int,b int)
distributed by (a)
partition by range (a)
subpartition by range (b)
subpartition template
(start(1) end(3) every(1))
(start(1) end(3) every(1));

create external web table exchange_external_table(like exchange_sub_partition_1_prt_1_2_prt_1)
execute 'printf "2,2\n1,1\n"' on host
format 'csv';

alter table exchange_sub_partition
alter partition for (integer '2')
exchange partition for (integer '2') with table exchange_external_table;

select * from exchange_sub_partition;

--
-- ************************************************************
-- * Scenario 11
-- *  - Test unnamed index creation on all partitions for a
-- *      single level partitioned table.
-- *  - no default parts, no dropped columns
-- ************************************************************
--
create table unnamed_index_part_table
(
  part_key int,
  index_key int
)
partition by range(part_key)
(
  partition part1 end (3),
  partition part2 start (3)
);

-- physical unnamed index on all parts
-- output shows indexes created on all parts
create index on unnamed_index_part_table(index_key);

SELECT n.nspname as "Schema", c.relname as "Name", c.relkind, c2.relname as "Table"
FROM pg_catalog.pg_class c
     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace
     LEFT JOIN pg_catalog.pg_index i ON i.indexrelid = c.oid
     LEFT JOIN pg_catalog.pg_class c2 ON i.indrelid = c2.oid
WHERE c.relkind IN ('i', 'I') AND c2.relname like 'unnamed_index_part_table%';

--
-- ************************************************************
-- * Scenario 12
-- * 	- unnamed indexes on multi-level partitions
-- ************************************************************
--
create table unnamed_index_multi_part_table
(
  date date,
  region text,
  region1 text,
  amount decimal(10,2)
)
partition by range(date)
subpartition by list(region)
(
 partition part1 start(date '2008-01-01') end(date '2009-01-01')
	(
	subpartition usa	values ('usa'),
	subpartition asia	values ('asia'),
	default subpartition def
	),
 partition part2 start(date '2009-01-01') end(date '2010-01-01')
	(
	subpartition usa	values ('usa'),
	subpartition asia	values ('asia')
	)
);

-- create unnamed indexe (will be created on all parts/subparts)
create index on unnamed_index_multi_part_table(date, amount);

SELECT n.nspname as "Schema", c.relname as "Name", c.relkind, c2.relname as "Table"
FROM pg_catalog.pg_class c
     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace
     LEFT JOIN pg_catalog.pg_index i ON i.indexrelid = c.oid
     LEFT JOIN pg_catalog.pg_class c2 ON i.indrelid = c2.oid
WHERE c.relkind IN ('i','I') AND c2.relname like 'unnamed_index_multi_part_table%';

drop table exchange_tab;
drop table part_table1;
drop table part_table2;
drop table part_table3;
drop table part_table11;
drop table part_table12;
drop table exchange_sub_partition;
drop table exchange_external_table;
