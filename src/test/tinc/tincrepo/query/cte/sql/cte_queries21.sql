-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @skip GPSQL-1020
-- @db_name world_db
-- @description cte tests from cdbfast 

-- queries with CTEs using index scans on bitmap indexes
\echo --start_ignore
Drop index bitmap_city_countrycode;
Drop index bitmap_country_gf;
Drop index bitmap_country_region;
Drop index bitmap_country_continent;
Drop index bitmap_countrylanguage_countrycode;
\echo --end_ignore
create index bitmap_city_countrycode on city using bitmap(countrycode);
create index bitmap_country_gf on country using bitmap(governmentform);
create index bitmap_country_region on country using bitmap(region);
create index bitmap_country_continent on country using bitmap(continent);
create index bitmap_countrylanguage_countrycode on countrylanguage using bitmap(countrycode);



set enable_seqscan=off;
set enable_indexscan=on;




--query1
with capitals as 
(select country.code,id,city.name from city,country 
 where city.countrycode = country.code AND city.id = country.capital) 

select * from 
capitals,countrylanguage
where capitals.code = countrylanguage.countrycode and isofficial='true'
order by capitals.code,countrylanguage.language;

--query2
with lang_total as
( select count(*) as lang_count,country.code,countrylanguage.countrycode
  from country join countrylanguage on (country.code=countrylanguage.countrycode and governmentform='Federal Republic')
  group by country.code,countrylanguage.countrycode order by country.code)
 
select lang_count,country.code,country.name,country.continent,country.region,country.population
 from country left outer join lang_total
 on (lang_total.code = country.code)
 where country.indepyear > 1970 and lang_total.lang_count >=1 and lang_total.lang_count >=1
  order by lang_total.lang_count desc;


-- Using CTE in the WHERE clause

--query3
with lang_total as
( select lang_count,countrycode from
  (
  (
   select count(*) as lang_count,country.code,countrylanguage.countrycode
  from country join countrylanguage on (country.code=countrylanguage.countrycode and governmentform='Federal Republic')
  group by country.code,countrylanguage.countrycode order by country.code)
 
   UNION ALL
 
  (
   select count(*) as lang_count,country.code,countrylanguage.countrycode
  from country join countrylanguage on (country.code=countrylanguage.countrycode and governmentform='Monarchy')
  group by country.code,countrylanguage.countrycode order by country.code)
 
 ) FOO1
)
select * from
(
select count(*) as cnt,country.code,country.name 
from
country,countrylanguage
where country.code=countrylanguage.countrycode group by country.code,country.name) AS FOO
where foo.cnt = (select max(lang_count) from lang_total) order by foo.code;

--query4
with diversecountries as
(select country.code,country.name,country.capital,d.CNT
 from country,
 (select countrylanguage.countrycode,count(*) as CNT from countrylanguage group by countrycode
  HAVING count(*) > 6) d
 where d.countrycode = country.code and country.gnp > 100000)

 select diversecountries.name,city.name,diversecountries.CNT
 from diversecountries,city where city.id = diversecountries.capital
 order by diversecountries.name;

-- some queries with merge joins and index scans
set enable_nestloop=off;
set enable_hashjoin=off;
set enable_mergejoin=on;


-- query 5
with somecheapasiandiversecountries as
(
 select FOO.code,FOO.COUNTRY,FOO.CAPITAL,FOO.headofstate,count(*) ASIAN_COUNT from
 (
 select country.code,country.name COUNTRY,city.name CAPITAL,country.headofstate
 from country,city
 where country.capital = city.id 
 and country.gnp < 10000
 and country.region = 'Southeast Asia'
 and country.continent = 'Asia'
 
 UNION ALL

 select country.code,country.name COUNTRY,city.name CAPITAL,country.headofstate
 from country,city
 where country.capital = city.id 
 and country.gnp < 10000
 and country.region = 'Eastern Asia'
 and country.continent = 'Asia'

 UNION ALL

 select country.code,country.name COUNTRY,city.name CAPITAL,country.headofstate
 from country,city
 where country.capital = city.id 
 and country.gnp < 10000
 and country.region = 'Middle East'
 and country.continent = 'Asia'
 ) FOO, countrylanguage
 where FOO.code = countrylanguage.countrycode
 group by FOO.code,FOO.COUNTRY,FOO.CAPITAL,FOO.headofstate,countrylanguage.countrycode
 HAVING count(*) >=
 (select min(CNT) FROM
   (select count(*) CNT,country.code from countrylanguage,country
    where countrylanguage.countrycode=country.code
    and country.continent = 'Asia'
    and country.region = 'Southern and Central Asia'
    group by country.code
   ) FOO1
 )
)

select FOO.code,FOO.COUNTRY,FOO.CAPITAL,FOO.headofstate,count(*) COMPARED_WITH_CHEAP_ASIAN_CNT
from
(
select country.code,country.name COUNTRY,city.name CAPITAL,country.headofstate
from country,city
where country.capital = city.id 
and country.continent = 'North America'


UNION ALL

select country.code,country.name COUNTRY,city.name CAPITAL,country.headofstate
from country,city
where country.capital =	city.id	
and country.continent =	'South America'
) FOO,countrylanguage

