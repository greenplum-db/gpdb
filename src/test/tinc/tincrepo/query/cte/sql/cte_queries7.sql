-- @author prabhd 
-- @created 2012-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @skip MPP-19324
-- @description cte tests from cdbfast 

-- queries using deeply nested CTEs

-- query1 using a CTE at every subquery level 
select avg(population),CITY
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
               select s1.city,s1.population from size11_cities s1,size10_cities s2
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
) FOO0 group by city order by city;

--query 2 deeply nested CTEs with shared scans in the plan 
select avg(population),CITY
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
               select s1.city,s1.population from size11_cities s1,size10_cities s2
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
) FOO0 group by city order by city;

-- query 3 deeply nested CTEs using every CTE defined 
select avg(population) avg_p,CITY
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
) FOO0 group by city order by avg_p,city
LIMIT 20;

