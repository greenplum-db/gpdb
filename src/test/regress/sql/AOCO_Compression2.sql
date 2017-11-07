--
-- Drop table if exists
--
DROP TABLE if exists co_crtb_with_strg_dir_and_col_ref_1 cascade;

DROP TABLE if exists co_crtb_with_strg_dir_and_col_ref_1_uncompr cascade;

--
-- Create table
--
CREATE TABLE co_crtb_with_strg_dir_and_col_ref_1 ( 
 id SERIAL,a1 int ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a2 char(5) ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
a3 numeric ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a4 boolean DEFAULT false  ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a5 char DEFAULT 'd' ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a6 text ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
a7 timestamp ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a8 character varying(705) ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a9 bigint ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a10 date ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
a11 varchar(600) ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a12 text ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a13 decimal ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a14 real ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
a15 bigint ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a16 int4  ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a17 bytea ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a18 timestamp with time zone ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
a19 timetz ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a20 path ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a21 box ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a22 macaddr ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
a23 interval ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a24 character varying(800) ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a25 lseg ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a26 point ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
a27 double precision ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a28 circle ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a29 int4 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a30 numeric(8) ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
a31 polygon ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a32 date ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a33 real ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a34 money ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
a35 cidr ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a36 inet ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a37 time ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a38 text ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
a39 bit ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a40 bit varying(5) ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a41 smallint ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576),
a42 int ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768), COLUMN a1 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a2 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
, COLUMN a3 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a4 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a5 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a6 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
, COLUMN a7 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a8 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a9 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a10 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
, COLUMN a11 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a12 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a13 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a14 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
, COLUMN a15 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a16 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a17 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a18 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
, COLUMN a19 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a20 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a21 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a22 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
, COLUMN a23 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a24 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a25 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a26 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
, COLUMN a27 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a28 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a29 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a30 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
, COLUMN a31 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a32 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a33 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a34 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
, COLUMN a35 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a36 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a37 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a38 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)
, COLUMN a39 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a40 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a41 ENCODING (compresstype=zlib,compresslevel=1,blocksize=1048576)
, COLUMN a42 ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),DEFAULT COLUMN ENCODING (compresstype=zlib,blocksize=8192)) WITH (appendonly=true, orientation=column) distributed randomly;

-- 
-- Create Indexes
--
CREATE INDEX co_crtb_with_strg_dir_and_col_ref_1_idx_bitmap ON co_crtb_with_strg_dir_and_col_ref_1 USING bitmap (a1);

CREATE INDEX co_crtb_with_strg_dir_and_col_ref_1_idx_btree ON co_crtb_with_strg_dir_and_col_ref_1(a9);

--
-- Insert data to the table
--
 INSERT INTO co_crtb_with_strg_dir_and_col_ref_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_crtb_with_strg_dir_and_col_ref_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


-- More data for bigger block size 


--Create Uncompressed table of same schema definition

CREATE TABLE co_crtb_with_strg_dir_and_col_ref_1_uncompr(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly;

--
-- Insert to uncompressed table
--
 INSERT INTO co_crtb_with_strg_dir_and_col_ref_1_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_crtb_with_strg_dir_and_col_ref_1_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


-- More data for bigger block size 


--
-- ********Validation******* 
--
\d+ co_crtb_with_strg_dir_and_col_ref_1

--Select from pg_attribute_encoding to see the table entry 
select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'co_crtb_with_strg_dir_and_col_ref_1' and c.oid=e.attrelid  order by relname, attnum limit 3; 
--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  co_crtb_with_strg_dir_and_col_ref_1_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  co_crtb_with_strg_dir_and_col_ref_1;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from co_crtb_with_strg_dir_and_col_ref_1 t1 full outer join co_crtb_with_strg_dir_and_col_ref_1_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table co_crtb_with_strg_dir_and_col_ref_1;
--
-- Insert data again 
--
insert into co_crtb_with_strg_dir_and_col_ref_1 select * from co_crtb_with_strg_dir_and_col_ref_1_uncompr order by a1;

--
-- Select the data: Using the JOIN as mentioned above 
--
Select count(*) as count_join from co_crtb_with_strg_dir_and_col_ref_1 t1 full outer join co_crtb_with_strg_dir_and_col_ref_1_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--Alter table alter type of a column 
Alter table co_crtb_with_strg_dir_and_col_ref_1 Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into co_crtb_with_strg_dir_and_col_ref_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_with_strg_dir_and_col_ref_1 where id =10;
Select count(*) from co_crtb_with_strg_dir_and_col_ref_1; 

--Alter table drop a column 
Alter table co_crtb_with_strg_dir_and_col_ref_1 Drop column a12; 
Insert into co_crtb_with_strg_dir_and_col_ref_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_with_strg_dir_and_col_ref_1 where id =10;
Select count(*) from co_crtb_with_strg_dir_and_col_ref_1; 

--Alter table rename a column 
Alter table co_crtb_with_strg_dir_and_col_ref_1 Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into co_crtb_with_strg_dir_and_col_ref_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_with_strg_dir_and_col_ref_1 where id =10;
Select count(*) from co_crtb_with_strg_dir_and_col_ref_1; 

--Alter table add a column 
Alter table co_crtb_with_strg_dir_and_col_ref_1 Add column a12 text default 'new column'; 
--Insert data to the table, select count(*)
Insert into co_crtb_with_strg_dir_and_col_ref_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_with_strg_dir_and_col_ref_1 where id =10;
Select count(*) from co_crtb_with_strg_dir_and_col_ref_1; 

--
-- Drop table if exists
--
DROP TABLE if exists co_cr_sub_partzlib8192_1 cascade;

DROP TABLE if exists co_cr_sub_partzlib8192_1_uncompr cascade;

--
-- Create table
--
CREATE TABLE co_cr_sub_partzlib8192_1 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )  WITH (appendonly=true, orientation=column) distributed randomly 
 Partition by range(a1) Subpartition by list(a2) subpartition template ( default subpartition df_sp, subpartition sp1 values('M') , subpartition sp2 values('F')  , 
 COLUMN a2  ENCODING (compresstype=zlib,compresslevel=1,blocksize=8192), 
 COLUMN a1 encoding (compresstype = zlib),
 COLUMN a5 ENCODING (compresstype=zlib,compresslevel=1, blocksize=8192),
 DEFAULT COLUMN ENCODING (compresstype=zlib,compresslevel=1,blocksize=8192)) (start(1) end(5000) every(1000));

-- 
-- Create Indexes
--
CREATE INDEX co_cr_sub_partzlib8192_1_idx_bitmap ON co_cr_sub_partzlib8192_1 USING bitmap (a1);

CREATE INDEX co_cr_sub_partzlib8192_1_idx_btree ON co_cr_sub_partzlib8192_1(a9);

--
-- Insert data to the table
--
 INSERT INTO co_cr_sub_partzlib8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_cr_sub_partzlib8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


--Create Uncompressed table of same schema definition

