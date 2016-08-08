-- @description : update UAO tables , with different distributions
-- 

-- create select uao table with  No distribution (hence hash distribution)
DROP TABLE IF EXISTS  uao_tab_distrib_none_upd cascade;
CREATE TABLE uao_tab_distrib_none_upd  with (appendonly=true )  AS (
SELECT GENERATE_SERIES::numeric sno
     , (random() * 10000000)::numeric + 10000000 val1
     , timeofday()::varchar(50) val2
     , (random() * 5000)::bigint + 10000000 val3
     , (random() * 10000000)::numeric + 10000000 val4
     , (random() * 10000000)::numeric + 10000000 val5	
  FROM GENERATE_SERIES(10000, 19999)
) ;
CREATE index val3_bmp_idx_uao_tab_distrib_none_upd on uao_tab_distrib_none_upd using bitmap (val3);
SELECT count(*) AS cnt_row_distrib_none from uao_tab_distrib_none_upd;
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_distrib_none_upd');

update  uao_tab_distrib_none_upd set val2 = 'new_val' where sno = 10000;
select count(*) AS only_visi_tups  from uao_tab_distrib_none_upd;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_distrib_none_upd;
set gp_select_invisible = false;

-- create select uao table with  distributed by sno
DROP TABLE IF EXISTS uao_tab_distrib_key_upd cascade;
CREATE TABLE uao_tab_distrib_key_upd  with (appendonly=true )  AS (
SELECT GENERATE_SERIES::numeric sno
     , (random() * 10000000)::numeric + 10000000 val1
     , timeofday()::varchar(50) val2
     , (random() * 5000)::bigint + 10000000 val3
     , (random() * 10000000)::numeric + 10000000 val4
     , (random() * 10000000)::numeric + 10000000 val5	
  FROM GENERATE_SERIES(10000, 19999)
) DISTRIBUTED BY (sno) ;
CREATE index val3_bmp_idx_uao_tab_distrib_key_upd on uao_tab_distrib_key_upd using bitmap (val3);
SELECT count(*) AS cnt_row_distrib_key from uao_tab_distrib_key_upd;


SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_distrib_key_upd');

update  uao_tab_distrib_key_upd set val2 = 'new_val' where sno = 10000;
select count(*) AS only_visi_tups  from uao_tab_distrib_key_upd;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_distrib_key_upd;
set gp_select_invisible = false;

-- create select uao table with  distributed randomly
DROP TABLE IF EXISTS  uao_tab_distrib_randomly_upd cascade;
CREATE TABLE uao_tab_distrib_randomly_upd  with (appendonly=true )  AS (
SELECT GENERATE_SERIES::numeric sno
     , (random() * 10000000)::numeric + 10000000 val1
     , timeofday()::varchar(50) val2
     , (random() * 5000)::bigint + 10000000 val3
     , (random() * 10000000)::numeric + 10000000 val4
     , (random() * 10000000)::numeric + 10000000 val5	
  FROM GENERATE_SERIES(10000, 19999)
) DISTRIBUTED randomly ;
CREATE index val3_bmp_idx_uao_tab_distrib_randomly_upd on uao_tab_distrib_randomly_upd using bitmap (val3);
SELECT count(*) AS cnt_row_distrib_random from uao_tab_distrib_randomly_upd;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_distrib_randomly_upd');

update  uao_tab_distrib_randomly_upd set val2 = 'new_val' where sno = 10000;
select count(*) AS only_visi_tups  from uao_tab_distrib_randomly_upd;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_distrib_randomly_upd;
set gp_select_invisible = false;

