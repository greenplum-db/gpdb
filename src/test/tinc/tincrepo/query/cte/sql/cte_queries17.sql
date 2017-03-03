-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description cte tests from cdbfast 

-- negative cases with mismatching column list provided for CTEs

--query1
with capitals(code,id) as 
(select country.code,id,city.name from city,country 
 where city.countrycode = country.code AND city.id = country.capital)

select * from capitals;

--query 2
with lang_total(lang_count,code,countrycode,name) as
( select count(*) as lang_count,country.code,countrylanguage.countrycode
  from country join countrylanguage on (country.code=countrylanguage.countrycode and governmentform='Federal Republic')
  group by country.code,countrylanguage.countrycode order by country.code)
select * from lang_total;
