-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

--start_ignore
DROP DATABASE sto_db1;
--end_ignore
create database sto_db1 template template0;
\c sto_db1
CREATE TABLE sto_db_tb1 (
  text_col text,
  bigint_col bigint,
  char_vary_col character varying(30),
  numeric_col numeric
  ) with(appendonly= true) DISTRIBUTED RANDOMLY;

insert into sto_db_tb1 values ('0_zero', 0, '0_zero', 0);

select * from sto_db_tb1;

\c template1
select current_database();

DROP DATABASE sto_db1;
