
DROP TABLE IF EXISTS CITY_AO;

CREATE TABLE CITY_AO (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) with (appendonly=true) distributed by(id);


DROP TABLE IF EXISTS country_ao;

CREATE TABLE country_ao (
    code character(3) NOT NULL,
    name text NOT NULL,
    continent text NOT NULL,
    region text NOT NULL,
    surfacearea real NOT NULL,
    indepyear smallint,
    population integer NOT NULL,
    lifeexpectancy real,
    gnp numeric(10,2),
    gnpold numeric(10,2),
    localname text NOT NULL,
    governmentform text NOT NULL,
    headofstate text,
    capital integer,
    code2 character(2) NOT NULL
) with (appendonly=true) distributed by (code);

DROP TABLE IF EXISTS countrylanguage_ao;

CREATE TABLE countrylanguage_ao (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
) with (appendonly=true) distributed by (countrycode,language);

ALTER TABLE ONLY city_ao
    ADD CONSTRAINT city_ao_pkey PRIMARY KEY (id);

ALTER TABLE ONLY country_ao
    ADD CONSTRAINT country_ao_pkey PRIMARY KEY (code);

ALTER TABLE ONLY countrylanguage_ao
    ADD CONSTRAINT countrylanguage_ao_pkey PRIMARY KEY (countrycode, "language");

CREATE INDEX bitmap_city_ao_countrycode on city_ao using bitmap(countrycode);
CREATE INDEX bitmap_country_ao_gf on country_ao using bitmap(governmentform);
CREATE INDEX bitmap_country_ao_region on country_ao using bitmap(region);
CREATE INDEX bitmap_country_ao_continent on country_ao using bitmap(continent);
CREATE INDEX bitmap_countrylanguage_ao_countrycode on countrylanguage_ao using bitmap(countrycode);

INSERT INTO CITY_AO SELECT * FROM CITY;
INSERT INTO COUNTRY_AO SELECT * FROM COUNTRY;
INSERT INTO COUNTRYLANGUAGE_AO SELECT * FROM COUNTRYLANGUAGE;

ANALYZE CITY_AO;
ANALYZE COUNTRY_AO;
ANALYZE COUNTRYLANGUAGE_AO;
