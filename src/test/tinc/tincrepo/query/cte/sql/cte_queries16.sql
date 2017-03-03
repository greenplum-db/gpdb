-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description cte tests from cdbfast 

-- negative cases with queries forward referencing other CTEs

-- query1 CTE referencing itself
with lang_total as
( select count(*) as lang_count,country.code,countrylanguage.countrycode
  from country join countrylanguage on (country.code=countrylanguage.countrycode and governmentform='Federal Republic')
  group by country.code,countrylanguage.countrycode 
  HAVING count(*) > (select max(lang_count) from lang_total)
)

select count(*) from lang_total;

--query2 CTE forward referencing another CTE
with capitals as 
(select country.code,id,city.name from city,country 
 where city.countrycode = country.code AND city.id = country.capital),

lang_total as
( select count(*) as lang_count,country.code,alleuropeanlanguages.code
  from country join alleuropeanlanguages on (country.code=alleuropeanlanguages.code and governmentform='Federal Republic')
  group by country.code,alleuropeanlanguages.code order by country.code),

alleuropeanlanguages as 
(select country.code,country.name COUNTRY, city.name CAPITAL, language, isofficial, percentage
 FROM country,city,countrylanguage
 WHERE country.code = countrylanguage.countrycode
 and country.capital = city.id
 and country.continent = 'Europe')

select * from alleuropeanlanguages;