-- @description : Update UAO tables and Analyze after each Insert and Update 
-- 

-- Create AO tables
DROP TABLE IF EXISTS sto_uao_city_analyze_everyupd cascade;
BEGIN;

CREATE TABLE sto_uao_city_analyze_everyupd (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) with (appendonly=true, orientation=column) distributed by(id);

-- gucs set to auto update statistics after each insert
set gp_autostats_on_change_threshold=1;
set gp_autostats_mode=on_change;

select count(*)  from sto_uao_city_analyze_everyupd;
select relname, reltuples from pg_class where relname = 'sto_uao_city_analyze_everyupd';
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT relfilenode FROM pg_class WHERE relname='sto_uao_city_analyze_everyupd');

-- Copy 7 rows in table sto_uao_city_analyze_everyupd
COPY sto_uao_city_analyze_everyupd (id, name, countrycode, district, population) FROM stdin;
1	Kabul	AFG	Kabol	1780000
2	Qandahar	AFG	Qandahar	237500
3	Herat	AFG	Herat	186800
4	Mazar-e-Sharif	AFG	Balkh	127800
5	Amsterdam	NLD	Noord-Holland	731200
6	Rotterdam	NLD	Zuid-Holland	593321
7	Haag	NLD	Zuid-Holland	440900
\.

COMMIT;

select count(*)  from sto_uao_city_analyze_everyupd;
select relname, reltuples  from pg_class where relname = 'sto_uao_city_analyze_everyupd';

select *  from sto_uao_city_analyze_everyupd;
-- Increase the population by 1000 for all cities in NetherLands (NLD) 
-- Should update 3 rows
update sto_uao_city_analyze_everyupd set population=population+1000 where countrycode='NLD';
select count(*)  AS only_visi_tups from sto_uao_city_analyze_everyupd;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_uao_city_analyze_everyupd;

set gp_select_invisible = false;
select relname, reltuples from pg_class where relname = 'sto_uao_city_analyze_everyupd';
