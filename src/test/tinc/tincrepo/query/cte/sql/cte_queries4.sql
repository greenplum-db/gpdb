-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @skip MPP-19324
-- @description cte tests from cdbfast 

-- Queries with multiple CTEs referencing one another
-- query1 
with city_official_language as
(select city.id,city.name CITY,country.name COUNTRY,countrylanguage.language,city.population,country.capital 
 from city,countrylanguage,country
 where city.countrycode = country.code
 and country.code = countrylanguage.countrycode
 and countrylanguage.isofficial = 'True'
),
capital_official_language as 
(select c2.CITY, c1.COUNTRY,c1.language OFFICIAL_LANGUAGE,c1.CITY CAPITAL
 from city_official_language c1 , city_official_language c2 
 where c1.id = c2.capital
 and c1.id != c2.id
),
alleuropeanlanguages as 
(select c.COUNTRY,c.CITY,c.CAPITAL,c.OFFICIAL_LANGUAGE
 from capital_official_language c, country
 where c.COUNTRY = country.name 
 and country.continent = 'Europe'
)

select code,COUNTRY,CITY,alleuropeanlanguages.CAPITAL,OFFICIAL_LANGUAGE from alleuropeanlanguages,country
where alleuropeanlanguages.COUNTRY = country.name
and alleuropeanlanguages.city in (select CITY from city_official_language)
and alleuropeanlanguages.OFFICIAL_LANGUAGE IN (select OFFICIAL_LANGUAGE from capital_official_language)
order by code,country,city,official_language
limit 100;

--query2
with allcitystats as 
( select city.name CITY,city.id,country.name COUNTRY,city.district,city.population as CITY_POP
  from
  city,country
  where city.countrycode = country.code
),
alldistrictstats as 
( select allcitystats.district,allcitystats.COUNTRY,sum(CITY_POP) DISTRICT_POP,
  count(CITY) as D_CITY_CNT
  from allcitystats
  group by allcitystats.district,allcitystats.COUNTRY
  order by district,COUNTRY
),
allcountrystats as 
( select alldistrictstats.COUNTRY,country.code,sum(D_CITY_CNT) C_CITY_CNT,
  count(distinct countrylanguage.language) C_LANG_CNT
  from alldistrictstats,country,countrylanguage
  where alldistrictstats.COUNTRY = country.name
  and country.code = countrylanguage.countrycode
  group by COUNTRY,code
),
asian_region_stats as 
(
select sum(FOO.C_CITY_CNT) REGION_CITY_CNT,sum(FOO.C_LANG_CNT) REGION_LANG_CNT,FOO.region
FROM
(
select allcountrystats.code,allcountrystats.COUNTRY,C_CITY_CNT,C_LANG_CNT,country.region,city.name CAPITAL
from allcountrystats,country,city
where allcountrystats.code = country.code
and country.capital = city.id
and C_CITY_CNT/C_LANG_CNT > 1
and country.continent = 'Asia') FOO
,allcountrystats,country

WHERE allcountrystats.code = country.code
and FOO.region = country.region
group by FOO.region order by FOO.region
)

select * from
(
select REGION_CITY_CNT as CITY_CNT,REGION_LANG_CNT as LANG_CNT, region as IDENTIFIER from asian_region_stats
UNION ALL
(
select sum(FOO.C_CITY_CNT) CITY_CNT,sum(FOO.C_LANG_CNT) LANG_CNT,FOO.region as IDENTIFIER
FROM
(
select allcountrystats.code,allcountrystats.COUNTRY,C_CITY_CNT,C_LANG_CNT,country.region,allcitystats.CITY CAPITAL
from allcountrystats,country,allcitystats
where allcountrystats.code = country.code
and country.capital = allcitystats.id
and C_CITY_CNT/C_LANG_CNT > 1
and country.continent = 'Europe') FOO
,allcountrystats,country

WHERE allcountrystats.code = country.code
and FOO.region = country.region
group by FOO.region order by FOO.region
)
) FOO1
order by FOO1.lang_cnt,FOO1.identifier;
