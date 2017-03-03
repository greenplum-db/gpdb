-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description cte tests from cdbfast 

-- Queries using multiple CTEs

-- query1 - all CTEs being used once in the main query
with capitals as 
(select country.code,id,city.name from city,country 
 where city.countrycode = country.code AND city.id = country.capital),
lang_total as
( select count(*) as lang_count,country.code,countrylanguage.countrycode
  from country join countrylanguage on (country.code=countrylanguage.countrycode and governmentform='Federal Republic')
  group by country.code,countrylanguage.countrycode order by country.code),
alleuropeanlanguages as 
(select country.code,country.name COUNTRY, city.name CAPITAL, language, isofficial, percentage
 FROM country,city,countrylanguage
 WHERE country.code = countrylanguage.countrycode
 and country.capital = city.id
 and country.continent = 'Europe'),
diversecountries as
(select country.code,country.name,country.capital,d.CNT
 from country,
 (select countrylanguage.countrycode,count(*) as CNT from countrylanguage group by countrycode
  HAVING count(*) > 6) d
 where d.countrycode = country.code and country.gnp > 100000)

select capitals.name CAPITAL,lang_total.lang_count COUNTRY_LANG_COUNT,capitals.code,country.name
from capitals,lang_total,country
where capitals.code = lang_total.countrycode
and capitals.code = country.code
and country.code NOT IN (select diversecountries.code from diversecountries,alleuropeanlanguages
                         where diversecountries.code = alleuropeanlanguages.code)
order by capitals.code;

-- query 2 multiple CTEs being used multiple times through joins
with longlivingregions as 
(
select FOO.*,count(distinct language) as "lang_count"
from(
     select
       sum(population) as "REGION_POP",
       sum(gnp) as "REGION_GNP",
       avg(lifeexpectancy) as "REGION_LIFETIME",region
     from
      country
     group by region
    ) FOO,countrylanguage,country
where
   country.code = countrylanguage.countrycode
   and FOO.region = country.region
group by
FOO.region,foo."REGION_POP",foo."REGION_GNP",foo."REGION_LIFETIME"),

denseregions as 
(
select FOO.*,count(distinct language) as "lang_count",
       sum(surfacearea) as "REGION_SURFACE_AREA"
from(
     select
       sum(population) as "REGION_POP",
       sum(gnp) as "REGION_GNP",
       region
     from
      country
     group by region
    ) FOO,countrylanguage,country
where
   country.code = countrylanguage.countrycode
   and FOO.region = country.region
   and FOO."REGION_POP" != 0
group by
FOO.region,foo."REGION_POP",foo."REGION_GNP"
order by sum(surfacearea)/foo."REGION_POP" desc),

allcountrystats as 
( select country.code,country.name,count(distinct city.id) CITY_CNT,
  count(distinct countrylanguage.language) LANG_CNT
  from country,city,countrylanguage
  where country.code = city.countrycode
  and country.code = countrylanguage.countrycode
  group by country.code,country.name
)

select allcountrystats.CITY_CNT,allcountrystats.LANG_CNT,allcountrystats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountrystats,country
where longlivingregions.region = denseregions.region and allcountrystats.code = country.code and country.region = longlivingregions.region
and country.indepyear between 1800 and 1850

UNION ALL

select allcountrystats.CITY_CNT,allcountrystats.LANG_CNT,allcountrystats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountrystats,country
where longlivingregions.region = denseregions.region and allcountrystats.code = country.code and country.region = longlivingregions.region
and country.indepyear between 1850 and 1900

UNION ALL

select allcountrystats.CITY_CNT,allcountrystats.LANG_CNT,allcountrystats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountrystats,country
where longlivingregions.region = denseregions.region and allcountrystats.code = country.code and country.region = longlivingregions.region
and country.indepyear > 1900
order by name
LIMIT 50;

       


