\c tpch_heap
select count(*), sum(n_nationkey), min(n_nationkey), max(n_nationkey) from nation;
select count(*), sum(r_regionkey), min(r_regionkey), max(r_regionkey) from region;
select count(*), sum(p_partkey), min(p_partkey), max(p_partkey) from part;
select count(*), sum(s_suppkey), min(s_suppkey), max(s_suppkey) from supplier;
select count(*), sum(ps_partkey + ps_suppkey), min(ps_partkey + ps_suppkey), max(ps_partkey + ps_suppkey) from partsupp;
select count(*), sum(c_custkey), min(c_custkey), max(c_custkey) from customer;
select count(*), sum(o_orderkey), min(o_orderkey), max(o_orderkey) from orders;
select count(*), sum(l_linenumber), min(l_linenumber), max(l_linenumber) from lineitem;

\echo -- start_ignore
drop table if exists opperfscale;
create table opperfscale(nseg int, nscale int, nscaleperseg int); 

insert into opperfscale
 	select case when lc < 7000000 then 1 else seg end as nseg, 
	       case when lc < 7000000 then 1 else lc / 6000000 end as nscale,
	       case when lc < 7000000 or lc / 6000000 < seg 
	       		then 2 
			else lc / 6000000 / seg + 1 end as nscaleperseg
	from
	(
 		select max(content)+1 as seg from gp_configuration where isprimary = 't'
	) S,
	(
	 select count(*) as lc from lineitem 
	) L;

analyze opperfscale;
\echo -- end_ignore

select * from opperfscale;
-- scan test
-- Select one column from lineitem

select max(l_partkey) from lineitem;
-- Scan test2:
-- select several columns from lineitem
select max(length(l_linenumber::text) + length(l_linestatus) + length(l_comment))
from lineitem;

-- Sort test 1: 
select l_linenumber, l_tax from lineitem order by l_linenumber, l_tax
limit 100;

-- Sort test 2:
select count(*) from (select count(*) from
(
select l_linenumber, l_shipdate, l_linestatus  from lineitem
where l_orderkey % (
	select max(nscaleperseg) from opperfscale
	) = 0 
order by l_linenumber, l_shipdate, l_linestatus
limit 1000000
) tmpt) tmp;

-- Sort test 3: 

set gp_enable_sort_limit = off;
select count(*) from (select l_linenumber, l_shipdate, l_tax from lineitem order by l_linenumber, l_shipdate, l_tax
limit 100);
set gp_enable_sort_limit = on;

-- sort: string comp

select l_returnflag from lineitem order by l_returnflag, l_comment
limit 200;
-- Hashagg 1: yahoo

select count(*) from (select count(*) from (
select avg(l_quantity) as c1, max(l_discount) as c2 
from lineitem
group by 
l_orderkey, l_linenumber, l_linestatus, l_comment
) tmpt) tmp;

-- Hashagg 2: easy
select count(*) from (select count(*) from
(
select avg(l_quantity), max(l_discount) 
from lineitem
group by 
l_linenumber, l_linestatus, l_returnflag
) tmpt) tmp;
-- Sort agg 1: yahoo

set enable_hashagg = off;
select count(*) from (select count(*) from (
select avg(l_quantity), max(l_discount) 
from lineitem
where l_orderkey % (select max(nscaleperseg) from opperfscale) = 0 
group by 
l_orderkey, l_linenumber, l_linestatus, l_comment
) tmpt) tmp;
set enable_hashagg = on;
-- Sort agg 2: easy
set enable_hashagg = off;
select count(*) from (select count(*) from (
select avg(l_quantity), max(l_discount) 
from lineitem
where l_orderkey % (select max(nscaleperseg) from opperfscale) = 0
group by 
l_linenumber, l_linestatus, l_returnflag
) tmpt) tmp;
set enable_hashagg = on;
-- scan test with selection
-- Select one column from lineitem

select max(l_partkey) from lineitem where l_quantity > 20 and l_discount < 0.9 ;
-- Hash join

