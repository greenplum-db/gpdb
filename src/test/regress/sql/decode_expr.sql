-- Test Oracle-style DECODE() expressions.

begin;
create table decodeint(a int, b int) distributed by (a);

insert into decodeint values
  (0,0),
  (1,1),
  (2,2),
  (3,3),
  (4,4),
  (5,5),
  (6,6),
  (null,1),
  (1,1),
  (2,1),
  (3,1),
  (4,1),
  (5,1),
  (6,1);
commit;

select a, decode(a, 1, 'A', 2, 'B', 3, 'C', 4, 'D', 5, 'E') as decode from decodeint order by a, b;
select a, decode(a, 1, 'A', 2, 'B', 3, 'C', 4, 'D', 5, 'E', 'Z') as decode from decodeint order by a, b;
select a, decode(a, 10, 'J', 11, 'K', 12, 'L', 13, 'M', 14, 'N', 15, 'O', 16, 'P') as decode_nomatch
  from decodeint order by a, b;
select a, decode(a, 10, 'J', 11, 'K', 12, 'L', 13, 'M', 14, 'N', 15, 'O', 16, 'P', 'Z') as decode_nomatch_def
  from decodeint order by a, b;

begin;
CREATE TABLE decodenum1 (
  numcol numeric(6, 3),
  distcol int,
  ptcol int,
  name text
)
distributed by (distcol) partition by range (ptcol)
(
  default partition defaultpart,
  start (0) inclusive
  end (501) exclusive
  every (100)
);

insert into decodenum1 values
  (1.1, 100, 0, 'part0'),
  (10.10, 100, 10, 'part1'),
  (10.10, 200, 200, 'part2'),
  (20.22, 200, 200, 'part2'),
  (20.22, 100, 100, 'part1'),
  (300.333, 300, 300, 'part3'),
  (300.333, 300, 100, 'part1'),
  (300.333, 300, 100, 'part1');
commit;

select numcol, decode(numcol, 300.333, '300+') as "decode(numcol, 300.333, '300+')" from decodenum1 order by numcol, distcol;

begin;
CREATE TABLE decodenum2
(
numcol numeric(6, 3),
distcol int,
ptcol int,
name text
)
distributed by (distcol)
partition by range (ptcol)
(
default partition defaultpart,
start (0) inclusive
end (501) exclusive
every (100)
);

insert into decodenum2 values
  (1.1, 100, 0, 'part0'),
  (2.2, 200, 0, 'part0'),
  (10.10, 100, 10, 'part1'),
  (10.10, 200, 200, 'part2'),
  (20.22, 200, 200, 'part2'),
  (20.22, 100, 100, 'part1'),
  (100.311, 100, 100, 'part3'),
  (100.322, 100, 100, 'part1'),
  (100.333, 200, 200, 'part2');
commit;

select numcol, decode(numcol, 10.10, 'Under 100', 20.22, 'Under 100', 100.311, '100+', 100.322, '100+', 100.333, '100+', 'None') from decodenum2 order by numcol, distcol;

drop table decodenum1;
drop table decodenum2;


-- Test DECODE() with a char(2) column.
begin;
CREATE TABLE decodecharao1 (country_code char(2), region text)
WITH (appendonly=true)
DISTRIBUTED BY (region);

insert into decodecharao1 values
  ('US', 'Americas'),
  ('CA', 'Americas'),
  ('UK', 'Europe'),
  ('FR', 'France');
commit;

select country_code, decode(country_code, 'CA', 'Canada') as decode from decodecharao1 order by country_code, region;

-- Test DECODE() in an INSERT .. SELECT statement.
begin;
CREATE TABLE regions(country_code char(2), region text) DISTRIBUTED BY (region);

CREATE TABLE decodecharao2
  (country_code char(2), country_name varchar(255), region text)
WITH (appendonly=true) DISTRIBUTED BY (region);

