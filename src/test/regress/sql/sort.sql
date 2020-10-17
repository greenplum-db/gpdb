create schema sort_schema;
set search_path to sort_schema;
 
-- start_ignore
create language plpython3u;
-- end_ignore
 
-- Check if analyze output has Sort Method
 create or replace function sort_schema.has_sortmethod(explain_analyze_query text)
 returns setof int as
 $$
 rv = plpy.execute(explain_analyze_query)
 search_text = 'Sort Method'
 result = []
 for i in range(len(rv)):
     cur_line = rv[i]['QUERY PLAN']
     if search_text.lower() in cur_line.lower():
         result.append(1)
 return result
 $$
 language plpython3u;
 
 select sort_schema.has_sortmethod('explain analyze select * from generate_series(1, 100) g order by g limit 100;');
 
 select sort_schema.has_sortmethod('explain analyze select * from generate_series(1, 100) g order by g;');
 
 select sort_schema.has_sortmethod('explain analyze select * from generate_series(1, 100) g order by g limit 100;');
 
 select sort_schema.has_sortmethod('explain analyze select * from generate_series(1, 100) g order by g;');
 
 create table sort_a(i int, j int);
 insert into sort_a values(1, 2);
 
 select sort_schema.has_sortmethod('explain analyze select i from sort_a order by i;');
 
 select sort_schema.has_sortmethod('explain analyze select i from sort_a order by i;');

create table gpsort_alltypes(dist_col int, col1 bigint, col2 bigserial, col3 bit, col4 bit varying(10), col5 bool, col6 char, col7 varchar(10), col8 cidr, col9 circle, col10 date, col11 numeric(5,2), col12 float8, col13 inet, col14 int4, col15 interval, col16 lseg, col17 macaddr, col18 money, col19 path, col20 point, col21 polygon, col22 float4, col23 serial4, col24 smallint, col25 text, col26 time, col27 timetz, col28 timestamp, col29 timestamptz) distributed by (dist_col);

insert into gpsort_alltypes values(1, 1234567891011,13942492347494,'1','0001','yes', 'a', 'abcdefgh', '192.168.100.1', circle '((0,0),1)', '2007-01-01', 123.45, 12323423424.324, inet '192.168.1.1', 123123, interval '24 hours',lseg '((0,0),(1,1))', macaddr '12:34:56:78:90:ab', '$1000.00',  path '((0,0),(1,1),(2,2))', point '(0,0)',polygon '((0,0),(1,1))',213234.23,1,7123,'abcdsafasfasfasfasfdasfasfdasf2asdfhsadfsfs','04:00:00','04:00:00 EST','2007-01-01 04:00:00','2007-01-01 04:00:00 EST');
insert into gpsort_alltypes values(1, 10987654321,212394723492342,'0','0010','y', 'b', 'xyz', '192.168.100.2', circle '((0,0),2)', '2007-02-01', 23.45, 1232324.324, inet '192.168.1.2', 123124, interval '12 hours',lseg '((0,0),(1,2))', macaddr '12:34:56:78:90:00', '$5000.00',  path '((0,0),(4,4),(3,3))', point '(0,1)',polygon '((-1,-2),(1,1))',213.23234,2,2343,'2342abcddfasfasf234234234','04:30:00','04:30:00 EST','2007-02-01 04:30:00','2007-02-01 04:30:00 EST');
insert into gpsort_alltypes values(1, 122223333333366,423402340240234,'1','0100','f', 'd', '1xyz', '192.168.100.10', circle '((2,1),2)', '2001-03-02', 34.45, 312324.324, inet '192.168.2.5', 1324, interval '10 secs',lseg '((1,1),(6,6))', macaddr '12:34:56:78:89:cd', '$1.50',  path '((0,0),(4,4),(3,3),(5,5))', point '(2,1)',polygon '((2,0),(2,1))',21312121.23,4,123,'abcd23423afasflasfasf','16:30:00','16:30:00 PST','2006-02-01 16:30:00','2006-02-01 16:30:00 PST');
insert into gpsort_alltypes values(1, 99999999999999999,312394234,'1','0000','false', 'c', 'cde', '192.168.100.3', circle '((1,1),2)', '2007-02-02', 34.45, 122324.324, inet '192.168.1.5', 13124, interval '30 mins',lseg '((0,0),(5,6))', macaddr '12:34:56:78:90:cd', '$4321.00',  path '((0,0),(4,4),(3,3))', point '(1,1)',polygon '((1,0),(2,3))',2133459.23123,3,1323,'234234abcddasdflasjflasfjalf','14:30:00','14:30:00 PST','2007-02-01 14:30:00','2007-02-01 14:30:00 PST');

