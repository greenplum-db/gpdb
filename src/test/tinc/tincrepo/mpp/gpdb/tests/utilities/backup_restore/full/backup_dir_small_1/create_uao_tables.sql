-- @gucs gp_create_table_random_default_distribution=off
-- @product_version gpdb: [4.3.0.0- main]

-- Create updatable AO tables
CREATE TABLE uao_table1(
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
          col_set_default numeric) with(appendonly=true) DISTRIBUTED RANDOMLY;

insert into uao_table1 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
insert into uao_table1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
insert into uao_table1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);

--Table in non-public schema

CREATE SCHEMA uaoschema1;
CREATE TABLE uaoschema1.uao_table3(
          stud_id int,
          stud_name varchar(20)
          ) with(appendonly=true) DISTRIBUTED by(stud_id);

insert into uaoschema1.uao_table3 values ( 1,'ann');
insert into uaoschema1.uao_table3 values ( 2,'ben');

-- Updatable AO tables
CREATE TABLE uao_table6 with(appendonly=true) as select * from uao_table2;
Update uao_table6 set col_text= 'new_0_zero' where col_numeric=0;

-- Tables with compression
CREATE TABLE uao_compr01(
          col_text text,
          col_numeric numeric
          CONSTRAINT tbl_chk_con1 CHECK (col_numeric < 250)
          ) with(appendonly=true, compresstype='zlib', compresslevel=1, blocksize=8192) DISTRIBUTED by(col_numeric);

insert into uao_compr01 values ('0_zero',0);
insert into uao_compr01 values ('1_one',1);
insert into uao_compr01 values ('2_two',2);
