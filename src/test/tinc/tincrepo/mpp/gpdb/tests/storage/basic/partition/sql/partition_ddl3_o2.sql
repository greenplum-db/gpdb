-- @product_version gpdb: [4.3.6.0-]
-- @optimizer_mode off
-- start_matchsubs
-- m/%USER%/
-- s/%USER%/username/
-- s/command //
-- s/line 1: //
-- m/type: \\d+/
-- s/type: \\d+/type: XXX/
-- m/mpp6724_1_prt_r\d+_pkey/
-- s/(mpp6724_1_prt_r)\d+(_pkey)/$1xxxxxx$2/
-- end_matchsubs
set client_min_messages = WARNING;
DROP SCHEMA IF EXISTS partition_ddl3 CASCADE;
CREATE SCHEMA partition_ddl3;
set search_path to partition_ddl3;

CREATE TABLE mpp3263 (
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
( partition aa start (0) end (1000) every (500), default partition default_part );

-- These are OK
alter table mpp3263 add column AAA int;
alter table mpp3263 add column BBB int;
alter table mpp3263 drop column BBB;

alter table mpp3263 drop partition;

alter table mpp3263 add column CCC int;

insert into mpp3263 (unique1) values (1111);

drop table mpp3263;
CREATE TABLE mpp3541 (f1 time(2) with time zone, f2 char(4), f3 varchar(10))
partition by list (f2)
subpartition by range (f3)
subpartition template (
  subpartition male values ('Male','M'),
  subpartition female values ('Female','F')
)
( partition pst values ('PST'),
  partition est values ('EST')
);

CREATE TABLE mpp3541 (f1 time(2) with time zone, f2 char(4), f3 varchar(10))
partition by range (f2)
subpartition by list (f3)
subpartition template (
  subpartition male values ('Male','M'),
  subpartition female values ('Female','F')
)
( partition pst values ('PST'),
  partition est values ('EST')
);

CREATE TABLE mpp3541 (f1 time(2) with time zone, f2 char(4), f3 varchar(10))
partition by range (f2)
subpartition by range (f3)
subpartition template (
  subpartition male values ('Male','M'),
  subpartition female values ('Female','F')
)
( partition pst values ('PST'),
  partition est values ('EST')
);
CREATE TABLE mpp3542_000000000011111111112222222222333333333344444444445555555555556666666666777777777788888888889999999999 (f1 time(2) with time zone)
partition by range (f1)
(
  partition "Los Angeles" start (time with time zone '00:00 PST') end (time with time zone '23:00 PST') EVERY (INTERVAL '1 hour'),
  partition "New York" start (time with time zone '00:00 EST') end (time with time zone '23:00 EST') EVERY (INTERVAL '1 hour')
);

-- Truncates the table name to mpp3542_0000000000111111111122222222223333333333444444444455555, but partition name is too long, so ERROR
alter table mpp3542_000000000011111111112222222222333333333344444444445555555555556666666666777777777788888888889999999999 rename partition "Los Angeles_1" to "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
-- Truncates the table name to mpp3542_0000000000111111111122222222223333333333444444444455555, and partition name is safe, so renamed
alter table mpp3542_000000000011111111112222222222333333333344444444445555555555556666666666777777777788888888889999999999 rename partition "Los Angeles_1" to "LA1";
-- Use the actual table name
alter table mpp3542_0000000000111111111122222222223333333333444444444455555 rename partition "Los Angeles_2" to "LA2";
-- MPP-3542
alter table  mpp3542_0000000000111111111122222222223333333333444444444455555 rename to m; 


CREATE TABLE MULTI_PART2(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
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

alter table multi_part2 rename to multi_part2_0000000;
alter table multi_part2 rename to m_0000000;

-- We want to check both m and m_00000, thus I didn't put where clause
-- Assumes that there are no other partitions
-- order 2
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename like 'm_000%' order by tablename;

drop table m;
drop table m_0000000;
create table mpp3061 (i int) partition by range(i) (start(1) end(10) every(1));
insert into mpp3061 values(1);
-- Not allowed. MPP-3061
update mpp3061 set i = 2 where i = 1;
drop table mpp3061;
create table mpp3466 (i int) partition by range(i) (start(1) end(10) every(2), default partition f);
alter table mpp3466 split partition f at (3) into (partition f, partition new);
drop table mpp3466;
create table mpp3058 (a char(1), b date, d char(3))       
 distributed by (a)         
partition by range (b)                                                                                            
 (              
 partition aa start (date '2008-01-01') end (date '2009-01-01') 
 every (interval '10 days'));
drop table mpp3058;

create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)     
partition by range (b)    
(        
     partition aa start ('2008-01-01') end ('2010-01-01')           
  );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)     
partition by range (b)    
(        
     partition aa start ('2008-01-01') end ('2006-01-01')           
  );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)     
partition by range (b)    
(        
     partition aa start ('2008-01-01') end ('-2009-01-01')           
  );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)     
partition by range (b)    
(        
     partition aa start ('-2008-01-01') end ('2009-01-01')           
  );
drop table mpp3058;

create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)     
partition by range (b)    
(        
     partition aa start ('2008-01-01') end ('2010-01-01')           
  );
drop table mpp3058;

create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)        
partition by range (b)      
 (                  
           partition aa start ('2008-01-01') end ('2008-04-01') every(interval '1 day')   
     );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))  
  distributed by (a)    
   partition by range (b)             
  (           
    partition aa start ('2008-01-01') end ('2009-01-01') every( '1 day')  
     );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))  
  distributed by (a)    
   partition by range (b)             
  (           
    partition aa end ('2009-01-01') every( interval '1 day')  
     );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))  
  distributed by (a)    
   partition by range (b)             
  (           
    partition aa start ('2006-01-01') every( interval '1 day')  
     );
drop table mpp3058;
create table mpp3607 (aa int, bb int) partition by range (bb)
(partition foo start(2));
alter table mpp3607 add partition baz end (3); -- Expected Overlaps
alter table mpp3607 add partition baz end (4); -- Expected Overlaps
alter table mpp3607 add partition aa end (2); -- OK as of 4.2.0.0 (RIO)
alter table mpp3607 add partition bb end (1); -- Expected Overlaps
alter table mpp3607 add partition baz end (3); -- Expected Overlaps
alter table mpp3607 add partition baz end (4); -- Expected Overlaps
alter table mpp3607 add partition baz end (2); -- Expected Overlaps
alter table mpp3607 add partition bb end (0); -- Expected Overlaps

alter table mpp3607 drop partition aa;

alter table mpp3607 add partition aa end (-4); -- partition rule aa < -4, foo >=2
alter table mpp3607 add partition bb end (-3); -- Overlaps