where FOO.code = countrylanguage.countrycode
group by FOO.code,FOO.COUNTRY,FOO.CAPITAL,FOO.headofstate

HAVING count(*)  >=
 (select min(ASIAN_COUNT) FROM
   (select ASIAN_COUNT FROM somecheapasiandiversecountries,country
    where somecheapasiandiversecountries.code = country.code
    and country.gnp >= country.gnpold
   ) ASIANCOUNT
 )
order by COUNTRY;


-- query 6 
 

select count(*) from
( select r.* from
  ( with fact as 
     (
      select country.name as COUNTRY,country.code,city.name as CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,AGG1.region
      from      
         (select
         sum(case when (city.population >= 0.5 * country.population) then country.population else city.population end) as S_POPULATION,
         sum(case when (gnp >= gnpold) then gnp else gnpold end) as S_GNP,
         avg(case when (lifeexpectancy > 60) then 50 else lifeexpectancy end) as AVG_LIFE,country.region
         from country,city  
         where governmentform != 'Constitutional Monarchy'
         and country.capital = city.id
         and indepyear > 0
         group by country.region) AGG1
         ,country,city
         where country.capital = city.id
         and country.region = AGG1.region
      )
     
     select code,COUNTRY,CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,language as OFFICIALLANGUAGE,region
     from fact,countrylanguage
     where fact.code = countrylanguage.countrycode and isofficial = 'True'
     and fact.region = 'South America'
     
     UNION ALL
     
     select code,COUNTRY,CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,language as OFFICIALLANGUAGE,region
     from fact,countrylanguage
     where fact.code = countrylanguage.countrycode and isofficial = 'True'
     and fact.region = 'North America'
     
     UNION ALL
     
     select code,COUNTRY,CAPITAL,S_POPULATION,S_GNP,AVG_LIFE,language as OFFICIALLANGUAGE,region
     from fact,countrylanguage
     where fact.code = countrylanguage.countrycode and isofficial = 'True'
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
(select country.code,country.name COUNTRY, city.name CAPITAL, language, isofficial, percentage
 FROM country,city,countrylanguage
 WHERE country.code = countrylanguage.countrycode
 and country.capital = city.id
 and country.continent = 'Europe')

select * from
(select * from alleuropeanlanguages where isofficial='True') e1,
(select * from alleuropeanlanguages where percentage > 50) e2
where e1.code = e2.code order by e2.COUNTRY,e1.language;


-- query8

with allcountrystats as 
( select country.code,country.name,count(distinct city.id) CITY_CNT,
  count(distinct countrylanguage.language) LANG_CNT
  from country,city,countrylanguage
  where country.code = city.countrycode
  and country.code = countrylanguage.countrycode
  group by country.code,country.name
)

select sum(FOO.CITY_CNT) REGION_CITY_CNT,sum(FOO.LANG_CNT) REGION_LANG_CNT,FOO.region
FROM
(
select allcountrystats.code,allcountrystats.name COUNTRY,CITY_CNT,LANG_CNT,country.region,city.name CAPITAL
from allcountrystats,country,city
where allcountrystats.code = country.code
and country.capital = city.id
and CITY_CNT/LANG_CNT > 1
and country.continent = 'Asia'

UNION ALL

select allcountrystats.code,allcountrystats.name COUNTRY,CITY_CNT,LANG_CNT,country.region,city.name CAPITAL
from allcountrystats,country,city
where allcountrystats.code = country.code
and country.capital = city.id
and CITY_CNT/LANG_CNT > 1
and country.continent = 'North America'

UNION ALL

select allcountrystats.code,allcountrystats.name COUNTRY,CITY_CNT,LANG_CNT,country.region,city.name CAPITAL
from allcountrystats,country,city
where allcountrystats.code = country.code
and country.capital = city.id
and CITY_CNT/LANG_CNT > (select  max(CITY_CNT/LANG_CNT)  from allcountrystats,country where allcountrystats.code = country.code AND country.continent='Europe')
) FOO
,allcountrystats,country

WHERE allcountrystats.code = country.code
and FOO.region = country.region
group by FOO.region order by FOO.region;

-- some queries with hash joins and index scans
set enable_nestloop=off;
set enable_hashjoin=on;
set enable_mergejoin=off;


--query 9
with diversecountries as
(select country.code,country.name,country.capital,d.CNT
 from country,
 (select countrylanguage.countrycode,count(*) as CNT from countrylanguage group by countrycode
  HAVING count(*) > 6) d
 where d.countrycode = country.code and country.gnp > 100000)

select d1.code,d1.name,d1.capital,city.name CAPITAL_CITY,d1.CNT,d2.CNT
from
diversecountries d1 left join country
ON (d1.code = country.code AND d1.CNT < 8)
left join diversecountries d2
ON (country.code = d2.code AND d2.CNT > 8)
INNER JOIN city
ON(d1.capital = city.id)
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

--query 11
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
