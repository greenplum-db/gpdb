-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description cte tests from cdbfast 

-- queries using column lists for CTEs

-- query 1 use column list despite having no duplicate names
with capitals("C","ID","CAP") as 
(select country.code,id,city.name from city,country 
 where city.countrycode = country.code AND city.id = country.capital),

lang_total(LC,CC,CLC) as
( select count(*) as lang_count,country.code,countrylanguage.countrycode
  from country join countrylanguage on (country.code=countrylanguage.countrycode and governmentform='Federal Republic')
  group by country.code,countrylanguage.countrycode order by country.code) 

select country.name,"C","ID","CAP",lang_total.lc
from capitals,lang_total,country
where capitals."C" = country.code
and country.code = lang_total.cc;


-- query 2 Check case sensitivity for quoted names in column list. This should error out
with "lang_total"("LC",CC,CLC) as
( select count(*) as lang_count,country.code,countrylanguage.countrycode
  from country join countrylanguage on (country.code=countrylanguage.countrycode and governmentform='Federal Republic')
  group by country.code,countrylanguage.countrycode order by country.code) 
select LC from lang_total;

-- query 3 use column list when there are duplicate names within the CTE
with capitals("CO_C","C_ID","CAPITAL",country) as 
(select country.code,id,city.name,country.name from city,country 
 where city.countrycode = country.code AND city.id = country.capital)

select "CO_C","C_ID","CAPITAL",COUNTRY from capitals where "CO_C"='SMR';


-- query4 use column list within another CTE
with capitals("CO_C","C_ID","CAPITAL",country) as 
(select country.code,id,city.name,country.name from city,country 
 where city.countrycode = country.code AND city.id = country.capital),
south_asian_capitals(A_CO_C,"C_ID",A_C_NAME,A_CO_NAME) as 
(select "CO_C","C_ID","CAPITAL",country from capitals,country where capitals."CO_C"=country.code and country.region = 'Southern and Central Asia')
select "a_co_c","C_ID",A_c_NaMe,"a_co_name" from south_asian_capitals order by A_CO_C;