alter table mpp3607 drop partition aa;

alter table mpp3607 add partition aa end (0); -- partition rule aa < 0, foo >=2
alter table mpp3607 drop partition aa;

alter table mpp3607 add partition aa start (4); -- Overlaps
alter table mpp3607 add partition aa start (3) end (4); -- Overlaps
alter table mpp3607 add partition aa start (0) end (1); -- partition rule aa bb>=0 and bb<1, foo bb>=2
alter table mpp3607 add partition bb start (-1) end (0); -- partition rule bb bb>=-1 and bb<0
alter table mpp3607 add partition cc start (-4); -- partition rule cc bb>=-4 and bb <-1
alter table mpp3607 add partition dd start (-5) end (-4);
alter table mpp3607 add partition ee start (-10);
alter table mpp3607 add partition ff start (-9) end (-8); -- Overlaps

drop table mpp3607;
CREATE TABLE mpp3632(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );
alter table mpp3632 add partition a1 start (30);
alter table mpp3632 add partition a2 start (25) end (30);
alter table mpp3632 add partition a3 start (10) end (20);
alter table mpp3632 add partition a4 start (20) end (21);
alter table mpp3632 add partition a5 start (22) end (24);
alter table mpp3632 add partition a6 start (21) end (22);
-- alter table mpp3632 add partition a7 start (23) end (24); -- MPP-3667

drop table mpp3632;
create table mpp3671 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
insert into mpp3671 select i from generate_series(1, 10) i;
alter table mpp3671 split partition for(1) at (1,2) into (partition f1a, partition f1b);
drop table mpp3671;
create table mpp3639 (i int) partition by range(i) (start(0) exclusive end(100) inclusive every(10));
insert into mpp3639 select i from generate_series(1, 100) i;
insert into mpp3639 select i from generate_series(1, 100) i;
insert into mpp3639 select i from generate_series(1, 100) i;
select * from mpp3639 order by i;
drop table mpp3639;
create table mpp3588 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8));
insert into mpp3588 select i from generate_series(1, 8) i;

alter table mpp3588 split partition for(1) at (1,2) into (partition fa, partition fb);
alter table mpp3588 split partition for(1) at (1,2) into (partition f1a, partition f1b); -- This has partition rules that overlaps
alter table mpp3588 split partition for(1) at (1,2) into (partition f2a, partition f2b); -- 5,6 are not within the boundary of first partition
alter table mpp3588 split partition for(1) at (1,2) into (partition f3a, partition f3b); 
alter table mpp3588 split partition for(1) at (1,2) into (partition f4a, partition f4b);
alter table mpp3588 split partition for(1) at (1,2) into (partition f5a, partition f5b); -- Out of the original boundary specification
alter table mpp3588 split partition for(1) at (1,2) into (partition f6a, partition f6b); -- I can keep going and going and going....
alter table mpp3588 split partition for(1) at (1,2) into (partition f7a, partition f7b);

drop table mpp3588;
--  MPP-3692, MPP-3679
create table mpp3679 (a text, b text) partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz); 
insert into mpp3679 values ('foo', 'blah');
insert into mpp3679 values ('bar', 'blah');
insert into mpp3679 values ('baz', 'blah');

alter table mpp3679 split default partition at ('baz') into (partition bing, default partition);
drop table mpp3679;
-- MPP-3691, MPP-3681
create table mpp3681 (id int, date date, amt decimal(10,2)) distributed by (id) partition by range(date) (start (date '2008-01-01') inclusive end ('2008-04-01') exclusive every (interval '1 month')); 

alter table mpp3681 add default partition def;

alter table mpp3681 split default partition start('2008-04-01') inclusive end('2008-05-01') exclusive into (partition apr08, default partition);

drop table mpp3681;
-- MPP-3593
create table mpp3593 (i int) partition by range(i) (start(1) end(100) every(10));
insert into mpp3593 select i from generate_series(1, 99) i;
alter table mpp3593 split partition for(1) at (5) into (partition aa, partition bb);
alter table mpp3593 split partition for(15) at (20) into (partition a1, partition b1);
alter table mpp3593 split partition for(25) at (30) into (partition a2, partition b2);
alter table mpp3593 split partition for(35) at (40) into (partition a3, partition b3);
alter table mpp3593 split partition for(55) at (60) into (partition a4, partition b4);
alter table mpp3593 split partition for(45) at (50) into (partition a5, partition b5);

drop table mpp3593;
CREATE TABLE mpp3742 (
c_id varchar(36),
ss_id varchar(36),
c_ts timestamp,
name varchar(36),
PRIMARY KEY (c_id,ss_id,c_ts)) partition by range (c_ts)
(
  start (date '2007-01-01')
  end (date '2008-01-01') every (interval '1 month'),
  default partition default_part

);
alter table mpp3742 split default partition start ('2009-01-01') end ('2009-02-01') into (partition a3, default partition);

drop table mpp3742;
create table mpp3597 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
insert into mpp3597 select i from generate_series(1, 100) i;
insert into mpp3597 values (NULL);
alter table mpp3597 split default partition at (NULL);
insert into mpp3597 values (NULL);

select * from mpp3597_1_prt_default_part where i=NULL; -- No NULL values

drop table mpp3597;
create table mpp3594 (i date) partition by range(i) (start('2008-01-01') end('2009-01-01') every(interval '1 month'), default partition default_part);
alter table mpp3594 split default partition start ('2009-01-01') end ('2009-02-01') into (partition aa, partition nodate);
drop table mpp3594;
CREATE TABLE mpp3512 (id int, rank int, year int, gender char(1), count int)
DISTRIBUTED BY (id);

create table mpp3512_part (like mpp3512) partition by range (year) ( start (2001) end (2006) every ('1'));

create table mpp3512a (like mpp3512_part);

\d mpp3512
\d mpp3512a
select * from pg_partitions where tablename='mpp3512_part';

drop table mpp3512;
drop table mpp3512_part;
drop table mpp3512a;
CREATE TABLE mpp3988 ( ps_partkey integer,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_supplycost)
subpartition by range (ps_supplycost)
(default partition foo (default subpartition bar));

drop table mpp3988;
CREATE TABLE mpp3816 (
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
        string4         name,
	startDate       date		
) partition by range (startDate)
( start ('2007-01-01') end ('2008-01-01') every (interval '1 month'), default partition default_part );

alter table mpp3816 add column AAA int;
alter table mpp3816 add column BBB int;
alter table mpp3816 drop column BBB;
alter table mpp3816 drop column startDate;

