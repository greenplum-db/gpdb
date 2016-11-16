\echo -- start_ignore
drop database vacuum_data_db;
drop database unvacuum_data_db;
drop database "TEST_DB";
drop database "TEST_db";

DROP USER "MAIN_USER";
DROP USER "sub_user" ;
DROP USER "SUB_user_1";
DROP USER "user123" ;

drop role  user_1;
drop role "ISO";
drop role "geography" ;
drop role "ISO_ro_1";
drop role "iso123" ;

DROP RESOURCE QUEUE db_resque2;
DROP RESOURCE QUEUE DB_RESque3;
DROP RESOURCE QUEUE DB_RESQUE4;

DROP ROLE db_role1;
DROP ROLE db_role2;
DROP ROLE db_role3;
DROP ROLE db_role4;
DROP ROLE db_role5;
DROP ROLE db_role6;
DROP ROLE db_role7;
DROP ROLE new_role8;
DROP ROLE db_role9;
DROP ROLE db_role10;
DROP ROLE db_role11;
DROP ROLE db_role12;
DROP GROUP db_grp1;

DROP GROUP db_user_grp1;
DROP ROLE db_user1;
DROP ROLE db_user2;
DROP ROLE db_user3;
DROP ROLE db_user4;
DROP ROLE db_user5;
DROP ROLE db_user6;
DROP ROLE db_user7;
DROP ROLE new_user8;
DROP ROLE db_user9;
DROP ROLE db_user10;
DROP ROLE db_user11;
DROP ROLE db_user12;
DROP RESOURCE QUEUE resqueu3;
DROP RESOURCE QUEUE resqueu4;

DROP DATABASE db_schema_test;
DROP USER db_user13 ;

DROP ROLE db_owner1;
DROP DATABASE db_name1;
DROP ROLE db_owner2;

DROP GROUP db_group1; 
DROP GROUP db_grp2;
DROP GROUP db_grp3;
DROP GROUP db_grp4;
DROP GROUP db_grp5;
DROP GROUP db_grp6;
DROP GROUP db_grp7;
DROP GROUP db_grp8;
DROP GROUP db_grp9;
DROP GROUP db_grp10;
DROP GROUP db_grp11;
DROP GROUP db_grp12;
DROP ROLE grp_role1;
DROP ROLE grp_role2;
DROP USER test_user_1;
DROP RESOURCE QUEUE grp_rsq1 ;

DROP RESOURCE QUEUE db_resque1;

DROP ROLE domain_owner;

DROP ROLE agg_owner;
DROP ROLE func_role ;

DROP ROLE sally;
DROP ROLE ron;
DROP ROLE ken;
\c template1
select current_database();
DROP ROLE admin ;

--
\echo -- end_ignore
set optimizer_disable_missing_stats_collection = on;

--
\c regression


--Create Table

--with uppercase
    CREATE TABLE "TABLE_NAME"( col_text text, col_numeric numeric ) DISTRIBUTED RANDOMLY;

    insert into "TABLE_NAME" values ('0_zero',0);
    insert into "TABLE_NAME" values ('1_one',1);
    insert into "TABLE_NAME" values ('2_two',2);
    insert into "TABLE_NAME" select i||'_'||repeat('text',100),i from generate_series(1,100)i;

--with lowercase
    CREATE TABLE table_name( col_text text, col_numeric numeric ) DISTRIBUTED RANDOMLY;

    insert into table_name values ('0_zero',0);
    insert into table_name values ('1_one',1);
    insert into table_name values ('2_two',2);
    insert into table_name select i||'_'||repeat('text',100),i from generate_series(1,100)i;

--with mixedcase
    CREATE TABLE "TABLE_name"( col_text text, col_numeric numeric ) DISTRIBUTED RANDOMLY;

    insert into "TABLE_name" values ('0_zero',0);
    insert into "TABLE_name" values ('1_one',1);
    insert into "TABLE_name" values ('2_two',2);
    insert into "TABLE_name" select i||'_'||repeat('text',100),i from generate_series(1,100)i;

--with numbers 
    CREATE TABLE table_123( col_text text, col_numeric numeric ) DISTRIBUTED RANDOMLY;

    insert into table_123 values ('0_zero',0);
    insert into table_123 values ('1_one',1);
    insert into table_123 values ('2_two',2);
    insert into table_123 select i||'_'||repeat('text',100),i from generate_series(1,100)i;

--Create Sequence

--with uppercase
    CREATE SEQUENCE "SERIAL" START 101;
--with lowercase
    CREATE SEQUENCE serialone START 101;

--with mixedcase
    CREATE SEQUENCE "Serial_123" START 101;

--with numbers
    CREATE SEQUENCE serial123 START 101;


--REINDEX bitmap index : Note: create bitmap index then vacuum leads to corrupted bitmap

    create table bm_test (i int, j int) distributed randomly;
    insert into bm_test values (0, 0), (0, 0), (0, 1), (1,0), (1,0), (1,1);
    create index bm_test_j on bm_test using bitmap(j);
    delete from bm_test where j =1;
    insert into bm_test values (0, 0), (1,0);
    insert into bm_test select i,i from generate_series(2,100)i;
    REINDEX index "public"."bm_test_j";
    --drop index  bm_test_j ;

--Create Rule
create table foo_rule_ao (a int) with (appendonly=true);
create table bar_rule_ao (a int);

--try and trick by setting rules
create rule one as on insert to bar_rule_ao do instead update foo_rule_ao set a=1;
create rule two as on insert to bar_rule_ao do instead delete from foo_rule_ao where a=1;

--Create User

--with uppercase
    create user "MAIN_USER" login password 'MAIN_USER';
    --DROP USER "MAIN_USER";

--with lowercase

    create user "sub_user" login password 'sub_user';
    --DROP USER "sub_user" ;

--with mixedcase

    create user "SUB_user_1" login password 'SUB_user_1';
    --DROP USER "SUB_user_1";

--with numbers

    create user "user123" login password 'user123';
    --DROP USER "user123" ;

--Create Roles

--with uppercase

    create role "ISO" login password 'ISO';
    --drop role "ISO";

--with lowercase

    create role "geography" login password 'geography';
    --drop role "geography" ;

--with mixedcase

    create role "ISO_ro_1" login password 'ISO_ro_1';
    --drop role "ISO_ro_1";

--with numbers

    create role "iso123" login password 'iso123';
    --drop role "iso123" ;
--Create schema
    
--with uppercase

    create schema "USDA";
    --drop schema "USDA";
    
--with lowercase

    create schema "geography";
    --drop schema "geography";
    
--with mixedcase

    create schema "test_schema-3166";
    --drop schema "test_schema-3166";
    
--with numbers

    create schema "test_schema1";
    --drop schema "test_schema1";

--Create Database
    
--with uppercase

    create database "TEST_DB";
    --drop database "TEST_DB";
    
--with mixedcase

    create database "TEST_db";
    --drop database "TEST_db";

--


--Table with all data types
    CREATE TABLE all_types( bit1 bit(1), bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),
varchar1 varchar(50),date1 date,dp1 double precision,int1 integer,interval1 interval,numeric1 numeric,real1 real,smallint1 smallint,time1 time,bigint1 bigint,
bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,
time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) distributed randomly;

insert into all_types values ('1','0','t','c','varchar1','char1','varchar1','2001-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','AA:AA:AA:AA:AA:AA','34.23',5,'text1','00:00:00','00:00:00+1359','2001-12-13 01:51:15','2001-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar2','char2','varchar2','2002-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','BB:BB:BB:BB:BB:BB','34.23',5,'text2','00:00:00','00:00:00+1359','2002-12-13 01:51:15','2002-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar3','char3','varchar3','2003-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','CC:CC:CC:CC:CC:CC','34.23',5,'text3','00:00:00','00:00:00+1359','2003-12-13 01:51:15','2003-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar4','char4','varchar4','2004-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','DD:DD:DD:DD:DD:DD','34.23',5,'text4','00:00:00','00:00:00+1359','2004-12-13 01:51:15','2004-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar5','char5','varchar5','2005-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','EE:EE:EE:EE:EE:EE','34.23',5,'text5','00:00:00','00:00:00+1359','2005-12-13 01:51:15','2005-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar5','char5','varchar5','2005-06-16',634.23234,6,'24',634,63,6,'12:12:12',6,6,'d','0.0.0.0',6,'0.0.0.0','EE:EE:EE:EE:EE:EE','64.23',generate_series(6,100),repeat('text',100),'00:00:00','00:00:00+1359','2005-07-15 01:51:15','2005-07-16 01:51:15+1359');


--global table

    CREATE GLOBAL TEMP TABLE table_global (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;

    insert into table_global values ('0_zero',0000,'0_test',0);
    insert into table_global values ('1_one',1111,'1_test',1);
    insert into table_global values ('2_two',2222,'2_test',2);
    insert into table_global select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

--local table

    CREATE LOCAL TEMP TABLE table_local (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;

    insert into table_local values ('0_zero',0000,'0_test',0);
    insert into table_local values ('1_one',1111,'1_test',1);
    insert into table_local values ('2_two',2222,'2_test',2);
    insert into table_local select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;


--like parent_table

    CREATE TABLE table_like_parent (
    like table_local
    ) DISTRIBUTED RANDOMLY;

--like parent_table

    CREATE TABLE table_like_parent1 (
    like table_local INCLUDING DEFAULTS
    ) DISTRIBUTED RANDOMLY;

--like parent_table

    CREATE TABLE table_like_parent2 (
    like table_local INCLUDING CONSTRAINTS
    ) DISTRIBUTED RANDOMLY;

--like parent_table

    CREATE TABLE table_like_parent3 (
    like table_local EXCLUDING DEFAULTS
    ) DISTRIBUTED RANDOMLY;

--like parent_table

CREATE TABLE table_like_parent4 (
    like table_local EXCLUDING CONSTRAINTS
    ) DISTRIBUTED RANDOMLY; 
    
--Incorrect inherits parent_table
-- good test case, but there's a bug for earlier gpdb versions so it won't work with upgrade.  it causes pg_dump to die
--    CREATE TABLE table_inherits_parent (
--    INHERITS table_local
--    ) DISTRIBUTED RANDOMLY;
    
--Inherits parent_table - MPP-5300 
    
   CREATE TABLE table_parent ( a int, b text) DISTRIBUTED BY (a);
   insert into table_parent values (generate_series(1,10),'test');
\echo -- start_ignore
   select count(*) from table_parent;

   CREATE TABLE table_child() INHERITS(table_parent);
   insert into table_child values (generate_series(11,20),'child');
   select * from table_child;

   select * from table_parent;
   select * from table_child;
   \echo -- end_ignore 

--Table Creation using Create Table As (CTAS) with both the new tables columns being explicitly or implicitly created


    CREATE TABLE test_table4(
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

insert into test_table4 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
insert into test_table4 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
insert into test_table4 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);
insert into test_table4 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i from generate_series(3,100)i;
    
CREATE TABLE ctas_table1 AS SELECT * FROM test_table4;
CREATE TABLE ctas_table2 AS SELECT text_col,bigint_col,char_vary_col,numeric_col FROM test_table4;
    
    
--Create Table and then Select Into / Insert
    

    CREATE TABLE test_table5(
    int_array_col int[],
    before_rename_col int4, 
    change_datatype_col numeric,
    a_ts_without timestamp without time zone,
    b_ts_with timestamp with time zone,
    date_column date
) DISTRIBUTED RANDOMLY;
    
insert into test_table5(int_array_col ,    before_rename_col ,    change_datatype_col ,    a_ts_without ,b_ts_with ,date_column)(select    int_array_col ,    before_rename_col ,    change_datatype_col ,    a_ts_without ,b_ts_with ,date_column from test_table4);


--Toast Table
    
    CREATE TABLE toast_table (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;
    
    insert into toast_table values ('0_zero', 0, '0_zero', 0);
    insert into toast_table values ('1_zero', 1, '1_zero', 1);
    insert into toast_table values ('2_zero', 2, '2_zero', 2);
    insert into toast_table select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;
    
--Temp Table
                                                      
 CREATE TEMP TABLE temp_table(
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;

    insert into temp_table values ('0_zero', 0, '0_zero', 0);
    insert into temp_table values ('1_zero', 1, '1_zero', 1);
    insert into temp_table values ('2_zero', 2, '2_zero', 2);
    insert into table_local select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,50000)i;

--External Table

CREATE EXTERNAL TABLE ext_20081031095512_23244_11671 (
    a_party_id bigint,
    start_date date,
    start_time_hour smallint,
    start_time_min smallint,
    start_time_sec smallint,
    duration integer,
    record_dt timestamp without time zone,
    framed_ip_address character varying(15)
) LOCATION (
    'gpfdist://97.253.36.12:9010/R_LAA_DATA_2008-10-27.dat'
) FORMAT 'text' (delimiter ',' null '' )
ENCODING 'UTF8'
LOG ERRORS SEGMENT REJECT LIMIT 100 ROWS;


CREATE WRITABLE EXTERNAL TABLE wet_ext_20081031095512_23244_11671 (
    a_party_id bigint,
    start_date date,
    start_time_hour smallint,
    start_time_min smallint,
    start_time_sec smallint,
    duration integer,
    record_dt timestamp without time zone,
    framed_ip_address character varying(15)
) LOCATION (
'gpfdist://97.253.36.12:9010/R_LAA_DATA_2008-10-27.dat'
) FORMAT 'text' (delimiter ',' null '' );
    
--Web External Table
    
    CREATE EXTERNAL WEB TABLE web_ex_table_vmstat_output (
    hostname text,
    threads text,
    memory text,
    page text,
    disk text,
    faults text,
    cpu text
    ) EXECUTE E'vmstat 1 2|tail -1|nawk -v H=`hostname` \'{print H"|"$1" "$2" "$3"|"$4" "$5"|"$6" "$7" "$8" "$9" "$10" "$11" "$12"|"$13" "$14" "$15" "$16"|"$17" "$18" "$19"|"$20" "$21" "$22}\'' ON HOST
    FORMAT 'text' (delimiter '|' null ''  )
--ENCODING 'UFT8'
;   
    
--Tables with distributed randomly and distributed columns
    
    CREATE TABLE table_distributed_by (
    col_with_default numeric DEFAULT 0,
    col_with_default_drop_default character varying(30) DEFAULT 'test1',
    col_with_constraint numeric UNIQUE
    ) DISTRIBUTED BY (col_with_constraint);

    insert into table_distributed_by (col_with_default_drop_default,col_with_constraint) values('0_zero',0);
    insert into table_distributed_by values(11,'1_zero',1);
    insert into table_distributed_by select i,i||'_'||repeat('text',5),i from generate_series(20,100)i;
    insert into table_distributed_by (col_with_default,col_with_constraint)values (33,3);

    CREATE TABLE table_distributed_randomly (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;
    
    insert into table_distributed_randomly values ('0_zero', 0, '0_zero', 0);
    insert into table_distributed_randomly values ('1_zero', 1, '1_zero', 1);
    insert into table_distributed_randomly values ('2_zero', 2, '2_zero', 2);
    insert into table_distributed_randomly select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

--Table constraint

    CREATE TABLE table_constraint  (
    did integer,
    name varchar(40)
    CONSTRAINT con1 CHECK (did > 99 AND name <> '')
    )DISTRIBUTED RANDOMLY;

    insert into table_constraint  values (100,'name_1');
    insert into table_constraint  values (200,'name_2');
    insert into table_constraint  values (300,'name_3');

--Tables with Default and column constraint

    CREATE TABLE table_with_default_constraint (
    col_with_default_numeric numeric DEFAULT 0,
    col_with_default_text character varying(30) DEFAULT 'test1',
    col_with_constraint numeric UNIQUE
    ) DISTRIBUTED BY (col_with_constraint);

    insert into table_with_default_constraint  (col_with_default_text,col_with_constraint) values('0_zero',0);
    insert into table_with_default_constraint  values(11,'1_zero',1);
    insert into table_with_default_constraint select i,i||'_'||repeat('text',5),i from generate_series(20,100)i;
    insert into table_with_default_constraint  (col_with_default_numeric,col_with_constraint)values (33,3);

--Table with column constraint

    CREATE TABLE table_with_default_constraint1 (
    col_with_default_numeric numeric PRIMARY KEY,
    col_with_default_text character varying(30) DEFAULT 'test1',
    col_with_constraint numeric
    ) DISTRIBUTED BY (col_with_default_numeric);

    insert into table_with_default_constraint1 values (0,'hello0',0);
    insert into table_with_default_constraint1 values (1,'hello1',1);
    insert into table_with_default_constraint1 values (2,'hello2',2);
    insert into table_with_default_constraint1 select i,i||'_'||repeat('text',5),i from generate_series(3,100)i;

--Table with Oids

    CREATE TABLE table_with_oid (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) WITH OIDS DISTRIBUTED RANDOMLY;
    
\echo -- start_ignore
    insert into table_with_oid values ('0_zero', 0, '0_zero', 0);
    insert into table_with_oid values ('1_zero', 1, '1_zero', 1);
    insert into table_with_oid values ('2_zero', 2, '2_zero', 2);
    insert into table_with_oid select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

\echo -- end_ignore
    
--Tables with storage Parameters is covered in ao_tables
    
create table nodetest (testname text, oldnode text, newnode text) distributed by (testname);
copy nodetest from stdin delimiter '|';
int8|{OPEXPR :opno 413 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 20 :typeMod -1} {CONST :consttype 20 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ 0 16 -91 -44 -24 0 0 0 ]})}|{OPEXPR :opno 413 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 20 :typeMod -1} {CONST :consttype 20 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ 0 16 -91 -44 -24 0 0 0 ]})}
float4|{OPEXPR :opno 623 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 700 :typeMod -1} {CONST :consttype 700 :constlen 4 :constbyval false :constisnull false :constvalue 4 [ 0 0 -56 66 ]})}|{OPEXPR :opno 623 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 700 :typeMod -1} {CONST :consttype 700 :constlen 4 :constbyval true :constisnull false :constvalue 4 [ 0 0 -56 66 0 0 0 0 ]})}
float8|{OPEXPR :opno 674 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 701 :typeMod -1} {CONST :consttype 701 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ 0 0 0 0 0 -120 -61 64 ]})}|{OPEXPR :opno 674 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 701 :typeMod -1} {CONST :consttype 701 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ 0 0 0 0 0 -120 -61 64 ]})}
timestamp|{OPEXPR :opno 2064 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1114 :typeMod -1} {CONST :consttype 1114 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ -64 -111 -112 112 -9 -56 0 0 ]})}|{OPEXPR :opno 2064 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1114 :typeMod -1} {CONST :consttype 1114 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ -64 -111 -112 112 -9 -56 0 0 ]})}
timestamptz|{OPEXPR :opno 1324 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1184 :typeMod -1} {CONST :consttype 1184 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ -64 -59 114 -95 -5 -56 0 0 ]})}|{OPEXPR :opno 1324 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1184 :typeMod -1} {CONST :consttype 1184 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ -64 -59 114 -95 -5 -56 0 0 ]})}
\.  
SELECT 'GARBAGE AFTER BOGUS END-OF-COPY (NOTE WHITESPACE AFTER THE EOF MARKER)';
int8|{OPEXPR :opno 413 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 20 :typeMod -1} {CONST :consttype 20 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ 0 16 -91 -44 -24 0 0 0 ]})}|{OPEXPR :opno 413 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 20 :typeMod -1} {CONST :consttype 20 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ 0 16 -91 -44 -24 0 0 0 ]})}
float4|{OPEXPR :opno 623 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 700 :typeMod -1} {CONST :consttype 700 :constlen 4 :constbyval false :constisnull false :constvalue 4 [ 0 0 -56 66 ]})}|{OPEXPR :opno 623 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 700 :typeMod -1} {CONST :consttype 700 :constlen 4 :constbyval true :constisnull false :constvalue 4 [ 0 0 -56 66 0 0 0 0 ]})}
float8|{OPEXPR :opno 674 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 701 :typeMod -1} {CONST :consttype 701 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ 0 0 0 0 0 -120 -61 64 ]})}|{OPEXPR :opno 674 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 701 :typeMod -1} {CONST :consttype 701 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ 0 0 0 0 0 -120 -61 64 ]})}
timestamp|{OPEXPR :opno 2064 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1114 :typeMod -1} {CONST :consttype 1114 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ -64 -111 -112 112 -9 -56 0 0 ]})}|{OPEXPR :opno 2064 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1114 :typeMod -1} {CONST :consttype 1114 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ -64 -111 -112 112 -9 -56 0 0 ]})}
timestamptz|{OPEXPR :opno 1324 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1184 :typeMod -1} {CONST :consttype 1184 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ -64 -59 114 -95 -5 -56 0 0 ]})}|{OPEXPR :opno 1324 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1184 :typeMod -1} {CONST :consttype 1184 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ -64 -59 114 -95 -5 -56 0 0 ]})}
\.