-- explain
select count(*) from lineitem l1, lineitem l2
where 
l1.l_orderkey % (select max(nscaleperseg) from opperfscale) = 0 and
l2.l_orderkey % (select max(nscaleperseg) from opperfscale) = 0 and
l1.l_partkey = l2.l_partkey and l1.l_returnflag = l2.l_returnflag
;

-- Hash join, lots of fields
-- explain 
select count(*) from lineitem l1, lineitem l2
where 
l1.l_orderkey % (
		select max(nscaleperseg) from opperfscale 
		) = 0 and
l2.l_orderkey % (
		select max(nscaleperseg) from opperfscale 
		) = 0 and
-- l1.l_orderkey = l2.l_orderkey and 
l1.l_partkey = l2.l_partkey
and l1.l_suppkey = l2.l_suppkey
and l1.l_linenumber = l2.l_linenumber
and l1.l_extendedprice = l2.l_extendedprice
and l1.l_returnflag = l2.l_returnflag
and l1.l_shipdate = l2.l_shipdate
and l1.l_commitdate = l2.l_commitdate
and l1.l_shipmode = l2.l_shipmode
and l1.l_comment = l2.l_comment
;

-- scan test
-- Merge join

set enable_hashjoin = off;
set enable_mergejoin=on;
-- explain
select count(*) from lineitem l1, lineitem l2
where 
l1.l_orderkey % (select max(nscaleperseg * 4) from opperfscale) = 0 and
l2.l_orderkey % (select max(nscaleperseg * 4) from opperfscale) = 0 and
l1.l_partkey = l2.l_partkey and l1.l_returnflag = l2.l_returnflag
;
set enable_hashjoin = on;
set enable_mergejoin=off;
-- Merge join, lots of fields
set enable_mergejoin=on;
set enable_hashjoin = off;

select count(*) from lineitem l1, lineitem l2
where 
l1.l_orderkey % (select max(nscaleperseg * 4) from opperfscale) = 0 and
l2.l_orderkey % (select max(nscaleperseg * 4) from opperfscale) = 0 and
l1.l_partkey = l2.l_partkey
and l1.l_suppkey = l2.l_suppkey
and l1.l_linenumber = l2.l_linenumber
and l1.l_extendedprice = l2.l_extendedprice
and l1.l_returnflag = l2.l_returnflag
and l1.l_shipdate = l2.l_shipdate
and l1.l_commitdate = l2.l_commitdate
and l1.l_shipmode = l2.l_shipmode
and l1.l_comment = l2.l_comment
;
set enable_mergejoin=off;
set enable_hashjoin = on;
-- Nested loop join
-- explain
select count(*) from part p1, part p2
where 
p1.p_partkey % (select max(nscale * 10) from opperfscale) = 0 and 
p2.p_partkey % (select max(nscale * 10) from opperfscale) = 0 and 
p1.p_size < p2.p_size 
and p1.p_retailprice > p2.p_retailprice
and p1.p_brand > p2.p_brand
;

-- Left outer join, with hash join
-- explain 
select count(*) from lineitem l left outer join partsupp p
on l.l_partkey = p.ps_partkey and l.l_suppkey = p.ps_suppkey
and l.l_quantity > (p.ps_availqty / 10)
where l.l_orderkey % (select max(nscaleperseg) from opperfscale) = 0
;

-- Full outer join, merge join
set enable_hashjoin = off;

-- explain 
select count(*) from (select count(*) from lineitem l left outer join partsupp p
on l.l_partkey = p.ps_partkey and l.l_suppkey = p.ps_suppkey
and l.l_quantity > (p.ps_availqty / 10)
where l.l_orderkey % (select max(nscaleperseg) from opperfscale) = 0) tmp
;

