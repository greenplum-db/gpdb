-- @product_version gpdb: [4.3.99-]
set client_min_messages = WARNING;
set gp_enable_hash_partitioned_tables = true;
set timezone to '+07:00';
DROP SCHEMA IF EXISTS partition_ddl2 CASCADE;
CREATE SCHEMA partition_ddl2;
set search_path to partition_ddl2;
-- MPP-2977
create table catcheck(a int, b date, d int, e numeric, f float, g numeric, h char, i text) distributed by (a) partition by hash(b) partitions 2 subpartition by hash(d) subpartitions 2, subpartition by hash(e) subpartitions 2, subpartition by hash(f) subpartitions 2, subpartition by hash(h) subpartitions 2;

\d catcheck*

drop table catcheck cascade;
set enable_partition_rules=off;
create table T1_PART(C4 int, C5 int, C6 int, C7 int, C8 int, C9 int, C10 int, C11 int, C12 int, C13 int, C14 int) 
partition by range(c4)
subpartition by hash(c11) subpartitions 8
(
	partition p1 start(1) end(4),
	partition p2 end(10)
);

INSERT INTO T1_PART VALUES ( 2, 5, 1, 4, 5, 4, 5, 1, 5, 4, 1 );
INSERT INTO T1_PART VALUES ( 1, 3, 1, 1, 5, 2, 4, 4, 3, 3, 1 );
INSERT INTO T1_PART VALUES ( 1, 3, 1, 4, 1, 3, 2, 3, 1, 3, 5 );
INSERT INTO T1_PART VALUES ( 2, 5, 2, 5, 2, 2, 4, 1, 2, 5, 1 );
INSERT INTO T1_PART VALUES ( 3, 5, 3, 4, 1, 2, 3, 5, 3, 1, 1 );
INSERT INTO T1_PART VALUES ( 2, 4, 4, 4, 2, 5, 2, 2, 3, 2, 5 );
INSERT INTO T1_PART VALUES ( 4, 5, 5, 3, 1, 3, 1, 1, 3, 5, 1 );
INSERT INTO T1_PART VALUES ( 3, 4, 1, 3, 5, 3, 1, 4, 2, 1, 3 );
INSERT INTO T1_PART VALUES ( 4, 2, 3, 5, 4, 5, 5, 5, 1, 4, 4 );
INSERT INTO T1_PART VALUES ( 1, 2, 4, 4, 3, 2, 3, 1, 5, 1, 3 );

SELECT * FROM T1_PART;

DROP TABLE T1_PART CASCADE;
set enable_partition_rules = off;

create table hashtest (i int) distributed by (i) partition by hash(i) partitions 2;
insert into hashtest values(NULL);
insert into hashtest values(NULL);

select * from hashtest;

drop table hashtest;

create table hashtest (i int, j int) distributed by (i) partition by hash(i, j) partitions 2;
insert into hashtest values(NULL, NULL);
insert into hashtest values(NULL, 1);
insert into hashtest values(1, 1);
insert into hashtest values(1, NULL);

select * from hashtest order by 1,2;

select * from hashtest_1_prt_1 order by 1,2;

select * from hashtest_1_prt_2 order by 1,2;

drop table hashtest;
CREATE TABLE rank (
                id int,
                rank int,
                year smallint,
                gender char(1),
                count int )

        DISTRIBUTED BY (id, gender, year);

set enable_partition_rules = off;

create table rank2 (LIKE rank) 
PARTITION BY LIST (gender) 
SUBPARTITION BY RANGE (year) 
SUBPARTITION TEMPLATE (start ('2000') end ('2006') every (interval '1')) 
(PARTITION girls VALUES ('F'), PARTITION boys VALUES ('M'));

drop table rank;
set enable_partition_rules = off;

drop table if exists ggg cascade;

create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by LIST (b)
( partition aa values ('a', 'b', 'c', 'd'),
partition bb values ('e', 'f', NULL) );

insert into ggg values ('a','e','111');
insert into ggg values ('b',NULL,'111');

-- order 1,2
select * from ggg order by 1,2;

-- order 1,2
select * from ggg_1_prt_bb order by 1,2;

drop table ggg;
create table mpp3053_test(a int, b int) partition by range(a) subpartition by hash (b) subpartitions 2(partition p1 end(5), partition p2 end(10));
insert into mpp3053_test values(5,5);
select * from mpp3053_test;
drop table mpp3053_test;
CREATE TABLE MPP3083_REGION (
                    R_REGIONKEY INTEGER,
                    R_NAME CHAR(25),
                    R_COMMENT VARCHAR(152),
                    primary key (r_regionkey, r_name, r_comment)
                    )
partition by hash (r_name) partitions 1
subpartition by hash (r_regionkey)
,subpartition by hash (r_comment) subpartitions 2
(
partition p1(subpartition sp1,subpartition sp2,subpartition sp3)
);

select tablename, parentpartitionname, count(*)
from pg_partitions
where tablename = 'mpp3083_region'
group by tablename, parentpartitionname
order by 1,2 asc;

INSERT INTO mpp3083_region VALUES (0, 'AFRICA', 'lar deposits. blithely final packages cajole. regular waters are final requests. regular accounts are according to ');
INSERT INTO mpp3083_region VALUES (4, 'MIDDLE EAST', 'uickly special accounts cajole carefully blithely close requests. carefully final asymptotes haggle furiousl');
INSERT INTO mpp3083_region VALUES (3, 'EUROPE', 'ly final courts cajole furiously final excuse');
INSERT INTO mpp3083_region VALUES (1, 'AMERICA', 'hs use ironic, even requests. s');
INSERT INTO mpp3083_region VALUES (2, 'ASIA', 'ges. thinly even pinto beans ca');

select * from mpp3083_region order by r_regionkey;

