-- @author ramans2
-- @created 2014-03-27 12:00:00
-- @modified 2014-03-27 12:00:00
-- @gpdiff True
-- @description Large query that fails with OOM

--start_ignore
drop table if exists mst;
drop table if exists dim_01;
drop table if exists dim_02;
drop table if exists dim_03;
drop table if exists dim_04;
drop table if exists dim_05;
drop table if exists dim_06;
drop table if exists dim_07;
drop table if exists dim_08;
drop table if exists dim_09;
drop table if exists dim_10;
drop table if exists dim_11;
drop table if exists dim_12;
drop table if exists dim_13;
drop table if exists dim_14;
drop table if exists dim_15;
drop table if exists dim_16;
drop table if exists dim_17;
drop table if exists dim_18;
drop table if exists dim_19;
drop table if exists dim_20;
drop table if exists dim_21;
drop table if exists dim_22;
drop table if exists dim_23;
drop table if exists dim_24;
drop table if exists dim_25;
--end_ignore

--Fact Table
create table mst
(
seq int,
dim_01_cd int,
dim_02_cd int,
dim_03_cd int,
dim_04_cd int,
dim_05_cd int,
dim_06_cd int,
dim_07_cd int,
dim_08_cd int,
dim_09_cd int,
dim_10_cd int,
dim_11_cd int,
dim_12_cd int,
dim_13_cd int,
dim_14_cd int,
dim_15_cd int,
dim_16_cd int,
dim_17_cd int,
dim_18_cd int,
dim_19_cd int,
dim_20_cd int,
dim_21_cd int,
dim_22_cd int,
dim_23_cd int,
dim_24_cd int,
dim_25_cd int,
qry int,
amt int
)
distributed by (seq);

--Dimension Table
create table dim_01 ( dim_01_cd int, dim_01_cd_nm varchar(10)) ;
create table dim_02 ( dim_02_cd int, dim_02_cd_nm varchar(10)) ;
create table dim_03 ( dim_03_cd int, dim_03_cd_nm varchar(10)) ;
create table dim_04 ( dim_04_cd int, dim_04_cd_nm varchar(10)) ;
create table dim_05 ( dim_05_cd int, dim_05_cd_nm varchar(10)) ;
create table dim_06 ( dim_06_cd int, dim_06_cd_nm varchar(10)) ;
create table dim_07 ( dim_07_cd int, dim_07_cd_nm varchar(10)) ;
create table dim_08 ( dim_08_cd int, dim_08_cd_nm varchar(10)) ;
create table dim_09 ( dim_09_cd int, dim_09_cd_nm varchar(10)) ;
create table dim_10 ( dim_10_cd int, dim_10_cd_nm varchar(10)) ;
create table dim_11 ( dim_11_cd int, dim_11_cd_nm varchar(10)) ;
create table dim_12 ( dim_12_cd int, dim_12_cd_nm varchar(10)) ;
create table dim_13 ( dim_13_cd int, dim_13_cd_nm varchar(10)) ;
create table dim_14 ( dim_14_cd int, dim_14_cd_nm varchar(10)) ;
create table dim_15 ( dim_15_cd int, dim_15_cd_nm varchar(10)) ;
create table dim_16 ( dim_16_cd int, dim_16_cd_nm varchar(10)) ;
create table dim_17 ( dim_17_cd int, dim_17_cd_nm varchar(10)) ;
create table dim_18 ( dim_18_cd int, dim_18_cd_nm varchar(10)) ;
create table dim_19 ( dim_19_cd int, dim_19_cd_nm varchar(10)) ;
create table dim_20 ( dim_20_cd int, dim_20_cd_nm varchar(10)) ;
create table dim_21 ( dim_21_cd int, dim_21_cd_nm varchar(10)) ;
create table dim_22 ( dim_22_cd int, dim_22_cd_nm varchar(10)) ;
create table dim_23 ( dim_23_cd int, dim_23_cd_nm varchar(10)) ;
create table dim_24 ( dim_24_cd int, dim_24_cd_nm varchar(10)) ;
create table dim_25 ( dim_25_cd int, dim_25_cd_nm varchar(10)) ;

