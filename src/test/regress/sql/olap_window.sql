--
-- Exercise outfuncs
--

set Debug_print_parse=on;
set Debug_print_plan=on;

--
-- OLAP_WINDOW 
--
-- Changes here should also be made to olap_window_seq.sql

---- 1 -- Null window specification -- OVER () ----

select row_number() over (), cn,pn,vn 
from sale; -- mvd 1->1

select rank() over (), * 
from sale; --error - order by req'd

select dense_rank() over (), * 
from sale; --error - order by order by req'd


-- 2 -- Plan trivial windows over various input plan types ----

select row_number() over (), pn, cn, vn, dt, qty, prc
from sale; -- mvd 1->1

select row_number() over (), s.pn, s.cn, s.vn, s.dt, s.qty, s.prc, c.cname, c.cloc, p.pname, p.pcolor
from sale s, customer c, product p 
where s.cn = c.cn and s.pn = p.pn; -- mvd 1->1

select row_number() over (), vn, count(*), sum(qty*prc) as amt
from sale group by vn; -- mvd 1->1

select row_number() over (), name, loc
from (select vname, vloc from vendor union select cname, cloc from customer) r(name,loc); -- mvd 1->1

select row_number() over (), s 
from generate_series(1,3) r(s); -- mvd 1->1

select row_number() over (), u, v
from ( values (1,2),(3,4),(5,6) ) r(u,v); -- mvd 1->1


---- 3 -- Exercise WINDOW Clause ----

select row_number() over (w), * from sale window w as () -- mvd 1->1
order by 1 desc; -- order 1

select row_number() over (w), * from sale
order by 1 desc
window w as (); --error - window can't follow order by

select  count(*) over (w2) from customer
window
    w1 as (),
    w2 as (w1 partition by cn); --error - can't partition if referencing ordering window

select count(*) over (w2) from customer
window
    w1 as (partition by cloc order by cname),
    w2 as (w1 order by cn); --error - can't reorder the ordering window

select  count(*) over (w2) from customer
window 
    w1 as (order by cn rows unbounded preceding),
    w2 as (w1); --error - ordering window can't specify framing

select * from sale
window 
    w as (partition by pn),
    wa as (w order by cn),
    wb as (w order by vn),
    waf as (wa rows between 2 preceding and 3 preceding),
    wbf as (wb range between 2 preceding and 3 preceding); --mvd 3->1
    
select * from customer
window 
    w as (partition by cn), 
    w as (w order by cname); --error - can't refer to window in its definition

select * from customer
window w as (w order by cname); --error - can't refer to window in its definition

-- test the frame parser
select cn, 
  count(*) over (order by cn range '1'::interval preceding) 
  from customer; -- error, type mismatch

select cn,
  count(*) over (order by cn range between '-11' preceding and '-2' following)
  from customer; -- error, negative values not permitted

select cn,
  sum(cn) over (order by cn range NULL preceding)
  from customer; -- we don't permit NULL

select cn,
  sum(cn) over (order by cn range '1'::float8 preceding)
  from customer; -- this, however, should work

---- 4 -- Partitioned, non-ordered window specifications -- OVER (PARTTION BY ...) ----

-- !
select row_number() over (partition by cn), cn, pn 
from sale; -- mvd 2->1

select row_number() over (partition by pn, cn), cn, pn 
from sale; -- mvd 2,3->1

select rank() over (partition by cn), cn, pn 
from sale; --error - rank requires ordering

select dense_rank() over (partition by cn), cn, pn 
from sale; --error - dense_rank requires ordering

select row_number() over (partition by pname, cname), pname, cname
from sale s, customer c, product p 
where s.cn = c.cn and s.pn = p.pn; -- mvd 2,3->1

select row_number() over (partition by vn), vn, count(*)
from sale 
group by vn; -- mvd 2->1

select row_number() over (partition by count(*)), vn, count(*)
from sale 
group by vn; -- mvd 3->1



---- 5 -- Ordered, non-partitioned window specifications -- OVER (ORDER BY ...) ----

select row_number() over (order by cn), cn, pn 
from sale; -- mvd 2->1

select row_number() over (order by pn desc), cn, pn 
from sale; -- mvd 3->1

select row_number() over (order by pn, cn desc), cn, pn 
from sale; -- mvd 2,3->1

select rank() over (order by cn), cn, pn 
from sale; -- mvd 1->-- mvd

select rank() over (order by pn desc), cn, pn 
from sale; -- mvd 1->2,3

select rank() over (order by pn, cn desc), cn, pn 
from sale; -- mvd 1->2,3

select dense_rank() over (order by cn), cn, pn 
from sale; -- mvd 1->2,3

select dense_rank() over (order by pn desc), cn, pn 
from sale; -- mvd 1->2,3

select dense_rank() over (order by pn, cn desc), cn, pn 
from sale; -- mvd 1->2,3

select rank() over (w), cn, pn 
from sale 
window w as (order by cn); -- mvd 1->2,3

select rank() over (w), cn, pn 
from sale 
window w as (order by pn desc); -- mvd 1->2,3

select rank() over (w), cn, pn 
from sale 
window w as (order by pn, cn desc); -- mvd 1->2,3

select row_number() over (order by pname, cname)
from sale s, customer c, product p 
where s.cn = c.cn and s.pn = p.pn; -- mvd 1->1

select row_number() over (order by vn), vn, count(*)
from sale 
group by vn; -- mvd 2->1

select row_number() over (order by count(*)), vn, count(*)
from sale 
group by vn; -- mvd 3->1

-- Test NULLS FIRST/LAST in window clauses
create table tbl_with_nulls (t text, a int, b int);
insert into tbl_with_nulls values
  ( 'a', 1, 10),
  ( 'b', 1, 10),
  ( 'c', 1, 10),
  ( 'd', 2, 10),
  ( 'e', 2, 20),
  ( 'f', 2, 20),
  ( 'g', NULL, 20),
  ( 'h', NULL, 20),
  ( 'i', NULL, 30);

select t, a, b,
  first_value(t) over (order by a nulls first, t),
  first_value(t) over (order by a nulls last, t),
  first_value(t) over (partition by b order by a nulls first, t),
  first_value(t) over (partition by b order by a nulls last, t)
from tbl_with_nulls order by t;

-- Same with explicitly named window
select t, a, b,
  first_value(t) over (w1),
  first_value(t) over (w2),
  first_value(t) over (w3),
  first_value(t) over (w4)
from tbl_with_nulls
window w1 as (order by a nulls first, t),
       w2 as (order by a nulls last, t),
       w3 as (partition by b order by a nulls first, t),
       w4 as (partition by b order by a nulls last, t)
order by t;

---- 6 -- Exclude clause ----
-- MPP-13628: exclude clause doesn't work
select vn, sum(vn) over (w)
from sale
window w as (order by vn rows between unbounded preceding and unbounded following exclude CURRENT ROW);

select vn, sum(vn) over (w)
from sale
window w as (order by vn rows between unbounded preceding and unbounded following exclude GROUP);

select vn, sum(vn) over (w)
from sale
window w as (order by vn rows between unbounded preceding and unbounded following exclude TIES);

select vn, sum(vn) over (w)
from sale
window w as (order by vn rows between unbounded preceding and unbounded following exclude NO OTHERS);

-- MPP-14244: add guc to ignore exclude clause so the above won't error.
-- Note: produces the wrong answer, we will probably want to remove this once we fix the MPP-13628
set gp_ignore_window_exclude = true;
select vn, sum(vn) over (w)
from sale
window w as (order by vn rows between unbounded preceding and unbounded following exclude GROUP);


---- X -- Miscellaneous (e.g. old bugs, etc.) ----

-- Why was this here? We can't guarantee all correct answers will pass!
--select 
--    cn*100 + row_number() over (), 
--    cname, 
--    row_number() over () - 1, 
--    cn 
--from customer; -- mvd 4->1,3

-- !
select row_number() over (), a,b 
from (
    select cn, count(*) 
    from sale 
    group by cn ) r(a,b); -- mvd 1->1

select * 
from (
    select row_number() over () 
    from customer ) r(a), 
    (
    select row_number() over () 
    from sale ) s(a) 
where s.a = r.a;

select 
    row_number() over (w), 
    rank() over (w), 
    dense_rank() over (w), 
    * 
from sale 
window w as (order by cn); -- mvd 4->1; 4->2; 4->3

select cn, row_number() over (w), count(*) over (w), rank() over (w), dense_rank() over (w), vn 
from sale 
window w as (partition by cn order by vn); -- mvd 1,4->2

select cn,vn, rank() over (order by cn), rank() over (order by cn,vn) 
from sale;

-- !
select cn,vn,pn, 
  row_number() over (partition by cn), 
  rank() over (partition by cn order by vn), 
  rank() over (partition by cn order by vn,pn) 
from sale; -- mvd 1->4

