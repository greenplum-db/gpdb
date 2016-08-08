-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description All alter tables

--Alter table

create database alter_table_db;
\c alter_table_db

--Rename Table

          CREATE TABLE table_name(
          col_text text,
          col_numeric numeric
          ) DISTRIBUTED RANDOMLY;

    	insert into table_name values ('0_zero',0);
    	insert into table_name values ('1_one',1);
    	insert into table_name values ('2_two',2);
        insert into table_name select i||'_'||repeat('text',100),i from generate_series(1,100)i;

          ALTER TABLE table_name RENAME TO table_new_name;

--ALTER Schema name

          CREATE SCHEMA dept;
          CREATE TABLE dept.csc(
          stud_id int,
          stud_name varchar(20)
          ) DISTRIBUTED RANDOMLY;

	  insert into dept.csc values ( 1,'ann');
          insert into dept.csc values ( 2,'ben');
          insert into dept.csc values ( 3,'sam');
          insert into dept.csc select i,i||'_'||repeat('text',3) from generate_series(4,100)i;

          CREATE SCHEMA new_dept;
          ALTER TABLE dept.csc SET SCHEMA new_dept;

--RENAME & ADD Column & ALTER column TYPE type & ALTER column SET DEFAULT expression

          CREATE TABLE test_alter_table(
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric,
          int_col int4,
          float_col float4,
          int_array_col int[],
          before_rename_col int4,
          change_datatype_col numeric,
          a_ts_without timestamp without time zone,
          b_ts_with timestamp with time zone,
          date_column date,
          col_set_default numeric)DISTRIBUTED RANDOMLY;

          insert into test_alter_table values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
          insert into test_alter_table values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
          insert into test_alter_table values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);
          insert into test_alter_table select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i from generate_series(3,100)i;


--ADD column type [ column_constraint [ ... ] ]

          ALTER TABLE test_alter_table ADD COLUMN added_col character varying(30);

--RENAME Column

          ALTER TABLE test_alter_table RENAME COLUMN before_rename_col TO after_rename_col;

--ALTER column TYPE type

          ALTER TABLE test_alter_table ALTER COLUMN change_datatype_col TYPE int4;

--ALTER column SET DEFAULT expression

          ALTER TABLE test_alter_table ALTER COLUMN col_set_default SET DEFAULT 0;

--ALTER column [ SET | DROP ] NOT NULL

          CREATE TABLE alter_table1(
          col_text text,
          col_numeric numeric NOT NULL
          ) DISTRIBUTED RANDOMLY;

          insert into alter_table1 values ('0_zero',0);
          insert into alter_table1 values ('1_one',1);
          insert into alter_table1 values ('2_two',2);
          insert into alter_table1 select i||'_'||repeat('text',100),i from generate_series(3,100)i;


          ALTER TABLE alter_table1 ALTER COLUMN col_numeric DROP NOT NULL;
          ALTER TABLE alter_table1 ALTER COLUMN col_numeric SET NOT NULL;

--ALTER column SET STATISTICS integer

          CREATE TABLE alter_set_statistics_table(
          col_text text,
          col_numeric numeric NOT NULL
          ) DISTRIBUTED RANDOMLY;

          insert into alter_set_statistics_table values ('0_zero',0);
          insert into alter_set_statistics_table values ('1_one',1);
          insert into alter_set_statistics_table values ('2_two',2);
          insert into alter_set_statistics_table select i||'_'||repeat('text',100),i from generate_series(3,100)i;


          ALTER TABLE alter_set_statistics_table ALTER col_numeric SET STATISTICS 1;

--ALTER column SET STORAGE

          CREATE TABLE alter_set_storage_table(
          col_text text,
          col_numeric numeric NOT NULL
          ) DISTRIBUTED RANDOMLY;

         insert into alter_set_storage_table values ('0_zero',0);
         insert into alter_set_storage_table values ('1_one',1);
         insert into alter_set_storage_table values ('2_two',2);
         insert into alter_set_storage_table select i||'_'||repeat('text',100),i from generate_series(3,100)i;

	 ALTER TABLE alter_set_storage_table ALTER col_text SET STORAGE PLAIN;

--ADD table_constraint

          CREATE TABLE distributors (
          did integer,
          name varchar(40)
          ) DISTRIBUTED BY (name);

	  insert into distributors values (1,'1_one');
          insert into distributors values (2,'2_two');
          insert into distributors values (3,'3_three');
          insert into distributors select i,i||'_'||repeat('text',7) from generate_series(4,100)i;

 
          ALTER TABLE distributors ADD UNIQUE(name);

--DROP CONSTRAINT constraint_name [ RESTRICT | CASCADE ]

          CREATE TABLE films (
          code char(5),
          title varchar(40),
          did integer,
          date_prod date,
          kind varchar(10),
          len interval hour to minute,
          CONSTRAINT production UNIQUE(date_prod)
          );

	  insert into films values ('aaa','Heavenly Life',1,'2008-03-03','good','2 hr 30 mins');
          insert into films values ('bbb','Beautiful Mind',2,'2007-05-05','good','1 hr 30 mins');
          insert into films values ('ccc','Wonderful Earth',3,'2009-03-03','good','2 hr 15 mins');
 
          ALTER TABLE films DROP CONSTRAINT production;

