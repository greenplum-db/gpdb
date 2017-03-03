-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @skip GPSQL-1020
-- @gucs enable_seqscan=off;enable_indexscan=on
-- @description cte tests from cdbfast 

-- queries with CTEs on CO tables
--query1
with capitals as 
(select country_co.code,id,city_co.name from city_co,country_co 
 where city_co.countrycode = country_co.code AND city_co.id = country_co.capital) 

select * from 
capitals,countrylanguage_co
where capitals.code = countrylanguage_co.countrycode and isofficial='true'
order by capitals.code,countrylanguage_co.language;

--query2
with lang_total as
( select count(*) as lang_count,country_co.code,countrylanguage_co.countrycode
  from country_co join countrylanguage_co on (country_co.code=countrylanguage_co.countrycode and governmentform='Federal Republic')
  group by country_co.code,countrylanguage_co.countrycode order by country_co.code)
 
select lang_count,country_co.code,country_co.name,country_co.continent,country_co.region,country_co.population
 from country_co left outer join lang_total
 on (lang_total.code = country_co.code)
 where country_co.indepyear > 1970 and lang_total.lang_count >=1 and lang_total.lang_count >=1
  order by lang_total.lang_count desc;


-- Using CTE in the WHERE clause

--query3
with lang_total as
( select lang_count,countrycode from
  (
  (
   select count(*) as lang_count,country_co.code,countrylanguage_co.countrycode
  from country_co join countrylanguage_co on (country_co.code=countrylanguage_co.countrycode and governmentform='Federal Republic')
  group by country_co.code,countrylanguage_co.countrycode order by country_co.code)
 
   UNION ALL
 
  (
   select count(*) as lang_count,country_co.code,countrylanguage_co.countrycode
  from country_co join countrylanguage_co on (country_co.code=countrylanguage_co.countrycode and governmentform='Monarchy')
  group by country_co.code,countrylanguage_co.countrycode order by country_co.code)
 
 ) FOO1
)
select * from
(
select count(*) as cnt,country_co.code,country_co.name 
from
country_co,countrylanguage_co
where country_co.code=countrylanguage_co.countrycode group by country_co.code,country_co.name) AS FOO
where foo.cnt = (select max(lang_count) from lang_total) order by foo.code;

--query4
with diversecountries as
(select country_co.code,country_co.name,country_co.capital,d.CNT
 from country_co,
 (select countrylanguage_co.countrycode,count(*) as CNT from countrylanguage_co group by countrycode
  HAVING count(*) > 6) d
 where d.countrycode = country_co.code and country_co.gnp > 100000)

 select diversecountries.name,city_co.name,diversecountries.CNT
 from diversecountries,city_co where city_co.id = diversecountries.capital
 order by diversecountries.name;

-- some queries with merge joins and index scans
set enable_nestloop=off;
set enable_hashjoin=off;
set enable_mergejoin=on;


-- query 5
with somecheapasiandiversecountries as
(
 select FOO.code,FOO.COUNTRY_CO,FOO.CAPITAL,FOO.headofstate,count(*) ASIAN_COUNT from
 (
 select country_co.code,country_co.name COUNTRY_CO,city_co.name CAPITAL,country_co.headofstate
 from country_co,city_co
 where country_co.capital = city_co.id 
 and country_co.gnp < 10000
 and country_co.region = 'Southeast Asia'
 and country_co.continent = 'Asia'
 
 UNION ALL

 select country_co.code,country_co.name COUNTRY_CO,city_co.name CAPITAL,country_co.headofstate
 from country_co,city_co
 where country_co.capital = city_co.id 
 and country_co.gnp < 10000
 and country_co.region = 'Eastern Asia'
 and country_co.continent = 'Asia'

 UNION ALL

 select country_co.code,country_co.name COUNTRY_CO,city_co.name CAPITAL,country_co.headofstate
 from country_co,city_co
 where country_co.capital = city_co.id 
 and country_co.gnp < 10000
 and country_co.region = 'Middle East'
 and country_co.continent = 'Asia'
 ) FOO, countrylanguage_co
 where FOO.code = countrylanguage_co.countrycode
 group by FOO.code,FOO.COUNTRY_CO,FOO.CAPITAL,FOO.headofstate,countrylanguage_co.countrycode
 HAVING count(*) >=
 (select min(CNT) FROM
   (select count(*) CNT,country_co.code from countrylanguage_co,country_co
    where countrylanguage_co.countrycode=country_co.code
    and country_co.continent = 'Asia'
    and country_co.region = 'Southern and Central Asia'
    group by country_co.code
   ) FOO1
 )
)