select cn,vn, row_number() over (partition by cn,vn),
  cn,vn, rank() over (partition by cn,vn order by vn),
  cn,vn,pn, rank() over (partition by cn,vn order by vn,pn)
from sale; -- mvd 1,2->3

select 
  dense_rank() over (order by pname, cname), cname, pname
from sale s, customer c, product p 
where s.cn = c.cn and s.pn = p.pn;

select pn, cn, prc*qty, 
    row_number() over (partition by pn),
    avg(prc*qty) over (partition by pn),
    avg(prc*qty) over (partition by pn order by cn),
    percent_rank() over (partition by pn order by cn),
    rank() over (partition by pn order by cn)
from sale; -- mvd 1->4

select pn, row_number() over (partition by pn), avg(pn) over (partition by pn) 
from sale; -- mvd 1->2

select pn, row_number() over (partition by pn), avg(pn) over (partition by pn) 
from sale order by pn; -- mvd 1->2

select cn, row_number() over (partition by cn), avg(cn) over (partition by cn) 
from sale; -- mvd 1->2

select cn, row_number() over (partition by cn) 
from sale; -- mvd 1->2

-- !
select cn, vn, pn, 
    row_number() over (partition by cn), 
    row_number() over (partition by vn), 
    row_number() over (partition by pn)
from sale; -- mvd 1->4; 2->5; 3->6

select cn, vn, pn, avg(qty) over (partition by cn) 
from sale; --mvd 1->4

select cn, vn, pn, avg(qty) over (partition by vn) 
from sale; --mvd 2->4

-- !
select avg(qty) over (partition by cn) 
from sale;

select ntile(3) over (order by cn) 
from sale;

select dt, ntile(5) over (order by dt) 
from sale;

-- !
select cn, dt, ntile(3) over (partition by cn order by dt) 
from sale; -- mvd 1,2->3

-- !
select cn, dt, 
    ntile(3) over (partition by cn order by dt), 
    sum(prc) over (order by cn, dt) 
from sale; -- mvd 1,2->3

-- !
select cn, dt, 
    percent_rank() over (partition by cn order by dt), 
    sum(prc) over (order by cn, dt) 
from sale; -- mvd 1,2->3; 1,2->4

select                                                         
  grouping(cn, vn, pn) as gr, cn, vn, pn,
  sum(qty*prc), rank() over (partition by cn order by sum(qty*prc))
from sale
group by rollup(cn,vn,pn) 
order by 2 desc, 5; -- order 2,5

select                                                         
  grouping(cn, vn, pn) as gr, cn, vn, pn,
  sum(qty*prc), rank() over (partition by cn order by sum(qty*prc))
from sale
group by rollup(cn,vn,pn) 
order by 2, 5; -- order 2,5

select cn, vn, pn, rank() over (partition by vn order by cn) 
from sale; --mvd 2->4

select row_number() over (partition by vn), 
    cn, vn, pn, 
    rank() over (partition by vn order by cn) 
from sale; -- mvd 3->1

select row_number() over (partition by pn), 
    cn, vn, pn, 
    rank() over (partition by vn order by cn) 
from sale; -- mvd 4->1

select cname, 
	   rank() over (partition by sale.cn order by pn),
	   rank() over (partition by sale.cn order by vn)
from sale, customer
where sale.cn = customer.cn; -- mvd 1->2,3

-- Check that we invert count() correctly. count() is a special case
-- because when not invoked as count(*), it uses "any" as an argument.
-- This can trip us up.
SELECT sale.pn, COUNT(sale.pn) OVER(order by sale.pn)              
FROM sale;

-- Reject DISTINCT qualified aggs when an ORDER BY is present
SELECT count(distinct sale.pn) OVER (order by sale.pn) from sale;

-- Aggregate nesting --

create table olap_tmp_for_window(g integer, h integer, i integer, x integer);

insert into olap_tmp_for_window values 
    (9,1,1,1),
    (9,1,1,0),
    (9,1,1,1),
    (9,1,1,0),
    (9,1,2,1),
    (9,1,2,0),
    (9,1,2,1),
    (9,1,2,0),
    (9,4,1,1),
    (9,4,1,0),
    (9,4,1,1),
    (9,4,1,0),
    (9,4,2,1),
    (9,4,2,0),
    (9,4,2,1),
    (9,4,2,0),
    (9,1,1,1),
    (9,1,1,0),
    (9,1,1,1),
    (9,1,1,0),
    (9,1,2,1),
    (9,1,2,0),
    (9,1,2,1),
    (9,1,2,0),
    (9,4,1,1),
    (9,4,1,0),
    (9,4,1,1),
    (9,4,1,0),
    (9,4,2,1),
    (9,4,2,0),
    (9,4,2,1),
    (9,4,2,0);


select g,h,i,avg(x) as ax
from olap_tmp_for_window
group by g,h,i;

-- begin equivalent

select 
  g, 
  ax, 
  avg(g) over (partition by h order by i), 
  sum(ax) over (partition by i order by g)
from (select g,h,i,avg(x) as ax from olap_tmp_for_window group by g,h,i) rr;

select
  g,
  avg(x),
  avg(g) over (partition by h order by i),
  sum(avg(x)) over (partition by i order by g)
from olap_tmp_for_window
group by g,h,i;

-- end equivalent

drop table olap_tmp_for_window;

-- begin equivalent

select g, cn, vn, pn, s, rank() over (partition by g order by s)
from
  (select
    grouping(cn, vn, pn),
    cn, vn, pn,
    sum(qty*prc)
  from sale
  group by rollup(cn,vn,pn)) olap_tmp_for_window(g,cn,vn,pn,s)
order by 1,5; -- order 1,5

select
  grouping(cn, vn, pn),
  cn, vn, pn,
  sum(qty*prc),
  rank() over (partition by grouping(cn,vn,pn) order by sum(qty*prc))
from sale
group by rollup(cn,vn,pn)
order by 1,5; -- order 1,5

-- end equivalent

-- basic framed query test
select pn, count(*) 
over (order by pn range between 1 preceding and 1 following) as c
from sale
order by pn;

-- Some data types, like date, when mixed with operators like "-" result in
-- data returned in a different data type. We're smart enough to detect
-- this but some tests are good.

select cn, dt, qty,
	sum(qty) over (order by dt 
       range between '1 year'::interval preceding and 
                     '1 month'::interval following)
from sale; --mvd 2->4

select cn, dt, qty, prc,
	sum(qty) over (order by prc range '314.15926535'::float8 preceding) as sum
from sale; --mvd 4->5

-- There are some types we cannot cast back, test for those too
select cn, dt, qty,
	sum(qty) over (order by dt
       range '2007-08-05'::date preceding) as sum
from sale; --mvd 2->4

select cn, dt, qty,
	sum(qty) over (order by dt
       range '192.168.1.1'::inet preceding) as sum
from sale; --mvd 2->4

-- Test for FOLLOWING frames
select cn, prc, dt, sum(prc) over (order by ord,dt,cn rows between 2 following and 3 following) as f from sale_ord;

-- Check that mixing cume_dist() with other window functions on the same
-- window does not result in badness

select cn, rank() over (w), cume_dist() over (w) from
  customer
  window w as (order by cname);

-- Test for MPP-1762

-- begin equivalent

SELECT sale.prc,sale.cn, sale.cn, 
AVG(sale.pn) OVER(order by sale.pn desc,sale.vn asc,sale.cn desc rows between unbounded preceding and unbounded following) as avg,
sale.vn,sale.pn,
DENSE_RANK() OVER(order by sale.pn asc) FROM sale,vendor WHERE sale.vn =vendor.vn;

SELECT sale.prc,sale.cn,sale.cn, AVG(sale.pn) OVER(win1),
sale.vn,sale.pn,
DENSE_RANK() OVER(win2)
FROM sale,vendor                                                       
WHERE sale.vn=vendor.vn
WINDOW win1 as (order by sale.pn desc,sale.vn asc,sale.cn desc rows between unbounded preceding and unbounded following ),
win2 as (order by sale.pn asc);

-- end equivalent

-- Test for MPP-1756, the planner should create just the one key level

explain select cn,
sum(qty) over (order by ord,cn rows between 1 preceding and 1 following),
sum(qty) over (order by ord,cn rows between 1 preceding and 1 following)
from sale_ord;

select cn,
sum(qty) over (order by ord,cn rows between 1 preceding and 1 following),
sum(qty) over (order by ord,cn rows between 1 preceding and 1 following)
from sale_ord;

-- test for MPP-1760. Framed queries without order by clauses are not
-- permitted.

select cn,
sum(count(*)) over (range between 1 preceding and 1 following)
from sale
group by cn;

-- test for issue which reopened MPP-1762
-- We allow the user to specify DESC sort order
SELECT sale.cn,sale.vn,AVG(cast (sale.vn as int)) OVER(order by ord desc,sale.cn desc) as avg from sale_ord as sale;