--Alter table

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
          ) distributed by (date_prod);

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
          ) distributed by (apn, effective);

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
          ' LANGUAGE plpgsql MODIFIES SQL DATA;

          --create a trigger on the table you wish to monitor:

          CREATE TRIGGER insert_price_change AFTER INSERT OR DELETE OR UPDATE ON stock
          FOR EACH ROW EXECUTE PROCEDURE insert_price_change();

          ALTER TABLE stock DISABLE TRIGGER insert_price_change;
          ALTER TABLE stock ENABLE TRIGGER insert_price_change;

--SET WITHOUT OIDS

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

--TODO - drop column from partitioned table

--Defining Multi-level Partitions
 
CREATE TABLE sales (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
SUBPARTITION BY LIST (region) 
SUBPARTITION TEMPLATE 
( SUBPARTITION usa VALUES ('usa'),  
  SUBPARTITION asia VALUES ('asia'),  
  SUBPARTITION europe VALUES ('europe') ) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month') ); 

insert into sales values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'usa');

insert into sales values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'asia');

insert into sales values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'europe');


insert into sales values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'usa');

insert into sales values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'asia');

insert into sales values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'europe');



-- Partitioning an Existing Table

CREATE TABLE sales1 (LIKE sales) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month') );

INSERT INTO sales1 SELECT * FROM sales; 

-- Verifying Your Partition Strategy 
--Viewing Your Partition Design 

SELECT partitionboundary, partitiontablename, partitionname, 
partitionlevel, partitionrank FROM pg_partitions WHERE 
tablename='sales'; 

-- Adding a New Partition 
ALTER TABLE sales ALTER PARTITION FOR (RANK(12)) 
      ADD PARTITION africa VALUES ('africa');

-- Adding a Default Partition

ALTER TABLE sales ADD DEFAULT PARTITION other;

insert into sales values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'usa');

insert into sales values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'asia');

insert into sales values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'europe');

-- Renaming a Partition
--ALTER TABLE sales RENAME PARTITION FOR ('2008-01-01') TO jan08;
ALTER TABLE sales RENAME PARTITION FOR (RANK(1)) TO jan08;


-- Dropping a Partition

ALTER TABLE sales DROP PARTITION FOR (RANK(1));

-- Truncating a Partition

ALTER TABLE sales TRUNCATE PARTITION FOR (RANK(1));

-- Exchanging a Partition

CREATE TABLE sales2 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month') ); 

insert into sales2 values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'usa');

insert into sales2 values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'asia');

insert into sales2 values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'europe');

CREATE TABLE jan08 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) ;

ALTER TABLE sales2 EXCHANGE PARTITION FOR ('2008-01-01') WITH 
TABLE jan08;


-- Splitting a Partition
CREATE TABLE sales3 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month') ); 

ALTER TABLE sales3 SPLIT PARTITION FOR ('2008-01-01') 
AT ('2008-01-16') 
INTO (PARTITION jan081to15, PARTITION jan0816to31);

ALTER TABLE sales3 ADD DEFAULT PARTITION other;

ALTER TABLE sales3 SPLIT DEFAULT PARTITION 
START ('2009-01-01') INCLUSIVE 
END ('2009-02-01') EXCLUSIVE 
INTO (PARTITION jan09, PARTITION other);


---------------------
--AO Partitions
---------------------

CREATE TABLE sales_ao (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
SUBPARTITION BY LIST (region) 
SUBPARTITION TEMPLATE 
( SUBPARTITION usa VALUES ('usa'),  
  SUBPARTITION asia VALUES ('asia'),  
  SUBPARTITION europe VALUES ('europe') ) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

insert into sales_ao values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'usa');

insert into sales_ao values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'asia');

insert into sales_ao values(generate_series(1,10000),'2008-01-01',generate_series(1,10000), 'europe');


insert into sales_ao values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'usa');

insert into sales_ao values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'asia');

insert into sales_ao values(generate_series(10001,20000),'2008-06-01',generate_series(1,10000), 'europe');



-- Partitioning an Existing Table

CREATE TABLE sales_ao1 (LIKE sales_ao) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5));

INSERT INTO sales_ao1 SELECT * FROM sales_ao; 

-- Verifying Your Partition Strategy 
--Viewing Your Partition Design 

SELECT partitionboundary, partitiontablename, partitionname, 
partitionlevel, partitionrank FROM pg_partitions WHERE 
tablename='sales_ao'; 

-- Adding a New Partition 
ALTER TABLE sales_ao ALTER PARTITION FOR (RANK(12)) 
      ADD PARTITION africa VALUES ('africa');

-- Adding a Default Partition

ALTER TABLE sales_ao ADD DEFAULT PARTITION other;

insert into sales_ao values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'usa');

insert into sales_ao values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'asia');

insert into sales_ao values(generate_series(1,10000),'2009-01-01',generate_series(1,10000), 'europe');

-- Renaming a Partition
--ALTER TABLE sales_ao RENAME PARTITION FOR ('2008-01-01') TO jan08;
ALTER TABLE sales_ao RENAME PARTITION FOR (RANK(1)) TO jan08;


-- Dropping a Partition

ALTER TABLE sales_ao DROP PARTITION FOR (RANK(1));

-- Truncating a Partition

ALTER TABLE sales_ao TRUNCATE PARTITION FOR (RANK(1));

-- Exchanging a Partition

CREATE TABLE sales_ao2 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

insert into sales_ao2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'usa');

insert into sales_ao2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'asia');

insert into sales_ao2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'europe');

ALTER TABLE sales_ao2 EXCHANGE PARTITION FOR ('2008-01-01') WITH 
TABLE jan08;


-- Splitting a Partition
CREATE TABLE sales_ao3 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

ALTER TABLE sales_ao3 SPLIT PARTITION FOR ('2008-01-01') 
AT ('2008-01-16') 
INTO (PARTITION jan081to15, PARTITION jan0816to31);

ALTER TABLE sales_ao3 ADD DEFAULT PARTITION other;

ALTER TABLE sales_ao3 SPLIT DEFAULT PARTITION 
START ('2009-01-01') INCLUSIVE 
END ('2009-02-01') EXCLUSIVE 
INTO (PARTITION jan09, PARTITION other);

-------------------------
---Hybrid partitions
-------------------------

CREATE TABLE sales_hybrid (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
SUBPARTITION BY LIST (region) 
SUBPARTITION TEMPLATE 
( SUBPARTITION usa VALUES ('usa'),  
  SUBPARTITION asia VALUES ('asia'),  
  SUBPARTITION europe VALUES ('europe') ) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

insert into sales_hybrid values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'usa');

insert into sales_hybrid values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'asia');

insert into sales_hybrid values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'europe');


insert into sales_hybrid values(generate_series(1001,2000),'2008-06-01',generate_series(1,1000), 'usa');

insert into sales_hybrid values(generate_series(1001,2000),'2008-06-01',generate_series(1,1000), 'asia');

insert into sales_hybrid values(generate_series(1001,2000),'2008-06-01',generate_series(1,1000), 'europe');



-- Partitioning an Existing Table

CREATE TABLE sales_hybrid1 (LIKE sales_hybrid) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month'));

INSERT INTO sales_hybrid1 SELECT * FROM sales_hybrid; 

-- Verifying Your Partition Strategy 
--Viewing Your Partition Design 

SELECT partitionboundary, partitiontablename, partitionname, 
partitionlevel, partitionrank FROM pg_partitions WHERE 
tablename='sales_hybrid'; 

-- Adding a New Partition 
ALTER TABLE sales_hybrid ALTER PARTITION FOR (RANK(12)) 
      ADD PARTITION africa VALUES ('africa');

-- Adding a Default Partition

ALTER TABLE sales_hybrid ADD DEFAULT PARTITION other;

insert into sales_hybrid values(generate_series(1,1000),'2009-01-01',generate_series(1,1000), 'usa');

insert into sales_hybrid values(generate_series(1,1000),'2009-01-01',generate_series(1,1000), 'asia');

insert into sales_hybrid values(generate_series(1,1000),'2009-01-01',generate_series(1,1000), 'europe');

-- Renaming a Partition
--ALTER TABLE sales_hybrid RENAME PARTITION FOR ('2008-01-01') TO jan08;
ALTER TABLE sales_hybrid RENAME PARTITION FOR (RANK(1)) TO jan08;


-- Dropping a Partition

ALTER TABLE sales_hybrid DROP PARTITION FOR (RANK(1));

-- Truncating a Partition

ALTER TABLE sales_hybrid TRUNCATE PARTITION FOR (RANK(1));

-- Exchanging a Partition

CREATE TABLE sales_hybrid2 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

insert into sales_hybrid2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'usa');

insert into sales_hybrid2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'asia');

insert into sales_hybrid2 values(generate_series(1,1000),'2008-01-01',generate_series(1,1000), 'europe');

ALTER TABLE sales_hybrid2 EXCHANGE PARTITION FOR ('2008-01-01') WITH 
TABLE jan08;


-- Splitting a Partition
CREATE TABLE sales_hybrid3 (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month')
   WITH (appendonly=true, compresslevel=5)); 

ALTER TABLE sales_hybrid3 SPLIT PARTITION FOR ('2008-01-01') 
AT ('2008-01-16') 
INTO (PARTITION jan081to15, PARTITION jan0816to31);

ALTER TABLE sales_hybrid3 ADD DEFAULT PARTITION other;

ALTER TABLE sales_hybrid3 SPLIT DEFAULT PARTITION 
START ('2009-01-01') INCLUSIVE 
END ('2009-02-01') EXCLUSIVE 
INTO (PARTITION jan09, PARTITION other);
-- Visibility of data and objects--Update, Delete, Insert of data into a table
--Vaccuumed vs. Unvaccumed Data
--DROP TABLE
--TRUNCATE TABLE

--VACUUMED DATA

create database vacuum_data_db;
\c vacuum_data_db;

CREATE TABLE all_types(bit1 bit(1),bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),varchar1 varchar(50),date1 date,dp1 double precision,int1 integer, interval1 interval, numeric1 numeric, real1 real,smallint1 smallint,time1 time,bigint1 bigint,bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) distributed randomly; 

insert into all_types values ('1','0','t','c','varchar1','char1','varchar1','2001-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','AA:AA:AA:AA:AA:AA','34.23',5,'text1','00:00:00','00:00:00+1359','2001-12-13 01:51:15','2001-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar2','char2','varchar2','2002-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','BB:BB:BB:BB:BB:BB','34.23',5,'text2','00:00:00','00:00:00+1359','2002-12-13 01:51:15','2002-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar3','char3','varchar3','2003-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','CC:CC:CC:CC:CC:CC','34.23',5,'text3','00:00:00','00:00:00+1359','2003-12-13 01:51:15','2003-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar4','char4','varchar4','2004-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','DD:DD:DD:DD:DD:DD','34.23',5,'text4','00:00:00','00:00:00+1359','2004-12-13 01:51:15','2004-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar5','char5','varchar5','2005-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','EE:EE:EE:EE:EE:EE','34.23',5,'text5','00:00:00','00:00:00+1359','2005-12-13 01:51:15','2005-12-13 01:51:15+1359');

update all_types set cidr1='1.1.1.1' where cidr1='0.0.0.0';

update all_types set bytea1='x' where bytea1='d';

update all_types set charvar1='hello' where charvar1='varchar1';

delete from all_types where  charvar1='varchar2';

delete from all_types where  date1 = '2003-11-11';

delete from all_types;

truncate all_types;

drop table all_types cascade;

CREATE EXTERNAL TABLE ext_table1 (
    a_party_id bigint,
    start_date date,
    start_time_hour smallint,
    start_time_min smallint,
    start_time_sec smallint,
    duration integer,
    record_dt timestamp without time zone,
    framed_ip_address character varying(15)
) LOCATION (
    'gpfdist://97.253.36.12:9010/R_LAA_DATA_2008-10-27.dat'
) FORMAT 'text' (delimiter ',' null '' )
ENCODING 'UTF8';
DROP EXTERNAL TABLE ext_table1;

vacuum ;

--UNVACUUMED DATA
create database unvacuum_data_db;
\c unvacuum_data_db

CREATE TABLE all_types(bit1 bit(1),bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),varchar1 varchar(50),date1 date,dp1 double precision,int1 integer, interval1 interval, numeric1 numeric, real1 real,smallint1 smallint,time1 time,bigint1 bigint,bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) distributed randomly;

insert into all_types values ('1','0','t','c','varchar1','char1','varchar1','2001-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','AA:AA:AA:AA:AA:AA','34.23',5,'text1','00:00:00','00:00:00+1359','2001-12-13 01:51:15','2001-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar2','char2','varchar2','2002-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','BB:BB:BB:BB:BB:BB','34.23',5,'text2','00:00:00','00:00:00+1359','2002-12-13 01:51:15','2002-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar3','char3','varchar3','2003-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','CC:CC:CC:CC:CC:CC','34.23',5,'text3','00:00:00','00:00:00+1359','2003-12-13 01:51:15','2003-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar4','char4','varchar4','2004-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','DD:DD:DD:DD:DD:DD','34.23',5,'text4','00:00:00','00:00:00+1359','2004-12-13 01:51:15','2004-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar5','char5','varchar5','2005-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','EE:EE:EE:EE:EE:EE','34.23',5,'text5','00:00:00','00:00:00+1359','2005-12-13 01:51:15','2005-12-13 01:51:15+1359');

update all_types set cidr1='1.1.1.1' where cidr1='0.0.0.0';

update all_types set bytea1='x' where bytea1='d';

update all_types set charvar1='hello' where charvar1='varchar1';

delete from all_types where  charvar1='varchar2';

delete from all_types where  date1 = '2003-11-11';

delete from all_types;

truncate all_types;

drop table all_types cascade;

CREATE EXTERNAL TABLE ext_table1 (
    a_party_id bigint,
    start_date date,
    start_time_hour smallint,
    start_time_min smallint,
    start_time_sec smallint,
    duration integer,
    record_dt timestamp without time zone,
    framed_ip_address character varying(15)
) LOCATION (
    'gpfdist://97.253.36.12:9010/R_LAA_DATA_2008-10-27.dat'
) FORMAT 'text' (delimiter ',' null '' )
ENCODING 'UTF8';
DROP EXTERNAL TABLE ext_table1;

\c regression

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
    CREATE TABLE test_drop_column_2(
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
    ) distributed by (col_with_constraint);

    insert into test_drop_column_2 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
    insert into test_drop_column_2 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
    insert into test_drop_column_2 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);
    insert into test_drop_column_2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i,i||'_'||repeat('text',3) from generate_series(3,500)i;

--drop toast column

    ALTER TABLE test_drop_column_2 DROP COLUMN toast_col ;

--drop non toast column

    ALTER TABLE test_drop_column_2 DROP COLUMN non_toast_col ;

--drop default

    ALTER TABLE test_drop_column_2 ALTER COLUMN col_with_default_text DROP DEFAULT;

    ALTER TABLE test_drop_column_2 ALTER COLUMN col_with_default_text SET DEFAULT 'test1';

    ALTER TABLE test_drop_column_2 ADD COLUMN added_col character varying(30);

    ALTER TABLE test_drop_column_2 RENAME COLUMN bigint_col TO after_rename_col; 

    ALTER TABLE test_drop_column_2 ALTER COLUMN numeric_col SET NOT NULL; 

ROLLBACK;

CREATE RESOURCE QUEUE db_resque1 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;
CREATE RESOURCE QUEUE db_resque2 COST THRESHOLD 3000.00 OVERCOMMIT;
CREATE RESOURCE QUEUE DB_RESque3 COST THRESHOLD 2000.0 NOOVERCOMMIT;
CREATE RESOURCE QUEUE DB_RESQUE4 ACTIVE THRESHOLD 2  IGNORE THRESHOLD  1000.0;

ALTER RESOURCE QUEUE db_resque1 ACTIVE THRESHOLD 3 COST THRESHOLD 1000.00;
ALTER RESOURCE QUEUE db_resque2 COST THRESHOLD 300.00 NOOVERCOMMIT;
ALTER RESOURCE QUEUE DB_RESque3 COST THRESHOLD 200.0 OVERCOMMIT;
ALTER RESOURCE QUEUE DB_RESQUE4 ACTIVE THRESHOLD 5  IGNORE THRESHOLD  500.0;

CREATE ROLE db_role1 WITH SUPERUSER CREATEDB  INHERIT LOGIN CONNECTION LIMIT  1 ENCRYPTED PASSWORD 'passwd';
CREATE ROLE db_role2 WITH NOSUPERUSER NOCREATEDB  NOINHERIT NOLOGIN  UNENCRYPTED PASSWORD 'passwd';
CREATE ROLE db_role3 WITH NOCREATEROLE NOCREATEUSER;
CREATE ROLE db_role4 WITH CREATEROLE CREATEUSER;
CREATE ROLE db_role5 WITH VALID UNTIL '2009-02-13 01:51:15';
CREATE ROLE db_role6 WITH IN ROLE db_role1; 
CREATE GROUP db_grp1;
CREATE ROLE db_role7 WITH IN GROUP db_grp1; 
CREATE ROLE db_role8 WITH ROLE db_role7;
CREATE ROLE db_role9 WITH ADMIN db_role8;
CREATE ROLE db_role10 WITH USER db_role1;
CREATE ROLE db_role11 SYSID 100 ;
CREATE RESOURCE QUEUE db_resque1 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;
CREATE ROLE db_role12 RESOURCE QUEUE db_resque1;


ALTER ROLE db_role1 WITH NOSUPERUSER NOCREATEDB  NOINHERIT NOLOGIN  UNENCRYPTED PASSWORD 'passwd';
ALTER ROLE db_role2 WITH SUPERUSER CREATEDB  INHERIT LOGIN CONNECTION LIMIT  1 ENCRYPTED PASSWORD 'passwd';
ALTER ROLE db_role3 WITH CREATEROLE CREATEUSER;
ALTER ROLE db_role4 WITH NOCREATEROLE NOCREATEUSER;
ALTER ROLE db_role5 WITH VALID UNTIL '2009-06-13 01:51:15';
ALTER ROLE db_role6 WITH CONNECTION LIMIT 5;
ALTER ROLE db_role7 WITH RESOURCE QUEUE db_resque1;
ALTER ROLE db_role8 RENAME TO new_role8; 
CREATE SCHEMA db_schema1;
ALTER ROLE db_role9 SET search_path TO db_schema1;
ALTER ROLE db_role9 RESET search_path ;

CREATE USER db_user1 WITH SUPERUSER CREATEDB  INHERIT LOGIN CONNECTION LIMIT  1 ENCRYPTED PASSWORD 'passwd';
CREATE USER db_user2 WITH NOSUPERUSER NOCREATEDB  NOINHERIT NOLOGIN  UNENCRYPTED PASSWORD 'passwd';
CREATE USER db_user3 WITH NOCREATEROLE NOCREATEUSER;
CREATE USER db_user4 WITH CREATEROLE CREATEUSER;
CREATE USER db_user5 WITH VALID UNTIL '2009-02-13 01:51:15';
CREATE USER db_user6 WITH IN ROLE db_role1; 
CREATE GROUP db_user_grp1;
CREATE USER db_user7 WITH IN GROUP db_user_grp1; 
CREATE USER db_user8 WITH ROLE db_user7;
CREATE USER db_user9 WITH ADMIN db_user8;
CREATE USER db_user10 WITH USER db_user1;
CREATE USER db_user11 SYSID 100 ;
CREATE RESOURCE QUEUE resqueu3 ACTIVE THRESHOLD 1;
CREATE USER db_user12 RESOURCE QUEUE resqueu3;


ALTER USER db_user1 WITH NOSUPERUSER NOCREATEDB  NOINHERIT NOLOGIN  UNENCRYPTED PASSWORD 'passwd';
ALTER USER db_user2 WITH SUPERUSER CREATEDB  INHERIT LOGIN CONNECTION LIMIT  1 ENCRYPTED PASSWORD 'passwd';
ALTER USER db_user3 WITH CREATEROLE CREATEUSER;
ALTER USER db_user4 WITH NOCREATEROLE NOCREATEUSER;
ALTER USER db_user5 WITH VALID UNTIL '2009-06-13 01:51:15';
ALTER USER db_user6 WITH CONNECTION LIMIT 5;
CREATE RESOURCE QUEUE resqueu4 ACTIVE THRESHOLD 1;
ALTER USER db_user7 WITH RESOURCE QUEUE resqueu4;
ALTER USER db_user8 RENAME TO new_user8; 
CREATE SCHEMA db_schema2;
ALTER USER db_user9 SET search_path TO db_schema2;
ALTER USER db_user9 RESET search_path ;

