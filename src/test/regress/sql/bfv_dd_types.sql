--
-- Direct Dispatch Test when optimizer is on
--
-- start_ignore
set optimizer_log=on;
set optimizer_print_missing_stats = off;
-- end_ignore

set test_print_direct_dispatch_info=on; 

set gp_autostats_mode = 'None';

-- test direct dispatch for different data types

CREATE TABLE direct_test_type_int2 (id int2) DISTRIBUTED BY (id);
CREATE TABLE direct_test_type_int4 (id int4) DISTRIBUTED BY (id);
CREATE TABLE direct_test_type_int8 (id int8) DISTRIBUTED BY (id);

create table direct_test_type_real (real1 real, smallint1 smallint, boolean1 boolean, int1 int, double1 double precision, date1 date, numeric1 numeric) distributed by (real1);
create table direct_test_type_smallint (real1 real, smallint1 smallint, boolean1 boolean, int1 int, double1 double precision, date1 date, numeric1 numeric) distributed by (smallint1);
create table direct_test_type_boolean2 (real1 real, smallint1 smallint, boolean1 boolean, int1 int, double1 double precision, date1 date, numeric1 numeric) distributed by (boolean1);
create table direct_test_type_double (real1 real, smallint1 smallint, boolean1 boolean, int1 int, double1 double precision, date1 date, numeric1 numeric) distributed by (double1);
create table direct_test_type_date (real1 real, smallint1 smallint, boolean1 boolean, int1 int, double1 double precision, date1 date, numeric1 numeric) distributed by (date1);
create table direct_test_type_numeric (real1 real, smallint1 smallint, boolean1 boolean, int1 int, double1 double precision, date1 date, numeric1 numeric) distributed by (numeric1);
create table direct_test_type_bit (x bit) distributed by (x);
create table direct_test_type_bpchar (x bpchar) distributed by (x);
create table direct_test_type_bytea (x bytea) distributed by (x);
create table direct_test_type_cidr (x cidr) distributed by (x);
create table direct_test_type_inet (x inet) distributed by (x);
create table direct_test_type_macaddr (x macaddr) distributed by (x);
create table direct_test_type_varbit (x varbit) distributed by (x);

INSERT INTO direct_test_type_int2 VALUES (1);
INSERT INTO direct_test_type_int4 VALUES (1);
INSERT INTO direct_test_type_int8 VALUES (1);

insert into direct_test_type_real values (8,8,true,8,8,'2008-08-08',8.8);
insert into direct_test_type_smallint values (8,8,true,8,8,'2008-08-08',8.8);
insert into direct_test_type_boolean2 values (8,8,true,8,8,'2008-08-08',8.8);
insert into direct_test_type_double values (8,8,true,8,8,'2008-08-08',8.8);
insert into direct_test_type_date values (8,8,true,8,8,'2008-08-08',8.8);
insert into direct_test_type_numeric values (8,8,true,8,8,'2008-08-08',8.8);
insert into direct_test_type_bit values('1');
insert into direct_test_type_bpchar values('abs');
insert into direct_test_type_bytea values('greenplum');
insert into direct_test_type_cidr values('68.44.55.111');
insert into direct_test_type_inet values('68.44.55.111');
insert into direct_test_type_macaddr values('12:34:56:78:90:ab');
insert into direct_test_type_varbit values('0101010');


-- @author antovl
-- @created 2014-11-07 12:00:00 
-- @modified 2014-11-07 12:00:00
-- @optimizer_mode on
-- @gpopt 1.510
-- @product_version gpdb: [4.3.3-], [5.0-], hawq: [1.2.2.0-] 
-- @tags bfv
-- @gucs optimizer_enable_constant_expression_evaluation=on;

select * from direct_test_type_real where real1 = 8::real;
select * from direct_test_type_smallint where smallint1 = 8::smallint;
select * from direct_test_type_double where double1 = 8;
select * from direct_test_type_date where date1 = '2008-08-08';
select * from direct_test_type_numeric where numeric1 = 8.8;
select * from direct_test_type_bit where x = '1';
select * from direct_test_type_bpchar where x = 'abs';
select * from direct_test_type_bytea where x = 'greenplum';

-- TODO: this currently not directly dispatched (AGL-1246)
select * from direct_test_type_cidr where x = '68.44.55.111';

select * from direct_test_type_inet where x = '68.44.55.111';
select * from direct_test_type_macaddr where x = '12:34:56:78:90:ab';
select * from direct_test_type_varbit where x = '0101010';

SELECT * FROM direct_test_type_int2 WHERE id = 1::int2;
SELECT * FROM direct_test_type_int2 WHERE id = 1::int4;
SELECT * FROM direct_test_type_int2 WHERE id = 1::int8;

SELECT * FROM direct_test_type_int2 WHERE 1::int2 = id;
SELECT * FROM direct_test_type_int2 WHERE 1::int4 = id;
SELECT * FROM direct_test_type_int2 WHERE 1::int8 = id;

SELECT * FROM direct_test_type_int4 WHERE id = 1::int2;
SELECT * FROM direct_test_type_int4 WHERE id = 1::int4;
SELECT * FROM direct_test_type_int4 WHERE id = 1::int8;

SELECT * FROM direct_test_type_int4 WHERE 1::int2 = id;
SELECT * FROM direct_test_type_int4 WHERE 1::int4 = id;
SELECT * FROM direct_test_type_int4 WHERE 1::int8 = id;

SELECT * FROM direct_test_type_int8 WHERE id = 1::int2;
SELECT * FROM direct_test_type_int8 WHERE id = 1::int4;
SELECT * FROM direct_test_type_int8 WHERE id = 1::int8;
SELECT * FROM direct_test_type_int8 WHERE 1::int2 = id;
SELECT * FROM direct_test_type_int8 WHERE 1::int4 = id;
SELECT * FROM direct_test_type_int8 WHERE 1::int8 = id;

-- overflow test
SELECT * FROM direct_test_type_int2 WHERE id = 32768::int4;
SELECT * FROM direct_test_type_int2 WHERE id = -32769::int4;

SELECT * FROM direct_test_type_int2 WHERE 32768::int4 = id;
SELECT * FROM direct_test_type_int2 WHERE -32769::int4 = id;

SELECT * FROM direct_test_type_int2 WHERE id = 2147483648::int8;
SELECT * FROM direct_test_type_int2 WHERE id = -2147483649::int8;

SELECT * FROM direct_test_type_int2 WHERE 2147483648::int8 = id;
SELECT * FROM direct_test_type_int2 WHERE -2147483649::int8 = id;

drop table direct_test_type_real;
drop table direct_test_type_smallint;
drop table direct_test_type_double;
drop table direct_test_type_date;
drop table direct_test_type_numeric;
drop table direct_test_type_int2;
drop table direct_test_type_int4;
drop table direct_test_type_int8;

drop table direct_test_type_bit;
drop table direct_test_type_bpchar;
drop table direct_test_type_bytea;
drop table direct_test_type_cidr;
drop table direct_test_type_inet;
drop table direct_test_type_macaddr;
drop table direct_test_type_varbit;
drop table direct_test_type_boolean2;
reset test_print_direct_dispatch_info;