-- a bit harder
SELECT sale.cn,sale.dt, sale.vn,AVG(cast (sale.vn as int)) OVER(order by sale.cn desc, sale.dt asc) as avg from sale;--mvd 1,2->4

-- Even harder (MPP-1805)
SELECT sale.cn,sale.prc,sale.qty,                                  
SUM(floor(sale.prc*sale.qty)) 
OVER(order by sale.cn desc range between 4  preceding and 4 following) as foo 
FROM sale; --mvd 1->4

SELECT sale.pn,sale.vn, 
SUM(cast (sale.vn as int)) 
OVER(order by sale.cn desc range current row) as sum,
sale.cn from sale; --mvd 4->3

-- test for MPP-1810 and other similar queries
SELECT sale.vn,sale.vn,sale.qty,                                   
FIRST_VALUE(floor(sale.vn)) OVER(order by sale.vn asc range 0 preceding) as f
from sale; --mvd 1->4

select cn, cast(cname as varchar(10)), first_value(cast(cname as varchar(10))) 
  over(order by cn range 2 preceding) as f
from customer;

select cn, prc, dt, first_value(prc) over (order by ord,dt rows between 1 following
and 4 following) as f from sale_ord;

-- test for second issue in MPP-1810
select vn, first_value(vn) over(order by vn range 2 preceding) from vendor;

-- MPP-1819
SELECT sale.cn,sale.pn, 
FIRST_VALUE(sale.pn+sale.pn) OVER(order by ord,sale.cn rows unbounded preceding) as fv from sale_ord as sale;

SELECT sale.cn,sale.pn, 
FIRST_VALUE(((cn*100 + (sale.pn+sale.pn)%100)/100)) OVER(order by sale.cn range unbounded preceding) as fv from sale; --mvd 1->3

-- MPP-1812
SELECT sale.cn,sale.prc,sale.vn,
SUM(floor(sale.prc*sale.vn)) OVER(order by sale.cn asc,sale.prc range current row) as fv 
from sale; --mvd 1,2->4

-- MPP-1843, check the interaction between ROWS frames and partitioning
SELECT sale.dt,sale.prc,sale.cn,
sale.vn,
SUM(sale.cn) OVER(partition by sale.dt,sale.prc order by sale.cn asc rows 
between 0 following and 1 following) as sum 
from sale order by dt, prc, cn; --mvd 1,2->5

-- MPP-1804, Used to return incorrect number of rows
SELECT sale.vn,sale.cn,
SUM(sale.cn) OVER(partition by sale.vn order by sale.cn desc range between current row and unbounded following) as sum from sale; --mvd 1->3

-- MPP-1840, grouping + windowing
--begin_equivalent
SELECT cn, vn, pn, GROUPING(cn,vn,pn),
SUM(vn) OVER (PARTITION BY GROUPING(cn,vn,pn) ORDER BY cn) as sum FROM sale
GROUP BY ROLLUP(cn,vn,pn) order by 4; -- order 4

select cn,vn,pn,grouping,
sum(vn) over (partition by grouping order by cn) as sum 
from (select cn,vn,pn,grouping(cn,vn,pn) from sale group by rollup(cn,vn,pn)) t
order by 4; -- order 4
--end_equivalent

-- MPP-1860
SELECT sale.pn,sale.vn,sale.qty,
SUM(floor(sale.vn*sale.qty)) OVER(order by ord,sale.pn asc rows between 4 preceding and 0 preceding)  as sum 
FROM sale_ord as sale;

-- MPP-1866
SELECT sale.cn,sale.pn,sale.prc, 
SUM(floor(sale.pn*sale.prc)) OVER(order by sale.cn asc, pn rows between current row and unbounded following ) as sum 
FROM sale;

SELECT sale.pn,sale.vn, 
COUNT(floor(sale.vn)) OVER(order by ord,sale.pn asc rows between current row and unbounded following) 
FROM sale_ord as sale;    

-- Sanity test for executor detection of non-sense frames
SELECT sale.pn,sale.cn,sale.prc,
    AVG(cast (sale.prc as int))
      OVER(order by sale.cn asc range between 6 following and 0 following ) as c
      FROM sale; --mvd 2->4

SELECT sale.pn,sale.cn,sale.prc,
    AVG(cast (sale.prc as int))
      OVER(order by ord,sale.cn asc rows between 6 following and 0 following ) as c
      FROM sale_ord as sale;

SELECT sale.pn,sale.cn,sale.prc,
    AVG(cast (sale.prc as int))
      OVER(order by sale.cn asc range between 1 preceding and 10 preceding ) as c
      FROM sale; --mvd 2->4

-- Check LEAD()
-- sanity tests
select p.proname, p.proargtypes from pg_window w, pg_proc p, pg_proc p2 where w.winfunc = p.oid and w.winfnoid = p2.oid and p2.proname = 'lead' order by 1,2;

select lead(cn) from sale;

select cn, cname, lead(cname, -1) over (order by cn) from customer;

-- actual LEAD tests
select cn, cname, lead(cname, 2, 'undefined') over (order by cn) from customer;

select cn, cname, lead(cname, 2) over (order by cn) from customer;

select cn, cname, lead(cname) over (order by cn) from customer;

select cn, vn, pn, lead(cn, 1, cn + 1) over (order by cn, vn, pn) from 
sale order by 1, 2, 3;

select cn, vn, pn, qty * prc, 
	lead(qty * prc) over (order by cn, vn, pn) from sale
order by 1, 2, 3;

-- Check LAG()
-- sanity tests
select p.proname, p.proargtypes from pg_window w, pg_proc p, pg_proc p2 where w.winfunc = p.oid and w.winfnoid = p2.oid and p2.proname = 'lag' order by 1,2;

-- actual LAG tests
select cn, cname, lag(cname, 2, 'undefined') over (order by cn) from customer;

select cn, cname, lag(cname, 2) over (order by cn) from customer;

select cn, cname, lag(cname) over (order by cn) from customer;

select cn, vn, pn, lag(cn, 1, cn + 1) over (order by cn, vn, pn) from 
sale order by 1, 2, 3;

select cn, vn, pn, qty * prc, 
	lag(qty * prc) over (order by cn, vn, pn) from sale
order by 1, 2, 3;

-- LAST_VALUE tests
SELECT sale.cn,sale.qty,sale.pn, 
LAST_VALUE(sale.qty*sale.pn) OVER(partition by sale.cn order by sale.cn,pn range between unbounded preceding and unbounded following ) as lv 
from sale order by 1, 2, 3; --mvd 1->4

SELECT sale.vn,sale.qty,                                   
LAST_VALUE(floor(sale.vn)) OVER(order by sale.vn asc range 0 preceding) as f
from sale; --mvd 1->3

select cn, cast(cname as varchar(10)), last_value(cast(cname as varchar(10))) 
  over(order by cn range 2 preceding) as f
from customer;

select cn, prc, dt, 
last_value(prc) over (order by dt, prc, cn rows between 1 following and 4 following) as f from sale;

select vn, last_value(vn) 
over(order by vn range between 2 preceding and 2 following) from vendor;

select vn, last_value(vn) 
over(order by vn range between 20 preceding and 20 following) from vendor;

SELECT sale.cn,sale.pn, 
last_VALUE(sale.pn+sale.pn) 
OVER(order by ord,sale.cn rows between current row and 
unbounded following) as fv from sale_ord as sale;

SELECT sale.cn,sale.pn, 
last_VALUE(((cn*100 + (sale.pn+sale.pn)/100)%100)) OVER(order by sale.cn, sale.pn range between current row and
unbounded following) as fv from sale; --mvd 1,2->3

-- Test obscure types
create table typetest_for_window (i int[], j box, k bit, l bytea, m text[]);
insert into typetest_for_window values('{1, 2, 3}', '(1, 2), (3, 4)', '1', E'\012\001', 
'{foo, bar}');
insert into typetest_for_window values('{4, 5, 6}', '(3, 4), (6, 7)', '0', E'\002', 
'{bar, baz}');

select i, lead(i, 1, '{1}') over (w), 
lead(j, 1, '(0, 0), (1, 1)') over (w), lead(k, 1) over(w),
lead(l, 1, E'\015') over (w), lead(m, 1, '{test}') over (w)
from typetest_for_window window w as (order by i);

select i, lag(i, 1, '{1}') over (w), 
lag(j, 1, '(0, 0), (1, 1)') over (w), lag(k, 1) over(w),
lag(l, 1, E'\015') over (w), lag(m, 1, '{test}') over (w)
from typetest_for_window window w as (order by i);

select i, first_value(i) over (w), 
first_value(j) over (w), first_value(k) over(w),
first_value(l) over (w), first_value(m) over (w)
from typetest_for_window window w as (order by i rows 1 preceding);

