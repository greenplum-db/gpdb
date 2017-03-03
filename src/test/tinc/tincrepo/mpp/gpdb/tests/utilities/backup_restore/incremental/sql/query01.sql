-- @gucs gp_create_table_random_default_distribution=off
\c gptest
-- Check all records of all tables are correctly restored
select count(*), sum(n_nationkey), min(n_nationkey), max(n_nationkey) from nation;
select count(*), sum(r_regionkey), min(r_regionkey), max(r_regionkey) from region;
select count(*), sum(p_partkey), min(p_partkey), max(p_partkey) from part;
select count(*), sum(s_suppkey), min(s_suppkey), max(s_suppkey) from supplier;
select count(*), sum(ps_partkey + ps_suppkey), min(ps_partkey + ps_suppkey), max(ps_partkey + ps_suppkey) from partsupp;
select count(*), sum(c_custkey), min(c_custkey), max(c_custkey) from customer;
select count(*), sum(o_orderkey), min(o_orderkey), max(o_orderkey) from orders;
select count(*), sum(l_linenumber), min(l_linenumber), max(l_linenumber) from lineitem;

-- Check indexes and trigger on customer and lineitem table
\d+ customer
\d+ lineitem

-- Check dumptesttbl
insert into dumptesttbl(zipcode,areacode) values('12345',972);
insert into dumptesttbl(zipcode) values('123456');
insert into dumptesttbl(areacode) values(-1);
select * from dumptesttbl;

-- Check gp_toolkit schema and its objects
-- MPP-13382
-- \dv gp_toolkit.