CREATE TABLE co_cr_sub_partzlib8192_1_uncompr(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly Partition by range(a1) Subpartition by list(a2) subpartition template ( subpartition sp1 values('M') , subpartition sp2 values('F') ) (start(1)  end(5000) every(1000)) ;

--
-- Insert to uncompressed table
--
 INSERT INTO co_cr_sub_partzlib8192_1_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_cr_sub_partzlib8192_1_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


--
-- ********Validation******* 
--
\d+ co_cr_sub_partzlib8192_1_1_prt_1_2_prt_sp2


--Select from pg_attribute_encoding to see the table entry 
select parencattnum, parencattoptions from pg_partition_encoding e, pg_partition p, pg_class c  where c.relname = 'co_cr_sub_partzlib8192_1' and c.oid = p.parrelid and p.oid = e.parencoid order by parencattnum limit 3; 
--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  co_cr_sub_partzlib8192_1_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  co_cr_sub_partzlib8192_1;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from co_cr_sub_partzlib8192_1 t1 full outer join co_cr_sub_partzlib8192_1_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table co_cr_sub_partzlib8192_1;
--
-- Insert data again 
--
insert into co_cr_sub_partzlib8192_1 select * from co_cr_sub_partzlib8192_1_uncompr order by a1;

--
-- Compression ratio
--
--
-- Select the data: Using the JOIN as mentioned above 
--
Select count(*) as count_join from co_cr_sub_partzlib8192_1 t1 full outer join co_cr_sub_partzlib8192_1_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;

--Alter table Add Partition 
alter table co_cr_sub_partzlib8192_1 add partition new_p start(5050) end (6051);

--Validation with psql utility 
  \d+ co_cr_sub_partzlib8192_1_1_prt_new_p_2_prt_sp1

alter table co_cr_sub_partzlib8192_1 add default partition df_p ;

--Validation with psql utility 
  \d+ co_cr_sub_partzlib8192_1_1_prt_df_p_2_prt_sp2

-- Insert data 
Insert into co_cr_sub_partzlib8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,5000),'C',2011,'t','a','dfjjjjjj','2001-12-24 02:26:11','hghgh',333,'2011-10-11','Tddd','sss','1234.56',323453,4454,7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','dfdf','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','ggg','1','0',12,23) ; 


--Alter table Exchange Partition 
--Create a table to use in exchange partition 
Drop Table if exists co_cr_sub_partzlib8192_1_exch; 
 CREATE TABLE co_cr_sub_partzlib8192_1_exch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column, compresstype=zlib)  distributed randomly;
 
Drop Table if exists co_cr_sub_partzlib8192_1_defexch; 
 CREATE TABLE co_cr_sub_partzlib8192_1_defexch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column, compresstype=zlib)  distributed randomly;
 
Insert into co_cr_sub_partzlib8192_1_exch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1 where  a1=10 and a2!='C';

Insert into co_cr_sub_partzlib8192_1_defexch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1 where a1 =10 and a2!='C';

Alter table co_cr_sub_partzlib8192_1 alter partition FOR (RANK(1)) exchange partition sp1 with table co_cr_sub_partzlib8192_1_exch;
\d+ co_cr_sub_partzlib8192_1_1_prt_1_2_prt_sp1


Select count(*) from co_cr_sub_partzlib8192_1; 

--Alter table Drop Partition 
alter table co_cr_sub_partzlib8192_1 drop partition new_p;

-- Drop the default partition 
alter table co_cr_sub_partzlib8192_1 drop default partition;

--Alter table alter type of a column 
Alter table co_cr_sub_partzlib8192_1 Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into co_cr_sub_partzlib8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1 where id =10;
Select count(*) from co_cr_sub_partzlib8192_1; 

--Alter table drop a column 
Alter table co_cr_sub_partzlib8192_1 Drop column a12; 
Insert into co_cr_sub_partzlib8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1 where id =10;
Select count(*) from co_cr_sub_partzlib8192_1; 

--Alter table rename a column 
Alter table co_cr_sub_partzlib8192_1 Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into co_cr_sub_partzlib8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1 where id =10;
Select count(*) from co_cr_sub_partzlib8192_1; 

--Alter table add a column 
Alter table co_cr_sub_partzlib8192_1 Add column a12 text default 'new column'; 
--Insert data to the table, select count(*)
Insert into co_cr_sub_partzlib8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1 where id =10;
Select count(*) from co_cr_sub_partzlib8192_1; 

--
-- Drop table if exists
--
DROP TABLE if exists co_cr_sub_partzlib8192_1_2 cascade;

DROP TABLE if exists co_cr_sub_partzlib8192_1_2_uncompr cascade;

--
-- Create table
--
CREATE TABLE co_cr_sub_partzlib8192_1_2 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )  WITH (appendonly=true, orientation=column) distributed randomly 
 Partition by list(a2) Subpartition by range(a1) subpartition template (default subpartition df_sp, start(1)  end(5000) every(1000), 
 COLUMN a2  ENCODING (compresstype=zlib,compresslevel=1,blocksize=8192), 
 COLUMN a1 encoding (compresstype = zlib),
 COLUMN a5 ENCODING (compresstype=zlib,compresslevel=1, blocksize=8192),
 DEFAULT COLUMN ENCODING (compresstype=zlib,compresslevel=1,blocksize=8192)) (partition p1 values('F'), partition p2 values ('M'));

-- 
-- Create Indexes
--
CREATE INDEX co_cr_sub_partzlib8192_1_2_idx_bitmap ON co_cr_sub_partzlib8192_1_2 USING bitmap (a1);

CREATE INDEX co_cr_sub_partzlib8192_1_2_idx_btree ON co_cr_sub_partzlib8192_1_2(a9);

--
-- Insert data to the table
--
 INSERT INTO co_cr_sub_partzlib8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_cr_sub_partzlib8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


--Create Uncompressed table of same schema definition

CREATE TABLE co_cr_sub_partzlib8192_1_2_uncompr(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly Partition by list(a2)  Subpartition by range(a1) subpartition template (start(1)  end(5000) every(1000)) (default partition p1 , partition p2 values ('M') );

--
-- Insert to uncompressed table
--
 INSERT INTO co_cr_sub_partzlib8192_1_2_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_cr_sub_partzlib8192_1_2_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 

--
-- ********Validation******* 
--
\d+ co_cr_sub_partzlib8192_1_2_1_prt_p1_2_prt_2 


--Select from pg_attribute_encoding to see the table entry 
select parencattnum, parencattoptions from pg_partition_encoding e, pg_partition p, pg_class c  where c.relname = 'co_cr_sub_partzlib8192_1_2' and c.oid = p.parrelid and p.oid = e.parencoid order by parencattnum limit 3; 
--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  co_cr_sub_partzlib8192_1_2_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  co_cr_sub_partzlib8192_1_2;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from co_cr_sub_partzlib8192_1_2 t1 full outer join co_cr_sub_partzlib8192_1_2_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table co_cr_sub_partzlib8192_1_2;
--
-- Insert data again 
--
insert into co_cr_sub_partzlib8192_1_2 select * from co_cr_sub_partzlib8192_1_2_uncompr order by a1;

--
-- Compression ratio
--
--
-- Select the data: Using the JOIN as mentioned above 
--
Select count(*) as count_join from co_cr_sub_partzlib8192_1_2 t1 full outer join co_cr_sub_partzlib8192_1_2_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;

--Alter table Add Partition 
alter table co_cr_sub_partzlib8192_1_2 add partition new_p values('C') ;

--Validation with psql utility 
  \d+ co_cr_sub_partzlib8192_1_2_1_prt_new_p_2_prt_3

alter table co_cr_sub_partzlib8192_1_2 add default partition df_p ;

--Validation with psql utility 
  \d+ co_cr_sub_partzlib8192_1_2_1_prt_df_p_2_prt_2

-- Insert data 
Insert into co_cr_sub_partzlib8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,5000),'C',2011,'t','a','dfjjjjjj','2001-12-24 02:26:11','hghgh',333,'2011-10-11','Tddd','sss','1234.56',323453,4454,7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','dfdf','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','ggg','1','0',12,23) ; 