select i, last_value(i) over (w), 
last_value(j) over (w), last_value(k) over(w),
last_value(l) over (w), last_value(m) over (w)
from typetest_for_window window w as (order by i rows between current row and 1 following);

drop table typetest_for_window;

-- MPP-1881
SELECT sale.pn,
COUNT(pn) OVER(order by sale.pn asc range between unbounded preceding and unbounded following) 
FROM sale;

-- MPP-1878
SELECT sale.vn,sale.cn, COUNT(vn) OVER(order by sale.cn asc range between unbounded preceding and 2 following)
FROM sale; --mvd 2->3

-- MPP-1877
SELECT sale.vn,sale.cn, 
COUNT(vn) OVER(order by sale.cn asc range between unbounded preceding and 2 preceding) 
FROM sale; --mvd 2->3
SELECT sale.vn,sale.cn, 
COUNT(vn) OVER(order by ord,sale.cn asc rows between unbounded preceding and 2 preceding) 
FROM sale_ord as sale; 

-- Framing clauses
select cn,count(*) over (order by cn rows between 2 preceding and 1 preceding) as c from sale;
select cn,count(*) over (order by cn rows between 2 preceding and 0 preceding) as c from sale;
select cn,count(*) over (order by cn rows between 2 preceding and 1 following) as c from sale;
select cn,count(*) over (order by cn rows between 0 preceding and 1 following) as c from sale;
select cn,count(*) over (order by cn rows between 0 following and 1 following) as c from sale;
select cn,count(*) over (order by cn rows between 1 following and 2 following) as c from sale;
select cn,count(*) over (order by cn rows between unbounded preceding and 2 preceding) as c from sale;
select cn,count(*) over (order by cn rows between unbounded preceding and 0 preceding) as c from sale;
select cn,count(*) over (order by cn rows between unbounded preceding and 2 following) as c from sale;
select cn,count(*) over (order by cn rows between 2 preceding and unbounded following) as c from sale;
select cn,count(*) over (order by cn rows between 0 preceding and unbounded following) as c from sale;
select cn,count(*) over (order by cn rows between 0 following and unbounded following) as c from sale;
select cn,count(*) over (order by cn rows between 1 following and unbounded following) as c from sale;
select cn,count(*) over (order by cn rows between unbounded preceding and unbounded following) as c from sale;

select cn,count(*) over (order by cn desc rows between 2 preceding and 1 preceding) as c from sale;
select cn,count(*) over (order by cn desc rows between 2 preceding and 0 preceding) as c from sale;
select cn,count(*) over (order by cn desc rows between 2 preceding and 1 following) as c from sale;
select cn,count(*) over (order by cn desc rows between 0 preceding and 1 following) as c from sale;
select cn,count(*) over (order by cn desc rows between 0 following and 1 following) as c from sale;
select cn,count(*) over (order by cn desc rows between 1 following and 2 following) as c from sale;
select cn,count(*) over (order by cn desc rows between unbounded preceding and 2 preceding) as c from sale;
select cn,count(*) over (order by cn desc rows between unbounded preceding and 0 preceding) as c from sale;
select cn,count(*) over (order by cn desc rows between unbounded preceding and 2 following) as c from sale;
select cn,count(*) over (order by cn desc rows between 2 preceding and unbounded following) as c from sale;
select cn,count(*) over (order by cn desc rows between 0 preceding and unbounded following) as c from sale;
select cn,count(*) over (order by cn desc rows between 0 following and unbounded following) as c from sale;
select cn,count(*) over (order by cn desc rows between 1 following and unbounded following) as c from sale;
select cn,count(*) over (order by cn desc rows between unbounded preceding and unbounded following) as c from sale;

select cn,count(*) over (order by cn range between 2 preceding and 1 preceding) as c from sale;
select cn,count(*) over (order by cn range between 2 preceding and 0 preceding) as c from sale;
select cn,count(*) over (order by cn range between 2 preceding and 1 following) as c from sale;
select cn,count(*) over (order by cn range between 0 preceding and 1 following) as c from sale;
select cn,count(*) over (order by cn range between 0 following and 1 following) as c from sale;
select cn,count(*) over (order by cn range between 1 following and 2 following) as c from sale;
select cn,count(*) over (order by cn range between unbounded preceding and 2 preceding) as c from sale;
select cn,count(*) over (order by cn range between unbounded preceding and 0 preceding) as c from sale;
select cn,count(*) over (order by cn range between unbounded preceding and 2 following) as c from sale;
select cn,count(*) over (order by cn range between 2 preceding and unbounded following) as c from sale;
select cn,count(*) over (order by cn range between 0 preceding and unbounded following) as c from sale;
select cn,count(*) over (order by cn range between 0 following and unbounded following) as c from sale;
select cn,count(*) over (order by cn range between 1 following and unbounded following) as c from sale;
select cn,count(*) over (order by cn range between unbounded preceding and unbounded following) as c from sale;

select cn,count(*) over (order by cn desc range between 2 preceding and 1 preceding) as c from sale;
select cn,count(*) over (order by cn desc range between 2 preceding and 0 preceding) as c from sale;
select cn,count(*) over (order by cn desc range between 2 preceding and 1 following) as c from sale;
select cn,count(*) over (order by cn desc range between 0 preceding and 1 following) as c from sale;
select cn,count(*) over (order by cn desc range between 0 following and 1 following) as c from sale;
select cn,count(*) over (order by cn desc range between 1 following and 2 following) as c from sale;
select cn,count(*) over (order by cn desc range between unbounded preceding and 2 preceding) as c from sale;
select cn,count(*) over (order by cn desc range between unbounded preceding and 0 preceding) as c from sale;
select cn,count(*) over (order by cn desc range between unbounded preceding and 2 following) as c from sale;
select cn,count(*) over (order by cn desc range between 2 preceding and unbounded following) as c from sale;
select cn,count(*) over (order by cn desc range between 0 preceding and unbounded following) as c from sale;
select cn,count(*) over (order by cn desc range between 0 following and unbounded following) as c from sale;
select cn,count(*) over (order by cn desc range between 1 following and unbounded following) as c from sale;
select cn,count(*) over (order by cn desc range between unbounded preceding and unbounded following) as c from sale;

-- MPP-1885
SELECT sale.vn,
COUNT(vn) OVER(order by sale.vn asc range between unbounded preceding and 2 preceding)
FROM sale;

-- aggregates in multiple key levels
select cn,pn,vn, count(*) over (order by cn) as c1, 
count(*) over (order by cn,vn) as c2,
count(*) over (order by cn,vn,pn) as c3 from sale;
select cn,pn,vn, count(*) over (order by cn range between 2 preceding and 2 following) as c1,
count(*) over (order by cn,vn rows between 2 preceding and 2 following) as c2,
count(*) over (order by cn,vn,pn) as c3 from sale;

-- MPP-1897
SELECT sale.cn,sale.qty,
SUM(floor(sale.qty)) OVER(order by sale.cn asc range between 2 following and 2 following) 
FROM sale; --mvd 1->3
SELECT sale.cn,sale.qty,
SUM(floor(sale.qty)) OVER(order by ord,sale.cn asc rows between 2 following and 2 following) as sum
FROM sale_ord as sale;

-- MPP-1892
SELECT sale.vn,sale.cn,sale.prc,sale.pn,sale.prc,
COUNT(floor(sale.prc)) OVER(partition by sale.vn,sale.cn,sale.prc,sale.vn order by sale.pn asc range between unbounded preceding and 1 preceding) as count 
from sale; --mvd 1,2,3->6

-- MPP-1893
SELECT sale.prc,sale.cn,sale.vn,sale.pn,sale.cn,
AVG(floor(sale.pn-sale.cn)) OVER(partition by sale.prc,sale.cn order by sale.vn desc range between 1 preceding and unbounded following) as avg
FROM sale; --mvd 1,2->6
SELECT sale.prc,sale.cn,sale.vn,sale.pn,sale.cn,
AVG(floor(sale.pn-sale.cn)) OVER(partition by sale.prc,sale.cn order by sale.vn desc range between 0 preceding and unbounded following) as avg
FROM sale; --mvd 1,2->6

-- MPP-1895
SELECT sale.prc,sale.vn,
COUNT(vn) OVER(partition by sale.prc order by sale.vn asc range between 3 preceding and 3 following) 
FROM sale; --mvd 1->3

-- MPP-1898
SELECT sale.vn,sale.qty,
SUM(floor(sale.qty)) OVER(order by sale.vn desc range between 0 following and 2 following ) as sum
from sale; --mvd 1->3

