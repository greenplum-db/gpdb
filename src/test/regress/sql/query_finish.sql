create schema qf;

-- Create and load common test tables.

CREATE TABLE qf.lineitem (
    l_orderkey bigint NOT NULL,
    l_partkey integer NOT NULL,
    l_suppkey integer NOT NULL,
    l_linenumber integer NOT NULL,
    l_quantity numeric(15,2) NOT NULL,
    l_extendedprice numeric(15,2) NOT NULL,
    l_discount numeric(15,2) NOT NULL,
    l_tax numeric(15,2) NOT NULL,
    l_returnflag character(1) NOT NULL,
    l_linestatus character(1) NOT NULL,
    l_shipdate date NOT NULL,
    l_commitdate date NOT NULL,
    l_receiptdate date NOT NULL,
    l_shipinstruct character(25) NOT NULL,
    l_shipmode character(10) NOT NULL,
    l_comment character varying(44) NOT NULL
)
DISTRIBUTED BY (l_orderkey);
\copy qf.lineitem ( L_ORDERKEY, L_PARTKEY, L_SUPPKEY,L_LINENUMBER,L_QUANTITY, L_EXTENDEDPRICE,L_DISCOUNT,L_TAX,L_RETURNFLAG,L_LINESTATUS,L_SHIPDATE,L_COMMITDATE,L_RECEIPTDATE,L_SHIPINSTRUCT,L_SHIPMODE,L_COMMENT) from 'data/lineitem_small.csv' with delimiter '|'; 

CREATE TABLE qf.orders (
    o_orderkey bigint NOT NULL,
    o_custkey integer NOT NULL,
    o_orderstatus character(1) NOT NULL,
    o_totalprice numeric(15,2) NOT NULL,
    o_orderdate date NOT NULL,
    o_orderpriority character(15) NOT NULL,
    o_clerk character(15) NOT NULL,
    o_shippriority integer NOT NULL,
    o_comment character varying(79) NOT NULL
)
DISTRIBUTED BY (o_orderkey);
\copy qf.orders ( O_ORDERKEY,O_CUSTKEY,O_ORDERSTATUS,O_TOTALPRICE,O_ORDERDATE,O_ORDERPRIORITY,O_CLERK,O_SHIPPRIORITY,O_COMMENT) from 'data/order_small.csv' with delimiter '|'; 

CREATE TABLE qf.supplier (
    s_suppkey integer NOT NULL,
    s_name character(25) NOT NULL,
    s_address character varying(40) NOT NULL,
    s_nationkey integer NOT NULL,
    s_phone character(15) NOT NULL,
    s_acctbal numeric(15,2) NOT NULL,
    s_comment character varying(101) NOT NULL
)
DISTRIBUTED BY (s_suppkey);
\copy qf.supplier (S_SUPPKEY,S_NAME,S_ADDRESS,S_NATIONKEY,S_PHONE,S_ACCTBAL,S_COMMENT) from 'data/supplier.csv' with delimiter '|';

create table skewed_lineitem as
select 1 AS l_skewkey, *
from qf.lineitem
distributed by (l_skewkey);

insert into skewed_lineitem
values(
    2,
    generate_series(1, 3), 42, 15, 23,
    0, 4000, 0.1, 0.3,
    NULL, NULL,
    '2001-01-01', '2012-12-01', NULL,
    'foobarfoobarbaz', '0937',
    'supercalifragilisticexpialidocious');


--
-- The actual tests.
--

-- It used to fail because query cancel is sent and QE goes out of
-- transaction block, though QD sends 2PC request.
begin;
drop table if exists qf.foo;
create table qf.foo as select i a, i b from generate_series(1, 100)i;
select case when gp_segment_id = 0 then pg_sleep(3) end from qf.foo limit 1;
commit;

-- with order by.  CTE can prevent LIMIT from being pushed down.
begin;
drop table if exists qf.bar2;
create table qf.bar2 as select i a, i b from generate_series(1, 100)i;
with t2 as(
	select * from skewed_lineitem
	order by l_orderkey
)
select * from t2 order by 1,2,3,4,5 limit 1;
commit;

-- with window function.
begin;
drop table if exists qf.bar3;
create table qf.bar3(a int, b text, c numeric) with (appendonly=true);
insert into qf.bar3 select a, repeat('x', 10) b, b from qf.bar2;
with t3 as(
	select
		l_skewkey,
		count(*) over (partition by l_skewkey order by l_quantity, l_orderkey)
	from skewed_lineitem
)
select * from t3 order by 1,2 limit 2;
commit;