--Alter table Exchange Partition 
--Create a table to use in exchange partition 
Drop Table if exists co_cr_sub_partzlib8192_1_2_exch; 
 CREATE TABLE co_cr_sub_partzlib8192_1_2_exch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column, compresstype=zlib)  distributed randomly;
 
Drop Table if exists co_cr_sub_partzlib8192_1_2_defexch; 
 CREATE TABLE co_cr_sub_partzlib8192_1_2_defexch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column, compresstype=zlib)  distributed randomly;
 
Insert into co_cr_sub_partzlib8192_1_2_exch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1_2 where  a1=10 and a2!='C';

Insert into co_cr_sub_partzlib8192_1_2_defexch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1_2 where a1 =10 and a2!='C';

Alter table co_cr_sub_partzlib8192_1_2 alter partition p2 exchange partition FOR (RANK(1)) with table co_cr_sub_partzlib8192_1_2_exch;
\d+ co_cr_sub_partzlib8192_1_2_1_prt_p2_2_prt_2


--Alter table Split Partition 
 Alter table co_cr_sub_partzlib8192_1_2 alter partition p1 split partition FOR (RANK(4)) at(4000) into (partition splita,partition splitb) ;
\d+ co_cr_sub_partzlib8192_1_2_1_prt_p1_2_prt_splita 


Select count(*) from co_cr_sub_partzlib8192_1_2; 

--Alter table Drop Partition 
alter table co_cr_sub_partzlib8192_1_2 drop partition new_p;

-- Drop the default partition 
alter table co_cr_sub_partzlib8192_1_2 drop default partition;

--Alter table alter type of a column 
Alter table co_cr_sub_partzlib8192_1_2 Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into co_cr_sub_partzlib8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1_2 where id =10;
Select count(*) from co_cr_sub_partzlib8192_1_2; 

--Alter table drop a column 
Alter table co_cr_sub_partzlib8192_1_2 Drop column a12; 
Insert into co_cr_sub_partzlib8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1_2 where id =10;
Select count(*) from co_cr_sub_partzlib8192_1_2; 

--Alter table rename a column 
Alter table co_cr_sub_partzlib8192_1_2 Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into co_cr_sub_partzlib8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1_2 where id =10;
Select count(*) from co_cr_sub_partzlib8192_1_2; 

--Alter table add a column 
Alter table co_cr_sub_partzlib8192_1_2 Add column a12 text default 'new column'; 
--Insert data to the table, select count(*)
Insert into co_cr_sub_partzlib8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_cr_sub_partzlib8192_1_2 where id =10;
Select count(*) from co_cr_sub_partzlib8192_1_2; 

--
-- Drop table if exists
--
DROP TABLE if exists co_wt_sub_partrle_type8192_1 cascade;

DROP TABLE if exists co_wt_sub_partrle_type8192_1_uncompr cascade;

--
-- Create table
--
CREATE TABLE co_wt_sub_partrle_type8192_1 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )
 WITH (appendonly=true, orientation=column) distributed randomly  Partition by range(a1) Subpartition by list(a2) subpartition template ( default subpartition df_sp, subpartition sp1 values('M') , subpartition sp2 values('F')  
 WITH (appendonly=true, orientation=column,compresstype=rle_type,compresslevel=1,blocksize=8192)) (start(1)  end(5000) every(1000) );

-- 
-- Create Indexes
--
CREATE INDEX co_wt_sub_partrle_type8192_1_idx_bitmap ON co_wt_sub_partrle_type8192_1 USING bitmap (a1);

CREATE INDEX co_wt_sub_partrle_type8192_1_idx_btree ON co_wt_sub_partrle_type8192_1(a9);

--
-- Insert data to the table
--
 INSERT INTO co_wt_sub_partrle_type8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_wt_sub_partrle_type8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 




--Create Uncompressed table of same schema definition

CREATE TABLE co_wt_sub_partrle_type8192_1_uncompr(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly Partition by range(a1) Subpartition by list(a2) subpartition template ( subpartition sp1 values('M') , subpartition sp2 values('F') ) (start(1)  end(5000) every(1000)) ;

--
-- Insert to uncompressed table
--
 INSERT INTO co_wt_sub_partrle_type8192_1_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_wt_sub_partrle_type8192_1_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


--
-- ********Validation******* 
--
\d+ co_wt_sub_partrle_type8192_1_1_prt_1_2_prt_sp2


--Select from pg_attribute_encoding to see the table entry 
select parencattnum, parencattoptions from pg_partition_encoding e, pg_partition p, pg_class c  where c.relname = 'co_wt_sub_partrle_type8192_1' and c.oid = p.parrelid and p.oid = e.parencoid order by parencattnum limit 3; 
--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  co_wt_sub_partrle_type8192_1_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  co_wt_sub_partrle_type8192_1;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from co_wt_sub_partrle_type8192_1 t1 full outer join co_wt_sub_partrle_type8192_1_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table co_wt_sub_partrle_type8192_1;
--
-- Insert data again 
--
insert into co_wt_sub_partrle_type8192_1 select * from co_wt_sub_partrle_type8192_1_uncompr order by a1;

--
-- Compression ratio
--
--
-- Select the data: Using the JOIN as mentioned above 
--
Select count(*) as count_join from co_wt_sub_partrle_type8192_1 t1 full outer join co_wt_sub_partrle_type8192_1_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;

--Alter table Add Partition 
alter table co_wt_sub_partrle_type8192_1 add partition new_p start(5050) end (6051) WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1);

--Validation with psql utility 
  \d+ co_wt_sub_partrle_type8192_1_1_prt_new_p_2_prt_sp1

alter table co_wt_sub_partrle_type8192_1 add default partition df_p ;

--Validation with psql utility 
  \d+ co_wt_sub_partrle_type8192_1_1_prt_df_p_2_prt_sp2

