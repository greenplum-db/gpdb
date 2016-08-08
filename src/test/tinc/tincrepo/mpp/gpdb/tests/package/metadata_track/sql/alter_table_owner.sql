--OWNER TO new_owner

          CREATE TABLE mdt_table_owner (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          )DISTRIBUTED RANDOMLY;

          CREATE ROLE mdt_user1;

          ALTER TABLE mdt_table_owner OWNER TO mdt_user1;

drop table mdt_table_owner;
drop role mdt_user1;

