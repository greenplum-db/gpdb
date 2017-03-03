--Coverage for all the table and column constraints.
--Drop tables
Drop table if exists tbl_unique_constraint;
Drop table if exists tbl_unique_constraint2;
Drop table if exists tbl_primry_constraint;
Drop table if exists tbl_primry_constraint2;
Drop table if exists tbl_check_constraint;
Drop table if exists col_unique_constraint;
Drop table if exists col_primry_constraint;
Drop table if exists col_check_constraint;


--Create table with table constraint -Unique

CREATE table tbl_unique_constraint (i int, t text, constraint tbl_unq1 unique(i)) distributed by (i);

INSERT into tbl_unique_constraint  values (100,'text1');
INSERT into tbl_unique_constraint  values (200,'text2');
INSERT into tbl_unique_constraint  values (300,'text3');

--Create table with table constraint -Primary key

CREATE table tbl_primry_constraint (i int, t text, constraint tbl_primary1 primary key (i)) distributed by (i);

INSERT into tbl_primry_constraint  values (100,'text1');
INSERT into tbl_primry_constraint  values (200,'text2');
INSERT into tbl_primry_constraint  values (300,'text3');


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