insert into regions(country_code, region) values
  ('JP', 'Asia'),
  ('US', 'Americas'),
  ('CA', 'Americas'),
  ('FR', 'Europe'),
  ('UK', 'Europe'),
  ('IT', 'Europe');
commit;

insert into decodecharao2(country_code, country_name, region)
select country_code,
       decode(country_code, 'AU', 'Australia', 'BR', 'Brazil', 'CA', 'Canada', 'CH', 'China', 'FR', 'France', 'IT', 'Italy', 'JP', 'Japan', 'RU', 'Russia', 'SP', 'Spain', 'US', 'United States', 'United States'),
       region
from regions;

select * from decodecharao2 order by region, country_code;

-- Test DECODE() with a varchar field
begin;
create table decodevarchar
(
  dayname varchar(25),
  dayid int
) distributed by (dayid);

insert into decodevarchar values
  ('Monday', 1),
  ('Tuesday', 2),
  ('Wednesday', 3),
  ('Thursday', 4),
  ('Friday', 5),
  ('Saturday', 6),
  ('Sunday', 7);
commit;

select dayname,
       decode(dayname, 'Monday', true, 'Tuesday', true, 'Wednesday', true, 'Thursday', true, 'Friday', true, 'Saturday', false, 'Sunday', false) as is_workday
from decodevarchar order by dayid;
select dayname,
       decode(dayname, 'Monday', true, 'Tuesday', true, 'Wednesday', true, 'Thursday', true, 'Friday', true, false) as is_workday
from decodevarchar order by dayid;


begin;
CREATE TABLE employees
(
   empid integer,
   name text,
   gender char(1),
   PRIMARY KEY (empid, gender)
) distributed by (empid)
partition by list (gender)
(
partition women values ('F'),
partition men values ('M'),
default partition other
);

CREATE TABLE emp_start_dates
(
   empid integer,
   startdate date
) distributed by (startdate);

insert into employees(empid, name, gender) values
  (100, 'John Smith', 'M'),
  (101, 'John Deere', 'M'),
  (102, 'Jane Doe', 'F'),
  (103, 'Janet Jackson', 'F'),
  (104, 'Anne Smith', 'F'),
  (105, 'Ryan Goesling', 'M'),
  (106, 'George Clooney', 'M'),
  (107, 'Julia Roberts', 'F'),
  (108, 'Jennifer Aniston', 'F'),
  (109, 'Brad Pitt', 'M');

insert into emp_start_dates(empid, startdate) values
  (100, '2011-01-01'::date),
  (101, '2010-01-15'::date),
  (102, '2011-02-28'::date),
  (103, '2009-02-01'::date),
  (104, '2011-03-15'::date),
  (105, '2011-05-01'::date),
  (106, '2011-05-01'::date),
  (107, '2010-01-15'::date),
  (108, '2011-02-15'::date);
insert into emp_start_dates(empid) values(109);
commit;

CREATE TABLE employees_2011
as
select e.empid as emp_id,
       e.name as emp_name,
       e.gender as gender,
       s.startdate as emp_start_dt,
       decode(s.startdate, '2011-01-01'::date, 1, '2011-01-15'::date, 1, '2011-01-31'::date, 1, '2011-02-01'::date, 2, '2011-02-15'::date, 2, '2011-02-28'::date, 2, '2011-03-01'::date, 3, '2011-03-15'::date, 3, '2011-03-31'::date, 3, '2011-04-01'::date, 4, '2011-04-15'::date, 4, '2011-04-30'::date, 4, '2011-05-01'::date, 5, '2011-05-15'::date, 5, '2011-05-31'::date, 5, '2011-06-01'::date, 6, '2011-06-15'::date, 6, '2011-06-30'::date, 6, 0) as start_mon_2011
from employees e
  join emp_start_dates s on (e.empid = s.empid)
DISTRIBUTED BY (start_mon_2011);

begin;
create table decodetimestamptz
(
    lastlogin  timestamptz,
    username   varchar(25),
    userid     int,
    decodetxt  text
) WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1, blocksize=32768) distributed by (userid);

