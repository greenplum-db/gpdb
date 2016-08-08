-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- Create table
--
CREATE TABLE resync_co_compr_quicklz_1(id SERIAL,
	 a1 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a2 char(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a3 numeric ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a4 boolean DEFAULT false  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a5 char DEFAULT 'd' ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a6 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a7 timestamp ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a8 character varying(705) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a9 bigint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a10 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a11 varchar(600) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a12 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a13 decimal ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a14 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a15 bigint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a16 int4  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a17 bytea ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a18 timestamp with time zone ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a19 timetz ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a20 path ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a21 box ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a22 macaddr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a23 interval ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a24 character varying(800) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a25 lseg ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a26 point ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a27 double precision ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a28 circle ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a29 int4 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a30 numeric(8) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a31 polygon ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a32 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a33 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a34 money ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a35 cidr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a36 inet ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a37 time ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a38 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a39 bit ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a40 bit varying(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a41 smallint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a42 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)) WITH (appendonly=true, orientation=column) distributed randomly;

-- 
-- Create Indexes
--
CREATE INDEX resync_co_compr_quicklz_1_idx_bitmap ON resync_co_compr_quicklz_1 USING bitmap (a1);

CREATE INDEX resync_co_compr_quicklz_1_idx_btree ON resync_co_compr_quicklz_1(a9);

--
-- Insert data to the table
--
 INSERT INTO resync_co_compr_quicklz_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23);
 
 
 
 --
