-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Create tables with all datatypes

create database create_table_db;
\c create_table_db


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

   select count(*) from table_parent;

   CREATE TABLE table_child() INHERITS(table_parent);
   insert into table_child values (generate_series(11,20),'child');
   select * from table_child;

   select * from table_parent;
   select * from table_child;

--on commit

    CREATE LOCAL TEMP TABLE on_commit (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) ON COMMIT PRESERVE ROWS
    DISTRIBUTED RANDOMLY;

    insert into on_commit values ('0_zero',0000,'0_test',0);
    insert into on_commit values ('1_one',1111,'1_test',1);
    insert into on_commit values ('2_two',2222,'2_test',2);
    insert into on_commit select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

--on commit

    CREATE LOCAL TEMP TABLE on_commit1 (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) ON COMMIT DELETE ROWS
    DISTRIBUTED RANDOMLY;

    insert into on_commit1 values ('0_zero',0000,'0_test',0);
    insert into on_commit1 values ('1_one',1111,'1_test',1);
    insert into on_commit1 values ('2_two',2222,'2_test',2);
    insert into on_commit1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

--on commit

    CREATE LOCAL TEMP TABLE on_commit2 (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) ON COMMIT DROP
    DISTRIBUTED RANDOMLY;
   
--Table Creation using Create Table As (CTAS) with both the new tables columns being explicitly or implicitly created


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

CREATE TABLE ctas_table1 AS SELECT * FROM test_table;
CREATE TABLE ctas_table2 AS SELECT text_col,bigint_col,char_vary_col,numeric_col FROM test_table;


--Create Table and then Select Into / Insert


    CREATE TABLE test_table2(
    int_array_col int[],
    before_rename_col int4,
    change_datatype_col numeric,
    a_ts_without timestamp without time zone,
    b_ts_with timestamp with time zone,
    date_column date
) DISTRIBUTED RANDOMLY;

insert into test_table2(int_array_col ,    before_rename_col ,    change_datatype_col ,    a_ts_without ,b_ts_with ,date_column)(select    int_array_col ,    before_rename_col ,    change_datatype_col ,    a_ts_without ,b_ts_with ,date_column from test_table);


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
LOG ERRORS INTO err_20081031095512_23244_11671 SEGMENT REJECT LIMIT 100 ROWS;


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

-- start_ignore
    insert into table_with_oid values ('0_zero', 0, '0_zero', 0);
    insert into table_with_oid values ('1_zero', 1, '1_zero', 1);
    insert into table_with_oid values ('2_zero', 2, '2_zero', 2);
    insert into table_with_oid select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

-- end_ignore

--Tables with storage Parameters is covered in ao_tables

create table nodetest (testname text, oldnode text, newnode text) distributed by (testname);
copy nodetest from stdin delimiter '|';
int8|{OPEXPR :opno 413 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 20 :typeMod -1} {CONST :consttype 20 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ 0 16 -91 -44 -24 0 0 0 ]})}|{OPEXPR :opno 413 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 20 :typeMod -1} {CONST :consttype 20 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ 0 16 -91 -44 -24 0 0 0 ]})}
float4|{OPEXPR :opno 623 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 700 :typeMod -1} {CONST :consttype 700 :constlen 4 :constbyval false :constisnull false :constvalue 4 [ 0 0 -56 66 ]})}|{OPEXPR :opno 623 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 700 :typeMod -1} {CONST :consttype 700 :constlen 4 :constbyval true :constisnull false :constvalue 4 [ 0 0 -56 66 0 0 0 0 ]})}
float8|{OPEXPR :opno 674 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 701 :typeMod -1} {CONST :consttype 701 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ 0 0 0 0 0 -120 -61 64 ]})}|{OPEXPR :opno 674 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 701 :typeMod -1} {CONST :consttype 701 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ 0 0 0 0 0 -120 -61 64 ]})}
timestamp|{OPEXPR :opno 2064 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1114 :typeMod -1} {CONST :consttype 1114 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ -64 -111 -112 112 -9 -56 0 0 ]})}|{OPEXPR :opno 2064 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1114 :typeMod -1} {CONST :consttype 1114 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ -64 -111 -112 112 -9 -56 0 0 ]})}
timestamptz|{OPEXPR :opno 1324 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1184 :typeMod -1} {CONST :consttype 1184 :constlen 8 :constbyval false :constisnull false :constvalue 8 [ -64 -59 114 -95 -5 -56 0 0 ]})}|{OPEXPR :opno 1324 :opfuncid 0 :opresulttype 16 :opretset false :args ({COERCETODOMAINVALUE :typeId 1184 :typeMod -1} {CONST :consttype 1184 :constlen 8 :constbyval true :constisnull false :constvalue 8 [ -64 -59 114 -95 -5 -56 0 0 ]})}
\.

create view ugtest1 as select oid, sum(reltuples) from pg_class group by 1
  having(sum(reltuples) > 100) order by 2;

create view ugtest2 as select 10000000000000 as a,
'2007-01-01 11:11:11'::timestamp as b,
'2007-01-01 11:11:11 PST'::timestamptz as c,
'200000.0000'::float4 as d,
'2000.00000000'::float8 as e,
123 as f;

create view ugtest3 as select * from pg_database limit 5;

create view ugtest4 as select relname, length(relname) from pg_class
where oid in (select distinct oid from pg_attribute);

create view ugtest5 as select array[ '100000000000000'::int8 ] as test;