CREATE ROLE grp_role1;
CREATE ROLE grp_role2;
CREATE GROUP db_group1 WITH SUPERUSER CREATEDB  INHERIT LOGIN CONNECTION LIMIT  1 ENCRYPTED PASSWORD 'passwd';
CREATE GROUP db_grp2 WITH NOSUPERUSER NOCREATEDB  NOINHERIT NOLOGIN  UNENCRYPTED PASSWORD 'passwd';
CREATE GROUP db_grp3 WITH NOCREATEROLE NOCREATEUSER;
CREATE GROUP db_grp4 WITH CREATEROLE CREATEUSER;
CREATE GROUP db_grp5 WITH VALID UNTIL '2009-02-13 01:51:15';
CREATE GROUP db_grp6 WITH IN ROLE grp_role1; 
CREATE GROUP db_grp7 WITH IN GROUP db_group1; 
CREATE GROUP db_grp8 WITH ROLE grp_role2;
CREATE GROUP db_grp9 WITH ADMIN db_grp8;
CREATE GROUP db_grp10 WITH USER db_group1;
CREATE GROUP db_grp11 SYSID 100 ;
CREATE RESOURCE QUEUE grp_rsq1 ACTIVE THRESHOLD 1;
CREATE GROUP db_grp12 RESOURCE QUEUE grp_rsq1;


CREATE USER test_user_1;
ALTER GROUP db_grp12 ADD USER test_user_1;
ALTER GROUP db_grp12 DROP USER test_user_1;
ALTER GROUP db_grp12 RENAME TO new_db_grp12;
ALTER GROUP new_db_grp12 RENAME TO db_grp12;

CREATE TABLE test_emp (ename varchar(20),eno int,salary int,ssn int,gender char(1)) distributed by (ename,eno,gender);

CREATE UNIQUE INDEX eno_idx ON test_emp (eno);
CREATE INDEX gender_bmp_idx ON test_emp USING bitmap (gender);
CREATE INDEX lower_ename_idex  ON test_emp ((upper(ename))) WHERE upper(ename)='JIM';
CREATE  INDEX ename_idx ON test_emp  (ename) WITH (fillfactor =80);

ALTER INDEX gender_bmp_idx RENAME TO new_gender_bmp_idx;
ALTER INDEX ename_idx SET (fillfactor =100);
ALTER INDEX ename_idx RESET (fillfactor) ;

CREATE TEMPORARY SEQUENCE  db_seq1 START WITH 101;
CREATE TEMP SEQUENCE  db_seq2 START 101;
CREATE SEQUENCE db_seq3  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100;
CREATE SEQUENCE db_seq4  INCREMENT BY 2 NO MINVALUE  NO MAXVALUE ;
CREATE SEQUENCE db_seq5  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE db_seq6  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  NO CYCLE;
CREATE SEQUENCE db_seq7 START 101 OWNED BY NONE;
CREATE TABLE test_tbl ( col1 int, col2 int) DISTRIBUTED RANDOMLY;
INSERT INTO test_tbl values (generate_series(1,100),generate_series(1,100));
CREATE SEQUENCE db_seq8 START 101 OWNED BY test_tbl.col1;
ALTER TABLE test_tbl DROP COLUMN col1;

ALTER SEQUENCE db_seq1 RESTART WITH 100;
ALTER SEQUENCE db_seq2 INCREMENT BY 2 MINVALUE 101 MAXVALUE  400  CACHE 100 CYCLE;
ALTER SEQUENCE db_seq3  INCREMENT BY 2 NO MINVALUE  NO MAXVALUE;
ALTER SEQUENCE db_seq4 INCREMENT BY 2 MINVALUE 1 MAXVALUE  100;
ALTER SEQUENCE db_seq5 NO CYCLE;
CREATE SCHEMA db_schema9;
ALTER SEQUENCE db_seq6 SET SCHEMA db_schema9;
ALTER SEQUENCE db_seq7  OWNED BY test_tbl.col2;
ALTER SEQUENCE db_seq7  OWNED BY NONE;


-- MPP-8466: set this GUC so that we can create database with latin8 encoding
-- 20100412: Ngoc
-- SET gp_check_locale_encoding_compatibility = off;
-- 20100414: Ngoc: GUC is not ported into 4.0 => remove ENCODING='latin8'

CREATE ROLE db_owner1;
CREATE ROLE db_owner2;

CREATE DATABASE db_name1 WITH OWNER = db_owner1 TEMPLATE =template1 CONNECTION LIMIT= 2;
--CREATE DATABASE db_name1 WITH OWNER = db_owner1 TEMPLATE =template1 ENCODING='latin8'  CONNECTION LIMIT= 2;
ALTER DATABASE db_name1 WITH  CONNECTION LIMIT 3;
ALTER DATABASE db_name1  RENAME TO new_db_name1;
ALTER DATABASE new_db_name1  OWNER TO db_owner2;
ALTER DATABASE new_db_name1 RENAME TO db_name1;

CREATE SCHEMA myschema;
ALTER DATABASE db_name1 SET search_path TO myschema, public, pg_catalog;
ALTER DATABASE db_name1 RESET search_path;

CREATE USER db_user13;
CREATE DATABASE db_schema_test owner db_user13;
\c db_schema_test
CREATE SCHEMA db_schema5 AUTHORIZATION db_user13 ;
CREATE SCHEMA AUTHORIZATION db_user13;

ALTER SCHEMA db_user13 RENAME TO db_schema6;
ALTER SCHEMA  db_schema6 OWNER TO db_user13;

\c regression

CREATE DOMAIN domain_us_zip_code AS TEXT CHECK ( VALUE ~ E'\\d{5}$' OR VALUE ~ E'\\d{5}-\\d{4}$');
CREATE DOMAIN domain_1 AS int DEFAULT 1 CONSTRAINT cons_not_null NOT NULL;
CREATE DOMAIN domain_2 AS int CONSTRAINT cons_null NULL;
CREATE DOMAIN domain_3 AS TEXT ;

CREATE ROLE domain_owner;
CREATE SCHEMA domain_schema;

ALTER DOMAIN domain_3 ADD CONSTRAINT  domain_constraint3 CHECK (char_length(VALUE) = 5) ;
ALTER DOMAIN domain_3 DROP CONSTRAINT  domain_constraint3 RESTRICT;
ALTER DOMAIN domain_3 ADD CONSTRAINT  domain_constraint3 CHECK (char_length(VALUE) = 5);
ALTER DOMAIN domain_3 DROP CONSTRAINT domain_constraint3 CASCADE;
ALTER DOMAIN domain_3 OWNER TO domain_owner;
ALTER DOMAIN domain_3 SET SCHEMA domain_schema;

CREATE OR REPLACE FUNCTION add(integer, integer) RETURNS integer 
    AS 'select $1 + $2;' 
    LANGUAGE SQL CONTAINS SQL
    STABLE 
    RETURNS NULL ON NULL INPUT; 


CREATE FUNCTION dup(in int, out f1 int, out f2 text) 
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$ 
    LANGUAGE SQL IMMUTABLE STRICT CONTAINS SQL;

CREATE FUNCTION dup1(in int, out f1 int, out f2 text) 
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$ 
    LANGUAGE SQL IMMUTABLE CONTAINS SQL CALLED ON NULL INPUT  EXTERNAL  SECURITY INVOKER  ;

CREATE FUNCTION dup2(in int, out f1 int, out f2 text) 
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$ 
    LANGUAGE SQL IMMUTABLE CONTAINS SQL CALLED ON NULL INPUT  EXTERNAL  SECURITY DEFINER  ;

CREATE FUNCTION dup3(in int, out f1 int, out f2 text) 
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$ 
    LANGUAGE SQL IMMUTABLE CONTAINS SQL CALLED ON NULL INPUT  SECURITY INVOKER  ;

CREATE FUNCTION dup4(in int, out f1 int, out f2 text) 
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$ 
    LANGUAGE SQL IMMUTABLE CONTAINS SQL CALLED ON NULL INPUT  SECURITY DEFINER  ;

CREATE ROLE func_role;
CREATE SCHEMA func_schema;

ALTER FUNCTION add(integer, integer) OWNER TO func_role;
ALTER FUNCTION add(integer, integer)  SET SCHEMA func_schema;
set search_path to func_schema;
ALTER FUNCTION add(integer, integer)  SET SCHEMA public;
set search_path to public;
ALTER FUNCTION add(integer, integer)   RENAME TO new_add;
ALTER FUNCTION new_add(integer, integer)   RENAME TO add;
ALTER FUNCTION dup(in int, out f1 int, out f2 text) RETURNS NULL ON NULL INPUT STABLE EXTERNAL SECURITY INVOKER;
ALTER FUNCTION dup(in int, out f1 int, out f2 text) CALLED ON NULL INPUT VOLATILE EXTERNAL SECURITY DEFINER;
ALTER FUNCTION dup(in int, out f1 int, out f2 text) IMMUTABLE STRICT SECURITY INVOKER;
ALTER FUNCTION dup(in int, out f1 int, out f2 text) IMMUTABLE STRICT SECURITY DEFINER;

CREATE FUNCTION scube_accum(numeric, numeric) RETURNS 
numeric 
    AS 'select $1 + $2 * $2 * $2' 
    LANGUAGE SQL CONTAINS SQL
    IMMUTABLE 
    RETURNS NULL ON NULL INPUT; 

CREATE FUNCTION pre_accum(numeric, numeric) RETURNS 
numeric 
    AS 'select $1 + $2 * $2 * $2 * $2' 
    LANGUAGE SQL CONTAINS SQL
    IMMUTABLE 
    RETURNS NULL ON NULL INPUT; 


CREATE FUNCTION final_accum(numeric) RETURNS 
numeric 
    AS 'select $1 + $1 * $1' 
    LANGUAGE SQL CONTAINS SQL
    IMMUTABLE 
    RETURNS NULL ON NULL INPUT; 


CREATE AGGREGATE scube(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 


CREATE AGGREGATE old_scube ( 
	BASETYPE = numeric,
    SFUNC = scube_accum, 
    STYPE = numeric, 
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 

CREATE ROLE agg_owner;
CREATE SCHEMA agg_schema;

ALTER AGGREGATE scube(numeric) RENAME TO new_scube;
ALTER AGGREGATE new_scube(numeric) RENAME TO scube;
ALTER AGGREGATE scube(numeric) OWNER TO agg_owner;
ALTER AGGREGATE scube(numeric) SET SCHEMA agg_schema;

CREATE ROLE sally;
CREATE ROLE ron;
CREATE ROLE ken;
CREATE ROLE admin;

CREATE TABLE foo1 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo1 OWNER TO sally;
CREATE TABLE foo2 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo2 OWNER TO ron;
CREATE TABLE foo3 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo3 OWNER TO ken;

DROP OWNED by sally CASCADE;
DROP OWNED by ron RESTRICT;
DROP OWNED by ken;

CREATE TABLE foo1 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo1 OWNER TO sally;
CREATE TABLE foo2 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo2 OWNER TO ron;
CREATE TABLE foo3 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo3 OWNER TO ken;

REASSIGN OWNED BY sally,ron,ken to admin;
    CREATE TABLE test_table(
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

    insert into test_table values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
    insert into test_table values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
    insert into test_table values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);
    insert into test_table select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i from generate_series(3,100)i;

    CREATE TABLE test_table1(
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

CREATE TABLE err_table1 (cmdtime timestamp with time zone, relname text, filename text, linenum integer, bytenum integer, errmsg text, rawdata text, rawbytes bytea) DISTRIBUTED RANDOMLY;    

    insert into test_table1 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
    insert into test_table1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
    insert into test_table1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);
    insert into test_table1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i from generate_series(3,100)i;


    CREATE TABLE test_table3(
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
    col_set_default numeric) WITH OIDS DISTRIBUTED RANDOMLY;
    insert into test_table3 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
    insert into test_table3 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
    insert into test_table3 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);
    insert into test_table3 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i from generate_series(3,100)i;
COPY (select * from test_table3) TO 'data/test1_file_copy' WITH DELIMITER AS ',' NULL AS 'null string' ESCAPE AS E'\n' CSV HEADER QUOTE AS '"' FORCE QUOTE char_vary_col;


COPY (select * from test_table3) TO 'data/test1_file_copy' WITH HEADER DELIMITER AS ',' NULL AS 'null string' ESCAPE AS E'\n' CSV QUOTE AS '"' FORCE QUOTE char_vary_col;


COPY test_table3(   text_col,    bigint_col,    char_vary_col,    numeric_col,    int_col,    float_col,    int_array_col,    before_rename_col,    change_datatype_col,    a_ts_without ,    b_ts_with ,    date_column,    col_set_default)
 TO 'data/test2_file_copy' WITH OIDS  DELIMITER AS ','  NULL AS 'null string' ESCAPE AS 'OFF' ;


COPY test_table3 (   text_col,    bigint_col,    char_vary_col,    numeric_col,    int_col,    float_col,    int_array_col,    before_rename_col,    change_datatype_col,    a_ts_without ,    b_ts_with ,    date_column,    col_set_default) FROM 'data/test1_file_copy' WITH DELIMITER AS ',' NULL AS 'null string' ESCAPE AS E'\n' CSV HEADER QUOTE AS '"' FORCE NOT NULL date_column LOG ERRORS SEGMENT REJECT LIMIT 10 ROWS  ;

COPY test_table3 (   text_col,    bigint_col,    char_vary_col,    numeric_col,    int_col,    float_col,    int_array_col,    before_rename_col,    change_datatype_col,    a_ts_without ,    b_ts_with ,    date_column,    col_set_default) FROM 'data/test2_file_copy' WITH OIDS DELIMITER AS ',' NULL AS 'null string' ESCAPE AS 'OFF' LOG ERRORS SEGMENT REJECT LIMIT 10 PERCENT  ;


CREATE OR REPLACE TEMP VIEW vista AS SELECT 'Hello World'; 
CREATE TEMPORARY VIEW vista1 AS SELECT text 'Hello World' AS hello;
CREATE TABLE test_emp_view (ename varchar(20),eno int,salary int,ssn int,gender char(1)) distributed by (ename,eno,gender);

CREATE VIEW emp_view AS SELECT ename ,eno ,salary ,ssn FROM test_emp_view WHERE salary >5000;

--Create the tables and views

CREATE TABLE sch_tbint (
    rnum integer NOT NULL,
    cbint bigint
) DISTRIBUTED BY (rnum);
COMMENT ON TABLE sch_tbint IS 'This describes table SCH_TBINT.';

CREATE TABLE sch_tchar (
    rnum integer NOT NULL,
    cchar character(32)
) DISTRIBUTED BY (rnum);
COMMENT ON TABLE sch_tchar IS 'This describes table SCH_TCHAR.';

CREATE TABLE sch_tclob (
    rnum integer NOT NULL,
    cclob text
) DISTRIBUTED BY (rnum);
COMMENT ON TABLE sch_tclob IS 'This describes table SCH_TCLOB.';

CREATE VIEW sch_vbint AS
    SELECT tbint.rnum, tbint.cbint FROM sch_tbint as tbint;
COMMENT ON VIEW sch_vbint IS 'This describes view SCH_VBINT.';

CREATE VIEW sch_vchar AS
    SELECT tchar.rnum, tchar.cchar FROM sch_tchar as tchar;
COMMENT ON VIEW sch_vchar IS 'This describes view SCH_VCHAR.';

CREATE VIEW sch_vclob AS
    SELECT tclob.rnum, tclob.cclob FROM sch_tclob as tclob;
COMMENT ON VIEW sch_vclob IS 'This describes view SCH_VCLOB.';


CREATE TABLE sch_tversion (
    rnum integer NOT NULL,
    c1 integer,
    cver character(6),
    cnnull integer,
    ccnull character(1)
) DISTRIBUTED BY (rnum);

COPY sch_tversion (rnum, c1, cver, cnnull, ccnull) FROM stdin;
0	1	1.0   	\N	\N
\.

CREATE TABLE sch_tjoin2 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(2)
) DISTRIBUTED BY (rnum);

COPY sch_tjoin2 (rnum, c1, c2) FROM stdin;
3	10	FF
0	10	BB
1	15	DD
2	\N	EE
\.

create view sch_view1001 as 
select 10 f1, 'BB' f2 from sch_tversion union
select sch_tjoin2.c1, sch_tjoin2.c2 from sch_tjoin2 where sch_tjoin2.c2 like '_B';


--Views with deep sliced queries

create table sch_T43(	C487 int,	C488 int,	C489 int,	C490 int,	C491 int,	C492 int,	C493 int,	C494 int,	C495 int,	C496 int,	C497 int,	C498 int,	C499 int,	C500 int,	C501 int) WITH (compresstype=zlib,compresslevel=1,appendonly=true,blocksize=1327104,checksum=true);

COPY sch_T43 FROM STDIN delimiter '|';
5|3|4|1|3|2|1|2|5|2|4|3|2|5|1
4|1|2|5|4|5|3|5|4|3|4|2|5|3|1
5|3|3|1|5|3|2|1|5|5|3|5|1|4|4
3|2|1|4|3|3|5|1|1|3|3|5|4|5|4
2|3|4|4|5|3|3|3|3|4|2|1|5|3|4
3|5|5|3|4|1|4|4|4|5|2|3|1|3|1
5|5|3|4|3|1|2|2|5|2|4|2|1|5|3
2|5|2|3|1|1|2|4|5|4|2|1|3|5|1
2|4|3|3|1|1|3|1|5|4|5|1|2|5|1
3|3|3|4|1|1|2|3|2|5|4|2|5|1|3
\.


create table sch_T33(	C383 int,	C384 int,	C385 int,	C386 int,	C387 int,	C388 int,	C389 int,	C390 int,	C391 int,	C392 int,	C393 int,	C394 int,	C395 int,	C396 int,	C397 int,	C398 int,	C399 int,	C400 int,	C401 int) WITH (compresstype=zlib,blocksize=548864,appendonly=true,compresslevel=2,checksum=true);

INSERT INTO sch_T33 VALUES ( 5, 1, 3, 1, 5, 2, 4, 2, 2, 2, 4, 5, 3, 4, 1, 4, 2, 1, 3 );

INSERT INTO sch_T33 VALUES ( 2, 2, 1, 5, 2, 4, 3, 4, 5, 5, 2, 4, 2, 4, 2, 1, 2, 3, 5 );

INSERT INTO sch_T33 VALUES ( 1, 3, 2, 3, 5, 3, 2, 2, 5, 5, 5, 1, 4, 1, 5, 2, 4, 2, 4 );

INSERT INTO sch_T33 VALUES ( 1, 1, 4, 3, 1, 5, 1, 2, 1, 1, 3, 2, 4, 3, 5, 1, 1, 2, 2 );

INSERT INTO sch_T33 VALUES ( 1, 4, 1, 2, 5, 2, 5, 1, 4, 2, 3, 5, 3, 2, 3, 3, 2, 2, 4 );

INSERT INTO sch_T33 VALUES ( 4, 2, 5, 3, 4, 4, 3, 2, 1, 2, 1, 3, 3, 3, 5, 4, 2, 1, 4 );

INSERT INTO sch_T33 VALUES ( 2, 3, 1, 5, 2, 2, 3, 4, 2, 5, 2, 3, 4, 2, 4, 4, 5, 5, 3 );

INSERT INTO sch_T33 VALUES ( 3, 1, 2, 2, 2, 5, 3, 3, 3, 5, 3, 2, 2, 4, 3, 5, 3, 4, 1 );

INSERT INTO sch_T33 VALUES ( 2, 5, 5, 3, 1, 2, 4, 3, 3, 4, 1, 4, 3, 2, 5, 3, 2, 1, 1 );

INSERT INTO sch_T33 VALUES ( 1, 2, 5, 1, 2, 2, 4, 5, 2, 1, 2, 2, 3, 5, 3, 5, 5, 1, 2 );


create table sch_T29(	C334 int,	C335 int,	C336 int,	C337 int,	C338 int,	C339 int,	C340 int) WITH (compresstype=zlib,checksum=true,appendonly=true,blocksize=1155072,compresslevel=1);

INSERT INTO sch_T29 VALUES ( 5, 1, 1, 3, 1, 2, 4 );

INSERT INTO sch_T29 VALUES ( 5, 5, 4, 3, 3, 3, 2 );

INSERT INTO sch_T29 VALUES ( 5, 1, 3, 2, 1, 5, 4 );

INSERT INTO sch_T29 VALUES ( 4, 1, 4, 4, 3, 2, 3 );

INSERT INTO sch_T29 VALUES ( 3, 4, 1, 5, 3, 3, 3 );

INSERT INTO sch_T29 VALUES ( 2, 5, 4, 3, 5, 3, 4 );

INSERT INTO sch_T29 VALUES ( 4, 5, 1, 1, 1, 3, 2 );

INSERT INTO sch_T29 VALUES ( 4, 5, 2, 1, 5, 3, 4 );

INSERT INTO sch_T29 VALUES ( 4, 5, 2, 1, 5, 3, 1 );

INSERT INTO sch_T29 VALUES ( 1, 2, 2, 2, 4, 3, 3 );



CREATE VIEW sch_view_ao (col1,col2,col3,col4,col5,col6) AS
SELECT
	DT102.C339
	, DT102.C340
	, DT100.C498
	, SUM( DT102.C339 )
	, SUM( DT102.C340 )
	, COUNT( DT100.C498 )