select FOO.code,FOO.COUNTRY_CO,FOO.CAPITAL,FOO.headofstate,count(*) COMPARED_WITH_CHEAP_ASIAN_CNT
from
(
select country_co.code,country_co.name COUNTRY_CO,city_co.name CAPITAL,country_co.headofstate
from country_co,city_co
where country_co.capital = city_co.id 
and country_co.continent = 'North America'


UNION ALL

select country_co.code,country_co.name COUNTRY_CO,city_co.name CAPITAL,country_co.headofstate
from country_co,city_co
where country_co.capital =	city_co.id	
and country_co.continent =	'South America'
) FOO,countrylanguage_co

where FOO.code = countrylanguage_co.countrycode
group by FOO.code,FOO.COUNTRY_CO,FOO.CAPITAL,FOO.headofstate

HAVING count(*)  >=
 (select min(ASIAN_COUNT) FROM
   (select ASIAN_COUNT FROM somecheapasiandiversecountries,country_co
    where somecheapasiandiversecountries.code = country_co.code
    and country_co.gnp >= country_co.gnpold
   ) ASIANCOUNT
 )
order by COUNTRY_CO;


-- query 6 
 

select count(*) from
( select r.* from
  ( with fact as 
     (
      select country_co.name as COUNTRY_CO,country_co.code,city_co.name as CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,AGG1.region
      from      
         (select
         sum(case when (city_co.population >= 0.5 * country_co.population) then country_co.population else city_co.population end) as S_POPULATION,
         sum(case when (gnp >= gnpold) then gnp else gnpold end) as S_GNP,
         avg(case when (lifeexpectancy > 60) then 50 else lifeexpectancy end) as AVG_LIFE,country_co.region
         from country_co,city_co  
         where governmentform != 'Constitutional Monarchy'
         and country_co.capital = city_co.id
         and indepyear > 0
         group by country_co.region) AGG1
         ,country_co,city_co
         where country_co.capital = city_co.id
         and country_co.region = AGG1.region
      )
     
     select code,COUNTRY_CO,CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,language as OFFICIALLANGUAGE,region
     from fact,countrylanguage_co
     where fact.code = countrylanguage_co.countrycode and isofficial = 'True'
     and fact.region = 'South America'
     
     UNION ALL
     
     select code,COUNTRY_CO,CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,language as OFFICIALLANGUAGE,region
     from fact,countrylanguage_co
     where fact.code = countrylanguage_co.countrycode and isofficial = 'True'
     and fact.region = 'North America'
     
     UNION ALL
     
     select code,COUNTRY_CO,CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,language as OFFICIALLANGUAGE,region
     from fact,countrylanguage_co
     where fact.code = countrylanguage_co.countrycode and isofficial = 'True'
     and fact.region = 'Caribbean'
 ) as r
 left join
  (
   select 'ARG' as CODE UNION ALL
   select 'BOL' as CODE UNION ALL
   select 'BRA' as CODE UNION ALL
   select 'PER' as CODE UNION ALL
   select 'URY' as CODE UNION ALL
   select 'IND' as CODE  UNION ALL
   select 'LCA' as CODE UNION ALL
   select 'VCT' as CODE
   ) as r1
on r.code = r1.code) AS FOO;


-- query7
with alleuropeanlanguages as 
(select country_co.code,country_co.name COUNTRY_CO, city_co.name CAPITAL, language, isofficial, percentage
 FROM country_co,city_co,countrylanguage_co
 WHERE country_co.code = countrylanguage_co.countrycode
 and country_co.capital = city_co.id
 and country_co.continent = 'Europe')

