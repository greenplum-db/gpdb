\c gptest;

-- @description : Create heap tables , with indexes, with and without compression
-- @author : Divya Sivanandan

-- Create heap tables 
--start_ignore
DROP TABLE if exists sto_heap1;
--end_ignore
CREATE TABLE sto_heap1(
          col_text text,
          col_numeric numeric,
          col_unq int
          ) DISTRIBUTED by(col_unq);

insert into sto_heap1 values ('0_zero',0, 0);
insert into sto_heap1 values ('1_one',1, 1);

--start_ignore
Drop table if exists sto_heap2;
--end_ignore
Create table sto_heap2 (id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int ) distributed randomly;

INSERT INTO sto_heap2 (a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,4),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(5,7),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(4,6),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23);

-- Create table with constriants
--start_ignore
Drop table if exists sto_heap3;
--end_ignore
CREATE TABLE sto_heap3(
          col_text text DEFAULT 'text',
          col_numeric numeric
          CONSTRAINT tbl_chk_con1 CHECK (col_numeric < 250)
          )  DISTRIBUTED by(col_text);

insert into sto_heap3 values ('0_zero',30);
insert into sto_heap3 values ('1_one',10);
insert into sto_heap3 values ('2_two',25);

select count(*) from sto_heap3;

--start_ignore
Drop table if exists sto_heap4;
--end_ignore
CREATE TABLE sto_heap4  (did integer,
    name varchar(40), 
    CONSTRAINT con1 CHECK (did > 99 AND name <> '') 
    )DISTRIBUTED RANDOMLY;

insert into sto_heap4  values (100,'name_1');
insert into sto_heap4  values (200,'name_2');
insert into sto_heap4  values (300,'name_3');

--Create table in user created scehma
--start_ignore
Drop table if exists heapschema1.sto_heap5;
Drop schema heapschema1 cascade;
--end_ignore
Create schema heapschema1;

CREATE TABLE heapschema1.sto_heap5(
          stud_id int,
          stud_name varchar(20)
          )  DISTRIBUTED by(stud_id);

Insert into heapschema1.sto_heap5 values(generate_series(1,20), 'studentname');

-- Truncate, Drop table and index

--start_ignore
Drop table if exists sto_heap6;
--end_ignore
CREATE TABLE sto_heap6  (did integer,
    name varchar(40),
    CONSTRAINT con1 CHECK (did > 99 AND name <> '')
    )DISTRIBUTED RANDOMLY;

Create index heap6_idx on sto_heap6(did);

insert into sto_heap6  values (100,'name_1');
insert into sto_heap6  values (200,'name_2');
