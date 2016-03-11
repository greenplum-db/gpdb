-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

RESET ALL;
-- start_ignore
create schema qp_idf;
set search_path to qp_idf;

create language plpythonu;

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

SET datestyle = "ISO, DMY";
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

select c, percentile_cont(0.9999) within group (order by ts2::date -ts1::date) from perctint group by c order by 2 limit 10;

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
