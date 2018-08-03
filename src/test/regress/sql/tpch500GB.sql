-- ensure legacy default settings;
-- new installations have different settings due to modified conf files;

set optimizer = off;
set enable_mergejoin = on;
set enable_nestloop = on;
set gp_enable_agg_distinct = off;
set gp_selectivity_damping_for_scans = off;
set gp_selectivity_damping_for_joins = off;

set allow_system_table_mods = true;

-- 
-- Greenplum Database Statistics Dump
-- Copyright (C) 2007  Greenplum Inc.
-- 
-- Database: tpch500
-- Date:     2007-10-05
-- Time:     12:07:47.570184-07
-- CmdLine:    
-- Version:  PostgreSQL 8.2.4 (Greenplum Database Release-3_1_0_0-alpha1-branch build 1896) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Sep 25 2007 10:48:21
-- 
--
-- Greenplum Database database dump
--

SET client_encoding = 'SQL_ASCII';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

--
-- Name: SCHEMA tpch500gb; Type: COMMENT; Schema: -; Owner: -
--

COMMENT ON SCHEMA public IS 'Standard public schema';
create schema tpch500gb;

SET search_path = tpch500gb, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: customer; Type: TABLE; Schema: tpch500gb; Owner: -; Tablespace: 
--

CREATE TABLE customer (
    c_custkey integer NOT NULL,
    c_name character varying(25) NOT NULL,
    c_address character varying(40) NOT NULL,
    c_nationkey integer NOT NULL,
    c_phone character(15) NOT NULL,
    c_acctbal numeric(15,2) NOT NULL,
    c_mktsegment character(10) NOT NULL,
    c_comment character varying(117) NOT NULL
) distributed by (c_custkey);


--
-- Name: lineitem; Type: TABLE; Schema: tpch500gb; Owner: -; Tablespace: 
--

CREATE TABLE lineitem (
    l_orderkey bigint NOT NULL,
    l_partkey integer NOT NULL,
    l_suppkey integer NOT NULL,
    l_linenumber integer NOT NULL,
    l_quantity numeric(15,2) NOT NULL,
    l_extendedprice numeric(15,2) NOT NULL,
    l_discount numeric(15,2) NOT NULL,
    l_tax numeric(15,2) NOT NULL,
    l_returnflag character(1) NOT NULL,
    l_linestatus character(1) NOT NULL,
    l_shipdate date NOT NULL,
    l_commitdate date NOT NULL,
    l_receiptdate date NOT NULL,
    l_shipinstruct character(25) NOT NULL,
    l_shipmode character(10) NOT NULL,
    l_comment character varying(44) NOT NULL
) distributed by (l_orderkey);


--
-- Name: nation; Type: TABLE; Schema: tpch500gb; Owner: -; Tablespace: 
--

CREATE TABLE nation (
    n_nationkey integer NOT NULL,
    n_name character(25) NOT NULL,
    n_regionkey integer NOT NULL,
    n_comment character varying(152)
) distributed by (n_nationkey);


--
-- Name: orders; Type: TABLE; Schema: tpch500gb; Owner: -; Tablespace: 
--

CREATE TABLE orders (
    o_orderkey bigint NOT NULL,
    o_custkey integer NOT NULL,
    o_orderstatus character(1) NOT NULL,
    o_totalprice numeric(15,2) NOT NULL,
    o_orderdate date NOT NULL,
    o_orderpriority character(15) NOT NULL,
    o_clerk character(15) NOT NULL,
    o_shippriority integer NOT NULL,
    o_comment character varying(79) NOT NULL
) distributed by (o_orderkey);


--
-- Name: part; Type: TABLE; Schema: tpch500gb; Owner: -; Tablespace: 
--

CREATE TABLE part (
    p_partkey integer NOT NULL,
    p_name character varying(55) NOT NULL,
    p_mfgr character(25) NOT NULL,
    p_brand character(10) NOT NULL,
    p_type character varying(25) NOT NULL,
    p_size integer NOT NULL,
    p_container character(10) NOT NULL,
    p_retailprice numeric(15,2) NOT NULL,
    p_comment character varying(23) NOT NULL
) distributed by (p_partkey);


--
-- Name: partsupp; Type: TABLE; Schema: tpch500gb; Owner: -; Tablespace: 
--

CREATE TABLE partsupp (
    ps_partkey integer NOT NULL,
    ps_suppkey integer NOT NULL,
    ps_availqty integer NOT NULL,
    ps_supplycost numeric(15,2) NOT NULL,
    ps_comment character varying(199) NOT NULL
) distributed by (ps_partkey);


--
-- Name: region; Type: TABLE; Schema: tpch500gb; Owner: -; Tablespace: 
--

CREATE TABLE region (
    r_regionkey integer NOT NULL,
    r_name character(25) NOT NULL,
    r_comment character varying(152)
) distributed by (r_regionkey);


--
-- Name: revenue0; Type: VIEW; Schema: tpch500gb; Owner: -
--

CREATE VIEW revenue0 AS
    SELECT lineitem.l_suppkey AS supplier_no, sum((lineitem.l_extendedprice * ((1)::numeric - lineitem.l_discount))) AS total_revenue FROM lineitem WHERE ((lineitem.l_shipdate >= '1993-01-01'::date) AND (lineitem.l_shipdate < ('1993-01-01'::date + '3 mons'::interval))) GROUP BY lineitem.l_suppkey;


--
-- Name: supplier; Type: TABLE; Schema: tpch500gb; Owner: -; Tablespace: 
--

CREATE TABLE supplier (
    s_suppkey integer NOT NULL,
    s_name character(25) NOT NULL,
    s_address character varying(40) NOT NULL,
    s_nationkey integer NOT NULL,
    s_phone character(15) NOT NULL,
    s_acctbal numeric(15,2) NOT NULL,
    s_comment character varying(101) NOT NULL
) distributed by (s_suppkey);



-- 
-- Table: customer, Attribute: c_custkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.customer'::regclass,
	1::smallint,
	0::real,
	4::integer,
	-1::real,
	0::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	97::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::int4[],
	'{804,2921948,5863959,8787979,11802789,14784434,17816642,20845138,23890922,26778640,29746505,32749801,35795334,38741386,41761423,44846847,47954678,51000547,53971082,56912203,59868521,62836009,65897832,68906877,72003218,74983822,74997699}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: customer, Attribute: c_name
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.customer'::regclass,
	2::smallint,
	0::real,
	19::integer,
	-1::real,
	0::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	0::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: customer, Attribute: c_address
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.customer'::regclass,
	3::smallint,
	0::real,
	26::integer,
	-1::real,
	0::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	0::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: customer, Attribute: c_nationkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.customer'::regclass,
	4::smallint,
	0::real,
	4::integer,
	25::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{0.0428491,0.0427156,0.0423152,0.0421283,0.0414876,0.0410337,0.0409002,0.0406066,0.0406066,0.0398857,0.0396989,0.0396722,0.0396722,0.0396188,0.0393251,0.0393251,0.0391916,0.0391916,0.0390314,0.0388712,0.0386844,0.038631,0.0382038,0.0382038,0.0381504}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{23,2,15,19,11,8,9,18,13,24,22,1,12,4,3,7,21,16,10,5,17,14,6,0,20}'::int4[],
	'{0,1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,21,22,23,24}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: customer, Attribute: c_phone
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.customer'::regclass,
	5::smallint,
	0::real,
	16::integer,
	-1::real,
	0::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	1058::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::bpchar[],
	'{10-100-191-7266,11-141-431-6620,12-146-228-8482,12-989-498-7126,14-102-488-8228,15-110-736-8802,16-134-382-8692,17-180-990-4687,18-179-696-6251,19-168-563-7001,20-156-810-2884,21-169-585-7571,22-134-156-5368,23-142-180-9482,24-124-668-5127,25-146-190-8013,26-105-145-1566,27-125-452-1979,28-158-715-8691,29-138-317-9725,29-986-797-4558,31-131-791-7114,32-156-414-9083,33-147-732-1957,33-991-436-9035,34-994-434-4065,34-997-979-7892}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: customer, Attribute: c_acctbal
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.customer'::regclass,
	6::smallint,
	0::real,
	8::integer,
	1.02349e+06::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1752::oid,
	1754::oid,
	0::oid,
	0::oid,
	'{0.000106789,8.00918e-05,8.00918e-05,8.00918e-05,8.00918e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05,5.33946e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{1326.23,6331.71,7152.66,8601.76,6948.19,8351.64,5258.77,8026.44,3748.38,3272.33,9381.39,6373.51,6483.41,7139.38,8439.24,2742.33,7603.02,24.43,3497.27,6354.67,2552.85,592.58,9742.41,8182.96,9651.53}'::numeric[],
	'{-999.66,-528.82,-82.74,363.12,801.53,1249.85,1674.72,2111.78,2532.23,2966.11,3412.64,3850.63,4281.29,4720.86,5170.36,5625.04,6066.04,6511.02,6960.76,7389.45,7820.73,8247.92,8669.04,9108.46,9560.62,9998.53,9999.71}'::numeric[],
	NULL::numeric[],
	NULL::numeric[]);