drop table mpp3083_region;
-- QA-877
-- Johnny Soedomo
-- Updated test case to use default tablespace. We now support partition and tablespace
create table ggg (a char(1), b char(2), d char(3))
distributed by (a)
partition by LIST (b)
( partition aa values ('a', 'b', 'c', 'd') tablespace pg_default);
drop table ggg;
create table mpp3137_region
(
		R_REGIONKEY INTEGER,
                    R_NAME CHAR(25),
                    R_COMMENT VARCHAR(152),
                    primary key (r_regionkey, r_name, r_comment)
                    )
partition by list (r_regionkey)
subpartition by list (r_name)
,subpartition by list (r_comment) subpartition template (
        values('ges. thinly even pinto beans ca'),
        values('uickly special accounts cajole carefully blithely close requests. carefully final asymptotes haggle furiousl','lar deposits. blithely final packages cajole. regular waters are final requests. regular accounts are according to','hs use ironic, even requests. s'),
        values('ly final courts cajole furiously final excuse'),
        values(null)
)
(
partition p1 values('4','1','3','0','2')(subpartition sp1 values('MIDDLE EAST','AMERICA'),subpartition sp2 values('AFRICA','EUROPE','ASIA'))
);

select tablename, parentpartitionname, count(*)
from pg_partitions
where tablename = 'mpp3137_region'
group by tablename, parentpartitionname
order by 1,2 asc;

COPY mpp3137_region from STDIN delimiter '|';
0|AFRICA|lar deposits. blithely final packages cajole. regular waters are final requests. regular accounts are according to
1|AMERICA|hs use ironic, even requests. s
2|ASIA|ges. thinly even pinto beans ca
3|EUROPE|ly final courts cajole furiously final excuse
4|MIDDLE EAST|uickly special accounts cajole carefully blithely close requests. carefully final asymptotes haggle furiousl
\.

select count(*) from mpp3137_region;
select * from mpp3137_region order by r_regionkey;

drop table mpp3137_region;
create table mpp3084_region
(
                    R_REGIONKEY INTEGER,
                    R_NAME CHAR(25),
                    R_COMMENT VARCHAR(152),
                    primary key (r_regionkey, r_name, r_comment)
                    )
partition by hash (r_regionkey) partitions 1
subpartition by hash (r_name)
,subpartition by hash (r_comment) subpartitions 2
(
partition p1(subpartition sp1,subpartition sp2,subpartition sp3)
);

select tablename, parentpartitionname, count(*)
from pg_partitions
where tablename = 'mpp3084_region'
group by tablename, parentpartitionname
order by 1,2 asc;

COPY mpp3084_region from STDIN delimiter '|';
0|AFRICA|lar deposits. blithely final packages cajole. regular waters are final requests. regular accounts are according to
1|AMERICA|hs use ironic, even requests. s
2|ASIA|ges. thinly even pinto beans ca
3|EUROPE|ly final courts cajole furiously final excuse
4|MIDDLE EAST|uickly special accounts cajole carefully blithely close requests. carefully final asymptotes haggle furiousl
\.

select count(*) from mpp3084_region;
select * from mpp3084_region order by r_regionkey;

drop table mpp3084_region;

