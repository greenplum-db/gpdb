create table perct as select a, a / 10 as b from generate_series(1, 100)a distributed by (a);
create table perct2 as select a, a / 10 as b from generate_series(1, 100)a, generate_series(1, 2);
create table perct3 as select a, b from perct, generate_series(1, 10)i where a % 7 < i;
create table perct4 as select case when a % 10 = 5 then null else a end as a,
	b, null::float as c from perct;
create table percts as select '2012-01-01 00:00:00'::timestamp + interval '1day' * i as a,
	i / 10 as b, i as c from generate_series(1, 100)i;
create table perctsz as select '2012-01-01 00:00:00 UTC'::timestamptz + interval '1day' * i as a,
	i / 10 as b, i as c from generate_series(1, 100)i;
create view percv as select percentile_cont(0.4) within group (order by a / 10),
	median(a), percentile_disc(0.51) within group (order by a desc) from perct group by b order by b;
create view percv2 as select median(a) as m1, median(a::float) as m2 from perct;

create table mpp_22219(col_a character(2) NOT NULL, dkey_a character varying(8) NOT NULL, value double precision)
WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column, COMPRESSTYPE=zlib, OIDS=FALSE)
DISTRIBUTED BY (dkey_a);
insert into mpp_22219 select i, i, i from  (select * from generate_series(1, 20) i ) a ;

create table mpp_21026 ( t1 varchar(10), t2 int);
insert into mpp_21026 select i, i from  (select * from generate_series(1, 20) i ) a ;

create table mpp_20076 (col1 timestamp, col2 int);
insert into mpp_20076 select to_timestamp(i),i from generate_series(1,20) i;

CREATE TABLE mpp_22413
(
  col_a character(2) NOT NULL,
  d1 character varying(8) NOT NULL,
  d2 character varying(8) NOT NULL,
  d3 character varying(8) NOT NULL,
  value1 double precision,
  value2 double precision
)
WITH (OIDS=FALSE)
DISTRIBUTED BY (d1,d2,d3);

insert into mpp_22413
select i, i, i, i, i,i
from  (select * from generate_series(1, 99) i ) a ;

select percentile_cont(0.5) within group (order by a),
	median(a), percentile_disc(0.5) within group(order by a) from perct;
select b, percentile_cont(0.5) within group (order by a),
	median(a), percentile_disc(0.5) within group(order by a) from perct group by b order by b;
select percentile_cont(0.2) within group (order by a) from generate_series(1, 100)a;
select a / 10, percentile_cont(0.2) within group (order by a) from generate_series(1, 100)a
	group by a / 10 order by a / 10;
select percentile_cont(0.2) within group (order by a),
	percentile_cont(0.8) within group (order by a desc) from perct group by b order by b;
select percentile_cont(0.1) within group (order by a), count(*), sum(a) from perct
	group by b order by b;
select percentile_cont(0.6) within group (order by a), count(*), sum(a) from perct;
select percentile_cont(0.3) within group (order by a) + count(*) from perct group by b order by b;
select median(a) from perct group by b having median(a) = 5;
select median(a), percentile_cont(0.6) within group (order by a desc) from perct group by b having count(*) > 1 order by 1;
select median(10);
select count(*), median(b+1) from perct group by b+2
	having median(b+1) in (select avg(b+1) from perct group by b+2);
select median(a) from perct2;
select median(a) from perct2 group by b order by b;
select b, count(*), count(distinct a), median(a) from perct3 group by b order by b;
select b+1, count(*), count(distinct a),
       median(a), percentile_cont(0.3) within group (order by a desc)
from perct group by b+1 order by b+1;
select median(a), median(c) from perct4;
select median(a), median(c) from perct4 group by b;
select count(*) over (partition by b), median(a) from perct group by b order by b;
select sum(median(a)) over (partition by b) from perct group by b order by b;
select percentile_disc(0) within group (order by a) from perct;
prepare p (float) as select percentile_cont($1) within group (order by a)
  from perct group by b order by b;
execute p(0.1);
execute p(0.8);
deallocate p;
select sum((select median(a) from perct)) from perct;
select percentile_cont(null) within group (order by a) from perct;
select percentile_cont(null) within group (order by a),
       percentile_disc(null) within group (order by a desc) from perct group by b;
select median(a), percentile_cont(0.5) within group (order by a),
       percentile_disc(0.5) within group(order by a),
       (select min(a) from percts) - interval '1day' + interval '1day' * median(c),
       (select min(a) from percts) - interval '1day' + interval '1day' *
         percentile_disc(0.5) within group (order by c)
from percts group by b order by b;
select percentile_cont(1.0/86400) within group (order by a) from percts
	where c between 1 and 2;
select percentile_cont(0.1) within group (order by a),
       percentile_cont(0.9) within group (order by a desc) from percts;
