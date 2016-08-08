-- @description : Update UAO tableswith fillfector 
-- 


-- create select uao table with fillfector=70 
Drop table if exists uao_tab_fillfector70_del;
CREATE TABLE uao_tab_fillfector70_del (      
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true, orientation=column , FILLFACTOR=70) ; 
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_fillfector70_del');

CREATE index col_text_bmp_idx_fillfector70_del on uao_tab_fillfector70_del using bitmap (col_text) with (FILLFACTOR=70);

select  1 AS fillfector_ao_set from pg_class where relname = 'uao_tab_fillfector70_del' and reloptions ='{appendonly=true, orientation=column,fillfactor=70}';

\d+ uao_tab_fillfector70_del

insert into uao_tab_fillfector70_del values(1,'val1',100);
insert into uao_tab_fillfector70_del values(2,'val2',200);
insert into uao_tab_fillfector70_del values(3,'val3',300);
select *  from uao_tab_fillfector70_del order by col_int;
delete from uao_tab_fillfector70_del  where col_int = 1;
select *  from uao_tab_fillfector70_del order by col_int;
select count(*) AS only_visi_tups  from uao_tab_fillfector70_del ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_fillfector70_del ;
select *  from uao_tab_fillfector70_del order by col_int;

set gp_select_invisible = false;


