-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description cte tests from cdbfast 

-- sanity tests with queries using CTEs in insert,update,delete and create

-- query 1 CTAS using CTE
create table bad_headofstates as 
(
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
order by OUTERMOST_FOO.region,bad_headofstates.headofstate LIMIT 40
);

select * from bad_headofstates order by region,headofstate,avg;

--query 2 insert using CTE
insert into bad_headofstates
(
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
order by OUTERMOST_FOO.region,bad_headofstates.headofstate LIMIT 40
);

select * from bad_headofstates order by region,headofstate,avg;

--query3 update using CTE
update bad_headofstates set region = cm.region FROM
(

with bad_headofstates as
(
 select country.code,country.name,country.headofstate,countrylanguage.language
 from
 country,countrylanguage
 where country.code = countrylanguage.countrycode and countrylanguage.isofficial=true
 and (country.gnp < country.gnpold or country.gnp < 3000)
)

select avg(OUTERMOST_FOO.AVG),OUTERMOST_FOO.region from (
select avg(population) AVG,region from
(
select FOO.*,bad_headofstates.headofstate,city.name
from
(select bad_headofstates.code,country.capital,country.region,country.population from
bad_headofstates,country where bad_headofstates.code = country.code) FOO, bad_headofstates,city
where FOO.code = bad_headofstates.code and FOO.capital = city.id) OUTER_FOO
group by region ) OUTERMOST_FOO,bad_headofstates,country
where country.code = bad_headofstates.code and country.region = OUTERMOST_FOO.region
and OUTERMOST_FOO.region = 'Central America'
group by OUTERMOST_FOO.region
order by OUTERMOST_FOO.region
) cm
where bad_headofstates.region = 'Caribbean';
 
select * from bad_headofstates order by region,headofstate,avg;

--query4 delete using CTE
delete from bad_headofstates USING
(
with bad_headofstates as
(
 select country.code,country.name,country.headofstate,countrylanguage.language
 from
 country,countrylanguage
 where country.code = countrylanguage.countrycode and countrylanguage.isofficial=true
 and (country.gnp < country.gnpold or country.gnp < 3000)
)

select avg(OUTERMOST_FOO.AVG),OUTERMOST_FOO.region from (
select avg(population) AVG,region from
(
select FOO.*,bad_headofstates.headofstate,city.name
from
(select bad_headofstates.code,country.capital,country.region,country.population from
bad_headofstates,country where bad_headofstates.code = country.code) FOO, bad_headofstates,city
where FOO.code = bad_headofstates.code and FOO.capital = city.id) OUTER_FOO
group by region ) OUTERMOST_FOO,bad_headofstates,country
where country.code = bad_headofstates.code and country.region = OUTERMOST_FOO.region
and OUTERMOST_FOO.region = 'Central America'
group by OUTERMOST_FOO.region
order by OUTERMOST_FOO.region
) as  cm
where bad_headofstates.region = cm.region;

select * from bad_headofstates order by region,headofstate,avg;

-- delete using CTE in its subplan
delete from bad_headofstates where bad_headofstates.avg NOT IN
(
with bad_headofstates as
(
 select country.code,country.name,country.headofstate,countrylanguage.language
 from
 country,countrylanguage
 where country.code = countrylanguage.countrycode and countrylanguage.isofficial=true
 and (country.gnp < country.gnpold or country.gnp < 3000)
)

select OUTERMOST_FOO.AVG from (
select avg(population) AVG,region from
(
select FOO.*,bad_headofstates.headofstate,city.name
from
(select bad_headofstates.code,country.capital,country.region,country.population from
bad_headofstates,country where bad_headofstates.code = country.code) FOO, bad_headofstates,city
where FOO.code = bad_headofstates.code and FOO.capital = city.id) OUTER_FOO
group by region ) OUTERMOST_FOO,bad_headofstates,country
where country.code = bad_headofstates.code and country.region = OUTERMOST_FOO.region
and OUTERMOST_FOO.region = 'Australia and New Zealand' or OUTERMOST_FOO.region = 'Eastern Asia'
order by OUTERMOST_FOO.region
);

select * from bad_headofstates order by region,headofstate,avg;

drop table bad_headofstates;