-- 
-- Table: customer, Attribute: c_mktsegment
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.customer'::regclass,
	7::smallint,
	0::real,
	11::integer,
	5::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.201458,0.200203,0.199776,0.199455,0.199108}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"HOUSEHOLD ","BUILDING  ","FURNITURE ","MACHINERY ",AUTOMOBILE}'::bpchar[],
	'{AUTOMOBILE,"BUILDING  ","FURNITURE ","HOUSEHOLD ","MACHINERY "}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: customer, Attribute: c_comment
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.customer'::regclass,
	8::smallint,
	0::real,
	73::integer,
	-0.760944::real,
	1::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	98::oid,
	0::oid,
	0::oid,
	0::oid,
	'{5.33946e-05,5.33946e-05,5.33946e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05,2.66973e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"uests. carefully ironic asymptotes wake carefully across the courts. special, furious requests sleep. si"," ironic orbits. slyly express requests haggle across the final dependencies! unusual,","ets haggle slyly carefully bold requests. ironic theodolites wake quickly. carefully final pi"," requests cajole against the ironic pinto beans. slyly regular packages cajole furiously. blithely e"," use furiously according to the ","nst the sly, silent deposits boost ironic, bold requests. quickly regular theodolites integrate blithely quickly fi","lly special instructions. ironic, final requests nag alongside","ently regular accounts cajole carefully silent theodolites. final, regular foxes nag blithely regular request","s cajole blithely carefully regular warthogs. furio","ackages. pending courts wake. furiously u","ly final deposits wake slyly among th"," silent grouches. instructions sleep? furiously special deposits sleep b"," pinto beans lose fluffily. slyly silent theodolites cajole fluffily according to ","regular accounts eat blithely. theodolites according to the express pinto","r accounts are furiously. quickly enticing braids are about th"," escapades. blithely regular pinto beans detect carefully carefully permanent tithes. fluffi","ing packages are slyly across the blithely ironic accounts. regular pl","ackages. slyly special deposits sleep slyly alongside of the quickly e","ourts haggle quickly blithely express packages. carefully final requests haggle quickly blithely ironic ideas."," accounts. unusual accounts nag fluffi","lent theodolites kindle slyly ","ites use! blithely special packa","quickly across the packages. final ins","the regular deposits. fluffily special accoun","eposits. slyly quiet deposits use blithely pending, unusual ideas. furiously bold excu"}'::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: lineitem, Attribute: l_orderkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	1::smallint,
	0::real,
	8::integer,
	2.83108e+08::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	410::oid,
	412::oid,
	0::oid,
	0::oid,
	'{4.61276e-05,4.61276e-05,4.61276e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{684660003,2498724164,2601169125,665000674,966822981,70399204,2970597478,2001386055,291876741,1402120835,2416267206,1493869799,873810726,247996930,2143385415,1485096743,2140300835,2048646853,639228836,1250075495,210992546,1999492995,678628385,2045559744,395294948}'::int8[],
	'{63270,122175974,245661766,368423714,481598659,603820005,715990439,832983360,955418464,1078067687,1197146500,1315714816,1434740995,1556093057,1675966246,1797661605,1918969792,2041767175,2162361312,2279143781,2398070019,2510123811,2635543239,2757323586,2881136101,2999710886,2999977856}'::int8[],
	NULL::int8[],
	NULL::int8[]);

-- 
-- Table: lineitem, Attribute: l_partkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	2::smallint,
	0::real,
	4::integer,
	5.42498e+07::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05,2.30638e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{74816973,99986397,35934058,83893509,24494308,38058554,64215895,71106066,94717702,2166665,33238873,76663389,89934431,41370087,38358229,62637654,79148976,44613095,88856810,94549934,29688330,14757870,81756903,81285545,52654208}'::int4[],
	'{3514,3778791,7623228,11814393,15941479,20062466,24081414,28064591,32197294,36158321,39989279,43964528,48004896,52096756,56105946,60253290,64325207,68313587,72237369,76164467,80046212,83991569,87905531,91954431,95975224,99985038,99996317}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: lineitem, Attribute: l_suppkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	3::smallint,
	0::real,
	4::integer,
	5.07803e+06::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{571170,342194,4232105,2317002,1649152,449904,790709,2556241,460723,2261648,225307,2522688,3140446,29637,1236417,796525,4361999,1871637,4087599,4459098,2658631,2278367,3121922,57515,2428966}'::int4[],
	'{254,199905,397439,595963,789527,987880,1192416,1393289,1594098,1792035,1997828,2198858,2397260,2595017,2789066,2991306,3190760,3387653,3581070,3778384,3982147,4182247,4389723,4592954,4799542,4999295,4999864}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: lineitem, Attribute: l_linenumber
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	4::smallint,
	0::real,
	4::integer,
	7::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{0.250081,0.213156,0.178814,0.14258,0.1075,0.0717515,0.0361179}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{1,2,3,4,5,6,7}'::int4[],
	'{1,2,3,4,5,6,7}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: lineitem, Attribute: l_quantity
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	5::smallint,
	0::real,
	7::integer,
	50::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1752::oid,
	1754::oid,
	0::oid,
	0::oid,
	'{0.0212418,0.0211726,0.0210342,0.0210111,0.0209881,0.0209419,0.0208266,0.0207574,0.0207344,0.0206421,0.020596,0.0205729,0.0204345,0.0203884,0.0203423,0.0203192,0.0202961,0.0202961,0.0202269,0.0202039,0.0202039,0.0202039,0.0201808,0.0201578,0.0200886}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{1.00,5.00,6.00,4.00,23.00,11.00,31.00,36.00,28.00,12.00,42.00,2.00,50.00,7.00,29.00,32.00,17.00,22.00,45.00,43.00,15.00,48.00,41.00,44.00,47.00}'::numeric[],
	'{1.00,2.00,4.00,6.00,8.00,10.00,12.00,14.00,16.00,18.00,20.00,22.00,24.00,26.00,28.00,30.00,32.00,34.00,36.00,38.00,41.00,42.00,44.00,46.00,48.00,50.00}'::numeric[],
	NULL::numeric[],
	NULL::numeric[]);

-- 
-- Table: lineitem, Attribute: l_extendedprice
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	6::smallint,
	0::real,
	10::integer,
	2.69251e+06::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1752::oid,
	1754::oid,
	0::oid,
	0::oid,
	'{4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05,4.61276e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{1585.12,54070.70,36389.10,29623.92,70271.12,28700.28,19146.40,55276.36,53105.76,8911.28,50629.60,23917.60,66085.44,34047.43,45696.96,22102.08,38978.10,25021.44,39818.16,72407.16,60931.20,44936.00,36705.20,46454.52,55112.40}'::numeric[],
	'{910.92,3461.73,6301.72,9103.08,11906.10,14773.44,17681.70,20503.04,23425.87,26367.39,29258.19,32097.88,35049.00,37955.84,40728.45,43636.62,46445.00,49474.18,52516.82,55719.30,59404.64,63507.36,68282.28,73785.77,82066.50,102182.00,103239.50}'::numeric[],
	NULL::numeric[],
	NULL::numeric[]);

-- 
-- Table: lineitem, Attribute: l_discount
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	7::smallint,
	0::real,
	6::integer,
	11::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1752::oid,
	1754::oid,
	0::oid,
	0::oid,
	'{0.0941695,0.0934314,0.0923936,0.0914249,0.0913557,0.0910328,0.0905254,0.0901794,0.0892338,0.0886342,0.0876194}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{0.03,0.10,0.08,0.09,0.01,0.04,0.00,0.06,0.05,0.07,0.02}'::numeric[],
	'{0.00,0.01,0.02,0.03,0.04,0.05,0.06,0.07,0.08,0.09,0.10}'::numeric[],
	NULL::numeric[],
	NULL::numeric[]);

-- 
-- Table: lineitem, Attribute: l_tax
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	8::smallint,
	0::real,
	6::integer,
	9::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1752::oid,
	1754::oid,
	0::oid,
	0::oid,
	'{0.113197,0.112597,0.112367,0.112021,0.11179,0.111191,0.109853,0.108515,0.108469}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{0.01,0.06,0.07,0.08,0.05,0.02,0.04,0.03,0.00}'::numeric[],
	'{0.00,0.01,0.02,0.03,0.04,0.05,0.06,0.07,0.08}'::numeric[],
	NULL::numeric[],
	NULL::numeric[]);

-- 
-- Table: lineitem, Attribute: l_returnflag
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	9::smallint,
	0::real,
	2::integer,
	3::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.508072,0.247082,0.244845}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{N,A,R}'::bpchar[],
	'{A,N,R}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: lineitem, Attribute: l_linestatus
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	10::smallint,
	0::real,
	2::integer,
	2::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.501707,0.498293}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{O,F}'::bpchar[],
	'{F,O}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: lineitem, Attribute: l_shipdate
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	11::smallint,
	0::real,
	4::integer,
	2515.75::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1093::oid,
	1095::oid,
	0::oid,
	0::oid,
	'{0.000761105,0.000761105,0.000738041,0.000738041,0.000714978,0.000714978,0.000714978,0.000714978,0.000714978,0.000691914,0.000691914,0.000691914,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.000645786,0.000645786,0.000645786,0.000645786}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{1992-04-25,1998-07-20,1994-12-05,1998-07-10,1996-01-06,1997-06-18,1996-02-25,1996-07-10,1998-07-09,1996-11-25,1995-09-01,1994-02-12,1997-07-26,1998-06-22,1996-05-30,1997-12-22,1998-05-07,1992-08-29,1993-09-08,1996-09-24,1994-06-22,1995-09-10,1995-05-05,1995-03-04,1996-01-23}'::date[],
	'{1992-01-05,1992-06-03,1992-09-04,1992-12-13,1993-03-20,1993-06-28,1993-10-05,1994-01-09,1994-04-15,1994-07-19,1994-10-23,1995-01-27,1995-05-04,1995-08-11,1995-11-16,1996-02-21,1996-05-29,1996-08-31,1996-12-04,1997-03-12,1997-06-18,1997-09-21,1997-12-25,1998-03-29,1998-06-30,1998-11-23,1998-11-30}'::date[],
	NULL::date[],
	NULL::date[]);

