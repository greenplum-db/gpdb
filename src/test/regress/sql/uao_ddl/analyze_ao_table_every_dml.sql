-- Analyze after each insert/update/delete.
BEGIN;
CREATE TABLE sto_uao_city_analyze_everydml (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) with (appendonly=true) distributed by(id);

-- gucs set to auto update statistics after each dml
set gp_autostats_on_change_threshold=1;
set gp_autostats_mode=on_change;

select count(*)  from sto_uao_city_analyze_everydml;
select relname, reltuples from pg_class where relname = 'sto_uao_city_analyze_everydml';
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_uao_city_analyze_everydml');

-- Copy 7 rows in table sto_uao_city_analyze_everydml
COPY sto_uao_city_analyze_everydml (id, name, countrycode, district, population) FROM stdin;
1	Kabul	AFG	Kabol	1780000
2	Qandahar	AFG	Qandahar	237500
3	Herat	AFG	Herat	186800
4	Mazar-e-Sharif	AFG	Balkh	127800
5	Amsterdam	NLD	Noord-Holland	731200
6	Rotterdam	NLD	Zuid-Holland	593321
7	Haag	NLD	Zuid-Holland	440900
\.

COMMIT;

select count(*)  from sto_uao_city_analyze_everydml;
select relname, reltuples  from pg_class where relname = 'sto_uao_city_analyze_everydml';

select *  from sto_uao_city_analyze_everydml order by id;
-- Should delete 3 rows
delete from  sto_uao_city_analyze_everydml where countrycode='NLD';
select count(*)  AS only_visi_tups from sto_uao_city_analyze_everydml;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_uao_city_analyze_everydml;
set gp_select_invisible = false;
select relname, reltuples from pg_class where relname = 'sto_uao_city_analyze_everydml';
update sto_uao_city_analyze_everydml set population=population+1000 where countrycode='AFG';
select count(*)  AS only_visi_tups from sto_uao_city_analyze_everydml;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_uao_city_analyze_everydml;
set gp_select_invisible = false;
select relname, reltuples from pg_class where relname = 'sto_uao_city_analyze_everydml';

-- Copy 2 more rows in table sto_uao_city_analyze_everyins
COPY sto_uao_city_analyze_everydml (id, name, countrycode, district, population) FROM stdin;
11	Groningen	NLD	Groningen	172701
12	Breda	NLD	Noord-Brabant	160398
\.

select count(*)  from sto_uao_city_analyze_everydml;
select relname, reltuples from pg_class where relname = 'sto_uao_city_analyze_everydml';

