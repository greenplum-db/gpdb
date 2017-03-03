-- @gucs gp_create_table_random_default_distribution=off
-- Create test database mpp11880
drop database if exists mpp11880;
create database mpp11880;
\c mpp11880

-- Create test table test_10m
create table public.test_10m (Same text, Mod10 text, Mod100 text, Mod1000 text, Mod10000 text, Uniq text) distributed by (Mod10);

-- insert two long records: MPP-12079
insert into test_10m select 'SAME','0','0',i from repeat('ox',100000)i;
insert into test_10m select 'SAME','0','0',i from repeat('xo',100000)i;

-- insert 10M records
insert into test_10m select 'SAME', i%10, i%100, i%1000, i%10000, i from generate_series(1,10000000) i;

