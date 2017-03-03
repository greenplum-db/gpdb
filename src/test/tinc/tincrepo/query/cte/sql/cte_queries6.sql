-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @skip MPP-19261
-- @description cte tests from cdbfast 

-- queries using CTEs in initplans and main plan 

--query1 using CTE in the select list(initplan) . One CTE using another CTE in it's initplan
with gnpstats as 
(
select REG_GNP/REG_OLD_GNP as GNP_INDEX,region from
(select sum(gnpold) as REG_OLD_GNP,
 sum(gnp) as REG_GNP,region
 from country
 group by region) FOO
where (case when (REG_GNP/REG_OLD_GNP) > 0 then 1.2 * (REG_GNP/REG_OLD_GNP) else null end) between 0.5 and 1.5
order by region
),
gnp_index_compare as 
(
select (select max(GNP_INDEX) from gnpstats) M_GNP_IDX,(gnp/gnpold) as GNP_IDX,country.name from
country where  country.continent = 'Asia'
)
select (select min(GNP_IDX) from gnp_index_compare) MIN_COUNTRY_GNP_IDX, (select max(GNP_INDEX) from gnpstats) MAX_REG_GNP_IDX,city.name CAPITAL,country.name COUNTRY
 from city,country where city.id = country.capital and country.continent='Europe';


--query2 using the CTE in the where clause(initplan) of the main query. One CTE using another CTE in it's where clause as it's initplan
with diversecountries as
(select country.code,country.name,country.capital,d.CNT
 from country, 
 (select countrylanguage.countrycode,count(*) as CNT from countrylanguage group by countrycode
  HAVING count(*) > 6) d
 where d.countrycode = country.code and country.gnp > 100000),

notdiversecountries as
(select country.code,country.name,country.capital,d.CNT
 from country,
 (select countrylanguage.countrycode,count(*) as CNT from countrylanguage group by countrycode
  HAVING count(*) < 3) d
 where d.countrycode = country.code and country.gnp > 100000
 and d.CNT < (select max(CNT) from diversecountries))
select LANG_CNT,name from
(
select count(*) LANG_CNT,country.name,country.code from country,countrylanguage
where country.code = countrylanguage.countrycode and country.continent = 'North America'
group by country.name,country.code
) FOO
where FOO.LANG_CNT between (select min(CNT) from notdiversecountries) AND (select max(CNT) from diversecountries);



--query3 using CTE more than once in the same initplan and also more than once in the main query
with alleuropeanlanguages as 
(select country.code,country.name COUNTRY, city.name CAPITAL, language, isofficial, percentage
 FROM country,city,countrylanguage
 WHERE country.code = countrylanguage.countrycode
 and country.capital = city.id
 and country.continent = 'Europe'),
all_official_languages as
(
 select country.code,country.name,alleuropeanlanguages.language,alleuropeanlanguages.percentage
 from
 country,alleuropeanlanguages
 where country.code = alleuropeanlanguages.code and isofficial = 'True'
 and country.governmentform NOT  IN (select 'Commonwealth of the US' UNION ALL select 'Monarchy (Sultanate)' UNION ALL select 'Monarchy')
 and country.gnp > (select min(gnpold) from country where country.region = 'Western Europe')
)

SELECT avg(percentage),language
FROM
(
select country.name,city.name,percentage,language
from country,city,all_official_languages
where country.code = all_official_languages.code and country.capital = city.id and country.continent = 'Europe'

UNION ALL

select country.name,city.name,percentage,language
from country,city,all_official_languages
where country.code = all_official_languages.code and country.capital = city.id and country.continent = 'Asia'
) FOO 
group by FOO.language
HAVING avg(percentage) >=
(select avg(percentage) from 
( select percentage from all_official_languages,country where country.code = all_official_languages.code and country.region = 'British Islands'
  UNION
  select percentage from all_official_languages,country where country.code = all_official_languages.code and country.region = 'Western Europe'
) FOO )
order by FOO.language;  




