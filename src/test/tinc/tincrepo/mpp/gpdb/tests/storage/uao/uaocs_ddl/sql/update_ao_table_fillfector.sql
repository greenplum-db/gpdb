-- @description : Update UAO tableswith fillfector 
-- 


-- create select uao table with fillfector=70 
Drop table if exists uao_tab_fillfector70_upd;
CREATE TABLE uao_tab_fillfector70_upd (      
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true, orientation=column , FILLFACTOR=70) ; 
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_fillfector70_upd');

CREATE index col_text_bmp_idx_fillfector70_upd on uao_tab_fillfector70_upd using bitmap (col_text) with (FILLFACTOR=70);

select  1 AS fillfector_ao_set from pg_class where relname = 'uao_tab_fillfector70_upd' and reloptions ='{appendonly=true, orientation=column,fillfactor=70}';

\d+ uao_tab_fillfector70_upd

insert into uao_tab_fillfector70_upd values(1,'val1',100);
insert into uao_tab_fillfector70_upd values(2,'val2',200);
insert into uao_tab_fillfector70_upd values(3,'val3',300);
select *  from uao_tab_fillfector70_upd order by col_int,col_text;
update uao_tab_fillfector70_upd set col_text=col_text||' new value' where col_int = 1;
select *  from uao_tab_fillfector70_upd order by col_int,col_text;
select count(*) AS only_visi_tups  from uao_tab_fillfector70_upd ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_fillfector70_upd ;
select *  from uao_tab_fillfector70_upd order by col_int,col_text;

set gp_select_invisible = false;