-- 
-- Table: lineitem, Attribute: l_commitdate
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	12::smallint,
	0::real,
	4::integer,
	2462.34::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1093::oid,
	1095::oid,
	0::oid,
	0::oid,
	'{0.000738041,0.000738041,0.000738041,0.000714978,0.000714978,0.000714978,0.000714978,0.000691914,0.000691914,0.000691914,0.000691914,0.000691914,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.00066885,0.000645786,0.000645786}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{1995-05-14,1992-06-23,1998-01-31,1992-08-03,1993-01-12,1993-04-05,1996-05-08,1995-09-10,1995-01-25,1996-01-26,1994-07-17,1992-08-14,1994-10-30,1996-01-22,1995-10-12,1998-01-16,1992-05-04,1994-12-27,1996-08-27,1998-05-10,1994-06-02,1997-07-26,1993-03-16,1996-03-23,1993-11-24}'::date[],
	'{1992-02-02,1992-06-04,1992-09-03,1992-12-12,1993-03-20,1993-06-25,1993-10-04,1994-01-09,1994-04-14,1994-07-19,1994-10-22,1995-01-27,1995-05-03,1995-08-10,1995-11-13,1996-02-19,1996-05-29,1996-08-28,1996-12-03,1997-03-11,1997-06-17,1997-09-20,1997-12-26,1998-03-29,1998-06-30,1998-10-25,1998-10-31}'::date[],
	NULL::date[],
	NULL::date[]);

-- 
-- Table: lineitem, Attribute: l_receiptdate
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	13::smallint,
	0::real,
	4::integer,
	2521.81::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1093::oid,
	1095::oid,
	0::oid,
	0::oid,
	'{0.000830297,0.000784169,0.000784169,0.000761105,0.000761105,0.000738041,0.000738041,0.000738041,0.000738041,0.000738041,0.000738041,0.000738041,0.000714978,0.000714978,0.000714978,0.000691914,0.000691914,0.000691914,0.000691914,0.000691914,0.000691914,0.000691914,0.00066885,0.00066885,0.00066885}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{1996-06-27,1995-04-20,1997-10-21,1997-04-18,1996-12-24,1994-07-15,1992-08-19,1995-06-21,1998-05-27,1994-06-16,1994-10-15,1997-12-24,1994-06-01,1992-09-09,1997-06-17,1992-05-30,1997-07-24,1998-05-14,1995-01-09,1998-08-21,1994-03-24,1994-09-30,1998-01-13,1997-02-17,1998-01-31}'::date[],
	'{1992-01-18,1992-06-18,1992-09-20,1992-12-28,1993-04-05,1993-07-13,1993-10-21,1994-01-25,1994-05-01,1994-08-03,1994-11-07,1995-02-13,1995-05-20,1995-08-27,1995-12-01,1996-03-07,1996-06-14,1996-09-15,1996-12-19,1997-03-27,1997-07-04,1997-10-05,1998-01-10,1998-04-13,1998-07-15,1998-12-15,1998-12-18}'::date[],
	NULL::date[],
	NULL::date[]);

-- 
-- Table: lineitem, Attribute: l_shipinstruct
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	14::smallint,
	0::real,
	26::integer,
	4::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.252848,0.252687,0.248097,0.246367}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"TAKE BACK RETURN         ","NONE                     ","COLLECT COD              ","DELIVER IN PERSON        "}'::bpchar[],
	'{"COLLECT COD              ","DELIVER IN PERSON        ","NONE                     ","TAKE BACK RETURN         "}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: lineitem, Attribute: l_shipmode
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	15::smallint,
	0::real,
	11::integer,
	7::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.146086,0.145256,0.143734,0.142165,0.141704,0.141358,0.139697}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"FOB       ","AIR       ","MAIL      ","TRUCK     ","REG AIR   ","SHIP      ","RAIL      "}'::bpchar[],
	'{"AIR       ","FOB       ","MAIL      ","RAIL      ","REG AIR   ","SHIP      ","TRUCK     "}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: lineitem, Attribute: l_comment
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.lineitem'::regclass,
	16::smallint,
	0::real,
	27::integer,
	1.23854e+06::real,
	1::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	98::oid,
	0::oid,
	0::oid,
	0::oid,
	'{0.000230638,0.00018451,0.000161447,0.000161447,0.000161447,0.000161447,0.000138383,0.000138383,0.000115319,0.000115319,0.000115319,0.000115319,0.000115319,0.000115319,0.000115319,0.000115319,0.000115319,0.000115319,0.000115319,9.22552e-05,9.22552e-05,9.22552e-05,9.22552e-05,9.22552e-05,9.22552e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"furiously "," carefully ","deposits. ","y regular "," furiously"," theodolit",structions," instructio"," furiously ","ly regular ","ly regular","e carefull","s. regular","e furiousl"," instructi",". furiousl"," packages ","carefully ","ly ironic "," regular request",dependencie," final packages",". furiously"," carefully"," deposits. "}'::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: nation, Attribute: n_nationkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.nation'::regclass,
	1::smallint,
	0::real,
	4::integer,
	-1::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{7,17,12,3,8,19,14,4,11,2,13,6,9,0,24,5,16,21,22,23,20,18,15,1,10}'::int4[],
	'{}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: nation, Attribute: n_name
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.nation'::regclass,
	2::smallint,
	0::real,
	26::integer,
	-1::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"GERMANY                  ","FRANCE                   ","RUSSIA                   ","KENYA                    ","UNITED KINGDOM           ","ARGENTINA                ","INDONESIA                ","IRAN                     ","BRAZIL                   ","MOROCCO                  ","CHINA                    ","INDIA                    ","VIETNAM                  ","JAPAN                    ","JORDAN                   ","PERU                     ","ROMANIA                  ","MOZAMBIQUE               ","CANADA                   ","IRAQ                     ","UNITED STATES            ","EGYPT                    ","ETHIOPIA                 ","ALGERIA                  ","SAUDI ARABIA             "}'::bpchar[],
	'{}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: nation, Attribute: n_regionkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.nation'::regclass,
	3::smallint,
	0::real,
	4::integer,
	-0.2::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{0.2,0.2,0.2,0.2,0.2}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{3,4,2,0,1}'::int4[],
	'{}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: nation, Attribute: n_comment
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.nation'::regclass,
	4::smallint,
	0::real,
	75::integer,
	-1::real,
	1::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	98::oid,
	0::oid,
	0::oid,
	0::oid,
	'{0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04,0.04}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{" pending excuses haggle furiously deposits. pending, express pinto beans wake fluffily past t"," haggle. carefully final deposits detect slyly agai","platelets. blithely pending dependencies use fluffily across the even pinto beans. carefully silent accoun","y alongside of the pending deposits. carefully special packages are about the ironic forges. slyly special ","rns. blithely bold courts among the closely regular packages use furiously bold platelets?","refully final requests. regular, ironi","eans boost carefully special requests. accounts are. carefull","y final packages. slow foxes cajole quickly. quickly silent platelets breach ironic accounts. unusual pinto be","ic deposits are blithely about the carefully regular pa","ular asymptotes are about the furious multipliers. express dependencies nag above the ironically ironic account","al foxes promise slyly according to the regular accounts. bold requests alon","efully alongside of the slyly final dependencies. ","ss excuses cajole slyly across the packages. deposits print aroun","ts. silent requests haggle. closely express packages sleep across the blithely"," slyly express asymptotes. regular deposits haggle slyly. carefully ironic hockey players sleep blithely. carefull"," requests against the platelets use never according to the quickly regular pint","s. ironic, unusual asymptotes wake blithely r","y above the carefully unusual theodolites. final dugouts are quickly across the furiously regular d","eas hang ironic, silent packages. slyly regular packages are furiously over the tithes. fluffily bold","hely enticingly express accounts. even, final ","l platelets. regular accounts x-ray: unusual, regular acco","nic deposits boost atop the quickly final requests? quickly regula","ously. final, express gifts cajole a","c dependencies. furiously express notornis sleep slyly regular accounts. ideas sleep. depos","ven packages wake quickly. regu"}'::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: orders, Attribute: o_orderkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.orders'::regclass,
	1::smallint,
	0::real,
	8::integer,
	-1::real,
	0::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	412::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::int8[],
	'{61892,119652261,243055783,365757188,486140609,604643847,727829185,844894951,963745156,1080925607,1204339687,1320041447,1435639654,1554755745,1684670886,1808222915,1926850179,2047859874,2168544097,2289931588,2405096417,2522349095,2641814116,2761700166,2880131844,2998909952,2999943650}'::int8[],
	NULL::int8[],
	NULL::int8[]);

