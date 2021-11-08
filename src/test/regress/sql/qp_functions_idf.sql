-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

RESET ALL;
-- start_ignore
create schema qp_idf;
set search_path to qp_idf;

create language plpython3u;

create table perct as select a, a / 10 as b from generate_series(1, 100)a;
create table perct2 as select a, a / 10 as b from generate_series(1, 100)a, generate_series(1, 2);
create table perct3 as select a, b from perct, generate_series(1, 10)i where a % 7 < i;
create table perct4 as select case when a % 10 = 5 then null else a end as a,
        b, null::float as c from perct;
create table percts as select '2012-01-01 00:00:00'::timestamp + interval '1day' * i as a,
        i / 10 as b, i as c from generate_series(1, 100)i;
create table perctsz as select '2012-01-01 00:00:00 UTC'::timestamptz + interval '1day' * i as a,
        i / 10 as b, i as c from generate_series(1, 100)i;
create table perctnum as select a, (a / 13)::float8  as b, (a * 1.9999 )::numeric as c  from generate_series(1, 100)a;
create table perctint as select 
'2006-01-01 13:10:13'::timestamp + interval '1day' * i as ts1,
'2010-01-01 23:10:03'::timestamp + interval '1day 20 hours 12 minutes' * i as ts2,
 '2006-01-01 13:10:13'::timestamptz + interval '10 minutes' * i as tstz1,
'2006-01-01 13:10:13'::timestamptz + interval '12 hours 10 minutes' * i as tstz2, 
interval '1 day 1 hour 12 secs' * i as days1,interval '42 minutes 10 seconds' * i as days2,
random() * 9 + i  as b, 
i as c from generate_series(1, 100)i;

-- reduce noise, specifying a precision and format
SET datestyle = "ISO, DMY";
SET extra_float_digits to 0;
-- end_ignore

--TIMESTAMPTZ

select c, percentile_cont(0.9999) within group (order by tstz2 - tstz1) from perctint group by c  order by c limit 10;

select c, percentile_cont(0.9999) within group (order by tstz2 + days1 ) from perctint group by c  order by c limit 10;

select c, percentile_cont(0.9999) within group (order by tstz2 - days1 ) from perctint group by c  order by c limit 10;

select c, percentile_cont(0.9999) within group (order by tstz2 + interval '2 hours 3 minutes 10 secs' ) from perctint group by c  order by c limit 10;

select c, percentile_cont(0.9999) within group (order by tstz2 - interval '1 hour' ) from perctint group by c  order by c limit 10;

select c, percentile_cont(0.9999) within group (order by tstz2 + time '03:00' ) from perctint group by c  order by c limit 10;

select c, percentile_cont(0.9999) within group (order by tstz2 - time '10:11:26' ) from perctint group by c  order by c limit 10;

select c , median( tstz2 - tstz1 ) from perctint group by c order by c limit 10 ;

select c, percentile_cont(0.9999) within group (order by ts2::timestamptz -ts1:: timestamptz) from perctint group by c order by 2 limit 10;

-- DATE

-- The test frame work now does not sort the output of
-- `select ... order by` statement. This is not enough.
-- Because there are tuples with the same order-by key
-- but different other cols. Their order is important
-- to the correctness of the case. So we add more order-by
-- keys here.
select * from (select c, percentile_cont(0.9999) within group (order by ts2::date -ts1::date) from perctint group by c order by 2 limit 10) r order by 2,1;

select c, percentile_cont(0.9999) within group (order by ts2::date + integer '10' ) from perctint group by c order by 2 limit 10;

select c, percentile_cont(0.9999) within group (order by ts2::date - integer '07' ) from perctint group by c order by 2 limit 10;

select c, percentile_cont(0.9999) within group (order by ts2::date + days2 ) from perctint group by c order by 2 limit 10;

select c, percentile_cont(0.9999) within group (order by ts2::date - days1 ) from perctint group by c order by 2 limit 10;

select median(ts2::date + interval '2 hours 10 minutes' ), percentile_cont(0.9999) within group (order by ts2::date + time '03:00' )    from perctint;

