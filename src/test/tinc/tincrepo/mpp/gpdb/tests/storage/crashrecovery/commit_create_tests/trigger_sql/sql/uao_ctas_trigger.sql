-- @product_version gpdb: [4.3.0.0-]
CREATE TABLE cr_uao_ctas WITH (appendonly=true) AS SELECT * FROM cr_seed_table_for_uao;
select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='cr_uao_ctas');

select count(*) AS only_visi_tups_ins  from cr_uao_ctas;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from cr_uao_ctas;
set gp_select_invisible = false;
update cr_uao_ctas set  col001 = 'e'   where a=1;
select count(*) AS only_visi_tups_upd  from cr_uao_ctas;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from cr_uao_ctas;
set gp_select_invisible = false;
delete from cr_uao_ctas  where a =  3;
select count(*) AS only_visi_tups  from cr_uao_ctas;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from cr_uao_ctas;
set gp_select_invisible = false;
