--Table constraint

    CREATE TABLE mdt_table_constraint  (
    did integer,
    name varchar(40)
    CONSTRAINT con1 CHECK (did > 99 AND name <> '')
    )DISTRIBUTED RANDOMLY;

--Tables with Default and column constraint

    CREATE TABLE mdt_table_with_default_constraint (
    col_with_default_numeric numeric DEFAULT 0,
    col_with_default_text character varying(30) DEFAULT 'test1',
    col_with_constraint numeric UNIQUE
    ) DISTRIBUTED BY (col_with_constraint);

--Table with column constraint

    CREATE TABLE mdt_table_with_default_constraint1 (
    col_with_default_numeric numeric PRIMARY KEY,
    col_with_default_text character varying(30) DEFAULT 'test1',
    col_with_constraint numeric
    ) DISTRIBUTED BY (col_with_default_numeric);

drop table mdt_table_constraint ;
drop table mdt_table_with_default_constraint;
drop table mdt_table_with_default_constraint1;
