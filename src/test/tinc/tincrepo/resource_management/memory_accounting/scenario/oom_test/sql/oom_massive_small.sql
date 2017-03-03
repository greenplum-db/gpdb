-- @author ramans2
-- @created 2014-03-27 12:00:00
-- @modified 2014-03-27 12:00:00
-- @gpdiff True
-- @description Large query that fails with OOM
-- start_matchsubs
-- m/.*pg_temp_\d+.pg_analyze_\d+_\d+.*/
-- s/pg_temp_\d+.pg_analyze_\d+_\d+/pg_temp_XX.pg_analyze_XXXX/
-- end_matchsubs

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
select 5 as oom_test;
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
