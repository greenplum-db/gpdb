create database gp_persistent_test_db1;


--
CREATE TABLE gp_persistent_heap_all_types( bit1 bit(1), bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),
varchar1 varchar(50),date1 date,dp1 double precision,int1 integer,interval1 interval,numeric1 numeric,real1 real,smallint1 smallint,time1 time,bigint1 bigint,
bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,
time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) distributed randomly;


--
-- Table with CO

CREATE TABLE gp_persistent_co_all_types ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT
'111',col005 text DEFAULT 'pookie',col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time',
col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision,
col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,
col022 money, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, col031 bit varying(256),col032 date,
col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp )with (orientation='column',appendonly=true);


--
--Table Creation using Create Table As (CTAS) with both the new tables columns being explicitly or implicitly created


    CREATE TABLE gp_persistent_test_table_tobe_ctas(
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric,
    int_col int4,
    float_col float4,
    int_array_col int[],
    before_rename_col int4,
    change_datatype_col numeric,
    a_ts_without timestamp without time zone,
    b_ts_with timestamp with time zone,
    date_column date,
    col_set_default numeric)DISTRIBUTED RANDOMLY;

CREATE TABLE gp_persistent_ctas_table1 AS SELECT text_col,bigint_col,char_vary_col,numeric_col FROM gp_persistent_test_table_tobe_ctas;



--
SET gp_enable_inheritance = true;
--Inherits parent_table - MPP-5300

   CREATE TABLE gp_persistent_table_parent ( a int, b text) DISTRIBUTED BY (a);

   CREATE TABLE gp_persistent_table_child() INHERITS(gp_persistent_table_parent);


--
CREATE SEQUENCE gp_persistent_test_seq  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;


--
CREATE TABLE gp_persistent_test_emp_index (ename varchar(20),eno int,salary int,ssn int,gender char(1)) distributed by (ename,eno,gender);
CREATE INDEX gp_persistent_gender_bmp_idx ON gp_persistent_test_emp_index USING bitmap (gender);


--
-- Table with all data types

CREATE TABLE gp_persistent_all_types( bit1 bit(1), bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),
varchar1 varchar(50),date1 date,dp1 double precision,int1 integer,interval1 interval,numeric1 numeric,real1 real,smallint1 smallint,time1 time,bigint1 bigint,
bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,
time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) distributed randomly;

create table gp_persistent_like_all_types (like gp_persistent_all_types including defaults);


--
-- Partition table

CREATE TABLE gp_persistent_supplier_hybrid_part(
                S_SUPPKEY INTEGER,
                S_NAME CHAR(25),
                S_ADDRESS VARCHAR(40),
                S_NATIONKEY INTEGER,
                S_PHONE CHAR(15),
                S_ACCTBAL decimal,
                S_COMMENT VARCHAR(101)
                )
partition by range (s_suppkey)
subpartition by list (s_nationkey) subpartition template (
        values('22','21','17'),
        values('6','11','1','7','16','2') WITH (checksum=false,appendonly=true,blocksize=1171456,compresslevel=3),
        values('18','20'),
        values('9','23','13') WITH (checksum=true,appendonly=true,blocksize=1335296,compresslevel=7),
        values('0','3','12','15','14','8','4','24','19','10','5')
)
(
partition p1 start('1') end('10001') every(10000)

);


--
-- Partition table

CREATE TABLE gp_persistent_supplier_hybrid_subpart(
                S_SUPPKEY INTEGER,
                S_NAME CHAR(25),
                S_ADDRESS VARCHAR(40),
                S_NATIONKEY INTEGER,
                S_PHONE CHAR(15),
                S_ACCTBAL decimal,
                S_COMMENT VARCHAR(101)
                )
partition by range (s_suppkey)
subpartition by list (s_nationkey) subpartition template (
        values('22','21','17'),
        values('6','11','1','7','16','2') WITH (checksum=false,appendonly=true,blocksize=1171456,compresslevel=3),
        values('18','20'),
        values('9','23','13') WITH (checksum=true,appendonly=true,blocksize=1335296,compresslevel=7),
        values('0','3','12','15','14','8','4','24','19','10','5')
)
(
partition p1 start('1') end('10001') every(10000)

);


--
-- Partition table