select * from
(select * from alleuropeanlanguages where isofficial='True') e1,
(select * from alleuropeanlanguages where percentage > 50) e2
where e1.code = e2.code order by e2.COUNTRY_CO,e1.language;


-- query8

with allcountry_costats as 
( select country_co.code,country_co.name,count(distinct city_co.id) CITY_CO_CNT,
  count(distinct countrylanguage_co.language) LANG_CNT
  from country_co,city_co,countrylanguage_co
  where country_co.code = city_co.countrycode
  and country_co.code = countrylanguage_co.countrycode
  group by country_co.code,country_co.name
)

select sum(FOO.CITY_CO_CNT) REGION_CITY_CO_CNT,sum(FOO.LANG_CNT) REGION_LANG_CNT,FOO.region
FROM
(
select allcountry_costats.code,allcountry_costats.name COUNTRY_CO,CITY_CO_CNT,LANG_CNT,country_co.region,city_co.name CAPITAL
from allcountry_costats,country_co,city_co
where allcountry_costats.code = country_co.code
and country_co.capital = city_co.id
and CITY_CO_CNT/LANG_CNT > 1
and country_co.continent = 'Asia'

UNION ALL

select allcountry_costats.code,allcountry_costats.name COUNTRY_CO,CITY_CO_CNT,LANG_CNT,country_co.region,city_co.name CAPITAL
from allcountry_costats,country_co,city_co
where allcountry_costats.code = country_co.code
and country_co.capital = city_co.id
and CITY_CO_CNT/LANG_CNT > 1
and country_co.continent = 'North America'

UNION ALL

select allcountry_costats.code,allcountry_costats.name COUNTRY_CO,CITY_CO_CNT,LANG_CNT,country_co.region,city_co.name CAPITAL
from allcountry_costats,country_co,city_co
where allcountry_costats.code = country_co.code
and country_co.capital = city_co.id
and CITY_CO_CNT/LANG_CNT > (select  max(CITY_CO_CNT/LANG_CNT)  from allcountry_costats,country_co where allcountry_costats.code = country_co.code AND country_co.continent='Europe')
) FOO
,allcountry_costats,country_co

WHERE allcountry_costats.code = country_co.code
and FOO.region = country_co.region
group by FOO.region order by FOO.region;

-- some queries with hash joins and index scans
set enable_nestloop=off;
set enable_hashjoin=on;
set enable_mergejoin=off;


--query 9
with diversecountries as
(select country_co.code,country_co.name,country_co.capital,d.CNT
 from country_co,
 (select countrylanguage_co.countrycode,count(*) as CNT from countrylanguage_co group by countrycode
  HAVING count(*) > 6) d
 where d.countrycode = country_co.code and country_co.gnp > 100000)

select d1.code,d1.name,d1.capital,city_co.name CAPITAL_CITY_CO,d1.CNT,d2.CNT
from
diversecountries d1 left join country_co
ON (d1.code = country_co.code AND d1.CNT < 8)
left join diversecountries d2
ON (country_co.code = d2.code AND d2.CNT > 8)
INNER JOIN city_co
ON(d1.capital = city_co.id)
ORDER BY d1.name;

--query 10 , multiple ctes, joins
with longlivingregions as 
(
select FOO.*,count(distinct language) as "lang_count"
from(
     select
       sum(population) as "REGION_POP",
       sum(gnp) as "REGION_GNP",
       avg(lifeexpectancy) as "REGION_LIFETIME",region
     from
      country_co
     group by region
    ) FOO,countrylanguage_co,country_co
where
   country_co.code = countrylanguage_co.countrycode
   and FOO.region = country_co.region
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
      country_co
     group by region
    ) FOO,countrylanguage_co,country_co
where
   country_co.code = countrylanguage_co.countrycode
   and FOO.region = country_co.region
   and FOO."REGION_POP" != 0
group by
FOO.region,foo."REGION_POP",foo."REGION_GNP"
order by sum(surfacearea)/foo."REGION_POP" desc),

