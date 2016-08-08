-- @description : Create Updatable AO tables  and copy data from external source
-- 

-- Create AO tables
DROP TABLE IF EXISTS sto_uao_city cascade;
BEGIN;

CREATE TABLE sto_uao_city (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) with (appendonly=true, orientation=column) distributed by(id);

select count(*)  from sto_uao_city;
select relname, reltuples from pg_class where relname = 'sto_uao_city';
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_uao_city');

-- Copy 7 rows in table sto_uao_city
COPY sto_uao_city (id, name, countrycode, district, population) FROM stdin;
1	Kabul	AFG	Kabol	1780000
2	Qandahar	AFG	Qandahar	237500
3	Herat	AFG	Herat	186800
4	Mazar-e-Sharif	AFG	Balkh	127800
5	Amsterdam	NLD	Noord-Holland	731200
6	Rotterdam	NLD	Zuid-Holland	593321
7	Haag	NLD	Zuid-Holland	440900
\.

COMMIT;

select count(*)  from sto_uao_city;
select relname, reltuples from pg_class where relname = 'sto_uao_city';

-- Copy 3 more rows in table sto_uao_city
COPY sto_uao_city (id, name, countrycode, district, population) FROM stdin;
8	Utrecht	NLD	Utrecht	234323
9	Eindhoven	NLD	Noord-Brabant	201843
10	Rafah	PSE	Rafah	92020
\.
select count(*)  from sto_uao_city;

-- Copy 2 more rows in table sto_uao_city
COPY sto_uao_city (id, name, countrycode, district, population) FROM stdin;
11	Groningen	NLD	Groningen	172701
12	Breda	NLD	Noord-Brabant	160398
\.

select count(*)  from sto_uao_city;
