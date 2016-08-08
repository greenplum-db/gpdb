--INHERIT & NO INHERIT mdt_parent_table

          CREATE TABLE mdt_parent_table (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) DISTRIBUTED RANDOMLY;

          CREATE TABLE mdt_child_table(
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) DISTRIBUTED RANDOMLY;

          CREATE TABLE mdt_child_table1(
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) DISTRIBUTED RANDOMLY;

          ALTER TABLE mdt_child_table INHERIT mdt_parent_table;

          ALTER TABLE mdt_child_table1 INHERIT mdt_parent_table;
          ALTER TABLE mdt_child_table1 NO INHERIT mdt_parent_table;

drop table mdt_child_table ;
drop table mdt_child_table1 ;
drop table mdt_parent_table ;