allcountry_costats as 
( select country_co.code,country_co.name,count(distinct city_co.id) CITY_CO_CNT,
  count(distinct countrylanguage_co.language) LANG_CNT
  from country_co,city_co,countrylanguage_co
  where country_co.code = city_co.countrycode
  and country_co.code = countrylanguage_co.countrycode
  group by country_co.code,country_co.name
)

select allcountry_costats.CITY_CO_CNT,allcountry_costats.LANG_CNT,allcountry_costats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountry_costats,country_co
where longlivingregions.region = denseregions.region and allcountry_costats.code = country_co.code and country_co.region = longlivingregions.region
and country_co.indepyear between 1800 and 1850

UNION ALL

select allcountry_costats.CITY_CO_CNT,allcountry_costats.LANG_CNT,allcountry_costats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountry_costats,country_co
where longlivingregions.region = denseregions.region and allcountry_costats.code = country_co.code and country_co.region = longlivingregions.region
and country_co.indepyear between 1850 and 1900

UNION ALL

select allcountry_costats.CITY_CO_CNT,allcountry_costats.LANG_CNT,allcountry_costats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountry_costats,country_co
where longlivingregions.region = denseregions.region and allcountry_costats.code = country_co.code and country_co.region = longlivingregions.region
and country_co.indepyear > 1900
order by name
LIMIT 50;

--query 11
with allcity_costats as 
( select city_co.name CITY_CO,city_co.id,country_co.name COUNTRY_CO,city_co.district,city_co.population as CITY_CO_POP
  from
  city_co,country_co
  where city_co.countrycode = country_co.code
),
alldistrictstats as 
( select allcity_costats.district,allcity_costats.COUNTRY_CO,sum(CITY_CO_POP) DISTRICT_POP,
  count(CITY_CO) as D_CITY_CO_CNT
  from allcity_costats
  group by allcity_costats.district,allcity_costats.COUNTRY_CO
  order by district,COUNTRY_CO
),
allcountry_costats as 
( select alldistrictstats.COUNTRY_CO,country_co.code,sum(D_CITY_CO_CNT) C_CITY_CO_CNT,
  count(distinct countrylanguage_co.language) C_LANG_CNT
  from alldistrictstats,country_co,countrylanguage_co
  where alldistrictstats.COUNTRY_CO = country_co.name
  and country_co.code = countrylanguage_co.countrycode
  group by COUNTRY_CO,code
),
asian_region_stats as 
(
select sum(FOO.C_CITY_CO_CNT) REGION_CITY_CO_CNT,sum(FOO.C_LANG_CNT) REGION_LANG_CNT,FOO.region
FROM
(
select allcountry_costats.code,allcountry_costats.COUNTRY_CO,C_CITY_CO_CNT,C_LANG_CNT,country_co.region,city_co.name CAPITAL
from allcountry_costats,country_co,city_co
where allcountry_costats.code = country_co.code
and country_co.capital = city_co.id
and C_CITY_CO_CNT/C_LANG_CNT > 1
and country_co.continent = 'Asia') FOO
,allcountry_costats,country_co

WHERE allcountry_costats.code = country_co.code
and FOO.region = country_co.region
group by FOO.region order by FOO.region
)

select * from
(
select REGION_CITY_CO_CNT as CITY_CO_CNT,REGION_LANG_CNT as LANG_CNT, region as IDENTIFIER from asian_region_stats
UNION ALL
(
select sum(FOO.C_CITY_CO_CNT) CITY_CO_CNT,sum(FOO.C_LANG_CNT) LANG_CNT,FOO.region as IDENTIFIER
FROM
(
select allcountry_costats.code,allcountry_costats.COUNTRY_CO,C_CITY_CO_CNT,C_LANG_CNT,country_co.region,allcity_costats.CITY_CO CAPITAL
from allcountry_costats,country_co,allcity_costats
where allcountry_costats.code = country_co.code
and country_co.capital = allcity_costats.id
and C_CITY_CO_CNT/C_LANG_CNT > 1
and country_co.continent = 'Europe') FOO
,allcountry_costats,country_co

WHERE allcountry_costats.code = country_co.code
and FOO.region = country_co.region
group by FOO.region order by FOO.region
)
) FOO1
order by FOO1.lang_cnt,FOO1.identifier;

