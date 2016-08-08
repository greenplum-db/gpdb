-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
Drop table if exists st_uao_oids_del ;
CREATE TABLE st_uao_oids_del(
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
date_column date) with ( appendonly='true', OIDS)  distributed randomly;

INSERT INTO st_uao_oids_del select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1,100)i;


select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='st_uao_oids_del');
select count(*) AS only_visi_tups_ins  from st_uao_oids_del;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from st_uao_oids_del;
set gp_select_invisible = false;

delete from st_uao_oids_del  where bigint_col >=  50;
select count(*) AS only_visi_tups  from st_uao_oids_del;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from st_uao_oids_del;
set gp_select_invisible = false;


VACUUM st_uao_oids_del;

select count(*) AS only_visi_tups_vacuum  from st_uao_oids_del;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_vacuum  from st_uao_oids_del;
set gp_select_invisible = false;