insert into decodetimestamptz(lastlogin, username, userid) values
  ('2011-12-19 10:30:54 PST'::timestamp with time zone, 'user1', 1000),
  ('2011-12-15 11:20:14 PST'::timestamp with time zone, 'user2', 1010),
  ('2011-12-19 07:25:15 PST'::timestamp with time zone, 'user3', 1020),
  ('2011-12-19 08:20:54 PST'::timestamp with time zone, 'user4', 1030),
  ('2011-12-15 11:20:14 PST'::timestamp with time zone, 'user5', 1040),
  ('2011-12-19 10:30:54 PST'::timestamp with time zone, 'user6', 1050),
  ('2011-12-19 10:30:54 PST'::timestamp with time zone, 'user7', 1060),
  ('2011-12-15 11:20:14 PST'::timestamp with time zone, 'user8', 1070),
  ('2011-12-19 10:30:54 PST'::timestamp with time zone, 'user9', 1080),
  ('2011-01-19 10:30:54 PST'::timestamp with time zone, 'user10', 1090);
commit;

select * from decodetimestamptz order by userid;

select lastlogin,
  decode(lastlogin,
         '2011-12-19 10:30:54 PST'::timestamp with time zone, 'December',
	 '2011-12-15 11:20:14 PST'::timestamp with time zone, 'December',
	 '2011-12-19 07:25:15 PST'::timestamp with time zone, 'December',
	 '2011-12-19 08:20:54 PST'::timestamp with time zone, 'December',
	 '2011-01-19 10:30:54 PST'::timestamp with time zone, 'January',
	 '2011-02-19 10:30:54 PST'::timestamp with time zone, 'February'
	) as decode
from decodetimestamptz
order by lastlogin, userid;

drop table employees;
drop table emp_start_dates;

begin;
CREATE TABLE employees
(
   empid integer,
   name text,
   gender char(1),
   PRIMARY KEY (empid, gender)
) distributed by (empid)
partition by list (gender)
(
partition women values ('F'),
partition men values ('M'),
default partition other
);

CREATE TABLE emp_start_dates
(
   empid integer,
   startdate date
) distributed by (startdate);

insert into employees(empid, name, gender) values
  (100, 'John Smith', 'M'),
  (101, 'John Deere', 'M'),
  (102, 'Jane Doe', 'F'),
  (103, 'Janet Jackson', 'F'),
  (104, 'Anne Smith', 'F'),
  (105, 'Ryan Goesling', 'M'),
  (106, 'George Clooney', 'M'),
  (107, 'Julia Roberts', 'F'),
  (108, 'Jennifer Aniston', 'F');

insert into emp_start_dates(empid, startdate) values
  (100, '2011-01-01'::date),
  (101, '2010-01-15'::date),
  (102, '2011-02-28'::date),
  (103, '2009-02-01'::date),
  (104, '2011-03-15'::date),
  (105, '2011-05-01'::date),
  (106, '2011-05-01'::date),
  (107, '2010-01-15'::date),
  (108, '2011-02-15'::date);
commit;

select e.empid as emp_id,
       e.name as emp_name,
       e.gender as gender,
       s.startdate as emp_start_dt,
       decode((s.startdate >= '2011-01-01'::date), true, 'Y', 'N') as started_2011
from employees e
  join emp_start_dates s on (e.empid = s.empid)
order by s.startdate, e.empid asc;

drop table employees;
drop table emp_start_dates;

--
begin;
CREATE TABLE employees
(
   empid integer,
   name text,
   gender char(1),
   PRIMARY KEY (empid, gender)
) distributed by (empid)
partition by list (gender)
(
partition women values ('F'),
partition men values ('M'),
default partition other
);

CREATE TABLE emp_start_dates
(
   empid integer,
   startdate date
) distributed by (startdate);