select col1 from gpsort_alltypes order by col1 asc;
select col1 from gpsort_alltypes order by col1 desc;
select col2 from gpsort_alltypes order by col2 asc;
select col2 from gpsort_alltypes order by col2 desc;
select col3 from gpsort_alltypes order by col3 asc;
select col3 from gpsort_alltypes order by col3 desc;
select col4 from gpsort_alltypes order by col4 asc;
select col4 from gpsort_alltypes order by col4 desc;
select col5 from gpsort_alltypes order by col5 asc;
select col5 from gpsort_alltypes order by col5 desc;
select col6 from gpsort_alltypes order by col6 asc;
select col6 from gpsort_alltypes order by col6 desc;
select col7 from gpsort_alltypes order by col7 asc;
select col7 from gpsort_alltypes order by col7 desc;
select col8 from gpsort_alltypes order by col8 asc;
select col8 from gpsort_alltypes order by col8 desc;
select col9 from gpsort_alltypes order by col9 asc;
select col9 from gpsort_alltypes order by col9 desc;
select col10 from gpsort_alltypes order by col10 asc;
select col10 from gpsort_alltypes order by col10 desc;
select col11 from gpsort_alltypes order by col11 asc;
select col11 from gpsort_alltypes order by col11 desc;
select col12 from gpsort_alltypes order by col12 asc;
select col12 from gpsort_alltypes order by col12 desc;
select col13 from gpsort_alltypes order by col13 asc;
select col13 from gpsort_alltypes order by col13 desc;
select col14 from gpsort_alltypes order by col14 asc;
select col14 from gpsort_alltypes order by col14 desc;
select col15 from gpsort_alltypes order by col15 asc;
select col15 from gpsort_alltypes order by col15 desc;
select col16 from gpsort_alltypes order by col16 asc;
select col16 from gpsort_alltypes order by col16 desc;
select col17 from gpsort_alltypes order by col17 asc;
select col17 from gpsort_alltypes order by col17 desc;
select col18 from gpsort_alltypes order by col18 asc;
select col18 from gpsort_alltypes order by col18 desc;
select col19 from gpsort_alltypes order by col19 asc;
select col19 from gpsort_alltypes order by col19 desc;
select col20 from gpsort_alltypes order by col20 asc;
select col20 from gpsort_alltypes order by col20 desc;
select col21 from gpsort_alltypes order by col21 asc;
select col21 from gpsort_alltypes order by col21 desc;
select col22 from gpsort_alltypes order by col22 asc;
select col22 from gpsort_alltypes order by col22 desc;
select col23 from gpsort_alltypes order by col23 asc;
select col23 from gpsort_alltypes order by col23 desc;
select col24 from gpsort_alltypes order by col24 asc;
select col24 from gpsort_alltypes order by col24 desc;
select col25 from gpsort_alltypes order by col25 asc;
select col25 from gpsort_alltypes order by col25 desc;
select col26 from gpsort_alltypes order by col26 asc;
select col26 from gpsort_alltypes order by col26 desc;
select col27 from gpsort_alltypes order by col27 asc;
select col27 from gpsort_alltypes order by col27 desc;
select col28 from gpsort_alltypes order by col28 asc;
select col28 from gpsort_alltypes order by col28 desc;
select col29 from gpsort_alltypes order by col29 asc;
select col29 from gpsort_alltypes order by col29 desc;

select col1, col2, col3, col4, col5 from gpsort_alltypes order by col1, col2, col3, col4, col5;
select col1, col2, col3, col4, col5 from gpsort_alltypes order by col3 desc, col2 asc, col1, col4, col5;
select col1, col2, col3, col4, col5 from gpsort_alltypes order by col5 desc, col3 asc, col2 desc, col4 asc, col1 desc;


--
-- Test Motion node preserving sort order. With collations and NULLS FIRST/LAST
--
create table colltest (t text);
insert into colltest VALUES ('a'), ('A'), ('b'), ('B'), ('c'), ('C'), ('d'), ('D'), (NULL);

select * from colltest order by t COLLATE "C";
select * from colltest order by t COLLATE "C" NULLS FIRST;


--
-- Test strxfrm()/strcoll() sort order inconsistency in a
-- merge join with russian characters and default collation
--
set enable_hashjoin = off;

with t as (
    select * from (values ('б б'), ('бб ')) as t1(b)
    full join (values ('б б'), ('бб ')) as t2(b)
    using (b)
)
select count(*) from t;

reset enable_hashjoin;
