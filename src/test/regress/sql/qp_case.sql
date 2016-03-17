
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_case;
set search_path to qp_case;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: test01_case_noelse.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop table if exists genders;
-- end_ignore

select CASE 'M'
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    END;
select CASE 'F'
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    END;
select CASE ''
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    END;
select CASE null
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    END;

create table genders (gid integer, gender char(1)) distributed by (gid);

insert into genders(gid, gender) values (1, 'F');
insert into genders(gid, gender) values (2, 'M');
insert into genders(gid, gender) values (3, 'Z');
insert into genders(gid, gender) values (4, '');
insert into genders(gid, gender) values (5, null);
insert into genders(gid, gender) values (6, 'G');

select gender, CASE gender
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    END
from genders
order by gid;

-- ----------------------------------------------------------------------
-- Test: test02_case_withelse.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop table if exists genders;
-- end_ignore

select CASE 'M'
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    ELSE 'Other' END;

select CASE null
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    ELSE 'Other' END;

create table genders (gid integer, gender char(1)) distributed by (gid);

insert into genders(gid, gender) values (1, 'F');
insert into genders(gid, gender) values (2, 'M');
insert into genders(gid, gender) values (3, 'Z');
insert into genders(gid, gender) values (4, '');
insert into genders(gid, gender) values (5, null);
insert into genders(gid, gender) values (6, 'G');

select gender, CASE gender
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    ELSE 'Other' END
from genders
order by gid;

-- ----------------------------------------------------------------------
-- Test: test03_case_combined_when_match_found.sql
-- ----------------------------------------------------------------------

select 'a' as lhs, CASE 'a'
       WHEN 'f' THEN 'WHEN: f'
       WHEN IS NOT DISTINCT FROM 'f' THEN 'WHEN NEW: f'
       WHEN 'e' THEN 'WHEN: e'
       WHEN IS NOT DISTINCT FROM 'e' THEN 'WHEN NEW: e'
       WHEN 'd' THEN 'WHEN: d'
       WHEN IS NOT DISTINCT FROM 'd' THEN 'WHEN NEW: d'
       WHEN 'c' THEN 'WHEN: c'
       WHEN IS NOT DISTINCT FROM 'c' THEN 'WHEN NEW: c'
       WHEN 'b' THEN 'WHEN: b'
       WHEN IS NOT DISTINCT FROM 'b' THEN 'WHEN NEW: b'
       WHEN 'a' THEN 'WHEN: a'
       WHEN IS NOT DISTINCT FROM 'a' THEN 'WHEN NEW: a'
    ELSE 'NO MATCH' END as match; 


select 1 as lhs, CASE 1
       WHEN 4 THEN 'WHEN: 4'
       WHEN IS NOT DISTINCT FROM 4 THEN 'WHEN NEW: 4'
       WHEN 3 THEN 'WHEN: 3'
       WHEN IS NOT DISTINCT FROM 3 THEN 'WHEN NEW: 3'
       WHEN 2 THEN 'WHEN: 2'
       WHEN IS NOT DISTINCT FROM 2 THEN 'WHEN NEW: 2'
       WHEN 11 THEN 'WHEN: 11'
       WHEN IS NOT DISTINCT FROM 1 THEN 'WHEN NEW: 1'
    ELSE 'NO MATCH' END as match; 

select '2011-05-27'::date as lsh,  CASE '2011-05-27'::date
       WHEN '2011-07-25'::date THEN 'WHEN: 2011-07-25'
       WHEN IS NOT DISTINCT FROM '2011-05-27'::date THEN 'WHEN NEW: 2011-05-27'
    END as match; 




-- ----------------------------------------------------------------------
-- Test: test04_case_combined_when_nomatch_found.sql
-- ----------------------------------------------------------------------

-- start_ignore 
drop table if exists nomatch_case;
-- end_ignore

create table nomatch_case
(
   sid integer, 
   gender char(1) default 'F',
   name text,
   start_dt date
) distributed by (sid);

