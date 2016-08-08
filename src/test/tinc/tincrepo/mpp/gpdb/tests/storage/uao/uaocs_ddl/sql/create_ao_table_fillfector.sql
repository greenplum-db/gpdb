-- @description : Create Updatable AO tables , with fillfactor
-- 


-- create select uao table with fillfector=70
Drop table if exists uao_tab_fillfector70;
CREATE TABLE uao_tab_fillfector70 (      
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true, orientation=column , FILLFACTOR=70) ; 
insert into uao_tab_fillfector70 select i , 'This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling '||i, (random() * 10000000)::numeric + 10000000 from GENERATE_SERIES(40000, 49999) AS i;

CREATE index col_text_bmp_idx_fillfector70 on uao_tab_fillfector70 using bitmap (col_text) with (FILLFACTOR=70);

\d+ uao_tab_fillfector70
select count(*) from uao_tab_fillfector70;
select  1 AS fillfector_set from pg_class where relname = 'uao_tab_fillfector70' and reloptions ='{appendonly=true, orientation=column,fillfactor=70}';
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_fillfector70');