drop table if exists mpp3285_lineitem;
CREATE TABLE mpp3285_LINEITEM (
                L_ORDERKEY INT8,
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
partition by range (l_commitdate)
(
partition p1 start('1992-01-31') end('1998-11-01') every(interval '20 months')
);
copy mpp3285_lineitem from stdin delimiter '|';
18182|5794|3295|4|9|15298.11|0.04|0.01|N|O|1995-07-04|1995-05-30|1995-08-03|DELIVER IN PERSON|RAIL|y special platelets.
\.
select * from mpp3285_lineitem;
drop table mpp3285_lineitem;
drop table mpp3282_partsupp;
CREATE TABLE mpp3282_PARTSUPP (
PS_PARTKEY INTEGER,
PS_SUPPKEY INTEGER,
PS_AVAILQTY integer,
PS_SUPPLYCOST decimal,
PS_COMMENT VARCHAR(199)
)
partition by range (ps_suppkey)
subpartition by range (ps_partkey)
,subpartition by range (ps_supplycost) subpartition template (start('1') end('1001') every(500))
(
partition p1 start('1') end('10001') every(5000)
(subpartition sp1 start('1') end('200001') every(66666)
)
);
copy mpp3282_partsupp from stdin delimiter '|';
1|2|3325|771.64|, even theodolites. regular, final theodolites eat after the carefully pending foxes. furiously regular deposits sleep slyly. carefully bold realms above the ironic dependencies haggle careful
\.
select * from mpp3282_partsupp;
drop table mpp3282_partsupp;
drop table mpp3238_supplier;
CREATE TABLE mpp3238_supplier(
                S_SUPPKEY INTEGER,
                S_NAME CHAR(25),
                S_ADDRESS VARCHAR(40),
                S_NATIONKEY INTEGER,
                S_PHONE CHAR(15),
                S_ACCTBAL decimal,
                S_COMMENT VARCHAR(101)
                )
partition by range (s_nationkey)
(
partition p1 start(0) , 
partition p2 start(12) end(13), 
partition p3 end(20) inclusive, 
partition p4 start(20) exclusive , 
partition p5 start(22) end(25)
);
insert into mpp3238_supplier values(1,'Supplier#000000001',' N kD4on9OM Ipw3,gf0JBoQDd7tgrzrddZ',17,'27-918-335-1736',5755.94,'each slyly above the careful');
select * from mpp3238_supplier;
drop table mpp3238_supplier;
drop table if exists mpp3219_customer;
CREATE TABLE mpp3219_CUSTOMER (
C_CUSTKEY INTEGER,
C_NAME VARCHAR(25),
C_ADDRESS VARCHAR(40),
C_NATIONKEY INTEGER,
C_PHONE CHAR(15),
C_ACCTBAL decimal,
C_MKTSEGMENT CHAR(10),
C_COMMENT VARCHAR(117)
)
partition by range (c_acctbal)
(
partition sp1 start('-999.99') end('9833.01'),
partition sp2                  end('9905.01'),
partition sp3                  end('9978.01'));
drop table mpp3219_customer;
drop table mpp3190_partsupp;
-- This should fail
CREATE TABLE mpp3190_PARTSUPP (
PS_PARTKEY INTEGER, 
PS_SUPPKEY INTEGER,
 PS_AVAILQTY integer, 
PS_SUPPLYCOST decimal,
PS_COMMENT VARCHAR(199) ) 
partition by range (ps_supplycost)
 subpartition by range (ps_suppkey) subpartition template (start(1) end(10000) every(2499))
(partition p1, partition p2, partition p3 );
drop table if exists mpp3304_customer;
create table mpp3304_CUSTOMER (
                C_CUSTKEY INTEGER,
                C_NAME VARCHAR(25),
                C_ADDRESS VARCHAR(40),
               C_NATIONKEY INTEGER,
                C_PHONE CHAR(15),
                C_ACCTBAL decimal,
                C_MKTSEGMENT CHAR(10),
                C_COMMENT VARCHAR(117)
                )
partition by range (c_custkey) 
subpartition by range (c_acctbal) 
subpartition template (start('-999.99') end('10000.99') every(11000)
)
,subpartition by range (c_nationkey) 
subpartition template (start('0') end('25') every(5))
(
partition p1 start('1') end('150001') every(50000)
);
Alter table mpp3304_customer alter partition for (rank(2)) rename partition for (rank(1)) to newname;
drop table mpp3304_customer;
drop table if exists mpp3045_hhh;
create table mpp3045_hhh (a char(1), b date, d char(3)) with (appendonly=true)
distributed by (a) 
partition by range (b) 
 (partition aa start (date '2007-01-01') end (date '2008-01-01'),
 partition bb start (date '2008-01-01') end (date '2009-01-01'));
alter table mpp3045_hhh add partition aa;
drop table mpp3045_hhh;
drop table if exists mpp3287_nation;
CREATE TABLE mpp3287_NATION (
            N_NATIONKEY INTEGER,
            N_NAME CHAR(25),
            N_REGIONKEY INTEGER,
            N_COMMENT VARCHAR(152)
            )
partition by range (n_nationkey)
(
partition p1 start('0')  WITH (appendonly=true,checksum=true,blocksize=1998848,compresslevel=4), 
partition p2 start('11') end('15') inclusive WITH (checksum=false,appendonly=true,blocksize=655360,compresslevel=4), 
partition p3 start('15') exclusive end('19'), partition p4 start('19')  WITH (compresslevel=8,appendonly=true,checksum=false,blocksize=884736), 
partition p5 start('20')
);
delete from mpp3287_nation;

INSERT INTO mpp3287_nation VALUES (1, 'ARGENTINA                ', 1, 'al foxes promise slyly according to the regular accounts. bold requests alon');
INSERT INTO mpp3287_nation VALUES (3, 'CANADA                   ', 1, 'eas hang ironic, silent packages. slyly regular packages are furiously over the tithes. fluffily bold');
INSERT INTO mpp3287_nation VALUES (5, 'ETHIOPIA                 ', 0, 'ven packages wake quickly. regu');
INSERT INTO mpp3287_nation VALUES (7, 'GERMANY                  ', 3, 'l platelets. regular accounts x-ray: unusual, regular acco');
INSERT INTO mpp3287_nation VALUES (9, 'INDONESIA                ', 2, ' slyly express asymptotes. regular deposits haggle slyly. carefully ironic hockey players sleep blithely. carefull');
INSERT INTO mpp3287_nation VALUES (11, 'IRAQ                     ', 4, 'nic deposits boost atop the quickly final requests? quickly regula');
INSERT INTO mpp3287_nation VALUES (13, 'JORDAN                   ', 4, 'ic deposits are blithely about the carefully regular pa');
INSERT INTO mpp3287_nation VALUES (15, 'MOROCCO                  ', 0, 'rns. blithely bold courts among the closely regular packages use furiously bold platelets?');
INSERT INTO mpp3287_nation VALUES (17, 'PERU                     ', 1, 'platelets. blithely pending dependencies use fluffily across the even pinto beans. carefully silent accoun');
INSERT INTO mpp3287_nation VALUES (19, 'ROMANIA                  ', 3, 'ular asymptotes are about the furious multipliers. express dependencies nag above the ironically ironic account');
INSERT INTO mpp3287_nation VALUES (21, 'VIETNAM                  ', 2, 'hely enticingly express accounts. even, final ');
INSERT INTO mpp3287_nation VALUES (23, 'UNITED KINGDOM           ', 3, 'eans boost carefully special requests. accounts are. carefull');
INSERT INTO mpp3287_nation VALUES (0, 'ALGERIA                  ', 0, ' haggle. carefully final deposits detect slyly agai');
INSERT INTO mpp3287_nation VALUES (2, 'BRAZIL                   ', 1, 'y alongside of the pending deposits. carefully special packages are about the ironic forges. slyly special ');
INSERT INTO mpp3287_nation VALUES (4, 'EGYPT                    ', 4, 'y above the carefully unusual theodolites. final dugouts are quickly across the furiously regular d');
INSERT INTO mpp3287_nation VALUES (6, 'FRANCE                   ', 3, 'refully final requests. regular, ironi');
INSERT INTO mpp3287_nation VALUES (8, 'INDIA                    ', 2, 'ss excuses cajole slyly across the packages. deposits print aroun');
INSERT INTO mpp3287_nation VALUES (10, 'IRAN                     ', 4, 'efully alongside of the slyly final dependencies. ');
INSERT INTO mpp3287_nation VALUES (12, 'JAPAN                    ', 2, 'ously. final, express gifts cajole a');
INSERT INTO mpp3287_nation VALUES (14, 'KENYA                    ', 0, ' pending excuses haggle furiously deposits. pending, express pinto beans wake fluffily past t');
INSERT INTO mpp3287_nation VALUES (16, 'MOZAMBIQUE               ', 0, 's. ironic, unusual asymptotes wake blithely r');
INSERT INTO mpp3287_nation VALUES (18, 'CHINA                    ', 2, 'c dependencies. furiously express notornis sleep slyly regular accounts. ideas sleep. depos');
INSERT INTO mpp3287_nation VALUES (20, 'SAUDI ARABIA             ', 4, 'ts. silent requests haggle. closely express packages sleep across the blithely');
INSERT INTO mpp3287_nation VALUES (22, 'RUSSIA                   ', 3, ' requests against the platelets use never according to the quickly regular pint');
INSERT INTO mpp3287_nation VALUES (24, 'UNITED STATES            ', 1, 'y final packages. slow foxes cajole quickly. quickly silent platelets breach ironic accounts. unusual pinto be');
delete from mpp3287_nation;
drop table mpp3287_nation;
drop table if exists mpp3283_nation;
CREATE TABLE mpp3283_NATION (
            N_NATIONKEY INTEGER,
            N_NAME CHAR(25),
            N_REGIONKEY INTEGER,
            N_COMMENT VARCHAR(152)
            )
partition by range (n_regionkey)
(
partition p1 start('0') end('5') exclusive
);

-- Data for Name: nation; Type: TABLE DATA; Schema: public; Owner: bmaryada
INSERT INTO mpp3283_nation VALUES (1, 'ARGENTINA                ', 1, 'al foxes promise slyly according to the regular accounts. bold requests alon');
INSERT INTO mpp3283_nation VALUES (3, 'CANADA                   ', 1, 'eas hang ironic, silent packages. slyly regular packages are furiously over the tithes. fluffily bold');
INSERT INTO mpp3283_nation VALUES (5, 'ETHIOPIA                 ', 0, 'ven packages wake quickly. regu');
INSERT INTO mpp3283_nation VALUES (7, 'GERMANY                  ', 3, 'l platelets. regular accounts x-ray: unusual, regular acco');
INSERT INTO mpp3283_nation VALUES (9, 'INDONESIA                ', 2, ' slyly express asymptotes. regular deposits haggle slyly. carefully ironic hockey players sleep blithely. carefull');
INSERT INTO mpp3283_nation VALUES (11, 'IRAQ                     ', 4, 'nic deposits boost atop the quickly final requests? quickly regula');
INSERT INTO mpp3283_nation VALUES (13, 'JORDAN                   ', 4, 'ic deposits are blithely about the carefully regular pa');
INSERT INTO mpp3283_nation VALUES (15, 'MOROCCO                  ', 0, 'rns. blithely bold courts among the closely regular packages use furiously bold platelets?');
INSERT INTO mpp3283_nation VALUES (17, 'PERU                     ', 1, 'platelets. blithely pending dependencies use fluffily across the even pinto beans. carefully silent accoun');
INSERT INTO mpp3283_nation VALUES (19, 'ROMANIA                  ', 3, 'ular asymptotes are about the furious multipliers. express dependencies nag above the ironically ironic account');
INSERT INTO mpp3283_nation VALUES (21, 'VIETNAM                  ', 2, 'hely enticingly express accounts. even, final ');
INSERT INTO mpp3283_nation VALUES (23, 'UNITED KINGDOM           ', 3, 'eans boost carefully special requests. accounts are. carefull');
INSERT INTO mpp3283_nation VALUES (0, 'ALGERIA                  ', 0, ' haggle. carefully final deposits detect slyly agai');
INSERT INTO mpp3283_nation VALUES (2, 'BRAZIL                   ', 1, 'y alongside of the pending deposits. carefully special packages are about the ironic forges. slyly special ');
INSERT INTO mpp3283_nation VALUES (4, 'EGYPT                    ', 4, 'y above the carefully unusual theodolites. final dugouts are quickly across the furiously regular d');
INSERT INTO mpp3283_nation VALUES (6, 'FRANCE                   ', 3, 'refully final requests. regular, ironi');
INSERT INTO mpp3283_nation VALUES (8, 'INDIA                    ', 2, 'ss excuses cajole slyly across the packages. deposits print aroun');
INSERT INTO mpp3283_nation VALUES (10, 'IRAN                     ', 4, 'efully alongside of the slyly final dependencies. ');
INSERT INTO mpp3283_nation VALUES (12, 'JAPAN                    ', 2, 'ously. final, express gifts cajole a');
INSERT INTO mpp3283_nation VALUES (14, 'KENYA                    ', 0, ' pending excuses haggle furiously deposits. pending, express pinto beans wake fluffily past t');
INSERT INTO mpp3283_nation VALUES (16, 'MOZAMBIQUE               ', 0, 's. ironic, unusual asymptotes wake blithely r');
INSERT INTO mpp3283_nation VALUES (18, 'CHINA                    ', 2, 'c dependencies. furiously express notornis sleep slyly regular accounts. ideas sleep. depos');
INSERT INTO mpp3283_nation VALUES (20, 'SAUDI ARABIA             ', 4, 'ts. silent requests haggle. closely express packages sleep across the blithely');
INSERT INTO mpp3283_nation VALUES (22, 'RUSSIA                   ', 3, ' requests against the platelets use never according to the quickly regular pint');
INSERT INTO mpp3283_nation VALUES (24, 'UNITED STATES            ', 1, 'y final packages. slow foxes cajole quickly. quickly silent platelets breach ironic accounts. unusual pinto be');
select count(*) from mpp3283_nation;
copy mpp3283_nation to '/dev/null' delimiter '|';
drop table mpp3283_nation;

CREATE TABLE mpp3240(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );
alter table mpp3240 add default partition default_part;
drop table mpp3240;
CREATE TABLE mpp3259 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
( partition aa start (0) end (1000) every (100), default partition default_part );

alter table mpp3259 drop partition default_part;
alter table mpp3259 add default partition default_part;

insert into mpp3259 (unique1) values (100001);
select * from mpp3259;
select * from mpp3259_1_prt_default_part;

drop table mpp3259;


CREATE TABLE mpp3265 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
( partition aa start (0) end (500),
  partition bb start (500) end (1000),
  partition cc start (1000) end (1500),
  partition dd start (1500) end (2000),
  default partition default_part );

alter table mpp3265 drop partition for (position(100));
alter table mpp3265 drop partition for (position(-5));
alter table mpp3265 drop partition for (position(0));

drop table mpp3265;
CREATE TABLE mpp3237(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
distributed by (a)
partition by range (a)
subpartition by range (b) subpartition template ( start (1) end (2) every (1)),
subpartition by range (c) subpartition template ( start (1) end (2) every (1)),
subpartition by range (d) subpartition template ( start (1) end (2) every (1)),
subpartition by range (e) subpartition template ( start (1) end (2) every (1)),
subpartition by range (f) subpartition template ( start (1) end (2) every (1)),
subpartition by range (g) subpartition template ( start (1) end (2) every (1)),
subpartition by range (h) subpartition template ( start (1) end (2) every (1)),
subpartition by range (i) subpartition template ( start (1) end (2) every (1)),
subpartition by range (j) subpartition template ( start (1) end (2) every (1)),
subpartition by range (k) subpartition template ( start (1) end (2) every (1)),
subpartition by range (l) subpartition template ( start (1) end (2) every (1)),
subpartition by range (m) subpartition template ( start (1) end (2) every (1)),
subpartition by range (n) subpartition template ( start (1) end (2) every (1)),
subpartition by range (o) subpartition template ( start (1) end (2) every (1)),
subpartition by range (p) subpartition template ( start (1) end (2) every (1)),
subpartition by range (q) subpartition template ( start (1) end (2) every (1)),
subpartition by range (r) subpartition template ( start (1) end (2) every (1)),
subpartition by range (s) subpartition template ( start (1) end (2) every (1)),
subpartition by range (t) subpartition template ( start (1) end (2) every (1)),
subpartition by range (u) subpartition template ( start (1) end (2) every (1)),
subpartition by range (v) subpartition template ( start (1) end (2) every (1)),
subpartition by range (w) subpartition template ( start (1) end (2) every (1)),
subpartition by range (x) subpartition template ( start (1) end (2) every (1)),
subpartition by range (y) subpartition template ( start (1) end (2) every (1)),
subpartition by range (z) subpartition template ( start (1) end (2) every (1))
( start (1) end (2) every (1));

DROP TABLE mpp3237;
drop  TABLE IF EXISTS INT_P1A CASCADE;
CREATE TABLE INT_P1A ( num1    INTEGER NOT NULL,
                      num2     INTEGER NOT NULL,
                      average    DECIMAL(15,2) NOT NULL,
                      date1    DATE NOT NULL,
                      date2  DATE NOT NULL,
                      message      VARCHAR(44) NOT NULL )
DISTRIBUTED BY   (num1)
PARTITION BY RANGE(date1)
(
     PARTITION y1992  END('1992-12-31'),
     PARTITION y1993 END('1993-12-31'),
     PARTITION y1994 END('1994-12-31'),
     PARTITION y1995 END('1995-12-31'),
     PARTITION y1996  START('1996-01-01'),
     PARTITION y1997 START('1997-01-01') END('1997-12-31') 
 );


drop  TABLE IF EXISTS INT_P1B CASCADE;

CREATE TABLE INT_P1B ( num1    INTEGER NOT NULL,
                      num2     INTEGER NOT NULL,
                      average    DECIMAL(15,2) NOT NULL,
                      date1    DATE NOT NULL,
                      date2  DATE NOT NULL,
                      message      VARCHAR(44) NOT NULL )
DISTRIBUTED BY   (num1)
PARTITION BY RANGE(date1)
(
     PARTITION y1992  END('1992-12-31'),
     PARTITION y1993 END('1993-12-31'),
     PARTITION y1994 END('1994-12-31'),
     PARTITION y1995,
     PARTITION y1996  START('1996-01-01'),
     PARTITION y1997 start('1997-01-01') END('1997-12-31')
 );

DROP TABLE INT_P1A;
DROP TABLE INT_P1B;
CREATE TABLE mpp2564_transactions (obligation_trans_date date, cust_no integer, company_no character(3), obligation_trans_no character varying(25), item_no integer, orig_oblig_trans_no character varying(25), split_ind character(1), qty_sold integer, net_wgt_lbs numeric(10,5), unit_price numeric(10,2), ext_price numeric(10,2), catch_wgt_ind character(1), qty_sold_case_equivalent numeric(10,5), marketing_assoc_id character varying(10), cost_of_goods_sold numeric(10,2), adj_cost_good_sold numeric(10,2), trade_sales_ind character(1), create_date timestamp without time zone) DISTRIBUTED RANDOMLY 
PARTITION BY RANGE (obligation_trans_date)
SUBPARTITION BY LIST (company_no) subpartition template
( subpartition p1 values ('047'),
  subpartition p2 values ('002'),
  subpartition p3 values ('056'),
  subpartition p4 values ('022')
)
( start ('2005-06-01') end ('2006-05-01') every (INTERVAL '1 month') );

\d mpp2564_transactions*

DROP TABLE mpp2564_transactions;
CREATE TABLE mpp3363(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );

-- MPP-3363
alter table mpp3363 add default partition default;

drop table mpp3363;
-- start_matchsubs
-- #Daylight savings for Pacific Time
-- m/00:00-0\d/
-- s/00:00-0\d/00:00-0x/g
-- end_matchsubs
-- 1 level partition
CREATE TABLE mpp3059(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3059';

alter table mpp3059 rename to mpp3059_rename; 
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3059_rename';

alter table mpp3059_rename rename to mpp3059; 
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3059';

-- 2 level partition
CREATE TABLE mpp3059a (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
subpartition by range (unique2) subpartition template ( start (0) end (1000) every (100) )
( start (0) end (1000) every (100));
alter table mpp3059a rename to mpp3059a_rename; 
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3059a_rename';

create table mpp3216a (id int, rank int, year int, gender char(1), count int) distributed by (id); 
create table mpp3216 (like mpp3216a) partition by range (year) ( start (2001) end (2006) every (1));
alter table mpp3216 rename to mpp3216_rename;
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3216_rename';

CREATE TABLE mpp3059b (f1 time(2) with time zone, f2 char(4))
partition by list (f2)
( partition pst values ('PST'),
  partition est values ('EST')
);
alter table mpp3059b rename partition pst to "pacific time"; 
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3059b';

CREATE TABLE mpp3059c (f1 time(2) with time zone, f2 char(4))
partition by list (f2)
subpartition by range (f1)
subpartition template (
start (time '00:00'),
start (time '01:00')
)
( partition pst values ('PST'),
  partition est values ('EST')
);
alter table mpp3059c rename partition pst to pacific;
alter table mpp3059c rename partition est to "Eastern Time";
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3059c';

CREATE TABLE mpp3059d (f1 time(2) with time zone, f2 char(4), f3 varchar(10))
partition by list (f2)
subpartition by list (f3)
subpartition template (
  subpartition male values ('Male','M'),
  subpartition female values ('Female','F')
)
( partition pst values ('PST'),
  partition est values ('EST')
);
alter table mpp3059d rename partition pst to pacific;
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3059d';

drop table mpp3059;
drop table mpp3059a_rename;
drop table mpp3059b;
drop table mpp3059c;
drop table mpp3059d;
drop table mpp3216a;
drop table mpp3216_rename;
create table test_khush (a integer, b date, c varchar(30))
partition by range(b)
( start ('2008-01-01') end ('2008-04-01') );

Insert into test_khush values (1, '01-Jan-2008', 'abc');
Insert into test_khush values (2, '01-Feb-2008', 'jhg');
Insert into test_khush values (3, '01-Mar-2008', 'xyz');

alter table test_khush add column d varchar(20);

insert into test_khush values (5,'05-Feb-2008','hgjhg','Test');

drop table test_khush;
CREATE TABLE mpp3244(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
subpartition by range (b) subpartition template ( start (1) end (10) every (1))( partition aa start (1) end (10) every (1) );

CREATE TABLE mpp3244a(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
subpartition by range (b) subpartition template ( start (1) end (10) every (1))( partition aa start (1) end (11) every (1) );

insert into mpp3244 (a,b) values (10,1);
insert into mpp3244a (a,b) values (10,1);
alter table mpp3244 add partition bb end (11);
insert into mpp3244 (a,b) values (10,1);

select count(*) from mpp3244;
select count(*) from mpp3244a;

drop table mpp3244;
drop table mpp3244a;
CREATE TABLE mpp3256 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
subpartition by range (unique2) subpartition template ( start (0) end (1000) every (100) )
( start (0) end (1000) every (100));
alter table mpp3256 add default partition default_part;
alter table mpp3256 drop partition default_part;
alter table mpp3256 add default partition default_part;
alter table mpp3256 drop partition default_part;
alter table mpp3256 add default partition default_part;
alter table mpp3256 drop partition default_part;
alter table mpp3256 add default partition default_part;
alter table mpp3256 drop partition default_part;
alter table mpp3256 add default partition default_part;
alter table mpp3256 drop partition default_part;

drop table mpp3256;
-- Examples from Documentation
CREATE TABLE mpp3377_sales (trans_id int, date date, amount decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY RANGE (date)
SUBPARTITION BY LIST (region)
SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'),
SUBPARTITION asia VALUES ('asia'),
SUBPARTITION europe VALUES ('europe') )
( START (date '2008-01-01') INCLUSIVE
END (date '2009-01-01') EXCLUSIVE
EVERY (INTERVAL '1 month') );

-- Note from Jeff: MPP-3377
-- since you already defined a template for your subpartition, you cannot specify it again in ADD (this now matches the behavior of CREATE TABLE).
ALTER TABLE mpp3377_sales ADD PARTITION
START (date '2009-02-01') INCLUSIVE
END (date '2009-03-01') EXCLUSIVE
( SUBPARTITION usa VALUES ('usa'),
SUBPARTITION asia VALUES ('asia'),
SUBPARTITION europe VALUES ('europe') );

ALTER TABLE mpp3377_sales ADD PARTITION
START (date '2009-02-01') INCLUSIVE
END (date '2009-03-01') EXCLUSIVE
( SUBPARTITION usa VALUES ('usa'),
SUBPARTITION asia VALUES ('asia'),
SUBPARTITION europe VALUES ('europe') );

-- This is the new way
ALTER TABLE mpp3377_sales ADD PARTITION START (date '2009-02-01') INCLUSIVE END (date '2009-03-01') EXCLUSIVE;

ALTER TABLE mpp3377_sales ADD PARTITION START (date '2009-02-01') INCLUSIVE END (date '2009-03-01') EXCLUSIVE;

DROP TABLE mpp3377_sales;
CREATE TABLE mpp3250 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
subpartition by range (unique2) subpartition template ( start (0) end (1000) every (100) )
( start (0) end (1000) every (100));
alter table mpp3250 add default partition default_part;

copy mpp3250 from '%PATH%/_data/onek.data';

drop table mpp3250;
CREATE TABLE mpp3375 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
);

copy mpp3375 from '%PATH%/_data/onek.data';

CREATE TABLE mpp3375a (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
subpartition by range (unique2) subpartition template ( start (0) end (1000) every (100) )
( start (0) end (1000) every (100));

insert into mpp3375a select * from mpp3375;

alter table mpp3375a add default partition default_part;
alter table mpp3375a drop partition;
alter table mpp3375a drop partition default_part;

drop table mpp3375a;
drop table mpp3375;
CREATE TABLE mpp3241(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );
alter table mpp3241 add partition zz start (-1) end (0);

DROP TABLE mpp3241;
set enable_partition_rules = off;

CREATE TABLE mpp3373 (a int, b date, c char, d char(4), e varchar(20), f timestamp)
partition by range (b)
subpartition by list (a) subpartition template ( subpartition l1 values (1,2,3,4,5), subpartition l2 values (6,7,8,9,10) ),
subpartition by list (e) subpartition template ( subpartition ll1 values ('Engineering'), subpartition ll2 values ('QA') ),
subpartition by list (c) subpartition template ( subpartition lll1 values ('M'), subpartition lll2 values ('F') )
(
  start (date '2007-01-01')
  end (date '2010-01-01') every (interval '1 year')
);

-- start_ignore
\! gp_dump gptest -t mpp3373
-- end_ignore

drop table mpp3373;
CREATE TABLE mpp3438 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by list (unique1)
subpartition by list (unique2)
(
partition aa values (1,2,3,4,5,6,7,8,9,10) (subpartition cc values (1,2,3), subpartition dd values (4,5,6) ),
partition bb values (11,12,13,14,15,16,17,18,19,20) (subpartition cc values (1,2,3), subpartition dd values (4,5,6) )
);
-- This should fail
alter table mpp3438 add default partition default_part;

-- This should be the correct way
alter table mpp3438 add default partition default_part (default subpartition def2);
alter table mpp3438 alter partition aa add default partition def3;
\d mpp3438*

drop table mpp3438;
set enable_partition_rules = false;

CREATE TABLE mpp3261 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
);

copy mpp3261 from '%PATH%/_data/onek.data';


CREATE TABLE mpp3261_part (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
( partition aa start (0) end (1000) every (100), default partition default_part );

alter table mpp3261_part drop partition;
alter table mpp3261_part drop partition;
alter table mpp3261_part drop partition;
alter table mpp3261_part drop partition;
alter table mpp3261_part drop partition;
alter table mpp3261_part drop partition;
alter table mpp3261_part drop partition;
alter table mpp3261_part drop partition;
alter table mpp3261_part drop partition;
alter table mpp3261_part drop partition;
-- Last partition, cannot be dropped, only default partition is left
alter table mpp3261_part drop partition;

-- Shouldn't take a long time to insert
insert into mpp3261_part select * from mpp3261;

drop table mpp3261;
drop table mpp3261_part;
CREATE TABLE mpp3079(q1 int8, q2 int8)
partition by range (q1)
(start (1) end (10) every (1));

CREATE TABLE mpp3079a(q1 int2, q2 int2)
partition by range (q1)
(start (1) end (10) every (1));

select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3079';

select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3079a';

drop table mpp3079;
drop table mpp3079a;
CREATE TABLE mpp3242(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );
alter table mpp3242 add default partition default_part;
-- Needs to use ALTER SPLIT instead of ADD. It's not automatic. MPP-3242
alter table mpp3242 add partition zz start ('-1') end (0);

select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3242';

drop table mpp3242;
CREATE TABLE mpp3115 (f1 time(2) with time zone)
partition by range (f1)
(
  partition "Los Angeles" start (time with time zone '00:00 PST') end (time with time zone '23:00 PST') EVERY (INTERVAL '1 hour'),
  partition "New York" start (time with time zone '00:00 EST') end (time with time zone '23:00 EST') EVERY (INTERVAL '1 hour')
);

-- This should not work
alter table mpp3115 rename partition "Los Angeles" to "LA2";
alter table mpp3115 rename partition "Los Angeles" to "LA";
-- ALTER OK
alter table mpp3115 rename partition "Los Angeles_1" to "LA";
alter table mpp3115 rename partition "Los Angeles_2" to "LA";
alter table mpp3115 rename partition "Los Angeles_2" to "Los Angeles_1";
alter table mpp3115 rename partition "Los Angeles_1" to "Los Angeles_1";
-- ALTER OK
alter table mpp3115 rename partition "LA" to "Los Angeles_1";
-- alter table mpp3115 rename partition "Los Angeles_1" to "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";

select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename = 'mpp3115';

drop table mpp3115;

CREATE TABLE mpp3523 (f1 time(2) with time zone)
partition by range (f1)
(
  partition "Los Angeles" start (time with time zone '00:00 PST') end (time with time zone '23:00 PST') EVERY (INTERVAL '1 hour'),
  partition "New York" start (time with time zone '00:00 EST') end (time with time zone '23:00 EST') EVERY (INTERVAL '1 hour')
);

-- Tries to truncate first, but the partition name is still too long, so ERROR
alter table mpp3523 rename partition "Los Angeles_1" to "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";

-- Truncates the table name to mpp3523_0000000000111111111122222222223333333333444444444455555
CREATE TABLE mpp3523_000000000011111111112222222222333333333344444444445555555555556666666666777777777788888888889999999999 (f1 time(2) with time zone)
partition by range (f1)
(
  partition "Los Angeles" start (time with time zone '00:00 PST') end (time with time zone '23:00 PST') EVERY (INTERVAL '1 hour'),
  partition "New York" start (time with time zone '00:00 EST') end (time with time zone '23:00 EST') EVERY (INTERVAL '1 hour')
);

-- Truncates the table name to mpp3523_0000000000111111111122222222223333333333444444444455555, but partition name is too long, so ERROR
alter table mpp3523_000000000011111111112222222222333333333344444444445555555555556666666666777777777788888888889999999999 rename partition "Los Angeles_1" to "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
-- Truncates the table name to mpp3523_0000000000111111111122222222223333333333444444444455555, and partition name is safe, so renamed
alter table mpp3523_000000000011111111112222222222333333333344444444445555555555556666666666777777777788888888889999999999 rename partition "Los Angeles_1" to "LA1";
-- Use the actual table name
alter table mpp3523_0000000000111111111122222222223333333333444444444455555 rename partition "Los Angeles_2" to "LA2";

drop table mpp3523;
-- Truncates the table name to mpp3523_0000000000111111111122222222223333333333444444444455555
drop table mpp3523_000000000011111111112222222222333333333344444444445555555555556666666666777777777788888888889999999999;
CREATE TABLE mpp3260 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
( partition aa start (0) end (1000) every (100), default partition default_part );

-- Not Allowed to drop partition table directly
drop table mpp3260_1_prt_aa_6;
alter table mpp3260 drop partition;
alter table mpp3260 drop partition;
alter table mpp3260 drop partition;
alter table mpp3260 drop partition;
alter table mpp3260 drop partition;
alter table mpp3260 drop partition;
alter table mpp3260 drop partition;
alter table mpp3260 drop partition;
alter table mpp3260 drop partition;
alter table mpp3260 drop partition;
drop table mpp3260_1_prt_default_part ;
insert into mpp3260 (unique1) values (1);

CREATE TABLE mpp3260a (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
subpartition by range (unique2) subpartition template ( start (0) end (1000) every (100) )
( start (0) end (1000) every (100));
-- Not Allowed to drop partition table directly
drop table mpp3260a_1_prt_10;
alter table mpp3260a drop partition;
alter table mpp3260a drop partition;
alter table mpp3260a drop partition;
alter table mpp3260a drop partition;
alter table mpp3260a drop partition;
alter table mpp3260a drop partition;
alter table mpp3260a drop partition;
alter table mpp3260a drop partition;
alter table mpp3260a drop partition;
-- Last subpartition, cannot be dropped
alter table mpp3260a drop partition;
drop table mpp3260a_1_prt_10;

drop table mpp3260;
drop table mpp3260a;
drop table if exists mpp3455 cascade;
drop table if exists mpp3455a cascade;
drop table if exists mpp3455b cascade;

CREATE TABLE mpp3455 (
id int,
rank int,
year int,
gender char(1),
count int )
DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
SUBPARTITION BY RANGE (year)
SUBPARTITION TEMPLATE (
SUBPARTITION 2001 START (2001),
SUBPARTITION 2002 START (2002),
SUBPARTITION 2003 START (2003),
SUBPARTITION 2004 START (2004),
SUBPARTITION 2005 START (2005),
SUBPARTITION 2006 START (2006) END (2007) )
(PARTITION girls VALUES ('F'),
PARTITION boys VALUES ('M') ) ;

CREATE TABLE mpp3455a (
id int,
rank int,
year int,
gender char(1),
count int )
DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
SUBPARTITION BY RANGE (year)
SUBPARTITION TEMPLATE (
SUBPARTITION "2001" START (2001),
SUBPARTITION "2002" START (2002),
SUBPARTITION "2003" START (2003),
SUBPARTITION "2004" START (2004),
SUBPARTITION "2005" START (2005),
SUBPARTITION "2006" START (2006) END (2007) )
(PARTITION girls VALUES ('F'),
PARTITION boys VALUES ('M') ) ;

CREATE TABLE mpp3455b (
id int,
rank int,
year int,
gender char(1),
count int )
DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
SUBPARTITION BY RANGE (year)
SUBPARTITION TEMPLATE (
SUBPARTITION year1 START (2001),
SUBPARTITION year2 START (2002),
SUBPARTITION year3 START (2003),
SUBPARTITION year4 START (2004),
SUBPARTITION year5 START (2005),
SUBPARTITION year6 START (2006) END (2007) )
(PARTITION girls VALUES ('F'),
PARTITION boys VALUES ('M') )
;

drop table mpp3455;
drop table mpp3455a;
drop table mpp3455b;
CREATE TABLE mpp3080_int8(q1 int8, q2 int8)
partition by range (q1)
(start (1) end (10) every (1));

CREATE TABLE mpp3080_float4 (f1  float4)
partition by range (f1)
(start (1) end (10) every (1));

CREATE TABLE mpp3080_float8(i INT DEFAULT 1, f1 float8)
partition by range (f1)
(start (1) end (10) every (1));

CREATE TABLE mpp3080_floatreal(i INT DEFAULT 1, f1 float(24))
partition by range (f1)
(start (1) end (10) every (1));

CREATE TABLE mpp3080_floatdouble(i INT DEFAULT 1, f1 float(53))
partition by range (f1)
(start (1) end (10) every (1));

CREATE TABLE mpp3080_numeric (id int4, val numeric(210,10))
partition by range (val)
(start (1) end (10) every (1));

CREATE TABLE mpp3080_numericbig (id int4, val numeric(1000,800))
partition by range (val)
(start (1) end (10) every (1));
-- order 1,5
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary 
  from pg_partitions where tablename like 'mpp3080%' order by tablename;

drop table mpp3080_int8;
drop table mpp3080_float4;
drop table mpp3080_float8;
drop table mpp3080_floatreal;
drop table mpp3080_floatdouble;
drop table mpp3080_numeric;
drop table mpp3080_numericbig;
create temp TABLE temp_hour_range (f1 time(2))
partition by range (f1)
(
  start (time '09:00') end (time '17:00') EVERY (INTERVAL '1 hour'),
  default partition default_part
);