select c, percentile_cont(0.9999) within group (order by ts2::date - time '10:11:26' ) from perctint group by c order by 2 limit 10;

select c,median(ts2::date -ts1::date) from perctint group by c order by c limit 10;

-- TIMESTAMP

select c, percentile_cont(0.9999) within group (order by ts2::timestamp - ts1::timestamp) from perctint group by c  order by c limit 10;

select c, percentile_cont(0.9999) within group (order by ts2::timestamp + days1 ) from perctint group by c  order by c limit 10;

select c, percentile_cont(0.9999) within group (order by ts2::timestamp - days1 ) from perctint group by c  order by c limit 10;

select c, percentile_cont(0.9999) within group (order by ts2::timestamp + interval '2 hours 3 minutes 10 secs' ) from perctint group by c  order by c limit 10;

select c, percentile_cont(0.9999) within group (order by ts2::timestamp - interval '1 hour' ) from perctint group by c  order by c limit 10;

select median(ts2::timestamp + time '   12:00'), percentile_cont(0.9999) within group (order by ts2::timestamp + time '03:00' ) from perctint ;

select c, percentile_cont(0.9999) within group (order by ts2::timestamp - time '10:11:26' ) from perctint group by c  order by c limit 10;

-- TIME

select median(  time '01:00' + interval '3 hours') ;

select percentile_cont(0.77) within group ( order by time '01:00' + interval '3 hours') ;

select c, percentile_cont(0.9999) within group (order by time '11:11' + days2 ) from perctint group by c order by c limit 10;

-- interval

select median(- interval '23 hours');

select median(interval '1 hour' / double precision '1.5');

select c, percentile_cont(0.9999) within group (order by days1 -days2 ) from perctint group by c order by c limit 10;

select c, percentile_cont(0.9999) within group (order by ((days1 -days2) / double precision '1.75')) from perctint group by c order by c limit 10;

select c, percentile_cont(0.9999) within group (order by ((days1 + days2) * 1.2) ) from perctint group by c order by c limit 10;

--numeric types

select b, percentile_cont(0.9876) within group( order by c::numeric - 2.8765::numeric) from perctnum group by b order by b limit 10;

select median( c::numeric + (0.2*0.99):: numeric) from perctnum;

select percentile_cont(1.00) within group( order by b::float8 + (110 / 13)::float8) from perctnum; 

--SQL with <> operator with IDF in HAVING clause

select count(*),median(a) from perct group by b having median(b) <> 33 order by median(a);

--SQL with multiple IDF conditions in HAVING clause

select a, avg(b),percentile_cont(0.4) within group (order by a)  from perct group by a having percentile_cont(0.4) within group (order by a)  > 1 and percentile_cont(0.4) within group( order by a ) < 10  order by a;

--SQL with multiple IDF conditions in HAVING clause (including median)

select a, avg(b)  from perct group by a having percentile_cont(0.4) within group (order by a)  > median(b)  order by a desc limit 10;

--SQL with IDF and not in condition in HAVING clause

select  median(b)  from perct group by a having median(a) not in (select avg(b) from perct4) order by median(b) desc limit 10;

--SQL with multiple IDF and not in condition in HAVING clause

select count(*), median(b)  from perct group by a having median(a) not in ( select b from perct4 group by b having median(b) < 5 and percentile_disc(0.9) within group(order by a) > 3) order by median(b) limit 10;

-- SQL with IDF and aggregate func and Over clause

select variance(a) over(partition by median(b))from perct group by a,b order by b limit 10;

-- SQL with IDF in aggregate function - MPP-16862

select median( median(a)) from perct;
select median(percentile_cont(0.5) within group (order by a)) from perct;
select a, percentile_disc(0.1234567890)  within group ( order by avg(a)) from perct group by a;
select a,median( ( count (*) )) from perct group by 1 limit 2;

-- SQL with IDF with SRF

select gendates( '1992-01-01', '1998-08-02', 10 ), median('1day'::interval);

-- SQL with IDF and OVER


-- SQL with IDF and Windows function



