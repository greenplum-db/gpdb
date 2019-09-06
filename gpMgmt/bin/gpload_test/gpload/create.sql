DROP DATABASE IF EXISTS gptest;
CREATE DATABASE gptest;
\c gptest

DROP TABLE IF EXISTS lineitem;
CREATE TABLE lineitem (
                L_ORDERKEY INT8 UNIQUE,
                L_PARTKEY INTEGER,
                L_SUPPKEY INTEGER,
                L_LINENUMBER integer,
                L_QUANTITY decimal,
                L_EXTENDEDPRICE decimal,
                L_DISCOUNT decimal,
                L_TAX decimal,
                L_RETURNFLAG CHAR(1),
                L_LINESTATUS CHAR(1),
                L_SHIPDATE date,
                L_COMMITDATE date,
                L_RECEIPTDATE date,
                L_SHIPINSTRUCT CHAR(25),
                L_SHIPMODE CHAR(10),
                L_COMMENT VARCHAR(44)
                ) 
distributed by (L_ORDERKEY);

DROP TABLE IF EXISTS lineitem_space;
CREATE TABLE lineitem_space (
                "l_orderkey " INT8,
                L_PARTKEY INTEGER,
                L_SUPPKEY INTEGER,
                L_LINENUMBER integer,
                L_QUANTITY decimal,
                L_EXTENDEDPRICE decimal,
                L_DISCOUNT decimal,
                L_TAX decimal,
                L_RETURNFLAG CHAR(1),
                L_LINESTATUS CHAR(1),
                L_SHIPDATE date,
                L_COMMITDATE date,
                L_RECEIPTDATE date,
                L_SHIPINSTRUCT CHAR(25),
                L_SHIPMODE CHAR(10),
                L_COMMENT VARCHAR(44)
                ) 
distributed by (L_PARTKEY);

DROP TABLE IF EXISTS all_types;
create table all_types(
bit1 bit(5), 
bit2 bit varying(50), 
boolean1 boolean, 
char1 char(50), 
charvar1 character varying(50), 
char2 character(50), 
varchar1 varchar(50), 
date1 date, 
dp1 double precision, 
int1 integer, 
interval1 interval, 
numeric1 numeric, 
real1 real, 
smallint1 smallint, 
time1 time,
bigint1 bigint,
bigserial1 bigserial,
bytea1 bytea,
cidr1 cidr,
decimal1 decimal,
inet1 inet,
macaddr1 macaddr,
money1 money,
serial1 serial,
text1 text,
time2 time without time zone,
time3 time with time zone,
time4 timestamp without time zone,
time5 timestamp with time zone)
distributed randomly;

DROP TABLE IF EXISTS errortable;
CREATE TABLE errortable (
    cmdtime timestamp with time zone,
    relname text,
    filename text,
    linenum integer,
    bytenum integer,
    errmsg text,
    rawdata text,
    rawbytes bytea
) distributed randomly;
