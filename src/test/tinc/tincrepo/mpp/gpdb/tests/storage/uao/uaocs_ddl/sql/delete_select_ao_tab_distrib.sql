-- @description : delete from UAO tables , with different distributions
-- 

-- create select uao table with  No distribution (hence hash distribution)
DROP TABLE IF EXISTS  uao_tab_distrib_none_del cascade;
CREATE TABLE uao_tab_distrib_none_del  with (appendonly=true, orientation=column )  AS (
SELECT GENERATE_SERIES::numeric sno
     , (random() * 10000000)::numeric + 10000000 val1
     , timeofday()::varchar(50) val2
     , (random() * 5000)::bigint + 10000000 val3
     , (random() * 10000000)::numeric + 10000000 val4
     , (random() * 10000000)::numeric + 10000000 val5	
  FROM GENERATE_SERIES(10000, 19999)
) ;
CREATE index val3_bmp_idx_uao_tab_distrib_none_del on uao_tab_distrib_none_del using bitmap (val3);
SELECT count(*) AS cnt_row_distrib_none from uao_tab_distrib_none_del;
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_distrib_none_del');

delete from  uao_tab_distrib_none_del where sno = 10000;
select count(*) AS only_visi_tups  from uao_tab_distrib_none_del;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_distrib_none_del;
set gp_select_invisible = false;

-- create select uao table with  distributed by sno
DROP TABLE IF EXISTS uao_tab_distrib_key_del cascade;
CREATE TABLE uao_tab_distrib_key_del  with (appendonly=true, orientation=column )  AS (
SELECT GENERATE_SERIES::numeric sno
     , (random() * 10000000)::numeric + 10000000 val1
     , timeofday()::varchar(50) val2
     , (random() * 5000)::bigint + 10000000 val3
     , (random() * 10000000)::numeric + 10000000 val4
     , (random() * 10000000)::numeric + 10000000 val5	
  FROM GENERATE_SERIES(10000, 19999)
) DISTRIBUTED BY (sno) ;
CREATE index val3_bmp_idx_uao_tab_distrib_key_del on uao_tab_distrib_key_del using bitmap (val3);
SELECT count(*) AS cnt_row_distrib_key from uao_tab_distrib_key_del;


SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_distrib_key_del');

delete from  uao_tab_distrib_key_del  where sno = 10000;
select count(*) AS only_visi_tups  from uao_tab_distrib_key_del;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_distrib_key_del;
set gp_select_invisible = false;

-- create select uao table with  distributed randomly
DROP TABLE IF EXISTS  uao_tab_distrib_randomly_del cascade;
CREATE TABLE uao_tab_distrib_randomly_del  with (appendonly=true, orientation=column )  AS (
SELECT GENERATE_SERIES::numeric sno
     , (random() * 10000000)::numeric + 10000000 val1
     , timeofday()::varchar(50) val2
     , (random() * 5000)::bigint + 10000000 val3
     , (random() * 10000000)::numeric + 10000000 val4
     , (random() * 10000000)::numeric + 10000000 val5	
  FROM GENERATE_SERIES(10000, 19999)
) DISTRIBUTED randomly ;
CREATE index val3_bmp_idx_uao_tab_distrib_randomly_del on uao_tab_distrib_randomly_del using bitmap (val3);
SELECT count(*) AS cnt_row_distrib_random from uao_tab_distrib_randomly_del;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_distrib_randomly_del');

delete from  uao_tab_distrib_randomly_del where sno = 10000;
select count(*) AS only_visi_tups  from uao_tab_distrib_randomly_del;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_distrib_randomly_del;
set gp_select_invisible = false;

