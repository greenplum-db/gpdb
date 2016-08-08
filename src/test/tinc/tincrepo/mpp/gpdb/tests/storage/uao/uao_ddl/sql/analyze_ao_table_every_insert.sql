-- @description : Create Updatable AO tables  and copy data from external source
-- 

-- Create AO tables
DROP TABLE IF EXISTS sto_uao_city_analyze_everyins cascade;
BEGIN;

CREATE TABLE sto_uao_city_analyze_everyins (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) with (appendonly=true) distributed by(id);

-- gucs set to auto update statistics after each insert
set gp_autostats_on_change_threshold=1;
set gp_autostats_mode=on_change;

select count(*)  from sto_uao_city_analyze_everyins;
select relname, reltuples from pg_class where relname = 'sto_uao_city_analyze_everyins';
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_uao_city_analyze_everyins');

-- Copy 7 rows in table sto_uao_city_analyze_everyins
COPY sto_uao_city_analyze_everyins (id, name, countrycode, district, population) FROM stdin;
1	Kabul	AFG	Kabol	1780000
2	Qandahar	AFG	Qandahar	237500
3	Herat	AFG	Herat	186800
4	Mazar-e-Sharif	AFG	Balkh	127800
5	Amsterdam	NLD	Noord-Holland	731200
6	Rotterdam	NLD	Zuid-Holland	593321
7	Haag	NLD	Zuid-Holland	440900
\.

COMMIT;

select count(*)  from sto_uao_city_analyze_everyins;
select relname, reltuples  from pg_class where relname = 'sto_uao_city_analyze_everyins';

-- Copy 3 more rows in table sto_uao_city_analyze_everyins
COPY sto_uao_city_analyze_everyins (id, name, countrycode, district, population) FROM stdin;
8	Utrecht	NLD	Utrecht	234323
9	Eindhoven	NLD	Noord-Brabant	201843
10	Rafah	PSE	Rafah	92020
\.
select count(*)  from sto_uao_city_analyze_everyins;
select relname, reltuples from pg_class where relname = 'sto_uao_city_analyze_everyins';

-- Copy 2 more rows in table sto_uao_city_analyze_everyins
COPY sto_uao_city_analyze_everyins (id, name, countrycode, district, population) FROM stdin;
11	Groningen	NLD	Groningen	172701
12	Breda	NLD	Noord-Brabant	160398
\.

select count(*)  from sto_uao_city_analyze_everyins;
select relname, reltuples from pg_class where relname = 'sto_uao_city_analyze_everyins';
