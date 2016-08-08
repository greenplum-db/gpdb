-- @description : Create Updatable AO tables , with indexes different distributions
-- 

-- create select uao table with  No distribution (hence hash distribution)
DROP TABLE IF EXISTS  uao_tab_distrib_none cascade;
CREATE TABLE uao_tab_distrib_none  with (appendonly=true, orientation=column )  AS (
SELECT GENERATE_SERIES::numeric sno
     , (random() * 10000000)::numeric + 10000000 val1
     , timeofday()::varchar(50) val2
     , (random() * 5000)::bigint + 10000000 val3
     , (random() * 10000000)::numeric + 10000000 val4
     , (random() * 10000000)::numeric + 10000000 val5	
  FROM GENERATE_SERIES(10000, 19999)
) ;
CREATE index val3_bmp_idx_uao_tab_distrib_none on uao_tab_distrib_none using bitmap (val3);
\d+ uao_tab_distrib_none;
SELECT count(*) AS cnt_row_distrib_none from uao_tab_distrib_none;
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_distrib_none');

-- create select uao table with  distributed by sno
DROP TABLE IF EXISTS uao_tab_distrib_key cascade;
CREATE TABLE uao_tab_distrib_key  with (appendonly=true, orientation=column )  AS (
SELECT GENERATE_SERIES::numeric sno
     , (random() * 10000000)::numeric + 10000000 val1
     , timeofday()::varchar(50) val2
     , (random() * 5000)::bigint + 10000000 val3
     , (random() * 10000000)::numeric + 10000000 val4
     , (random() * 10000000)::numeric + 10000000 val5	
  FROM GENERATE_SERIES(10000, 19999)
) DISTRIBUTED BY (sno) ;
CREATE index val3_bmp_idx_uao_tab_distrib_key on uao_tab_distrib_key using bitmap (val3);
\d+ uao_tab_distrib_key;
SELECT count(*) AS cnt_row_distrib_key from uao_tab_distrib_key;


SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_distrib_key');

-- create select uao table with  distributed randomly
DROP TABLE IF EXISTS  uao_tab_distrib_randomly cascade;
CREATE TABLE uao_tab_distrib_randomly  with (appendonly=true, orientation=column )  AS (
SELECT GENERATE_SERIES::numeric sno
     , (random() * 10000000)::numeric + 10000000 val1
     , timeofday()::varchar(50) val2
     , (random() * 5000)::bigint + 10000000 val3
     , (random() * 10000000)::numeric + 10000000 val4
     , (random() * 10000000)::numeric + 10000000 val5	
  FROM GENERATE_SERIES(10000, 19999)
) DISTRIBUTED randomly ;
CREATE index val3_bmp_idx_uao_tab_distrib_randomly on uao_tab_distrib_randomly using bitmap (val3);
\d+ uao_tab_distrib_randomly;

SELECT count(*) AS cnt_row_distrib_random from uao_tab_distrib_randomly;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_distrib_randomly');


