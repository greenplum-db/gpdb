-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description starts, commmit and rollback transactions

CREATE DATABASE cancel_trans;
\c cancel_trans

CREATE TABLE employee(ID int,name varchar(10),salary real,start_date date,city varchar(10),region char(1));

INSERT INTO employee values (1,'jason',40420,'02/01/2007','New York');
INSERT INTO employee values (2,'Robert',14420,'01/02/2007','Vacouver');

SELECT * FROM employee;

BEGIN work;

INSERT INTO employee(ID,name) values (106,'Hall');
INSERT INTO employee values (3,'ann',40420,'02/01/2008','CA');
INSERT INTO employee values (4,'ben',14420,'01/02/2009','Texus');
INSERT INTO employee values (5,'cen',40420,'02/01/2007','NJ');
INSERT INTO employee values (6,'den',14420,'01/02/2006','Vacouver');
INSERT INTO employee values (7,'emily',40420,'02/01/2005','New York');
INSERT INTO employee values (8,'fen',14420,'01/02/2004','Vacouver');
INSERT INTO employee values (9,'ivel',40420,'02/01/2003','MD');
INSERT INTO employee values (10,'sam',14420,'01/02/2002','OH');
INSERT INTO employee values (11,'jack',40420,'02/01/2001','VT');
INSERT INTO employee values (12,'tom',14420,'01/02/2000','CT');
COMMIT work;

BEGIN ; 
INSERT INTO employee(ID,salary) values ( generate_series(13,100),generate_series(13,100));
ROLLBACK ;



BEGIN;
CREATE SCHEMA admin;
create table admin.fact_shc_dly_opr_sls(
 vend_pack_id  integer        not null,
 locn_nbr    integer        not null,
 day_nbr       date           not null,
 trs_typ_cd   character(1)  not null,
 mds_sts       smallint       not null,
 trs_un_qt     integer       , 
 trs_cst_dlr  numeric(15,4) , 
 trs_sll_dlr   numeric(15,2) )
Distributed by (vend_pack_id, locn_nbr, day_nbr, trs_typ_cd, mds_sts);


insert into admin.fact_shc_dly_opr_sls values ( generate_series(1,100), generate_series(1,100),'2008-11-11','A',1,12,123,1234);

 insert into admin.fact_shc_dly_opr_sls values ( generate_series(1,100), generate_series(1,100),'2009-12-12','R',2,22,223,2234);

 insert into admin.fact_shc_dly_opr_sls values ( generate_series(1,100), generate_series(1,100),'2006-10-10','S',3,33,225,3334);

 insert into admin.fact_shc_dly_opr_sls values ( generate_series(1,100), generate_series(1,100),'2006-10-10','B',4,44,445,4444);

 insert into admin.fact_shc_dly_opr_sls values ( generate_series(1,100), generate_series(1,100),'2005-09-09','X',5,55,555,555);
ROLLBACK;



BEGIN;
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
    insert into test_drop_column select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i,i||'_'||repeat('text',3) from generate_series(3,500)i;

--drop toast column

    ALTER TABLE test_drop_column DROP COLUMN toast_col ;

--drop non toast column

    ALTER TABLE test_drop_column DROP COLUMN non_toast_col ;

--drop default

    ALTER TABLE test_drop_column ALTER COLUMN col_with_default_text DROP DEFAULT;

    ALTER TABLE test_drop_column ALTER COLUMN col_with_default_text SET DEFAULT 'test1';

    ALTER TABLE test_drop_column ADD COLUMN added_col character varying(30);

    ALTER TABLE test_drop_column RENAME COLUMN bigint_col TO after_rename_col; 

    ALTER TABLE test_drop_column ALTER COLUMN numeric_col SET NOT NULL; 

ROLLBACK;
