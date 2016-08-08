-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CK_SYNC1 ALTER TABLE INHERIT & NOINHERIT
--
--
--
-- HEAP TABLE
--
--
 CREATE TABLE ck_sync1_heap_parent_table1 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_parent_table1 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_heap_parent_table1 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_heap_parent_table1 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_heap_parent_table1;

 CREATE TABLE ck_sync1_heap_child_table1(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_child_table1 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_heap_child_table1;



 CREATE TABLE ck_sync1_heap_parent_table2 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_parent_table2 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_heap_parent_table2 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_heap_parent_table2 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_heap_parent_table2;

 CREATE TABLE ck_sync1_heap_child_table2(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_child_table2 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_heap_child_table2;

 CREATE TABLE ck_sync1_heap_parent_table3 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_parent_table3 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_heap_parent_table3 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_heap_parent_table3 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_heap_parent_table3;

 CREATE TABLE ck_sync1_heap_child_table3(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_child_table3 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_heap_child_table3;


 CREATE TABLE ck_sync1_heap_parent_table4 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_parent_table4 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_heap_parent_table4 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_heap_parent_table4 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_heap_parent_table4;

 CREATE TABLE ck_sync1_heap_child_table4(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_child_table4 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_heap_child_table4;

 CREATE TABLE ck_sync1_heap_parent_table5 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_parent_table5 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_heap_parent_table5 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_heap_parent_table5 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_heap_parent_table5;

 CREATE TABLE ck_sync1_heap_child_table5(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_child_table5 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_heap_child_table5;



 CREATE TABLE ck_sync1_heap_parent_table6 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_parent_table6 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_heap_parent_table6 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_heap_parent_table6 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_heap_parent_table6;

 CREATE TABLE ck_sync1_heap_child_table6(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_child_table6 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_heap_child_table6;


 CREATE TABLE ck_sync1_heap_parent_table7 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_parent_table7 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_heap_parent_table7 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_heap_parent_table7 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_heap_parent_table7;

 CREATE TABLE ck_sync1_heap_child_table7(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ck_sync1_heap_child_table7 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_heap_child_table7;



--
--
--  HEAP ALTER TABLE INHERIT & NOINHERIT
--
--
 ALTER TABLE sync1_heap_child_table2 INHERIT sync1_heap_parent_table2;
insert into sync1_heap_parent_table2 values ('4_four', 4, '4_four', 4);
 select * from sync1_heap_parent_table2;
 ALTER TABLE sync1_heap_child_table2 NO INHERIT sync1_heap_parent_table2;
insert into sync1_heap_parent_table2 values ('5_five', 5, '5_five', 5);
 select * from sync1_heap_parent_table2;
--
--

 ALTER TABLE ck_sync1_heap_child_table1 INHERIT ck_sync1_heap_parent_table1;
insert into ck_sync1_heap_parent_table1 values ('4_four', 4, '4_four', 4);
 select * from ck_sync1_heap_parent_table1;
 ALTER TABLE ck_sync1_heap_child_table1 NO INHERIT ck_sync1_heap_parent_table1;
insert into ck_sync1_heap_parent_table1 values ('5_five', 5, '5_five', 5);
 select * from ck_sync1_heap_parent_table1;




--
--
-- AO TABLE
--
--
 CREATE TABLE ck_sync1_ao_parent_table1 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_parent_table1 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_ao_parent_table1 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_ao_parent_table1 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_ao_parent_table1;

 CREATE TABLE ck_sync1_ao_child_table1(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_child_table1 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_ao_child_table1;


 CREATE TABLE ck_sync1_ao_parent_table2 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_parent_table2 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_ao_parent_table2 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_ao_parent_table2 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_ao_parent_table2;

 CREATE TABLE ck_sync1_ao_child_table2(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_child_table2 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_ao_child_table2;

 CREATE TABLE ck_sync1_ao_parent_table3 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_parent_table3 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_ao_parent_table3 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_ao_parent_table3 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_ao_parent_table3;

 CREATE TABLE ck_sync1_ao_child_table3(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_child_table3 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_ao_child_table3;


 CREATE TABLE ck_sync1_ao_parent_table4 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_parent_table4 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_ao_parent_table4 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_ao_parent_table4 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_ao_parent_table4;

 CREATE TABLE ck_sync1_ao_child_table4(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_child_table4 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_ao_child_table4;

 CREATE TABLE ck_sync1_ao_parent_table5 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_parent_table5 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_ao_parent_table5 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_ao_parent_table5 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_ao_parent_table5;

 CREATE TABLE ck_sync1_ao_child_table5(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_child_table5 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_ao_child_table5;

 CREATE TABLE ck_sync1_ao_parent_table6 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_parent_table6 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_ao_parent_table6 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_ao_parent_table6 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_ao_parent_table6;

 CREATE TABLE ck_sync1_ao_child_table6(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_child_table6 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_ao_child_table6;

 CREATE TABLE ck_sync1_ao_parent_table7 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_parent_table7 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_ao_parent_table7 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_ao_parent_table7 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_ao_parent_table7;

 CREATE TABLE ck_sync1_ao_child_table7(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ck_sync1_ao_child_table7 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_ao_child_table7;



--
--
--  AO ALTER TABLE INHERIT & NOINHERIT
--
--
 ALTER TABLE sync1_ao_child_table2 INHERIT sync1_ao_parent_table2;
insert into sync1_ao_parent_table2 values ('4_four', 4, '4_four', 4);
 select * from sync1_ao_parent_table2;
 ALTER TABLE sync1_ao_child_table2 NO INHERIT sync1_ao_parent_table2;
insert into sync1_ao_parent_table2 values ('5_five', 5, '5_five', 5);
 select * from sync1_ao_parent_table2;
--
--
 ALTER TABLE ck_sync1_ao_child_table1 INHERIT ck_sync1_ao_parent_table1;
insert into ck_sync1_ao_parent_table1 values ('4_four', 4, '4_four', 4);
 select * from ck_sync1_ao_parent_table1;
 ALTER TABLE ck_sync1_ao_child_table1 NO INHERIT ck_sync1_ao_parent_table1;
insert into ck_sync1_ao_parent_table1 values ('5_five', 5, '5_five', 5);
 select * from ck_sync1_ao_parent_table1;



--
--
-- CO TABLE
--
--


 CREATE TABLE ck_sync1_co_parent_table1 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_parent_table1 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_co_parent_table1 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_co_parent_table1 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_co_parent_table1;

 CREATE TABLE ck_sync1_co_child_table1(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_child_table1 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_co_parent_table1;

 CREATE TABLE ck_sync1_co_parent_table2 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_parent_table2 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_co_parent_table2 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_co_parent_table2 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_co_parent_table2;

 CREATE TABLE ck_sync1_co_child_table2(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_child_table2 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_co_parent_table2;

 CREATE TABLE ck_sync1_co_parent_table3 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_parent_table3 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_co_parent_table3 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_co_parent_table3 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_co_parent_table3;

 CREATE TABLE ck_sync1_co_child_table3(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_child_table3 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_co_parent_table3;

 CREATE TABLE ck_sync1_co_parent_table4 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_parent_table4 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_co_parent_table4 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_co_parent_table4 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_co_parent_table4;

 CREATE TABLE ck_sync1_co_child_table4(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_child_table4 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_co_parent_table4;

 CREATE TABLE ck_sync1_co_parent_table5 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_parent_table5 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_co_parent_table5 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_co_parent_table5 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_co_parent_table5;

 CREATE TABLE ck_sync1_co_child_table5(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_child_table5 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_co_parent_table5;


 CREATE TABLE ck_sync1_co_parent_table6 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_parent_table6 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_co_parent_table6 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_co_parent_table6 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_co_parent_table6;

 CREATE TABLE ck_sync1_co_child_table6(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_child_table6 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_co_parent_table6;

 CREATE TABLE ck_sync1_co_parent_table7 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_parent_table7 values ('0_zero', 0, '0_zero', 0);
 insert into ck_sync1_co_parent_table7 values ('1_zero', 1, '1_zero', 1);
 insert into ck_sync1_co_parent_table7 values ('2_zero', 2, '2_zero', 2);

 select * from ck_sync1_co_parent_table7;

 CREATE TABLE ck_sync1_co_child_table7(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ck_sync1_co_child_table7 values ('3_zero', 3, '3_zero', 3);
 select * from ck_sync1_co_parent_table7;



--
--
--  CO ALTER TABLE INHERIT & NOINHERIT
--
--
 ALTER TABLE sync1_co_child_table2 INHERIT sync1_co_parent_table2;
insert into sync1_co_parent_table2 values ('4_four', 4, '4_four', 4);
 select * from sync1_co_parent_table2;
 ALTER TABLE sync1_co_child_table2 NO INHERIT sync1_co_parent_table2;
insert into sync1_co_parent_table2 values ('5_five', 5, '5_five', 5);
 select * from sync1_co_parent_table2;
--
--
 ALTER TABLE ck_sync1_co_child_table1 INHERIT ck_sync1_co_parent_table1;
insert into ck_sync1_co_parent_table1 values ('4_four', 4, '4_four', 4);
 select * from ck_sync1_co_parent_table1;
 ALTER TABLE ck_sync1_co_child_table1 NO INHERIT ck_sync1_co_parent_table1;
insert into ck_sync1_co_parent_table1 values ('5_five', 5, '5_five', 5);
 select * from ck_sync1_co_parent_table1;

