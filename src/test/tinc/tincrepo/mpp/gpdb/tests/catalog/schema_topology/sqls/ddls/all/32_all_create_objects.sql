-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description All create objects.

create database create_objects_db;
\c create_objects_db


--Create Table

--with uppercase
    CREATE TABLE "TABLE_NAME"( col_text text, col_numeric numeric ) DISTRIBUTED RANDOMLY;
   
    insert into "TABLE_NAME" values ('0_zero',0);
    insert into "TABLE_NAME" values ('1_one',1);
    insert into "TABLE_NAME" values ('2_two',2);
    insert into "TABLE_NAME" select i||'_'||repeat('text',100),i from generate_series(1,100)i;

--with lowercase
    CREATE TABLE table_name( col_text text, col_numeric numeric ) DISTRIBUTED RANDOMLY;

    insert into table_name values ('0_zero',0);
    insert into table_name values ('1_one',1);
    insert into table_name values ('2_two',2);
    insert into table_name select i||'_'||repeat('text',100),i from generate_series(1,100)i;

--with mixedcase
    CREATE TABLE "TABLE_name"( col_text text, col_numeric numeric ) DISTRIBUTED RANDOMLY;

    insert into "TABLE_name" values ('0_zero',0);
    insert into "TABLE_name" values ('1_one',1);
    insert into "TABLE_name" values ('2_two',2);
    insert into "TABLE_name" select i||'_'||repeat('text',100),i from generate_series(1,100)i;

--with numbers 
    CREATE TABLE table_123( col_text text, col_numeric numeric ) DISTRIBUTED RANDOMLY;

    insert into table_123 values ('0_zero',0);
    insert into table_123 values ('1_one',1);
    insert into table_123 values ('2_two',2);
    insert into table_123 select i||'_'||repeat('text',100),i from generate_series(1,100)i;

--Create Sequence

--with uppercase
    CREATE SEQUENCE "SERIAL" START 101;

--with lowercase
    CREATE SEQUENCE serialone START 101;

--with mixedcase
    CREATE SEQUENCE "Serial_123" START 101;

--with numbers
    CREATE SEQUENCE serial123 START 101;


--Create indexes

    create table empl1(id integer,empname varchar,sal integer) distributed randomly;
    insert into empl1 values (50001,'mohit',1000);
    insert into empl1 values (50002,'lalit',2000);
    insert into empl1 select i,i||'_'||repeat('text',100),i from generate_series(1,100)i;

    create index empl_idx ON empl1(id) ;
    select count(*) from empl1;
    --drop table empl1;

--REINDEX bitmap index : Note: create bitmap index then vacuum leads to corrupted bitmap

    create table bm_test (i int, j int) distributed randomly;
    insert into bm_test values (0, 0), (0, 0), (0, 1), (1,0), (1,0), (1,1);
    create index bm_test_j on bm_test using bitmap(j);
    delete from bm_test where j =1;
    insert into bm_test values (0, 0), (1,0);
    insert into bm_test select i,i from generate_series(2,100)i;
    REINDEX index "public"."bm_test_j";
    --drop index  bm_test_j ;

--CLUSTER clusterindex on table

    create table table2 (col1 int,col2 int) distributed randomly;
    insert into table2 values (1,1);
    insert into table2 values (2,2);
    insert into table2 values (3,3);
    insert into table2 values (4,4);
    insert into table2 values (5,5);
    insert into table2 select i,i from generate_series(6,100)i;
    create index clusterindex on table2(col1);
    CLUSTER clusterindex on table2;
    --drop index clusterindex;


--Create View

    create table employees1(id integer,empname varchar,sal integer) distributed randomly;
    insert into employees1 values (1,'mohit',1000);
    insert into employees1 values (2,'lalit',2000);
    insert into employees1 select i,i||'_'||repeat('text',100),i from generate_series(3,100)i;

    select count(*) from employees1;
    create view empvw1 as select * from employees1; drop view empvw1;
    --drop table employees1;

--Create Language

    CREATE LANGUAGE plpgsql;
    --DROP LANGUAGE plperl;

--Create Function

    CREATE FUNCTION add(integer,integer) RETURNS integer AS 'select $1 + $2;'
    LANGUAGE SQL IMMUTABLE
    RETURNS NULL ON NULL INPUT;
    --DROP FUNCTION add(integer,integer);

--Create Group

    create GROUP prachgrp; 
    --drop GROUP prachgrp;

--Create Domain

    create DOMAIN country_code1 char(2) NOT NULL;
    create TABLE countrylist1(id integer,country text)distributed randomly;
   --drop DOMAIN country_code1 CASCADE;
   --drop TABLE countrylist1;

--Create Rule

create table foo_ao (a int) with (appendonly=true);
create table bar_ao (a int);

--try and trick by setting rules
create rule one as on insert to bar_ao do instead update foo_ao set a=1;
create rule two as on insert to bar_ao do instead delete from foo_ao where a=1;

--Create User

--with uppercase

    create user "MAIN_USER" login password 'MAIN_USER';
    --drop user "MAIN_USER";

--with lowercase

    create user "sub_user" login password 'sub_user';
    --drop user "sub_user" ;

--with mixedcase

    create user "SUB_user_1" login password 'SUB_user_1';
    --drop user "SUB_user_1";

--with numbers

    create user "user123" login password 'user123';
    --drop user "user123" ;

--Create Roles

--with uppercase

    create role "ISO" login password 'ISO';
    --drop role "ISO";

--with lowercase

    create role "geography" login password 'geography';
    --drop role "geography" ;

--with mixedcase

    create role "ISO_ro_1" login password 'ISO_ro_1';
    --drop role "ISO_ro_1";

--with numbers

    create role "iso123" login password 'iso123';
    --drop role "iso123" ;

--Create schema

--with uppercase

    create schema "USDA";
    --drop schema "USDA";

--with lowercase

    create schema "geography";
    --drop schema "geography";

--with mixedcase

    create schema "test_schema-3166";
    --drop schema "test_schema-3166";

--with numbers

    create schema "test_schema1";
    --drop schema "test_schema1";

--Create Database

--with uppercase

    create database "TEST_DB";
    --drop database "TEST_DB";

--with lowercase

    create database test_db;
    --drop database test_db;

--with mixedcase

    create database "TEST_db";
    --drop database "TEST_db";

--with numbers

    create database test_db1;
   -- drop database test_db1;