-- 
-- Table: orders, Attribute: o_custkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.orders'::regclass,
	2::smallint,
	0::real,
	4::integer,
	5.58477e+07::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{39105769,63807202,72410635,18846655,62315263,70525174,14436991,23197640,59424196,57096607,43924048,5033527,59821054,35895631,66929311,38935051,67681264,1100941,74896732,10194644,5643805,23958109,32067908,5807269,11897753}'::int4[],
	'{1453,2973184,5965483,9029530,12087334,15028252,18069238,21116027,24112660,27070826,30064510,32947372,36016186,39153244,42008777,44991388,47992295,51063148,54016762,56970046,59948365,62894089,65982722,68987845,72089479,74983168,74999722}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: orders, Attribute: o_orderstatus
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.orders'::regclass,
	3::smallint,
	0::real,
	2::integer,
	3::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.487203,0.486596,0.0262013}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{F,O,P}'::bpchar[],
	'{F,O,P}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: orders, Attribute: o_totalprice
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.orders'::regclass,
	4::smallint,
	0::real,
	10::integer,
	4.19851e+07::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1752::oid,
	1754::oid,
	0::oid,
	0::oid,
	'{4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{51720.84,336372.40,253417.01,153220.96,33107.69,117940.45,198176.51,183294.55,74513.29,82571.35,259080.20,170610.02,260342.50,130267.63,187183.44,58940.35,176680.48,84023.62,59491.86,249518.04,159130.57,262426.61,186030.55,108733.56,236015.03}'::numeric[],
	'{887.99,18823.19,32959.30,45056.42,55345.18,65221.12,75531.97,85832.71,96300.81,107620.82,118015.57,128845.57,139675.31,150230.96,160829.22,172131.44,183414.41,194926.94,207069.97,219616.63,233419.72,247929.48,265036.75,285932.07,315923.62,437564.16,517486.23}'::numeric[],
	NULL::numeric[],
	NULL::numeric[]);

-- 
-- Table: orders, Attribute: o_orderdate
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.orders'::regclass,
	5::smallint,
	0::real,
	4::integer,
	2406::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1093::oid,
	1095::oid,
	0::oid,
	0::oid,
	'{0.000874997,0.000802081,0.000777775,0.000777775,0.00075347,0.00075347,0.00075347,0.00075347,0.000729164,0.000729164,0.000704859,0.000704859,0.000704859,0.000704859,0.000680553,0.000680553,0.000680553,0.000680553,0.000656248,0.000656248,0.000656248,0.000656248,0.000656248,0.000656248,0.000656248}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{1997-09-22,1995-02-19,1996-08-13,1997-06-19,1996-12-31,1995-05-09,1993-10-18,1996-05-06,1994-12-08,1992-05-06,1995-09-08,1992-11-10,1995-10-20,1996-02-10,1996-03-26,1993-05-02,1997-08-31,1992-10-24,1992-09-25,1992-07-21,1993-02-04,1993-02-12,1995-08-20,1992-10-15,1995-04-02}'::date[],
	'{1992-01-01,1992-04-07,1992-07-09,1992-10-11,1993-01-13,1993-04-20,1993-07-25,1993-10-29,1994-02-04,1994-05-11,1994-08-21,1994-11-25,1995-02-28,1995-06-04,1995-09-08,1995-12-15,1996-03-22,1996-06-25,1996-10-02,1997-01-06,1997-04-13,1997-07-18,1997-10-20,1998-01-21,1998-04-25,1998-08-02}'::date[],
	NULL::date[],
	NULL::date[]);

-- 
-- Table: orders, Attribute: o_orderpriority
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.orders'::regclass,
	6::smallint,
	0::real,
	16::integer,
	5::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.200787,0.200472,0.200253,0.199281,0.199208}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"4-NOT SPECIFIED","2-HIGH         ","1-URGENT       ","3-MEDIUM       ","5-LOW          "}'::bpchar[],
	'{"1-URGENT       ","2-HIGH         ","3-MEDIUM       ","4-NOT SPECIFIED","5-LOW          "}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: orders, Attribute: o_clerk
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.orders'::regclass,
	7::smallint,
	0::real,
	16::integer,
	488990::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{9.72219e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05,7.29164e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{Clerk#000208581,Clerk#000292245,Clerk#000088870,Clerk#000097719,Clerk#000105522,Clerk#000387151,Clerk#000275998,Clerk#000018085,Clerk#000126515,Clerk#000161705,Clerk#000172216,Clerk#000235515,Clerk#000489479,Clerk#000279130,Clerk#000045535,Clerk#000150794,Clerk#000395144,Clerk#000420523,Clerk#000486717,Clerk#000197312,Clerk#000402389,Clerk#000304725,Clerk#000294622,Clerk#000002206,Clerk#000468131}'::bpchar[],
	'{Clerk#000000011,Clerk#000020130,Clerk#000040407,Clerk#000060196,Clerk#000080283,Clerk#000100917,Clerk#000120280,Clerk#000140140,Clerk#000159663,Clerk#000180207,Clerk#000199184,Clerk#000219792,Clerk#000239685,Clerk#000259956,Clerk#000279872,Clerk#000300064,Clerk#000319923,Clerk#000339438,Clerk#000359262,Clerk#000379550,Clerk#000399768,Clerk#000420258,Clerk#000439369,Clerk#000459093,Clerk#000479016,Clerk#000499839,Clerk#000499998}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: orders, Attribute: o_shippriority
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.orders'::regclass,
	8::smallint,
	0::real,
	4::integer,
	1::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{1}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{0}'::int4[],
	'{0}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: orders, Attribute: o_comment
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.orders'::regclass,
	9::smallint,
	0::real,
	49::integer,
	3.81924e+07::real,
	1::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	98::oid,
	0::oid,
	0::oid,
	0::oid,
	'{4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,4.86109e-05,2.43055e-05,2.43055e-05,2.43055e-05,2.43055e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"ly final accounts. ","carefully regular d"," ironic deposits cajole again","regular instructions","e furiously regular reque","ously regular accounts","nding courts use slyly after the even, final asy","instructions haggle","ironic pinto beans. ","y regular pinto bean","ts haggle furiously","ing theodolites. sl"," the furiously even","above the fluffily regular","express deposits. e"," furiously regular ","unts. furiously pending ","the quickly regular requests","equests. slyly iron"," regular pinto bean"," regular requests across the fluffily unusual asymptotes shall have to sl","packages haggle among the pending deposits. special, f"," packages. slyly ironic deposits grow furiously. regularly ironic","usly blithely bold pearls. carefully pending accounts detect car","side of the final requests cajole quickly after the i"}'::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: part, Attribute: p_partkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.part'::regclass,
	1::smallint,
	0::real,
	4::integer,
	-1::real,
	0::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	97::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::int4[],
	'{2184,4034646,8033425,12028701,15957541,19878421,23863338,27932589,31819643,35915302,39819845,43873091,47925930,51821011,55820315,59832091,63933931,67992462,72090902,76191246,80167793,84131791,88015771,91885736,95986573,99955210,99999134}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: part, Attribute: p_name
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.part'::regclass,
	2::smallint,
	0::real,
	34::integer,
	-1::real,
	0::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	0::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: part, Attribute: p_mfgr
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.part'::regclass,
	3::smallint,
	0::real,
	26::integer,
	5::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.202716,0.20029,0.199842,0.199604,0.197547}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"Manufacturer#4           ","Manufacturer#5           ","Manufacturer#2           ","Manufacturer#1           ","Manufacturer#3           "}'::bpchar[],
	'{"Manufacturer#1           ","Manufacturer#2           ","Manufacturer#3           ","Manufacturer#4           ","Manufacturer#5           "}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: part, Attribute: p_brand
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.part'::regclass,
	4::smallint,
	0::real,
	11::integer,
	25::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.0433281,0.0431698,0.0417985,0.041192,0.0410338,0.0408228,0.0407964,0.0407173,0.0403217,0.0401899,0.0399789,0.0399789,0.0399262,0.0398998,0.0397416,0.0396888,0.0394515,0.0394251,0.0392141,0.0389504,0.0388449,0.038634,0.0384494,0.0375264,0.0369198}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"Brand#42  ","Brand#15  ","Brand#25  ","Brand#43  ","Brand#13  ","Brand#21  ","Brand#55  ","Brand#44  ","Brand#22  ","Brand#32  ","Brand#51  ","Brand#23  ","Brand#54  ","Brand#52  ","Brand#31  ","Brand#53  ","Brand#33  ","Brand#12  ","Brand#35  ","Brand#34  ","Brand#45  ","Brand#41  ","Brand#14  ","Brand#11  ","Brand#24  "}'::bpchar[],
	'{"Brand#11  ","Brand#12  ","Brand#13  ","Brand#14  ","Brand#15  ","Brand#21  ","Brand#22  ","Brand#23  ","Brand#25  ","Brand#31  ","Brand#32  ","Brand#33  ","Brand#34  ","Brand#35  ","Brand#41  ","Brand#42  ","Brand#43  ","Brand#44  ","Brand#45  ","Brand#51  ","Brand#52  ","Brand#54  ","Brand#55  "}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: part, Attribute: p_type
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.part'::regclass,
	5::smallint,
	0::real,
	21::integer,
	150::real,
	1::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	98::oid,
	0::oid,
	0::oid,
	0::oid,
	'{0.00785865,0.00783228,0.00767405,0.00751582,0.00743671,0.00738397,0.0073576,0.00733122,0.00733122,0.00733122,0.00727848,0.00725211,0.00725211,0.00722574,0.00722574,0.007173,0.007173,0.007173,0.007173,0.00714662,0.00714662,0.00712025,0.00709388,0.00709388,0.00706751}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"ECONOMY PLATED STEEL","ECONOMY POLISHED BRASS","ECONOMY BRUSHED TIN","ECONOMY PLATED COPPER","STANDARD POLISHED BRASS","SMALL PLATED COPPER","ECONOMY ANODIZED STEEL","ECONOMY BRUSHED NICKEL","ECONOMY PLATED TIN","ECONOMY BURNISHED NICKEL","LARGE BURNISHED BRASS","LARGE PLATED BRASS","MEDIUM ANODIZED COPPER","SMALL ANODIZED TIN","LARGE PLATED STEEL","PROMO POLISHED TIN","MEDIUM POLISHED NICKEL","MEDIUM BURNISHED BRASS","SMALL BURNISHED STEEL","ECONOMY POLISHED STEEL","MEDIUM ANODIZED STEEL","PROMO PLATED NICKEL","LARGE BURNISHED NICKEL","MEDIUM BRUSHED STEEL","ECONOMY BURNISHED BRASS"}'::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: part, Attribute: p_size
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.part'::regclass,
	6::smallint,
	0::real,
	4::integer,
	50::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{0.0216245,0.0212816,0.0212553,0.0212289,0.0210443,0.0210443,0.0210179,0.0209388,0.0207542,0.0207015,0.0207015,0.0206487,0.020596,0.0205432,0.0204641,0.0203059,0.0202532,0.0202004,0.0202004,0.0201477,0.0201477,0.0200949,0.0200422,0.0200158,0.0199631}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{27,40,34,32,50,21,45,26,48,46,25,44,42,11,4,8,9,36,31,13,5,3,29,15,20}'::int4[],
	'{1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31,33,35,37,39,41,43,45,46,48,50}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: part, Attribute: p_container
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.part'::regclass,
	7::smallint,
	0::real,
	11::integer,
	40::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.0262395,0.0262131,0.0261076,0.0260285,0.025923,0.0258966,0.0258703,0.0258175,0.025712,0.0256593,0.0256593,0.0256329,0.0254483,0.0253692,0.0253428,0.0252901,0.0252901,0.0251846,0.0250791,0.0249736,0.0248945,0.0248154,0.024789,0.0247363,0.0247363}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"WRAP BAG  ","MED PKG   ","WRAP PKG  ","JUMBO BOX ","JUMBO CASE","WRAP DRUM ","LG CAN    ","JUMBO JAR ","JUMBO PACK","JUMBO BAG ","SM CAN    ","SM DRUM   ","SM JAR    ","LG PACK   ","MED CAN   ","LG DRUM   ","WRAP CASE ","SM CASE   ","JUMBO DRUM","LG BAG    ","SM BAG    ","LG JAR    ","WRAP JAR  ","LG CASE   ","SM PACK   "}'::bpchar[],
	'{"JUMBO BAG ","JUMBO BOX ","JUMBO CASE","JUMBO DRUM","JUMBO PACK","JUMBO PKG ","LG BOX    ","LG CASE   ","LG DRUM   ","LG PACK   ","LG PKG    ","MED BOX   ","MED CASE  ","MED DRUM  ","MED PACK  ","SM BAG    ","SM BOX    ","SM CASE   ","SM DRUM   ","SM PACK   ","WRAP BAG  ","WRAP BOX  ","WRAP CASE ","WRAP DRUM ","WRAP PACK ","WRAP PKG  "}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: part, Attribute: p_retailprice
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.part'::regclass,
	8::smallint,
	0::real,
	8::integer,
	107651::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1752::oid,
	1754::oid,
	0::oid,
	0::oid,
	'{0.000131857,0.000131857,0.000131857,0.000131857,0.000131857,0.000131857,0.000131857,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485,0.000105485}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{1701.00,1135.47,1184.41,1383.89,1865.31,1202.43,1243.40,1757.91,1911.54,1630.79,1438.70,1571.20,1738.70,1348.58,1671.72,1516.98,1823.17,1961.74,1305.92,1091.32,1620.74,1941.17,1661.08,1358.44,1753.02}'::numeric[],
	'{900.03,1020.96,1074.29,1116.61,1156.85,1197.79,1238.08,1277.78,1317.59,1358.91,1397.27,1437.00,1477.94,1519.24,1560.33,1599.55,1640.29,1681.02,1719.61,1759.89,1800.78,1841.23,1881.69,1922.11,1973.22,2083.39,2094.18}'::numeric[],
	NULL::numeric[],
	NULL::numeric[]);