insert into nomatch_case(sid, gender, name, start_dt)
values(1000, 'F', 'Jane Doe', '2011-01-15'::date);
insert into nomatch_case(sid, gender, name, start_dt)
values(2000, 'M', 'Ryan Goesling', '2011-02-01'::date);
insert into nomatch_case(sid, gender, name, start_dt)
values(3000, 'M', 'Tim Tebow', '2011-01-15'::date);
insert into nomatch_case(sid, gender, name, start_dt)
values(4000, 'F', 'Katy Perry', '2011-03-01'::date);
insert into nomatch_case(sid, gender, name, start_dt)
values(5000, 'F', 'Michael Scott', '2011-02-01'::date);


select sid,
       name,
       gender,
       start_dt,
       CASE upper(gender)
          WHEN 'MALE' THEN 'M'
          WHEN IS NOT DISTINCT FROM 'FEMALE' THEN 'F'
          WHEN trim('MALE ') THEN 'M'
       ELSE 'NO MATCH' END as case_gender
from nomatch_case
order by sid, name; 

select sid,
       name,
       gender,
       start_dt,
       CASE start_dt
           WHEN IS NOT DISTINCT FROM '2009-01-01'::date THEN 2009
           WHEN '2008-01-01'::date THEN 2008
           WHEN IS NOT DISTINCT FROM '2010-01-01'::date then 2010
           WHEN 2007 THEN 2007
           WHEN IS NOT DISTINCT FROM 2007 THEN 2007
           WHEN '2006-01-01'::date then 2006
       END as case_start_dt
from nomatch_case
order by sid, name;

select sid,
       name,
       gender,
       start_dt,
       CASE sid
           WHEN 100 THEN 'Dept 10' 
           WHEN 200 THEN 'Dept 20' 
           WHEN IS NOT DISTINCT FROM 300 then 'Dept 30'
           WHEN 400 THEN 'Dept 40'
           WHEN 500 THEN 'Dept 50'
           WHEN IS NOT DISTINCT FROM 600 then 'Dept 60'
           WHEN IS NOT DISTINCT FROM 700 then 'Dept 70'
       END as case_sid
from nomatch_case
order by sid, name;


-- ----------------------------------------------------------------------
-- Test: test05_case_combined_when_in_aggr.sql
-- ----------------------------------------------------------------------

-- start_ignore 
drop table if exists combined_when;
-- end_ignore

create table combined_when 
(
   sid integer, 
   gender varchar(10) default 'F',
   name text,
   start_dt date
) distributed by (sid);

insert into combined_when(sid, gender, name, start_dt)
values(1000, 'F', 'Jane Doe', '2011-01-15'::date);
insert into combined_when(sid, gender, name, start_dt)
values(2000, 'M', 'Ryan Goesling', '2011-02-01'::date);
insert into combined_when(sid, gender, name, start_dt)
values(3000, 'm', 'Tim Tebow', '2007-01-15'::date);
insert into combined_when(sid, gender, name, start_dt)
values(4000, 'F', 'Katy Perry', '2011-03-01'::date);
insert into combined_when(sid, gender, name, start_dt)
values(5000, 'f', 'Michael Scott', '2011-02-01'::date);
insert into combined_when(sid, gender, name, start_dt)
values(6000, 'Female  ', 'Mila Kunis', '2011-02-01'::date);
insert into combined_when(sid, gender, name, start_dt)
values(7000, ' Male ', 'Tom Brady', '2011-03-01'::date);
insert into combined_when(sid, gender, name, start_dt)
values(8000,  ' ', 'Lady Gaga', '2008-01-15'::date);
insert into combined_when(sid, gender, name, start_dt)
values(9000,  null, 'George Michael', '2011-01-15'::date);
insert into combined_when(sid, gender, name, start_dt)
values(10000,  'Male   ', 'Michael Jordan', null);

select case_yr_start_dt, count(sid)
from (select sid,
       name,
       gender,
       start_dt,
       CASE extract(year from start_dt)
           WHEN IS NOT DISTINCT FROM 2009 THEN 2009
           WHEN abs(2009-1) THEN 2008
           WHEN IS NOT DISTINCT FROM extract(year from '2010-01-01'::date) then 2010
           WHEN round(2007.05, 0) THEN 2007
           WHEN IS NOT DISTINCT FROM 2007 THEN 2007
           WHEN extract(year from '2006-01-01'::date) then 2006
           WHEN extract(year from '2011-01-01'::date) then 2011
       END as case_yr_start_dt
from combined_when
order by sid, name) a
group by case_yr_start_dt
order by 2 desc, 1;