set enable_hashjoin = on;
-- nested loop outer join
-- explain analyze
select count(*) from part p left outer join supplier s 
on p.p_partkey > s.s_suppkey and p.p_size < s.s_nationkey
where
p.p_partkey % (select max(nscale * 4) from opperfscale) = 1
and s.s_suppkey % (select max(nscale * 4) from opperfscale) = 1
;
-- Full outer join, with hash join
-- explain 
-- select count(*) from lineitem l full outer join partsupp p
-- on l.l_partkey = p.ps_partkey and l.l_suppkey = p.ps_suppkey
-- ;
select 'Not supported';
-- Full outer join, merge join
-- set enable_hashjoin = off;

-- explain 
select count(*) from (select count(*) from lineitem l full outer join partsupp p
on l.l_partkey = p.ps_partkey and l.l_suppkey = p.ps_suppkey) tmp
-- and l.l_quantity > (p.ps_availqty / 10)
;

-- set enable_hashjoin = on;
-- nested loop outer join
-- explain
-- select count(*) from part p full outer join supplier s
-- on p.p_partkey > s.s_suppkey and p.p_size < s.s_nationkey
-- where p.p_partkey % 7 = 1 and s.s_suppkey % 11 = 1
-- ;

select 'Not supported';

-- distinct
select count(*) from (select count(*) from
(
select distinct l_partkey, l_suppkey, l_shipmode from lineitem 
where l_orderkey % (select max(nscaleperseg) from opperfscale) = 0 
) tmpt) tmp;

-- distinct using group by
select count(*) from 
(select count(1) from lineitem
 where l_orderkey % (select max(nscaleperseg) from opperfscale) = 0
 group by l_partkey, l_suppkey, l_shipmode
) tmpt;

-- count distinct
select count(distinct l_partkey), count(distinct l_suppkey), count(distinct l_shipmode) 
	from lineitem 
where l_orderkey % (select max(nscale * 4) from opperfscale) = 0;
-- Union

select count(*) from (select count(*) from
(
  select l_suppkey, l_partkey, l_shipmode from lineitem where
  l_orderkey % (select max(nscaleperseg) * 20 from opperfscale) = 1
  union

  select l_suppkey, l_partkey, l_shipmode from lineitem where
  l_orderkey % (select max(nscaleperseg) * 20 from opperfscale) = 2
  union

  select l_suppkey, l_partkey, l_shipmode from lineitem where
  l_orderkey % (select max(nscaleperseg) * 20 from opperfscale) = 3
  union

  select l_suppkey, l_partkey, l_shipmode from lineitem  where
  l_orderkey % (select max(nscaleperseg) * 20 from opperfscale) = 4
  union

  select l_suppkey, l_partkey, l_shipmode from lineitem where
  l_orderkey % (select max(nscaleperseg) * 20 from opperfscale) = 5
) tmpt) tmp;
-- Union all

select count(*) from
(
  select l_suppkey, l_partkey, l_shipmode from lineitem where
  l_orderkey % (select max(nscaleperseg * 10) from opperfscale) = 1
  union all
  select l_suppkey, l_partkey, l_shipmode from lineitem where
  l_orderkey % (select max(nscaleperseg * 10) from opperfscale) = 2
  union all
  select l_suppkey, l_partkey, l_shipmode from lineitem where
  l_orderkey % (select max(nscaleperseg * 10) from opperfscale) = 3
  union all
  select l_suppkey, l_partkey, l_shipmode from lineitem where
  l_orderkey % (select max(nscaleperseg * 10) from opperfscale) = 4
  union all
  select l_suppkey, l_partkey, l_shipmode from lineitem where
  l_orderkey % (select max(nscaleperseg * 10) from opperfscale) = 5
) tmpt;
-- select into

select * into tmplineitem from lineitem
where l_orderkey % 20 = 1;
-- update

update tmplineitem set l_linestatus = 'O'
where l_suppkey % 3 = 1;
-- delete

delete from tmplineitem 
where l_partkey % 4 = 1;
-- clean up

drop table tmplineitem;
-- stats functions, min, max, stddev and variance

select min(l_quantity), max(l_tax), stddev(l_tax), variance(l_tax) 
	from lineitem;
-- copy tests

copy customer to '/dev/null';
