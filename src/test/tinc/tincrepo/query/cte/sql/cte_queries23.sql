-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @skip GPSQL-1020
-- @db_name world_db
-- @gucs enable_seqscan=off;enable_indexscan=on
-- @description cte tests from cdbfast 

-- queries with CTEs on AO tables
--query1
with capitals as 
(select country_ao.code,id,city_ao.name from city_ao,country_ao 
 where city_ao.countrycode = country_ao.code AND city_ao.id = country_ao.capital) 

select * from 
capitals,countrylanguage_ao
where capitals.code = countrylanguage_ao.countrycode and isofficial='true'
order by capitals.code,countrylanguage_ao.language;

--query2
with lang_total as
( select count(*) as lang_count,country_ao.code,countrylanguage_ao.countrycode
  from country_ao join countrylanguage_ao on (country_ao.code=countrylanguage_ao.countrycode and governmentform='Federal Republic')
  group by country_ao.code,countrylanguage_ao.countrycode order by country_ao.code)
 
select lang_count,country_ao.code,country_ao.name,country_ao.continent,country_ao.region,country_ao.population
 from country_ao left outer join lang_total
 on (lang_total.code = country_ao.code)
 where country_ao.indepyear > 1970 and lang_total.lang_count >=1 and lang_total.lang_count >=1
  order by lang_total.lang_count desc;


-- Using CTE in the WHERE clause

--query3
with lang_total as
( select lang_count,countrycode from
  (
  (
   select count(*) as lang_count,country_ao.code,countrylanguage_ao.countrycode
  from country_ao join countrylanguage_ao on (country_ao.code=countrylanguage_ao.countrycode and governmentform='Federal Republic')
  group by country_ao.code,countrylanguage_ao.countrycode order by country_ao.code)
 
   UNION ALL
 
  (
   select count(*) as lang_count,country_ao.code,countrylanguage_ao.countrycode
  from country_ao join countrylanguage_ao on (country_ao.code=countrylanguage_ao.countrycode and governmentform='Monarchy')
  group by country_ao.code,countrylanguage_ao.countrycode order by country_ao.code)
 
 ) FOO1
)
select * from
(
select count(*) as cnt,country_ao.code,country_ao.name 
from
country_ao,countrylanguage_ao
where country_ao.code=countrylanguage_ao.countrycode group by country_ao.code,country_ao.name) AS FOO
where foo.cnt = (select max(lang_count) from lang_total) order by foo.code;

--query4
with diversecountries as
(select country_ao.code,country_ao.name,country_ao.capital,d.CNT
 from country_ao,
 (select countrylanguage_ao.countrycode,count(*) as CNT from countrylanguage_ao group by countrycode
  HAVING count(*) > 6) d
 where d.countrycode = country_ao.code and country_ao.gnp > 100000)

 select diversecountries.name,city_ao.name,diversecountries.CNT
 from diversecountries,city_ao where city_ao.id = diversecountries.capital
 order by diversecountries.name;

-- some queries with merge joins and index scans
set enable_nestloop=off;
set enable_hashjoin=off;
set enable_mergejoin=on;


-- query 5
with somecheapasiandiversecountries as
(
 select FOO.code,FOO.COUNTRY_AO,FOO.CAPITAL,FOO.headofstate,count(*) ASIAN_COUNT from
 (
 select country_ao.code,country_ao.name COUNTRY_AO,city_ao.name CAPITAL,country_ao.headofstate
 from country_ao,city_ao
 where country_ao.capital = city_ao.id 
 and country_ao.gnp < 10000
 and country_ao.region = 'Southeast Asia'
 and country_ao.continent = 'Asia'
 
 UNION ALL

 select country_ao.code,country_ao.name COUNTRY_AO,city_ao.name CAPITAL,country_ao.headofstate
 from country_ao,city_ao
 where country_ao.capital = city_ao.id 
 and country_ao.gnp < 10000
 and country_ao.region = 'Eastern Asia'
 and country_ao.continent = 'Asia'

 UNION ALL

 select country_ao.code,country_ao.name COUNTRY_AO,city_ao.name CAPITAL,country_ao.headofstate
 from country_ao,city_ao
 where country_ao.capital = city_ao.id 
 and country_ao.gnp < 10000
 and country_ao.region = 'Middle East'
 and country_ao.continent = 'Asia'
 ) FOO, countrylanguage_ao
 where FOO.code = countrylanguage_ao.countrycode
 group by FOO.code,FOO.COUNTRY_AO,FOO.CAPITAL,FOO.headofstate,countrylanguage_ao.countrycode
 HAVING count(*) >=
 (select min(CNT) FROM
   (select count(*) CNT,country_ao.code from countrylanguage_ao,country_ao
    where countrylanguage_ao.countrycode=country_ao.code
    and country_ao.continent = 'Asia'
    and country_ao.region = 'Southern and Central Asia'
    group by country_ao.code
   ) FOO1
 )
)

