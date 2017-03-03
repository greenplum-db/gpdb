\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS CITY_CO;

CREATE TABLE CITY_CO (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) with (appendonly=true,orientation=column) distributed by(id);


DROP TABLE IF EXISTS country_co;

CREATE TABLE country_co (
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
) with (appendonly=true,orientation=column) distributed by (code);

DROP TABLE IF EXISTS countrylanguage_co;

CREATE TABLE countrylanguage_co (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
) with (appendonly=true,orientation=column) distributed by (countrycode,language);

ALTER TABLE ONLY city_co
    ADD CONSTRAINT city_co_pkey PRIMARY KEY (id);

ALTER TABLE ONLY country_co
    ADD CONSTRAINT country_co_pkey PRIMARY KEY (code);

ALTER TABLE ONLY countrylanguage_co
    ADD CONSTRAINT countrylanguage_co_pkey PRIMARY KEY (countrycode, "language");

CREATE INDEX bitmap_city_co_countrycode on city_co using bitmap(countrycode);
CREATE INDEX bitmap_country_co_gf on country_co using bitmap(governmentform);
CREATE INDEX bitmap_country_co_region on country_co using bitmap(region);
CREATE INDEX bitmap_country_co_continent on country_co using bitmap(continent);
CREATE INDEX bitmap_countrylanguage_co_countrycode on countrylanguage_co using bitmap(countrycode);

INSERT INTO CITY_CO SELECT * FROM CITY;
INSERT INTO COUNTRY_CO SELECT * FROM COUNTRY;
INSERT INTO COUNTRYLANGUAGE_CO SELECT * FROM COUNTRYLANGUAGE;

ANALYZE CITY_CO;
ANALYZE COUNTRY_CO;
ANALYZE COUNTRYLANGUAGE_CO;