drop table mpp3816;
CREATE TABLE mpp3762_cities (
        city     varchar(80) primary key,
        location point
);

CREATE TABLE mpp3762_weather (
        city      varchar(80) references mpp3762_cities(city),
        temp_lo   int,
        temp_hi   int,
        prcp      real,
        date      date
);

CREATE TABLE mpp3762_cities_partition (
        city     varchar(80) primary key,
        location point
)  partition by list (city) ( partition a values ('Los Angeles'), partition b values ('San Mateo') );

CREATE TABLE mpp3762_weather_partition (
        city      varchar(80) references mpp3762_cities_partition(city),
        temp_lo   int,
        temp_hi   int,
        prcp      real,
        date      date
) partition by range(date) ( start('2008-01-01') end ('2009-01-01') every (interval '1 month'));

drop table mpp3762_cities, mpp3762_weather cascade;
drop table mpp3762_cities_partition, mpp3762_weather_partition cascade;

create table mpp3754a ( i int, d date, primary key (d)) partition by range(d) ( start ('2008-01-01')  inclusive end ('2008-12-01')  exclusive every (interval '1 month'));
create table mpp3754b ( i int, d date, constraint prim_tr primary key (d)) partition by range(d) ( start ('2008-01-01')  inclusive end ('2008-12-01')  exclusive every (interval '1 month'));

drop table mpp3754a;
drop table mpp3754b;
create table mpp4172 (a char(1), b int)
distributed by (b)
partition by range(a)
(
partition aa start ('2006') end ('2009'), partition bb start ('2007') end ('2008')
);
CREATE TABLE mpp4582 (id int,
mpp4582 int, year date, gender char(1))
DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01')
end (date '2006-01-01') every (interval '1 year')) (
partition boys values ('M'),
partition girls values ('F'),
default partition neuter
);

ALTER table mpp4582 drop partition for ('-1');
ALTER table mpp4582 drop partition for ('--');
alter table mpp4582 drop partition for (';');
alter table mpp4582 drop partition for ();
alter table mpp4582 drop partition for (NULL);
alter table mpp4582 drop partition for ('NULL');

drop table mpp4582;