select FOO.code,FOO.COUNTRY_AO,FOO.CAPITAL,FOO.headofstate,count(*) COMPARED_WITH_CHEAP_ASIAN_CNT
from
(
select country_ao.code,country_ao.name COUNTRY_AO,city_ao.name CAPITAL,country_ao.headofstate
from country_ao,city_ao
where country_ao.capital = city_ao.id 
and country_ao.continent = 'North America'


UNION ALL

select country_ao.code,country_ao.name COUNTRY_AO,city_ao.name CAPITAL,country_ao.headofstate
from country_ao,city_ao
where country_ao.capital =	city_ao.id	
and country_ao.continent =	'South America'
) FOO,countrylanguage_ao

where FOO.code = countrylanguage_ao.countrycode
group by FOO.code,FOO.COUNTRY_AO,FOO.CAPITAL,FOO.headofstate

HAVING count(*)  >=
 (select min(ASIAN_COUNT) FROM
   (select ASIAN_COUNT FROM somecheapasiandiversecountries,country_ao
    where somecheapasiandiversecountries.code = country_ao.code
    and country_ao.gnp >= country_ao.gnpold
   ) ASIANCOUNT
 )
order by COUNTRY_AO;


-- query 6 
 

select count(*) from
( select r.* from
  ( with fact as 
     (
      select country_ao.name as COUNTRY_AO,country_ao.code,city_ao.name as CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,AGG1.region
      from      
         (select
         sum(case when (city_ao.population >= 0.5 * country_ao.population) then country_ao.population else city_ao.population end) as S_POPULATION,
         sum(case when (gnp >= gnpold) then gnp else gnpold end) as S_GNP,
         avg(case when (lifeexpectancy > 60) then 50 else lifeexpectancy end) as AVG_LIFE,country_ao.region
         from country_ao,city_ao  
         where governmentform != 'Constitutional Monarchy'
         and country_ao.capital = city_ao.id
         and indepyear > 0
         group by country_ao.region) AGG1
         ,country_ao,city_ao
         where country_ao.capital = city_ao.id
         and country_ao.region = AGG1.region
      )
     
     select code,COUNTRY_AO,CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,language as OFFICIALLANGUAGE,region
     from fact,countrylanguage_ao
     where fact.code = countrylanguage_ao.countrycode and isofficial = 'True'
     and fact.region = 'South America'
     
     UNION ALL
     
     select code,COUNTRY_AO,CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,language as OFFICIALLANGUAGE,region
     from fact,countrylanguage_ao
     where fact.code = countrylanguage_ao.countrycode and isofficial = 'True'
     and fact.region = 'North America'
     
     UNION ALL
     
     select code,COUNTRY_AO,CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,language as OFFICIALLANGUAGE,region
     from fact,countrylanguage_ao
     where fact.code = countrylanguage_ao.countrycode and isofficial = 'True'
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
(select country_ao.code,country_ao.name COUNTRY_AO, city_ao.name CAPITAL, language, isofficial, percentage
 FROM country_ao,city_ao,countrylanguage_ao
 WHERE country_ao.code = countrylanguage_ao.countrycode
 and country_ao.capital = city_ao.id
 and country_ao.continent = 'Europe')

select * from
(select * from alleuropeanlanguages where isofficial='True') e1,
(select * from alleuropeanlanguages where percentage > 50) e2
where e1.code = e2.code order by e2.COUNTRY_AO,e1.language;


-- query8

with allcountry_aostats as 
( select country_ao.code,country_ao.name,count(distinct city_ao.id) CITY_AO_CNT,
  count(distinct countrylanguage_ao.language) LANG_CNT
  from country_ao,city_ao,countrylanguage_ao
  where country_ao.code = city_ao.countrycode
  and country_ao.code = countrylanguage_ao.countrycode
  group by country_ao.code,country_ao.name
)

select sum(FOO.CITY_AO_CNT) REGION_CITY_AO_CNT,sum(FOO.LANG_CNT) REGION_LANG_CNT,FOO.region
FROM
(
select allcountry_aostats.code,allcountry_aostats.name COUNTRY_AO,CITY_AO_CNT,LANG_CNT,country_ao.region,city_ao.name CAPITAL
from allcountry_aostats,country_ao,city_ao
where allcountry_aostats.code = country_ao.code
and country_ao.capital = city_ao.id
and CITY_AO_CNT/LANG_CNT > 1
and country_ao.continent = 'Asia'

UNION ALL

select allcountry_aostats.code,allcountry_aostats.name COUNTRY_AO,CITY_AO_CNT,LANG_CNT,country_ao.region,city_ao.name CAPITAL
from allcountry_aostats,country_ao,city_ao
where allcountry_aostats.code = country_ao.code
and country_ao.capital = city_ao.id
and CITY_AO_CNT/LANG_CNT > 1
and country_ao.continent = 'North America'

UNION ALL

select allcountry_aostats.code,allcountry_aostats.name COUNTRY_AO,CITY_AO_CNT,LANG_CNT,country_ao.region,city_ao.name CAPITAL
from allcountry_aostats,country_ao,city_ao
where allcountry_aostats.code = country_ao.code
and country_ao.capital = city_ao.id
and CITY_AO_CNT/LANG_CNT > (select  max(CITY_AO_CNT/LANG_CNT)  from allcountry_aostats,country_ao where allcountry_aostats.code = country_ao.code AND country_ao.continent='Europe')
) FOO
,allcountry_aostats,country_ao

WHERE allcountry_aostats.code = country_ao.code
and FOO.region = country_ao.region
group by FOO.region order by FOO.region;

-- some queries with hash joins and index scans
set enable_nestloop=off;
set enable_hashjoin=on;
set enable_mergejoin=off;


--query 9
with diversecountries as
(select country_ao.code,country_ao.name,country_ao.capital,d.CNT
 from country_ao,
 (select countrylanguage_ao.countrycode,count(*) as CNT from countrylanguage_ao group by countrycode
  HAVING count(*) > 6) d
 where d.countrycode = country_ao.code and country_ao.gnp > 100000)

select d1.code,d1.name,d1.capital,city_ao.name CAPITAL_CITY_AO,d1.CNT,d2.CNT
from
diversecountries d1 left join country_ao
ON (d1.code = country_ao.code AND d1.CNT < 8)
left join diversecountries d2
ON (country_ao.code = d2.code AND d2.CNT > 8)
INNER JOIN city_ao
ON(d1.capital = city_ao.id)
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
      country_ao
     group by region
    ) FOO,countrylanguage_ao,country_ao
where
   country_ao.code = countrylanguage_ao.countrycode
   and FOO.region = country_ao.region
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
      country_ao
     group by region
    ) FOO,countrylanguage_ao,country_ao