-- MPP-1907, MPP-1912
-- begin_equivalent
SELECT sale.pn,
COUNT(floor(sale.pn)) OVER(order by sale.pn desc rows between 4 preceding and current row) as count 
FROM sale;
SELECT sale.pn,
COUNT(floor(sale.pn)) OVER(order by sale.pn desc rows between 4 preceding and 0 preceding) as count 
FROM sale;
SELECT sale.pn,
COUNT(floor(sale.pn)) OVER(order by sale.pn desc rows between 4 preceding and 0 following) as count 
FROM sale;
SELECT sale.pn,
COUNT(floor(sale.pn)) OVER(order by sale.pn desc rows 4 preceding) as count 
FROM sale;
-- end_equivalent

-- begin_equivalent
SELECT sale.pn,
COUNT(floor(sale.pn)) OVER(order by sale.pn desc range between 4 preceding and current row) as count 
FROM sale;
SELECT sale.pn,
COUNT(floor(sale.pn)) OVER(order by sale.pn desc range between 4 preceding and 0 preceding) as count 
FROM sale;
SELECT sale.pn,
COUNT(floor(sale.pn)) OVER(order by sale.pn desc range between 4 preceding and 0 following) as count 
FROM sale;
SELECT sale.pn,
COUNT(floor(sale.pn)) OVER(order by sale.pn desc range 4 preceding) as count 
FROM sale;
-- end_equivalent

-- MPP-1915
select cn, qty, sum(qty) over(order by cn) as sum, cume_dist() over(order by cn) as cume1 from sale; --mvd 1->3
SELECT sale.cn,
SUM(sale.cn) OVER(order by sale.cn asc range 1 preceding ),COUNT(floor(sale.vn)) OVER(order by sale.cn asc range 1 preceding ) 
FROM sale;
SELECT sale.cn,
SUM(sale.cn) OVER(order by sale.cn asc range 2 preceding ),COUNT(floor(sale.vn)) OVER(order by sale.cn asc range 1 preceding ) 
FROM sale;

select pn, count(*) over (order by pn range between 100 preceding and 100 following),
count(*) over (order by pn range between 200 preceding and 200 following) from sale;

-- MPP-1923
SELECT sale.cn,sale.pn,sale.vn,
CUME_DIST() OVER(partition by sale.cn,sale.pn order by sale.vn desc,sale.pn desc,sale.cn asc)
FROM sale; --mvd 1,2->4

SELECT sale.cn,sale.vn,sale.pn,
SUM((cn*100+pn/100)%100) OVER(partition by sale.vn,sale.pn order by sale.pn asc rows between 1 following and unbounded following) as sum
from sale; --mvd 2,3->4

-- MPP-1924
SELECT sale.cn,
COUNT(cn) OVER(order by sale.cn range between 7 following and 7 following) as count 
FROM sale;

-- MPP-1925
SELECT sale.pn,sale.vn,
COUNT(floor(sale.cn-sale.prc)) OVER(order by sale.pn asc,sale.vn asc,sale.vn desc rows between current row and 2 following ) as count,sale.pn,
CUME_DIST() OVER(order by sale.pn asc) as cume_dist
FROM sale;

-- MPP-1874
CREATE TABLE fact_test_for_window
(DATE_KEY    NUMERIC(10,0)  NOT NULL,
 BCOOKIE     text  NOT NULL,
 TIME_KEY    INTEGER  NOT NULL,
 EVENT_TYPE  text  NOT NULL
);
insert into fact_test_for_window values
  (20070101, 'W', 100, 'c'),
  (20070101, 'W', 100, 'p'),
  (20070101, 'B', 100, 'c'),
  (20070101, 'A', 10100101, 'p'),
  (20070101, 'A', 20100101, 'p'),
  (20070101, 'B', 101, 'p'),
  (20070101, 'C', 105, 'p'),
  (20070101, 'D', 107, 'p'),
  (20070101, 'E', 10, 'c'),
  (20070101, 'E', 10, 'p'),
  (20070101, 'A', 101, 'c'),
  (20070101, 'A', 101, 'p'),
  (20070101, 'A', 10100101, 'p');

select date_key, bcookie, time_key, event_type, 
(
 case
   when (time_key- lag(time_key, 1, NULL) over (
         partition by date_key, bcookie
         order by date_key, bcookie, time_key, event_type)
   ) > 1800 then 1
            else 0
 end
) AS time_check
from fact_test_for_window;
drop table fact_test_for_window;

-- MPP-1929
SELECT vn,pn,cn,
TO_CHAR(COALESCE(CUME_DIST() OVER(partition by sale.vn,sale.pn order by sale.cn desc),0),'99999999.9999999') as cum_dist,
TO_CHAR(COALESCE(CUME_DIST() OVER(partition by sale.vn,sale.pn order by sale.cn desc),0),'99999999.9999999') as cum_dist
from sale; --mvd 1,2->4

select cn,pn,lag(pn,cn) over (order by ord,pn) from sale_ord;
select cn,pn,lead(pn,cn) over (order by ord,pn) from sale_ord;
select vn,cn,pn,lead(pn,cn) over (partition by vn order by cn,pn) from sale; --mvd 1->4
select vn,cn,pn,lag(pn,cn) over (partition by vn order by cn,pn) from sale; --mvd 1->4

-- MPP-1934
SELECT sale.dt,sale.vn,sale.qty,sale.pn,sale.cn,
LEAD(cast(floor(sale.cn+sale.vn) as int),cast (floor(sale.qty) as int),NULL) OVER(partition by sale.dt,sale.vn,sale.qty order by sale.pn desc,sale.cn desc) as lead
FROM sale; --mvd 1,2,3->6

-- MPP-1935
SELECT sale.vn,sale.qty,sale.prc,
LAG(cast(floor(sale.qty*sale.prc) as int),cast (floor(sale.prc) as int),NULL) OVER(order by ord,sale.vn asc) as lag 
from sale_ord as sale;

-- MPP-1936
SELECT sale.vn,sale.prc,sale.qty,
COUNT(floor(sale.prc)) OVER(order by sale.vn asc),
LAG(cast(floor(sale.qty*sale.prc) as int),cast (floor(sale.prc) as int),NULL) OVER(order by ord,sale.vn asc) as lag 
from sale_ord as sale;

select cn,vn,qty,
sum(qty) over(order by ord,cn rows between 1 preceding and 0 following) as sum1,
sum(qty) over(order by ord,cn rows between 1 preceding and 1 following) as sum2
from sale_ord;

-- MPP-1933
SELECT sale.prc,sale.vn,sale.cn,sale.pn,sale.qty,
LAG(cast(floor(sale.vn) as int),cast (floor(sale.prc) as int),NULL) OVER(partition by sale.prc,sale.vn,sale.cn,sale.pn order by sale.cn desc) as lag1
from sale; --mvd 1,2,3,4->6
SELECT sale.prc,sale.vn,sale.cn,sale.pn,sale.qty,
LAG(cast(floor(sale.qty/sale.vn) as int),cast (floor(sale.pn) as int),NULL) OVER(partition by sale.prc,sale.vn,sale.cn,sale.pn order by sale.cn desc) as lag2
from sale; --mvd 1,2,3,4->6
SELECT sale.prc,sale.vn,sale.cn,sale.pn,sale.qty,
LAG(cast(floor(sale.vn) as int),cast (floor(sale.prc) as int),NULL) OVER(partition by sale.prc,sale.vn,sale.cn,sale.pn order by sale.cn desc) as lag1,
LAG(cast(floor(sale.qty/sale.vn) as int),cast (floor(sale.pn) as int),NULL) OVER(partition by sale.prc,sale.vn,sale.cn,sale.pn order by sale.cn desc) as lag2
from sale; --mvd 1,2,3,4->6

-- MIN/MAX
select cn,vn,min(vn) over (order by ord,cn rows between 2 preceding and 1 preceding) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between 2 preceding and 0 preceding) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between 2 preceding and 1 following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between 0 preceding and 1 following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between 0 following and 1 following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between 1 following and 2 following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between unbounded preceding and 2 preceding) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between unbounded preceding and 0 preceding) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between unbounded preceding and 2 following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between 2 preceding and unbounded following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between 0 preceding and unbounded following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between 0 following and unbounded following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between 1 following and unbounded following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord,cn rows between unbounded preceding and unbounded following) as min from sale_ord;

select cn,vn,min(vn) over (order by ord desc,cn desc rows between 2 preceding and 1 preceding) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between 2 preceding and 0 preceding) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between 2 preceding and 1 following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between 0 preceding and 1 following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between 0 following and 1 following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between 1 following and 2 following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between unbounded preceding and 2 preceding) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between unbounded preceding and 0 preceding) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between unbounded preceding and 2 following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between 2 preceding and unbounded following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between 0 preceding and unbounded following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between 0 following and unbounded following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between 1 following and unbounded following) as min from sale_ord;
select cn,vn,min(vn) over (order by ord desc,cn desc rows between unbounded preceding and unbounded following) as min from sale_ord;

