-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @skip MPP-19324
-- @description cte tests from cdbfast 

-- queries with views using CTEs
-- view1 with multiple CTEs being used multiple times

create view view_with_shared_scans as
(
with longlivingregions as 
(
select FOO.*,count(distinct language) as "lang_count"
from(
     select
       sum(population) as "REGION_POP",
       sum(gnp) as "REGION_GNP",
       avg(lifeexpectancy) as "REGION_LIFETIME",region
     from
      country
     group by region
    ) FOO,countrylanguage,country
where
   country.code = countrylanguage.countrycode
   and FOO.region = country.region
group by
FOO.region,foo."REGION_POP",foo."REGION_GNP",foo."REGION_LIFETIME"),

denseregions as 
(
select FOO.*,count(distinct language) as "lang_count",
       sum(surfacearea) as "REGION_SURFACE_AREA"
from(
     select
       sum(population) as "REGION_POP",
       sum(gnp) as "REGION_GNP",
       region
     from
      country
     group by region
    ) FOO,countrylanguage,country
where
   country.code = countrylanguage.countrycode
   and FOO.region = country.region
   and FOO."REGION_POP" != 0
group by
FOO.region,foo."REGION_POP",foo."REGION_GNP"
order by sum(surfacearea)/foo."REGION_POP" desc),

allcountrystats as 
( select country.code,country.name,count(distinct city.id) CITY_CNT,
  count(distinct countrylanguage.language) LANG_CNT
  from country,city,countrylanguage
  where country.code = city.countrycode
  and country.code = countrylanguage.countrycode
  group by country.code,country.name
)

select allcountrystats.CITY_CNT,allcountrystats.LANG_CNT,allcountrystats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountrystats,country
where longlivingregions.region = denseregions.region and allcountrystats.code = country.code and country.region = longlivingregions.region
and country.indepyear between 1800 and 1850

UNION ALL

select allcountrystats.CITY_CNT,allcountrystats.LANG_CNT,allcountrystats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountrystats,country
where longlivingregions.region = denseregions.region and allcountrystats.code = country.code and country.region = longlivingregions.region
and country.indepyear between 1850 and 1900

UNION ALL

select allcountrystats.CITY_CNT,allcountrystats.LANG_CNT,allcountrystats.name,
       "REGION_SURFACE_AREA","REGION_LIFETIME",longlivingregions."REGION_POP",longlivingregions.lang_count,longlivingregions."REGION_GNP",longlivingregions.region
from longlivingregions,denseregions,allcountrystats,country
where longlivingregions.region = denseregions.region and allcountrystats.code = country.code and country.region = longlivingregions.region
and country.indepyear > 1900
);

\d view_with_shared_scans;

select city_cnt,lang_cnt,name,region from view_with_shared_scans order by name LIMIT 50;

select city_cnt,lang_cnt,name,"REGION_POP","REGION_GNP",region from view_with_shared_scans where region = 'Eastern Europe';

drop view view_with_shared_scans;


-- view with deeply nested CTEs
create view view_with_deep_nested_CTE as
(select avg(population),CITY
from
(
with size0_cities(CITY,POPULATION) as (select city.name,city.population from city where city.population >= 100)

select CITY,POPULATION from 
(
  with size1_cities(CITY,POPULATION) as ( select city,population from size0_cities where population >= 1000 )
  select CITY,POPULATION from 
  (
     with size2_cities(CITY,POPULATION) as (select city,population from size1_cities where population >= 10000)
     select CITY,POPULATION from
     (
       with size3_cities as (select city,population from  size2_cities where population >= 20000)
       select CITY,POPULATION from
       (
        with size4_cities as (select city,population from  size3_cities where population >= 50000)
        select CITY,POPULATION from
        (
         with size5_cities as (select city,population from  size4_cities where population >= 80000)
         select CITY,POPULATION from
         (
          with size6_cities as (select city,population from  size5_cities where population >= 150000)
          select CITY,POPULATION from
          (
           with size7_cities as (select city,population from  size6_cities where population >= 200000)
           select CITY,POPULATION from
           (
            with size8_cities as (select city,population from  size7_cities where population >= 250000)
            select CITY,POPULATION from
            (
             with size9_cities as (select city,population from  size8_cities where population >= 300000)
             select city,population from
             (
              with size10_cities as (select city,population from  size9_cities where population >= 6500000)
              select CITY,POPULATION from
              (
               with size11_cities as (select city,population from  size10_cities where population >= 7000000)
               select s1.city,s1.population from size11_cities s1,size1_cities s2
               UNION
               select s1.city,s1.population from size10_cities s1,size2_cities s2
               UNION
               select s1.city,s1.population from size9_cities s1,size3_cities s2
               UNION
               select s1.city,s1.population from size8_cities s1,size4_cities s2
               UNION
               select s1.city,s1.population from size7_cities s1,size5_cities s2
               UNION
               select s1.city,s1.population from size6_cities s1,size6_cities s2
              ) FOO11
             ) FOO10
            ) FOO9
           ) FOO8
          ) FOO7
         ) FOO6
        ) FOO5
       ) FOO4
     )FOO3
  ) FOO2
) FOO1 order by city
) FOO0 group by city order by city
);

\d view_with_deep_nested_CTE;

select * from view_with_deep_nested_CTE order by city LIMIT 20;

select * from view_with_deep_nested_CTE where avg > 2000000 order by city LIMIT 20;

drop view view_with_deep_nested_CTE;



