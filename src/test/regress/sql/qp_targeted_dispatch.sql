-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

create schema qp_targeted_dispatch;
set search_path to qp_targeted_dispatch;

-- ----------------------------------------------------------------------
-- Test: query02.sql
-- ----------------------------------------------------------------------
create table direct_test1
(
  key int NULL,
  value varchar(50) NULL
)
distributed by (key);
insert into direct_test1 values (200, 'horse');
-- enable printing of printing info
set test_print_direct_dispatch_info=on;
Begin;
declare c0 cursor for select * from direct_test1 where value='horse';
select * from direct_test1 where value='horse';
select value from direct_test1 where value='horse';
select * from direct_test1 where key=200;
declare c1 cursor for select * from direct_test1 where key=200;
fetch c1;
fetch c0;
fetch c1;
fetch c0;
End;

reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query03.sql
-- ----------------------------------------------------------------------
create table key_value_table
(
  key int NULL,
  value varchar(50) NULL
)
distributed by (key);
insert into key_value_table values (200, 'horse');
-- enable printing of printing info
set test_print_direct_dispatch_info=on;
Begin;
SELECT * FROM key_value_table WHERE key = 200 FOR UPDATE;
update key_value_table set value=300 where key =200;
savepoint s;
update key_value_table set value=200 where key =300;
update key_value_table set value=300 where key =200;
rollback to s;
commit;
Begin;
SELECT * FROM key_value_table WHERE key = 200 FOR UPDATE;
savepoint s;
update key_value_table set value=200 where key =300;
rollback;
savepoint s;
abort;
reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query04.sql
-- ----------------------------------------------------------------------
CREATE TABLE MWV_CDetail_TABLE(
 ATTRIBUTE066 DATE,
 ATTRIBUTE084 TIMESTAMP,
 ATTRIBUTE097 BYTEA,
 ATTRIBUTE096 BIGINT,
 ATTRIBUTE032 VARCHAR (3),
 ATTRIBUTE031 VARCHAR (20),
 ATTRIBUTE151 VARCHAR (4000),
 ATTRIBUTE037 VARCHAR (4000),
 ATTRIBUTE047 VARCHAR (2048)
)
with (appendonly=true, orientation=column, compresstype=zlib)
distributed by (attribute066)
partition by range (attribute066)
(
start (date '2009-10-01') inclusive end (date '2009-12-31') inclusive every (interval '1 week')
)
;

--explain
SELECT
  ATTRIBUTE066,ATTRIBUTE032 ,ATTRIBUTE031
 ,COUNT(*) AS CNT
FROM 
  MWV_CDETAIL_TABLE
WHERE ATTRIBUTE066 = '2009-12-31'::date -           1 
GROUP BY ATTRIBUTE066,ATTRIBUTE032 ,ATTRIBUTE031;

reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query05.sql
-- ----------------------------------------------------------------------

-- Targeted Dispatch to make sure it works fine for all possible data types. This test case is to check if it works fine for boolean and int data type
create table boolean (boo boolean, b int);
insert into boolean values ('f', 1);
set test_print_direct_dispatch_info=on;
insert into boolean values ('t', 2);
alter table boolean set distributed by (b);
insert into boolean values ('t', 1);
alter table boolean set distributed randomly;
insert into boolean values ('t', 1);
alter table boolean set distributed by (boo, b);
select * from boolean where boo='t' and b=2;
reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query06.sql
-- ----------------------------------------------------------------------

-- Targeted Dispatch to make sure it works fine for all possible data types. This test case is to check if it works fine for date and double precision data type
create table date (date1 date, dp1 double precision);
insert into date values ('2001-11-11',234.23234);
set test_print_direct_dispatch_info=on;
insert into date values ('2001-11-12',234.2323);
alter table date set distributed by (dp1);
insert into date values ('2001-11-13',234.23234);
insert into date values ('2001-11-14',234.2323);
alter table date set distributed by (date1, dp1);
select * from date where date1='2001-11-12' and dp1=234.2323;

reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query07.sql
-- ----------------------------------------------------------------------

-- Targeted Dispatch to make sure it works fine for all possible data types. This test case is to check if it works fine for interval and Numeric data type
create table interval (interval1 interval, num numeric);
insert into interval values ('23',2345);
set test_print_direct_dispatch_info=on;
insert into interval values ('2',234);
alter table interval set distributed by (num);
insert into interval values ('24',234);
insert into interval values ('26',2343);
alter table interval set distributed by (num,interval1);
select * from interval where interval1='23' and num=2345;

reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query08.sql
-- ----------------------------------------------------------------------

-- This test case is to check if it works fine for real and smallint data type
create table real (real1 real, si1 smallint) distributed by (real1);
insert into real values (23, 4);
set test_print_direct_dispatch_info=on;
insert into real values (23, 4);
Alter table real set distributed by (si1);
insert into real values (21, 3);
insert into real values (21, 2);
select * from real where real.si1=3;
Alter table real set distributed by (si1,real1);
select * from real where real1=21 and si1=3;
reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query09.sql
-- ----------------------------------------------------------------------

-- This test case is to check if it works fine for bytea and cidr data type
create table bytea (bytea1 bytea, cidr1 cidr) distributed by (bytea1);
insert into bytea values ('d','0.0.0.0');
set test_print_direct_dispatch_info=on;
insert into bytea values ('d','0.0.0.1');
alter table bytea set distributed by (cidr1,bytea1);
insert into bytea values ('e','0.0.1.0');
select * from bytea where bytea1='d' and cidr1='0.0.0.1';
reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query10.sql
-- ----------------------------------------------------------------------

-- This test case is to check if it works fine for inet and macaddr data type
create table inetmac (inet1 inet, macaddr1 macaddr) distributed by (inet1);
insert into inetmac values ('0.0.0.0','AA:AA:AA:AA:AA:AA');
set test_print_direct_dispatch_info=on;
insert into inetmac values ('0.0.0.0','AC:AA:AA:AA:AA:AA');
alter table inetmac set distributed by (macaddr1);
insert into inetmac values ('0.0.0.2','AA:AA:AA:AA:AA:AC');
alter table inetmac set distributed by (macaddr1,inet1);
insert into inetmac values ('0.0.0.2','AA:AA:AA:AA:AA:AC');
select * from inetmac where inet1='0.0.0.0' and macaddr1 ='AA:AA:AA:AA:AA:AA';
reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query12.sql
-- ----------------------------------------------------------------------

create table time2 (time2 time with time zone, text1 text);
insert into time2 values ('00:00:00+1359', 'abcg');
set test_print_direct_dispatch_info=on;
insert into time2 values ('00:00:00+1359', 'abcf');
alter table time2 set distributed by (text1);
insert into time2 values ('00:00:00+1352', 'abce');
alter table time2 set distributed by (text1,time2);
insert into time2 values ('00:00:00+1352', 'abcd');
select * from time2 where time2='00:00:00+1359' and text1='abcg';
reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query13.sql
-- ----------------------------------------------------------------------

create table timestamp (timestamp1 timestamp without time zone, time2 timestamp with time zone);
insert into timestamp values ('2004-12-13 01:51:15','2004-12-13 01:51:15+1359');
set test_print_direct_dispatch_info=on;
insert into timestamp values ('2004-12-13 01:51:15','2004-12-13 01:51:15+1359');
alter table timestamp set distributed by (time2);
insert into timestamp values ('2004-12-13 01:51:25','2004-12-12 01:51:15+1359');
alter table timestamp set distributed by (time2, timestamp1);
insert into timestamp values ('2004-12-13 01:51:25','2004-12-12 01:51:15+1359');
select * from timestamp where timestamp1='2004-12-13 01:51:25' and time2 ='2004-12-12 01:51:15+1359';
reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query14.sql
-- ----------------------------------------------------------------------