FROM
	(
		(
			sch_T43 DT100
		INNER JOIN
			sch_T33 DT101
		ON
			DT100.C498 = DT101.C401
		)
	INNER JOIN
		sch_T29 DT102
	ON
		DT101.C385 = DT102.C338
	)
WHERE
	(
		(
			DT101.C390 = DT100.C500
		)
		OR
		(
			(
				DT102.C338 <> DT101.C388
			)
			AND
			(
				DT102.C336 < DT100.C494
			)
		)
	)
	OR
	(
		(
			DT101.C391 < DT101.C399
		)
		AND
		(
			DT102.C335 = DT100.C490
		)
	)
GROUP BY
	DT100.C498
	, DT102.C339
	, DT102.C340
ORDER BY
	DT102.C339
	, DT102.C340
	, DT100.C498
	, SUM( DT102.C339 )
	, SUM( DT102.C340 )
	, COUNT( DT100.C498 )
LIMIT 497;

SELECT count(*) FROM sch_view_ao;

--Create view from set_returning_functions
create view sch_srf_view1 as select * from generate_series(1,10);

Select count(*) from sch_srf_view1;

--Create view from functions

CREATE FUNCTION sch_multiply(value integer,times integer) RETURNS integer AS $$
DECLARE
    res integer;
BEGIN
    RETURN $1*$2;
END;
 
$$ LANGUAGE plpgsql NO SQL;

create view sch_fn_view2 as select  sch_multiply(4,5);

Select * from sch_fn_view2;

CREATE OR REPLACE FUNCTION int4(boolean)
  RETURNS int4 AS
$BODY$

SELECT CASE WHEN $1 THEN 1 ELSE 0 END;

$BODY$
  LANGUAGE 'sql' IMMUTABLE CONTAINS SQL;

CREATE CAST (boolean AS int4) WITH FUNCTION int4(boolean) AS ASSIGNMENT;

CREATE CAST (varchar AS text) WITHOUT FUNCTION AS IMPLICIT;

-- Heap Table 

CREATE TABLE retail_heap (
    datacenter character varying(32),
    poolname character varying(256),
    machinename character varying(256),
    transactionid character varying(32),
    threadid integer,
    transactionorder integer,
    eventclass character(1),
    eventtime timestamp(2) without time zone,
    eventtype character varying(256),
    eventname character varying(256),
    status character varying(256),
    duration numeric(18,2),
    data character varying(4096),
    value int,
    test text
) distributed randomly;

-- Insert values into the Heap Table

insert into retail_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,276,'A','2008-11-08 00:01:10.27','EXEC','2250293824','0',39.00,'userlookupreadhost',1,'value_1');
insert into retail_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,277,'A','2008-11-08 00:01:10.30','EXEC','3722270207','0',39.00,'userwrite17host',2,'value_2');
insert into retail_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,278,'A','2008-11-08 00:01:10.34','EXEC','2098551587','0',40.00,'userwrite17host',3,'value_3');
insert into retail_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,279,'A','2008-11-08 00:01:10.39','EXEC','1143318638','0',39.00,'userwrite17host',4,'value_4');
insert into retail_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,280,'A','2008-11-08 00:01:10.42','EXEC','1143318638','0',35.00,'userwrite17host',5,'value_5');
insert into retail_heap values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,280,'A','2008-11-08 00:01:10.42','EXEC','1143318638','0',35.00,'userwrite17host',generate_series(6,100),repeat('text',100));


-- Select from the Heap Table

select count(*) from retail_heap ;

-- AO Table with zlib compression level 1

CREATE TABLE retail_zlib_1 (
    datacenter character varying(32),
    poolname character varying(256),
    machinename character varying(256),
    transactionid character varying(32),
    threadid integer,
    transactionorder integer,
    eventclass character(1),
    eventtime timestamp(2) without time zone,
    eventtype character varying(256),
    eventname character varying(256),
    status character varying(256),
    duration numeric(18,2),
    data character varying(4096),
    value int,
    test text
) WITH (appendonly=true, compresslevel=1, compresstype=zlib) distributed by (transactionid);

-- Insert values into the AO zlib 1 table

insert into retail_zlib_1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,276,'A','2008-11-08 00:01:10.27','EXEC','2250293824','0',39.00,'userlookupreadhost',1,'value_1');
insert into retail_zlib_1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,277,'A','2008-11-08 00:01:10.30','EXEC','3722270207','0',39.00,'userwrite17host',2,'value_2');
insert into retail_zlib_1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,278,'A','2008-11-08 00:01:10.34','EXEC','2098551587','0',40.00,'userwrite17host',3,'value_3');
insert into retail_zlib_1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,279,'A','2008-11-08 00:01:10.39','EXEC','1143318638','0',39.00,'userwrite17host',4,'value_4');
insert into retail_zlib_1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,279,'A','2008-11-08 00:01:10.39','EXEC','1143318638','0',39.00,'userwrite17host',generate_series(5,100),repeat('text',100));

-- Select from AO zlib 3 table

select count(*) from retail_zlib_1 ;

-- AO Table with zlib compression level 1

CREATE TABLE retail_zlib1 (
    datacenter character varying(32),
    poolname character varying(256),
    machinename character varying(256),
    transactionid character varying(32),
    threadid integer,
    transactionorder integer,
    eventclass character(1),
    eventtime timestamp(2) without time zone,
    eventtype character varying(256),
    eventname character varying(256),
    status character varying(256),
    duration numeric(18,2),
    data character varying(4096),
    value int,
    test text
) WITH (appendonly=true, compresslevel=1, compresstype=zlib) distributed by (transactionid); 

-- Insert values into the AO zlib 1 table 

insert into retail_zlib1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,276,'A','2008-11-08 00:01:10.27','EXEC','2250293824','0',39.00,'userlookupreadhost',1,'value_1');
insert into retail_zlib1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,277,'A','2008-11-08 00:01:10.30','EXEC','3722270207','0',39.00,'userwrite17host',2,'value_2');
insert into retail_zlib1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,278,'A','2008-11-08 00:01:10.34','EXEC','2098551587','0',40.00,'userwrite17host',3,'value_3');
insert into retail_zlib1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,279,'A','2008-11-08 00:01:10.39','EXEC','1143318638','0',39.00,'userwrite17host',4,'value_4');
insert into retail_zlib1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,280,'A','2008-11-08 00:01:10.42','EXEC','1143318638','0',35.00,'userwrite17host',5,'value_5');
insert into retail_zlib1 values ('den02','v3listingsintl','hr-wlst012','491539361b0000035e5be7d916',173,280,'A','2008-11-08 00:01:10.42','EXEC','1143318638','0',35.00,'userwrite17host',generate_series(6,100),repeat('text',100));

-- Select from AO zlib 1 table

select count(*) from retail_zlib1 ;

-- Adding checkpoint
checkpoint;


CREATE TABLE heap_column_with_32k(
text_col1 text,
text_col2 text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) distributed randomly;

