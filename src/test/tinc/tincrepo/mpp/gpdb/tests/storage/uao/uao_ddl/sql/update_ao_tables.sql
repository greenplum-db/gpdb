-- @description : Create Updatable AO tables , with indexes, with and without compression
-- 

-- Create AO tables 
DROP TABLE if exists sto_uao_upd_1_upd;
CREATE TABLE sto_uao_upd_1_upd (
          col_int int,
          col_text text,
          col_numeric numeric
          ) with(appendonly=true) DISTRIBUTED RANDOMLY;


SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_uao_upd_1_upd');

Create index sto_uao_upd_1_upd_int_idx1 on sto_uao_upd_1_upd(col_int);

insert into sto_uao_upd_1_upd values(1,'val1',100);
insert into sto_uao_upd_1_upd values(2,'val2',200);
insert into sto_uao_upd_1_upd values(3,'val3',300);
select *  from sto_uao_upd_1_upd order by col_int,col_text;
update sto_uao_upd_1_upd set col_text=col_text||' new value' where col_int = 1;
select *  from sto_uao_upd_1_upd order by col_int,col_text;
select count(*) AS only_visi_tups  from sto_uao_upd_1_upd ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sto_uao_upd_1_upd ;

set gp_select_invisible = false;

-- Create table with all data_types 

-- Create table with constriants
Drop table if exists sto_uao_upd_2_upd;
CREATE TABLE sto_uao_upd_2_upd(
          col_text text DEFAULT 'text',
          col_numeric numeric
          CONSTRAINT tbl_chk_con1 CHECK (col_numeric < 250)
          ) with(appendonly=true) DISTRIBUTED by(col_text);

SELECT 1  AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_uao_upd_2_upd');
insert into sto_uao_upd_2_upd values ('0_zero',20);
insert into sto_uao_upd_2_upd values ('1_one',10);
insert into sto_uao_upd_2_upd values ('2_two',25);
select count(*) from sto_uao_upd_2_upd;
update sto_uao_upd_2_upd set col_numeric = 30 where col_text = '1_one';
select count(*) from sto_uao_upd_2_upd;
set gp_select_invisible = true;
select count(*) from sto_uao_upd_2_upd;


--Update table in user created scehma
set gp_select_invisible = false;
Drop schema if exists uao_upd_schema1 cascade;
Create schema uao_upd_schema1;

CREATE TABLE uao_upd_schema1.sto_uao_upd_3_upd(
          stud_id int,
          stud_name varchar(20)
          ) with(appendonly=true) DISTRIBUTED by(stud_id);

SELECT 1  AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_uao_upd_3_upd');
Insert into uao_upd_schema1.sto_uao_upd_3_upd values(1,'name1'), (2,'name2'),(3,'name3');
select * from uao_upd_schema1.sto_uao_upd_3_upd;
update uao_upd_schema1.sto_uao_upd_3_upd set stud_name = stud_name||' new value' where stud_id=1;
select * from uao_upd_schema1.sto_uao_upd_3_upd;
set gp_select_invisible = true;
select * from uao_upd_schema1.sto_uao_upd_3_upd;


-- Truncate


Drop table if exists sto_uao_upd_7_upd;
CREATE TABLE sto_uao_upd_7_upd  (
    did integer,
    name varchar(40),
    CONSTRAINT con1 CHECK (did > 99 AND name <> '')
    ) with(appendonly=true) DISTRIBUTED RANDOMLY;


insert into sto_uao_upd_7_upd  values (100,'name_1');
insert into sto_uao_upd_7_upd  values (200,'name_2');

set gp_select_invisible = true;
select count(*) from sto_uao_upd_7_upd ;
update sto_uao_upd_7_upd set name= name || ' new value';
select count(*) from sto_uao_upd_7_upd;
Truncate sto_uao_upd_7_upd;
select count(*) from sto_uao_upd_7_upd;
set gp_select_invisible = false;
