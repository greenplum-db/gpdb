-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CT ALTER TABLE INHERIT & NOINHERIT
--
--
--
-- HEAP TABLE
--
--
 CREATE TABLE ct_heap_parent_table1 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ct_heap_parent_table1 values ('0_zero', 0, '0_zero', 0);
 insert into ct_heap_parent_table1 values ('1_zero', 1, '1_zero', 1);
 insert into ct_heap_parent_table1 values ('2_zero', 2, '2_zero', 2);

 select * from ct_heap_parent_table1;

 CREATE TABLE ct_heap_child_table1(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ct_heap_child_table1 values ('3_zero', 3, '3_zero', 3);
 select * from ct_heap_child_table1;


 CREATE TABLE ct_heap_parent_table2 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ct_heap_parent_table2 values ('0_zero', 0, '0_zero', 0);
 insert into ct_heap_parent_table2 values ('1_zero', 1, '1_zero', 1);
 insert into ct_heap_parent_table2 values ('2_zero', 2, '2_zero', 2);

 select * from ct_heap_parent_table2;

 CREATE TABLE ct_heap_child_table2(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ct_heap_child_table2 values ('3_zero', 3, '3_zero', 3);
 select * from ct_heap_child_table2;

 CREATE TABLE ct_heap_parent_table3 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ct_heap_parent_table3 values ('0_zero', 0, '0_zero', 0);
 insert into ct_heap_parent_table3 values ('1_zero', 1, '1_zero', 1);
 insert into ct_heap_parent_table3 values ('2_zero', 2, '2_zero', 2);

 select * from ct_heap_parent_table3;

 CREATE TABLE ct_heap_child_table3(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ct_heap_child_table3 values ('3_zero', 3, '3_zero', 3);
 select * from ct_heap_child_table3;


 CREATE TABLE ct_heap_parent_table4 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ct_heap_parent_table4 values ('0_zero', 0, '0_zero', 0);
 insert into ct_heap_parent_table4 values ('1_zero', 1, '1_zero', 1);
 insert into ct_heap_parent_table4 values ('2_zero', 2, '2_zero', 2);

 select * from ct_heap_parent_table4;

 CREATE TABLE ct_heap_child_table4(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ct_heap_child_table4 values ('3_zero', 3, '3_zero', 3);
 select * from ct_heap_child_table4;

 CREATE TABLE ct_heap_parent_table5 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ct_heap_parent_table5 values ('0_zero', 0, '0_zero', 0);
 insert into ct_heap_parent_table5 values ('1_zero', 1, '1_zero', 1);
 insert into ct_heap_parent_table5 values ('2_zero', 2, '2_zero', 2);

 select * from ct_heap_parent_table5;

 CREATE TABLE ct_heap_child_table5(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) DISTRIBUTED RANDOMLY;

 insert into ct_heap_child_table5 values ('3_zero', 3, '3_zero', 3);
 select * from ct_heap_child_table5;

--
--
--  HEAP ALTER TABLE INHERIT & NOINHERIT
--
--
 ALTER TABLE sync1_heap_child_table4 INHERIT sync1_heap_parent_table4;
insert into sync1_heap_parent_table4 values ('4_four', 4, '4_four', 4);
 select * from sync1_heap_parent_table4;
 ALTER TABLE sync1_heap_child_table4 NO INHERIT sync1_heap_parent_table4;
insert into sync1_heap_parent_table4 values ('5_five', 5, '5_five', 5);
 select * from sync1_heap_parent_table4;
--
--
 ALTER TABLE ck_sync1_heap_child_table3 INHERIT ck_sync1_heap_parent_table3;
insert into ck_sync1_heap_parent_table3 values ('4_four', 4, '4_four', 4);
 select * from ck_sync1_heap_parent_table3;
 ALTER TABLE ck_sync1_heap_child_table3 NO INHERIT ck_sync1_heap_parent_table3;
insert into ck_sync1_heap_parent_table3 values ('5_five', 5, '5_five', 5);
 select * from ck_sync1_heap_parent_table3;
--
--
 ALTER TABLE ct_heap_child_table1 INHERIT ct_heap_parent_table1;
insert into ct_heap_parent_table1 values ('4_four', 4, '4_four', 4);
 select * from ct_heap_parent_table1;
 ALTER TABLE ct_heap_child_table1 NO INHERIT ct_heap_parent_table1;
insert into ct_heap_parent_table1 values ('5_five', 5, '5_five', 5);
 select * from ct_heap_parent_table1;




--
--
-- AO TABLE
--
--
 CREATE TABLE ct_ao_parent_table1 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ct_ao_parent_table1 values ('0_zero', 0, '0_zero', 0);
 insert into ct_ao_parent_table1 values ('1_zero', 1, '1_zero', 1);
 insert into ct_ao_parent_table1 values ('2_zero', 2, '2_zero', 2);

 select * from ct_ao_parent_table1;

 CREATE TABLE ct_ao_child_table1(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ct_ao_child_table1 values ('3_zero', 3, '3_zero', 3);
 select * from ct_ao_child_table1;


 CREATE TABLE ct_ao_parent_table2 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ct_ao_parent_table2 values ('0_zero', 0, '0_zero', 0);
 insert into ct_ao_parent_table2 values ('1_zero', 1, '1_zero', 1);
 insert into ct_ao_parent_table2 values ('2_zero', 2, '2_zero', 2);

 select * from ct_ao_parent_table2;

 CREATE TABLE ct_ao_child_table2(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ct_ao_child_table2 values ('3_zero', 3, '3_zero', 3);
 select * from ct_ao_child_table2;

 CREATE TABLE ct_ao_parent_table3 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ct_ao_parent_table3 values ('0_zero', 0, '0_zero', 0);
 insert into ct_ao_parent_table3 values ('1_zero', 1, '1_zero', 1);
 insert into ct_ao_parent_table3 values ('2_zero', 2, '2_zero', 2);

 select * from ct_ao_parent_table3;

 CREATE TABLE ct_ao_child_table3(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ct_ao_child_table3 values ('3_zero', 3, '3_zero', 3);
 select * from ct_ao_child_table3;


 CREATE TABLE ct_ao_parent_table4 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ct_ao_parent_table4 values ('0_zero', 0, '0_zero', 0);
 insert into ct_ao_parent_table4 values ('1_zero', 1, '1_zero', 1);
 insert into ct_ao_parent_table4 values ('2_zero', 2, '2_zero', 2);

 select * from ct_ao_parent_table4;

 CREATE TABLE ct_ao_child_table4(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ct_ao_child_table4 values ('3_zero', 3, '3_zero', 3);
 select * from ct_ao_child_table4;

 CREATE TABLE ct_ao_parent_table5 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true')  DISTRIBUTED RANDOMLY;

 insert into ct_ao_parent_table5 values ('0_zero', 0, '0_zero', 0);
 insert into ct_ao_parent_table5 values ('1_zero', 1, '1_zero', 1);
 insert into ct_ao_parent_table5 values ('2_zero', 2, '2_zero', 2);

 select * from ct_ao_parent_table5;

 CREATE TABLE ct_ao_child_table5(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 ) with ( appendonly='true') DISTRIBUTED RANDOMLY;

 insert into ct_ao_child_table5 values ('3_zero', 3, '3_zero', 3);
 select * from ct_ao_child_table5;


--
--
--  AO ALTER TABLE INHERIT & NOINHERIT
--
--
 ALTER TABLE sync1_ao_child_table4 INHERIT sync1_ao_parent_table4;
insert into sync1_ao_parent_table4 values ('4_four', 4, '4_four', 4);
 select * from sync1_ao_parent_table4;
 ALTER TABLE sync1_ao_child_table4 NO INHERIT sync1_ao_parent_table4;
insert into sync1_ao_parent_table4 values ('5_five', 5, '5_five', 5);
 select * from sync1_ao_parent_table4;
--
--
 ALTER TABLE ck_sync1_ao_child_table3 INHERIT ck_sync1_ao_parent_table3;
insert into ck_sync1_ao_parent_table3 values ('4_four', 4, '4_four', 4);
 select * from ck_sync1_ao_parent_table3;
 ALTER TABLE ck_sync1_ao_child_table3 NO INHERIT ck_sync1_ao_parent_table3;
insert into ck_sync1_ao_parent_table3 values ('5_five', 5, '5_five', 5);
 select * from ck_sync1_ao_parent_table3;
--
--
 ALTER TABLE ct_ao_child_table1 INHERIT ct_ao_parent_table1;
insert into ct_ao_parent_table1 values ('4_four', 4, '4_four', 4);
 select * from ct_ao_parent_table1;
 ALTER TABLE ct_ao_child_table1 NO INHERIT ct_ao_parent_table1;
insert into ct_ao_parent_table1 values ('5_five', 5, '5_five', 5);
 select * from ct_ao_parent_table1;



--
--
-- CO TABLE
--
--


 CREATE TABLE ct_co_parent_table1 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ct_co_parent_table1 values ('0_zero', 0, '0_zero', 0);
 insert into ct_co_parent_table1 values ('1_zero', 1, '1_zero', 1);
 insert into ct_co_parent_table1 values ('2_zero', 2, '2_zero', 2);

 select * from ct_co_parent_table1;

 CREATE TABLE ct_co_child_table1(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ct_co_child_table1 values ('3_zero', 3, '3_zero', 3);
 select * from ct_co_parent_table1;

 CREATE TABLE ct_co_parent_table2 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ct_co_parent_table2 values ('0_zero', 0, '0_zero', 0);
 insert into ct_co_parent_table2 values ('1_zero', 1, '1_zero', 1);
 insert into ct_co_parent_table2 values ('2_zero', 2, '2_zero', 2);

 select * from ct_co_parent_table2;

 CREATE TABLE ct_co_child_table2(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ct_co_child_table2 values ('3_zero', 3, '3_zero', 3);
 select * from ct_co_parent_table2;

 CREATE TABLE ct_co_parent_table3 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ct_co_parent_table3 values ('0_zero', 0, '0_zero', 0);
 insert into ct_co_parent_table3 values ('1_zero', 1, '1_zero', 1);
 insert into ct_co_parent_table3 values ('2_zero', 2, '2_zero', 2);

 select * from ct_co_parent_table3;

 CREATE TABLE ct_co_child_table3(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ct_co_child_table3 values ('3_zero', 3, '3_zero', 3);
 select * from ct_co_parent_table3;

 CREATE TABLE ct_co_parent_table4 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ct_co_parent_table4 values ('0_zero', 0, '0_zero', 0);
 insert into ct_co_parent_table4 values ('1_zero', 1, '1_zero', 1);
 insert into ct_co_parent_table4 values ('2_zero', 2, '2_zero', 2);

 select * from ct_co_parent_table4;

 CREATE TABLE ct_co_child_table4(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ct_co_child_table4 values ('3_zero', 3, '3_zero', 3);
 select * from ct_co_parent_table4;

 CREATE TABLE ct_co_parent_table5 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ct_co_parent_table5 values ('0_zero', 0, '0_zero', 0);
 insert into ct_co_parent_table5 values ('1_zero', 1, '1_zero', 1);
 insert into ct_co_parent_table5 values ('2_zero', 2, '2_zero', 2);

 select * from ct_co_parent_table5;

 CREATE TABLE ct_co_child_table5(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )  with ( appendonly='true', orientation='column')  DISTRIBUTED RANDOMLY;

 insert into ct_co_child_table5 values ('3_zero', 3, '3_zero', 3);
 select * from ct_co_parent_table5;




--
--
--  CO ALTER TABLE INHERIT & NOINHERIT
--
--

 ALTER TABLE sync1_co_child_table4 INHERIT sync1_co_parent_table4;
insert into sync1_co_parent_table4 values ('4_four', 4, '4_four', 4);
 select * from sync1_co_parent_table4;
 ALTER TABLE sync1_co_child_table4 NO INHERIT sync1_co_parent_table4;
insert into sync1_co_parent_table4 values ('5_five', 5, '5_five', 5);
 select * from sync1_co_parent_table4;
--
--
 ALTER TABLE ck_sync1_co_child_table3 INHERIT ck_sync1_co_parent_table3;
insert into ck_sync1_co_parent_table3 values ('4_four', 4, '4_four', 4);
 select * from ck_sync1_co_parent_table3;
 ALTER TABLE ck_sync1_co_child_table3 NO INHERIT ck_sync1_co_parent_table3;
insert into ck_sync1_co_parent_table3 values ('5_five', 5, '5_five', 5);
 select * from ck_sync1_co_parent_table3;
--
--
 ALTER TABLE ct_co_child_table1 INHERIT ct_co_parent_table1;
insert into ct_co_parent_table1 values ('4_four', 4, '4_four', 4);
 select * from ct_co_parent_table1;
 ALTER TABLE ct_co_child_table1 NO INHERIT ct_co_parent_table1;
insert into ct_co_parent_table1 values ('5_five', 5, '5_five', 5);
 select * from ct_co_parent_table1;

