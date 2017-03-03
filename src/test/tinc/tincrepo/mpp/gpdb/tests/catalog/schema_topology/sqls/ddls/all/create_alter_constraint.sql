-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Coverage for all the table and column constraints

--Drop tables
Drop table if exists tbl_unique_constraint;
Drop table if exists tbl_unique_constraint2;
Drop table if exists tbl_primry_constraint;
Drop table if exists tbl_primry_constraint2;
Drop table if exists tbl_check_constraint;
Drop table if exists col_unique_constraint;
Drop table if exists col_primry_constraint;
Drop table if exists col_check_constraint;
Drop table if exists dim_site_experiment;
Drop table if exists tbl_samename_constraint;

--Create table with table constraint -Unique

CREATE table tbl_unique_constraint (i int, t text, constraint tbl_unq1 unique(i)) distributed by (i);

INSERT into tbl_unique_constraint  values (100,'text1');
INSERT into tbl_unique_constraint  values (200,'text2');
INSERT into tbl_unique_constraint  values (300,'text3');

--Alter table to add/drop a table constraint -Unique

CREATE table tbl_unique_constraint2 (i int , t text) distributed by (i);

INSERT into tbl_unique_constraint2  values (100,'text1');
INSERT into tbl_unique_constraint2  values (200,'text2');
INSERT into tbl_unique_constraint2  values (300,'text3');

ALTER table tbl_unique_constraint2 add constraint tbl_unq2 unique(i);

ALTER TABLE tbl_unique_constraint2 DROP CONSTRAINT tbl_unq2;


--Create table with table constraint -Primary key

CREATE table tbl_primry_constraint (i int, t text, constraint tbl_primary1 primary key (i)) distributed by (i);

INSERT into tbl_primry_constraint  values (100,'text1');
INSERT into tbl_primry_constraint  values (200,'text2');
INSERT into tbl_primry_constraint  values (300,'text3');

--Alter table to add/drop a table constraint -Primary key

CREATE table tbl_primry_constraint2 (i int, t text) distributed by (i);

INSERT into tbl_primry_constraint2  values (100,'text1');
INSERT into tbl_primry_constraint2  values (200,'text2');
INSERT into tbl_primry_constraint2  values (300,'text3');

ALTER table tbl_primry_constraint2 add constraint tbl_primary2 primary key(i);

ALTER TABLE tbl_primry_constraint DROP CONSTRAINT tbl_primary1;

--Create table with table constraint -Check

CREATE TABLE tbl_check_constraint  (
    a1 integer,
    a2 text,
    a3 varchar(10),
    CONSTRAINT tbl_chk_con1 CHECK (a1 > 25 AND a2 <> '')
    )DISTRIBUTED RANDOMLY;

INSERT into tbl_check_constraint  values (100,'text1');
INSERT into tbl_check_constraint  values (200,'text2');
INSERT into tbl_check_constraint  values (300,'text3');

--Alter table to add/drop a table constraint -Check

ALTER TABLE tbl_check_constraint ADD CONSTRAINT zipchk CHECK (char_length(a3) = 5);

ALTER TABLE tbl_check_constraint DROP CONSTRAINT tbl_chk_con1;


--Create table with column constraint -Unique

CREATE table col_unique_constraint (i int  constraint col_unique1 unique, t text) distributed by (i);

INSERT into col_unique_constraint  values (100,'text1');
INSERT into col_unique_constraint  values (200,'text2');
INSERT into col_unique_constraint  values (300,'text3');

--Create table with column constraint -Primary Key

CREATE table col_primry_constraint (i int constraint col_primary1 primary key, t text) distributed by (i);

INSERT into col_primry_constraint  values (100,'text1');
INSERT into col_primry_constraint  values (200,'text2');
INSERT into col_primry_constraint  values (300,'text3');


--Create table with column constraint -Check

CREATE TABLE col_check_constraint  (
    did integer,
    name varchar(40) NOT NULL constraint col_chk1 CHECK (name <> '')
    )DISTRIBUTED RANDOMLY;
    
INSERT into col_check_constraint  values (100,'text1');
INSERT into col_check_constraint  values (200,'text2');
INSERT into col_check_constraint  values (300,'text3');

-- Negative tests for duplicate constriants MPP20466
-- #1Alter table to add constriant with the same key

CREATE TABLE dim_site_experiment
(
  site_experiment_dim_key integer NOT NULL , -- Primary key.
  site_experiment_name character varying(100) NOT NULL,
  site_experiment_start_date timestamp(0) without time zone NOT NULL, -- Starting date of the experiment.
  site_experiment_end_date timestamp(0) without time zone, -- End date of the experiment.
  site_experiment_prime_num integer NOT NULL, -- Prime number to determine variation eligibility.
  CONSTRAINT dim_site_experiment_site_experiment_prime_num_key UNIQUE (site_experiment_prime_num, site_experiment_start_date)
)
WITH (
  OIDS=FALSE
)
DISTRIBUTED BY (site_experiment_prime_num, site_experiment_start_date);

ALTER TABLE dim_site_experiment
ADD CONSTRAINT dim_site_experiment_site_experiment_prime_num_key
UNIQUE(site_experiment_prime_num, site_experiment_start_date,site_experiment_dim_key);

-- #2 Alter table to add partition with new name
ALTER TABLE dim_site_experiment ADD CONSTRAINT dim_site_experiment_site_experiment_dim_key CHECK (site_experiment_dim_key < 25);


-- #3 Create a table with two constriants of same name
CREATE table tbl_samename_constraint (i int constraint tbl_name1 unique, t text, constraint tbl_name1 check (char_length(t) = 5)) distributed by (i);

