-- test truncate table and create table in the same transaction for ao table
begin;
create table truncate_with_create_ao(a int, b int) with (appendoptimized = true, orientation = row) distributed by (a);
insert into truncate_with_create_ao select i, i from generate_series(1,10)i;
truncate truncate_with_create_ao;
end; 

create table truncate_without_create_ao(a int, b int) with (appendoptimized = true, orientation = row) distributed by (a);
insert into truncate_without_create_ao select i, i from generate_series(1,10)i;
truncate truncate_without_create_ao;

-- the table size after truncate should be the same,
-- no matter truncate table and create table are in the same transaction or not
select pg_table_size('truncate_without_create_ao') - pg_table_size('truncate_with_create_ao');

-- test truncate table and create table in the same transaction for aocs table
begin;
create table truncate_with_create_aocs(a int, b int) with (appendoptimized = true, orientation = column) distributed by (a);
insert into truncate_with_create_aocs select i, i from generate_series(1,10)i;
truncate truncate_with_create_aocs;
end; 

create table truncate_without_create_aocs(a int, b int) with (appendoptimized = true, orientation = column) distributed by (a);
insert into truncate_without_create_aocs select i, i from generate_series(1,10)i;
truncate truncate_without_create_aocs;

-- the table size after truncate should be the same,
-- no matter truncate table and create table are in the same transaction or not
select pg_table_size('truncate_without_create_aocs') - pg_table_size('truncate_with_create_aocs');

-- test truncate table and create table in the same transaction for heap table
begin;                                                                          
create table truncate_with_create_heap(a int, b int) distributed by (a);
insert into truncate_with_create_heap select i, i from generate_series(1,10)i;
truncate truncate_with_create_heap;
end;

create table truncate_without_create_heap(a int, b int) distributed by (a);
insert into truncate_without_create_heap select i, i from generate_series(1,10)i;
truncate truncate_without_create_heap;

-- the table size after truncate should be the same,
-- no matter truncate table and create table are in the same transaction or not
select pg_table_size('truncate_without_create_heap') - pg_table_size('truncate_with_create_heap');