-- 
-- Table: part, Attribute: p_comment
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.part'::regclass,
	9::smallint,
	0::real,
	14::integer,
	121849::real,
	1::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	98::oid,
	0::oid,
	0::oid,
	0::oid,
	'{0.000501055,0.000474684,0.000448312,0.000448312,0.000421941,0.000421941,0.000421941,0.000421941,0.000421941,0.000421941,0.00039557,0.00039557,0.00039557,0.00039557,0.00039557,0.000369198,0.000369198,0.000369198,0.000369198,0.000369198,0.000369198,0.000369198,0.000369198,0.000342827,0.000342827}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{" care",uriou,iously,"lyly ",egula,furio,arefully,refully," packa"," final "," slyl",fully,"ronic "," deposi",furiousl," regu",lithely,"arefully ",arefu,packages,caref,riousl,regular," acco",uriously}'::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: partsupp, Attribute: ps_partkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.partsupp'::regclass,
	1::smallint,
	0::real,
	4::integer,
	-0.20436::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{45103944,90376746,29576586,98082786,69829133,28227832,41329757,10852676,72493567,9766328,87820714,30995304,21438586,58827416,80241615,4814718,8752986,39556982,75633063,30374373,40349610,23971462,80473032,59725828,13992782}'::int4[],
	'{1353,3810511,7883465,11871349,15839722,19831885,23772438,27894006,31807139,35802350,39601411,43485366,47544279,51433702,55739695,59790643,63788168,67761612,71789861,75848464,79869092,83944519,87947251,91927837,95998197,99980787,99996634}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: partsupp, Attribute: ps_suppkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.partsupp'::regclass,
	2::smallint,
	0::real,
	4::integer,
	5.53321e+06::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05,4.98318e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{2504243,2698290,515527,1189017,4069774,3745950,2087453,779459,4989047,2487265,2182961,2779653,189788,2811164,2243101,627303,3788784,4549789,4583609,3855610,3086600,1658041,3585719,1500550,1288256}'::int4[],
	'{124,201570,404602,600433,790524,991895,1202777,1393680,1587079,1781735,1984692,2193781,2405959,2607920,2794278,2988335,3180103,3379907,3590409,3793786,3992046,4189129,4388420,4587739,4794408,4998460,4999773}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: partsupp, Attribute: ps_availqty
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.partsupp'::regclass,
	3::smallint,
	0::real,
	4::integer,
	10000.4::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{0.000323907,0.000323907,0.000323907,0.000298991,0.000298991,0.000298991,0.000298991,0.000298991,0.000298991,0.000298991,0.000298991,0.000298991,0.000298991,0.000298991,0.000298991,0.000274075,0.000274075,0.000274075,0.000274075,0.000274075,0.000274075,0.000274075,0.000274075,0.000274075,0.000274075}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{9228,6888,7938,4581,9064,2417,4233,5575,307,2588,8331,9577,3091,3163,8735,6016,1107,7725,3552,7206,8645,4454,3845,4806,2399}'::int4[],
	'{1,397,794,1196,1601,1993,2399,2821,3208,3621,4014,4409,4812,5214,5609,6014,6395,6800,7205,7574,7979,8378,8777,9177,9583,9997,9999}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: partsupp, Attribute: ps_supplycost
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.partsupp'::regclass,
	4::smallint,
	0::real,
	8::integer,
	98854.1::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1752::oid,
	1754::oid,
	0::oid,
	0::oid,
	'{0.00012458,0.00012458,0.00012458,0.00012458,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05,9.96636e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{949.57,39.48,684.21,595.27,409.50,655.77,398.88,797.76,357.96,689.40,35.87,170.59,528.30,650.23,650.71,33.73,321.64,292.88,228.39,185.92,38.38,648.17,661.42,229.34,630.37}'::numeric[],
	'{1.01,39.05,79.11,118.88,160.40,200.41,241.42,281.84,321.02,359.21,399.82,437.80,479.63,517.55,558.10,600.56,641.25,680.98,720.20,759.01,800.10,840.53,878.79,918.81,959.67,999.71,999.93}'::numeric[],
	NULL::numeric[],
	NULL::numeric[]);

