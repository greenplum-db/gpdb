-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description  Defining Multi-level Partitions

create database partition_db;
\c partition_db

 
CREATE TABLE sales (trans_id int, date date, amount 
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

insert into sales values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'usa');

insert into sales values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'asia');

insert into sales values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'europe');


insert into sales values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'usa');

insert into sales values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'asia');

insert into sales values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'europe');



-- Partitioning an Existing Table

CREATE TABLE sales1 (LIKE sales) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month') );

INSERT INTO sales1 SELECT * FROM sales; 

-- Verifying Your Partition Strategy 

EXPLAIN SELECT * FROM sales WHERE date='01-07-08' AND 
region='usa';

--Viewing Your Partition Design 

SELECT partitionboundary, partitiontablename, partitionname, 
partitionlevel, partitionrank FROM pg_partitions WHERE 
tablename='sales'; 

-- Adding a New Partition 
ALTER TABLE sales ALTER PARTITION FOR (RANK(12)) 
      ADD PARTITION africa VALUES ('africa');

-- Adding a Default Partition

ALTER TABLE sales ADD DEFAULT PARTITION other;

insert into sales values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'usa');

insert into sales values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'asia');

insert into sales values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'europe');

-- Renaming a Partition
--ALTER TABLE sales RENAME PARTITION FOR ('2008-01-01') TO jan08;
ALTER TABLE sales RENAME PARTITION FOR (RANK(1)) TO jan08;


-- Dropping a Partition

ALTER TABLE sales DROP PARTITION FOR (RANK(1));

-- Truncating a Partition

ALTER TABLE sales TRUNCATE PARTITION FOR (RANK(1));

-- Exchanging a Partition

CREATE TABLE sales2 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month') ); 

insert into sales2 values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'usa');

insert into sales2 values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'asia');

insert into sales2 values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'europe');

CREATE TABLE jan08 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) ;

ALTER TABLE sales2 EXCHANGE PARTITION FOR ('2008-01-01') WITH 
TABLE jan08;


-- Splitting a Partition
CREATE TABLE sales3 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month') ); 

ALTER TABLE sales3 SPLIT PARTITION FOR ('2008-01-01') 
AT ('2008-01-16') 
INTO (PARTITION jan081to15, PARTITION jan0816to31);

ALTER TABLE sales3 ADD DEFAULT PARTITION other;

ALTER TABLE sales3 SPLIT DEFAULT PARTITION 
START ('2009-01-01') INCLUSIVE 
END ('2009-02-01') EXCLUSIVE 
INTO (PARTITION jan09, PARTITION other);


---------------------
--AO Partitions
---------------------

CREATE TABLE sales_ao (trans_id int, date date, amount 
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
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

insert into sales_ao values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'usa');

insert into sales_ao values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'asia');

insert into sales_ao values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'europe');


insert into sales_ao values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'usa');

insert into sales_ao values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'asia');

insert into sales_ao values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'europe');



-- Partitioning an Existing Table

CREATE TABLE sales_ao1 (LIKE sales_ao) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5));

INSERT INTO sales_ao1 SELECT * FROM sales_ao; 

-- Verifying Your Partition Strategy 

EXPLAIN SELECT * FROM sales_ao WHERE date='01-07-08' AND 
region='usa';

--Viewing Your Partition Design 

SELECT partitionboundary, partitiontablename, partitionname, 
partitionlevel, partitionrank FROM pg_partitions WHERE 
tablename='sales_ao'; 

-- Adding a New Partition 
ALTER TABLE sales_ao ALTER PARTITION FOR (RANK(12)) 
      ADD PARTITION africa VALUES ('africa');

-- Adding a Default Partition

ALTER TABLE sales_ao ADD DEFAULT PARTITION other;

insert into sales_ao values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'usa');

insert into sales_ao values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'asia');

insert into sales_ao values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'europe');

-- Renaming a Partition
--ALTER TABLE sales_ao RENAME PARTITION FOR ('2008-01-01') TO jan08;
ALTER TABLE sales_ao RENAME PARTITION FOR (RANK(1)) TO jan08;


-- Dropping a Partition

ALTER TABLE sales_ao DROP PARTITION FOR (RANK(1));

-- Truncating a Partition