-- Insert data 
Insert into co_wt_sub_partrle_type8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,5000),'C',2011,'t','a','dfjjjjjj','2001-12-24 02:26:11','hghgh',333,'2011-10-11','Tddd','sss','1234.56',323453,4454,7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','dfdf','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','ggg','1','0',12,23) ; 


--Alter table Exchange Partition 
--Create a table to use in exchange partition 
Drop Table if exists co_wt_sub_partrle_type8192_1_exch; 
 CREATE TABLE co_wt_sub_partrle_type8192_1_exch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column, compresstype=zlib)  distributed randomly;
 
Drop Table if exists co_wt_sub_partrle_type8192_1_defexch; 
 CREATE TABLE co_wt_sub_partrle_type8192_1_defexch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column, compresstype=zlib)  distributed randomly;
 
Insert into co_wt_sub_partrle_type8192_1_exch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1 where  a1=10 and a2!='C';

Insert into co_wt_sub_partrle_type8192_1_defexch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1 where a1 =10 and a2!='C';

Alter table co_wt_sub_partrle_type8192_1 alter partition FOR (RANK(1)) exchange partition sp1 with table co_wt_sub_partrle_type8192_1_exch;
\d+ co_wt_sub_partrle_type8192_1_1_prt_1_2_prt_sp1


Select count(*) from co_wt_sub_partrle_type8192_1; 

--Alter table Drop Partition 
alter table co_wt_sub_partrle_type8192_1 drop partition new_p;

-- Drop the default partition 
alter table co_wt_sub_partrle_type8192_1 drop default partition;

--Alter table alter type of a column 
Alter table co_wt_sub_partrle_type8192_1 Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into co_wt_sub_partrle_type8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1 where id =10;
Select count(*) from co_wt_sub_partrle_type8192_1; 

--Alter table drop a column 
Alter table co_wt_sub_partrle_type8192_1 Drop column a12; 
Insert into co_wt_sub_partrle_type8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1 where id =10;
Select count(*) from co_wt_sub_partrle_type8192_1; 

--Alter table rename a column 
Alter table co_wt_sub_partrle_type8192_1 Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into co_wt_sub_partrle_type8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1 where id =10;
Select count(*) from co_wt_sub_partrle_type8192_1; 

--Alter table add a column 
Alter table co_wt_sub_partrle_type8192_1 Add column a12 text default 'new column'; 
--Insert data to the table, select count(*)
Insert into co_wt_sub_partrle_type8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1 where id =10;
Select count(*) from co_wt_sub_partrle_type8192_1; 

--
-- Drop table if exists
--
DROP TABLE if exists co_wt_sub_partrle_type8192_1_2 cascade;

DROP TABLE if exists co_wt_sub_partrle_type8192_1_2_uncompr cascade;

--
-- Create table
--
CREATE TABLE co_wt_sub_partrle_type8192_1_2 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )
 WITH (appendonly=true, orientation=column) distributed randomly  Partition by list(a2) Subpartition by range(a1) subpartition template (default subpartition df_sp, start(1)  end(5000) every(1000) 
 WITH (appendonly=true, orientation=column,compresstype=rle_type,compresslevel=1,blocksize=8192)) (partition p1 values ('M'), partition p2 values ('F'));

-- 
-- Create Indexes
--
CREATE INDEX co_wt_sub_partrle_type8192_1_2_idx_bitmap ON co_wt_sub_partrle_type8192_1_2 USING bitmap (a1);

CREATE INDEX co_wt_sub_partrle_type8192_1_2_idx_btree ON co_wt_sub_partrle_type8192_1_2(a9);

--
-- Insert data to the table
--
 INSERT INTO co_wt_sub_partrle_type8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_wt_sub_partrle_type8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 



--Create Uncompressed table of same schema definition

CREATE TABLE co_wt_sub_partrle_type8192_1_2_uncompr(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly Partition by list(a2)  Subpartition by range(a1) subpartition template (start(1)  end(5000) every(1000)) (default partition p1 , partition p2 values ('M') );

--
-- Insert to uncompressed table
--
 INSERT INTO co_wt_sub_partrle_type8192_1_2_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_wt_sub_partrle_type8192_1_2_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


--
-- ********Validation******* 
--
\d+ co_wt_sub_partrle_type8192_1_2_1_prt_p1_2_prt_2 


--Select from pg_attribute_encoding to see the table entry 
select parencattnum, parencattoptions from pg_partition_encoding e, pg_partition p, pg_class c  where c.relname = 'co_wt_sub_partrle_type8192_1_2' and c.oid = p.parrelid and p.oid = e.parencoid order by parencattnum limit 3; 
--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  co_wt_sub_partrle_type8192_1_2_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  co_wt_sub_partrle_type8192_1_2;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from co_wt_sub_partrle_type8192_1_2 t1 full outer join co_wt_sub_partrle_type8192_1_2_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table co_wt_sub_partrle_type8192_1_2;
--
-- Insert data again 
--
insert into co_wt_sub_partrle_type8192_1_2 select * from co_wt_sub_partrle_type8192_1_2_uncompr order by a1;

--
-- Compression ratio
--
--
-- Select the data: Using the JOIN as mentioned above 
--
Select count(*) as count_join from co_wt_sub_partrle_type8192_1_2 t1 full outer join co_wt_sub_partrle_type8192_1_2_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;

--Alter table Add Partition 
alter table co_wt_sub_partrle_type8192_1_2 add partition new_p values('C')  WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1);

--Validation with psql utility 
  \d+ co_wt_sub_partrle_type8192_1_2_1_prt_new_p_2_prt_3

alter table co_wt_sub_partrle_type8192_1_2 add default partition df_p ;

--Validation with psql utility 
  \d+ co_wt_sub_partrle_type8192_1_2_1_prt_df_p_2_prt_2

-- Insert data 
Insert into co_wt_sub_partrle_type8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,5000),'C',2011,'t','a','dfjjjjjj','2001-12-24 02:26:11','hghgh',333,'2011-10-11','Tddd','sss','1234.56',323453,4454,7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','dfdf','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','ggg','1','0',12,23) ; 


--Alter table Exchange Partition 
--Create a table to use in exchange partition 
Drop Table if exists co_wt_sub_partrle_type8192_1_2_exch; 
 CREATE TABLE co_wt_sub_partrle_type8192_1_2_exch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column, compresstype=zlib)  distributed randomly;
 
Drop Table if exists co_wt_sub_partrle_type8192_1_2_defexch; 
 CREATE TABLE co_wt_sub_partrle_type8192_1_2_defexch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column, compresstype=zlib)  distributed randomly;
 
Insert into co_wt_sub_partrle_type8192_1_2_exch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1_2 where  a1=10 and a2!='C';

Insert into co_wt_sub_partrle_type8192_1_2_defexch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1_2 where a1 =10 and a2!='C';

Alter table co_wt_sub_partrle_type8192_1_2 alter partition p1 exchange partition FOR (RANK(1)) with table co_wt_sub_partrle_type8192_1_2_exch;
\d+ co_wt_sub_partrle_type8192_1_2_1_prt_p1_2_prt_2


