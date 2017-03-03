-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @skip MPP-21027
-- @db_name world_db
-- @description cte tests from cdbfast 

-- queries with one CTE that is referenced once

-- Using CTE in the FROM clause

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
 from diversecountries,city where city.id = diversecountries.capital;


--query5 -Using a CTE in the select list
with bigcities as
(
select city.id,city.name,city.population,city.countrycode
from city                                                                                                                                                  
where city.population >= 0.5 * (select population from country where country.code=city.countrycode)
)
select
(select max(bigcities.population) from bigcities where bigcities.countrycode='QAT') as MAX_POP,                                                           
(select avg(bigcities.population) from bigcities) AS WORLD_AVG,
 city.name,city.population
 from                                                                                                                                
 city where city.countrycode='QAT';

--query6 using CTE in the select list with a qual
with diversecountries as
(select country.code,country.name,country.capital,d.CNT
 from country, 
 (select countrylanguage.countrycode,count(*) as CNT from countrylanguage group by countrycode
  HAVING count(*) > 6) d
 where d.countrycode = country.code and country.gnp > 100000)
select * from
(
select
(select max(CNT) from diversecountries where  diversecountries.code = country.code) CNT,country.name COUNTRY,city.name CAPITAL
from country,city where country.capital = city.id) FOO where FOO.CNT is not null;


--queries Using a CTE in the HAVING clause

with notdiversecountries as
(select country.code,country.name,country.capital,d.CNT
 from country,
 (select countrylanguage.countrycode,count(*) as CNT from countrylanguage group by countrycode
  HAVING count(*) < 3) d
 where d.countrycode = country.code and country.gnp > 100000)

select country.name COUNTRY,city.name CAPITAL,count(*) LANGCNT from
country,city,countrylanguage
where country.code = countrylanguage.countrycode and country.capital = city.id
group by country.name,city.name
HAVING count(*) NOT IN (select CNT from notdiversecountries where notdiversecountries.name = country.name)
order by country.name
LIMIT 10;


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

 
 