-- 
-- Table: partsupp, Attribute: ps_comment
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.partsupp'::regclass,
	5::smallint,
	0::real,
	126::integer,
	-0.506794::real,
	1::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	98::oid,
	0::oid,
	0::oid,
	0::oid,
	'{4.98318e-05,4.98318e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05,2.49159e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"the fluffily ironic deposits. carefully ironic dependencies are slyly according to the slyly final depths. bold accounts wake silent, express asymptotes. ","es across the slyly express instructions poach carefully ironic packages. furiously regular notornis sleep quickly regular platelets. regular deco","yly final requests integrate furiously. express, ironic ideas boost slyly regular pinto beans. pending packages sleep quickly after the quickly ironic dinos; blithely speci","cajole carefully. carefully regular deposits wake carefully. blithely bold packages cajole furiously. carefully ironic packages boost fluffily final accounts: qu","ng the special platelets. carefully careful sauternes nag furiously even requests. blithely final ideas haggle above the special packages. ironic"," blithely fluffily regular packages. quickly final asymptotes cajole carefully special deposits. slyly ironic forges wake slyly according to the slyly bold requests. even, ironic ","aggle during the furiously regular theodolites. dependencies cajole carefully. ironic, ironic foxes use furiously ironic platelets. fluffily regular packages affix blit","ctions boost pending dolphins. slowly slow pinto beans use among the carefully unusual pains. bold accounts across the f","ions wake furiously across the regular, final foxes. regular packages doze fluffily pending, regular she","ual requests wake. even asymptotes wake carefully ironic accounts. deposits hinder blithely permanently regular packages. express, pending packages against the quickly bol","ets sleep against the even theodolites. even theodolites cajole ironic sheaves. final, silent instructions integrate slyl","iously ironic requests. quickly final requests cajole quickly along the regular notornis. even deposits among the b","ely silent pinto beans; slyly even packages among the carefully final deposits wake furiously regular ideas. slyly final deposits wake slyly across the ",". blithely fluffy requests sleep carefully. pinto beans haggle carefully ironic ideas. brave accounts play. slyly even request","leep at the quickly even packages. quickly even packages are. careful deposits are carefully after the requests. carefully special ins","ess theodolites hang carefully. fluffily idle platelets according to the quickly unusual patterns detect slyly unusual tithes! accounts k","e carefully silent escapades cajole furiously after the slyly careful pinto beans. furiously regular packages print blithely even pinto beans. pending, regular packages promise carefully dogged","rding to the pinto beans. regular packages engage carefully finally special packages. furiously bold deposits haggle. even escapades nag fluffily. special, unusual foxes are ironic, bold pac","ithely furiously express packages. carefully regular ideas nag. quickly stealthy foxes wake. regular dolphins cajole slyly fluffily unusual deposits. furiously special dependencies haggle","en instructions. even excuses around the ironic courts thrash blithely blithe accounts. blithely special foxes bo","s. furiously bold requests wake quickly at the slyly regular foxes. carefully final accounts affix. ironic deposits maintain special multipliers. regular depths x-ray fl",". quickly final gifts wake furiously. carefully special accounts about the enticingly regular theodolit","ngage alongside of the carefully ironic deposits. slyly even accounts sleep blith","ix quickly final, even instructions. blithely unusual requests sleep according to the slyly even deposits. slyly express requests are across the re"," special theodolites. furious, stealthy deposits are carefully. carefully regular pinto beans haggle furiously along the final, bold theodolit"}'::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: region, Attribute: r_regionkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.region'::regclass,
	1::smallint,
	0::real,
	4::integer,
	-1::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{0.2,0.2,0.2,0.2,0.2}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{3,4,2,0,1}'::int4[],
	'{}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: region, Attribute: r_name
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.region'::regclass,
	2::smallint,
	0::real,
	26::integer,
	-1::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1054::oid,
	1058::oid,
	0::oid,
	0::oid,
	'{0.2,0.2,0.2,0.2,0.2}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"AFRICA                   ","ASIA                     ","MIDDLE EAST              ","EUROPE                   ","AMERICA                  "}'::bpchar[],
	'{}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: region, Attribute: r_comment
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.region'::regclass,
	3::smallint,
	0::real,
	67::integer,
	-1::real,
	1::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	98::oid,
	0::oid,
	0::oid,
	0::oid,
	'{0.2,0.2,0.2,0.2,0.2}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"uickly special accounts cajole carefully blithely close requests. carefully final asymptotes haggle furiousl","hs use ironic, even requests. s","ly final courts cajole furiously final excuse","lar deposits. blithely final packages cajole. regular waters are final requests. regular accounts are according to ","ges. thinly even pinto beans ca"}'::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: supplier, Attribute: s_suppkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.supplier'::regclass,
	1::smallint,
	0::real,
	4::integer,
	-1::real,
	0::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	97::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::int4[],
	'{75,198009,403469,595318,795815,995854,1187745,1392906,1588223,1785531,1976849,2182915,2386482,2575972,2776861,2979720,3190512,3390881,3591012,3782111,3980788,4185809,4395823,4597173,4796910,4998851,4999799}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: supplier, Attribute: s_name
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.supplier'::regclass,
	2::smallint,
	0::real,
	26::integer,
	-1::real,
	0::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	1058::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::bpchar[],
	'{"Supplier#000000075       ","Supplier#000198009       ","Supplier#000403469       ","Supplier#000595318       ","Supplier#000795815       ","Supplier#000995854       ","Supplier#001187745       ","Supplier#001392906       ","Supplier#001588223       ","Supplier#001785531       ","Supplier#001976849       ","Supplier#002182915       ","Supplier#002386482       ","Supplier#002575972       ","Supplier#002776861       ","Supplier#002979720       ","Supplier#003190512       ","Supplier#003390881       ","Supplier#003591012       ","Supplier#003782111       ","Supplier#003980788       ","Supplier#004185809       ","Supplier#004395823       ","Supplier#004597173       ","Supplier#004796910       ","Supplier#004998851       ","Supplier#004999799       "}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: supplier, Attribute: s_address
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.supplier'::regclass,
	3::smallint,
	0::real,
	25::integer,
	-1::real,
	0::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	0::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Table: supplier, Attribute: s_nationkey
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.supplier'::regclass,
	4::smallint,
	0::real,
	4::integer,
	25::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	96::oid,
	97::oid,
	0::oid,
	0::oid,
	'{0.0433444,0.0422578,0.041805,0.0415032,0.0413522,0.0413221,0.041141,0.0407184,0.0405976,0.0404769,0.0400241,0.0398732,0.039843,0.0396921,0.0396318,0.0395714,0.0395714,0.0390884,0.0389677,0.0389073,0.0388772,0.0385753,0.0378509,0.0377603,0.0372472}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{4,0,8,1,10,5,19,2,22,3,16,9,18,12,6,17,7,14,11,24,20,23,21,13,15}'::int4[],
	'{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24}'::int4[],
	NULL::int4[],
	NULL::int4[]);

-- 
-- Table: supplier, Attribute: s_phone
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.supplier'::regclass,
	5::smallint,
	0::real,
	16::integer,
	-1::real,
	0::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	0::oid,
	1058::oid,
	0::oid,
	0::oid,
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	NULL::bpchar[],
	'{10-100-736-4380,10-950-210-5611,11-921-425-5389,12-907-335-2223,13-887-266-8935,14-814-568-2290,15-794-243-4488,16-783-625-5753,17-799-138-1484,18-758-245-4627,19-756-483-8004,20-726-611-7907,21-763-871-1204,22-757-408-4955,23-797-612-2644,24-820-694-4723,25-881-963-2940,26-896-992-4679,27-907-181-9303,28-910-466-3208,29-877-770-4908,30-894-451-5759,31-951-589-8870,32-940-205-3055,33-968-179-8702,34-994-363-7909,34-999-182-8076}'::bpchar[],
	NULL::bpchar[],
	NULL::bpchar[]);

-- 
-- Table: supplier, Attribute: s_acctbal
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.supplier'::regclass,
	6::smallint,
	0::real,
	8::integer,
	-0.18404::real,
	1::smallint,
	2::smallint,
	0::smallint,
	0::smallint,
	1752::oid,
	1754::oid,
	0::oid,
	0::oid,
	'{9.05524e-05,9.05524e-05,9.05524e-05,9.05524e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05,6.03682e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{9440.09,7515.32,-58.99,4065.42,4946.64,3195.76,3934.64,7389.49,-491.31,3994.96,9371.25,8072.80,8289.40,3524.95,8447.05,4552.66,1924.19,5767.39,8679.12,1076.73,-431.52,7108.51,7372.22,2718.32,2864.92}'::numeric[],
	'{-999.49,-571.36,-115.69,306.52,727.36,1186.43,1609.38,2045.63,2493.49,2914.18,3360.65,3810.07,4243.84,4670.28,5112.98,5561.66,5978.06,6430.82,6873.89,7320.80,7758.15,8217.33,8659.01,9125.49,9556.99,9998.20,9999.51}'::numeric[],
	NULL::numeric[],
	NULL::numeric[]);