CREATE TABLE mpp3641a (
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

CREATE TABLE mpp3641b (
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
alter table mpp3641b add default partition default_part;

CREATE INDEX mpp3641a_unique1 ON mpp3641a USING btree(unique1 int4_ops);
CREATE INDEX mpp3641a_unique2 ON mpp3641a USING btree(unique2 int4_ops);
CREATE INDEX mpp3641a_hundred ON mpp3641a USING btree(hundred int4_ops);
CREATE INDEX mpp3641a_stringu1 ON mpp3641a USING btree(stringu1 name_ops);

CREATE INDEX mpp3641b_unique1 ON mpp3641b USING btree(unique1 int4_ops);
CREATE INDEX mpp3641b_unique2 ON mpp3641b USING btree(unique2 int4_ops);
CREATE INDEX mpp3641b_hundred ON mpp3641b USING btree(hundred int4_ops);
CREATE INDEX mpp3641b_stringu1 ON mpp3641b USING btree(stringu1 name_ops);

\t
\di mpp3641*

drop table mpp3641a;
drop table mpp3641b;

\di mpp3641*
\t
create schema rgs;
show search_path;

create table rgs.mpp4604(
	id int,
	time timestamp
) partition by range( time ) (
	partition p1 start (date '2008-10-14') end (date '2008-10-15'),
	partition p2 end (date '2008-11-01')
);

ALTER TABLE rgs.mpp4604 SPLIT PARTITION p2 AT( '2008-10-20' ) INTO( PARTITION p2_tmp, PARTITION p3 );
alter table rgs.mpp4604 drop partition p3;

drop schema rgs cascade;
CREATE TABLE mpp3817 (
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
        string4         name,
	startDate       date		
) partition by range (startDate)
( start ('2007-01-01') end ('2008-01-01') every (interval '1 month'), default partition default_part );

alter table mpp3817 drop column unique1; -- Set distribution key to randomly
alter table mpp3817 drop column unique2;

\d mpp3817
\d mpp3817_1_prt_10
\d mpp3817_1_prt_default_part

drop table mpp3817;
-- All these should error out because they have overlapping range partitions

CREATE TABLE NATION (
            N_NATIONKEY INTEGER,
            N_NAME CHAR(25),
            N_REGIONKEY INTEGER,
            N_COMMENT VARCHAR(152)
            )

partition by range (n_regionkey) 
(
partition p1 start('0') end('1') inclusive, partition p2 start('1') end('5') inclusive
);

CREATE TABLE ORDERS (
                O_ORDERKEY INT8,
                O_CUSTKEY INTEGER,
                O_ORDERSTATUS CHAR(1),
                O_TOTALPRICE decimal,
                O_ORDERDATE date,
                O_ORDERPRIORITY CHAR(15),
                O_CLERK CHAR(15),
                O_SHIPPRIORITY integer,
                O_COMMENT VARCHAR(79)
                )
partition by range (o_custkey) 
subpartition by range (o_orderkey) 
(
partition p1 start('1') end('150001') every 9 (150000)
(subpartition sp1 start('1') end('1500000'),subpartition sp2 start('1351816') end('6000001'))
);

CREATE TABLE LINEITEM (
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
partition by range (l_discount) 
subpartition by range (l_quantity) 
,subpartition by range (l_tax) subpartition template (start('0') end('1.08') every 6 (1))
,subpartition by range (l_receiptdate) subpartition template (subpartition sp1 start('1992-01-03') end('1999-01-01'), subpartition sp2 start('1993-01-03') end ('1997-01-01'))
(
partition p1 start('0') end('1.1') 
(subpartition sp1 start('1') end('51') every(10)
) , partition p2 start('1.1') end ('2.2') (subpartition sp1 start('1') end('51') every (5))
);

CREATE TABLE ORDERS (
                O_ORDERKEY INT8,
                O_CUSTKEY INTEGER,
                O_ORDERSTATUS CHAR(1),
                O_TOTALPRICE decimal,
                O_ORDERDATE date,
                O_ORDERPRIORITY CHAR(15),
                O_CLERK CHAR(15),
                O_SHIPPRIORITY integer,
                O_COMMENT VARCHAR(79)
                )
partition by range (o_custkey) 
subpartition by range (o_orderdate) subpartition template (start('1992-01-01') end('1998-08-03') every 3 (interval '12 months')

)
,subpartition by range (o_orderkey) subpartition template (start('1') end('6000001') every 3 (3000000)

)
(
partition p1 start('1') , partition p2 start('55170') end('114873'), partition p3 start('44717') end('56000'), partition p4 start('114873') end('150001')
);
CREATE TABLE mpp3114 (f1 time(2) with time zone)
partition by range (f1)
(
  partition pst start (time with time zone '00:00 PST') end (time with time zone '23:00 PST') EVERY (INTERVAL '1 hour'),
  partition est start (time with time zone '00:00 EST') end (time with time zone '23:00 EST') EVERY (INTERVAL '1 hour')
);
DROP TABLE mpp3114;
CREATE TABLE sg_cal_detail_r1 (
     datacenter character varying(32),
     poolname character varying(128),
     machinename character varying(128),
     transactionid character varying(32),
     threadid integer,
     transactionorder integer,
     eventclass character(1),
     eventtime timestamp(2) without time zone,
     eventtype character varying(128),
     eventname character varying(128),
     status character varying(128),
     duration numeric(18,2),
     data character varying(4096)
)
WITH (appendonly=true, compresslevel=5, blocksize=2097152)
DISTRIBUTED BY (transactionid)
PARTITION BY RANGE(eventtime) 
SUBPARTITION BY LIST(datacenter)
SUBPARTITION TEMPLATE
(
SUBPARTITION SMF VALUES ('smf01','smf02'),
SUBPARTITION SJC VALUES ('sjc01','sjc02'),
SUBPARTITION DEN VALUES ('den01','den02'),
SUBPARTITION PHX VALUES ('phx01','phx02'),
DEFAULT SUBPARTITION xdc
)
SUBPARTITION BY LIST(eventtype)
SUBPARTITION TEMPLATE
(
SUBPARTITION ET1 VALUES ('EXEC'),
SUBPARTITION ET2 VALUES ('URL','EXECP','ufb'),
SUBPARTITION ET3 VALUES
('EXECT','V3Rules','SOJ','MEQ','RTM','TL','ActiveRules','RTMe','API',
'Info','BizProcess','APIRequest','_ui','Warning','Consume','XML','DSAPar
serTransform'),
SUBPARTITION ET4 VALUES('InflowHandler',
'TaskType',
'LOG',
'FETCH',
'TD',
'AxisInflowPipeline',
'AxisOutflowPipeline',
'API Security',
'SD_DSBE',
'SD_ExpressSale',
'V4Header',
'V4Footer',
'SOAP_Handler',
'MLR',
'EvictedStmtRemove',
'CT',
'DSATransform',
'APIClient',
'DSAQueryExec',
'processDSA',
'FilterEngine',
'Prefetch',
'AsyncCb',
'MC',
'SQL',
'SD_UInfo',
'TnSPayload',
'Serialization',
'CxtSetup',
'LazyInit',
'Deserialization',
'CleanUp',
'RESTDeserialize',
'RESTSerialize',
'SD_StoreNames',
'Serialize',
'Deserialize',
'SVC_INVOKE',
'SD_TitleAggr',
'eLVIS',
'SD_Promo',
'ServerCalLogId',
'SD_DSA',
'ClientCalLogId',
'NCF Async processor',
'V3Rules_OLAP',
'RTAM',
'SOAP_Handlers',
'SOAP_Ser',
'SOAP_Exec',
'RtmAB',
'RTPromotionOptimizer',
'crypt',
'Error',
'DBGetDataHlp',
'NoEncoding',
'Default',
'PromoAppCenter',
'BES_CONSUMER',
'TitleKeywordsModel',
'SOA_CLIENT',
'SD_UserContent',
'NCF',
'BEGenericPortlet',
'PortletExecution',
'SoaPortlet',
'ICEP',
'LOGIC',
'SYI_Eval_Detail',
'SD_Catalog',
'SignIn_Eval_Detail',
'Elvis Client',
'BES',
'TIMESTAMP',
'TLH',
'TLH-PRE-SYI',
'RFC',
'Offer_Eval_Detail',
'SFE_RunQuery',
'DBGetData',
'TKOItem2',
'Notification',
'XSHModel',
'APIDefinition',
'captcha',
'SD_HalfItem',
'Mail_Transport',
'MODPUT',
'60DAY_OLD_ITEM_FETCHED',
'List',
'RemotePortlet',
'MakeOffer_Eval_Detail',
'60_TO_90_DAY_OLD_ITEM_FETCHED',
'Logic',
'RtmGetContentName',
'BEPortletService',
'SYI_EUP_Rbo',
'SYI_Rbo',
'EOA',
'SEC',
'CCHP',
'TKOItem3',
'TnsFindingModelBucket',
'Mail_Send',
'SignIn_Rbo',
'SignIn=23_elvisEvl',
'TnsFindingModelXSH',
'RtmSvc',
'SWEET_TOOTH_LOCATOR_EXPIRED',
'COOKIE_INFO',
'Database',
'RYI_Eval_Detail',
'TnsFindingModelSNP',
'TitleRiskScoringModel_2_0',
'ClientIPin10',
'TnsFindingModelFraud',
'SignIn_BaseRbo2',
'Offer_EUP_Rbo',
'Offer_Rbo',
'FSA',
'Processing_elvis_events',
'NSS_API',
'MyebayBetaRedirect',
'MOTORS_PARTNER_RECIPIENT_HANDLER',
'ElvisEngine',
'PreSyi_Eval_Detail',
'RADAR',
'Latency',
'SD_TAggrCache',
'MEA',
'SD_TitleAggregatorShopping',
'KEM',
'SD_Batch',
'KG',
'ITEM_VISIBILITY',
'APPLOGIC',
'OOPexecute',
'ERRPAGE',
'FQ_RECIPIENT_HANDLER',
'RADAR_POST_Eval_Detail',
'Captcha',
'V3Rules_Detail',
'FilterEngDetail_AAQBuyerSentPre',
'Task',
'SYI_EUP_Report',
'WRITE_MOVE_FILE',
'KG_SYI',
'BatchRecord',
'SD_TitleDesc',
'B001_RTAM',
'SignIn_Report',
'SD_StoreUrl',
'CACHE_REFRESH',
'TKOItem',
'KG_EXTERNAL_CALL',
'WatchDelSkipped',
'SD_Completed',
'RequestCounts',
'FilterEngDetail_RTQEmail',
'FilterEngDetail_AAQBuyerSentPost',
'RYI_EUP_Rbo',
'RYI_Rbo',
'MF_RECIPIENT_HANDLER',
'SYI_Report',
'LCBT',
'HalfRyiSingle_Eval_Detail',
'FilterEngDetail_AAQBuyerSentEmail',
'ViewAdAbTests',
'MakeOffer_EUP_Rbo',
'MakeOffer_Rbo',
'ShipCalc Url',
'Offer_Report',
'TKOUser',
'RADAR_POST_EUP_Rbo',
'SiteStat_LeftNav',
'SiteStat_UserIsSeller',
'FilterEngDetail_RTQPost',
'INFO',
'Offer_EUP_Report',
'RADAR_POST_Rbo',
'SignIn_EUP_Rbo',
'Mail_XML',
'Processing_item_events',
'GEM',
'Mail',
'ELVIS',
'FilterEngDetail_SYIRYI',
'SD_TitleCach',
'Processing_itemend_events',
'HalfRyiSingle_EUP_Rbo',
'AlertNotify',
'AVSRedirectLog',
'BillerService',
'MENMSG',
'UserSegSvc',
'PRICE_CHANGE_ALERT_RECIPIENT_HANDLER',
'NSSOptP',
'PreSyi_Rbo',
'PreSyi_EUP_Rbo',
'NOTIFICATION.BES.BID_NEW',
'Mail_Connect',
'Mail_Close',
'GEMRECORD',
'McapCommunicatorTx',
'IMGPROC',
'KnownGood',
'FilterEngDetail_RTQPre',
'AUTH',
'BULKAPI',
'AAQBuyerSentPre_Eval_Detail',
'RYI_EUP_Report',
'HalfRyiSingle_Rbo',
'MakeOffer_Report',
'ItemClosureLOGIC',
'MakeOffer_EUP_Report',
'RADAR_POST_Report',
'BidBinStats',
'Iterator',
'RADAR_POST_EUP_Report',
'SessionStats',
'RYI_Report',
'SIBE',
'EOT',
'UsageProcessingTx',
'Processing_itemrevised_events',
'HalfSyiSingle_Eval_Detail',
'SignIn_EUP_Report',
'Referer',
'RTQEmail_Eval_Detail',
'AAQBuyerSentPost_Eval_Detail',
'AAQBuyerSentEmail_Eval_Detail',
'NCFEvent',
'CHKOUT',
'SocketWriter',
'RTQPost_Eval_Detail',
'HalfRyiSingle_Report',
'HalfRyiSingle_EUP_Report',
'DcpConnectRequest',
'SD_CatalogCache',
'PreSyi_Report',
'BotSignIn',
'Total Listing : BE_MAIN',
'Z',
'ItemPopularityScore',
'SD_TitleCacheOverflow',
'UserSegmentationCommand',
'FilterEngDetail_AAQSellerSentPre',
'PreSyi_EUP_Report',
'FilterEngDetail_AAQSellerSentPost',
'FilterEngDetail_BestOffer',
'RS',
'FilterEngDetail_AAQSellerSentEmail',
'HalfSyiSingle_EUP_Rbo',
'Service',
'Total Listing : BE_DE',
'BULK.API.HALF.PUT'),
DEFAULT SUBPARTITION etx
)
(
START ('2008-09-30')
END ('2008-10-01')
EVERY (INTERVAL '1 day')
);

ALTER TABLE sg_cal_detail_r1 ADD PARTITION START ('2008-10-01') INCLUSIVE END ('2008-10-02') EXCLUSIVE
WITH (appendonly=true, compresslevel=5, blocksize=2097152);

select count(*) from pg_partitions where tablename = 'sg_cal_detail_r1';

drop table sg_cal_detail_r1;
create table j (i int, a date) partition by range(i)
subpartition by list(a) subpartition template
(subpartition a values(1, 2, 3, 4),
subpartition b values(5, 6, 7, 8))
(
start (date '2001-01-01'),
start (date '2002-01-01'),
start (date '2003-01-01'),
start (date '2004-01-01'),
start (date '2005-01-01')
);
set optimizer_analyze_root_partition=on;
create table mpp3487 (i int) partition by range (i) (start(1) end(10) every(1));
insert into mpp3487 select i from generate_series(1, 9) i;
vacuum analyze mpp3487;
select  schemaname, tablename, attname, null_frac, avg_width, n_distinct, most_common_freqs, histogram_bounds, correlation from pg_stats where tablename like 'mpp3487%' order by 2;
drop table mpp3487;
-- Negative Test for Alter subpartition template
CREATE TABLE qa147sales (trans_id int, date date, amount 
decimal(9,2), region text)  
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

-- Invalid TEMPLATE
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (NULL);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (-1);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (10000);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ('');
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ("");
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (*);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (1*);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ("1*");
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (ABC);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ($);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (%%);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (#);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (!);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (&);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (^);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (@);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (<);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (>);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (.);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (?);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (/);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (|);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (~);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (`);

select * from pg_partition_templates where tablename='qa147sales';

drop table qa147sales;

select * from pg_partition_templates where tablename='qa147sales';;

-- Now with Schema
-- Negative Test for alter subpartition template with Schema
create schema qa147;
CREATE TABLE qa147.sales (trans_id int, date date, amount
decimal(9,2), region text)
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

-- Invalid TEMPLATE
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (NULL);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (-1);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (10000);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ('');
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ("");
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (*);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (1*);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ("1*");
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (ABC);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ($);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (%%);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (#);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (!);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (&);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (^);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (@);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (<);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (>);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (.);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (?);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (/);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (|);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (~);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (`);

select * from pg_partition_templates where schemaname='qa147' and tablename='sales';

drop schema qa147 cascade;

select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Mix-Match for Alter subpartition template
CREATE TABLE qa147sales (trans_id int, date date, amount 
decimal(9,2), region text)  
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

-- Clear TEMPLATE
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ();
select * from pg_partition_templates where tablename='qa147sales';
-- This will overwrite previous subpartition template
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), SUBPARTITION asia VALUES ('asia') );
select * from pg_partition_templates where tablename='qa147sales';
-- Invalid subpartition
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2008-02-01') EXCLUSIVE );
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2009-01-01') EXCLUSIVE EVERY (INTERVAL '1 month') );
select * from pg_partition_templates where tablename='qa147sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( 
SUBPARTITION usa1 VALUES('usa'),
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE);
select * from pg_partition_templates where tablename='qa147sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( 
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE,
SUBPARTITION usa1 VALUES('usa'));
select * from pg_partition_templates where tablename='qa147sales';

drop table qa147sales;

CREATE TABLE qa147sales (trans_id int, date date, amount
decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY LIST (region)
SUBPARTITION BY RANGE (date)
SUBPARTITION TEMPLATE
( START (date '2008-01-01') INCLUSIVE
   END (date '2009-01-01') EXCLUSIVE
   EVERY (INTERVAL '1 month') )
(
  PARTITION usa VALUES ('usa'),
  PARTITION asia VALUES ('asia'),
  PARTITION europe VALUES ('europe') );

-- Clear TEMPLATE
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ();
select * from pg_partition_templates where tablename='qa147sales';
-- This will overwrite previous subpartition template
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2008-02-01') EXCLUSIVE );
select * from pg_partition_templates where tablename='qa147sales';
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2009-01-01') EXCLUSIVE EVERY (INTERVAL '1 month') );
select * from pg_partition_templates where tablename='qa147sales';
-- Invalid subpartition template
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), SUBPARTITION asia VALUES ('asia') );
select * from pg_partition_templates where tablename='qa147sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
(
SUBPARTITION usa1 VALUES('usa'),
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE);
select * from pg_partition_templates where tablename='qa147sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( 
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE,
SUBPARTITION usa1 VALUES('usa'));
select * from pg_partition_templates where tablename='qa147sales';

