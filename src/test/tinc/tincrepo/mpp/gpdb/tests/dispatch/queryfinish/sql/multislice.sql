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
inner join orders o1
	on l1.l_orderkey = o1.o_orderkey
where l1.l_suppkey = (
	select s_suppkey
	from skewed_lineitem l2
	inner join supplier on l2.l_suppkey = s_suppkey
	where s_nationkey = 11 and l2.l_returnflag = 'A'
	limit 1
)
limit -1;

select l1.l_partkey, l1.l_suppkey, l1.l_comment
from skewed_lineitem l1
inner join orders o1
	on l1.l_orderkey = o1.o_orderkey
where l1.l_suppkey in (
	select s_suppkey
	from skewed_lineitem l2
	inner join supplier on l2.l_suppkey = s_suppkey
	where s_nationkey = 11 and l2.l_returnflag = 'A'
	limit 2
)
limit -1;

select l1.l_partkey, l1.l_suppkey, l1.l_comment
from skewed_lineitem l1
inner join orders o1
	on l1.l_orderkey = o1.o_orderkey
where exists (
	select *
	from supplier
	where l1.l_suppkey = s_suppkey
	and s_nationkey = 11 and l1.l_returnflag = 'A'
)
limit -1;

-- cause hashjoin to spill
set statement_mem = '1MB';
select l1.l_partkey IS NOT NULL as part_is_not_null
from skewed_lineitem l1
inner join supplier s1 on l1.l_suppkey = s1.s_suppkey
limit 2;
reset statement_mem;
