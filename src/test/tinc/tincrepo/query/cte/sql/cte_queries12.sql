-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description cte tests from cdbfast 

-- queries using same name for CTEs and the subquery aliases in the main query

-- query1
with c1 as 
(select country.code,country.name COUNTRY, city.name CAPITAL, language, isofficial, percentage
 FROM country,city,countrylanguage
 WHERE country.code = countrylanguage.countrycode
 and country.capital = city.id
 and country.continent = 'Europe'),
c2 as
(select country.code,country.name COUNTRY, city.name CAPITAL, language, isofficial, percentage
 FROM country,city,countrylanguage
 WHERE country.code = countrylanguage.countrycode
 and country.capital = city.id
 and country.continent = 'Asia')

select * from
(select * from c1 where isofficial='True') c1,
(select * from c2 where percentage > 50) c2
where c1.percentage = c2.percentage order by c2.COUNTRY,c1.language;

-- query2 using same names as tables 
with country as 
(select country.code,country.name COUNTRY, city.name CAPITAL, language, isofficial, percentage
 FROM country,city,countrylanguage
 WHERE country.code = countrylanguage.countrycode
 and country.capital = city.id
 and country.continent = 'Europe'),
countrylanguage as
(select country.code,country.COUNTRY,country.language,country.isofficial,country.percentage
 FROM country,countrylanguage
 WHERE country.code = countrylanguage.countrycode
)

select * from
(select * from country where isofficial='True') country,
(select * from countrylanguage where percentage > 50) countrylanguage
where country.percentage = countrylanguage.percentage order by countrylanguage.COUNTRY,country.language LIMIT 40;