-- ----------------------------------------------------------------------
-- Test: test06_case_combined_when_expr.sql
-- ----------------------------------------------------------------------

-- start_ignore 
drop table if exists case_expr;
-- end_ignore

create table case_expr
(
   sid integer, 
   gender char(1) default 'F',
   name text,
   start_dt date
) distributed by (sid);

insert into case_expr(sid, gender, name, start_dt)
values(1000, 'F', 'Jane Doe', '2011-01-15'::date);
insert into case_expr(sid, gender, name, start_dt)
values(2000, 'M', 'Ryan Goesling', '2011-02-01'::date);
insert into case_expr(sid, gender, name, start_dt)
values(3000, 'M', 'Tim Tebow', '2011-01-15'::date);
insert into case_expr(sid, gender, name, start_dt)
values(4000, 'F', 'Katy Perry', '2011-03-01'::date);
insert into case_expr(sid, gender, name, start_dt)
values(5000, 'F', 'Michael Scott', '2011-02-01'::date);


select sid,
       name,
       gender,
       start_dt,
       CASE (gender is not null)
          WHEN (gender = 'MALE') THEN 'M'
          WHEN IS NOT DISTINCT FROM (gender = 'FEMALE') THEN 'F'
          WHEN (gender = trim('MALE ')) THEN 'M'
          WHEN IS NOT DISTINCT FROM (gender = 'M') THEN 'M'
          WHEN (gender = 'F') THEN 'F'
       ELSE 'NO MATCH' END as case_gender
from case_expr
order by sid, name; 

select sid,
       name,
       gender,
       start_dt,
       CASE (extract(year from start_dt) = 2011)
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 1) THEN 'January'
           WHEN (extract(month from start_dt) = 2) THEN 'February'
           WHEN (extract(month from start_dt) = 3) THEN 'March'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 4) THEN 'April'
           WHEN (extract(month from start_dt) = 5) THEN 'May'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 6) THEN 'June'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 7) THEN 'July'
           WHEN (extract(month from start_dt) = 8) THEN 'August'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 9) THEN 'September'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 10) THEN 'October'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 11) THEN 'November'
           WHEN (extract(month from start_dt) = 12) THEN 'December'
       END as case_start_month
from case_expr
order by sid, name;

select sid,
       name,
       gender,
       start_dt,
       CASE (sid > 100)
           WHEN (sid = 1000) THEN 'Dept 10' 
           WHEN (sid > 1000 and sid <= 2000) THEN 'Dept 20' 
           WHEN IS NOT DISTINCT FROM (sid = 3000) then 'Dept 30'
           WHEN (sid = 4000) THEN 'Dept 40'
           WHEN (sid = 5000) THEN 'Dept 50'
           WHEN IS NOT DISTINCT FROM (sid > 5000 and sid <= 6000) then 'Dept 60'
           WHEN IS NOT DISTINCT FROM (sid = 7000) then 'Dept 70'
       END as case_sid
from case_expr
order by sid, name;


-- ----------------------------------------------------------------------
-- Test: test07_case_combined_when_functions.sql
-- ----------------------------------------------------------------------

-- start_ignore 
drop table if exists combined_when;
-- end_ignore

create table combined_when 
(
   sid integer, 
   gender varchar(10) default 'F',
   name text,
   start_dt date
) distributed by (sid);