select pn,vn,max(vn) over (order by pn range between 200 preceding and 150 preceding) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between 200 preceding and 0 preceding) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between 200 preceding and 150 following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between 0 preceding and 150 following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between 0 following and 150 following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between 150 following and 200 following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between unbounded preceding and 200 preceding) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between unbounded preceding and 0 preceding) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between unbounded preceding and 200 following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between 200 preceding and unbounded following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between 0 preceding and unbounded following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between 0 following and unbounded following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between 150 following and unbounded following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn range between unbounded preceding and unbounded following) as max from sale; --mvd 1->3

select pn,vn,max(vn) over (order by pn desc range between 200 preceding and 150 preceding) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between 200 preceding and 0 preceding) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between 200 preceding and 150 following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between 0 preceding and 150 following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between 0 following and 150 following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between 150 following and 200 following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between unbounded preceding and 200 preceding) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between unbounded preceding and 0 preceding) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between unbounded preceding and 200 following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between 200 preceding and unbounded following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between 0 preceding and unbounded following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between 0 following and unbounded following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between 150 following and unbounded following) as max from sale; --mvd 1->3
select pn,vn,max(vn) over (order by pn desc range between unbounded preceding and unbounded following) as max from sale; --mvd 1->3

select cn,vn,pn,min(vn) over (partition by cn order by pn rows between 2 preceding and 1 preceding) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between 2 preceding and 0 preceding) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between 2 preceding and 1 following) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between 0 preceding and 1 following) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between 0 following and 1 following) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between 1 following and 2 following) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between unbounded preceding and 2 preceding) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between unbounded preceding and 0 preceding) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between unbounded preceding and 2 following) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between 2 preceding and unbounded following) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between 0 preceding and unbounded following) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between 0 following and unbounded following) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between 1 following and unbounded following) as min from sale; --mvd 1->4
select cn,vn,pn,min(vn) over (partition by cn order by pn rows between unbounded preceding and unbounded following) as min from sale; --mvd 1->4

select cn,pn,vn,max(vn) over (partition by cn order by pn range between 200 preceding and 150 preceding) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between 200 preceding and 0 preceding) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between 200 preceding and 150 following) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between 0 preceding and 150 following) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between 0 following and 150 following) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between 150 following and 200 following) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between unbounded preceding and 200 preceding) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between unbounded preceding and 0 preceding) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between unbounded preceding and 200 following) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between 200 preceding and unbounded following) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between 0 preceding and unbounded following) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between 0 following and unbounded following) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between 150 following and unbounded following) as max from sale; --mvd 1,2->4
select cn,pn,vn,max(vn) over (partition by cn order by pn range between unbounded preceding and unbounded following) as max from sale; --mvd 1,2->4

-- MPP-1943
SELECT sale.cn,sale.pn,sale.vn,
MAX(floor(sale.pn/sale.vn)) OVER(order by sale.cn asc,sale.cn asc,sale.pn asc)
FROM sale;

-- MPP-1944
SELECT sale.pn,cn,dt,pn,prc,
MAX(floor(sale.prc+sale.prc)) OVER(partition by sale.pn,sale.cn,sale.dt,sale.pn order by sale.pn desc,sale.pn asc rows between current row and unbounded following ) 
FROM sale; --mvd 1,2,3->6

select qty,cn,pn,dt,pn,prc,
MAX(floor(sale.prc+sale.prc)) OVER(partition by sale.qty,sale.cn,sale.pn,sale.dt order by sale.pn asc range between 0 following and unbounded following ) 
from sale; --mvd 1,2,3,4->7

-- MPP-1947
SELECT dt,cn,cn,qty,
MIN(floor(sale.qty)) OVER(partition by sale.dt,sale.cn,sale.dt order by sale.cn desc range between 1 preceding and unbounded following ) 
FROM sale; --mvd 1,2->5

SELECT dt,cn,cn,qty, 
MAX(floor(sale.qty)) OVER(partition by sale.dt,sale.cn,sale.dt order by sale.cn desc range between 1 preceding and unbounded following )
from sale; --mvd 1,2->5

-- FIRST_VALUE/LAST_VALUE
select cn,vn,first_value(vn) over (order by ord,cn rows between 2 preceding and 1 preceding) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between 2 preceding and 0 preceding) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between 2 preceding and 1 following) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between 0 preceding and 1 following) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between 0 following and 1 following) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between 1 following and 2 following) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between unbounded preceding and 2 preceding) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between unbounded preceding and 0 preceding) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between unbounded preceding and 2 following) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between 2 preceding and unbounded following) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between 0 preceding and unbounded following) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between 0 following and unbounded following) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between 1 following and unbounded following) as first_value from sale_ord;
select cn,vn,first_value(vn) over (order by ord,cn rows between unbounded preceding and unbounded following) as first_value from sale_ord;

select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between 200 preceding and 150 preceding) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between 200 preceding and 0 preceding) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between 200 preceding and 150 following) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between 0 preceding and 150 following) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between 0 following and 150 following) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between 150 following and 200 following) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between unbounded preceding and 200 preceding) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between unbounded preceding and 0 preceding) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between unbounded preceding and 200 following) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between 200 preceding and unbounded following) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between 0 preceding and unbounded following) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between 0 following and unbounded following) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between 150 following and unbounded following) as last_value from sale;--mvd 1->3
select pn,vn,last_value((pn+vn)/100) over (order by pn desc range between unbounded preceding and unbounded following) as last_value from sale;--mvd 1->3

select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between 2 preceding and 1 preceding) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between 2 preceding and 0 preceding) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between 2 preceding and 1 following) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between 0 preceding and 1 following) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between 0 following and 1 following) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between 1 following and 2 following) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between unbounded preceding and 2 preceding) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between unbounded preceding and 0 preceding) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between unbounded preceding and 2 following) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between 2 preceding and unbounded following) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between 0 preceding and unbounded following) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between 0 following and unbounded following) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between 1 following and unbounded following) as last_value from sale; --mvd 1->4
select cn,vn,pn,last_value(vn) over (partition by cn order by pn rows between unbounded preceding and unbounded following) as last_value from sale; --mvd 1->4

select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between 200 preceding and 150 preceding) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between 200 preceding and 0 preceding) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between 200 preceding and 150 following) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between 0 preceding and 150 following) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between 0 following and 150 following) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between 150 following and 200 following) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between unbounded preceding and 200 preceding) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between unbounded preceding and 0 preceding) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between unbounded preceding and 200 following) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between 200 preceding and unbounded following) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between 0 preceding and unbounded following) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between 0 following and unbounded following) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between 150 following and unbounded following) as first_value from sale; --mvd 1->4
select cn,pn,vn,first_value(vn) over (partition by cn order by pn range between unbounded preceding and unbounded following) as first_value from sale; --mvd 1->4

-- MPP-1957
select dt,pn,qty,
LEAD(cast(floor(sale.qty/sale.qty) as int),cast (floor(sale.qty) as int),NULL) OVER(partition by sale.dt order by sale.pn asc) as lead 
from sale; --mvd 1->4

-- MPP-1958
SELECT sale.qty, 
MIN(floor(sale.qty)) OVER(order by ord,sale.pn asc rows unbounded preceding ) as min,sale.pn,
LAG(cast(floor(sale.qty-sale.prc) as int),cast (floor(sale.qty/sale.vn) as int),NULL) OVER(order by ord,sale.pn asc) as lag
FROM sale_ord as sale;

-- MPP-1960
SELECT prc,cn,vn,
FIRST_VALUE(floor(sale.prc)) OVER(partition by sale.prc,sale.cn order by sale.vn asc range between unbounded preceding and unbounded following) 
FROM sale; --mvd 1,2->4
SELECT cn,pn,dt,
FIRST_VALUE(floor(sale.pn/sale.pn)) OVER(partition by sale.cn,sale.pn,sale.dt order by sale.cn desc range between unbounded preceding and 1 following ) as fv 
from sale; --mvd 1,2,3->4

-- MPP-1964
SELECT cn,(cn-prc),prc,
CORR(floor(sale.cn-sale.prc),floor(sale.prc)) OVER(partition by sale.cn,sale.cn order by sale.cn desc range between 1 preceding and 1 following ) as corr 
FROM sale; --mvd 1->4
SELECT cn,floor(qty/pn),cn,
CORR(floor(sale.qty/sale.pn),floor(sale.cn)) OVER(order by sale.cn desc range 3 preceding) as correlation 
from sale; --mvd 1->4

-- MPP-1976
SELECT sale.cn,sale.vn,sale.pn, 
SUM(floor(sale.cn*sale.vn)) OVER(partition by sale.vn,sale.pn order by sale.pn asc range between 1 following and unbounded following ) as sum
from sale; --mvd 1,3->4