insert into employees(empid, name, gender) values
  (100, 'John Smith', 'M'),
  (101, 'John Deere', 'M'),
  (102, 'Jane Doe', 'F'),
  (103, 'Janet Jackson', 'F'),
  (104, 'Anne Smith', 'F'),
  (105, 'Ryan Goesling', 'M'),
  (106, 'George Clooney', 'M'),
  (107, 'Julia Roberts', 'F'),
  (108, 'Jennifer Aniston', 'F'),
  (109, 'Null Startdate', 'M');

insert into emp_start_dates(empid, startdate) values
  (100, '2011-01-01'::date),
  (101, '2010-01-15'::date),
  (102, '2011-02-28'::date),
  (103, '2009-02-01'::date),
  (104, '2011-03-15'::date),
  (105, '2011-05-01'::date),
  (106, '2011-05-01'::date),
  (107, '2010-01-15'::date),
  (108, '2011-02-15'::date),
  (109, null);
commit;

drop table if exists employees;
drop table if exists emp_start_dates;

CREATE TABLE employees
(
   empid integer,
   name text,
   gender char(1),
   PRIMARY KEY (empid, gender)
) distributed by (empid)
partition by list (gender)
(
partition women values ('F'),
partition men values ('M'),
default partition other
);

CREATE TABLE emp_start_dates
(
   empid integer,
   startdate date
) distributed by (startdate);

insert into employees(empid, name, gender) values
  (100, 'John Smith', 'M'),
  (101, 'John Deere', 'M'),
  (102, 'Jane Doe', 'F'),
  (103, 'Janet Jackson', 'F'),
  (104, 'Anne Smith', 'F'),
  (105, 'Ryan Goesling', 'M'),
  (106, 'George Clooney', 'M'),
  (107, 'Julia Roberts', 'F'),
  (108, 'Jennifer Aniston', 'F');

insert into emp_start_dates(empid, startdate) values
  (100, '2012-01-01'::date),
  (101, '2010-01-15'::date),
  (102, '2008-02-28'::date),
  (103, '2009-02-01'::date),
  (104, '2007-03-15'::date),
  (105, '2006-05-01'::date),
  (106, '2011-05-01'::date),
  (107, '2004-01-15'::date),
  (108, '2000-02-15'::date);

select e.empid as emp_id,
       e.name as emp_name,
       e.gender as gender,
       s.startdate as emp_start_dt,
       decode(extract(year from s.startdate), 2012, 2012, 2011, 2011, 2010, 2010, 2009, 2009, 2008, 2008, 2007, 2007, 2006, 2006, 2005) as emp_start_yr
from employees e
  join emp_start_dates s on (e.empid = s.empid)
order by s.startdate, e.empid asc;

CREATE TABLE office1
(
    locid integer,
    company_name varchar(100),
    address1 text,
    address2 text,
    city text,
    state varchar(3),
    zip varchar(9),
    country text,
    phone text
);

create or replace function get_area_code(phone text) returns text as
$$
declare
begin
  if (strpos(phone, '-') < 1) then
     raise exception 'Format for phone must be xxx-xxx-xxxx';
  end if;
  return split_part(phone, '-', 1);
end;
$$ language plpgsql NO SQL;

insert into office1(locid, company_name, address1, address2, city, state, zip, country, phone) values
 (100, 'Greenplum', '1900 S. Norfolk St.', 'Suite 125', 'San Mateo', 'CA', '94403', 'USA', '650-111-1111'),
 (200, 'Dairy Farm', '100 Meadow Lane', null, 'Stockton', 'CA', '95210', 'USA', '209-222-2000'),
 (300, 'Not Just a Database', '1900 S. Norfolk St.', 'Suite 224', 'San Mateo', 'CA', '94403', 'USA', '650-111-3000'),
 (400, 'EMC Corporation', '176 South Street', null, 'Santa Clara', 'CA', '95123', 'USA', '408-444-4000'),
 (500, 'YTR Homes', '1316 James Ct.', 'Suite 55', 'San Mateo', 'CA', '94401', 'USA', '650-500-5555'),
 (600, 'Taing Consulting', '123 Homestead Ave.', 'Suite 123', 'Santa Clara', 'CA', '95051', 'USA', '408-600-6000'),
 (700, 'Dance Central', '500 Jazz Circle', null, 'San Jose', 'CA', '95210', 'USA', '408-777-7000');

