-- @description : Alter  UAO tables and execute DML statements on the tables
-- 


-- Create AO tables
DROP TABLE if exists sto_alt_uao1_setstorage;
CREATE TABLE sto_alt_uao1_setstorage(
          text_col text default 'remove it',
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric,
          int_col int4 NOT NULL,
          float_col float4,
          int_array_col int[],
          before_rename_col int4,
          change_datatype_col numeric,
          a_ts_without timestamp without time zone,
          b_ts_with timestamp with time zone,
          date_column date,
          col_set_default numeric) with(appendonly=true, orientation=column) DISTRIBUTED RANDOMLY;

insert into sto_alt_uao1_setstorage values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
insert into sto_alt_uao1_setstorage values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
insert into sto_alt_uao1_setstorage values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);

select * from sto_alt_uao1_setstorage order by bigint_col;
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT relfilenode FROM pg_class WHERE relname='sto_alt_uao1_setstorage');

select count(*) AS only_visi_tups from sto_alt_uao1_setstorage;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao1_setstorage;
set gp_select_invisible = false;
-- Alter table SET STORAGE
Alter Table sto_alt_uao1_setstorage ALTER char_vary_col SET STORAGE PLAIN;
insert into sto_alt_uao1_setstorage values ('6_zero', 6, '6_zero', 6, 6, '{6}', 6, 6, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',6, 'newcol');
select * from sto_alt_uao1_setstorage order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao1_setstorage;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao1_setstorage;
set gp_select_invisible = false;
update sto_alt_uao1_setstorage set date_column = '2013-08-16' where text_col = '1_zero';
select * from sto_alt_uao1_setstorage order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao1_setstorage;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao1_setstorage;
set gp_select_invisible = false;
/*
delete from  sto_alt_uao1_setstorage where text_col = '1_zero';
select * from sto_alt_uao1_setstorage order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao1_setstorage;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao1_setstorage;
set gp_select_invisible = false;
*/
