-- @description : Alter  UAO tables and execute DML statements on the tables
-- 
-- @gpopt 1.556
Drop table if exists sto_alt_uao2_constraint cascade;
CREATE TABLE sto_alt_uao2_constraint(
  text_col text,
  bigint_col bigint,
  char_vary_col character varying(30),
  numeric_col numeric
  ) with(appendonly=true, orientation=column) DISTRIBUTED RANDOMLY;
\d+ sto_alt_uao2_constraint
insert into sto_alt_uao2_constraint values ('1_zero', 1, '1_zero', 1);
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao2_constraint');

select * from sto_alt_uao2_constraint order by bigint_col;
update sto_alt_uao2_constraint set numeric_col = 100 where text_col = '1_zero';
select count(*) AS only_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = false;
-- Alter table add table constraint
ALTER TABLE sto_alt_uao2_constraint  ADD CONSTRAINT lenchk CHECK (char_length(char_vary_col) < 10);
\d+ sto_alt_uao2_constraint
insert into sto_alt_uao2_constraint values ('6_zero', 6, '6_zeroooooo', 6);
select * from sto_alt_uao2_constraint order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = false;
insert into sto_alt_uao2_constraint values ('6_zero', 6, '6_zero', 6);
select * from sto_alt_uao2_constraint order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = false;
update sto_alt_uao2_constraint set numeric_col = 100 where text_col = '1_zero';
select * from sto_alt_uao2_constraint order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = false;
-- Alter table drop constriant
Alter table sto_alt_uao2_constraint  Drop constraint lenchk;
\d+ sto_alt_uao2_constraint
insert into sto_alt_uao2_constraint values ('7_zero', 7, '7_zeroooooooooo', 7);
select * from sto_alt_uao2_constraint order by bigint_col;
select count(*) AS only_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = false;
select * from sto_alt_uao2_constraint order by bigint_col;
update sto_alt_uao2_constraint set numeric_col = 100 where text_col = '1_zero';
select count(*) AS only_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao2_constraint;
set gp_select_invisible = false;