select locid, company_name, phone from office1 order by locid, phone;

select locid,
       company_name,
       phone,
       decode('408', get_area_code(phone), 'Y') as south_bay,
       decode('650', get_area_code(phone), 'Y', 'N') as peninsula,
       decode('415', get_area_code(phone), 'Y', 'N') as sf
from office1
order by locid, phone;

create schema test_functions;
alter function get_area_code(text) set schema test_functions;

select locid,
       company_name,
       phone,
       decode('408', test_functions.get_area_code(phone), 'Y') as south_bay,
       decode('650', test_functions.get_area_code(phone), 'Y', 'N') as peninsula,
       decode('415', test_functions.get_area_code(phone), 'Y', 'N') as sf
from office1
order by locid, phone;

drop table if exists employees;
drop table if exists emp_start_dates;

CREATE TABLE employees
(
   empid integer,
   name text,
   gender char(1),
   PRIMARY KEY (empid, gender)
) distributed by (empid)
partition by list (gender)
(
partition women values ('F'),
partition men values ('M'),
default partition other
);

CREATE TABLE emp_start_dates
(
   empid integer,
   startdate date
) distributed by (startdate);

insert into employees(empid, name, gender) values
  (100, 'John Smith', 'M'),
  (101, 'John Deere', 'M'),
  (102, 'Jane Doe', 'F'),
  (103, 'Janet Jackson', 'F'),
  (104, 'Anne Smith', 'F'),
  (105, 'Ryan Goesling', 'M'),
  (106, 'George Clooney', 'M'),
  (107, 'Julia Roberts', 'F'),
  (108, 'Jennifer Aniston', 'F'),
  (109, 'John Mayer', 'M');

insert into emp_start_dates(empid, startdate) values
  (100, '2012-01-01'::date),
  (101, '2010-01-15'::date),
  (102, '2008-02-28'::date),
  (103, '2009-02-01'::date),
  (104, '2007-03-15'::date),
  (105, '2006-05-01'::date),
  (106, '2011-05-01'::date),
  (107, '2004-01-15'::date),
  (108, '2000-02-15'::date),
  (109, null);

select e.empid as emp_id,
       e.name as emp_name,
       e.gender as gender,
       s.startdate as emp_start_dt,
       decode(extract(year from s.startdate),
              2012, coalesce(extract(year from s.startdate), '2012'),
	      2011, coalesce(extract(year from s.startdate), '2011'),
	      2010, coalesce(extract(year from s.startdate), '2010'),
	      2009, coalesce(extract(year from s.startdate), '2009'),
	      2008, coalesce(extract(year from s.startdate), '2008'),
	      2007, coalesce(extract(year from s.startdate), '2007'),
	      2006, coalesce(extract(year from s.startdate), '2006'),
	      null, coalesce(extract(year from s.startdate), '0'),
	      -1) as emp_start_yr
from employees e
  join emp_start_dates s on (e.empid = s.empid)
order by s.startdate, e.empid asc;

drop table if exists employees;
drop table if exists emp_start_dates;

CREATE TABLE employees
(
   empid integer,
   name text,
   gender char(1),
   PRIMARY KEY (empid, gender)
) distributed by (empid)
partition by list (gender)
(
partition women values ('F'),
partition men values ('M'),
default partition other
);

CREATE TABLE emp_start_dates
(
   empid integer,
   startdate date
) distributed by (startdate);