--Insert into Fact Table
INSERT INTO mst(
seq, dim_01_cd, dim_02_cd, dim_03_cd, dim_04_cd, dim_05_cd, dim_06_cd,
dim_07_cd, dim_08_cd, dim_09_cd, dim_10_cd, dim_11_cd, dim_12_cd, dim_13_cd,
dim_14_cd, dim_15_cd, dim_16_cd, dim_17_cd, dim_18_cd, dim_19_cd, dim_20_cd,
dim_21_cd, dim_22_cd, dim_23_cd, dim_24_cd, dim_25_cd,
qry, amt)
select i, i%100, i%100, i%100, i%100, i%100, i%100, i%100, i%100, i%100,
i%100, i%100, i%100, i%100, i%100, i%100, i%100, i%100, i%100, i%100, i%100,
i%100, i%100, i%100, i%100, i%100,10, 100
from generate_series(1, 100000) i;

-- Insert into Dimesion Table
insert into dim_01 values (generate_series(1, 100), 'asdfasdf');
insert into dim_02 select * From dim_01;
insert into dim_03 select * From dim_01;
insert into dim_04 select * From dim_01;
insert into dim_05 select * From dim_01;
insert into dim_06 select * From dim_01;
insert into dim_07 select * From dim_01;
insert into dim_08 select * From dim_01;
insert into dim_09 select * From dim_01;
insert into dim_10 select * From dim_01;
insert into dim_11 select * From dim_01;
insert into dim_12 select * From dim_01;
insert into dim_13 select * From dim_01;
insert into dim_14 select * From dim_01;
insert into dim_15 select * From dim_01;
insert into dim_16 select * From dim_01;
insert into dim_17 select * From dim_01;
insert into dim_18 select * From dim_01;
insert into dim_19 select * From dim_01;
insert into dim_20 select * From dim_01;
insert into dim_21 select * From dim_01;
insert into dim_22 select * From dim_01;
insert into dim_23 select * From dim_01;
insert into dim_24 select * From dim_01;
insert into dim_25 select * From dim_01;

show gp_vmem_protect_limit;
select 5 as oom_test;

select
dim_01_cd_nm,
dim_02_cd_nm,
dim_03_cd_nm,
dim_04_cd_nm,
dim_05_cd_nm,
dim_06_cd_nm,
dim_07_cd_nm,
dim_08_cd_nm,
dim_09_cd_nm,
dim_10_cd_nm,
dim_11_cd_nm,
dim_12_cd_nm,
dim_13_cd_nm,
dim_14_cd_nm,
dim_15_cd_nm,
sum(qry),
sum(amt)
from mst a00,
dim_01 a01,
dim_02 a02,
dim_03 a03,
dim_04 a04,
dim_05 a05,
dim_06 a06,
dim_07 a07,
dim_08 a08,
dim_09 a09,
dim_10 a10,
dim_11 a11,
dim_12 a12,
dim_13 a13,
dim_14 a14,
dim_15 a15,
(select pg_sleep(100)) as t
where a00.dim_01_cd = a01.dim_01_cd
and a00.dim_02_cd = a02.dim_02_cd
and a00.dim_03_cd = a03.dim_03_cd
and a00.dim_04_cd = a04.dim_04_cd
and a00.dim_05_cd = a05.dim_05_cd
and a00.dim_06_cd = a06.dim_06_cd
and a00.dim_07_cd = a07.dim_07_cd
and a00.dim_08_cd = a08.dim_08_cd
and a00.dim_09_cd = a09.dim_09_cd
and a00.dim_10_cd = a10.dim_10_cd
and a00.dim_11_cd = a11.dim_11_cd
and a00.dim_12_cd = a12.dim_12_cd
and a00.dim_13_cd = a13.dim_13_cd
and a00.dim_14_cd = a14.dim_14_cd
and a00.dim_15_cd = a15.dim_15_cd
group by
dim_01_cd_nm,
dim_02_cd_nm,
dim_03_cd_nm,
dim_04_cd_nm,
dim_05_cd_nm,
dim_06_cd_nm,
dim_07_cd_nm,
dim_08_cd_nm,
dim_09_cd_nm,
dim_10_cd_nm,
dim_11_cd_nm,
dim_12_cd_nm,
dim_13_cd_nm,
dim_14_cd_nm,
dim_15_cd_nm
;