-- Use expressions in the frame clause
select cn,pn,qty,sum(qty) over (order by ord,pn rows cn preceding) from sale_ord;
select cn,pn,qty,sum(qty) over (order by ord,pn rows between current row and cn following) from sale_ord;
select cn,pn,qty,sum(qty) over (order by ord,pn rows between cn preceding and cn+2 following) from sale_ord;
select cn,pn,qty,sum(qty) over (order by ord,pn rows between 1 preceding and 1 following),
sum(qty) over (order by ord,pn rows between cn preceding and cn+2 following) from sale_ord;
select cn,pn,qty,sum(qty) over (order by ord,pn rows between current row and cn following),
sum(qty) over (order by ord,pn rows between cn preceding and cn+2 following) from sale_ord;

select cn,pn,qty,sum(qty) over (order by pn range cn preceding) from sale; --mvd 2->4
select cn,pn,qty,sum(qty) over (order by pn range between current row and cn following) from sale; --mvd 2->4
select cn,pn,qty,sum(qty) over (order by cn range between current row and cn following) from sale; --mvd 1->4
select cn,pn,qty,sum(qty) over (order by cn range between cn-1 preceding and cn following) from sale; --mvd 1->4
select cn,pn,qty,sum(qty) over (partition by cn order by pn range between cn*100-50 preceding and cn*200 following) as sum from sale; --mvd 1->4
select pn,vn,max(vn) over (order by pn range between cn-1 following and cn-2 following) as max from sale;

-- MPP-2036
select cn,qty,sum(qty) over(order by cn range cn preceding) as sum from (select sale.* from sale,customer,vendor where sale.cn=customer.cn and sale.vn=vendor.vn) sale; --mvd 1->3

-- MPP-1744
select cn,vn,ntile(cn) over(partition by cn order by vn) from sale; --mvd 1->3
select cn,vn,ntile(qty) over(partition by cn order by vn) from sale;
select cn,vn,ntile(cn) over(partition by cn+vn order by vn) from sale;
select cn,vn,ntile(cn+vn) over(partition by cn+vn order by vn) from sale; --mvd 1,2->3

-- MPP-2045
SELECT sale.cn,sale.qty,
count(cn) OVER(order by sale.cn,sale.qty rows between sale.qty following and 4 following) as count
from sale;

-- MPP-2068
SELECT vn,cn,prc,
COUNT(floor(sale.cn)) OVER(order by sale.vn asc,sale.cn asc,sale.prc rows prc preceding ) as count
FROM sale;

-- MPP-2075
SELECT sale.vn,sale.cn,
AVG(sale.pn) OVER(order by sale.vn asc range between vn preceding and vn-10 preceding ) as avg
from sale; --mvd 1->3
SELECT sale.qty,sale.cn,sale.vn,
AVG(sale.pn) OVER(order by sale.vn asc range between 0 preceding and vn preceding ) as avg
from sale; --mvd 3->4
SELECT sale.vn,sale.cn,
min(sale.pn) OVER(order by sale.vn asc range between vn preceding and vn-10 preceding ) as min
from sale; --mvd 1->3
SELECT sale.qty,sale.cn,sale.vn,
min(sale.pn) OVER(order by sale.vn asc range between 0 preceding and vn preceding ) as min
from sale; --mvd 3->4

-- MPP-2078
SELECT sale.cn,sale.vn,
COUNT(floor(sale.cn)) OVER(partition by sale.cn order by sale.vn asc range between 1 preceding and floor(sale.cn) preceding ) 
FROM sale; --mvd 1,2->3

-- MPP-2080
SELECT sale.vn,qty,
COUNT(qty) OVER(order by sale.vn desc range between unbounded preceding and 1 preceding) 
FROM sale; --mvd 1->3

-- MPP-2081
SELECT cn,qty,floor(prc/cn),
COUNT(floor(sale.pn*sale.prc)) OVER(order by sale.cn asc range between floor(sale.qty) preceding and floor(sale.prc/sale.cn) preceding) 
from sale; --mvd 1->4

-- MPP-2135
select cn,sum(qty) over(order by cn), sum(qty) from sale group by cn,qty; --mvd 1->2
select cn,sum(sum(qty)) over(order by cn), sum(qty) from sale group by cn,qty; --mvd 1->2
select cn,sum(sum(qty) + 1) over(order by cn), sum(qty) + 1 from sale group by cn,qty; --mvd 1->2
select cn,sum(qty+1) over(order by cn), sum(qty+1) from sale group by cn,qty; --mvd 1->2

-- MPP-2152
SELECT cn, pn, dt, PERCENT_RANK() OVER(partition by cn,dt order by cn desc),
                 LEAD(pn,cn,NULL) OVER(partition by cn,dt order by pn, cn desc)
FROM sale; --mvd 1,3->4

-- MPP-2163
SELECT cn, pn, vn, dt, qty,
  ROW_NUMBER() OVER( partition by vn, qty, dt order by cn asc ),
  PERCENT_RANK() OVER( partition by vn, dt order by cn desc ),
  PERCENT_RANK() OVER( order by pn asc )
FROM (SELECT * FROM sale) sale; --mvd 3,4,5->6; 3,4->7

-- MPP-2189
create view v1_for_window as
select dt, sum(cn) over(order by grouping(cn) range grouping(cn) preceding) 
from sale group by rollup(cn,dt);
\d v1_for_window
drop view v1_for_window;

-- MPP-2194, MPP-2236
drop table if exists win_test_for_window;
create table win_test_for_window (i int, j int);
insert into win_test_for_window values (0,0), (1,null), (2, null), (3,null);
select i,j,sum(i) over(order by j range 1 preceding) from win_test_for_window; --mvd 2->3
select i,j,sum(j) over(order by i rows 1 preceding) from win_test_for_window;

-- MPP-2305
SELECT sale.vn,sale.cn,sale.pn,
CUME_DIST() OVER(partition by sale.vn,sale.cn order by sale.pn desc) as cume_dist1,
CUME_DIST() OVER(partition by sale.cn,sale.vn order by sale.cn asc) as cume_dist2
FROM sale order by 1,2,3; --order 1,2,3

-- MPP-2322
select ord, pn,cn,vn,sum(vn) over (order by ord, pn rows between cn following and cn+1 following) as sum from sale_ord;
select ord, pn,cn,vn,sum(vn) over (order by ord, pn rows between cn following and cn following) as sum from sale_ord;

-- MPP-2323
select ord, cn,vn,sum(vn) over (order by ord rows between 3 following and floor(cn) following ) from sale_ord;

-- Test use of window functions in places they shouldn't be allowed: MPP-2382
-- CHECK constraints
CREATE TABLE wintest_for_window (i int check (i < count(*) over (order by i)));

CREATE TABLE wintest_for_window (i int default count(*) over (order by i));

-- index expression and function
CREATE TABLE wintest_for_window (i int);
CREATE INDEX wintest_idx_for_window on wintest_for_window (i) where i < count(*) over (order by i);
CREATE INDEX wintest_idx_for_window on wintest_for_window (sum(i) over (order by i));
-- alter table
ALTER TABLE wintest_for_window alter i set default count(*) over (order by i);
alter table wintest_for_window alter column i type float using count(*) over (order by
i)::float;

-- update
insert into wintest_for_window values(1);
update wintest_for_window set i = count(*) over (order by i);

-- domain suport
create domain wintestd as int default count(*) over ();
create domain wintestd as int check (value < count(*) over ());

drop table wintest_for_window;

-- MPP-3295
-- begin equivalent
select cn,vn,rank() over (partition by cn order by vn) as rank
from sale group by cn,vn order by rank; --mvd 1->3

select cn,vn,rank() over (partition by cn order by vn) as rank
from (select cn,vn from sale group by cn,vn) sale order by rank; --mvd 1->3

-- end equivalent

-- begin equivalent
select cn,vn, 1+rank() over (partition by cn order by vn) as rank
from sale group by cn,vn order by rank; --mvd 1->3

select cn,vn, 1+rank() over(partition by cn order by vn) as rank
from (select cn,vn from sale group by cn,vn) sale order by rank; --mvd 1->3
-- end equivalent

-- begin equivalent
select cn,vn, sum(qty), 1+rank() over (partition by cn order by vn) as rank
from sale group by cn,vn order by rank; --mvd 1->3

select cn,vn, sum, 1+rank() over (partition by cn order by vn) as rank
from (select cn,vn,sum(qty) as sum from sale group by cn, vn) sale order by rank; --mvd 1->3
-- end equivalent

select cn, first_value(NULL) over (partition by cn order by case when 1=1 then pn || ' ' else 'test' end)
	from sale order by first_value(NULL) over (
	partition by cn order by case when 1=1 then (pn || ' ') else 'test'::character varying(15) end); --mvd 1->2
select cn, first_value(NULL) over (partition by cn order by case when 1=1 then pn || ' ' else 'test' end)
	from sale order by first_value(NULL) over (
	partition by cn order by case when 1=1 then (pn || ' ') else 'test' end); --mvd 1->2