--Alter table Split Partition 
 Alter table co_wt_sub_partrle_type8192_1_2 alter partition p2 split partition FOR (RANK(4)) at(4000) into (partition splita,partition splitb) ;
\d+ co_wt_sub_partrle_type8192_1_2_1_prt_p2_2_prt_splita 


Select count(*) from co_wt_sub_partrle_type8192_1_2; 

--Alter table Drop Partition 
alter table co_wt_sub_partrle_type8192_1_2 drop partition new_p;

-- Drop the default partition 
alter table co_wt_sub_partrle_type8192_1_2 drop default partition;

--Alter table alter type of a column 
Alter table co_wt_sub_partrle_type8192_1_2 Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into co_wt_sub_partrle_type8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1_2 where id =10;
Select count(*) from co_wt_sub_partrle_type8192_1_2; 

--Alter table drop a column 
Alter table co_wt_sub_partrle_type8192_1_2 Drop column a12; 
Insert into co_wt_sub_partrle_type8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1_2 where id =10;
Select count(*) from co_wt_sub_partrle_type8192_1_2; 

--Alter table rename a column 
Alter table co_wt_sub_partrle_type8192_1_2 Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into co_wt_sub_partrle_type8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1_2 where id =10;
Select count(*) from co_wt_sub_partrle_type8192_1_2; 

--Alter table add a column 
Alter table co_wt_sub_partrle_type8192_1_2 Add column a12 text default 'new column'; 
--Insert data to the table, select count(*)
Insert into co_wt_sub_partrle_type8192_1_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_wt_sub_partrle_type8192_1_2 where id =10;
Select count(*) from co_wt_sub_partrle_type8192_1_2; 

--
-- Drop table if exists
--
DROP TABLE if exists ao_wt_sub_partzlib8192_5 cascade;

DROP TABLE if exists ao_wt_sub_partzlib8192_5_uncompr cascade;

--
-- Create table
--
CREATE TABLE ao_wt_sub_partzlib8192_5 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )
 WITH (appendonly=true, orientation=row) distributed randomly  Partition by range(a1) Subpartition by list(a2) subpartition template ( default subpartition df_sp, subpartition sp1 values('M') , subpartition sp2 values('F')  
 WITH (appendonly=true, orientation=row,compresstype=zlib,compresslevel=5,blocksize=8192)) (start(1)  end(5000) every(1000) );

-- 
-- Create Indexes
--
CREATE INDEX ao_wt_sub_partzlib8192_5_idx_bitmap ON ao_wt_sub_partzlib8192_5 USING bitmap (a1);

CREATE INDEX ao_wt_sub_partzlib8192_5_idx_btree ON ao_wt_sub_partzlib8192_5(a9);

--
-- Insert data to the table
--
 INSERT INTO ao_wt_sub_partzlib8192_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO ao_wt_sub_partzlib8192_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 



--Create Uncompressed table of same schema definition

CREATE TABLE ao_wt_sub_partzlib8192_5_uncompr(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=row) distributed randomly Partition by range(a1) Subpartition by list(a2) subpartition template ( subpartition sp1 values('M') , subpartition sp2 values('F') ) (start(1)  end(5000) every(1000)) ;

--
-- Insert to uncompressed table
--
 INSERT INTO ao_wt_sub_partzlib8192_5_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO ao_wt_sub_partzlib8192_5_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


--
-- ********Validation******* 
--
\d+ ao_wt_sub_partzlib8192_5_1_prt_1_2_prt_sp2


--Select from pg_attribute_encoding to see the table entry 
select parencattnum, parencattoptions from pg_partition_encoding e, pg_partition p, pg_class c  where c.relname = 'ao_wt_sub_partzlib8192_5' and c.oid = p.parrelid and p.oid = e.parencoid order by parencattnum limit 3; 
--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  ao_wt_sub_partzlib8192_5_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  ao_wt_sub_partzlib8192_5;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from ao_wt_sub_partzlib8192_5 t1 full outer join ao_wt_sub_partzlib8192_5_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table ao_wt_sub_partzlib8192_5;
--
-- Insert data again 
--
insert into ao_wt_sub_partzlib8192_5 select * from ao_wt_sub_partzlib8192_5_uncompr order by a1;

--
-- Compression ratio
--
--
-- Select the data: Using the JOIN as mentioned above 
--
Select count(*) as count_join from ao_wt_sub_partzlib8192_5 t1 full outer join ao_wt_sub_partzlib8192_5_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;

--Alter table Add Partition 
alter table ao_wt_sub_partzlib8192_5 add partition new_p start(5050) end (6051) WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1);

--Validation with psql utility 
  \d+ ao_wt_sub_partzlib8192_5_1_prt_new_p_2_prt_sp1

alter table ao_wt_sub_partzlib8192_5 add default partition df_p ;

--Validation with psql utility 
  \d+ ao_wt_sub_partzlib8192_5_1_prt_df_p_2_prt_sp2

-- Insert data 
Insert into ao_wt_sub_partzlib8192_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,5000),'C',2011,'t','a','dfjjjjjj','2001-12-24 02:26:11','hghgh',333,'2011-10-11','Tddd','sss','1234.56',323453,4454,7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','dfdf','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','ggg','1','0',12,23) ; 


--Alter table Exchange Partition 
--Create a table to use in exchange partition 
Drop Table if exists ao_wt_sub_partzlib8192_5_exch; 
 CREATE TABLE ao_wt_sub_partzlib8192_5_exch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=row, compresstype=zlib)  distributed randomly;
 
Drop Table if exists ao_wt_sub_partzlib8192_5_defexch; 
 CREATE TABLE ao_wt_sub_partzlib8192_5_defexch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=row, compresstype=zlib)  distributed randomly;
 
Insert into ao_wt_sub_partzlib8192_5_exch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5 where  a1=10 and a2!='C';

Insert into ao_wt_sub_partzlib8192_5_defexch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5 where a1 =10 and a2!='C';

Alter table ao_wt_sub_partzlib8192_5 alter partition FOR (RANK(1)) exchange partition sp1 with table ao_wt_sub_partzlib8192_5_exch;
\d+ ao_wt_sub_partzlib8192_5_1_prt_1_2_prt_sp1


Select count(*) from ao_wt_sub_partzlib8192_5; 

--Alter table Drop Partition 
alter table ao_wt_sub_partzlib8192_5 drop partition new_p;

-- Drop the default partition 
alter table ao_wt_sub_partzlib8192_5 drop default partition;

--Alter table alter type of a column 
Alter table ao_wt_sub_partzlib8192_5 Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into ao_wt_sub_partzlib8192_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5 where id =10;
Select count(*) from ao_wt_sub_partzlib8192_5; 

--Alter table drop a column 
Alter table ao_wt_sub_partzlib8192_5 Drop column a12; 
Insert into ao_wt_sub_partzlib8192_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5 where id =10;
Select count(*) from ao_wt_sub_partzlib8192_5; 