-- combination.
begin;
drop table if exists qf.bar4;
create table qf.bar4(a int, b int, c text) with (appendonly=true, orientation=column);
insert into qf.bar4 select a, b, 'foo' from qf.bar2;
with t4a as(
	select
		l_skewkey,
		count(*) over (partition by l_skewkey order by l_quantity, l_orderkey)
	from skewed_lineitem
), t4b as (
	select * from skewed_lineitem
	order by l_orderkey
)
select a.l_skewkey, b.l_skewkey from t4a a
inner join t4b b on a.l_skewkey = b.l_skewkey
order by 1, 2
limit 3;
commit;

-- median.
begin;
drop table if exists qf.bar5;
create table qf.bar5(a int, b int);
insert into qf.bar5 select i, i % 10 from generate_series(1, 10)i;
with t5a as(
	select
		l_skewkey,
		median(l_quantity) med
	from skewed_lineitem
	group by l_skewkey
), t5b as (
	select * from skewed_lineitem
	order by l_orderkey
)
select a.l_skewkey, a.med from t5a a
inner join t5b b on a.l_skewkey = b.l_skewkey order by a.l_skewkey, a.med
limit 1;
commit;

--Combination median and windows

begin;

with t3 as(
    select
        l_skewkey,
        count(*) over (partition by l_skewkey order by l_quantity, l_orderkey)
    from skewed_lineitem
),

t4 as ( select
        l_skewkey,
        median(l_quantity) med
    from skewed_lineitem
    group by l_skewkey
)
select a.l_skewkey from t3 a  left outer join t4 b  on a. l_skewkey  = b. l_skewkey order by a.l_skewkey  limit 1;

commit;

--csq

begin;
select l_returnflag  from skewed_lineitem  t1 where l_skewkey in (select l_skewkey from  skewed_lineitem t2 where t1.l_shipinstruct = t2.l_shipinstruct) order by l_returnflag limit 3;
commit;
--
-- Exercise query-finish flag in sort.
--
with t6a as(
	select
		l_skewkey,
		count(*) over (partition by l_skewkey order by l_quantity, l_orderkey)
	from skewed_lineitem
), t6b as (
	select * from skewed_lineitem
	order by l_orderkey
), t6c as (
	select l_skewkey, median(l_quantity) med
	from skewed_lineitem
	group by l_skewkey
)
select a.l_skewkey, b.l_orderkey,
	c.med
from t6a a inner join t6b b on a.l_skewkey = b.l_skewkey
inner join t6c c on b.l_skewkey = c.l_skewkey
order by 1, 2, 3
limit 2;
--
-- Exercise query-finish vs motion/interconnect
--
with t7a as(
	select
		l_skewkey,
		l_orderkey
	from skewed_lineitem
), t7b as (
	select * from skewed_lineitem
)
select count(*) from(
select b.l_skewkey, b.l_orderkey
from t7a a inner join t7b b on a.l_orderkey = b.l_orderkey
limit 2
)s;

select l1.l_partkey, l1.l_suppkey, l1.l_comment
from skewed_lineitem l1
inner join qf.orders o1
	on l1.l_orderkey = o1.o_orderkey
where l1.l_suppkey = (
	select s_suppkey
	from skewed_lineitem l2
	inner join qf.supplier on l2.l_suppkey = s_suppkey
	where s_nationkey = 11 and l2.l_returnflag = 'A'
	limit 1
)
limit 0;

select l1.l_partkey, l1.l_suppkey, l1.l_comment
from skewed_lineitem l1
inner join qf.orders o1
	on l1.l_orderkey = o1.o_orderkey
where l1.l_suppkey in (
	select s_suppkey
	from skewed_lineitem l2
	inner join qf.supplier on l2.l_suppkey = s_suppkey
	where s_nationkey = 11 and l2.l_returnflag = 'A'
	limit 2
)
limit 0;

select l1.l_partkey, l1.l_suppkey, l1.l_comment
from skewed_lineitem l1
inner join qf.orders o1
	on l1.l_orderkey = o1.o_orderkey
where exists (
	select *
	from qf.supplier
	where l1.l_suppkey = s_suppkey
	and s_nationkey = 11 and l1.l_returnflag = 'A'
)
limit 0;

-- cause hashjoin to spill
set statement_mem = '1MB';
select l1.l_partkey IS NOT NULL as part_is_not_null
from skewed_lineitem l1
inner join qf.supplier s1 on l1.l_suppkey = s1.s_suppkey
limit 2;
reset statement_mem;