insert into combined_when(sid, gender, name, start_dt)
values(1000, 'F', 'Jane Doe', '2011-01-15'::date);
insert into combined_when(sid, gender, name, start_dt)
values(2000, 'M', 'Ryan Goesling', '2011-02-01'::date);
insert into combined_when(sid, gender, name, start_dt)
values(3000, 'm', 'Tim Tebow', '2007-01-15'::date);
insert into combined_when(sid, gender, name, start_dt)
values(4000, 'F', 'Katy Perry', '2011-03-01'::date);
insert into combined_when(sid, gender, name, start_dt)
values(5000, 'f', 'Michael Scott', '2011-02-01'::date);
insert into combined_when(sid, gender, name, start_dt)
values(6000, 'Female  ', 'Mila Kunis', '2011-02-01'::date);
insert into combined_when(sid, gender, name, start_dt)
values(7000, ' Male ', 'Tom Brady', '2011-03-01'::date);
insert into combined_when(sid, gender, name, start_dt)
values(8000,  ' ', 'Lady Gaga', '2008-01-15'::date);
insert into combined_when(sid, gender, name, start_dt)
values(9000,  null, 'George Michael', '2011-01-15'::date);
insert into combined_when(sid, gender, name, start_dt)
values(10000,  'Male   ', 'Michael Jordan', null);


select sid,
       name,
       gender,
       start_dt,
       CASE upper(trim(gender))
          WHEN 'MALE' THEN 'M'
          WHEN IS NOT DISTINCT FROM 'FEMALE' THEN 'F'
          WHEN trim('MALE ') THEN 'M'
          WHEN trim(' FEMALE ') THEN 'F'
          WHEN IS NOT DISTINCT FROM 'M' THEN 'M'
          WHEN IS NOT DISTINCT FROM 'F' THEN 'F'
       ELSE 'NO MATCH' END as case_gender
from combined_when
order by sid, name; 

select sid,
       name,
       gender,
       start_dt,
       CASE extract(year from start_dt)
           WHEN IS NOT DISTINCT FROM 2009 THEN 2009
           WHEN abs(2009-1) THEN 2008
           WHEN IS NOT DISTINCT FROM extract(year from '2010-01-01'::date) then 2010
           WHEN round(2007.05, 0) THEN 2007
           WHEN IS NOT DISTINCT FROM 2007 THEN 2007
           WHEN extract(year from '2006-01-01'::date) then 2006
           WHEN extract(year from '2011-01-01'::date) then 2011
       END as case_yr_start_dt
from combined_when
order by sid, name;

select case_yr_start_dt, count(sid)
from (select sid,
       name,
       gender,
       start_dt,
       CASE extract(year from start_dt)
           WHEN IS NOT DISTINCT FROM 2009 THEN 2009
           WHEN abs(2009-1) THEN 2008
           WHEN IS NOT DISTINCT FROM extract(year from '2010-01-01'::date) then 2010
           WHEN round(2007.05, 0) THEN 2007
           WHEN IS NOT DISTINCT FROM 2007 THEN 2007
           WHEN extract(year from '2006-01-01'::date) then 2006
           WHEN extract(year from '2011-01-01'::date) then 2011
       END as case_yr_start_dt
from combined_when
order by sid, name) a
group by case_yr_start_dt
order by 2 desc, 1;


-- ----------------------------------------------------------------------
-- Test: test08_case_negative_syntax_err.sql
-- ----------------------------------------------------------------------

select CASE 'a'
       WHEN IS NOT DISTINCT FROM 'b' THEN 'a=b'
       WHEN NOT DISTINCT FROM 'a' THEN 'a=a'
END;

select CASE 'a'
       WHEN IS NOT DISTINCT 'b' THEN 'a=b'
       WHEN IS NOT DISTINCT FROM 'a' THEN 'a=a'
END;

select CASE 'a'
       WHEN IS NOT DISTINCT FROM 'b' THEN 'a=b'
       WHEN IS NOT DISTINCT FROM 'a' IS 'a=a'
END;

select CASE 'a'
       WHEN IS NOT DISTINCT FROM 'b' IS 'a=b'
       WHEN IS NOT DISTINCT FROM 'a' THEN 'a=a'
END;


-- ----------------------------------------------------------------------
-- Test: test09_case_negative_unsupported.sql
-- ----------------------------------------------------------------------

select CASE 1
       WHEN IS NOT DISTINCT FROM 2 THEN '1=2'
       WHEN IS DISTINCT FROM 2 THEN '1<>2'
       WHEN IS NOT DISTINCT FROM 1 THEN '1=1'
END;

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_case cascade;
-- end_ignore