insert into employees(empid, name, gender) values
  (100, 'John Smith', 'M'),
  (101, 'John Deere', 'M'),
  (102, 'Jane Doe', 'F'),
  (103, 'Janet Jackson', 'F'),
  (104, 'Anne Smith', 'F'),
  (105, 'Ryan Goesling', 'M'),
  (106, 'George Clooney', 'M'),
  (107, 'Julia Roberts', 'F'),
  (108, 'Jennifer Aniston', 'F'),
  (109, 'John Mayer', 'M');

insert into emp_start_dates(empid, startdate) values
  (100, '2012-01-01'::date),
  (101, '2010-01-15'::date),
  (102, '2008-02-28'::date),
  (103, '2009-02-01'::date),
  (104, '2007-03-15'::date),
  (105, '2006-05-01'::date),
  (106, '2011-05-01'::date),
  (107, '2004-01-15'::date),
  (108, '2000-02-15'::date),
  (109, null);

select e.empid as emp_id,
       e.name as emp_name,
       e.gender as gender,
       s.startdate as emp_start_dt,
       decode(extract(year from s.startdate), 2012, 2012, 2011, 2011, 2010, 2010, 2009, 2009, 2008, 2008, 2007, 2007, 2006, 2006, null, 0, coalesce(extract(year from '2005-01-01'::date), 2005)) as emp_start_yr
from employees e
  join emp_start_dates s on (e.empid = s.empid)
order by s.startdate, e.empid asc;

drop table employees;


-- Test DECODE() with an expression with side-effects.
create table vtable(a int) distributed by (a);

create or replace function test_volatile(in_val integer)
returns integer as
$$
declare
    rowcnt integer;
begin
    execute 'insert into vtable values(' || in_val || ')';
    select count(*) from vtable into rowcnt;
    return rowcnt;
end;
$$ language plpgsql VOLATILE MODIFIES SQL DATA;

select decode(test_volatile(100), 1, 'PASS', 2, 'FAIL', 3, 'FAIL', 4, 'FAIL', 5, 'FAIL', 6, 'FAIL') as decode;

truncate table vtable;
select decode(2, test_volatile(100), 'FAIL', test_volatile(200), 'PASS', test_volatile(300), 'FAIL', 'FAIL') as decode;

select * from vtable order by a;

select decode(null, 1, 'null = 1', 'null != 1');

select decode(null::integer, 1, 'null = 1', 'null != 1');

select decode(1, null, '1 = null', '1 != null');

select decode(null, null, 'null = null', 'null != null');

select decode(10, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, null, 1);

select decode(11, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, null);
select decode(true);

select decode(true, false);

select decode(2);

select decode('2/14/2011'::date);

begin;
CREATE TABLE genders (gender char(1), student_id integer)
WITH (appendonly=true) DISTRIBUTED BY (student_id);

insert into genders values
  ('M', 11111),
  ('M', 12222),
  ('F', 22222),
  ('F', 33333),
  ('F', 44444),
  ('M', 55555),
  ('F', 55555),
  ('M', 66666);
commit;

select gender, decode(gender, 1, 'Unknown', 'M', 'Male', 'F', 'Female', 'Unknown')
from genders
order by gender,student_id;

select gender,decode(gender, 'N/A', 'Unknown', 'M', 'Male', 'F', 'Female')
from genders
order by gender,student_id;

select gender, decode(gender, 'Z', 'Unknown', 1)
from genders
order by student_id;

select gender, decode(gender, 'Z', 0,  'M')
from genders
order by student_id;

select gender, decode(gender, 'Z', 0, 'M', 'Male', 'F')
from genders
order by student_id;

select decode('2011-01-05'::date, '2011-01-01'::date, 2011, '2010-12-30'::date, 2010, '2012');

select decode('2011-01-05'::date, '2011-01-01'::date, '2011-01-01'::date, '2010-12-30'::date, '2010-01-01'::date, '2012');

select decode('2011-01-05'::date, '2011-01-01'::date, '2011-01-01'::date, '2010-12-30'::date, '2010-01-01'::date, 2012);