-- 
-- Table: supplier, Attribute: s_comment
-- 
INSERT INTO pg_statistic VALUES (
	'tpch500gb.supplier'::regclass,
	7::smallint,
	0::real,
	63::integer,
	-0.982448::real,
	1::smallint,
	0::smallint,
	0::smallint,
	0::smallint,
	98::oid,
	0::oid,
	0::oid,
	0::oid,
	'{6.03682e-05,6.03682e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05,3.01841e-05}'::real[],
	NULL::real[],
	NULL::real[],
	NULL::real[],
	'{"arefully regular accounts","inal warthogs wake carefully above the ideas. e","the regular, regular deposits. regular deposits ","e platelets. quickly bold sheav","ithely silent courts. silent ideas engage carefully slyly ironic foxes. blithel","ously unusual excuses haggle carefully alongside of the quickly bold ","uests. theodolites integrate fluffily bold ideas. slyly even foxes nag! fluffily ","ess asymptotes. sometimes special instructions are around the u","es. fluffily even dolphins against the blithely daring packages haggle carefully silent, ironic pac","uses integrate. quickly dogge","e the special excuses. slyly express dolphins kindle caref","silent packages. express accounts sleep. final, ironic ideas am","tructions haggle slyly. even requests nag? slyly pending requests are."," the furiously express dugouts. fluffily ironic ","ily careful accounts. pending instruction","ctions wake slyly carefully regular dependenci","ironic, regular deposits nag about the carefully express accounts. carefully final","lithely bold theodolites haggle fluff","l foxes. final theodolites engage blithely after the excuses. blithely final deposits a","ey players are furiously according to ","ts about the furiously even accounts sleep abou","ly bold requests. furiously ironic frays integrate furiously. regular, regular request","endencies sleep. slyly regular realms are quickly final deposits. silent dolphins haggle carefull","sly even packages. blithely final instructions boost. furiously ironic request","ould boost. thin dolphins affix. furiously fina"}'::varchar[],
	NULL::varchar[],
	NULL::varchar[],
	NULL::varchar[]);

-- 
-- Greenplum Database Statistics Dump complete
-- 


-- set pages and tuple counts

-- 
-- Table: nation
-- 
UPDATE pg_class
SET
	relpages = 24::int,
	reltuples = 25::int
WHERE relname = 'nation' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'tpch500gb');

-- 
-- Table: region
-- 
UPDATE pg_class
SET
	relpages = 5::int,
	reltuples = 5::int
WHERE relname = 'region' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'tpch500gb');

-- 
-- Table: part
-- 
UPDATE pg_class
SET
	relpages = 514920::int,
	reltuples = 9.81335e+07::int
WHERE relname = 'part' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'tpch500gb');

-- 
-- Table: supplier
-- 
UPDATE pg_class
SET
	relpages = 27809::int,
	reltuples = 4.91852e+06::int
WHERE relname = 'supplier' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'tpch500gb');

-- 
-- Table: partsupp
-- 
UPDATE pg_class
SET
	relpages = 2203275::int,
	reltuples = 3.91908e+08::int
WHERE relname = 'partsupp' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'tpch500gb');

-- 
-- Table: customer
-- 
UPDATE pg_class
SET
	relpages = 448938::int,
	reltuples = 7.34745e+07::int
WHERE relname = 'customer' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'tpch500gb');

-- 
-- Table: orders
-- 
UPDATE pg_class
SET
	relpages = 3418818::int,
	reltuples = 7.35569e+08::int
WHERE relname = 'orders' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'tpch500gb');

-- 
-- Table: lineitem
-- 
UPDATE pg_class
SET
	relpages = 15044639::int,
	reltuples = 2.93783e+09::real
WHERE relname = 'lineitem' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'tpch500gb');


set gp_segments_for_planner = 16;


--
--
-- 01.txt
--
--
-- using 2133894592 as a seed to the RNG


select 'query 01' as current_query;

explain select
	l_returnflag,
	l_linestatus,
	sum(l_quantity) as sum_qty,
	sum(l_extendedprice) as sum_base_price,
	sum(l_extendedprice * (1 - l_discount)) as sum_disc_price,
	sum(l_extendedprice * (1 - l_discount) * (1 + l_tax)) as sum_charge,
	avg(l_quantity) as avg_qty,
	avg(l_extendedprice) as avg_price,
	avg(l_discount) as avg_disc,
	count(*) as count_order
from
	lineitem
where
	l_shipdate <= date '1998-12-01' - interval '106 day'
group by
	l_returnflag,
	l_linestatus
order by
	l_returnflag,
	l_linestatus;

--
--
-- 02.txt
--
--
-- using 2133894592 as a seed to the RNG

select 'query 02' as current_query;

explain select
        s_acctbal,
        s_name,
        n_name,
        p_partkey,
        p_mfgr,
        s_address,
        s_phone,
        s_comment
from
        part,
        supplier,
        partsupp,
        nation,
        region
where
        p_partkey = ps_partkey
        and s_suppkey = ps_suppkey
        and p_size = 36
        and p_type like '%COPPER'
        and s_nationkey = n_nationkey
        and n_regionkey = r_regionkey
        and r_name = 'EUROPE'
        and ps_supplycost = (
                select
                        min(ps_supplycost)
                from
                        partsupp,
                        supplier,
                        nation,
                        region
                where
                        p_partkey = ps_partkey
                        and s_suppkey = ps_suppkey
                        and s_nationkey = n_nationkey
                        and n_regionkey = r_regionkey
                        and r_name = 'EUROPE'
        )
order by
        s_acctbal desc,
        n_name,
        s_name,
        p_partkey
LIMIT 100;

--
--
-- 03.txt
--
--
select 'query 03' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	l_orderkey,
	sum(l_extendedprice * (1 - l_discount)) as revenue,
	o_orderdate,
	o_shippriority
from
	customer,
	orders,
	lineitem
where
	c_mktsegment = 'AUTOMOBILE'
	and c_custkey = o_custkey
	and l_orderkey = o_orderkey
	and o_orderdate < date '1995-03-22'
	and l_shipdate > date '1995-03-22'
group by
	l_orderkey,
	o_orderdate,
	o_shippriority
order by
	revenue desc,
	o_orderdate
LIMIT 10;
--
--
-- 04.txt
--
--
select 'query 04' as current_query;

-- using 2133894592 as a seed to the RNG

explain select
        o_orderpriority,
        count(*) as order_count
from
        orders
where
        o_orderdate >= date '1996-07-01'
        and o_orderdate < date '1996-07-01' + interval '3 month'
        and exists (
                select
                        *
                from
                        lineitem
                where
                        l_orderkey = o_orderkey
                        and l_commitdate < l_receiptdate
        )
group by
        o_orderpriority
order by
        o_orderpriority;

--
--
-- 05.txt
--
--
select 'query 05' as current_query;

-- using 2133894592 as a seed to the RNG

explain select
count(*)
--	n_name,
--	sum(l_extendedprice * (1 - l_discount)) as revenue
from
	customer,
	orders,
	lineitem,
	supplier,
	nation,
	region
where
	c_custkey = o_custkey
	and l_orderkey = o_orderkey
	and l_suppkey = s_suppkey
	and c_nationkey = s_nationkey
	and s_nationkey = n_nationkey
	and n_regionkey = r_regionkey
	and r_name = 'ASIA'
	and o_orderdate >= date '1993-01-01'
	and o_orderdate < date '1993-01-01' + interval '1 year'
--group by
--	n_name
--order by
--	revenue desc
;

--
--
-- 06.txt
--
--
select 'query 06' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	sum(l_extendedprice * l_discount) as revenue
from
	lineitem
where
	l_shipdate >= date '1993-01-01'
	and l_shipdate < date '1993-01-01' + interval '1 year'
	and l_discount between 0.02 - 0.01 and 0.02 + 0.01
	and l_quantity < 25;

--
--
-- 07.txt
--
--
select 'query 07' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	supp_nation,
	cust_nation,
	l_year,
	sum(volume) as revenue
from
	(
		select
			n1.n_name as supp_nation,
			n2.n_name as cust_nation,
			extract(year from l_shipdate) as l_year,
			l_extendedprice * (1 - l_discount) as volume
		from
			supplier,
			lineitem,
			orders,
			customer,
			nation n1,
			nation n2
		where
			s_suppkey = l_suppkey
			and o_orderkey = l_orderkey
			and c_custkey = o_custkey
			and s_nationkey = n1.n_nationkey
			and c_nationkey = n2.n_nationkey
			and (
				(n1.n_name = 'ALGERIA' and n2.n_name = 'ROMANIA')
				or (n1.n_name = 'ROMANIA' and n2.n_name = 'ALGERIA')
			)
			and l_shipdate between date '1995-01-01' and date '1996-12-31'
	) as shipping
group by
	supp_nation,
	cust_nation,
	l_year
order by
	supp_nation,
	cust_nation,
	l_year;

--
--
-- 08.txt
--
--
select 'query 08' as current_query;

-- using 2133894592 as a seed to the RNG
-- set enable_nestloop=off;
explain select
	o_year,
	sum(case
		when nation = 'ROMANIA' then volume
		else 0
	end) / sum(volume) as mkt_share
from
	(
		select
			extract(year from o_orderdate) as o_year,
			l_extendedprice * (1 - l_discount) as volume,
			n2.n_name as nation
		from
			part,
			supplier,
			lineitem,
			orders,
			customer,
			nation n1,
			nation n2,
			region
		where
			p_partkey = l_partkey
			and s_suppkey = l_suppkey
			and l_orderkey = o_orderkey
			and o_custkey = c_custkey
			and c_nationkey = n1.n_nationkey
			and n1.n_regionkey = r_regionkey
			and r_name = 'EUROPE'
			and s_nationkey = n2.n_nationkey
			and o_orderdate between date '1995-01-01' and date '1996-12-31'
			and p_type = 'ECONOMY BRUSHED COPPER'
	) as all_nations
group by
	o_year
order by
	o_year;
--set enable_nestloop=on;

--
--
-- 09.txt
--
--
select 'query 09' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	nation,
	o_year,
	sum(amount) as sum_profit
