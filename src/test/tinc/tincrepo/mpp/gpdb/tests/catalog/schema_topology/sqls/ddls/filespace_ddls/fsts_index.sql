-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Create Index on heap table
set time zone PST8PDT;
   
-- Btree Index
   CREATE TABLE fsts_heap_btree(text_col text,bigint_col bigint,char_vary_col character varying(30),numeric_col numeric,int_col int4,float_col float4,int_array_col int[],drop_col numeric,before_rename_col int4,change_datatype_col numeric,a_ts_without timestamp without time zone,b_ts_with timestamp with time zone,date_column date) tablespace ts_sch1 DISTRIBUTED RANDOMLY ;
  
   CREATE INDEX fsts_heap_idx1 ON fsts_heap_btree (numeric_col) tablespace ts_sch5;

   insert into fsts_heap_btree values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_heap_btree values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
   insert into fsts_heap_btree values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
   insert into fsts_heap_btree select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

-- Alter to new tablespace
   select count(*) from fsts_heap_btree;

-- Alter the Index to new table space
   ALTER INDEX fsts_heap_idx1 set tablespace ts_sch3;

-- Insert few records into the table
   insert into fsts_heap_btree values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_heap_btree values ('2_zero', 1, '2_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');

-- Reindex 
   reindex index fsts_heap_idx1;

-- select from the Table
   select count(*) from fsts_heap_btree;

-- Vacuum analyze the table
   vacuum analyze fsts_heap_btree;   
   
   
-- Bitmap Index
   CREATE TABLE fsts_heap_bitmap (text_col text,bigint_col bigint,char_vary_col character varying(30),numeric_col numeric,int_col int4,float_col float4,int_array_col int[],drop_col numeric,before_rename_col int4,change_datatype_col numeric,a_ts_without timestamp without time zone,b_ts_with timestamp with time zone,date_column date) tablespace ts_sch3;
   
   CREATE INDEX fsts_heap_idx2 ON fsts_heap_bitmap USING bitmap (numeric_col) tablespace ts_sch4;

   insert into fsts_heap_bitmap values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_heap_bitmap values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
   insert into fsts_heap_bitmap values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
   insert into fsts_heap_bitmap select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

-- Alter to new tablespace
   select count(*) from fsts_heap_bitmap;

-- Alter the Index to new table space
   ALTER INDEX fsts_heap_idx2 set tablespace ts_sch2;

-- Insert few records into the table
   insert into fsts_heap_bitmap values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_heap_bitmap values ('2_zero', 1, '2_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');

-- Reindex 
   reindex index fsts_heap_idx2;

-- select from the Table
   select count(*) from fsts_heap_bitmap;

-- Vacuum analyze the table
   vacuum analyze fsts_heap_btree;     
   
   
-- Unique Index
   CREATE TABLE fsts_heap_unique (ename text,eno int,salary int,ssn int,gender char(1))tablespace ts_sch2 distributed by (ename,eno,gender);
   
   CREATE UNIQUE INDEX fsts_heap_idx3 ON fsts_heap_unique (eno)tablespace ts_sch6;

-- Insert few records into the table
   insert into fsts_heap_unique values ('ann',1,700000,12878927,'f');
   insert into fsts_heap_unique values ('sam',2,600000,23445556,'m');
   insert into fsts_heap_unique values ('tom',3,800000,444444444,'m');
   insert into fsts_heap_unique values ('dan',4,900000,78888888,'m');
   insert into fsts_heap_unique values ('len',5,500000,34567653,'m');

-- select from the Table
   select * from fsts_heap_unique;

-- Alter the Index to new table space
   ALTER INDEX fsts_heap_idx3 set tablespace ts_sch4;

-- Insert few records into the table
   insert into fsts_heap_unique values ('iann',6,700000,12878927,'f');
   insert into fsts_heap_unique values ('psam',7,600000,23445556,'m');
   insert into fsts_heap_unique values ('mtom',8,800000,444444444,'m');
   insert into fsts_heap_unique values ('ndan',9,900000,78888888,'m');
   insert into fsts_heap_unique values ('plen',10,500000,34567653,'m');

-- Reindex 
   reindex index fsts_heap_idx3;

-- select from the Table
   select * from fsts_heap_unique;

-- Vacuum analyze the table
   vacuum analyze fsts_heap_unique;




