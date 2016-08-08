DROP TABLE IF EXISTS multi_segfile_tab;
CREATE TABLE multi_segfile_tab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1) DISTRIBUTED BY (c_custkey);
insert into multi_segfile_tab values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now()); 
insert into multi_segfile_tab values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now()); 
update multi_segfile_tab set c_name = 'bcx' where c_custkey = 2;
vacuum multi_segfile_tab;
insert into multi_segfile_tab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now()); 



DROP TABLE IF EXISTS multi_segfile_bitab;
CREATE TABLE multi_segfile_bitab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1) DISTRIBUTED BY (c_custkey);
CREATE INDEX multi_segfile_btree_idx ON multi_segfile_bitab USING btree (c_custkey);
insert into multi_segfile_bitab values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now()); 
insert into multi_segfile_bitab values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now()); 
update multi_segfile_bitab set c_name = 'bcx' where c_custkey = 2;
vacuum multi_segfile_bitab;
insert into multi_segfile_bitab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now()); 


DROP TABLE IF EXISTS multi_segfile_zlibtab;
CREATE TABLE multi_segfile_zlibtab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=zlib, compresslevel=9) DISTRIBUTED BY (c_custkey);
insert into multi_segfile_zlibtab values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now()); 
insert into multi_segfile_zlibtab values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now()); 
update multi_segfile_zlibtab set c_name = 'bcx' where c_custkey = 2;
vacuum multi_segfile_zlibtab;
insert into multi_segfile_zlibtab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now()); 


DROP TABLE IF EXISTS multi_segfile_parttab;
CREATE TABLE multi_segfile_parttab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1) DISTRIBUTED BY (c_custkey)
partition by range(c_custkey)  subpartition by range( c_rating) 
subpartition template ( default subpartition subothers,start (0.0) end(1.9) every (2.0) ) 
(default partition others, partition p1 start(1) end(5000), partition p2 start(5000) end(10000), partition p3 start(10000) end(15000));
insert into multi_segfile_parttab values( 1, 'aa','this is a looong text' , 4.5, '12121212',1000.34,'2015/10/10',now()); 
insert into multi_segfile_parttab values( 2, 'ab','this is also a looong text' , 7.5, '3456789',3000.45,'2014/08/10',now()); 
update multi_segfile_parttab set c_name = 'bcx' where c_custkey = 2;
vacuum multi_segfile_parttab;
insert into multi_segfile_parttab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',4000.25,'2014/08/10',now());

DROP TABLE IF EXISTS multi_segfile_toast;
CREATE TABLE multi_segfile_toast (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1) DISTRIBUTED BY (c_custkey);
insert into multi_segfile_toast values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now()); 
insert into multi_segfile_toast values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now()); 
update multi_segfile_toast set c_name = 'bcx' where c_custkey = 2;
vacuum multi_segfile_toast;
insert into multi_segfile_toast values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now()); 