ALTER TABLE sales_ao TRUNCATE PARTITION FOR (RANK(1));

-- Exchanging a Partition

CREATE TABLE sales_ao2 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

insert into sales_ao2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'usa');

insert into sales_ao2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'asia');

insert into sales_ao2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'europe');

ALTER TABLE sales_ao2 EXCHANGE PARTITION FOR ('2008-01-01') WITH 
TABLE jan08;


-- Splitting a Partition
CREATE TABLE sales_ao3 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

ALTER TABLE sales_ao3 SPLIT PARTITION FOR ('2008-01-01') 
AT ('2008-01-16') 
INTO (PARTITION jan081to15, PARTITION jan0816to31);

ALTER TABLE sales_ao3 ADD DEFAULT PARTITION other;

ALTER TABLE sales_ao3 SPLIT DEFAULT PARTITION 
START ('2009-01-01') INCLUSIVE 
END ('2009-02-01') EXCLUSIVE 
INTO (PARTITION jan09, PARTITION other);

-------------------------
---Hybrid partitions
-------------------------

CREATE TABLE sales_hybrid (trans_id int, date date, amount 
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
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

insert into sales_hybrid values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'usa');

insert into sales_hybrid values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'asia');

insert into sales_hybrid values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'europe');


insert into sales_hybrid values(generate_series(1001,2000),'2008-06-01',generate_series(1,1000), 'usa');

insert into sales_hybrid values(generate_series(1001,2000),'2008-06-01',generate_series(1,1000), 'asia');

insert into sales_hybrid values(generate_series(1001,2000),'2008-06-01',generate_series(1,1000), 'europe');



-- Partitioning an Existing Table

CREATE TABLE sales_hybrid1 (LIKE sales_hybrid) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month'));

INSERT INTO sales_hybrid1 SELECT * FROM sales_hybrid; 

-- Verifying Your Partition Strategy 

EXPLAIN SELECT * FROM sales_hybrid WHERE date='01-07-08' AND 
region='usa';

--Viewing Your Partition Design 

SELECT partitionboundary, partitiontablename, partitionname, 
partitionlevel, partitionrank FROM pg_partitions WHERE 
tablename='sales_hybrid'; 

-- Adding a New Partition 
ALTER TABLE sales_hybrid ALTER PARTITION FOR (RANK(12)) 
      ADD PARTITION africa VALUES ('africa');

-- Adding a Default Partition

ALTER TABLE sales_hybrid ADD DEFAULT PARTITION other;

insert into sales_hybrid values(generate_series(1,1000),'2009-01-01',generate_series(1,1000), 'usa');

insert into sales_hybrid values(generate_series(1,1000),'2009-01-01',generate_series(1,1000), 'asia');

insert into sales_hybrid values(generate_series(1,1000),'2009-01-01',generate_series(1,1000), 'europe');

-- Renaming a Partition
--ALTER TABLE sales_hybrid RENAME PARTITION FOR ('2008-01-01') TO jan08;
ALTER TABLE sales_hybrid RENAME PARTITION FOR (RANK(1)) TO jan08;


-- Dropping a Partition

ALTER TABLE sales_hybrid DROP PARTITION FOR (RANK(1));

-- Truncating a Partition

ALTER TABLE sales_hybrid TRUNCATE PARTITION FOR (RANK(1));

-- Exchanging a Partition

CREATE TABLE sales_hybrid2 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

insert into sales_hybrid2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'usa');

insert into sales_hybrid2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'asia');

insert into sales_hybrid2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'europe');

ALTER TABLE sales_hybrid2 EXCHANGE PARTITION FOR ('2008-01-01') WITH 
TABLE jan08;


-- Splitting a Partition
CREATE TABLE sales_hybrid3 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

ALTER TABLE sales_hybrid3 SPLIT PARTITION FOR ('2008-01-01') 
AT ('2008-01-16') 
INTO (PARTITION jan081to15, PARTITION jan0816to31);

ALTER TABLE sales_hybrid3 ADD DEFAULT PARTITION other;

ALTER TABLE sales_hybrid3 SPLIT DEFAULT PARTITION 
START ('2009-01-01') INCLUSIVE 
END ('2009-02-01') EXCLUSIVE 
INTO (PARTITION jan09, PARTITION other);
