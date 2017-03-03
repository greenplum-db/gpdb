-- @gucs gp_create_table_random_default_distribution=off
-- @product_version gpdb: [4.3.0.0- main]

-- Create updatable AO tables
CREATE TABLE uco_table1(
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric,
          int_col int4 NOT NULL,
          float_col float4,
          int_array_col int[],
          before_rename_col int4,
          change_datatype_col numeric,
          a_ts_without timestamp without time zone,
          b_ts_with timestamp with time zone,
          date_column date ,
          col_set_default numeric) with(appendonly=true, orientation=column) DISTRIBUTED RANDOMLY;

insert into uco_table1 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
insert into uco_table1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
insert into uco_table1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);

CREATE TABLE uco_table2(
          col_text text,
          col_numeric numeric
          CONSTRAINT tbl_chk_con1 CHECK (col_numeric < 250)
          ) with(appendonly=true, orientation=column) DISTRIBUTED by(col_numeric);

insert into uco_table2 values ('0_zero',0);
insert into uco_table2 values ('1_one',1);
insert into uco_table2 values ('2_two',2);
insert into uco_table2 select i||'_'||repeat('text',3),i from generate_series(1,10)i;

--Table in non-public schema

CREATE SCHEMA ucoschema1;
CREATE TABLE ucoschema1.uco_table3(
          stud_id int,
          stud_name varchar(20)
          ) with(appendonly=true, orientation=column) DISTRIBUTED by(stud_id);

insert into ucoschema1.uco_table3 values ( 1,'ann');
insert into ucoschema1.uco_table3 values ( 2,'ben');

CREATE TABLE uco_table3 with(appendonly=true, orientation=column) as select * from uco_table2;
CREATE TABLE uco_table4 with(appendonly=true, orientation=column) as select * from uco_table1;
CREATE TABLE uco_table5 with(appendonly=true, orientation=column) as select * from uco_table1;

-- Updatable AO tables
CREATE TABLE uco_table6 with(appendonly=true, orientation=column) as select * from uco_table2;
Update uco_table6 set col_text= 'new_0_zero' where col_numeric=0;

CREATE TABLE ucoschema1.uco_table7 with(appendonly=true, orientation=column) as select * from uco_table2;
Update ucoschema1.uco_table7 set col_text= 'new_0_zero' where col_numeric=0;

CREATE TABLE uco_table8 with(appendonly=true, orientation=column) as select * from uco_table1;
Delete from uco_table8 where bigint_col=0;

CREATE TABLE uco_table9 with(appendonly=true, orientation=column) as select * from uco_table1;
Delete from uco_table9 where bigint_col=0;

-- Tables with compression
CREATE TABLE uco_compr01(
          col_text text,
          col_numeric numeric
          CONSTRAINT tbl_chk_con1 CHECK (col_numeric < 250)
          ) with(appendonly=true, orientation=column, compresstype='zlib', compresslevel=1, blocksize=8192) DISTRIBUTED by(col_numeric);

insert into uco_compr01 values ('0_zero',0);
insert into uco_compr01 values ('1_one',1);
insert into uco_compr01 values ('2_two',2);

Create table uco_compr02 with(appendonly=true, orientation=column, compresstype='rle_type', compresslevel=1, blocksize=8192) as select * from uco_compr01;
Update uco_compr02 set col_text='3_three' where col_numeric=3;