from
	(
		select
			n_name as nation,
			extract(year from o_orderdate) as o_year,
			l_extendedprice * (1 - l_discount) - ps_supplycost * l_quantity as amount
		from
			part,
			supplier,
			lineitem,
			partsupp,
			orders,
			nation
		where
			s_suppkey = l_suppkey
			and ps_suppkey = l_suppkey
			and ps_partkey = l_partkey
			and p_partkey = l_partkey
			and o_orderkey = l_orderkey
			and s_nationkey = n_nationkey
			and p_name like '%seashell%'
	) as profit
group by
	nation,
	o_year
order by
	nation,
	o_year desc;

--
--
-- 10.txt
--
--
select 'query 10' as current_query;

-- using 2133894592 as a seed to the RNG

explain select
	c_custkey,
	c_name,
	sum(l_extendedprice * (1 - l_discount)) as revenue,
	c_acctbal,
	n_name,
	c_address,
	c_phone,
	c_comment
from
	customer,
	orders,
	lineitem,
	nation
where
	c_custkey = o_custkey
	and l_orderkey = o_orderkey
	and o_orderdate >= date '1993-06-01'
	and o_orderdate < date '1993-06-01' + interval '3 month'
	and l_returnflag = 'R'
	and c_nationkey = n_nationkey
group by
	c_custkey,
	c_name,
	c_acctbal,
	c_phone,
	n_name,
	c_address,
	c_comment
order by
	revenue desc
LIMIT 20;
--
--
-- 11.txt
--
--
select 'query 11' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	ps_partkey,
	sum(ps_supplycost * ps_availqty) as value
from
	partsupp,
	supplier,
	nation
where
	ps_suppkey = s_suppkey
	and s_nationkey = n_nationkey
	and n_name = 'INDIA'
group by
	ps_partkey having
		sum(ps_supplycost * ps_availqty) > (
			select
				sum(ps_supplycost * ps_availqty) * 0.0000001000
			from
				partsupp,
				supplier,
				nation
			where
				ps_suppkey = s_suppkey
				and s_nationkey = n_nationkey
				and n_name = 'INDIA'
		)
order by
	value desc;

--
--
-- 12.txt
--
--
select 'query 12' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	l_shipmode,
	sum(case
		when o_orderpriority = '1-URGENT'
			or o_orderpriority = '2-HIGH'
			then 1
		else 0
	end) as high_line_count,
	sum(case
		when o_orderpriority <> '1-URGENT'
			and o_orderpriority <> '2-HIGH'
			then 1
		else 0
	end) as low_line_count
from
	orders,
	lineitem
where
	o_orderkey = l_orderkey
	and l_shipmode in ('AIR', 'MAIL')
	and l_commitdate < l_receiptdate
	and l_shipdate < l_commitdate
	and l_receiptdate >= date '1997-01-01'
	and l_receiptdate < date '1997-01-01' + interval '1 year'
group by
	l_shipmode
order by
	l_shipmode;

--
--
-- 13.txt
--
--
select 'query 13' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	c_count,
	count(*) as custdist
from
	(
		select
			c_custkey,
			count(o_orderkey)
		from
			customer left outer join orders on
				c_custkey = o_custkey
				and o_comment not like '%unusual%deposits%'
		group by
			c_custkey
	) as c_orders (c_custkey, c_count)
group by
	c_count
order by
	custdist desc,
	c_count desc;

--
--
-- 14.txt
--
--
select 'query 14' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	100.00 * sum(case
		when p_type like 'PROMO%'
			then l_extendedprice * (1 - l_discount)
		else 0
	end) / sum(l_extendedprice * (1 - l_discount)) as promo_revenue
from
	lineitem,
	part
where
	l_partkey = p_partkey
	and l_shipdate >= date '1997-08-01'
	and l_shipdate < date '1997-08-01' + interval '1 month';

--
--
-- 15.txt
--
--
select 'query 15' as current_query;


-- using 2133894592 as a seed to the RNG


create view revenue0 (supplier_no, total_revenue) as
	select
		l_suppkey,
		sum(l_extendedprice * (1 - l_discount))
	from
		lineitem
	where
		l_shipdate >= date '1997-08-01'
		and l_shipdate < date '1997-08-01' + interval '3 month'
	group by
		l_suppkey;


explain select
	s_suppkey,
	s_name,
	s_address,
	s_phone,
	total_revenue
from
	supplier,
	revenue0
where
	s_suppkey = supplier_no
	and total_revenue = (
		select
			max(total_revenue)
		from
			revenue0
	)
order by
	s_suppkey;

drop view revenue0;

--
--
-- 16.txt
--
--
select 'query 16' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	p_brand,
	p_type,
	p_size,
	count(distinct ps_suppkey) as supplier_cnt
from
	partsupp,
	part
where
	p_partkey = ps_partkey
	and p_brand <> 'Brand#15'
	and p_type not like 'LARGE BRUSHED%'
	and p_size in (4, 41, 17, 9, 15, 2, 25, 34)
	and ps_suppkey not in (
		select
			s_suppkey
		from
			supplier
		where
			s_comment like '%Customer%Complaints%'
	)
group by
	p_brand,
	p_type,
	p_size
order by
	supplier_cnt desc,
	p_brand,
	p_type,
	p_size;

--
--
-- 17.txt
--
--
select 'query 17' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	sum(l_extendedprice) / 7.0 as avg_yearly
from
	lineitem,
	part
where
	p_partkey = l_partkey
	and p_brand = 'Brand#54'
	and p_container = 'SM CAN'
	and l_quantity < (
		select
			0.2 * avg(l_quantity)
		from
			lineitem
		where
			l_partkey = p_partkey
	);

--
--
-- 18.txt
--
--
select 'query 18' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	c_name,
	c_custkey,
	o_orderkey,
	o_orderdate,
	o_totalprice,
	sum(l_quantity)
from
	customer,
	orders,
	lineitem
where
	o_orderkey in (
		select
			l_orderkey
		from
			lineitem
		group by
			l_orderkey having
				sum(l_quantity) > 314
	)
	and c_custkey = o_custkey
	and o_orderkey = l_orderkey
group by
	c_name,
	c_custkey,
	o_orderkey,
	o_orderdate,
	o_totalprice
order by
	o_totalprice desc,
	o_orderdate
LIMIT 100;
--
--
-- 19.txt
--
--
select 'query 19' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	sum(l_extendedprice* (1 - l_discount)) as revenue
from
	lineitem,
	part
where
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#11'
		and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
		and l_quantity >= 9 and l_quantity <= 9 + 10
		and p_size between 1 and 5
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#51'
		and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
		and l_quantity >= 10 and l_quantity <= 10 + 10
		and p_size between 1 and 10
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#23'
		and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
		and l_quantity >= 22 and l_quantity <= 22 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	);

--
--
-- 20.txt
--
--
select 'query 20' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	s_name,
	s_address
from
	supplier,
	nation
where
	s_suppkey in (
		select
			ps_suppkey
		from
			partsupp
		where
			ps_partkey in (
				select
					p_partkey
				from
					part
				where
					p_name like 'burnished%'
			)
			and ps_availqty > (
				select
					0.5 * sum(l_quantity)
				from
					lineitem
				where
					l_partkey = ps_partkey
					and l_suppkey = ps_suppkey
					and l_shipdate >= date '1997-01-01'
					and l_shipdate < date '1997-01-01' + interval '1 year'
			)
	)
	and s_nationkey = n_nationkey
	and n_name = 'ALGERIA'
order by
	s_name;


--
--
-- 21.txt
--
--
select 'query 21' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	s_name,
	count(*) as numwait
from
	supplier,
	lineitem l1,
	orders,
	nation
where
	s_suppkey = l1.l_suppkey
	and o_orderkey = l1.l_orderkey
	and o_orderstatus = 'F'
	and l1.l_receiptdate > l1.l_commitdate
	and exists (
		select
			*
		from
			lineitem l2
		where
			l2.l_orderkey = l1.l_orderkey
			and l2.l_suppkey <> l1.l_suppkey
	)
	and not exists (
		select
			*
		from
			lineitem l3
		where
			l3.l_orderkey = l1.l_orderkey
			and l3.l_suppkey <> l1.l_suppkey
			and l3.l_receiptdate > l3.l_commitdate
	)
	and s_nationkey = n_nationkey
	and n_name = 'SAUDI ARABIA'
group by
	s_name
order by
	numwait desc,
	s_name
LIMIT 100;

--
--
-- 22.txt
--
--
select 'query 22' as current_query;

-- using 2133894592 as a seed to the RNG
explain select
	cntrycode,
	count(*) as numcust,
	sum(c_acctbal) as totacctbal
from
	(
		select
			substring(c_phone from 1 for 2) as cntrycode,
			c_acctbal
		from
			customer
		where
			substring(c_phone from 1 for 2) in
				('10', '28', '11', '15', '18', '16', '19')
			and c_acctbal > (
				select
					avg(c_acctbal)
				from
					customer
				where
					c_acctbal > 0.00
					and substring(c_phone from 1 for 2) in
						('10', '28', '11', '15', '18', '16', '19')
			)
			and not exists (
				select
					*
				from
					orders
				where
					o_custkey = c_custkey
			)
	) as custsale
group by
	cntrycode
order by
	cntrycode;
	
reset optimizer;
-- EOF
