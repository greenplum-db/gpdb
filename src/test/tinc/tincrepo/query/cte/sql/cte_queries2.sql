-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @skip MPP-21028
-- @db_name world_db
-- @description cte tests from cdbfast 

--queries with a single CTE used more than once in different parts of the main query

-- query1
-- Similar to genetech query 'dxAcctMonthlyTrending_Avastin_WITH_Clause.sql'
-- This kind of query is their only use case for CTE. We dont error, we give correct results,use shared scan here and we are good ! 

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


-- query2
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


-- query3

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


--query 4
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


--query 5 Use CTE more than once in select list , from clause and where clause without correlation
with official_languages as
(
 select country.code,country.name,countrylanguage.language
 from
 country,countrylanguage
 where country.code = countrylanguage.countrycode and isofficial = 'True'
 and country.governmentform NOT  IN (select 'Commonwealth of the US' UNION ALL select 'Monarchy (Sultanate)' UNION ALL select 'Monarchy')
 and country.gnp > (select min(gnpold) from country where country.region = 'Western Europe')
)

select 
( select max(CNT) from (select count(*) CNT,o1.name from official_languages o1, official_languages o2
  where o1.code = o2.code group by o1.name) FOO
),* from official_languages;


--query 6 Use CTE in the main query and subqueries within the main query

with bad_headofstates as 
(
 select country.code,country.name,country.headofstate,countrylanguage.language
 from
 country,countrylanguage
 where country.code = countrylanguage.countrycode and countrylanguage.isofficial=true
 and (country.gnp < country.gnpold or country.gnp < 3000)
)
select OUTERMOST_FOO.*,bad_headofstates.headofstate from (
select avg(population),region from
(
select FOO.*,bad_headofstates.headofstate,city.name
from
(select bad_headofstates.code,country.capital,country.region,country.population from
bad_headofstates,country where bad_headofstates.code = country.code) FOO, bad_headofstates,city
where FOO.code = bad_headofstates.code and FOO.capital = city.id) OUTER_FOO
group by region ) OUTERMOST_FOO,bad_headofstates,country 
where country.code = bad_headofstates.code and country.region = OUTERMOST_FOO.region
order by OUTERMOST_FOO.region,bad_headofstates.headofstate LIMIT 40;


-- query 7 Use CTE in the main query, where clause and having clause
with district_population as 
(select sum(city.population) DISTRICT_POP,count(*) NUM_CITIES,district,countrycode,country.name COUNTRY
 from city,country
 where city.countrycode = country.code
 group by district,countrycode,country.name
 HAVING (sum(city.population)/count(*)) > ( select avg(population) from city where countrycode = 'CHN'))

select sum(FOO.DISTRICT_POP),sum(FOO.NUM_CITIES),COUNTRY,CAPITAL,CAPITAL_POP
from
(

(select district_population.*,city.name CAPITAL,city.population CAPITAL_POP from
district_population,country,city
where district_population.countrycode = country.code AND city.id = country.capital
AND DISTRICT_POP >= ( select avg(DISTRICT_POP) FROM district_population where district IN (select district from city where countrycode = 'USA') )
order by COUNTRY,district)
UNION ALL
(select district_population.*,city.name CAPITAL,city.population CAPITAL_POP from
district_population,country,city
where district_population.countrycode = country.code AND city.id = country.capital
AND DISTRICT_POP >= ( select avg(DISTRICT_POP) FROM district_population where district IN (select district from city where countrycode = 'IND') )
order by COUNTRY,district)
UNION ALL
(select district_population.*,city.name CAPITAL,city.population CAPITAL_POP from
district_population,country,city
where district_population.countrycode = country.code AND city.id = country.capital
AND DISTRICT_POP >= ( select avg(DISTRICT_POP) FROM district_population where district IN (select district from city where countrycode = 'CHN') )
order by COUNTRY,district)

) FOO

WHERE FOO.CAPITAL_POP > (select min(DISTRICT_POP) from district_population)

group by FOO.COUNTRY,FOO.CAPITAL,FOO.CAPITAL_POP

HAVING sum(FOO.DISTRICT_POP) >= ( select avg(DISTRICT_POP) FROM district_population where district IN (select district from city where countrycode = 'AUS') )
AND (sum(FOO.DISTRICT_POP)/sum(FOO.NUM_CITIES)) <= 
( select avg(DISTRICT_POP) FROM district_population where district IN (select district from city where countrycode = 'USA' or countrycode = 'IND' or countrycode = 'CHN'))
order by FOO.country;

-- query8 Use CTE in the select list and the from clause - MPP-13943
with official_languages as
(
 select country.code,country.name,countrylanguage.language
 from
 country,countrylanguage
 where country.code = countrylanguage.countrycode and isofficial = 'True'
)

select
( select max(CNT) from (select count(*) CNT from official_languages) FOO	
)
,* from official_languages order by official_languages.code,official_languages.language;