where
   country_ao.code = countrylanguage_ao.countrycode
   and FOO.region = country_ao.region
   and FOO."REGION_POP" != 0
group by
FOO.region,foo."REGION_POP",foo."REGION_GNP"
order by sum(surfacearea)/foo."REGION_POP" desc),

allcountry_aostats as 
( select country_ao.code,country_ao.name,count(distinct city_ao.id) CITY_AO_CNT,
  count(distinct countrylanguage_ao.language) LANG_CNT
  from country_ao,city_ao,countrylanguage_ao
  where country_ao.code = city_ao.countrycode
  and country_ao.code = countrylanguage_ao.countrycode
  group by country_ao.code,country_ao.name
)

select allcountry_aostats.CITY_AO_CNT,allcountry_aostats.LANG_CNT,allcountry_aostats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountry_aostats,country_ao
where longlivingregions.region = denseregions.region and allcountry_aostats.code = country_ao.code and country_ao.region = longlivingregions.region
and country_ao.indepyear between 1800 and 1850

UNION ALL

select allcountry_aostats.CITY_AO_CNT,allcountry_aostats.LANG_CNT,allcountry_aostats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountry_aostats,country_ao
where longlivingregions.region = denseregions.region and allcountry_aostats.code = country_ao.code and country_ao.region = longlivingregions.region
and country_ao.indepyear between 1850 and 1900