insert into heap_column_with_32k values ('0_zero','0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into heap_column_with_32k values ('1_zero','1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into heap_column_with_32k values ('2_zero','2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into heap_column_with_32k select i||'_'||repeat('text',30000),i||'_'||repeat('text',30000),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,1000)i;

CREATE TABLE ao_column_with_32k(
text_col1 text,
text_col2 text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) with (appendonly=true) distributed randomly;

insert into ao_column_with_32k values ('0_zero','0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into ao_column_with_32k values ('1_zero','1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into ao_column_with_32k values ('2_zero','2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into ao_column_with_32k select i||'_'||repeat('text',30000),i||'_'||repeat('text',30000),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,1000)i;

CREATE TABLE co_column_with_32k(
text_col1 text,
text_col2 text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) with (orientation='column',appendonly=true) distributed randomly;

insert into co_column_with_32k values ('0_zero','0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into co_column_with_32k values ('1_zero','1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into co_column_with_32k values ('2_zero','2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into co_column_with_32k select i||'_'||repeat('text',30000),i||'_'||repeat('text',30000),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,1000)i;

CREATE TABLE heap_table_unique_index(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date)
distributed by (text_col);

CREATE UNIQUE INDEX heap_table_unq_idx ON heap_table_unique_index (numeric_col);

insert into heap_table_unique_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into heap_table_unique_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into heap_table_unique_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into heap_table_unique_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE heap_table_btree_index(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) ;

CREATE INDEX heap_table_idx ON heap_table_btree_index (numeric_col);

insert into heap_table_btree_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into heap_table_btree_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into heap_table_btree_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into heap_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE heap_table_bitmap_index(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) ;

CREATE INDEX numeric_col_bm_idx ON heap_table_bitmap_index USING bitmap (numeric_col);;

insert into heap_table_bitmap_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into heap_table_bitmap_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into heap_table_bitmap_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into heap_table_bitmap_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE AO_table_btree_index(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) WITH (appendonly=true);

CREATE INDEX AO_table_idx ON AO_table_btree_index USING bitmap (numeric_col);

insert into AO_table_btree_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into AO_table_btree_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into AO_table_btree_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into AO_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE AO_table_bitmap_index(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) WITH (appendonly=true);

CREATE INDEX ao_numeric_col_bm_idx ON AO_table_bitmap_index USING bitmap (numeric_col);

insert into AO_table_bitmap_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into AO_table_bitmap_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into AO_table_bitmap_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into AO_table_bitmap_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE CO_table_btree_index(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) WITH (orientation='column', appendonly=true);

CREATE INDEX CO_table_idx ON CO_table_btree_index (numeric_col);

insert into CO_table_btree_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into CO_table_btree_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into CO_table_btree_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into CO_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE CO_table_bitmap_index(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) WITH (orientation='column', appendonly=true);

CREATE INDEX co_numeric_col_bm_idx ON CO_table_bitmap_index USING bitmap (numeric_col);

insert into CO_table_bitmap_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into CO_table_bitmap_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into CO_table_bitmap_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into CO_table_bitmap_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

-- Create schema and set to it   
   Create Schema oagg;
   SET search_path to oagg;

-- Create quantity table
   Drop table IF EXISTS sch_quantity;
   create table sch_quantity ( prod_key integer, qty integer, price integer, product character(3));
-- inserting values into sch_quantity table

   insert into sch_quantity values (1,100, 50, 'p1');
   insert into sch_quantity values (2,200, 100, 'p2');
   insert into sch_quantity values (3,300, 200, 'p3');
   insert into sch_quantity values (4,400, 35, 'p4');
   insert into sch_quantity values (5,500, 40, 'p5');
   insert into sch_quantity values (1,150, 50, 'p1');
   insert into sch_quantity values (2,50, 100, 'p2');
   insert into sch_quantity values (3,150, 200, 'p3');
   insert into sch_quantity values (4,200, 35, 'p4');
   insert into sch_quantity values (5,300, 40, 'p5');

-- Create a new ordered aggregate with initial condition and finalfunc
   CREATE ORDERED AGGREGATE sch_array_accum_final (anyelement)
   (
      sfunc = array_append,
      stype = anyarray,
      finalfunc = array_out,
      initcond = '{}'
   );

-- Using newly created ordered aggregate array_accum_final
   select prod_key, sch_array_accum_final(qty order by prod_key,qty) from sch_quantity group by prod_key having prod_key < 5 order by prod_key;

-- rename ordered aggregate
alter aggregate sch_array_accum_final (anyelement) rename to sch_array_accum_final_new;

-- check ordered aggregate
\df sch_array_accum_final_new;

-- drop ordered aggregate
drop aggregate if exists sch_array_accum_final_new(anyelement);

-- Drop Scehma
   Drop schema oagg CASCADE;
   SET search_path to public;
-- Drop table if exists

--Drop table 
DROP table if exists co_compr_part_1 cascade;
DROP table if exists exch_part2_compr_1 cascade;

--
-- Create table
--

CREATE TABLE co_compr_part_1
        (id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,
        a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )  WITH (appendonly=true, orientation=column) distributed randomly
 Partition by list(a2) Subpartition by range(a1) subpartition template (default subpartition df_sp, start(1)  end(5000) every(1000),
 COLUMN a2  ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
 COLUMN a1 encoding (compresstype = rle_type,compresslevel=1),
 COLUMN a3 encoding (compresstype = rle_type,compresslevel=3),
 COLUMN a5 ENCODING (compresstype=zlib,compresslevel=3, blocksize=32768),
 DEFAULT COLUMN ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)) (partition p1 values('F'), partition p2 values ('M'));


-- 
-- Create Indexes # commenting create indexes due to MPP-14747 Restore fails with duplicate index errors for subpartition level indexes
--
-- CREATE INDEX co_compr_part_1_idx_bitmap ON co_compr_part_1 USING bitmap (a1);

-- CREATE INDEX co_compr_part_1_idx_btree ON co_compr_part_1(a9);

--
--Insert data to the table
--
set LC_MONETARY='C';
INSERT INTO co_compr_part_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23);

INSERT INTO co_compr_part_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44);



--Alter table Add Partition 
ALTER TABLE co_compr_part_1 add partition new_p values('C') ;

ALTER TABLE co_compr_part_1 add default partition df_p ;

---- Create the tables to do the exchange partition--

CREATE TABLE exch_part2_compr_1 (id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly;

INSERT INTO  exch_part2_compr_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_part_1 where  a1=10;


--Alter table exchange partition
ALTER TABLE co_compr_part_1 alter partition p2 exchange partition FOR (RANK(1)) with table exch_part2_compr_1 ;

--Alter table split partition
ALTER TABLE co_compr_part_1 alter partition p1 split partition FOR (RANK(1)) at(505) into (partition splita_1,partition splitb_1) ;

--Alter table Drop partition
ALTER TABLE co_compr_part_1 drop partition new_p;
ALTER TABLE co_compr_part_1 drop default partition;

INSERT INTO co_compr_part_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_part_1 where  a1=500;

SELECT count(*) from co_compr_part_1;


-- Partition by range

--Drop table if exists

DROP table if exists co_compr_part_2 cascade;
DROP table if exists  exch_part2_compr_2 cascade;



--
-- Create table
--
CREATE TABLE co_compr_part_2
        (id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )  WITH (appendonly=true, orientation=column) distributed randomly
 Partition by range(a1) Subpartition by list(a2) subpartition template ( default subpartition df_sp, subpartition sp1 values('M') , subpartition sp2 values('F')  ,
 COLUMN a2  ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768),
 COLUMN a1 encoding (compresstype = rle_type,compresslevel=2),
 COLUMN a5 ENCODING (compresstype=zlib,compresslevel=1, blocksize=32768),
 DEFAULT COLUMN ENCODING (compresstype=rle_type,compresslevel=4,blocksize=32768)) (start(1) end(5000) every(1000));

-- 
-- Create Indexes
--
-- CREATE INDEX co_compr_part_2_idx_bitmap ON co_compr_part_2 USING bitmap (a1);

-- CREATE INDEX co_compr_part_2_idx_btree ON co_compr_part_2(a9);


--
-- Insert data to the table
--
 INSERT INTO co_compr_part_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23);


 INSERT INTO co_compr_part_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44);




\d+ co_compr_part_2_1_prt_1_2_prt_sp1


--Select from pg_attribute_encoding to see the table entry 
select parencattnum, parencattoptions from pg_partition_encoding e, pg_partition p, pg_class c  where c.relname = 'co_compr_part_2' and c.oid = p.parrelid and p.oid = e.parencoid order by parencattnum limit 3;


--Alter table Add Partition 
alter table co_compr_part_2 add partition new_p start(5050) end (5061);

--Validation with psql utility 
  \d+ co_compr_part_2_1_prt_new_p_2_prt_sp1

alter table co_compr_part_2 add default partition df_p ;


--Validation with psql utility 
  \d+ co_compr_part_2_1_prt_df_p_2_prt_sp1


-- Insert data 
Insert into co_compr_part_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(5051,5060),'C',2011,'t','a','dfjjjjjj','2001-12-24 02:26:11','hghgh',333,'2011-10-11','Tddd','sss','1234.56',323453,4454,7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','dfdf','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','ggg','1','0',12,23) ;


 CREATE TABLE exch_part2_compr_2 (id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly;


Insert into exch_part2_compr_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_part_2 where  a1=10;

Alter table co_compr_part_2 alter partition FOR (RANK(1)) exchange partition sp1 with table exch_part2_compr_2 ;


--Alter table Drop Partition 
alter table co_compr_part_2 drop partition new_p;

-- Drop the default partition 
alter table co_compr_part_2 drop default partition;


--Alter table alter type of a column 
Alter table co_compr_part_2 Alter column a3 TYPE int4;
--Insert data to the table, select count(*)
Insert into co_compr_part_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_part_2 where id =10;
Select count(*) from co_compr_part_2;

--Alter table drop a column 
Alter table co_compr_part_2 Drop column a12;
Insert into co_compr_part_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_part_2 where id =10;
Select count(*) from co_compr_part_2;

--Alter table rename a column 
Alter table co_compr_part_2 Rename column a13 TO after_rename_a13;
--Insert data to the table, select count(*)
Insert into co_compr_part_2(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_part_2 where id =10;
Select count(*) from co_compr_part_2;


DROP type if exists int_type1 cascade ; 

CREATE type int_type1;
CREATE FUNCTION int_type1_in(cstring) 
 RETURNS int_type1
 AS 'int4in' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE FUNCTION int_type1_out(int_type1) 
 RETURNS cstring
 AS 'int4out' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE TYPE int_type1( 
 input = int_type1_in ,
 output = int_type1_out ,
 internallength = 4, 
 default =55, 
 passedbyvalue, 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=3);

--Drop and recreate the data type 

 Drop type if exists int_type1 cascade;

CREATE FUNCTION int_type1_in(cstring) 
 RETURNS int_type1
 AS 'int4in' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE FUNCTION int_type1_out(int_type1) 
 RETURNS cstring
 AS 'int4out' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE TYPE int_type1( 
 input = int_type1_in ,
 output = int_type1_out ,
 internallength = 4, 
 default =55, 
 passedbyvalue, 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=2);


select typoptions from pg_type_encoding where typid='int_type1 '::regtype;

DROP type if exists char_type1 cascade ; 

CREATE type char_type1;
CREATE FUNCTION char_type1_in(cstring) 
 RETURNS char_type1
 AS 'charin' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE FUNCTION char_type1_out(char_type1) 
 RETURNS cstring
 AS 'charout' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE TYPE char_type1( 
 input = char_type1_in ,
 output = char_type1_out ,
 internallength = 4, 
 default = 'asd' , 
 passedbyvalue, 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists char_type1 cascade;

CREATE FUNCTION char_type1_in(cstring) 
 RETURNS char_type1
 AS 'charin' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE FUNCTION char_type1_out(char_type1) 
 RETURNS cstring
 AS 'charout' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE TYPE char_type1( 
 input = char_type1_in ,
 output = char_type1_out ,
 internallength = 4, 
 default = 'asd' , 
 passedbyvalue, 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=2);


select typoptions from pg_type_encoding where typid='char_type1 '::regtype;

DROP type if exists text_type1 cascade ; 

CREATE type text_type1;
CREATE FUNCTION text_type1_in(cstring) 
 RETURNS text_type1
 AS 'textin' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE FUNCTION text_type1_out(text_type1) 
 RETURNS cstring
 AS 'textout' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE TYPE text_type1( 
 input = text_type1_in ,
 output = text_type1_out ,
 internallength = variable, 
 default = 'hfkdshfkjsdhflkshadfkhsadflkh' , 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=2);

--Drop and recreate the data type 

 Drop type if exists text_type1 cascade;

CREATE FUNCTION text_type1_in(cstring) 
 RETURNS text_type1
 AS 'textin' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE FUNCTION text_type1_out(text_type1) 
 RETURNS cstring
 AS 'textout' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE TYPE text_type1( 
 input = text_type1_in ,
 output = text_type1_out ,
 internallength = variable, 
 default = 'hfkdshfkjsdhflkshadfkhsadflkh' , 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=7);


select typoptions from pg_type_encoding where typid='text_type1 '::regtype;

DROP type if exists varchar_type1 cascade ; 

CREATE type varchar_type1;
CREATE FUNCTION varchar_type1_in(cstring) 
 RETURNS varchar_type1
 AS 'varcharin' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE FUNCTION varchar_type1_out(varchar_type1) 
 RETURNS cstring
 AS 'varcharout' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE TYPE varchar_type1( 
 input = varchar_type1_in ,
 output = varchar_type1_out ,
 internallength = variable, 
 default = 'ajhgdjagdjasdkjashk' , 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists varchar_type1 cascade;

CREATE FUNCTION varchar_type1_in(cstring) 
 RETURNS varchar_type1
 AS 'varcharin' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE FUNCTION varchar_type1_out(varchar_type1) 
 RETURNS cstring
 AS 'varcharout' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE TYPE varchar_type1( 
 input = varchar_type1_in ,
 output = varchar_type1_out ,
 internallength = variable, 
 default = 'ajhgdjagdjasdkjashk' , 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=9);


select typoptions from pg_type_encoding where typid='varchar_type1 '::regtype;

DROP type if exists date_type1 cascade ; 

CREATE type date_type1;
CREATE FUNCTION date_type1_in(cstring) 
 RETURNS date_type1
 AS 'date_in' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE FUNCTION date_type1_out(date_type1) 
 RETURNS cstring
 AS 'date_out' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE TYPE date_type1( 
 input = date_type1_in ,
 output = date_type1_out ,
 internallength = 4, 
 default = '2001-12-11' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists date_type1 cascade;

CREATE FUNCTION date_type1_in(cstring) 
 RETURNS date_type1
 AS 'date_in' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE FUNCTION date_type1_out(date_type1) 
 RETURNS cstring
 AS 'date_out' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE TYPE date_type1( 
 input = date_type1_in ,
 output = date_type1_out ,
 internallength = 4, 
 default = '2001-12-11' , 
 passedbyvalue, 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=2);


select typoptions from pg_type_encoding where typid='date_type1 '::regtype;

DROP type if exists timestamp_type1 cascade ; 

CREATE type timestamp_type1;
CREATE FUNCTION timestamp_type1_in(cstring) 
 RETURNS timestamp_type1
 AS 'timestamp_in' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE FUNCTION timestamp_type1_out(timestamp_type1) 
 RETURNS cstring
 AS 'timestamp_out' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 

CREATE TYPE timestamp_type1( 
 input = timestamp_type1_in ,
 output = timestamp_type1_out ,
 internallength = 4, 
 default = '2001-12-24 02:26:11' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=2);

--Drop and recreate the data type 

 Drop type if exists timestamp_type1 cascade;

CREATE FUNCTION timestamp_type1_in(cstring) 
 RETURNS timestamp_type1
 AS 'timestamp_in' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL; 


CREATE FUNCTION timestamp_type1_out(timestamp_type1) 
 RETURNS cstring
 AS 'timestamp_out' 
 LANGUAGE internal IMMUTABLE STRICT NO SQL;


CREATE TYPE timestamp_type1( 
 input = timestamp_type1_in ,
 output = timestamp_type1_out ,
 internallength = 4, 
 default = '2001-12-24 02:26:11' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=4);


select typoptions from pg_type_encoding where typid='timestamp_type1 '::regtype;

DROP table if exists co_compr_type_tb; 
-- Create table 
CREATE TABLE co_compr_type_tb
	 (id serial,  a1 int_type1, a2 char_type1, a3 text_type1, a4 date_type1, a5 varchar_type1, a6 timestamp_type1)  WITH (appendonly=true, orientation=column) distributed randomly;


\d+ co_compr_type_tb

INSERT into co_compr_type_tb DEFAULT VALUES ; 
Select * from co_compr_type_tb;

Insert into co_compr_type_tb select * from co_compr_type_tb; 
Insert into co_compr_type_tb select * from co_compr_type_tb; 
Insert into co_compr_type_tb select * from co_compr_type_tb; 
Insert into co_compr_type_tb select * from co_compr_type_tb; 
Insert into co_compr_type_tb select * from co_compr_type_tb; 
Insert into co_compr_type_tb select * from co_compr_type_tb; 

Select * from co_compr_type_tb;

--Alter table drop a column 
Alter table co_compr_type_tb Drop column a2; 
Insert into co_compr_type_tb(a1,a3,a4,a5,a6)  select a1,a3,a4,a5,a6 from co_compr_type_tb ;
Select count(*) from co_compr_type_tb; 

--Alter table rename a column 
Alter table co_compr_type_tb Rename column a3 TO after_rename_a3; 
--Insert data to the table, select count(*)
Insert into co_compr_type_tb(a1,after_rename_a3,a4,a5,a6)  select a1,after_rename_a3,a4,a5,a6 from co_compr_type_tb ;
Select count(*) from co_compr_type_tb; 

Alter type int_type1 set default encoding (compresstype=rle_type,compresslevel=3);

\d+ co_compr_type_tb

Insert into co_compr_type_tb(a1,after_rename_a3,a4,a5,a6)  select a1,after_rename_a3,a4,a5,a6 from co_compr_type_tb ;
Select count(*) from co_compr_type_tb; 

--
-- Drop table if exists
--
DROP TABLE if exists co_compr_zlib_with cascade;

--
-- Create table
--
CREATE TABLE co_compr_zlib_with 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )
 WITH (appendonly=true, orientation=column,compresstype=zlib,compresslevel=1,blocksize=32768) distributed randomly;

-- 
-- Create Indexes
--
CREATE INDEX co_compr_zlib_with_idx_bitmap ON co_compr_zlib_with USING bitmap (a1);

CREATE INDEX co_compr_zlib_with_idx_btree ON co_compr_zlib_with(a9);

--
-- Insert data to the table
--
 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,525),'M',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2515),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support ','1134.26',311353,generate_series(3892,3902),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly. It is especially recommended for storing monetary amounts and other quantities where exactness is required. However, arithmetic on numeric values is very slow compared to the integer types, or to the floating-point types described in the next section.The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','bit type data must match the length n exactly; it is an error to attempt to store shorter or longer','1','10',43,54); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(600,625),'F',2011,'f','b','If the scale of a value to be stored is greater than the declared scale of the column, the system will round the value to the specified number of fractional digits. Then, if the number of digits to the left of the decimal point ','2001-11-24 02:26:11','bbffruikjjjk89kjhjjdsdsjflsdkfjowikfo;lwejkufhekusgfuyewhfkdhsuyfgjsdbfjhkdshgciuvhdskfhiewyerwhkjehriur687687rt3ughjgd67567tcghjzvcnzfTYr7tugfTRE#$#^%*GGHJFTEW#RUYJBJHCFDGJBJGYrythgfT^&^tjhE655ugHD655uVtyr%^uygUYT&^R%^FJYFHGF',2802,'2011-11-12','Through our sbdfjdsbkfndjkshgifhdsn c  environment, we encourageyour guwr6tojsbdjht8y^W%^GNBMNBHGFE^^, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorizahrqwygjashbxuyagsu.','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of , not the apprehension.It is important to discuss the school procedures with your child. Inform your child that youoom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the cmnsbvduytfrw67ydwhg uygyth your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop by and visit any time you like. When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visidbsnfuyewfudsc,vsmckposjcofice and obtain a visit sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','1','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1500,1525),'M',2011,'t','b','At Stratford, we believe all children love to learn and that success in school and life depends on developing a strong foundation for learning early in life gsdgdsfoppkhgfshjdgksahdiusahdksahdksahkdhsakdhskahkhasdfu','2001-11-24 02:26:11','ttttttttttttttttrrrrrrrrrrrrrrrrrrrrrtttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwttttttttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwweeeeeeeeeeeeeeeeeeeeeeeeeeeeeettttttttttttttttttttttttttttteeeeeeeeeeeeeeeeeeeeeeeeeeedddddddd',2572,'2011-11-12','Through our stimulating dnvuy9efoewlhrf78we68bcmnsbment, we enhsdfiyeowlfdshfkjsdhoifuoeifdhsjvdjnvbhjdfgusftsdbmfnbdstional maturity, and a lifetime loveof learning.Sign and submit the Suythjrbuyhjngy78yuhjkhgvfderujhbdfxrtyuhjcfxdserwhuc  dvfdfofdgjkfrtiomn,eriokljnhbgfdhjkljhgfrtuyhgvform (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehensmnbvcxzasdfghjkoiuytre456789hfghild that you willnot be staying in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','poiuytrewqtgvsxcvbniujhbgvdfrgtyhujjhndcvuhj, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the clasnbdsvuytg uhguyybvhcd with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(3300,3455),'F',2011,'f','b','Numeric values are physically stored without any extra leading or trailing zeroes. Thus, the declared precision and scale of a column are maximums, not fixed allocations jwgrkesfjkdshifhdskjnbfnscgffiupjhvgfdatfdjhav','2002-12-24 02:26:10','uewrtuewfhbdshmmfbbhjjjjjjjjjjjjjjjjjjjjjjjjjewjjjjjjjjjjjjjjsb ffffffffffffffeyyyyyyyyyyyyyyyybcj  hgiusyyyy7777777777eeeeeeeeeeewiuhgsuydte78wt87rwetug7666666we7w86e7w6e87w6ew786ew8e678w6e8w6e8w6e8we6w7e827e6238272377hsys6w6yehsy6wh',2552,'2011-11-12','Through odnviudfygojlskhiusdyfdslfun classroom environment, we encourageyour child to achieve self-confidence, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorization form (if required).','It is very typical for presclsjhfdiudhskjfnds,vnc,svljsdaon anxietyduring the first few days of school. Please visimngfrtetyuiujnsbvcrdtr to the first dayof class to help him/her become acquainted with the nsfgsduaytf8fodshkfjhdw786%$%#^&&MBHJFY*IUHjhghgasd76ewhfewagement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with yhtyrt$#%^*&*HVGfu8jkGUYT$ujjtygdkfghjklfdhgjkfndkjghfdkgkfdge classroom with him','1231.16',121451,generate_series(5,25),2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Snjhgfytwuie4r893r7yhwdgt678iuhjgfr5678u school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop  shasuymnsdjkhsayt6b bnftrrojmbmnctreiujmnb nbfyttojmn, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visitosdfiwe7r09e[wrwdhskcisitors sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(50,55),'M',2011,'f','b','Inexact means that some values cannot be converted exactly to the internal format and are stored as approximations, so that storing and printing back out hwgfrdjwpfidsbhfsyugdskbfkjshfihsdkjfhsjhayiuyihjhvghdhgfjhdva6h','2003-12-24 02:26:11','mjhsiury8w4r9834kfjsdhcjbjghsafre6547698ukljjhgftre4t@%$^&%*&DRTDHGGUY&*^*&(^Gfhe456543^RGHJFYTE%$#!@!~#$%TGJHIU(***$%#@TRFHJG%^$&^*&FHR^%$%UTGHffge45675786gfrtew43ehghjt^%&^86575675tyftyr6tfytr65edf564ttyr565r64tyyyyr4e65tyyyyyyyyyt76',generate_series(700,725),'2011-11-11','Through87678574678uygjr565ghjenvironment, we encourageyour child to achieve ssbfuwet8ryewsjfnjksdhkcmxznbcnsfyetrusdgfdsbhjca lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid klhgiueffhlskvhfgxtfyuh form (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit thuytrewfghjkjv cbvnmbvcrte78uhjgnmnbvc5678jnm 4587uijk vacquainted with the new environment.Ajfhgdsufdsjfldsbfcjhgdshhhhhhhhuyhgbn sfsfsdur child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with your child. Inform your child that you willnot be stayisdfdsgfuyehfihdfiyiewuyfiuwhfng in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.jhge7fefvjkehguejfgdkhjkjhgowu09f8r9wugfbwjhvuyTears will usually disappear after Mommy and  Daddy leave the classroom. bdys8snssbss97j with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, jge7rty3498rtkew mfuhqy970qf wjhg8er7ewrjmwe jhg When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school objjcshgifisdcnskjcbdiso be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 



-- More data for bigger block size 
 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,525),'M',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2515),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support ','1134.26',311353,generate_series(3892,3902),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly. It is especially recommended for storing monetary amounts and other quantities where exactness is required. However, arithmetic on numeric values is very slow compared to the integer types, or to the floating-point types described in the next section.The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','bit type data must match the length n exactly; it is an error to attempt to store shorter or longer','1','10',43,54); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(600,625),'F',2011,'f','b','If the scale of a value to be stored is greater than the declared scale of the column, the system will round the value to the specified number of fractional digits. Then, if the number of digits to the left of the decimal point ','2001-11-24 02:26:11','bbffruikjjjk89kjhjjdsdsjflsdkfjowikfo;lwejkufhekusgfuyewhfkdhsuyfgjsdbfjhkdshgciuvhdskfhiewyerwhkjehriur687687rt3ughjgd67567tcghjzvcnzfTYr7tugfTRE#$#^%*GGHJFTEW#RUYJBJHCFDGJBJGYrythgfT^&^tjhE655ugHD655uVtyr%^uygUYT&^R%^FJYFHGF',2802,'2011-11-12','Through our sbdfjdsbkfndjkshgifhdsn c  environment, we encourageyour guwr6tojsbdjht8y^W%^GNBMNBHGFE^^, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorizahrqwygjashbxuyagsu.','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of , not the apprehension.It is important to discuss the school procedures with your child. Inform your child that youoom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the cmnsbvduytfrw67ydwhg uygyth your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop by and visit any time you like. When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visidbsnfuyewfudsc,vsmckposjcofice and obtain a visit sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','1','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1500,1525),'M',2011,'t','b','At Stratford, we believe all children love to learn and that success in school and life depends on developing a strong foundation for learning early in life gsdgdsfoppkhgfshjdgksahdiusahdksahdksahkdhsakdhskahkhasdfu','2001-11-24 02:26:11','ttttttttttttttttrrrrrrrrrrrrrrrrrrrrrtttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwttttttttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwweeeeeeeeeeeeeeeeeeeeeeeeeeeeeettttttttttttttttttttttttttttteeeeeeeeeeeeeeeeeeeeeeeeeeedddddddd',2572,'2011-11-12','Through our stimulating dnvuy9efoewlhrf78we68bcmnsbment, we enhsdfiyeowlfdshfkjsdhoifuoeifdhsjvdjnvbhjdfgusftsdbmfnbdstional maturity, and a lifetime loveof learning.Sign and submit the Suythjrbuyhjngy78yuhjkhgvfderujhbdfxrtyuhjcfxdserwhuc  dvfdfofdgjkfrtiomn,eriokljnhbgfdhjkljhgfrtuyhgvform (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehensmnbvcxzasdfghjkoiuytre456789hfghild that you willnot be staying in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','poiuytrewqtgvsxcvbniujhbgvdfrgtyhujjhndcvuhj, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the clasnbdsvuytg uhguyybvhcd with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(3300,3455),'F',2011,'f','b','Numeric values are physically stored without any extra leading or trailing zeroes. Thus, the declared precision and scale of a column are maximums, not fixed allocations jwgrkesfjkdshifhdskjnbfnscgffiupjhvgfdatfdjhav','2002-12-24 02:26:10','uewrtuewfhbdshmmfbbhjjjjjjjjjjjjjjjjjjjjjjjjjewjjjjjjjjjjjjjjsb ffffffffffffffeyyyyyyyyyyyyyyyybcj  hgiusyyyy7777777777eeeeeeeeeeewiuhgsuydte78wt87rwetug7666666we7w86e7w6e87w6ew786ew8e678w6e8w6e8w6e8we6w7e827e6238272377hsys6w6yehsy6wh',2552,'2011-11-12','Through odnviudfygojlskhiusdyfdslfun classroom environment, we encourageyour child to achieve self-confidence, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorization form (if required).','It is very typical for presclsjhfdiudhskjfnds,vnc,svljsdaon anxietyduring the first few days of school. Please visimngfrtetyuiujnsbvcrdtr to the first dayof class to help him/her become acquainted with the nsfgsduaytf8fodshkfjhdw786%$%#^&&MBHJFY*IUHjhghgasd76ewhfewagement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with yhtyrt$#%^*&*HVGfu8jkGUYT$ujjtygdkfghjklfdhgjkfndkjghfdkgkfdge classroom with him','1231.16',121451,generate_series(5,25),2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Snjhgfytwuie4r893r7yhwdgt678iuhjgfr5678u school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop  shasuymnsdjkhsayt6b bnftrrojmbmnctreiujmnb nbfyttojmn, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visitosdfiwe7r09e[wrwdhskcisitors sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(50,55),'M',2011,'f','b','Inexact means that some values cannot be converted exactly to the internal format and are stored as approximations, so that storing and printing back out hwgfrdjwpfidsbhfsyugdskbfkjshfihsdkjfhsjhayiuyihjhvghdhgfjhdva6h','2003-12-24 02:26:11','mjhsiury8w4r9834kfjsdhcjbjghsafre6547698ukljjhgftre4t@%$^&%*&DRTDHGGUY&*^*&(^Gfhe456543^RGHJFYTE%$#!@!~#$%TGJHIU(***$%#@TRFHJG%^$&^*&FHR^%$%UTGHffge45675786gfrtew43ehghjt^%&^86575675tyftyr6tfytr65edf564ttyr565r64tyyyyr4e65tyyyyyyyyyt76',generate_series(700,725),'2011-11-11','Through87678574678uygjr565ghjenvironment, we encourageyour child to achieve ssbfuwet8ryewsjfnjksdhkcmxznbcnsfyetrusdgfdsbhjca lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid klhgiueffhlskvhfgxtfyuh form (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit thuytrewfghjkjv cbvnmbvcrte78uhjgnmnbvc5678jnm 4587uijk vacquainted with the new environment.Ajfhgdsufdsjfldsbfcjhgdshhhhhhhhuyhgbn sfsfsdur child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with your child. Inform your child that you willnot be stayisdfdsgfuyehfihdfiyiewuyfiuwhfng in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.jhge7fefvjkehguejfgdkhjkjhgowu09f8r9wugfbwjhvuyTears will usually disappear after Mommy and  Daddy leave the classroom. bdys8snssbss97j with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, jge7rty3498rtkew mfuhqy970qf wjhg8er7ewrjmwe jhg When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school objjcshgifisdcnskjcbdiso be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 


\d+ co_compr_zlib_with

--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  co_compr_zlib_with;

--Alter table alter type of a column 
Alter table co_compr_zlib_with Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_zlib_with where id =10;
Select count(*) from co_compr_zlib_with; 

--Alter table drop a column 
Alter table co_compr_zlib_with Drop column a12; 
Insert into co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_zlib_with where id =10;
Select count(*) from co_compr_zlib_with; 

--Alter table rename a column 
Alter table co_compr_zlib_with Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_zlib_with where id =10;
Select count(*) from co_compr_zlib_with; 

--Drop table 
DROP table co_compr_zlib_with; 

--Create table again and insert data 
CREATE TABLE co_compr_zlib_with 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )
 WITH (appendonly=true, orientation=column,compresstype=zlib,compresslevel=1,blocksize=32768) distributed randomly;
 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,525),'M',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2515),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support ','1134.26',311353,generate_series(3892,3902),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly. It is especially recommended for storing monetary amounts and other quantities where exactness is required. However, arithmetic on numeric values is very slow compared to the integer types, or to the floating-point types described in the next section.The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','bit type data must match the length n exactly; it is an error to attempt to store shorter or longer','1','10',43,54); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(600,625),'F',2011,'f','b','If the scale of a value to be stored is greater than the declared scale of the column, the system will round the value to the specified number of fractional digits. Then, if the number of digits to the left of the decimal point ','2001-11-24 02:26:11','bbffruikjjjk89kjhjjdsdsjflsdkfjowikfo;lwejkufhekusgfuyewhfkdhsuyfgjsdbfjhkdshgciuvhdskfhiewyerwhkjehriur687687rt3ughjgd67567tcghjzvcnzfTYr7tugfTRE#$#^%*GGHJFTEW#RUYJBJHCFDGJBJGYrythgfT^&^tjhE655ugHD655uVtyr%^uygUYT&^R%^FJYFHGF',2802,'2011-11-12','Through our sbdfjdsbkfndjkshgifhdsn c  environment, we encourageyour guwr6tojsbdjht8y^W%^GNBMNBHGFE^^, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorizahrqwygjashbxuyagsu.','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of , not the apprehension.It is important to discuss the school procedures with your child. Inform your child that youoom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the cmnsbvduytfrw67ydwhg uygyth your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop by and visit any time you like. When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visidbsnfuyewfudsc,vsmckposjcofice and obtain a visit sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','1','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1500,1525),'M',2011,'t','b','At Stratford, we believe all children love to learn and that success in school and life depends on developing a strong foundation for learning early in life gsdgdsfoppkhgfshjdgksahdiusahdksahdksahkdhsakdhskahkhasdfu','2001-11-24 02:26:11','ttttttttttttttttrrrrrrrrrrrrrrrrrrrrrtttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwttttttttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwweeeeeeeeeeeeeeeeeeeeeeeeeeeeeettttttttttttttttttttttttttttteeeeeeeeeeeeeeeeeeeeeeeeeeedddddddd',2572,'2011-11-12','Through our stimulating dnvuy9efoewlhrf78we68bcmnsbment, we enhsdfiyeowlfdshfkjsdhoifuoeifdhsjvdjnvbhjdfgusftsdbmfnbdstional maturity, and a lifetime loveof learning.Sign and submit the Suythjrbuyhjngy78yuhjkhgvfderujhbdfxrtyuhjcfxdserwhuc  dvfdfofdgjkfrtiomn,eriokljnhbgfdhjkljhgfrtuyhgvform (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehensmnbvcxzasdfghjkoiuytre456789hfghild that you willnot be staying in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','poiuytrewqtgvsxcvbniujhbgvdfrgtyhujjhndcvuhj, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the clasnbdsvuytg uhguyybvhcd with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(3300,3455),'F',2011,'f','b','Numeric values are physically stored without any extra leading or trailing zeroes. Thus, the declared precision and scale of a column are maximums, not fixed allocations jwgrkesfjkdshifhdskjnbfnscgffiupjhvgfdatfdjhav','2002-12-24 02:26:10','uewrtuewfhbdshmmfbbhjjjjjjjjjjjjjjjjjjjjjjjjjewjjjjjjjjjjjjjjsb ffffffffffffffeyyyyyyyyyyyyyyyybcj  hgiusyyyy7777777777eeeeeeeeeeewiuhgsuydte78wt87rwetug7666666we7w86e7w6e87w6ew786ew8e678w6e8w6e8w6e8we6w7e827e6238272377hsys6w6yehsy6wh',2552,'2011-11-12','Through odnviudfygojlskhiusdyfdslfun classroom environment, we encourageyour child to achieve self-confidence, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorization form (if required).','It is very typical for presclsjhfdiudhskjfnds,vnc,svljsdaon anxietyduring the first few days of school. Please visimngfrtetyuiujnsbvcrdtr to the first dayof class to help him/her become acquainted with the nsfgsduaytf8fodshkfjhdw786%$%#^&&MBHJFY*IUHjhghgasd76ewhfewagement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with yhtyrt$#%^*&*HVGfu8jkGUYT$ujjtygdkfghjklfdhgjkfndkjghfdkgkfdge classroom with him','1231.16',121451,generate_series(5,25),2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Snjhgfytwuie4r893r7yhwdgt678iuhjgfr5678u school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop  shasuymnsdjkhsayt6b bnftrrojmbmnctreiujmnb nbfyttojmn, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visitosdfiwe7r09e[wrwdhskcisitors sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(50,55),'M',2011,'f','b','Inexact means that some values cannot be converted exactly to the internal format and are stored as approximations, so that storing and printing back out hwgfrdjwpfidsbhfsyugdskbfkjshfihsdkjfhsjhayiuyihjhvghdhgfjhdva6h','2003-12-24 02:26:11','mjhsiury8w4r9834kfjsdhcjbjghsafre6547698ukljjhgftre4t@%$^&%*&DRTDHGGUY&*^*&(^Gfhe456543^RGHJFYTE%$#!@!~#$%TGJHIU(***$%#@TRFHJG%^$&^*&FHR^%$%UTGHffge45675786gfrtew43ehghjt^%&^86575675tyftyr6tfytr65edf564ttyr565r64tyyyyr4e65tyyyyyyyyyt76',generate_series(700,725),'2011-11-11','Through87678574678uygjr565ghjenvironment, we encourageyour child to achieve ssbfuwet8ryewsjfnjksdhkcmxznbcnsfyetrusdgfdsbhjca lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid klhgiueffhlskvhfgxtfyuh form (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit thuytrewfghjkjv cbvnmbvcrte78uhjgnmnbvc5678jnm 4587uijk vacquainted with the new environment.Ajfhgdsufdsjfldsbfcjhgdshhhhhhhhuyhgbn sfsfsdur child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with your child. Inform your child that you willnot be stayisdfdsgfuyehfihdfiyiewuyfiuwhfng in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.jhge7fefvjkehguejfgdkhjkjhgowu09f8r9wugfbwjhvuyTears will usually disappear after Mommy and  Daddy leave the classroom. bdys8snssbss97j with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, jge7rty3498rtkew mfuhqy970qf wjhg8er7ewrjmwe jhg When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school objjcshgifisdcnskjcbdiso be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 



-- More data for bigger block size 
 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,525),'M',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2515),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support ','1134.26',311353,generate_series(3892,3902),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly. It is especially recommended for storing monetary amounts and other quantities where exactness is required. However, arithmetic on numeric values is very slow compared to the integer types, or to the floating-point types described in the next section.The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','bit type data must match the length n exactly; it is an error to attempt to store shorter or longer','1','10',43,54); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(600,625),'F',2011,'f','b','If the scale of a value to be stored is greater than the declared scale of the column, the system will round the value to the specified number of fractional digits. Then, if the number of digits to the left of the decimal point ','2001-11-24 02:26:11','bbffruikjjjk89kjhjjdsdsjflsdkfjowikfo;lwejkufhekusgfuyewhfkdhsuyfgjsdbfjhkdshgciuvhdskfhiewyerwhkjehriur687687rt3ughjgd67567tcghjzvcnzfTYr7tugfTRE#$#^%*GGHJFTEW#RUYJBJHCFDGJBJGYrythgfT^&^tjhE655ugHD655uVtyr%^uygUYT&^R%^FJYFHGF',2802,'2011-11-12','Through our sbdfjdsbkfndjkshgifhdsn c  environment, we encourageyour guwr6tojsbdjht8y^W%^GNBMNBHGFE^^, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorizahrqwygjashbxuyagsu.','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of , not the apprehension.It is important to discuss the school procedures with your child. Inform your child that youoom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the cmnsbvduytfrw67ydwhg uygyth your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop by and visit any time you like. When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visidbsnfuyewfudsc,vsmckposjcofice and obtain a visit sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','1','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1500,1525),'M',2011,'t','b','At Stratford, we believe all children love to learn and that success in school and life depends on developing a strong foundation for learning early in life gsdgdsfoppkhgfshjdgksahdiusahdksahdksahkdhsakdhskahkhasdfu','2001-11-24 02:26:11','ttttttttttttttttrrrrrrrrrrrrrrrrrrrrrtttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwttttttttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwweeeeeeeeeeeeeeeeeeeeeeeeeeeeeettttttttttttttttttttttttttttteeeeeeeeeeeeeeeeeeeeeeeeeeedddddddd',2572,'2011-11-12','Through our stimulating dnvuy9efoewlhrf78we68bcmnsbment, we enhsdfiyeowlfdshfkjsdhoifuoeifdhsjvdjnvbhjdfgusftsdbmfnbdstional maturity, and a lifetime loveof learning.Sign and submit the Suythjrbuyhjngy78yuhjkhgvfderujhbdfxrtyuhjcfxdserwhuc  dvfdfofdgjkfrtiomn,eriokljnhbgfdhjkljhgfrtuyhgvform (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehensmnbvcxzasdfghjkoiuytre456789hfghild that you willnot be staying in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','poiuytrewqtgvsxcvbniujhbgvdfrgtyhujjhndcvuhj, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the clasnbdsvuytg uhguyybvhcd with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(3300,3455),'F',2011,'f','b','Numeric values are physically stored without any extra leading or trailing zeroes. Thus, the declared precision and scale of a column are maximums, not fixed allocations jwgrkesfjkdshifhdskjnbfnscgffiupjhvgfdatfdjhav','2002-12-24 02:26:10','uewrtuewfhbdshmmfbbhjjjjjjjjjjjjjjjjjjjjjjjjjewjjjjjjjjjjjjjjsb ffffffffffffffeyyyyyyyyyyyyyyyybcj  hgiusyyyy7777777777eeeeeeeeeeewiuhgsuydte78wt87rwetug7666666we7w86e7w6e87w6ew786ew8e678w6e8w6e8w6e8we6w7e827e6238272377hsys6w6yehsy6wh',2552,'2011-11-12','Through odnviudfygojlskhiusdyfdslfun classroom environment, we encourageyour child to achieve self-confidence, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorization form (if required).','It is very typical for presclsjhfdiudhskjfnds,vnc,svljsdaon anxietyduring the first few days of school. Please visimngfrtetyuiujnsbvcrdtr to the first dayof class to help him/her become acquainted with the nsfgsduaytf8fodshkfjhdw786%$%#^&&MBHJFY*IUHjhghgasd76ewhfewagement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with yhtyrt$#%^*&*HVGfu8jkGUYT$ujjtygdkfghjklfdhgjkfndkjghfdkgkfdge classroom with him','1231.16',121451,generate_series(5,25),2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Snjhgfytwuie4r893r7yhwdgt678iuhjgfr5678u school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop  shasuymnsdjkhsayt6b bnftrrojmbmnctreiujmnb nbfyttojmn, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visitosdfiwe7r09e[wrwdhskcisitors sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(50,55),'M',2011,'f','b','Inexact means that some values cannot be converted exactly to the internal format and are stored as approximations, so that storing and printing back out hwgfrdjwpfidsbhfsyugdskbfkjshfihsdkjfhsjhayiuyihjhvghdhgfjhdva6h','2003-12-24 02:26:11','mjhsiury8w4r9834kfjsdhcjbjghsafre6547698ukljjhgftre4t@%$^&%*&DRTDHGGUY&*^*&(^Gfhe456543^RGHJFYTE%$#!@!~#$%TGJHIU(***$%#@TRFHJG%^$&^*&FHR^%$%UTGHffge45675786gfrtew43ehghjt^%&^86575675tyftyr6tfytr65edf564ttyr565r64tyyyyr4e65tyyyyyyyyyt76',generate_series(700,725),'2011-11-11','Through87678574678uygjr565ghjenvironment, we encourageyour child to achieve ssbfuwet8ryewsjfnjksdhkcmxznbcnsfyetrusdgfdsbhjca lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid klhgiueffhlskvhfgxtfyuh form (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit thuytrewfghjkjv cbvnmbvcrte78uhjgnmnbvc5678jnm 4587uijk vacquainted with the new environment.Ajfhgdsufdsjfldsbfcjhgdshhhhhhhhuyhgbn sfsfsdur child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with your child. Inform your child that you willnot be stayisdfdsgfuyehfihdfiyiewuyfiuwhfng in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.jhge7fefvjkehguejfgdkhjkjhgowu09f8r9wugfbwjhvuyTears will usually disappear after Mommy and  Daddy leave the classroom. bdys8snssbss97j with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, jge7rty3498rtkew mfuhqy970qf wjhg8er7ewrjmwe jhg When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school objjcshgifisdcnskjcbdiso be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 



--Alter table drop a column 
Alter table co_compr_zlib_with Drop column a12; 
--Create CTAS table 

 Drop table if exists co_compr_zlib_with_ctas ;
--Create a CTAS table 
CREATE TABLE co_compr_zlib_with_ctas  WITH (appendonly=true, orientation=column) AS Select * from co_compr_zlib_with;

--
-- Drop table if exists
--
DROP TABLE if exists co_compr_zlib_with cascade;

--
-- Create table
--
CREATE TABLE co_compr_zlib_with 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )
 WITH (appendonly=true, orientation=column,compresstype=zlib,compresslevel=1,blocksize=32768) distributed randomly;

-- 
-- Create Indexes
--
CREATE INDEX co_compr_zlib_with_idx_bitmap ON co_compr_zlib_with USING bitmap (a1);

CREATE INDEX co_compr_zlib_with_idx_btree ON co_compr_zlib_with(a9);

--
-- Insert data to the table
--
 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,525),'M',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2515),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support ','1134.26',311353,generate_series(3892,3902),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly. It is especially recommended for storing monetary amounts and other quantities where exactness is required. However, arithmetic on numeric values is very slow compared to the integer types, or to the floating-point types described in the next section.The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','bit type data must match the length n exactly; it is an error to attempt to store shorter or longer','1','10',43,54); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(600,625),'F',2011,'f','b','If the scale of a value to be stored is greater than the declared scale of the column, the system will round the value to the specified number of fractional digits. Then, if the number of digits to the left of the decimal point ','2001-11-24 02:26:11','bbffruikjjjk89kjhjjdsdsjflsdkfjowikfo;lwejkufhekusgfuyewhfkdhsuyfgjsdbfjhkdshgciuvhdskfhiewyerwhkjehriur687687rt3ughjgd67567tcghjzvcnzfTYr7tugfTRE#$#^%*GGHJFTEW#RUYJBJHCFDGJBJGYrythgfT^&^tjhE655ugHD655uVtyr%^uygUYT&^R%^FJYFHGF',2802,'2011-11-12','Through our sbdfjdsbkfndjkshgifhdsn c  environment, we encourageyour guwr6tojsbdjht8y^W%^GNBMNBHGFE^^, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorizahrqwygjashbxuyagsu.','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of , not the apprehension.It is important to discuss the school procedures with your child. Inform your child that youoom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the cmnsbvduytfrw67ydwhg uygyth your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop by and visit any time you like. When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visidbsnfuyewfudsc,vsmckposjcofice and obtain a visit sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','1','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1500,1525),'M',2011,'t','b','At Stratford, we believe all children love to learn and that success in school and life depends on developing a strong foundation for learning early in life gsdgdsfoppkhgfshjdgksahdiusahdksahdksahkdhsakdhskahkhasdfu','2001-11-24 02:26:11','ttttttttttttttttrrrrrrrrrrrrrrrrrrrrrtttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwttttttttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwweeeeeeeeeeeeeeeeeeeeeeeeeeeeeettttttttttttttttttttttttttttteeeeeeeeeeeeeeeeeeeeeeeeeeedddddddd',2572,'2011-11-12','Through our stimulating dnvuy9efoewlhrf78we68bcmnsbment, we enhsdfiyeowlfdshfkjsdhoifuoeifdhsjvdjnvbhjdfgusftsdbmfnbdstional maturity, and a lifetime loveof learning.Sign and submit the Suythjrbuyhjngy78yuhjkhgvfderujhbdfxrtyuhjcfxdserwhuc  dvfdfofdgjkfrtiomn,eriokljnhbgfdhjkljhgfrtuyhgvform (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehensmnbvcxzasdfghjkoiuytre456789hfghild that you willnot be staying in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','poiuytrewqtgvsxcvbniujhbgvdfrgtyhujjhndcvuhj, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the clasnbdsvuytg uhguyybvhcd with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(3300,3455),'F',2011,'f','b','Numeric values are physically stored without any extra leading or trailing zeroes. Thus, the declared precision and scale of a column are maximums, not fixed allocations jwgrkesfjkdshifhdskjnbfnscgffiupjhvgfdatfdjhav','2002-12-24 02:26:10','uewrtuewfhbdshmmfbbhjjjjjjjjjjjjjjjjjjjjjjjjjewjjjjjjjjjjjjjjsb ffffffffffffffeyyyyyyyyyyyyyyyybcj  hgiusyyyy7777777777eeeeeeeeeeewiuhgsuydte78wt87rwetug7666666we7w86e7w6e87w6ew786ew8e678w6e8w6e8w6e8we6w7e827e6238272377hsys6w6yehsy6wh',2552,'2011-11-12','Through odnviudfygojlskhiusdyfdslfun classroom environment, we encourageyour child to achieve self-confidence, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorization form (if required).','It is very typical for presclsjhfdiudhskjfnds,vnc,svljsdaon anxietyduring the first few days of school. Please visimngfrtetyuiujnsbvcrdtr to the first dayof class to help him/her become acquainted with the nsfgsduaytf8fodshkfjhdw786%$%#^&&MBHJFY*IUHjhghgasd76ewhfewagement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with yhtyrt$#%^*&*HVGfu8jkGUYT$ujjtygdkfghjklfdhgjkfndkjghfdkgkfdge classroom with him','1231.16',121451,generate_series(5,25),2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Snjhgfytwuie4r893r7yhwdgt678iuhjgfr5678u school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop  shasuymnsdjkhsayt6b bnftrrojmbmnctreiujmnb nbfyttojmn, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visitosdfiwe7r09e[wrwdhskcisitors sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(50,55),'M',2011,'f','b','Inexact means that some values cannot be converted exactly to the internal format and are stored as approximations, so that storing and printing back out hwgfrdjwpfidsbhfsyugdskbfkjshfihsdkjfhsjhayiuyihjhvghdhgfjhdva6h','2003-12-24 02:26:11','mjhsiury8w4r9834kfjsdhcjbjghsafre6547698ukljjhgftre4t@%$^&%*&DRTDHGGUY&*^*&(^Gfhe456543^RGHJFYTE%$#!@!~#$%TGJHIU(***$%#@TRFHJG%^$&^*&FHR^%$%UTGHffge45675786gfrtew43ehghjt^%&^86575675tyftyr6tfytr65edf564ttyr565r64tyyyyr4e65tyyyyyyyyyt76',generate_series(700,725),'2011-11-11','Through87678574678uygjr565ghjenvironment, we encourageyour child to achieve ssbfuwet8ryewsjfnjksdhkcmxznbcnsfyetrusdgfdsbhjca lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid klhgiueffhlskvhfgxtfyuh form (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit thuytrewfghjkjv cbvnmbvcrte78uhjgnmnbvc5678jnm 4587uijk vacquainted with the new environment.Ajfhgdsufdsjfldsbfcjhgdshhhhhhhhuyhgbn sfsfsdur child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with your child. Inform your child that you willnot be stayisdfdsgfuyehfihdfiyiewuyfiuwhfng in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.jhge7fefvjkehguejfgdkhjkjhgowu09f8r9wugfbwjhvuyTears will usually disappear after Mommy and  Daddy leave the classroom. bdys8snssbss97j with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, jge7rty3498rtkew mfuhqy970qf wjhg8er7ewrjmwe jhg When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school objjcshgifisdcnskjcbdiso be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 



-- More data for bigger block size 
 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,525),'M',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2515),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support ','1134.26',311353,generate_series(3892,3902),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly. It is especially recommended for storing monetary amounts and other quantities where exactness is required. However, arithmetic on numeric values is very slow compared to the integer types, or to the floating-point types described in the next section.The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','bit type data must match the length n exactly; it is an error to attempt to store shorter or longer','1','10',43,54); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(600,625),'F',2011,'f','b','If the scale of a value to be stored is greater than the declared scale of the column, the system will round the value to the specified number of fractional digits. Then, if the number of digits to the left of the decimal point ','2001-11-24 02:26:11','bbffruikjjjk89kjhjjdsdsjflsdkfjowikfo;lwejkufhekusgfuyewhfkdhsuyfgjsdbfjhkdshgciuvhdskfhiewyerwhkjehriur687687rt3ughjgd67567tcghjzvcnzfTYr7tugfTRE#$#^%*GGHJFTEW#RUYJBJHCFDGJBJGYrythgfT^&^tjhE655ugHD655uVtyr%^uygUYT&^R%^FJYFHGF',2802,'2011-11-12','Through our sbdfjdsbkfndjkshgifhdsn c  environment, we encourageyour guwr6tojsbdjht8y^W%^GNBMNBHGFE^^, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorizahrqwygjashbxuyagsu.','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of , not the apprehension.It is important to discuss the school procedures with your child. Inform your child that youoom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the cmnsbvduytfrw67ydwhg uygyth your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop by and visit any time you like. When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visidbsnfuyewfudsc,vsmckposjcofice and obtain a visit sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','1','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1500,1525),'M',2011,'t','b','At Stratford, we believe all children love to learn and that success in school and life depends on developing a strong foundation for learning early in life gsdgdsfoppkhgfshjdgksahdiusahdksahdksahkdhsakdhskahkhasdfu','2001-11-24 02:26:11','ttttttttttttttttrrrrrrrrrrrrrrrrrrrrrtttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwttttttttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwweeeeeeeeeeeeeeeeeeeeeeeeeeeeeettttttttttttttttttttttttttttteeeeeeeeeeeeeeeeeeeeeeeeeeedddddddd',2572,'2011-11-12','Through our stimulating dnvuy9efoewlhrf78we68bcmnsbment, we enhsdfiyeowlfdshfkjsdhoifuoeifdhsjvdjnvbhjdfgusftsdbmfnbdstional maturity, and a lifetime loveof learning.Sign and submit the Suythjrbuyhjngy78yuhjkhgvfderujhbdfxrtyuhjcfxdserwhuc  dvfdfofdgjkfrtiomn,eriokljnhbgfdhjkljhgfrtuyhgvform (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehensmnbvcxzasdfghjkoiuytre456789hfghild that you willnot be staying in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','poiuytrewqtgvsxcvbniujhbgvdfrgtyhujjhndcvuhj, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the clasnbdsvuytg uhguyybvhcd with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(3300,3455),'F',2011,'f','b','Numeric values are physically stored without any extra leading or trailing zeroes. Thus, the declared precision and scale of a column are maximums, not fixed allocations jwgrkesfjkdshifhdskjnbfnscgffiupjhvgfdatfdjhav','2002-12-24 02:26:10','uewrtuewfhbdshmmfbbhjjjjjjjjjjjjjjjjjjjjjjjjjewjjjjjjjjjjjjjjsb ffffffffffffffeyyyyyyyyyyyyyyyybcj  hgiusyyyy7777777777eeeeeeeeeeewiuhgsuydte78wt87rwetug7666666we7w86e7w6e87w6ew786ew8e678w6e8w6e8w6e8we6w7e827e6238272377hsys6w6yehsy6wh',2552,'2011-11-12','Through odnviudfygojlskhiusdyfdslfun classroom environment, we encourageyour child to achieve self-confidence, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorization form (if required).','It is very typical for presclsjhfdiudhskjfnds,vnc,svljsdaon anxietyduring the first few days of school. Please visimngfrtetyuiujnsbvcrdtr to the first dayof class to help him/her become acquainted with the nsfgsduaytf8fodshkfjhdw786%$%#^&&MBHJFY*IUHjhghgasd76ewhfewagement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with yhtyrt$#%^*&*HVGfu8jkGUYT$ujjtygdkfghjklfdhgjkfndkjghfdkgkfdge classroom with him','1231.16',121451,generate_series(5,25),2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Snjhgfytwuie4r893r7yhwdgt678iuhjgfr5678u school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop  shasuymnsdjkhsayt6b bnftrrojmbmnctreiujmnb nbfyttojmn, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visitosdfiwe7r09e[wrwdhskcisitors sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(50,55),'M',2011,'f','b','Inexact means that some values cannot be converted exactly to the internal format and are stored as approximations, so that storing and printing back out hwgfrdjwpfidsbhfsyugdskbfkjshfihsdkjfhsjhayiuyihjhvghdhgfjhdva6h','2003-12-24 02:26:11','mjhsiury8w4r9834kfjsdhcjbjghsafre6547698ukljjhgftre4t@%$^&%*&DRTDHGGUY&*^*&(^Gfhe456543^RGHJFYTE%$#!@!~#$%TGJHIU(***$%#@TRFHJG%^$&^*&FHR^%$%UTGHffge45675786gfrtew43ehghjt^%&^86575675tyftyr6tfytr65edf564ttyr565r64tyyyyr4e65tyyyyyyyyyt76',generate_series(700,725),'2011-11-11','Through87678574678uygjr565ghjenvironment, we encourageyour child to achieve ssbfuwet8ryewsjfnjksdhkcmxznbcnsfyetrusdgfdsbhjca lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid klhgiueffhlskvhfgxtfyuh form (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit thuytrewfghjkjv cbvnmbvcrte78uhjgnmnbvc5678jnm 4587uijk vacquainted with the new environment.Ajfhgdsufdsjfldsbfcjhgdshhhhhhhhuyhgbn sfsfsdur child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with your child. Inform your child that you willnot be stayisdfdsgfuyehfihdfiyiewuyfiuwhfng in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.jhge7fefvjkehguejfgdkhjkjhgowu09f8r9wugfbwjhvuyTears will usually disappear after Mommy and  Daddy leave the classroom. bdys8snssbss97j with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, jge7rty3498rtkew mfuhqy970qf wjhg8er7ewrjmwe jhg When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school objjcshgifisdcnskjcbdiso be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 


\d+ co_compr_zlib_with

--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  co_compr_zlib_with;

--Alter table alter type of a column 
Alter table co_compr_zlib_with Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_zlib_with where id =10;
Select count(*) from co_compr_zlib_with; 

--Alter table drop a column 
Alter table co_compr_zlib_with Drop column a12; 
Insert into co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_zlib_with where id =10;
Select count(*) from co_compr_zlib_with; 

--Alter table rename a column 
Alter table co_compr_zlib_with Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_compr_zlib_with where id =10;
Select count(*) from co_compr_zlib_with; 

--Drop table 
DROP table co_compr_zlib_with; 

--Create table again and insert data 
CREATE TABLE co_compr_zlib_with 
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int )
 WITH (appendonly=true, orientation=column,compresstype=zlib,compresslevel=1,blocksize=32768) distributed randomly;
 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,525),'M',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2515),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support ','1134.26',311353,generate_series(3892,3902),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly. It is especially recommended for storing monetary amounts and other quantities where exactness is required. However, arithmetic on numeric values is very slow compared to the integer types, or to the floating-point types described in the next section.The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','bit type data must match the length n exactly; it is an error to attempt to store shorter or longer','1','10',43,54); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(600,625),'F',2011,'f','b','If the scale of a value to be stored is greater than the declared scale of the column, the system will round the value to the specified number of fractional digits. Then, if the number of digits to the left of the decimal point ','2001-11-24 02:26:11','bbffruikjjjk89kjhjjdsdsjflsdkfjowikfo;lwejkufhekusgfuyewhfkdhsuyfgjsdbfjhkdshgciuvhdskfhiewyerwhkjehriur687687rt3ughjgd67567tcghjzvcnzfTYr7tugfTRE#$#^%*GGHJFTEW#RUYJBJHCFDGJBJGYrythgfT^&^tjhE655ugHD655uVtyr%^uygUYT&^R%^FJYFHGF',2802,'2011-11-12','Through our sbdfjdsbkfndjkshgifhdsn c  environment, we encourageyour guwr6tojsbdjht8y^W%^GNBMNBHGFE^^, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorizahrqwygjashbxuyagsu.','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of , not the apprehension.It is important to discuss the school procedures with your child. Inform your child that youoom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the cmnsbvduytfrw67ydwhg uygyth your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop by and visit any time you like. When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visidbsnfuyewfudsc,vsmckposjcofice and obtain a visit sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','1','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1500,1525),'M',2011,'t','b','At Stratford, we believe all children love to learn and that success in school and life depends on developing a strong foundation for learning early in life gsdgdsfoppkhgfshjdgksahdiusahdksahdksahkdhsakdhskahkhasdfu','2001-11-24 02:26:11','ttttttttttttttttrrrrrrrrrrrrrrrrrrrrrtttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwttttttttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwweeeeeeeeeeeeeeeeeeeeeeeeeeeeeettttttttttttttttttttttttttttteeeeeeeeeeeeeeeeeeeeeeeeeeedddddddd',2572,'2011-11-12','Through our stimulating dnvuy9efoewlhrf78we68bcmnsbment, we enhsdfiyeowlfdshfkjsdhoifuoeifdhsjvdjnvbhjdfgusftsdbmfnbdstional maturity, and a lifetime loveof learning.Sign and submit the Suythjrbuyhjngy78yuhjkhgvfderujhbdfxrtyuhjcfxdserwhuc  dvfdfofdgjkfrtiomn,eriokljnhbgfdhjkljhgfrtuyhgvform (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehensmnbvcxzasdfghjkoiuytre456789hfghild that you willnot be staying in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','poiuytrewqtgvsxcvbniujhbgvdfrgtyhujjhndcvuhj, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the clasnbdsvuytg uhguyybvhcd with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(3300,3455),'F',2011,'f','b','Numeric values are physically stored without any extra leading or trailing zeroes. Thus, the declared precision and scale of a column are maximums, not fixed allocations jwgrkesfjkdshifhdskjnbfnscgffiupjhvgfdatfdjhav','2002-12-24 02:26:10','uewrtuewfhbdshmmfbbhjjjjjjjjjjjjjjjjjjjjjjjjjewjjjjjjjjjjjjjjsb ffffffffffffffeyyyyyyyyyyyyyyyybcj  hgiusyyyy7777777777eeeeeeeeeeewiuhgsuydte78wt87rwetug7666666we7w86e7w6e87w6ew786ew8e678w6e8w6e8w6e8we6w7e827e6238272377hsys6w6yehsy6wh',2552,'2011-11-12','Through odnviudfygojlskhiusdyfdslfun classroom environment, we encourageyour child to achieve self-confidence, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorization form (if required).','It is very typical for presclsjhfdiudhskjfnds,vnc,svljsdaon anxietyduring the first few days of school. Please visimngfrtetyuiujnsbvcrdtr to the first dayof class to help him/her become acquainted with the nsfgsduaytf8fodshkfjhdw786%$%#^&&MBHJFY*IUHjhghgasd76ewhfewagement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with yhtyrt$#%^*&*HVGfu8jkGUYT$ujjtygdkfghjklfdhgjkfndkjghfdkgkfdge classroom with him','1231.16',121451,generate_series(5,25),2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Snjhgfytwuie4r893r7yhwdgt678iuhjgfr5678u school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop  shasuymnsdjkhsayt6b bnftrrojmbmnctreiujmnb nbfyttojmn, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visitosdfiwe7r09e[wrwdhskcisitors sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(50,55),'M',2011,'f','b','Inexact means that some values cannot be converted exactly to the internal format and are stored as approximations, so that storing and printing back out hwgfrdjwpfidsbhfsyugdskbfkjshfihsdkjfhsjhayiuyihjhvghdhgfjhdva6h','2003-12-24 02:26:11','mjhsiury8w4r9834kfjsdhcjbjghsafre6547698ukljjhgftre4t@%$^&%*&DRTDHGGUY&*^*&(^Gfhe456543^RGHJFYTE%$#!@!~#$%TGJHIU(***$%#@TRFHJG%^$&^*&FHR^%$%UTGHffge45675786gfrtew43ehghjt^%&^86575675tyftyr6tfytr65edf564ttyr565r64tyyyyr4e65tyyyyyyyyyt76',generate_series(700,725),'2011-11-11','Through87678574678uygjr565ghjenvironment, we encourageyour child to achieve ssbfuwet8ryewsjfnjksdhkcmxznbcnsfyetrusdgfdsbhjca lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid klhgiueffhlskvhfgxtfyuh form (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit thuytrewfghjkjv cbvnmbvcrte78uhjgnmnbvc5678jnm 4587uijk vacquainted with the new environment.Ajfhgdsufdsjfldsbfcjhgdshhhhhhhhuyhgbn sfsfsdur child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with your child. Inform your child that you willnot be stayisdfdsgfuyehfihdfiyiewuyfiuwhfng in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.jhge7fefvjkehguejfgdkhjkjhgowu09f8r9wugfbwjhvuyTears will usually disappear after Mommy and  Daddy leave the classroom. bdys8snssbss97j with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, jge7rty3498rtkew mfuhqy970qf wjhg8er7ewrjmwe jhg When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school objjcshgifisdcnskjcbdiso be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 



-- More data for bigger block size 
 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1,20),'M',2011,'t','a','This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling','2001-12-24 02:26:11','U.S. House of Representatives Speaker John Boehner, the top Republican in Congress who has put forward a deficit reduction plan to be voted on later on Thursday said he had no control over whether his bill would avert a credit downgrade.',generate_series(2490,2505),'2011-10-11','The Republican-controlled House is tentatively scheduled to vote on Boehner proposal this afternoon at around 6 p.m. EDT (2200 GMT). The main Republican vote counter in the House, Kevin McCarthy, would not say if there were enough votes to pass the bill.','WASHINGTON:House Speaker John Boehner says his plan mixing spending cuts in exchange for raising the nations $14.3 trillion debt limit is not perfect but is as large a step that a divided government can take that is doable and signable by President Barack Obama.The Ohio Republican says the measure is an honest and sincere attempt at compromise and was negotiated with Democrats last weekend and that passing it would end the ongoing debt crisis. The plan blends $900 billion-plus in spending cuts with a companion increase in the nations borrowing cap.','1234.56',323453,generate_series(3452,3462),7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','Republicans had been working throughout the day Thursday to lock down support for their plan to raise the nations debt ceiling, even as Senate Democrats vowed to swiftly kill it if passed.','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','Johnson & Johnsons McNeil Consumer Healthcare announced the voluntary dosage reduction today. Labels will carry new dosing instructions this fall.The company says it will cut the maximum dosage of Regular Strength Tylenol and other acetaminophen-containing products in 2012.Acetaminophen is safe when used as directed, says Edwin Kuffner, MD, McNeil vice president of over-the-counter medical affairs. But, when too much is taken, it can cause liver damage.The action is intended to cut the risk of such accidental overdoses, the company says in a news release.','1','0',12,23); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,510),'F',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2516),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type integer is the usual choice, as it offers the best balance between range, storage size, and performance The type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer is the usual choice, as it offers the best balance between range, storage size, and performanceThe type integer ','1134.26',311353,generate_series(3982,3992),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. The types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges. Attempts to store values outside of the allowed range will result in an errorThe types smallint, integer, and bigint store whole numbers, that is, numbers without fractional components, of various ranges.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool','0','1',33,44); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(500,525),'M',2010,'f','b','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','2001-12-25 02:22:11','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child',generate_series(2500,2515),'2011-10-12','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support for eight-byte integers.The bigint type might not function correctly on all platforms, since it relies on compiler support ','1134.26',311353,generate_series(3892,3902),7885,'0101','2002-02-12 01:31:14+1344','2003-11-14 01:41:15','((1,1),(0,1),(1,1))','((2,1)(1,5))','08:00:2b:01:01:03','1-3','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly. It is especially recommended for storing monetary amounts and other quantities where exactness is required. However, arithmetic on numeric values is very slow compared to the integer types, or to the floating-point types described in the next section.The type numeric can store numbers with up to 1000 digits of precision and perform calculations exactly.','((6,5)(4,2))','(3,6)',12.233,'((5,4),2)',12,3114,'(1,1,0,3)','2010-03-21',43164,'$1,500.00','192.167.2','126.1.1.1','10:30:55','bit type data must match the length n exactly; it is an error to attempt to store shorter or longer','1','10',43,54); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(600,625),'F',2011,'f','b','If the scale of a value to be stored is greater than the declared scale of the column, the system will round the value to the specified number of fractional digits. Then, if the number of digits to the left of the decimal point ','2001-11-24 02:26:11','bbffruikjjjk89kjhjjdsdsjflsdkfjowikfo;lwejkufhekusgfuyewhfkdhsuyfgjsdbfjhkdshgciuvhdskfhiewyerwhkjehriur687687rt3ughjgd67567tcghjzvcnzfTYr7tugfTRE#$#^%*GGHJFTEW#RUYJBJHCFDGJBJGYrythgfT^&^tjhE655ugHD655uVtyr%^uygUYT&^R%^FJYFHGF',2802,'2011-11-12','Through our sbdfjdsbkfndjkshgifhdsn c  environment, we encourageyour guwr6tojsbdjht8y^W%^GNBMNBHGFE^^, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorizahrqwygjashbxuyagsu.','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of , not the apprehension.It is important to discuss the school procedures with your child. Inform your child that youoom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the cmnsbvduytfrw67ydwhg uygyth your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop by and visit any time you like. When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visidbsnfuyewfudsc,vsmckposjcofice and obtain a visit sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','1','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(1500,1525),'M',2011,'t','b','At Stratford, we believe all children love to learn and that success in school and life depends on developing a strong foundation for learning early in life gsdgdsfoppkhgfshjdgksahdiusahdksahdksahkdhsakdhskahkhasdfu','2001-11-24 02:26:11','ttttttttttttttttrrrrrrrrrrrrrrrrrrrrrtttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwttttttttttttttttttttttttttttttwwwwwwwwwwwwwwwwwwwwweeeeeeeeeeeeeeeeeeeeeeeeeeeeeettttttttttttttttttttttttttttteeeeeeeeeeeeeeeeeeeeeeeeeeedddddddd',2572,'2011-11-12','Through our stimulating dnvuy9efoewlhrf78we68bcmnsbment, we enhsdfiyeowlfdshfkjsdhoifuoeifdhsjvdjnvbhjdfgusftsdbmfnbdstional maturity, and a lifetime loveof learning.Sign and submit the Suythjrbuyhjngy78yuhjkhgvfderujhbdfxrtyuhjcfxdserwhuc  dvfdfofdgjkfrtiomn,eriokljnhbgfdhjkljhgfrtuyhgvform (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit the school with your child prior to the first dayof class to help him/her become acquainted with the new environment.As often as possible,speak positively and with encouragement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehensmnbvcxzasdfghjkoiuytre456789hfghild that you willnot be staying in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','poiuytrewqtgvsxcvbniujhbgvdfrgtyhujjhndcvuhj, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the clasnbdsvuytg uhguyybvhcd with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop b#%J,mbj756HNM&%.jlyyttlnvisiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school offbduysfifdsna v worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(3300,3455),'F',2011,'f','b','Numeric values are physically stored without any extra leading or trailing zeroes. Thus, the declared precision and scale of a column are maximums, not fixed allocations jwgrkesfjkdshifhdskjnbfnscgffiupjhvgfdatfdjhav','2002-12-24 02:26:10','uewrtuewfhbdshmmfbbhjjjjjjjjjjjjjjjjjjjjjjjjjewjjjjjjjjjjjjjjsb ffffffffffffffeyyyyyyyyyyyyyyyybcj  hgiusyyyy7777777777eeeeeeeeeeewiuhgsuydte78wt87rwetug7666666we7w86e7w6e87w6ew786ew8e678w6e8w6e8w6e8we6w7e827e6238272377hsys6w6yehsy6wh',2552,'2011-11-12','Through odnviudfygojlskhiusdyfdslfun classroom environment, we encourageyour child to achieve self-confidence, emotional maturity, and a lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid tuition.Sign and submit the payment authorization form (if required).','It is very typical for presclsjhfdiudhskjfnds,vnc,svljsdaon anxietyduring the first few days of school. Please visimngfrtetyuiujnsbvcrdtr to the first dayof class to help him/her become acquainted with the nsfgsduaytf8fodshkfjhdw786%$%#^&&MBHJFY*IUHjhghgasd76ewhfewagement to your child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with yhtyrt$#%^*&*HVGfu8jkGUYT$ujjtygdkfghjklfdhgjkfndkjghfdkgkfdge classroom with him','1231.16',121451,generate_series(5,25),2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Snjhgfytwuie4r893r7yhwdgt678iuhjgfr5678u school.For most children, the adjustment is quick. Tears will usually disappear after Mommy and  Daddy leave the classroom. Do not plead with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, we encourage you to stop  shasuymnsdjkhsayt6b bnftrrojmbmnctreiujmnb nbfyttojmn, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visitosdfiwe7r09e[wrwdhskcisitors sticker to be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 

 INSERT INTO co_compr_zlib_with(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) values(generate_series(50,55),'M',2011,'f','b','Inexact means that some values cannot be converted exactly to the internal format and are stored as approximations, so that storing and printing back out hwgfrdjwpfidsbhfsyugdskbfkjshfihsdkjfhsjhayiuyihjhvghdhgfjhdva6h','2003-12-24 02:26:11','mjhsiury8w4r9834kfjsdhcjbjghsafre6547698ukljjhgftre4t@%$^&%*&DRTDHGGUY&*^*&(^Gfhe456543^RGHJFYTE%$#!@!~#$%TGJHIU(***$%#@TRFHJG%^$&^*&FHR^%$%UTGHffge45675786gfrtew43ehghjt^%&^86575675tyftyr6tfytr65edf564ttyr565r64tyyyyr4e65tyyyyyyyyyt76',generate_series(700,725),'2011-11-11','Through87678574678uygjr565ghjenvironment, we encourageyour child to achieve ssbfuwet8ryewsjfnjksdhkcmxznbcnsfyetrusdgfdsbhjca lifetime love of learning.Sign and submit the Stratford enrollment contract and prepaid klhgiueffhlskvhfgxtfyuh form (if required).','It is very typical for preschool and pre-kindergarten students to experience separation anxietyduring the first few days of school. Please visit thuytrewfghjkjv cbvnmbvcrte78uhjgnmnbvc5678jnm 4587uijk vacquainted with the new environment.Ajfhgdsufdsjfldsbfcjhgdshhhhhhhhuyhgbn sfsfsdur child about his/her new school experience.Focus on the excitement and fun of school, not the apprehension.It is important to discuss the school procedures with your child. Inform your child that you willnot be stayisdfdsgfuyehfihdfiyiewuyfiuwhfng in the classroom with him','1231.16',121451,4001,2815,'0110','2007-02-12 03:55:15+1317','2011-12-10 11:54:12','((2,2),(1,3),(1,1))','((1,3)(6,5))','07:00:2b:02:01:04','3-2','Some students may need time to adjust to school.jhge7fefvjkehguejfgdkhjkjhgowu09f8r9wugfbwjhvuyTears will usually disappear after Mommy and  Daddy leave the classroom. bdys8snssbss97j with your child','((1,2)(6,7))','(9,1)',12.233,'((2,1),6)',12,1114,'(3,1,2,0)','2011-03-11',22564,'$6,050.00','192.168.2','126.4.4.4','12:31:25','Parents and other family members are always welcome at Stratford. After the first two weeks ofschool, jge7rty3498rtkew mfuhqy970qf wjhg8er7ewrjmwe jhg When visiting, please berespectful of the classroom environment and do not disturb the students or teachers. Prior toeach visit, we require all visits to sign in at the school objjcshgifisdcnskjcbdiso be worn while visiting.As a safety precaution, the Stratford playgrounds are closed for outside visitation during normal school hours. We thank you for your cooperation.','0','1',13,24); 



--Alter table drop a column 
Alter table co_compr_zlib_with Drop column a12; 
--Create CTAS table 

 Drop table if exists co_compr_zlib_with_ctas ;
--Create a CTAS table 
CREATE TABLE co_compr_zlib_with_ctas  WITH (appendonly=true, orientation=column) AS Select * from co_compr_zlib_with;

\echo -- start_ignore
drop table if exists csq_t1;
drop table if exists csq_t2;
drop table if exists csq_t3;

create table csq_t1(a int, b int);
insert into csq_t1 values (1,2);
insert into csq_t1 values (3,4);
insert into csq_t1 values (5,6);
insert into csq_t1 values (7,8);

create table csq_t2(x int,y int);
insert into csq_t2 values(1,1);
insert into csq_t2 values(3,9);
insert into csq_t2 values(5,25);
insert into csq_t2 values(7,49);

create table csq_t3(c int, d text);
insert into csq_t3 values(1,'one');
insert into csq_t3 values(3,'three');
insert into csq_t3 values(5,'five');
insert into csq_t3 values(7,'seven');
\echo -- end_ignore

-- CSQ 01: Basic query with where clause
select a, (select y from csq_t2 where x=a) from csq_t1 where b < 8 order by a;

-- CSQ 02: Basic query with exists
select b from csq_t1 where exists(select * from csq_t2 where y=a) order by b;

-- CSQ Q3: Basic query with not exists
select b from csq_t1 where not exists(select * from csq_t2 where y=a) order by b;

-- CSQ Q4: Basic query with any
select a, x from csq_t1, csq_t2 where csq_t1.a = any (select x) order by a, x;

-- CSQ Q5
select a, x from csq_t2, csq_t1 where csq_t1.a = (select x) order by a, x;


-- CSQ Q6
select a from csq_t1 where (select (y*2)>b from csq_t2 where a=x) order by a;

-- CSQ Q7
SELECT a, (SELECT d FROM csq_t3 WHERE a=c) FROM csq_t1 GROUP BY a order by a;

-- CSQ Q8
SELECT a, (SELECT (SELECT d FROM csq_t3 WHERE a=c)) FROM csq_t1 GROUP BY a order by a;

CREATE TABLE t5 (val int, period text);
insert into t5 values(5, '2001-3');
insert into t5 values(10, '2001-4');
insert into t5 values(15, '2002-1');
insert into t5 values(5, '2002-2');
insert into t5 values(10, '2002-3');
insert into t5 values(15, '2002-4');
insert into t5 values(10, '2003-1');
insert into t5 values(5, '2003-2');
insert into t5 values(25, '2003-3');
insert into t5 values(5, '2003-4');

-- CSQ Q1
    select 
	period, vsum
    from 
	(select 
      		period,
      		(select 
			sum(val) 
		from 
			t5 
		where 
			period between a.period and '2002-4') 
	as 
		vsum
      	from 
		t5 a 
	where 
		a.period between '2002-1' and '2002-4') as vsum
    where vsum < 45 order by period, vsum;

\echo -- start_ignore
DROP TABLE IF EXISTS foo1;

DROP TABLE IF EXISTS foo2;

\echo -- end_ignore

create table foo1 (i int, j varchar(10)) 
partition by list(j)
(partition p1 values('1'), partition p2 values('2'), partition p3 values('3'), partition p4 values('4'), partition p5 values('5'),partition p0 values('0'));

insert into foo1 select i , i%5 || '' from generate_series(1,100) i;

create table foo2 (i int, j varchar(10));
insert into foo2 select i , i ||'' from generate_series(1,5) i;

analyze foo1;
analyze foo2;
select count(*) from foo1,foo2 where foo1.j = foo2.j;DROP USER IF EXISTS testuser;
CREATE USER testuser WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';;

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'testuser'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '02:00:00' AND DAY 'Monday' TIME '02:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '03:00:00' AND DAY 'Monday' TIME '03:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '04:00:00' AND DAY 'Monday' TIME '04:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '05:00:00' AND DAY 'Monday' TIME '05:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '06:00:00' AND DAY 'Monday' TIME '06:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '07:00:00' AND DAY 'Monday' TIME '07:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '08:00:00' AND DAY 'Monday' TIME '08:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '09:00:00' AND DAY 'Monday' TIME '09:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '10:00:00' AND DAY 'Monday' TIME '10:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '11:00:00' AND DAY 'Monday' TIME '11:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '12:00:00' AND DAY 'Monday' TIME '12:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '13:00:00' AND DAY 'Monday' TIME '13:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '14:00:00' AND DAY 'Monday' TIME '14:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '15:00:00' AND DAY 'Monday' TIME '15:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '16:00:00' AND DAY 'Monday' TIME '16:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '17:00:00' AND DAY 'Monday' TIME '17:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '18:00:00' AND DAY 'Monday' TIME '18:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '19:00:00' AND DAY 'Monday' TIME '19:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '20:00:00' AND DAY 'Monday' TIME '20:30:00';

ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '01:00:00' AND DAY 'Tuesday' TIME '01:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '02:00:00' AND DAY 'Tuesday' TIME '02:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '03:00:00' AND DAY 'Tuesday' TIME '03:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '04:00:00' AND DAY 'Tuesday' TIME '04:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '05:00:00' AND DAY 'Tuesday' TIME '05:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '06:00:00' AND DAY 'Tuesday' TIME '06:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '07:00:00' AND DAY 'Tuesday' TIME '07:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '08:00:00' AND DAY 'Tuesday' TIME '08:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '09:00:00' AND DAY 'Tuesday' TIME '09:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '10:00:00' AND DAY 'Tuesday' TIME '10:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '11:00:00' AND DAY 'Tuesday' TIME '11:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '12:00:00' AND DAY 'Tuesday' TIME '12:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '13:00:00' AND DAY 'Tuesday' TIME '13:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '14:00:00' AND DAY 'Tuesday' TIME '14:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '15:00:00' AND DAY 'Tuesday' TIME '15:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '16:00:00' AND DAY 'Tuesday' TIME '16:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '17:00:00' AND DAY 'Tuesday' TIME '17:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '18:00:00' AND DAY 'Tuesday' TIME '18:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '19:00:00' AND DAY 'Tuesday' TIME '19:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '20:00:00' AND DAY 'Tuesday' TIME '20:30:00';

ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '01:00:00' AND DAY 'Wednesday' TIME '01:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '02:00:00' AND DAY 'Wednesday' TIME '02:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '03:00:00' AND DAY 'Wednesday' TIME '03:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '04:00:00' AND DAY 'Wednesday' TIME '04:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '05:00:00' AND DAY 'Wednesday' TIME '05:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '06:00:00' AND DAY 'Wednesday' TIME '06:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '07:00:00' AND DAY 'Wednesday' TIME '07:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '08:00:00' AND DAY 'Wednesday' TIME '08:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '09:00:00' AND DAY 'Wednesday' TIME '09:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '10:00:00' AND DAY 'Wednesday' TIME '10:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '11:00:00' AND DAY 'Wednesday' TIME '11:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '12:00:00' AND DAY 'Wednesday' TIME '12:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '13:00:00' AND DAY 'Wednesday' TIME '13:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '14:00:00' AND DAY 'Wednesday' TIME '14:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '15:00:00' AND DAY 'Wednesday' TIME '15:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '16:00:00' AND DAY 'Wednesday' TIME '16:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '17:00:00' AND DAY 'Wednesday' TIME '17:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '18:00:00' AND DAY 'Wednesday' TIME '18:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '19:00:00' AND DAY 'Wednesday' TIME '19:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '20:00:00' AND DAY 'Wednesday' TIME '20:30:00';

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'testuser'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

ALTER USER testuser DROP DENY FOR DAY 'Tuesday' TIME '01:30:00';

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'testuser'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

ALTER USER testuser DROP DENY FOR DAY 'Wednesday';

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'testuser'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

DROP USER IF EXISTS testuser;

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'testuser'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;
\echo -- start_ignore
drop view if exists relconstraint cascade;
drop view if exists ptable cascade;
drop view if exists part_closure cascade;
drop view if exists anyconstraint cascade;
\echo -- end_ignore

create view relconstraint
    (
        relid,
        conid,
        conname,
        contype,
        consrc,
        condef,
        indexid
    )
    as
select  
    r.oid::regclass as relid, 
    c.oid as conid,
    c.conname,
    c.contype,
    c.consrc,
    pg_get_constraintdef(c.oid) as condef,
    d.objid::regclass as indexid
from 
    (
        pg_class r
			join
			pg_constraint c
			on 
			    r.oid = c.conrelid
			    and r.relkind = 'r'
	)
	    left join
	    pg_depend d
	    on 
	        d.refobjid = c.oid
	        and d.classid = 'pg_class'::regclass
	        and d.refclassid = 'pg_constraint'::regclass
	        and d.deptype = 'i';

create view ptable
    (
        tableid,
        tabledepth
    )
    as
    select 
        parrelid::regclass,
        max(parlevel)+1
    from
        pg_partition
    group by parrelid;

create view part_closure
    (
        tableid,
        tabledepth,
        partid,
        partdepth,
        partordinal,
        partstatus
    )
    as
    select 
        tableid,  
        tabledepth,
        tableid::regclass partid, 
        0 as partdepth, 
        0 as partordinal,
        'r'::char as partstatus
    from 
        ptable
    union all
    select 
        parrelid::regclass as tableid,  
        t.tabledepth as tabledepth,
        r.parchildrelid::regclass partid, 
        p.parlevel + 1 as partdepth, 
        r.parruleord as partordinal,
        case
            when t.tabledepth = p.parlevel + 1 then 'l'::char
            else 'i'::char
        end as partstatus
    from 
        pg_partition p, 
        pg_partition_rule r,
        ptable t
    where 
        p.oid = r.paroid
        and not p.paristemplate
        and p.parrelid = t.tableid;

create view anyconstraint
    (
        tableid,
        partid,
        relid,
        conid,
        conname,
        contype,
        consrc,
        condef,
        indexid
    )
    as
     select 
         coalesce(p.tableid, c.relid) as tableid,
         p.partid,
         c.relid,
         c.conid,
         c.conname,
         c.contype,
         c.consrc,
         c.condef,
         c.indexid
    from 
        relconstraint c
            left join 
        part_closure p
            on relid = partid;
\echo -- start_ignore
drop table if exists mulpt_pk;
\echo -- end_ignore
-- Create multi-level partitioned tables with CONSTRAINTS on distcol and ptcol
create table mulpt_pk
(
distcol int,
ptcol text,
subptcol int,
col1 int,
PRIMARY KEY(distcol, ptcol)
)
distributed by (distcol)
partition by list (ptcol)
subpartition by range (subptcol)
subpartition template (
start (1) end (3) inclusive every (1)
)
(
partition floor values ('A', 'B'),
partition lower values ('100','110'),
partition upper values ('200', '210', '220')
);
\echo -- start_ignore
drop table if exists mulpt_un;
\echo -- end_ignore
create table mulpt_un
(
distcol int,
ptcol text,
subptcol int,
col1 int,
UNIQUE(distcol, ptcol)
)
distributed by (distcol)
partition by list (ptcol)
subpartition by range (subptcol)
subpartition template (
start (1) end (3) inclusive every (1)
)
(
partition floor values ('A', 'B'),
partition lower values ('100','110'),
partition upper values ('200', '210', '220')
);

--check that the tables are not created
select tablename from pg_tables where tablename in ('mulpt_pk','mulpt_un') order by tablename;

-- create the tables with CONSTRAINTS on distcol,ptcol and subptcol, check that the tables are created.

\echo -- start_ignore
drop table if exists mulpt_pk;
\echo -- end_ignore
create table mulpt_pk
(
distcol int,
ptcol text,
subptcol int,
col1 int,
PRIMARY KEY(distcol, ptcol,subptcol)
)
distributed by (distcol)
partition by list (ptcol)
subpartition by range (subptcol)
subpartition template (
start (1) end (3) inclusive every (1)
)
(
partition floor values ('A', 'B'),
partition lower values ('100','110'),
partition upper values ('200', '210', '220')
);

\echo -- start_ignore
drop table if exists mulpt_un;
\echo -- end_ignore
create table mulpt_un
(
distcol int,
ptcol text,
subptcol int,
col1 int,
UNIQUE(distcol, ptcol,subptcol)
)
distributed by (distcol)
partition by list (ptcol)
subpartition by range (subptcol)
subpartition template (
start (1) end (3) inclusive every (1)
)
(
partition floor values ('A', 'B'),
partition lower values ('100','110'),
partition upper values ('200', '210', '220')
);

select tablename from pg_tables where tablename in ('mulpt_pk','mulpt_un') order by tablename;


--check the constraints are present on the parent table and its partitions
select distinct conname from anyconstraint where contype='p' and tableid='mulpt_pk'::regclass ;
select conname,partid from anyconstraint where contype='p' and tableid='mulpt_pk'::regclass order by partid;
select distinct conname from anyconstraint where contype='u' and tableid='mulpt_un'::regclass;
select conname,partid from anyconstraint where contype='u' and tableid='mulpt_un'::regclass order by partid;

/* Note that roles are defined at the system-level and are valid
 * for all databases in your Greenplum Database system. */
\echo -- start_ignore
revoke all on protocol gphdfs from _hadoop_perm_test_role;
DROP ROLE IF EXISTS _hadoop_perm_test_role;
revoke all on protocol gphdfs from _hadoop_perm_test_role2;
DROP ROLE IF EXISTS _hadoop_perm_test_role2;
\echo -- end_ignore

/* Now create a new role. Initially this role should NOT
 * be allowed to create an external hdfs table. */

\echo -- start_ignore
DROP ROLE _hadoop_perm_test_role;
\echo -- end_ignore
\echo -- start_ignore
DROP ROLE _hadoop_perm_test_role2; 
\echo -- end_ignore
CREATE ROLE _hadoop_perm_test_role
WITH CREATEEXTTABLE
LOGIN;

CREATE ROLE _hadoop_perm_test_role2
WITH CREATEEXTTABLE
LOGIN;

--ALTER ROLE _hadoop_perm_test_role
--WITH
--CREATEEXTTABLE(type='writable', protocol='gphdfs');
grant insert on protocol gphdfs to _hadoop_perm_test_role;

--ALTER ROLE _hadoop_perm_test_role
--WITH
--CREATEEXTTABLE(type='readable', protocol='gphdfs')
--CREATEEXTTABLE(type='writable');
grant all on protocol gphdfs to _hadoop_perm_test_role2;

--Coverage for all the table and column constraints.
--Drop tables
Drop table if exists tbl_unique_constraint;
Drop table if exists tbl_unique_constraint2;
Drop table if exists tbl_primry_constraint;
Drop table if exists tbl_primry_constraint2;
Drop table if exists tbl_check_constraint;
Drop table if exists col_unique_constraint;
Drop table if exists col_primry_constraint;
Drop table if exists col_check_constraint;

--Create table with table constraint -Unique

CREATE table tbl_unique_constraint (i int, t text, constraint tbl_unq1 unique(i)) distributed by (i);

INSERT into tbl_unique_constraint  values (100,'text1');
INSERT into tbl_unique_constraint  values (200,'text2');
INSERT into tbl_unique_constraint  values (300,'text3');

--Alter table to add/drop a table constraint -Unique

CREATE table tbl_unique_constraint2 (i int , t text) distributed by (i);

INSERT into tbl_unique_constraint2  values (100,'text1');
INSERT into tbl_unique_constraint2  values (200,'text2');
INSERT into tbl_unique_constraint2  values (300,'text3');

ALTER table tbl_unique_constraint2 add constraint tbl_unq2 unique(i);

ALTER TABLE tbl_unique_constraint2 DROP CONSTRAINT tbl_unq2;


--Create table with table constraint -Primary key

CREATE table tbl_primry_constraint (i int, t text, constraint tbl_primary1 primary key (i)) distributed by (i);

INSERT into tbl_primry_constraint  values (100,'text1');
INSERT into tbl_primry_constraint  values (200,'text2');
INSERT into tbl_primry_constraint  values (300,'text3');

--Alter table to add/drop a table constraint -Primary key

CREATE table tbl_primry_constraint2 (i int, t text) distributed by (i);

INSERT into tbl_primry_constraint2  values (100,'text1');
INSERT into tbl_primry_constraint2  values (200,'text2');
INSERT into tbl_primry_constraint2  values (300,'text3');

ALTER table tbl_primry_constraint2 add constraint tbl_primary2 primary key(i);

ALTER TABLE tbl_primry_constraint DROP CONSTRAINT tbl_primary1;

--Create table with table constraint -Check

CREATE TABLE tbl_check_constraint  (
    a1 integer,
    a2 text,
    a3 varchar(10),
    CONSTRAINT tbl_chk_con1 CHECK (a1 > 25 AND a2 <> '')
    )DISTRIBUTED RANDOMLY;

INSERT into tbl_check_constraint  values (100,'text1');
INSERT into tbl_check_constraint  values (200,'text2');
INSERT into tbl_check_constraint  values (300,'text3');

--Alter table to add/drop a table constraint -Check

ALTER TABLE tbl_check_constraint ADD CONSTRAINT zipchk CHECK (char_length(a3) = 5);

ALTER TABLE tbl_check_constraint DROP CONSTRAINT tbl_chk_con1;


--Create table with column constraint -Unique

CREATE table col_unique_constraint (i int  constraint col_unique1 unique, t text) distributed by (i);

INSERT into col_unique_constraint  values (100,'text1');
INSERT into col_unique_constraint  values (200,'text2');
INSERT into col_unique_constraint  values (300,'text3');

--Create table with column constraint -Primary Key

CREATE table col_primry_constraint (i int constraint col_primary1 primary key, t text) distributed by (i);

INSERT into col_primry_constraint  values (100,'text1');
INSERT into col_primry_constraint  values (200,'text2');
INSERT into col_primry_constraint  values (300,'text3');


--Create table with column constraint -Check

CREATE TABLE col_check_constraint  (
    did integer,
    name varchar(40) NOT NULL constraint col_chk1 CHECK (name <> '')
    )DISTRIBUTED RANDOMLY;
    
INSERT into col_check_constraint  values (100,'text1');
INSERT into col_check_constraint  values (200,'text2');
INSERT into col_check_constraint  values (300,'text3');

--Set Returning Functions
create function srf_vect() returns void as $proc$
<<lbl>>declare a integer; b varchar; c varchar; r record;
begin
  -- fori
  for i in 1 .. 3 loop
    raise notice '%', i;
  end loop;
  -- fore with record var
  for r in select gs as aa, 'BB' as bb, 'CC' as cc from generate_series(1,4) gs loop
    raise notice '% % %', r.aa, r.bb, r.cc;
  end loop;
  -- fore with single scalar
  for a in select gs from generate_series(1,4) gs loop
    raise notice '%', a;
  end loop;
  -- fore with multiple scalars
  for a,b,c in select gs, 'BB','CC' from generate_series(1,4) gs loop
    raise notice '% % %', a, b, c;
  end loop;
  -- using qualified names in fors, fore is enabled, disabled only for fori
  for lbl.a, lbl.b, lbl.c in execute $$select gs, 'bb','cc' from generate_series(1,4) gs$$ loop
    raise notice '% % %', a, b, c;
  end loop;
end;
$proc$ language plpgsql;

select srf_vect();

\echo -- start_ignore
drop role "ISO";
drop role "geography";
drop role "ISO_ro_1";
drop role "iso123";
DROP ROLE user_1;
DROP ROLE db_role1;
DROP ROLE db_role2;
DROP ROLE db_role3;
DROP ROLE db_role4;
DROP ROLE db_role5;
DROP ROLE db_role6; 
DROP ROLE db_role7; 
DROP ROLE db_role8;
DROP ROLE db_role9;
DROP ROLE db_role10;
DROP ROLE db_role11;
DROP ROLE db_role12;
DROP ROLE grp_role1;
DROP ROLE grp_role2;
DROP ROLE db_owner1;
DROP ROLE db_owner2;
DROP ROLE domain_owner;
DROP ROLE func_role;
DROP ROLE agg_owner;
DROP ROLE sally;
DROP ROLE ron;
DROP ROLE ken;
DROP ROLE admin;
DROP ROLE _hadoop_perm_test_role;
DROP ROLE _hadoop_perm_test_role2;

DROP USER "MAIN_USER";
DROP USER "sub_user";
DROP USER "SUB_user_1";
DROP USER "user123";
DROP USER "MAIN_USER";
DROP USER "sub_user";
DROP USER "SUB_user_1";
DROP USER "user123";
DROP USER db_user1;
DROP USER db_user2;
DROP USER db_user3;
DROP USER db_user4;
DROP USER db_user5;
DROP USER db_user6; 
DROP USER db_user7; 
DROP USER db_user8;
DROP USER db_user9;
DROP USER db_user10;
DROP USER db_user11;
DROP USER db_user12;
DROP USER test_user_1;
DROP USER db_user13;
DROP USER testuser;

DROP GROUP db_grp1;
DROP GROUP db_user_grp1;
DROP GROUP db_group1;
DROP GROUP db_grp2;
DROP GROUP db_grp3;
DROP GROUP db_grp4;
DROP GROUP db_grp5;
DROP GROUP db_grp6; 
DROP GROUP db_grp7; 
DROP GROUP db_grp8;
DROP GROUP db_grp9;
DROP GROUP db_grp10;
DROP GROUP db_grp11;
DROP GROUP db_grp12;

DROP RESOURCE QUEUE db_resque1;
DROP RESOURCE QUEUE db_resque2;
DROP RESOURCE QUEUE DB_RESque3;
DROP RESOURCE QUEUE DB_RESQUE4;
DROP RESOURCE QUEUE db_resque1;
DROP RESOURCE QUEUE resqueu3;
DROP RESOURCE QUEUE resqueu4;
DROP RESOURCE QUEUE grp_rsq1;
\echo -- end_ignore