create table bit1 (a bit(1), b int) distributed by (a);
insert into bit1 values ('0', 23);
set test_print_direct_dispatch_info=on;
insert into bit1 values ('1', 23);
alter table bit1 set distributed by (b);
insert into bit1 values ('0', 24);
alter table bit1 set distributed by (b,a);
insert into bit1 values ('0', 24);
select * from bit1 where a='0' and b =24;
reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query18.sql
-- ----------------------------------------------------------------------

create table mpp7638 (a int, b int, c int, d int) partition by range(d) (start(1) end(10) every(1));
insert into mpp7638 select i, i+1, i+2, i+3 from generate_series(1, 2) i;
insert into mpp7638 select i, i+1, i+2, i+3 from generate_series(1, 3) i;
insert into mpp7638 select i, i+1, i+2, i+3 from generate_series(1, 4) i;
insert into mpp7638 select i, i+1, i+2, i+3 from generate_series(1, 5) i;
set test_print_direct_dispatch_info=on;
insert into mpp7638 select i, i+1, i+2, i+3 from generate_series(1, 6) i;
select count(*) from mpp7638 where a =1;
explain select count(*) from mpp7638 where a =1;
alter table mpp7638 set distributed by (a, b, c);
select * from mpp7638 where a=1 and b=2 and c=3;
reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query19.sql
-- ----------------------------------------------------------------------

--Partition by range table should use targeted diaptch

CREATE TABLE range_table (id INTEGER)
 PARTITION BY RANGE (id)
(START (0) END (200000) EVERY (100000)) ;
INSERT INTO range_table(id) VALUES (0);
CREATE INDEX id3 ON range_table USING BITMAP (id);
set test_print_direct_dispatch_info=on;
INSERT INTO range_table(id) VALUES (1);
DROP INDEX id3;
CREATE INDEX id3 ON range_table USING BITMAP (id);
INSERT INTO range_table(id) VALUES (2);
INSERT INTO range_table(id) VALUES (3);
INSERT INTO range_table(id) VALUES (4);
INSERT INTO range_table(id) VALUES (5);
INSERT INTO range_table(id) VALUES (5);
select * from range_table where id =1;
select count(*) from range_table where id=1;

reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query20.sql
-- ----------------------------------------------------------------------

-- Table using inheritance, Rules and Insert-Select is not getting targeted
-- even though Select is targeted.

create table tblexecutions (date date not null, mykey bigint, "sequence" int not null, firm character varying(4) NOT NULL) distributed by ("sequence");
create table tblexecutions_20080102 (CONSTRAINT tblexecutions_20080102_date_check CHECK (((date >= '2008-01-02'::date) AND (date <= '2008-01-02'::date)))) INHERITS (tblexecutions) distributed by ("sequence");
CREATE TABLE tblexecutions_20080103 (CONSTRAINT tblexecutions_20080103_date_check CHECK (((date >= '2008-01-03'::date) AND (date <= '2008-01-03'::date)))) INHERITS (tblexecutions) distributed by ("sequence");
CREATE TABLE tblexecutions_20080104 (CONSTRAINT tblexecutions_20080104_date_check CHECK (((date >= '2008-01-04'::date) AND (date <= '2008-01-04'::date)))) INHERITS (tblexecutions) distributed by ("sequence");
create index tblexecutions_20080103_idx on tblexecutions_20080103 using bitmap (firm);
CREATE RULE rule_tblexecutions_20080102 AS ON INSERT TO tblexecutions WHERE ((new.date >= '2008-01-02'::date) AND (new.date <= '2008-01-02'::date)) DO INSTEAD INSERT INTO tblexecutions_20080102 (date, mykey, "sequence", firm) VALUES (new.date, new.mykey, new."sequence", new.firm);
CREATE RULE rule_tblexecutions_20080103 AS ON INSERT TO tblexecutions WHERE ((new.date >= '2008-01-03'::date) AND (new.date <= '2008-01-03'::date)) DO INSTEAD INSERT INTO tblexecutions_20080103 (date, mykey, "sequence", firm) VALUES (new.date, new.mykey, new."sequence", new.firm);
CREATE RULE rule_tblexecutions_20080104 AS ON INSERT TO tblexecutions WHERE ((new.date >= '2008-01-04'::date) AND (new.date <= '2008-01-04'::date)) DO INSTEAD INSERT INTO tblexecutions_20080104 (date, mykey, "sequence", firm) VALUES (new.date, new.mykey, new."sequence", new.firm);
insert into tblexecutions select '2008/01/02'::date + ((i % 3) || ' days')::interval, i*10, i, 'f' || to_char(random()*100, '99') from generate_series(1, 1000) i;
insert into tblexecutions select * from tblexecutions where sequence=10;
set test_print_direct_dispatch_info=on;
select count(*) from tblexecutions where sequence=10;


reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query21.sql
-- ----------------------------------------------------------------------

--targeted dispatch for CTAS and Insert-Select, It doesn't work today still I have been asked to check in test cases and later move o/p to ans when we have this working. MPP_7620

create table mpp7620
(
  key int NULL,
  value varchar(50) NULL
)
distributed by (key);
insert into mpp7620 values (200, 'horse');
-- enable printing of printing info
set test_print_direct_dispatch_info=on;
Create table zoompp7620 as select * from mpp7620 where key=200;
insert into mpp7620 values (200, 200);
insert into zoompp7620 select * from mpp7620 where key=200;
insert into zoompp7620(key) select key from mpp7620 where mpp7620.key=200;
select key from mpp7620 where mpp7620.key=200;
select * from (select * from mpp7620 where key=200) ss where key =200;
explain select * from (select * from mpp7620 where key=200) ss where key =200; 
explain insert into zoompp7620(key) select key from mpp7620 where mpp7620.key=200;
explain select key from mpp7620 where mpp7620.key=200;
reset test_print_direct_dispatch_info;

-- ----------------------------------------------------------------------
-- Test: query22.sql
-- ----------------------------------------------------------------------

--Test case for Deepslice queries, since this is disabled right now we need to move correct o/p to expected result once feature is made available for Deepslice queries. QA-592
CREATE SEQUENCE s;
CREATE TABLE table_a (a0 int, a1 int, a2 int, a3 int);
INSERT INTO table_a (a3, a2, a0, a1) VALUES (nextval('s'), nextval('s'), nextval('s'), nextval('s'));
ANALYZE table_a;
insert into table_a (a3,a2,a0,a1) values (1,2,3,4);
set test_print_direct_dispatch_info=on;

--Check to see distributed vs distributed randomly
alter table table_a set distributed randomly;
select max(a0) from table_a where a0=3;
alter table table_a set distributed by (a0);
explain select * from table_a where a0=3;
explain select a0 from table_a where a0 in (select max(a1) from table_a);
select a0 from table_a where a0 in (select max(a1) from table_a);
select max(a1) from table_a;
select max(a0) from table_a where a0=1;
explain select a0 from table_a where a0 in (select max(a1) from table_a where a0=1);
reset test_print_direct_dispatch_info;

-- the filter is over a window, do not direct dispatch
create table foo_direct_dispatch (dist_key int, non_dist_key int);
insert into foo_direct_dispatch select i, i from generate_series (1,100)i;
select rank from ( select rank() over ( order by dist_key asc ) as rank , * from foo_direct_dispatch )a where 10 = dist_key;
explain select rank from ( select rank() over ( order by dist_key asc ) as rank , * from foo_direct_dispatch )a where 10 = dist_key;
-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

set client_min_messages='warning';
drop schema qp_targeted_dispatch cascade;