-- Create table
--
CREATE TABLE resync_co_compr_quicklz_2(id SERIAL,
	 a1 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a2 char(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a3 numeric ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a4 boolean DEFAULT false  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a5 char DEFAULT 'd' ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a6 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a7 timestamp ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a8 character varying(705) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a9 bigint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a10 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a11 varchar(600) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a12 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a13 decimal ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a14 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a15 bigint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a16 int4  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a17 bytea ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a18 timestamp with time zone ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a19 timetz ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a20 path ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a21 box ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a22 macaddr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a23 interval ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a24 character varying(800) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a25 lseg ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a26 point ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a27 double precision ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a28 circle ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a29 int4 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a30 numeric(8) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a31 polygon ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a32 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a33 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a34 money ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a35 cidr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a36 inet ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a37 time ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a38 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a39 bit ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a40 bit varying(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a41 smallint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a42 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)) WITH (appendonly=true, orientation=column) distributed randomly;

-- 
-- Create Indexes
--
CREATE INDEX resync_co_compr_quicklz_2_idx_bitmap ON resync_co_compr_quicklz_2 USING bitmap (a1);

CREATE INDEX resync_co_compr_quicklz_2_idx_btree ON resync_co_compr_quicklz_2(a9);

--
-- Insert data to the table
--
 INSERT INTO resync_co_compr_quicklz_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 
  
  

--
-- Create table
--
CREATE TABLE resync_co_compr_quicklz_3(id SERIAL,
	 a1 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a2 char(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a3 numeric ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a4 boolean DEFAULT false  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a5 char DEFAULT 'd' ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a6 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a7 timestamp ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a8 character varying(705) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a9 bigint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a10 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a11 varchar(600) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a12 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a13 decimal ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a14 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a15 bigint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a16 int4  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a17 bytea ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a18 timestamp with time zone ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a19 timetz ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a20 path ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a21 box ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a22 macaddr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a23 interval ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a24 character varying(800) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a25 lseg ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a26 point ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a27 double precision ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a28 circle ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a29 int4 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a30 numeric(8) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a31 polygon ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a32 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a33 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a34 money ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a35 cidr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a36 inet ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a37 time ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a38 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a39 bit ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a40 bit varying(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a41 smallint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a42 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)) WITH (appendonly=true, orientation=column) distributed randomly;

-- 
-- Create Indexes
--
CREATE INDEX resync_co_compr_quicklz_3_idx_bitmap ON resync_co_compr_quicklz_3 USING bitmap (a1);

CREATE INDEX resync_co_compr_quicklz_3_idx_btree ON resync_co_compr_quicklz_1(a9);

--
-- Insert data to the table
--
 INSERT INTO resync_co_compr_quicklz_3(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 
    
 
    
--sync1 table

--Alter table alter type of a column 

Alter table sync1_co_compr_quicklz_6 Alter column a3 TYPE int4; 

--Insert data to the table, select count(*)
Insert into sync1_co_compr_quicklz_6(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from sync1_co_compr_quicklz_6 where id =10;

Select count(*) from sync1_co_compr_quicklz_6; 

--Alter table drop a column 

Alter table sync1_co_compr_quicklz_6 Drop column a12; 

Insert into sync1_co_compr_quicklz_6(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from sync1_co_compr_quicklz_6 where id =10;

Select count(*) from sync1_co_compr_quicklz_6; 

--Alter table rename a column 

Alter table sync1_co_compr_quicklz_6 Rename column a13 TO after_rename_a13;
 
--Insert data to the table, select count(*)

Insert into sync1_co_compr_quicklz_6(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from sync1_co_compr_quicklz_6 where id =10;

Select count(*) from sync1_co_compr_quicklz_6; 


--Alter table add a column with encoding

Alter table sync1_co_compr_quicklz_6 Add column a12_new text default 'newly added column' encoding(compresstype=quicklz);

Insert into sync1_co_compr_quicklz_6(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from sync1_co_compr_quicklz_6 where id =10;

Select count(*) from sync1_co_compr_quicklz_6;


--Alter type, alter table add column with the altered type

Alter type int4 set default encoding (compresstype=quicklz,compresslevel=1);
Alter table sync1_co_compr_quicklz_6 Add column int_new int4 default 120;

Insert into sync1_co_compr_quicklz_6(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from sync1_co_compr_quicklz_6 where id =10;

Select count(*) from sync1_co_compr_quicklz_6;

--Alter the encoding of type int4 back to compress None
Alter type int4 set default encoding (compresstype=None,compresslevel=0);

--Create CTAS table
CREATE TABLE  sync1_ctas_compr_quicklz_6  with ( appendonly='true', orientation='column') AS SELECT * FROM sync1_co_compr_quicklz_6 distributed randomly;

--Drop table 
DROP table sync1_co_compr_quicklz_6;              


--ck_sync1 table

--Alter table alter type of a column 

Alter table ck_sync1_co_compr_quicklz_5 Alter column a3 TYPE int4; 

--Insert data to the table, select count(*)
Insert into ck_sync1_co_compr_quicklz_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ck_sync1_co_compr_quicklz_5 where id =10;

Select count(*) from ck_sync1_co_compr_quicklz_5; 

--Alter table drop a column 

Alter table ck_sync1_co_compr_quicklz_5 Drop column a12; 

Insert into ck_sync1_co_compr_quicklz_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ck_sync1_co_compr_quicklz_5 where id =10;

Select count(*) from ck_sync1_co_compr_quicklz_5; 

--Alter table rename a column 

Alter table ck_sync1_co_compr_quicklz_5 Rename column a13 TO after_rename_a13;
 
--Insert data to the table, select count(*)

Insert into ck_sync1_co_compr_quicklz_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ck_sync1_co_compr_quicklz_5 where id =10;

Select count(*) from ck_sync1_co_compr_quicklz_5; 


--Alter table add a column with encoding

Alter table ck_sync1_co_compr_quicklz_5 Add column a12_new text default 'newly added column' encoding(compresstype=quicklz);

Insert into ck_sync1_co_compr_quicklz_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ck_sync1_co_compr_quicklz_5 where id =10;

Select count(*) from ck_sync1_co_compr_quicklz_5;


--Alter type, alter table add column with the altered type

Alter type int4 set default encoding (compresstype=quicklz,compresslevel=1);
Alter table ck_sync1_co_compr_quicklz_5 Add column int_new int4 default 120;

Insert into ck_sync1_co_compr_quicklz_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ck_sync1_co_compr_quicklz_5 where id =10;

Select count(*) from ck_sync1_co_compr_quicklz_5;

--Alter the encoding of type int4 back to compress None
Alter type int4 set default encoding (compresstype=None,compresslevel=0);

--Create CTAS table
CREATE TABLE  ck_sync1_ctas_compr_quicklz_5  with ( appendonly='true', orientation='column') AS SELECT * FROM ck_sync1_co_compr_quicklz_5 distributed randomly;

--Drop table 
DROP table ck_sync1_co_compr_quicklz_5;    


--ct table

--Alter table alter type of a column 

Alter table ct_co_compr_quicklz_3 Alter column a3 TYPE int4; 

--Insert data to the table, select count(*)
Insert into ct_co_compr_quicklz_3(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ct_co_compr_quicklz_3 where id =10;

Select count(*) from ct_co_compr_quicklz_3; 

--Alter table drop a column 

Alter table ct_co_compr_quicklz_3 Drop column a12; 

Insert into ct_co_compr_quicklz_3(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ct_co_compr_quicklz_3 where id =10;

Select count(*) from ct_co_compr_quicklz_3; 

--Alter table rename a column 

Alter table ct_co_compr_quicklz_3 Rename column a13 TO after_rename_a13;
 
--Insert data to the table, select count(*)

Insert into ct_co_compr_quicklz_3(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ct_co_compr_quicklz_3 where id =10;

Select count(*) from ct_co_compr_quicklz_3; 


--Alter table add a column with encoding

Alter table ct_co_compr_quicklz_3 Add column a12_new text default 'newly added column' encoding(compresstype=quicklz);

Insert into ct_co_compr_quicklz_3(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ct_co_compr_quicklz_3 where id =10;

Select count(*) from ct_co_compr_quicklz_3;


--Alter type, alter table add column with the altered type

Alter type int4 set default encoding (compresstype=quicklz,compresslevel=1);
Alter table ct_co_compr_quicklz_3 Add column int_new int4 default 120;

Insert into ct_co_compr_quicklz_3(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from ct_co_compr_quicklz_3 where id =10;

Select count(*) from ct_co_compr_quicklz_3;

--Alter the encoding of type int4 back to compress None
Alter type int4 set default encoding (compresstype=None,compresslevel=0);

--Create CTAS table
CREATE TABLE  ct_ctas_compr_quicklz_3  with ( appendonly='true', orientation='column') AS SELECT * FROM ct_co_compr_quicklz_3 distributed randomly;

--Drop table 
DROP table ct_co_compr_quicklz_3;       


--resync table

--Alter table alter type of a column 

Alter table resync_co_compr_quicklz_1 Alter column a3 TYPE int4; 

--Insert data to the table, select count(*)
Insert into resync_co_compr_quicklz_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from resync_co_compr_quicklz_1 where id =10;

Select count(*) from resync_co_compr_quicklz_1; 

--Alter table drop a column 

Alter table resync_co_compr_quicklz_1 Drop column a12; 

Insert into resync_co_compr_quicklz_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from resync_co_compr_quicklz_1 where id =10;

Select count(*) from resync_co_compr_quicklz_1; 

--Alter table rename a column 

Alter table resync_co_compr_quicklz_1 Rename column a13 TO after_rename_a13;
 
--Insert data to the table, select count(*)

Insert into resync_co_compr_quicklz_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from resync_co_compr_quicklz_1 where id =10;

Select count(*) from resync_co_compr_quicklz_1; 


--Alter table add a column with encoding

Alter table resync_co_compr_quicklz_1 Add column a12_new text default 'newly added column' encoding(compresstype=quicklz);

Insert into resync_co_compr_quicklz_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from resync_co_compr_quicklz_1 where id =10;

Select count(*) from resync_co_compr_quicklz_1;


--Alter type, alter table add column with the altered type

Alter type int4 set default encoding (compresstype=quicklz,compresslevel=1);
Alter table resync_co_compr_quicklz_1 Add column int_new int4 default 120;

Insert into resync_co_compr_quicklz_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from resync_co_compr_quicklz_1 where id =10;

Select count(*) from resync_co_compr_quicklz_1;

--Alter the encoding of type int4 back to compress None
Alter type int4 set default encoding (compresstype=None,compresslevel=0);

--Create CTAS table
CREATE TABLE  resync_ctas_compr_quicklz_1  with ( appendonly='true', orientation='column') AS SELECT * FROM resync_co_compr_quicklz_1 distributed randomly;

--Drop table 
DROP table resync_co_compr_quicklz_1;
     