-- MPP-4836
select pcolor, pname, pn,
    row_number() over (w) as n,
    lag(pn+0) over (w) as l0,
    lag(pn+1) over (w) as l1,
    lag(pn+2) over (w) as l2,
    lag(pn+3) over (w) as l3,
    lag(pn+4) over (w) as l4,
    lag(pn+5) over (w) as l5,
    lag(pn+6) over (w) as l6,
    lag(pn+7) over (w) as l7,
    lag(pn+8) over (w) as l8,
    lag(pn+9) over (w) as l9,
    lag(pn+10) over (w) as l10,
    lag(pn+11) over (w) as l11,
    lag(pn+12) over (w) as l12,
    lag(pn+13) over (w) as l13,
    lag(pn+14) over (w) as l14,
    lag(pn+15) over (w) as l15,
    lag(pn+16) over (w) as l16,
    lag(pn+17) over (w) as l17,
    lag(pn+18) over (w) as l18,
    lag(pn+19) over (w) as l19,
    lag(pn+20) over (w) as l20,
    lag(pn+21) over (w) as l21,
    lag(pn+22) over (w) as l22,
    lag(pn+23) over (w) as l23,
    lag(pn+24) over (w) as l24,
    lag(pn+25) over (w) as l25,
    lag(pn+26) over (w) as l26,
    lag(pn+27) over (w) as l27,
    lag(pn+28) over (w) as l28,
    lag(pn+29) over (w) as l29,
    lag(pn+30) over (w) as l30,
    lag(pn+31) over (w) as l31,
    lag(pn+32) over (w) as l32
from product
window w as (partition by pcolor order by pname)
order by 1,2,3;

-- MPP-4840
explain select n from ( select row_number() over () from (values (0)) as t(x) ) as r(n) group by n;

-- MPP-5219
select case when 1=2 then rank() over (partition by cn order by pn) end from sale;
select cn, pn, case when 1=2 then rank() over (partition by cn order by pn) end,
rank() over (partition by cn order by pn) from sale; --mvd 1,2->4
select pn, vn, case when 1=2 then rank() over (partition by cn order by pn) end,
rank() over (partition by pn order by vn) from sale; --mvd 1,2->4

-- MPP-6027
drop table if exists olap_window_test;
create table olap_window_test (i int, j bigint, k int, l int, m int);
insert into olap_window_test select i, i%100, i%123, i%234, i%345 from generate_series(1, 500) i;
-- begin equivalent
select j, sum(k), row_number() over (partition by j order by sum(k)) from olap_window_test group by j order by j limit 10;

select j, sum, row_number() over (partition by j order by sum) from (select j, sum(k) as sum from olap_window_test group by j) tmp
order by j limit 10;

-- end equivalent
drop table olap_window_test;

-- Test for MPP-11645

create table olap_window_r (a int, b int, x int,  y int,  z int ) distributed by (b);

insert into olap_window_r values
    ( 1,  17, 419, 291, 2513 ),
    ( 2,  16, 434, 293, 2513 ),
    ( 3,  15, 439, 295, 2483 ),
    ( 4,  14, 445, 297, 2675 ),
    ( 5,  13, 473, 299, 2730 ),
    ( 6,  12, 475, 303, 2765 ),
    ( 7,  11, 479, 305, 2703 ),
    ( 8,  10, 502, 307, 2749 ),
    ( 9,   9, 528, 308, 2850 ),
    ( 10,  8, 532, 309, 2900 ),
    ( 11,  7, 567, 315, 2970 ),
    ( 12,  6, 570, 317, 3025 ),
    ( 13,  5, 635, 319, 3045 ),
    ( 14,  4, 653, 320, 3093 ),
    ( 15,  3, 711, 321, 3217 ),
    ( 16,  2, 770, 325, 3307 ),
    ( 17,  1, 778, 329, 3490 );

select b, sum(x) over (partition by b), 10*b
from olap_window_r order by b; --order 3

select sum(x) over (partition by b), 10*b
from olap_window_r order by b; --order 2

select a, sum(x) over (partition by a), 10*a
from olap_window_r order by a; --order 3

select sum(x) over (partition by a), 10*a
from olap_window_r order by a; -- order 2

drop table if exists olap_window_r cascade; --ignore

-- End MPP-11645

-- MPP-12082

select 
    f, 
    sum(g) over (partition by f)
from
    (select 'A', 1) b(j, g)
    join 
    (select 'A', 'B') c(j, f)
    using(j)
group by 
    1,
    b.g;


select 
    f, 
    sum(b.g) over (partition by f)
from
    (select 'A', 1) b(j, g)
    join 
    (select 'A', 'B') c(j, f)
    using(j)
group by 
    1,
    g;

select 
    2*b.g,
    lower(c.f),
    sum(2*b.g) over (partition by lower(c.f))
from
    (select 'A', 1) b(j, g)
    join 
    (select 'A', 'B') c(j, f)
    using(j)
group by 
    2*b.g,
    lower(c.f);

select 
    2*g,
    lower(c.f),
    sum(2*g) over (partition by lower(c.f))
from
    (select 'A', 1) b(j, g)
    join 
    (select 'A', 'B') c(j, f)
    using(j)
group by 
    2*b.g,
    lower(f);
-- End MPP-12082
-- MPP-13802
select lag(x) over (w)
	from(select 1 x, 2 y, 3 z)s
	window w as (partition by y order by z rows unbounded preceding);
select rank() over (wx)
	from(select 1 x, 2 y, 3 z)s
	window w as (partition by y order by z), wx as (w);
-- End MPP-13802

drop table if exists olap_window_test;
create table olap_window_test (n numeric, d date);
insert into olap_window_test values (12, '2011-05-01'), (34, '2011-05-02'), (89, '2011-05-03');

select stddev(n) over(order by d range interval '1 day' preceding), n from olap_window_test;
select stddev(n) over(order by d range interval '1 day' preceding),
       sum(n) over(order by d range interval '1 day' preceding),
       avg(n) over(order by d range interval '1 day' preceding), n from olap_window_test;
select stddev(n) over(order by d range interval '2 day' preceding),
       sum(n) over(order by d range interval '2 day' preceding),
       avg(n) over(order by d range interval '2 day' preceding), n from olap_window_test;
select stddev(n) over(order by d range between current row and interval '1 day' following),
       sum(n) over(order by d range between current row and interval '1 day' following),
       avg(n) over(order by d range between current row and interval '1 day' following), n from olap_window_test;


-- MPP-19244; push predicates below window functions
drop table if exists window_preds;
create table window_preds(i int, j int, k int);
insert into window_preds values
  (1,2,3),
  (2,3,4),
  (3,4,5),
  (4,5,6),
  (5,6,7),
  (6,7,8);
select * from (select i,j,k, sum(i) over(), row_number() over(order by i), rank() over(order by i) from window_preds) as foo where i>2 order by rank;
select * from (select i,j,k, sum(i) over(), row_number() over(partition by i), rank() over(order by i) from window_preds) as foo where i>2 order by rank;
select * from (select i,j,k, sum(i) over(partition by i), row_number() over(partition by i), rank() over(partition by i order by i) from window_preds) as foo where i>2 order by sum;
select * from (select i,j,k, sum(i) over(partition by i), row_number() over(partition by i), rank() over(partition by i order by i) from window_preds) as foo where i>2 or j>2 order by sum;
select * from (select i,j,k, sum(i) over(partition by i), row_number() over(partition by i), rank() over(partition by i order by i) from window_preds) as foo where i>2 and j>2 order by sum;
select * from (select i,j,k, sum(i) over(partition by i,j), row_number() over(partition by i,j), rank() over(partition by i,j order by i) from window_preds) as foo where i>2 and j>2 order by sum;
select * from (select i,j,k, sum(i) over(partition by i,j), row_number() over(partition by i,j), rank() over(partition by i,j order by i) from window_preds) as foo where i+j>2 order by sum;
select * from (select i, sum(i) over(partition by j) from (select i,j, row_number() over(partition by i) from window_preds) as bar) as foo where i>2 order by sum;

-- MPP-19964
drop table if exists customers_test;
create table customers_test(name text, device_model text, device_id integer, ppp numeric) distributed by (device_id);
INSERT INTO customers_test values
  ('n1', 'd1', 1, 1),
  ('n1', 'd1', 1, 3),
  ('n2', 'd1', 1, 2),
  ('n2', 'd1', 2, 1),
  ('n2', 'd1', 2, 2),
  ('n3', 'd3', 1, 0);
SELECT COUNT(*)
FROM
(
  SELECT name,device_model,median(ppp) md
  FROM
  (
    SELECT name,
      device_model,
      device_id,
      ppp,
      ntile(4) over (partition by name,device_model order by ppp)
    FROM customers_test
  ) a
  GROUP BY name,device_model
  HAVING COUNT(DISTINCT CASE WHEN ppp > 0 THEN device_id ELSE NULL END)>0
) b;
