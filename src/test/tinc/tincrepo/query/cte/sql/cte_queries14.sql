-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description cte tests from cdbfast 

-- negative cases where queries have duplicate names in CTEs

--query1 having duplicates without specifying a column list. Should error out. 
with capitals as 
(select country.code,id,city.name,city.countrycode as code from city,country 
 where city.countrycode = country.code AND city.id = country.capital) 

select * from capitals;

-- query2
with allofficiallanguages as 
(select countrylanguage.countrycode,city.countrycode,language from
 city,countrylanguage where countrylanguage.countrycode = city.countrycode and isofficial = 'True')
select * from allofficiallanguages;

-- query3 specifying duplicates explicitly in the column list
with capitals(code,id,name,code) as 
(select country.code,id,city.name,city.countrycode from city,country 
 where city.countrycode = country.code AND city.id = country.capital) 

select * from capitals;