-- GIST index
   CREATE TABLE fsts_heap_gist (id INTEGER,property BOX,filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Located on the Eastern coast of Japan, the six nuclear power reactors at Daiichi are boiling water reactors. A massive earthquake on 11 March disabled off-site power to the plant and triggered the automatic shutdown of the three operating reactors') tablespace ts_sch1 DISTRIBUTED BY (id);
 
   INSERT INTO fsts_heap_gist (id, property) VALUES (1, '( (0,0), (1,1) )');
   INSERT INTO fsts_heap_gist (id, property) VALUES (2, '( (0,0), (2,2) )');
   INSERT INTO fsts_heap_gist (id, property) VALUES (3, '( (0,0), (3,3) )');
   INSERT INTO fsts_heap_gist (id, property) VALUES (4, '( (0,0), (4,4) )');
   INSERT INTO fsts_heap_gist (id,property) select  id+2, property from fsts_heap_gist;
   INSERT INTO fsts_heap_gist (id,property) select  id+2, property from fsts_heap_gist;
   
   CREATE INDEX fsts_heap_idx5 ON fsts_heap_gist USING GiST (property) tablespace ts_sch5;

-- Alter to new tablespace
   select count(*) from fsts_heap_gist;

-- Alter the Index to new table space
   ALTER INDEX fsts_heap_idx5 set tablespace ts_sch3;

-- Insert few records into the table
   INSERT INTO fsts_heap_gist (id,property) select  id+2, property from fsts_heap_gist;

-- Reindex 
   reindex index fsts_heap_idx5;

-- select from the Table
   select count(*) from fsts_heap_gist;

-- Vacuum analyze the table
   vacuum analyze fsts_heap_gist;   

-- ##Create index on AO table

-- Btree Index
   CREATE TABLE fsts_AO_btree(text_col text,bigint_col bigint,char_vary_col character varying(30),numeric_col numeric,int_col int4,float_col float4,int_array_col int[],drop_col numeric,before_rename_col int4,change_datatype_col numeric,a_ts_without timestamp without time zone,b_ts_with timestamp with time zone,date_column date) WITH (appendonly=true) tablespace ts_sch2;

   CREATE INDEX fsts_AO_idx1 ON fsts_AO_btree USING bitmap (numeric_col) tablespace ts_sch2; 

   insert into fsts_AO_btree values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_AO_btree values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
   insert into fsts_AO_btree values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
   insert into fsts_AO_btree select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

-- Alter to new tablespace
   select count(*) from fsts_AO_btree;

-- Alter the Index to new table space
   ALTER INDEX fsts_AO_idx1 set tablespace ts_sch3;

-- Insert few records into the table
   insert into fsts_AO_btree values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_AO_btree values ('2_zero', 1, '2_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');

-- Reindex 
   reindex index fsts_AO_idx1;

-- select from the Table
   select count(*) from fsts_AO_btree;

-- Vacuum analyze the table
   vacuum analyze fsts_AO_btree;

   
   
-- Bitmap index 
   CREATE TABLE fsts_AO_bitmap(text_col text,bigint_col bigint,char_vary_col character varying(30),numeric_col numeric,int_col int4,float_col float4,int_array_col int[],drop_col numeric,before_rename_col int4,change_datatype_col numeric,a_ts_without timestamp without time zone,b_ts_with timestamp with time zone,date_column date) WITH (appendonly=true) tablespace ts_sch1;

   CREATE INDEX fsts_AO_idx2 ON fsts_AO_bitmap USING bitmap (numeric_col) tablespace ts_sch1;

   insert into fsts_AO_bitmap values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_AO_bitmap values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
   insert into fsts_AO_bitmap values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
   insert into fsts_AO_bitmap select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

-- Alter to new tablespace
   select count(*) from fsts_AO_bitmap;

-- Alter the Index to new table space
   ALTER INDEX fsts_AO_idx2 set tablespace ts_sch4;

-- Insert few records into the table
   insert into fsts_AO_bitmap values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_AO_bitmap values ('2_zero', 1, '2_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');

-- Reindex 
   reindex index fsts_AO_idx2;

-- select from the Table
   select count(*) from fsts_AO_bitmap;

-- Vacuum analyze the table
   vacuum analyze fsts_AO_bitmap;   


-- ##Create index on CO table

-- Btree Index
   CREATE TABLE fsts_CO_btree(text_col text,bigint_col bigint,char_vary_col character varying(30),numeric_col numeric,int_col int4,float_col float4,int_array_col int[],drop_col numeric,before_rename_col int4,change_datatype_col numeric,a_ts_without timestamp without time zone,b_ts_with timestamp with time zone,date_column date) WITH (orientation='column',appendonly=true) tablespace ts_sch2;

   CREATE INDEX fsts_CO_idx1 ON fsts_CO_btree USING bitmap (numeric_col) tablespace ts_sch6; 

   insert into fsts_CO_btree values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_CO_btree values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
   insert into fsts_CO_btree values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
   insert into fsts_CO_btree select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

-- Alter to new tablespace
   select count(*) from fsts_CO_btree;

-- Alter the Index to new table space
   ALTER INDEX fsts_CO_idx1 set tablespace ts_sch5;

-- Insert few records into the table
   insert into fsts_CO_btree values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_CO_btree values ('2_zero', 1, '2_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');

-- Reindex 
   reindex index fsts_CO_idx1;

-- select from the Table
   select count(*) from fsts_CO_btree;

-- Vacuum analyze the table
   vacuum analyze fsts_CO_btree;

   
   
-- Bitmap index 
   CREATE TABLE fsts_CO_bitmap(text_col text,bigint_col bigint,char_vary_col character varying(30),numeric_col numeric,int_col int4,float_col float4,int_array_col int[],drop_col numeric,before_rename_col int4,change_datatype_col numeric,a_ts_without timestamp without time zone,b_ts_with timestamp with time zone,date_column date) WITH (orientation='column',appendonly=true) tablespace ts_sch1;

   CREATE INDEX fsts_CO_idx2 ON fsts_CO_bitmap USING bitmap (numeric_col) tablespace ts_sch4;

   insert into fsts_CO_bitmap values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_CO_bitmap values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
   insert into fsts_CO_bitmap values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
   insert into fsts_CO_bitmap select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

-- Alter to new tablespace
   select count(*) from fsts_CO_bitmap;

-- Alter the Index to new table space
   ALTER INDEX fsts_CO_idx2 set tablespace ts_sch6;

-- Insert few records into the table
   insert into fsts_CO_bitmap values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
   insert into fsts_CO_bitmap values ('2_zero', 1, '2_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');

-- Reindex 
   reindex index fsts_CO_idx2;

-- select from the Table
   select count(*) from fsts_CO_bitmap;

-- Vacuum analyze the table
   vacuum analyze fsts_CO_bitmap; 