CREATE TABLE gp_persistent_supplier_hybrid_subpart1(
                S_SUPPKEY INTEGER,
                S_NAME CHAR(25),
                S_ADDRESS VARCHAR(40),
                S_NATIONKEY INTEGER,
                S_PHONE CHAR(15),
                S_ACCTBAL decimal,
                S_COMMENT VARCHAR(101)
                )
partition by range (s_suppkey)
subpartition by list (s_nationkey) subpartition template (
        values('22','21','17'),
        values('6','11','1','7','16','2') WITH (checksum=false,appendonly=true,blocksize=1171456,compresslevel=3),
        values('18','20'),
        values('9','23','13') WITH (checksum=true,appendonly=true,blocksize=1335296,compresslevel=7),
        values('0','3','12','15','14','8','4','24','19','10','5')
)
(
partition p1 start('1') end('10001') every(10000)

);



--
drop database gp_persistent_test_db1;
drop table gp_persistent_heap_all_types ;
drop table gp_persistent_ao_all_types;
drop table gp_persistent_co_all_types;
drop table gp_persistent_test_table_tobe_ctas;
drop table gp_persistent_ctas_table1;
drop table gp_persistent_table_child;
drop table gp_persistent_table_parent ;
drop sequence gp_persistent_test_seq;
drop table gp_persistent_test_emp_index;
drop table gp_persistent_like_all_types;
drop table gp_persistent_all_types;
drop table gp_persistent_supplier_hybrid_part;
drop table gp_persistent_supplier_hybrid_subpart;
drop table gp_persistent_supplier_hybrid_subpart1;
--
-- Test rebuilding free tid list
--

-- Input is PT OID and output is number of tuples in the rebuilt list.
create function gp_persistent_freelist_rebuild(oid) returns int as
	   '$libdir/gp_persistent_util', 'gp_persistent_freelist_rebuild' language C strict;

-- Verifying sanity of free list before rebuilding is not exactly
-- necessary, because rebuilding should make it sane.  But we check it
-- anyway, as we don't expect ICG tests to break freelist.
select count(*) = max(persistent_serial_num) as freelist_valid
	   from gp_persistent_relation_node where previous_free_tid > '(0,0)';
select gp_persistent_freelist_rebuild('gp_persistent_relation_node'::regclass) > 0
	   as freelist_rebuilt;
select count(*) = max(persistent_serial_num) as freelist_valid
	   from gp_persistent_relation_node where previous_free_tid > '(0,0)';

select count(*) = max(persistent_serial_num) as freelist_valid
	   from gp_persistent_database_node where previous_free_tid > '(0,0)';
select gp_persistent_freelist_rebuild('gp_persistent_database_node'::regclass) > 0
	   as freelist_rebuilt;
select count(*) = max(persistent_serial_num) as freelist_valid
	   from gp_persistent_database_node where previous_free_tid > '(0,0)';

-- TODO: Validate that max(ctid) and max(persistent_serial_num) yield
-- the same tuple in the free list.

SET allow_system_table_mods = 'DML';
SET gp_permit_persistent_metadata_update = true;

-- Break free list by updating persistent_serial_num = 2, so there are
-- two free list entries with same free order number.  This should be
-- fixed by rebuild.  Assume at least three entries in the free list,
-- because we have dropped very many objects earlier in this test.
UPDATE gp_persistent_relation_node SET persistent_serial_num = 2
	   WHERE persistent_serial_num = 3 AND previous_free_tid > '(0, 0)';

SET allow_system_table_mods = 'NONE';
SET gp_permit_persistent_metadata_update = false;

select count(*) = count(distinct(persistent_serial_num)) as freelist_valid
	   from gp_persistent_relation_node where previous_free_tid > '(0,0)';
select gp_persistent_freelist_rebuild('gp_persistent_relation_node'::regclass) > 0
	   as freelist_rebuilt;
select count(*) = count(distinct(persistent_serial_num)) as freelist_valid
	   from gp_persistent_relation_node where previous_free_tid > '(0,0)';

-- DDL after rebuilding free list
create table gp_persistent_ao(a int, b int) with (appendonly=true);
create index gp_persistent_ao_idx on gp_persistent_ao(a);
insert into gp_persistent_ao select i,i from generate_series(1,10)i;
select count(*) from gp_persistent_ao;
drop table gp_persistent_ao;