select percentile_cont(0.1) within group (order by a),
       percentile_cont(0.2) within group (order by a) from perctsz;
select median(a - (select min(a) from percts)) from percts;
select median(a), b from perct group by b order by b desc;
select count(*) from(select median(a) from perct group by ())s;
select median(a) from perct group by grouping sets((b)) order by b;
select distinct median(a), count(*) from perct;
select perct.a, 0.2*avg(perct2.a) as avga,
	percentile_cont(0.34)within group(order by perct2.b)
	from
		(select a, a / 10 b from generate_series(1, 100)a)perct,
		(select a, a / 10 b from generate_series(1, 100)a)perct2
	where perct.a=perct2.a group by perct.a having median(perct.b) > 10;
select median(a - '2011-12-31 00:00:00 UTC'::timestamptz) from perctsz group by b order by median;
-- view
select * from percv;

select pg_get_viewdef('percv');
select pg_get_viewdef('percv2');

-- errors
-- no WITHIN GROUP clause
select percentile_cont(a) from perct;
-- the argument must not contain variable
select percentile_cont(a) within group (order by a) from perct;
-- ungrouped column
select b, percentile_disc(0.1) within group (order by a) from perct;
-- nested aggregate
select percentile_cont(count(*)) within group (order by a) from perct;
select sum(percentile_cont(0.22) within group (order by a)) from perct;
-- OVER clause
select percentile_cont(0.3333) within group (order by a) over (partition by a%2) from perct;
select median(a) over (partition by b) from perct group by b;
-- function scan
select * from median(10);
-- wrong type argument
select percentile_disc('a') within group (order by a) from perct;
-- nested case
select count(median(a)) from perct;
select median(count(*)) from perct;
select percentile_cont(0.2) within group (order by count(*) over()) from perct;
select percentile_disc(0.1) within group (order by group_id()) from perct;
-- subquery in argument
select percentile_cont((select 0.1 from gp_id)) within group (order by a) from perct;
-- volatile argument
select percentile_cont(floor(random()*0.1)+0.5) within group (order by a) from perct;
-- out of range
select percentile_cont(-0.1) within group (order by a) from perct;
select percentile_cont(1.00000001) within group (order by a) from perct;
-- correlated subquery
select sum((select median(a) from perct where b = t.b)) from perct t;
-- used in LIMIT
select * from perct limit median(a);
-- multiple sort key
select percentile_cont(0.8) within group (order by a, a + 1, a + 2) from perct;
-- set-returning
select generate_series(1, 2), median(a) from perct;
-- GROUPING SETS
select median(a) from perct group by grouping sets((), (b));
-- wrong type in ORDER BY
select median('text') from perct;
select percentile_cont(now()) within group (order by a) from percts;
select percentile_cont(0.5) within group (order by point(0,0)) from perct;
-- outer references
select (select a from perct where median(t.a) = 5) from perct t;
select (select array_agg(a ORDER BY a) from perct where median(t.a) = 50.5) from (select * from perct t order by a offset 0) as t;

-- MPP-22219
select count(*) from
(SELECT b.dkey_a, MEDIAN(B.VALUE)
FROM     mpp_22219 B
GROUP BY b.dkey_a) s;

select count(*) from
(SELECT b.dkey_a, percentile_cont(0.5) within  group (order by b.VALUE)
FROM     mpp_22219 B
GROUP BY b.dkey_a) s;

-- MPP-21026
select median(t2) from mpp_21026 group by t1;

-- MPP-20076
select 1, to_char(col1, 'YYYY'), median(col2) from mpp_20076 group by 1, 2;
select 1, col1, median(col2) from mpp_20076 group by 1, 2;
select to_char(col1, 'YYYY') AS tstmp_column, median(col2) from mpp_20076 group by 1;
select 1, median(col2) from mpp_20076 group by 1;

-- MPP-22413
select median(value1), count(*)
from  mpp_22413
where d2 ='55'
group by d1, d2, d3, value2;

select median(value1), count(*)
from  mpp_22413
where d2 ='55'
group by d1, d2, d3, value2::int;

select median(value1), count(*)
from  mpp_22413
where d2 ='55'
group by d1, d2, d3, value2::varchar;

select median(value1), count(*)
from  mpp_22413
where d2 ='55'
group by d1, d2, value2;

select median(value1), count(*)
from  mpp_22413
where d2 ='55'
group by d1, d2, value2, d3;

select median(value1), count(*)
from  mpp_22413
where d2 ='55'
group by d1, d2;

drop view percv2;
drop view percv;
drop table perct;
drop table perct2;
drop table perct3;
drop table perct4;
drop table percts;
drop table perctsz;
drop table mpp_22219;
drop table mpp_21026;
drop table mpp_20076;
drop table mpp_22413;
