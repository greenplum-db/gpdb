-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description cte tests from cdbfast 

-- queries with name collision between CTEs and tables in the database

-- query 1 using the same name for the CTE as a table. Main query should reference the CTE
with country as 
(select country.code,country.name COUNTRY, city.name CAPITAL, language, isofficial, percentage
 FROM country,city,countrylanguage
 WHERE country.code = countrylanguage.countrycode
 and country.capital = city.id
 and country.continent = 'Europe')

select * from
(select * from country where isofficial='True') e1,
(select * from country where percentage > 50) e2
where e1.code = e2.code order by e2.COUNTRY,e1.language LIMIT 20;

-- query 2 using multiple CTEs with same names as tables. 
with country as 
(select country.code,country.name COUNTRY, city.name CAPITAL, language, isofficial, percentage
 FROM country,city,countrylanguage
 WHERE country.code = countrylanguage.countrycode
 and country.capital = city.id
 and country.continent = 'Europe'),
countrylanguage(code1,country1,capital1,language1,isofficial1,percentage1,code2,country2,capital2,language2,isofficial2,percentage2) as
(
select * from
(select * from country where isofficial='True') e1,
(select * from country where percentage > 50) e2
where e1.code = e2.code order by e2.COUNTRY,e1.language
)
select code1,country1,capital1,language1,isofficial1,percentage1,country.COUNTRY from country,countrylanguage where country.code = countrylanguage.code1
and country.percentage = countrylanguage.percentage1
order by COUNTRY,percentage1 LIMIT 20;