--ENABLE & DISABLE TRIGGER

          CREATE TABLE price_change (
          apn CHARACTER(15) NOT NULL,
          effective TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
          price NUMERIC(9,2),
          UNIQUE (apn, effective)
          );

	  insert into price_change (apn,price) values ('a',765);
          insert into price_change (apn,price) values ('b',766);
          insert into price_change (apn,price) values ('c',767);

          CREATE TABLE stock(
          stock_apn CHARACTER(15) NOT NULL,
          stock_effective TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
          stock_price NUMERIC(9,2)
          )DISTRIBUTED RANDOMLY;

          insert into stock (stock_apn,stock_price) values ('a',765);
          insert into stock (stock_apn,stock_price) values ('b',766);
          insert into stock (stock_apn,stock_price) values ('c',767);


          --trigger function to insert records as required:
          CREATE LANGUAGE plpgsql;

          CREATE OR REPLACE FUNCTION insert_price_change() RETURNS trigger AS '
          DECLARE
          changed boolean;
          BEGIN
          IF tg_op = ''DELETE'' THEN
          INSERT INTO price_change(apn, effective, price)
          VALUES (old.barcode, CURRENT_TIMESTAMP, NULL);
          RETURN old;
          END IF;
          IF tg_op = ''INSERT'' THEN
          changed := TRUE;
          ELSE
          changed := new.price IS NULL != old.price IS NULL OR new.price != old.price;
          END IF;
          IF changed THEN
          INSERT INTO price_change(apn, effective, price)
          VALUES (new.barcode, CURRENT_TIMESTAMP, new.price);
          END IF;
          RETURN new;
          END
          ' LANGUAGE plpgsql;

          --create a trigger on the table you wish to monitor:

          CREATE TRIGGER insert_price_change AFTER INSERT OR DELETE OR UPDATE ON stock
          FOR EACH ROW EXECUTE PROCEDURE insert_price_change();

          ALTER TABLE stock DISABLE TRIGGER insert_price_change;
          ALTER TABLE stock ENABLE TRIGGER insert_price_change;

--CLUSTER ON index_name & SET WITHOUT CLUSTER

          CREATE TABLE cluster_index_table (col1 int,col2 int) distributed randomly;

          insert into cluster_index_table values (1,1);
          insert into cluster_index_table values (2,2);
          insert into cluster_index_table values (3,3);
          insert into cluster_index_table values (4,4);
          insert into cluster_index_table values (generate_series(5,100),generate_series(5,100));

         create index clusterindex on cluster_index_table(col1);
          ALTER TABLE cluster_index_table CLUSTER on clusterindex;
          ALTER TABLE cluster_index_table SET WITHOUT CLUSTER;

--SET WITHOUT OIDS

          CREATE TABLE table_with_oid (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) WITH OIDS DISTRIBUTED RANDOMLY;
-- start_ignore
          insert into table_with_oid values ('0_zero', 0, '0_zero', 0);
          insert into table_with_oid values ('1_zero', 1, '1_zero', 1);
          insert into table_with_oid values ('2_zero', 2, '2_zero', 2);
          insert into table_with_oid select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(3,100)i;

-- end_ignore
          ALTER TABLE table_with_oid SET WITHOUT OIDS;

--SET & RESET ( storage_parameter = value , ... )

          CREATE TABLE table_set_storage_parameters (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) DISTRIBUTED RANDOMLY;

          insert into table_set_storage_parameters values ('0_zero', 0, '0_zero', 0);
          insert into table_set_storage_parameters values ('1_zero', 1, '1_zero', 1);
          insert into table_set_storage_parameters values ('2_zero', 2, '2_zero', 2);
          insert into table_set_storage_parameters select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(3,100)i;

          ALTER TABLE table_set_storage_parameters SET (APPENDONLY=TRUE , COMPRESSLEVEL= 5 , FILLFACTOR= 50);
          ALTER TABLE table_set_storage_parameters RESET (APPENDONLY , COMPRESSLEVEL, FILLFACTOR);

--INHERIT & NO INHERIT parent_table

          CREATE TABLE parent_table (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) DISTRIBUTED RANDOMLY;

          insert into parent_table values ('0_zero', 0, '0_zero', 0);
          insert into parent_table values ('1_zero', 1, '1_zero', 1);
          insert into parent_table values ('2_zero', 2, '2_zero', 2);

          CREATE TABLE child_table(
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) DISTRIBUTED RANDOMLY;

          insert into child_table values ('3_zero', 3, '3_zero', 3);

          ALTER TABLE child_table INHERIT parent_table;
          select * from parent_table;
          ALTER TABLE child_table NO INHERIT parent_table;
          select * from parent_table;

--OWNER TO new_owner

          CREATE TABLE table_owner (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          )DISTRIBUTED RANDOMLY;

	  insert into table_owner values ('1_one',1111,'1_test',1);
          insert into table_owner values ('2_two',2222,'2_test',2);
          insert into table_owner values ('3_three',3333,'3_test',3);
          insert into table_owner select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(3,100)i;

          CREATE ROLE user_1;

          ALTER TABLE table_owner OWNER TO user_1;
         -- DROP TABLE table_owner;
          -- DROP ROLE user_1;

--Drop column

    CREATE TABLE test_drop_column(
    toast_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric,
    int_col int4,
    float_col float4,
    int_array_col int[],
    non_toast_col numeric,
    a_ts_without timestamp without time zone,
    b_ts_with timestamp with time zone,
    date_column date,
    col_with_constraint numeric UNIQUE,
    col_with_default_text character varying(30) DEFAULT 'test1'
    );

    insert into test_drop_column values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
    insert into test_drop_column values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
    insert into test_drop_column values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);
    insert into test_drop_column select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i from generate_series(3,100)i;



--drop toast column

    ALTER TABLE test_drop_column DROP COLUMN toast_col ;

--drop non toast column

    ALTER TABLE test_drop_column DROP COLUMN non_toast_col ;

--drop default

    ALTER TABLE test_drop_column ALTER COLUMN col_with_default_text DROP DEFAULT;

--TODO - drop column from partitioned table