drop table qa147sales;
select * from pg_partition_templates where tablename='qa147sales';

-- Now with Schema
-- Mix-Match for Alter subpartition template in a schema
create schema qa147;
CREATE TABLE qa147.sales (trans_id int, date date, amount
decimal(9,2), region text)
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

-- Clear TEMPLATE
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ();
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- This will overwrite previous subpartition template
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), SUBPARTITION asia VALUES ('asia') );
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Invalid subpartition
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2008-02-01') EXCLUSIVE );
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2009-01-01') EXCLUSIVE EVERY (INTERVAL '1 month') );
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
(
SUBPARTITION usa1 VALUES('usa'),
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE);
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
(
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE,
SUBPARTITION usa1 VALUES('usa'));
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';

DROP SCHEMA qa147 cascade;
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';

CREATE SCHEMA qa147;
CREATE TABLE qa147.sales (trans_id int, date date, amount
decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY LIST (region)
SUBPARTITION BY RANGE (date)
SUBPARTITION TEMPLATE
( START (date '2008-01-01') INCLUSIVE
   END (date '2009-01-01') EXCLUSIVE
   EVERY (INTERVAL '1 month') )
(
  PARTITION usa VALUES ('usa'),
  PARTITION asia VALUES ('asia'),
  PARTITION europe VALUES ('europe') );

-- Clear TEMPLATE
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ();
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- This will overwrite previous subpartition template
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2008-02-01') EXCLUSIVE );
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2009-01-01') EXCLUSIVE EVERY (INTERVAL '1 month') );
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Invalid subpartition template
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), SUBPARTITION asia VALUES ('asia') );
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
(
SUBPARTITION usa1 VALUES('usa'),
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE);
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
(
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE,
SUBPARTITION usa1 VALUES('usa'));
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';