--Alter table rename a column 
Alter table ao_wt_sub_partzlib8192_5 Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into ao_wt_sub_partzlib8192_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5 where id =10;
Select count(*) from ao_wt_sub_partzlib8192_5; 

--Alter table add a column 
Alter table ao_wt_sub_partzlib8192_5 Add column a12 text default 'new column'; 
--Insert data to the table, select count(*)
Insert into ao_wt_sub_partzlib8192_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5 where id =10;
Select count(*) from ao_wt_sub_partzlib8192_5; 

--
-- Drop table if exists
--
DROP TABLE if exists ao_wt_sub_partzlib8192_5_2 cascade;

DROP TABLE if exists ao_wt_sub_partzlib8192_5_2_uncompr cascade;

--
-- Create table
--
CREATE TABLE ao_wt_sub_partzlib8192_5_2 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )
 WITH (appendonly=true, orientation=row) distributed randomly  Partition by list(a2) Subpartition by range(a1) subpartition template (default subpartition df_sp, start(1)  end(5000) every(1000) 
 WITH (appendonly=true, orientation=row,compresstype=zlib,compresslevel=5,blocksize=8192)) (partition p1 values ('M'), partition p2 values ('F'));

-- 
-- Create Indexes
--
CREATE INDEX ao_wt_sub_partzlib8192_5_2_idx_bitmap ON ao_wt_sub_partzlib8192_5_2 USING bitmap (a1);

CREATE INDEX ao_wt_sub_partzlib8192_5_2_idx_btree ON ao_wt_sub_partzlib8192_5_2(a9);

--
-- Insert data to the table
--
 INSERT INTO ao_wt_sub_partzlib8192_5_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO ao_wt_sub_partzlib8192_5_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 



--Create Uncompressed table of same schema definition

CREATE TABLE ao_wt_sub_partzlib8192_5_2_uncompr(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=row) distributed randomly Partition by list(a2)  Subpartition by range(a1) subpartition template (start(1)  end(5000) every(1000)) (default partition p1 , partition p2 values ('M') );

--
-- Insert to uncompressed table
--
 INSERT INTO ao_wt_sub_partzlib8192_5_2_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO ao_wt_sub_partzlib8192_5_2_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


--
-- ********Validation******* 
--
\d+ ao_wt_sub_partzlib8192_5_2_1_prt_p1_2_prt_2 


--Select from pg_attribute_encoding to see the table entry 
select parencattnum, parencattoptions from pg_partition_encoding e, pg_partition p, pg_class c  where c.relname = 'ao_wt_sub_partzlib8192_5_2' and c.oid = p.parrelid and p.oid = e.parencoid order by parencattnum limit 3; 
--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  ao_wt_sub_partzlib8192_5_2_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  ao_wt_sub_partzlib8192_5_2;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from ao_wt_sub_partzlib8192_5_2 t1 full outer join ao_wt_sub_partzlib8192_5_2_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table ao_wt_sub_partzlib8192_5_2;
--
-- Insert data again 
--
insert into ao_wt_sub_partzlib8192_5_2 select * from ao_wt_sub_partzlib8192_5_2_uncompr order by a1;

--
-- Compression ratio
--
--
-- Select the data: Using the JOIN as mentioned above 
--
Select count(*) as count_join from ao_wt_sub_partzlib8192_5_2 t1 full outer join ao_wt_sub_partzlib8192_5_2_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;

--Alter table Add Partition 
alter table ao_wt_sub_partzlib8192_5_2 add partition new_p values('C')  WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1);

--Validation with psql utility 
  \d+ ao_wt_sub_partzlib8192_5_2_1_prt_new_p_2_prt_3

alter table ao_wt_sub_partzlib8192_5_2 add default partition df_p ;

--Validation with psql utility 
  \d+ ao_wt_sub_partzlib8192_5_2_1_prt_df_p_2_prt_2

-- Insert data 
Insert into ao_wt_sub_partzlib8192_5_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,5000),'C',2011,'t','a','dfjjjjjj','2001-12-24 02:26:11','hghgh',333,'2011-10-11','Tddd','sss','1234.56',323453,4454,7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','dfdf','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','ggg','1','0',12,23) ; 


--Alter table Exchange Partition 
--Create a table to use in exchange partition 
Drop Table if exists ao_wt_sub_partzlib8192_5_2_exch; 
 CREATE TABLE ao_wt_sub_partzlib8192_5_2_exch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=row, compresstype=zlib)  distributed randomly;
 
Drop Table if exists ao_wt_sub_partzlib8192_5_2_defexch; 
 CREATE TABLE ao_wt_sub_partzlib8192_5_2_defexch(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=row, compresstype=zlib)  distributed randomly;
 
Insert into ao_wt_sub_partzlib8192_5_2_exch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5_2 where  a1=10 and a2!='C';

Insert into ao_wt_sub_partzlib8192_5_2_defexch(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5_2 where a1 =10 and a2!='C';

Alter table ao_wt_sub_partzlib8192_5_2 alter partition p1 exchange partition FOR (RANK(1)) with table ao_wt_sub_partzlib8192_5_2_exch;
\d+ ao_wt_sub_partzlib8192_5_2_1_prt_p1_2_prt_2


--Alter table Split Partition 
 Alter table ao_wt_sub_partzlib8192_5_2 alter partition p2 split partition FOR (RANK(4)) at(4000) into (partition splita,partition splitb) ;
\d+ ao_wt_sub_partzlib8192_5_2_1_prt_p2_2_prt_splita 


Select count(*) from ao_wt_sub_partzlib8192_5_2; 

--Alter table Drop Partition 
alter table ao_wt_sub_partzlib8192_5_2 drop partition new_p;

-- Drop the default partition 
alter table ao_wt_sub_partzlib8192_5_2 drop default partition;

--Alter table alter type of a column 
Alter table ao_wt_sub_partzlib8192_5_2 Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into ao_wt_sub_partzlib8192_5_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5_2 where id =10;
Select count(*) from ao_wt_sub_partzlib8192_5_2; 

--Alter table drop a column 
Alter table ao_wt_sub_partzlib8192_5_2 Drop column a12; 
Insert into ao_wt_sub_partzlib8192_5_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5_2 where id =10;
Select count(*) from ao_wt_sub_partzlib8192_5_2; 

--Alter table rename a column 
Alter table ao_wt_sub_partzlib8192_5_2 Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into ao_wt_sub_partzlib8192_5_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5_2 where id =10;
Select count(*) from ao_wt_sub_partzlib8192_5_2; 

--Alter table add a column 
Alter table ao_wt_sub_partzlib8192_5_2 Add column a12 text default 'new column'; 
--Insert data to the table, select count(*)
Insert into ao_wt_sub_partzlib8192_5_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_wt_sub_partzlib8192_5_2 where id =10;
Select count(*) from ao_wt_sub_partzlib8192_5_2; 

--
-- Drop table if exists
--
DROP TABLE if exists ao_crtb_with_row_zlib_8192_1 cascade;

DROP TABLE if exists ao_crtb_with_row_zlib_8192_1_uncompr cascade;

--
-- Create table
--
CREATE TABLE ao_crtb_with_row_zlib_8192_1 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )
 WITH (appendonly=true, orientation=row,compresstype=zlib,compresslevel=1,blocksize=8192) distributed randomly;