UNION ALL

select allcountry_aostats.CITY_AO_CNT,allcountry_aostats.LANG_CNT,allcountry_aostats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountry_aostats,country_ao
where longlivingregions.region = denseregions.region and allcountry_aostats.code = country_ao.code and country_ao.region = longlivingregions.region
and country_ao.indepyear > 1900
order by name
LIMIT 50;

--query 11
with allcity_aostats as 
( select city_ao.name CITY_AO,city_ao.id,country_ao.name COUNTRY_AO,city_ao.district,city_ao.population as CITY_AO_POP
  from
  city_ao,country_ao
  where city_ao.countrycode = country_ao.code
),
alldistrictstats as 
( select allcity_aostats.district,allcity_aostats.COUNTRY_AO,sum(CITY_AO_POP) DISTRICT_POP,
  count(CITY_AO) as D_CITY_AO_CNT
  from allcity_aostats
  group by allcity_aostats.district,allcity_aostats.COUNTRY_AO
  order by district,COUNTRY_AO
),
allcountry_aostats as 
( select alldistrictstats.COUNTRY_AO,country_ao.code,sum(D_CITY_AO_CNT) C_CITY_AO_CNT,
  count(distinct countrylanguage_ao.language) C_LANG_CNT
  from alldistrictstats,country_ao,countrylanguage_ao
  where alldistrictstats.COUNTRY_AO = country_ao.name
  and country_ao.code = countrylanguage_ao.countrycode
  group by COUNTRY_AO,code
),
asian_region_stats as 
(
select sum(FOO.C_CITY_AO_CNT) REGION_CITY_AO_CNT,sum(FOO.C_LANG_CNT) REGION_LANG_CNT,FOO.region
FROM
(
select allcountry_aostats.code,allcountry_aostats.COUNTRY_AO,C_CITY_AO_CNT,C_LANG_CNT,country_ao.region,city_ao.name CAPITAL
from allcountry_aostats,country_ao,city_ao
where allcountry_aostats.code = country_ao.code
and country_ao.capital = city_ao.id
and C_CITY_AO_CNT/C_LANG_CNT > 1
and country_ao.continent = 'Asia') FOO
,allcountry_aostats,country_ao

WHERE allcountry_aostats.code = country_ao.code
and FOO.region = country_ao.region
group by FOO.region order by FOO.region
)

select * from
(
select REGION_CITY_AO_CNT as CITY_AO_CNT,REGION_LANG_CNT as LANG_CNT, region as IDENTIFIER from asian_region_stats
UNION ALL
(
select sum(FOO.C_CITY_AO_CNT) CITY_AO_CNT,sum(FOO.C_LANG_CNT) LANG_CNT,FOO.region as IDENTIFIER
FROM
(
select allcountry_aostats.code,allcountry_aostats.COUNTRY_AO,C_CITY_AO_CNT,C_LANG_CNT,country_ao.region,allcity_aostats.CITY_AO CAPITAL
from allcountry_aostats,country_ao,allcity_aostats
where allcountry_aostats.code = country_ao.code
and country_ao.capital = allcity_aostats.id
and C_CITY_AO_CNT/C_LANG_CNT > 1
and country_ao.continent = 'Europe') FOO
,allcountry_aostats,country_ao

WHERE allcountry_aostats.code = country_ao.code
and FOO.region = country_ao.region
group by FOO.region order by FOO.region
)
) FOO1
order by FOO1.lang_cnt,FOO1.identifier;