drop schema qa147 cascade;
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
set gp_autostats_mode=on_change;
set gp_autostats_on_change_threshold=100;

create table mpp5427 (i int) partition by range (i) (start(1) end(10000000) every(100000));
insert into mpp5427 select i from generate_series(1, 100) i;
select * from pg_stats where tablename like 'mpp5427%';
insert into mpp5427 select i from generate_series(1, 100000) i;
select * from pg_stats where tablename like 'mpp5427%';
insert into mpp5427 select i from generate_series(1, 1000000) i;
select * from pg_stats where tablename like 'mpp5427%';

truncate table mpp5427;

alter table mpp5427 add default partition default_part;
insert into mpp5427 select i from generate_series(1, 100) i;
select * from pg_stats where tablename like 'mpp5427%';
insert into mpp5427 select i from generate_series(1, 100000) i;
select * from pg_stats where tablename like 'mpp5427%';
insert into mpp5427 select i from generate_series(1, 1000000) i;
select * from pg_stats where tablename like 'mpp5427%';
insert into mpp5427 select i from generate_series(10000000, 15000000) i;
select * from pg_stats where tablename like 'mpp5427%';

drop table mpp5427;

-- MPP-5524
create table mpp5524 (a int, b int, c int, d int) partition by range(d) (start(1) end(20) every(1));
-- Not allowed
alter table mpp5524 alter partition for(rank(1)) set distributed by (b);
-- Not allowed
alter table mpp5524 alter partition for(rank(2)) set distributed by (c);
insert into mpp5524 select i, i+1, i+2, i+3 from generate_series(1, 10) i;
drop table mpp5524;

CREATE TABLE fff_main (id int, rank int, year int, gender char(1), count int)
 partition by range (year) ( start (2001) end (2006) every ('1'));

alter table fff_main_1_prt_1 drop oids;
alter table fff_main_1_prt_1 no inherit fff_main;
alter table fff_main_1_prt_1 drop column rank;
alter table fff_main_1_prt_1 add partition;
alter table fff_main_1_prt_1 drop partition;

alter table fff_main_1_prt_1 add column c int;

create table fff_main2 (like fff_main);
alter table fff_main_1_prt_1 inherit fff_main2;
alter table fff_main_1_prt_1 alter column i type bigint;
alter table fff_main_1_prt_1 set without oids;
alter table fff_main_1_prt_1 drop constraint fff_main_1_prt_1_check;

-- Add default partition
alter table fff_main_1_prt_1 split partition def at ('2009');
alter table fff_main add default partition def;
alter table fff_main_1_prt_1 split partition def at ('2009');

-- Unable to coalesce or merge, not supported
alter table fff_main_1_prt_1 exchange partition aa  with table fff_main_1_prt_2 without validation;

alter table fff_main add partition aa start ('2008') end ('2009');
alter table fff_main add partition bb start ('2009') end ('2010');
alter table fff_main_1_prt_1 add partition cc start ('2010') end ('2011');

drop table fff_main, fff_main2;
create table mpp5878 (a int, b char, d char)
partition by list (b,d)
(values (('a','b'),('c','d')),
values (('e','f'),('g','h')));

select partitionlistvalues from pg_partitions where tablename like 'mpp5878';
select partitionboundary from pg_partitions where tablename like 'mpp5878';

-- pg_dump gptest -t mpp5878
--
-- Name: mpp5878; Type: TABLE; Schema: partition_ddl3; Owner: jsoedomo; Tablespace: 
--

CREATE TABLE mpp5878a (
    a integer,
    b character(1),
    d character(1)
) distributed by (a) PARTITION BY LIST(b, d) 
          (
          VALUES( ('a', 'b'),  ('c', 'd')), 
          VALUES( ('e', 'f'),  ('g', 'h'))
          );

select partitionlistvalues from pg_partitions where tablename like 'mpp5878a';
select partitionboundary from pg_partitions where tablename like 'mpp5878a';