-- 
-- Create Indexes
--
CREATE INDEX ao_crtb_with_row_zlib_8192_1_idx_bitmap ON ao_crtb_with_row_zlib_8192_1 USING bitmap (a1);

CREATE INDEX ao_crtb_with_row_zlib_8192_1_idx_btree ON ao_crtb_with_row_zlib_8192_1(a9);

--
-- Insert data to the table
--
 INSERT INTO ao_crtb_with_row_zlib_8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO ao_crtb_with_row_zlib_8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


--Create Uncompressed table of same schema definition

CREATE TABLE ao_crtb_with_row_zlib_8192_1_uncompr(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=row) distributed randomly;

--
-- Insert to uncompressed table
--
 INSERT INTO ao_crtb_with_row_zlib_8192_1_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO ao_crtb_with_row_zlib_8192_1_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


--
-- ********Validation******* 
--
\d+ ao_crtb_with_row_zlib_8192_1


--Select from pg_attribute_encoding to see the table entry 
select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'ao_crtb_with_row_zlib_8192_1' and c.oid=e.attrelid  order by relname, attnum limit 3; 
--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  ao_crtb_with_row_zlib_8192_1_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  ao_crtb_with_row_zlib_8192_1;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from ao_crtb_with_row_zlib_8192_1 t1 full outer join ao_crtb_with_row_zlib_8192_1_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table ao_crtb_with_row_zlib_8192_1;
--
-- Insert data again 
--
insert into ao_crtb_with_row_zlib_8192_1 select * from ao_crtb_with_row_zlib_8192_1_uncompr order by a1;


--
-- Select the data: Using the JOIN as mentioned above 
--
Select count(*) as count_join from ao_crtb_with_row_zlib_8192_1 t1 full outer join ao_crtb_with_row_zlib_8192_1_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--Alter table alter type of a column 
Alter table ao_crtb_with_row_zlib_8192_1 Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into ao_crtb_with_row_zlib_8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_crtb_with_row_zlib_8192_1 where id =10;
Select count(*) from ao_crtb_with_row_zlib_8192_1; 

--Alter table drop a column 
Alter table ao_crtb_with_row_zlib_8192_1 Drop column a12; 
Insert into ao_crtb_with_row_zlib_8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_crtb_with_row_zlib_8192_1 where id =10;
Select count(*) from ao_crtb_with_row_zlib_8192_1; 

--Alter table rename a column 
Alter table ao_crtb_with_row_zlib_8192_1 Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into ao_crtb_with_row_zlib_8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_crtb_with_row_zlib_8192_1 where id =10;
Select count(*) from ao_crtb_with_row_zlib_8192_1; 

--Alter table add a column 
Alter table ao_crtb_with_row_zlib_8192_1 Add column a12 text default 'new column'; 
--Insert data to the table, select count(*)
Insert into ao_crtb_with_row_zlib_8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ao_crtb_with_row_zlib_8192_1 where id =10;
Select count(*) from ao_crtb_with_row_zlib_8192_1; 

--Drop table 
DROP table ao_crtb_with_row_zlib_8192_1; 

--Create table again and insert data 
CREATE TABLE ao_crtb_with_row_zlib_8192_1 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )
 WITH (appendonly=true, orientation=row,compresstype=zlib,compresslevel=1,blocksize=8192) distributed randomly;
 INSERT INTO ao_crtb_with_row_zlib_8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO ao_crtb_with_row_zlib_8192_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 


--Alter table drop a column 
Alter table ao_crtb_with_row_zlib_8192_1 Drop column a12; 
--Create CTAS table 

 Drop table if exists ao_crtb_with_row_zlib_8192_1_ctas ;
--Create a CTAS table 
CREATE TABLE ao_crtb_with_row_zlib_8192_1_ctas  WITH (appendonly=true, orientation=column) AS Select * from ao_crtb_with_row_zlib_8192_1;

DROP type if exists int_rle_type cascade ; 

CREATE type int_rle_type;
CREATE FUNCTION int_rle_type_in(cstring) 
 RETURNS int_rle_type
 AS 'int4in' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION int_rle_type_out(int_rle_type) 
 RETURNS cstring
 AS 'int4out' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE int_rle_type( 
 input = int_rle_type_in ,
 output = int_rle_type_out ,
 internallength = 4, 
 default =55, 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);

--Drop and recreate the data type 

 Drop type if exists int_rle_type cascade;

CREATE FUNCTION int_rle_type_in(cstring) 
 RETURNS int_rle_type
 AS 'int4in' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION int_rle_type_out(int_rle_type) 
 RETURNS cstring
 AS 'int4out' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE int_rle_type( 
 input = int_rle_type_in ,
 output = int_rle_type_out ,
 internallength = 4, 
 default =55, 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);


select typoptions from pg_type_encoding where typid='int_rle_type '::regtype;

DROP type if exists char_rle_type cascade ; 

CREATE type char_rle_type;
CREATE FUNCTION char_rle_type_in(cstring) 
 RETURNS char_rle_type
 AS 'charin' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION char_rle_type_out(char_rle_type) 
 RETURNS cstring
 AS 'charout' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE char_rle_type( 
 input = char_rle_type_in ,
 output = char_rle_type_out ,
 internallength = 4, 
 default = 'asd' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);

--Drop and recreate the data type 

 Drop type if exists char_rle_type cascade;

CREATE FUNCTION char_rle_type_in(cstring) 
 RETURNS char_rle_type
 AS 'charin' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION char_rle_type_out(char_rle_type) 
 RETURNS cstring
 AS 'charout' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE char_rle_type( 
 input = char_rle_type_in ,
 output = char_rle_type_out ,
 internallength = 4, 
 default = 'asd' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);


select typoptions from pg_type_encoding where typid='char_rle_type '::regtype;

DROP type if exists text_rle_type cascade ; 

CREATE type text_rle_type;
CREATE FUNCTION text_rle_type_in(cstring) 
 RETURNS text_rle_type
 AS 'textin' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION text_rle_type_out(text_rle_type) 
 RETURNS cstring
 AS 'textout' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE text_rle_type( 
 input = text_rle_type_in ,
 output = text_rle_type_out ,
 internallength = variable, 
 default = 'hfkdshfkjsdhflkshadfkhsadflkh' , 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);

--Drop and recreate the data type 

 Drop type if exists text_rle_type cascade;

CREATE FUNCTION text_rle_type_in(cstring) 
 RETURNS text_rle_type
 AS 'textin' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION text_rle_type_out(text_rle_type) 
 RETURNS cstring
 AS 'textout' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE text_rle_type( 
 input = text_rle_type_in ,
 output = text_rle_type_out ,
 internallength = variable, 
 default = 'hfkdshfkjsdhflkshadfkhsadflkh' , 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);