-- Test using DECODE as table, database, column, constraint, index, partition name.
-- These all fail because DECODE is a reserved keyword.

create table decode (a int, b int) distributed by (b);

create database DECODE;

create table testdecode (a int, decode int)
distributed by (a);

create table invalid
(
  empid integer CONSTRAINT decode UNIQUE,
  name text,
  gender char(1)
) distributed by (empid);

create table validt
(
    record_id  integer,
    name text,
    pub_date date
) distributed by (record_id);

create unique index decode ON validt(record_id);

create table invalidpt
(
  empid integer,
  name text,
  gender char(1)
)
distributed by (empid)
partition by list (gender)
(
   partition decode values ('F'),
   partition men values ('M'),
   default partition other
);

-- Test using DECODE() in a plpgsql function.
create or replace function sales_region(country_code char(2)) returns text as
$$
begin
  return decode(country_code, 'US', 'Americas', 'CA', 'Americas', 'MX', 'Americas',
                'CN', 'APJ', 'JP', 'APJ', 'RU', 'APJ', 'UK', 'EMEA', 'FR', 'EMEA',
               'IL', 'EMEA', 'IT', 'EMEA', 'IE', 'EMEA');
end;
$$ language plpgsql CONTAINS SQL;

select 'FR' as country_code, sales_region('FR');
select 'PR' as country_code, sales_region('PR');

drop schema if exists test_functions cascade;
create schema test_functions;
alter function sales_region(char(2)) set schema test_functions;

select 'FR' as country_code, test_functions.sales_region('FR');
select 'PR' as country_code, test_functions.sales_region('PR');

--
begin;
create table locations
(
   locid integer,
   bus_name text,
   country_code char(2)
);

insert into locations(locid, bus_name, country_code) values
  (1000, 'Test Solutions', 'CN'),
  (1011, 'Taing Consulting', 'US'),
  (2000, 'Parts Plus', 'IT'),
  (1055, 'Computers Unlimited', 'IL'),
  (2005, 'Kangaroos Inc.', 'AU');
commit;

create or replace view decode_view
as
select bus_name,
       country_code,
       decode(country_code, 'US', 'Americas', 'CA', 'Americas', 'MX', 'Americas',
                'CN', 'APJ', 'JP', 'APJ', 'RU', 'APJ', 'UK', 'EMEA', 'FR', 'EMEA',
               'IL', 'EMEA', 'IT', 'EMEA', 'IE', 'EMEA') as region
from locations
order by locid, country_code;

\d+ decode_view;

select * from decode_view order by region, country_code;

--

select locid,
       bus_name,
       country_code
from locations
where  decode(country_code, 'US', 'Americas', 'CA', 'Americas', 'MX', 'Americas',
                'CN', 'APJ', 'JP', 'APJ', 'RU', 'APJ', 'UK', 'EMEA', 'FR', 'EMEA',
               'IL', 'EMEA', 'IT', 'EMEA', 'IE', 'EMEA') = 'EMEA'
order by locid, country_code;

-- Test that "decode", when quoted, is interpreted as the PostgreSQL built-in
-- function decode, rather than as a decode expression.
select "decode"('MTIzAAE=', 'base64');
select "decode"(md5('password'),'hex');

--
create table decode_sales
(ytd_sales decimal,
 currency char(1)
) distributed by (currency);

insert into decode_sales values
  (2000000, 'e'),
  (10500000.25, 'd'),
  (789100.50, 'y'),
  (300685, 'p');

select *
from decode_sales
order by ytd_sales desc;

select currency, decode(currency, 'd', 'USD', 'e', 'EUR', 'y', 'JPY', 'USD') from decode_sales order by ytd_sales desc;
select currency, decode(currency, 'd', 'USD', 'e', 'EUR', 'y', 'JPY', 'p', 'GBP', 'USD') from decode_sales order by ytd_sales desc;

-- clean up
drop table decode_sales;
