\c gptest;

-- start_ignore
DROP TABLE if exists sto_alt_heap1;
-- end_ignore
CREATE TABLE sto_alt_heap1(
          text_col text default 'remove it',
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
          date_column date,
          col_set_default numeric)DISTRIBUTED RANDOMLY;

insert into sto_alt_heap1 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
insert into sto_alt_heap1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
insert into sto_alt_heap1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);

-- start_ignore
Drop table if exists sto_alt_heap3;
-- end_ignore
Create table sto_alt_heap3 as select * from sto_alt_heap1;

-- Alter table inherit tables
-- start_ignore
Drop table if exists sto_heap_parent cascade;
-- end_ignore
CREATE TABLE sto_heap_parent (
  text_col text,
  bigint_col bigint,
  char_vary_col character varying(30),
  numeric_col numeric
  ) DISTRIBUTED RANDOMLY;

insert into sto_heap_parent values ('0_zero', 0, '0_zero', 0);

-- start_ignore
Drop table if exists sto_alt_heap2;
-- end_ignore
CREATE TABLE sto_alt_heap2(
  text_col text,
  bigint_col bigint,
  char_vary_col character varying(30),
  numeric_col numeric
  )  DISTRIBUTED RANDOMLY;
insert into sto_alt_heap2 values ('1_zero', 1, '1_zero', 1);
