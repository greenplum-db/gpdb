-- Create AO tables
CREATE TABLE sto_alt_uao1_default(
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric,
          int_col int4 NOT NULL,
          float_col float4,
          int_array_col int[],
          before_rename_col int4,
          change_datatype_col numeric,
          date_column date,
          col_set_default numeric,
          text_col text default 'i am default'
) with(appendonly=true) DISTRIBUTED RANDOMLY;

insert into sto_alt_uao1_default values (0, '0_zero', 0, 0, 0, '{0}', 0, 0, '1-1-2000',0,'0_zero');
insert into sto_alt_uao1_default values (1, '1_zero', 1, 1, 1, '{1}', 1, 1, '1-1-2001',1,'1_zero');
insert into sto_alt_uao1_default values (2, '2_zero', 2, 2, 2, '{2}', 2, 2, '1-1-2002',2);

-- Alter column Drop default
Alter table sto_alt_uao1_default  alter column text_col drop default;
SELECT 1 AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is not NULL AND
 visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao1_default');
insert into sto_alt_uao1_default values (3, '3_zero', 3, 3, 3, '{3}', 3, 3, '1-1-2002', 3);
insert into sto_alt_uao1_default values (4, '4_zero', 4, 4, 4, '{4}', 4, 4, '1-1-2002', 4,'4_zero');
select count(*) AS only_visi_tups from sto_alt_uao1_default;
update sto_alt_uao1_default set text_col = 'my new val' where col_set_default = 3;
select * from sto_alt_uao1_default order by bigint_col;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao1_default;
set gp_select_invisible = false;
-- Alter column set NOT NULL
Alter Table sto_alt_uao1_default ALTER COLUMN text_col SET NOT NULL;
insert into sto_alt_uao1_default values (5, '5_zero', 5, 5, 5, '{5}', 5, 5, '1-1-2002', 5);
select * from sto_alt_uao1_default order by bigint_col;

-- Alter column drop NOT NULL
Alter Table sto_alt_uao1_default ALTER COLUMN int_col DROP NOT NULL,
 ALTER COLUMN text_col DROP NOT NULL;
SELECT 1 AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is not NULL AND
 visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao1_default');
insert into sto_alt_uao1_default values (6, '6_zero', 6, 6, 6, '{6}', 6, 6, '1-1-2002', 6);
update sto_alt_uao1_default set date_column = '2013-08-15' where text_col = '1_zero';
select * from sto_alt_uao1_default order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao1_default;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao1_default;
set gp_select_invisible = false;