-- SQL with IDF on JOIN condition


-- SQL with nested IDF

select sum(median(a))  from perct ;
select median( median(0.9) ) ;
select median( percentile_cont(0.2) within group( ORDER by 1)) ;

-- SQL with IDF and Grouping Sets

select a,median(b) from perct4 group by median(c);


-- SQL with Outer aggregate reference and IDF

select ( (select sum(a) from perct group by b having median(t.a) < 5 limit 1 ) ) from perct t;

select ( select percentile_cont(0.6) within group (order by t.a) from perct where a <10 limit 1 )  from perct t;

-- SQL with Group by () and IDF
select a,median(b) from perct group by ();
select a,percentile_cont(0.7) within group(order by b) from perct group by ();

-- SQL with Cube and IDF

-- SQL with GroupingSets and IDF
select percentile_cont(0.7) within group (order by a) from perct4 group by grouping sets((b),(c), ());
select median(a) from perct group by grouping sets((), (b));

-- SQL with grouping and IDF
select a, median(b) , grouping(c) , grouping(a) from perct4 group by grouping sets((a,c),a,c,());

-- SQL with group_id and IDF
select median(group_id()) from perct group by a,b;

-- SQL with IDF and windows func : ERROR

select percentile_cont(0.2) within group (order by median(a) over()) from perct;

-- SQL with IDF , aggregate func and over clause within IDF : ERROR

select percentile_cont(0.2) within group (order by stddev(b) over() ) from perct;
select median(avg(a) over()) from perct;

-- SQL with math expression as input to median

select b+1 as col1 ,median(a+b) from perct group by b order by b desc;

select b^2, median((select median(a) from perct) - (select sum(a)/10+ median(a) from perct ) + b + 500) from perct group by b order by b desc ;

-- PERCENTILE FUNCTION: SQL with math expression as input to IDF

select b, percentile_disc(8*9/100 % 10 + 0.1::int) within group (order by a) from perct group by b order by b;

-- SQL with IDF AND ORDER BY constant value

select percentile_disc(0.05::int)  within group (order by a) from perct;

select percentile_disc(0.05::text) within group (order by a) from perct;

-- start_ignore
CREATE SEQUENCE serial START 101;
-- end_ignore

select percentile_cont(0.5) within group (order by NEXTVAL('SERIAL')) from perct;

-- SQL with IDF and Date/Time Functions and Operators : Functions and Other operations

select median(double precision '4.95' * interval '1 hour');
select median('19990101'::date);
select median('19990101'::timestamp);
select median('19990101'::timestamptz);
select median (( EXTRACT(microseconds FROM TIMESTAMP '2012-04-04 14:54:37.843901-07')));

-- SQL contains IDF and group by ()
select a,percentile_cont(0.9) within group (order by b) from perct4 group by a,() order by a limit 10;
select a,median(b) from perct group by a,() order by a limit 10;
select a, median(b) from perct4 GROUP BY GROUPING SETS((a)) order by a limit 10;
select percentile_cont(0.7) within group (order by a) from perct group by grouping sets((b)) order by 1;
select DISTINCT percentile_cont(0.7) within group (order by a) from perct group by grouping sets((b), (b)) order by 1;

-- VIEW with IDF and its definition -- Median and subquery
-- start_ignore
create view idf_v1 as select median (( select median((select median(a) from perct)) from perct ));
-- end_ignore
select pg_get_viewdef('idf_v1');  

-- VIEW with IDF and its definition -- Percentile function with wrong input
-- start_ignore
create view idf_v4 as select percentile_disc(1.5) within group (order by a) as percentile_disc_a, percentile_disc( 0.9) within group (order by b) as percentile_disc_b  from perct;
-- end_ignore
-- Expected Error : input is out of range
select * from idf_v4;

-- SQL with IDF and distinct

select distinct(median(b)) from perct group by a;

-- SQL with IDF and distinct on

select distinct on (median(a)) median(a) ,b from perct group by b order by median(a),b;

-- start_ignore
drop schema qp_idf cascade;
-- end_ignore