drop table mpp5878, mpp5878a;
CREATE TABLE sg_cal_event_silvertail_hour (
caldt date NOT NULL,                                               
calhr smallint NOT NULL,
ip character varying(128),
transactionid character varying(32),
transactiontime timestamp(2) without time zone
)
WITH (appendonly=true, compresslevel=5)
distributed by (ip) PARTITION BY RANGE(transactiontime)
(
PARTITION "P2009041607"
START ('2009-04-16 07:00:00'::timestamp without time zone)
END ('2009-04-16 08:00:00'::timestamp without time zone),
PARTITION "P2009041608"
START ('2009-04-16 08:00:00'::timestamp without time zone)
END ('2009-04-16 09:00:00'::timestamp without time zone),
DEFAULT PARTITION st_default
);

ALTER TABLE SG_CAL_EVENT_SILVERTAIL_HOUR SPLIT DEFAULT PARTITION
START ('2009-04-29 07:00:00'::timestamp) INCLUSIVE END ('2009-04-29
08:00:00'::timestamp) EXCLUSIVE INTO ( PARTITION "P2009042907" ,
PARTITION st_default );

\d sg_cal_event_silvertail_hour*

drop table sg_cal_event_silvertail_hour;
CREATE TABLE mpp5185 (id int, mpp5185 int, year date, gender
char(1)) DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'),
start (date '2002-01-01'),
start (date '2003-01-01'),
start (date '2004-01-01'),
start (date '2005-01-01')
)
(
partition boys values ('M'),
partition girls values ('F')
);

alter table mpp5185 set subpartition template();
alter table mpp5185 set subpartition template (start (date '2006-01-01') with (appendonly=true));

select * from pg_partition_templates where tablename='mpp5185';

alter table mpp5185 add partition f1 values ('N');
alter table mpp5185 set subpartition template (start (date '2007-01-01') with (appendonly=true, compresslevel=5));
alter table mpp5185 add partition f2 values ('C');

select * from pg_partition_templates where tablename='mpp5185';

drop table mpp5185;
CREATE TABLE partsupp_def ( ps_partkey int,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_partkey)
subpartition by range (ps_partkey)
subpartition template
( subpartition sp1 start(0) end (300) every(100),
  default subpartition subdef
)
( partition aa start (0) end (300) every(100),
  default partition def
);

alter table partsupp_def set subpartition template();
alter table partsupp_def set subpartition template( subpartition aaa start(400) end (600) every(100) );
-- Note 1: We cannot add subpartition template since we have a default partition
-- If we want to use the new subpartition template, we have to drop the default partition first, and then readd the default partition
-- Note 2: We do not support this function yet, but if we are able to split default partition with default subpartition, would we
-- be using the subpartition template to definte the "new" partition or the existing one.

select * from pg_partition_templates where tablename='partsupp_def';

alter table partsup_def add partition f1 start(0) end (300) every(100);

-- This works adding subpartition template with parent default partition
CREATE TABLE partsupp_def2 ( ps_partkey int,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_partkey)
subpartition by range (ps_partkey)
subpartition template
( subpartition sp1 start(0) end (300) every(100),
  default subpartition subdef
)
( partition aa start (0) end (300) every(100)
);

alter table partsupp_def2 set subpartition template();
alter table partsupp_def2 set subpartition template( subpartition aaa start(400) end (600) every(100) );

select * from pg_partition_templates where tablename='partsupp_def2';

drop table partsupp_def;
drop table partsupp_def2;
create table mpp5431 (c1 date, c2 int) distributed by (c2) partition by range (c1) (partition p20090401 start('2009-04-01 00:00:00'::timestamp) inclusive end ('2009-04-02 00:00:00'::timestamp) exclusive);
alter table mpp5431 add partition p20090402 start('2009-04-02 00:00:00'::timestamp) inclusive end ('2009-04-03 00:00:00'::timestamp) exclusive;

create or replace function mpp5431_f1 () returns void as $$
begin
alter table mpp5431 add partition p20090403 start('2009-04-03 00:00:00'::timestamp) inclusive end ('2009-04-04 00:00:00'::timestamp) exclusive;
end;
$$ LANGUAGE 'plpgsql';

select mpp5431_f1();

drop function mpp5431_f1();
drop table mpp5431;
CREATE TABLE sg_cal_event_silvertail_hour (
caldt date NOT NULL,                                               
calhr smallint NOT NULL,
ip character varying(128),
transactionid character varying(32),
transactiontime timestamp(2) without time zone
)
WITH (appendonly=true, compresslevel=5)
distributed by (ip) PARTITION BY RANGE(transactiontime)
(
PARTITION "P2009041607"
START ('2009-04-16 07:00:00'::timestamp without time zone)
END ('2009-04-16 08:00:00'::timestamp without time zone),
PARTITION "P2009041608"
START ('2009-04-16 08:00:00'::timestamp without time zone)
END ('2009-04-16 09:00:00'::timestamp without time zone),
DEFAULT PARTITION st_default
);

ALTER TABLE SG_CAL_EVENT_SILVERTAIL_HOUR SPLIT DEFAULT PARTITION
START ('2009-04-29 07:00:00'::timestamp) INCLUSIVE END ('2009-04-29
08:00:00'::timestamp) EXCLUSIVE INTO ( PARTITION "P2009042907" ,
PARTITION st_default );

select pg_get_partition_def('sg_cal_event_silvertail_hour'::regclass, true);

drop table sg_cal_event_silvertail_hour;