select typoptions from pg_type_encoding where typid='text_rle_type '::regtype;

DROP type if exists varchar_rle_type cascade ; 

CREATE type varchar_rle_type;
CREATE FUNCTION varchar_rle_type_in(cstring) 
 RETURNS varchar_rle_type
 AS 'varcharin' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION varchar_rle_type_out(varchar_rle_type) 
 RETURNS cstring
 AS 'varcharout' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE varchar_rle_type( 
 input = varchar_rle_type_in ,
 output = varchar_rle_type_out ,
 internallength = variable, 
 default = 'ajhgdjagdjasdkjashk' , 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);

--Drop and recreate the data type 

 Drop type if exists varchar_rle_type cascade;

CREATE FUNCTION varchar_rle_type_in(cstring) 
 RETURNS varchar_rle_type
 AS 'varcharin' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION varchar_rle_type_out(varchar_rle_type) 
 RETURNS cstring
 AS 'varcharout' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE varchar_rle_type( 
 input = varchar_rle_type_in ,
 output = varchar_rle_type_out ,
 internallength = variable, 
 default = 'ajhgdjagdjasdkjashk' , 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);


select typoptions from pg_type_encoding where typid='varchar_rle_type '::regtype;

DROP type if exists date_rle_type cascade ; 

CREATE type date_rle_type;
CREATE FUNCTION date_rle_type_in(cstring) 
 RETURNS date_rle_type
 AS 'date_in' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION date_rle_type_out(date_rle_type) 
 RETURNS cstring
 AS 'date_out' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE date_rle_type( 
 input = date_rle_type_in ,
 output = date_rle_type_out ,
 internallength = 4, 
 default = '2001-12-11' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);

--Drop and recreate the data type 

 Drop type if exists date_rle_type cascade;

CREATE FUNCTION date_rle_type_in(cstring) 
 RETURNS date_rle_type
 AS 'date_in' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION date_rle_type_out(date_rle_type) 
 RETURNS cstring
 AS 'date_out' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE date_rle_type( 
 input = date_rle_type_in ,
 output = date_rle_type_out ,
 internallength = 4, 
 default = '2001-12-11' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);


select typoptions from pg_type_encoding where typid='date_rle_type '::regtype;

DROP type if exists timestamp_rle_type cascade ; 

CREATE type timestamp_rle_type;
CREATE FUNCTION timestamp_rle_type_in(cstring) 
 RETURNS timestamp_rle_type
 AS 'timestamp_in' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION timestamp_rle_type_out(timestamp_rle_type) 
 RETURNS cstring
 AS 'timestamp_out' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE timestamp_rle_type( 
 input = timestamp_rle_type_in ,
 output = timestamp_rle_type_out ,
 internallength = 4, 
 default = '2001-12-24 02:26:11' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);

--Drop and recreate the data type 

 Drop type if exists timestamp_rle_type cascade;

CREATE FUNCTION timestamp_rle_type_in(cstring) 
 RETURNS timestamp_rle_type
 AS 'timestamp_in' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION timestamp_rle_type_out(timestamp_rle_type) 
 RETURNS cstring
 AS 'timestamp_out' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE timestamp_rle_type( 
 input = timestamp_rle_type_in ,
 output = timestamp_rle_type_out ,
 internallength = 4, 
 default = '2001-12-24 02:26:11' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=8192,
 compresslevel=4);


select typoptions from pg_type_encoding where typid='timestamp_rle_type '::regtype;

DROP table if exists co_create_type_rle_type_8192_4; 
-- Create table 
CREATE TABLE co_create_type_rle_type_8192_4
	 (id serial,  a1 int_rle_type, a2 char_rle_type, a3 text_rle_type, a4 date_rle_type, a5 varchar_rle_type, a6 timestamp_rle_type ) WITH (appendonly=true, orientation=column) distributed randomly;


\d+ co_create_type_rle_type_8192_4

INSERT into co_create_type_rle_type_8192_4 DEFAULT VALUES ; 
--Select * from co_create_type_rle_type_8192_4;

Insert into co_create_type_rle_type_8192_4 select * from co_create_type_rle_type_8192_4; 
Insert into co_create_type_rle_type_8192_4 select * from co_create_type_rle_type_8192_4; 
--Insert into co_create_type_rle_type_8192_4 select * from co_create_type_rle_type_8192_4; 
--Insert into co_create_type_rle_type_8192_4 select * from co_create_type_rle_type_8192_4; 
--Insert into co_create_type_rle_type_8192_4 select * from co_create_type_rle_type_8192_4; 
--Insert into co_create_type_rle_type_8192_4 select * from co_create_type_rle_type_8192_4; 

Select * from co_create_type_rle_type_8192_4;

--Alter table drop a column 
Alter table co_create_type_rle_type_8192_4 Drop column a2; 
Insert into co_create_type_rle_type_8192_4(a1,a3,a4,a5,a6)  select a1,a3,a4,a5,a6 from co_create_type_rle_type_8192_4 ;
Select count(*) from co_create_type_rle_type_8192_4; 

--Alter table rename a column 
Alter table co_create_type_rle_type_8192_4 Rename column a3 TO after_rename_a3; 
--Insert data to the table, select count(*)
Insert into co_create_type_rle_type_8192_4(a1,after_rename_a3,a4,a5,a6)  select a1,after_rename_a3,a4,a5,a6 from co_create_type_rle_type_8192_4 ;
Select count(*) from co_create_type_rle_type_8192_4; 

Alter type int_rle_type set default encoding (compresstype=zlib,compresslevel=1);

--Add a column 
  Alter table co_create_type_rle_type_8192_4 Add column new_cl int_rle_type default '5'; 

\d+ co_create_type_rle_type_8192_4

Insert into co_create_type_rle_type_8192_4(a1,after_rename_a3,a4,a5,a6)  select a1,after_rename_a3,a4,a5,a6 from co_create_type_rle_type_8192_4 ;
Select count(*) from co_create_type_rle_type_8192_4; 

Drop table if exists mpp17012_compress_test2;
create table mpp17012_compress_test2 (
col1 character(2),
col2 int,
col3 varchar,
col4 character(44),
DEFAULT COLUMN ENCODING (COMPRESSTYPE=zlib, compresslevel=5)
)
WITH (APPENDONLY=true, ORIENTATION=column, OIDS=FALSE)
distributed by (col1);
select pg_size_pretty(pg_relation_size('mpp17012_compress_test2')),
get_ao_compression_ratio('mpp17012_compress_test2');
\d+ mpp17012_compress_test2
insert into mpp17012_compress_test2 values('a',generate_series(1,250),'ksjdhfksdhfksdhfksjhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh','bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb');
select pg_size_pretty(pg_relation_size('mpp17012_compress_test2')),
get_ao_compression_ratio('mpp17012_compress_test2');

-- compute compression ratio on empty aorow table
create table empty_aorow_table (c int) with (appendonly=true) distributed by (c);
select get_ao_compression_ratio('empty_aorow_table');