CREATE TABLE mpp6612 (
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
( partition aa start (0) end (1000) every (500), default partition default_part );

-- Modify any other column
alter table mpp6612 alter column unique2 type char(15);
alter table mpp6612 alter column unique2 type char(10);
-- Show the dsecription
\d mpp6612*

-- Modify the partion definition. MPP-3724
-- alter table mpp6612 alter column unique1 type char(10); -- This should fail
-- alter table mpp6612 alter column unique1 type int8;
-- Show the dsecription
-- \d mpp6612*


drop table mpp6612;
create table mpp4048 (aaa int, bbb date)
partition by range (bbb)
subpartition by range (bbb)
subpartition by range (bbb)
(
partition y2008 start (date '2008-01-01') end (date '2009-01-01')
(
  subpartition dec start (date '2008-12-01') end (date '2009-01-01') (start (date '2008-12-01') end (date '2009-01-01') every (interval '1 day'))
));

drop table mpp4048;
-- This is only for ADD primary key for partition table
-- DROP primary key is still in progress
-- MPP-6573
CREATE TABLE mpp6573 (id int, date date, amt decimal(10,2)) 
DISTRIBUTED BY (id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2008-01-03') EXCLUSIVE 
   EVERY (INTERVAL '1 day') );
alter table mpp6573 add primary key (id, date) ;
\d mpp6573*

drop table mpp6573;


-- MPP-6724
create table mpp6724 ( c1 int, dt DATE, c2 varchar, PRIMARY KEY ( c1,dt ) ) distributed by (c1) partition by range ( dt ) ( start ( date '2009-01-01' ) inclusive end ( date '2009-01-03' ) EXCLUSIVE EVERY ( INTERVAL '1 day' ) );
insert into mpp6724 values ( 1,'2009-01-01','One'), (2,'2009-01-02','Two'),(3,'2009-01-01','three'), (3,'2009-01-02', 'three`');
insert into mpp6724 values ( 1,'2009-01-01','One'); -- This violate the primary key, expected to fail

alter table mpp6724 add partition start ( date '2009-01-03' ) inclusive end ( date '2009-01-04' ) ;
insert into mpp6724 values ( 4,'2009-01-03','Four');

-- Should fail because violates the primary key
insert into mpp6724 values ( 4,'2009-01-03','Four');
insert into mpp6724 values ( 4,'2009-01-03','Four');
insert into mpp6724 values ( 4,'2009-01-03','Four');
insert into mpp6724 values ( 4,'2009-01-03','Four');

select c1, dt, count(*) from mpp6724 group by 1,2 having count(*) > 1;

drop table mpp6724;

-- MPP-6247
-- Delete Using on partitioned table causes repetitive scans on using table	
create table mpp6247_foo ( c1 int, dt date ) distributed by ( c1 ) partition by range (dt) ( start ( date '2009-05-01' ) end ( date '2009-05-11' ) every ( interval '1 day' ) );
create table mpp6247_bar (like mpp6247_foo);

-- count number of certain operators in a given plan
-- start_ignore
create language plpythonu;
-- end_ignore
create or replace function count_operator(explain_query text, operator text) returns int as
$$
rv = plpy.execute(explain_query)
search_text = operator
result = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = result+1
return result
$$
language plpythonu;

-- EXPECT: Single HJ after partition elimination instead of sequence of HJ under Append
select count_operator('explain delete from mpp6247_foo using mpp6247_bar where mpp6247_foo.c1 = mpp6247_bar.c1 and mpp6247_foo.dt = ''2009-05-03''', 'Hash Join');

drop function count_operator;

drop table mpp6247_bar;
drop table mpp6247_foo;

-- --------------------------------------------------------------------------------------
-- COPYRIGHT: Copyright (c) 2010, Greenplum.  All rights reserved.
-- PURPOSE: test partition elimination, MPP-7891
--
-- AUTHOR: Lyublena Antova
--
-- WARNINGS (if applicable):
-- ALGORITHM (optional):
-- LAST MODIFIED:
--      - 2010/02/04: initial version
-- --------------------------------------------------------------------------------------

-- cleanup
-- start_ignore
drop table if exists r_part;
drop table if exists r_co;

deallocate f1;
deallocate f2;
deallocate f3;

create table r_part(a int, b int) partition by range(a) (start (1) end(10) every(1));
create table r_co(a int, b int) with (orientation=column, appendonly=true) partition by range(a) (start (1) end(10) every(1)) ;

-- end_ignore

insert into r_part values (1,1), (2,2), (3,3);

select * from r_part order by a,b;

analyze r_part;

explain select * from r_part r1, r_part r2 where r1.a=1; -- should eliminate partitions in the r1 copy of r_part

explain select * from r_part where a in (1,2); -- should eliminate partitions

/*
	test partition elimination in prepared statements
*/
prepare f1(int) as select * from r_part where a = 1 order by a,b; 
prepare f2(int) as select * from r_part where a = $1 order by a,b;


execute f1(1); 
execute f2(1); 
execute f2(2); 


explain select * from r_part where a = 1 order by a,b; -- should eliminate partitions
--force_explain
explain execute f1(1); -- should eliminate partitions 
--force_explain
explain execute f2(2); -- should eliminate partitions


/*
	test partition elimination on CO tables
*/

insert into r_co values (1,1), (2,2), (3,3);

analyze r_co; 

explain select * from r_co where a=2; -- should eliminate partitions

/*
	test partition elimination in prepared statements on CO tables
*/
prepare f3(int) as select * from r_co where a = $1 order by a,b; 
--force_explain
explain execute f3(2); -- should eliminate partitions

-- start_ignore
drop table r_part;
drop table r_co;
deallocate f1;
deallocate f2;
deallocate f3;
-- end_ignore
-- --------------------------------------------------------------------------------------
-- COPYRIGHT: Copyright (c) 2010, Greenplum.  All rights reserved.
-- PURPOSE: test partition elimination, MPP-7891
--
-- AUTHOR: Lyublena Antova
--
-- WARNINGS (if applicable):
-- ALGORITHM (optional):
-- LAST MODIFIED:
--      - 2010/02/04: initial version
-- --------------------------------------------------------------------------------------
-- start_ignore
drop table if exists fact;
deallocate f1;

create table fact(x int, dd date, dt text) distributed by (x) partition by range (dd) ( start('2008-01-01') end ('2320-01-01') every(interval '100 years'));
-- end_ignore

analyze fact;

select '2009-01-02'::date = to_date('2009-01-02','YYYY-MM-DD'); -- ensure that both are in fact equal

explain select * from fact where dd < '2009-01-02'::date; -- partitions eliminated

explain select * from fact where dd < to_date('2009-01-02','YYYY-MM-DD'); -- partitions eliminated

explain select * from fact where dd < current_date; --partitions eliminated

-- test partition elimination in prepared statements

prepare f1(date) as select * from fact where dd < $1;

-- force_explain
explain execute f1('2009-01-02'::date); -- should eliminate partitions
-- force_explain
explain execute f1(to_date('2009-01-02', 'YYYY-MM-DD')); -- should eliminate partitions

-- start_ignore
drop table fact;
deallocate f1;
-- end_ignore
-- Test for partition cleanup
-- Johnny Soedomo

-- start_ignore
drop schema partition_999 cascade;
create schema partition_999;

set search_path=partition_ddl3,partition_999;
-- end_ignore

create table partition_cleanup1 (a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );

CREATE TABLE partition_999.partition_cleanup2(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
subpartition by range (b) subpartition template ( start (1) end (10) every (1))
( partition aa start (1) end (10) every (1) );

drop table partition_cleanup1;
drop schema partition_999 cascade;

-- These should be empty
select 'pg_partition_columns', count(*) from pg_partition_columns where tablename='partition_cleanup%';
select 'pg_partitions', count(*) from pg_partitions where tablename='partition_cleanup%';
select 'pg_partition_templates', count(*) from pg_partition_templates where tablename='partition_cleanup%';

