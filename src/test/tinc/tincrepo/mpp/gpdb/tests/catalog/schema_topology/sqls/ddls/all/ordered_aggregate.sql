-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description  Ordered aggregates

-- Create schema and set to it   
   Create Schema oagg;
   SET search_path to oagg;

-- Create quantity table
   Drop table IF EXISTS sch_quantity;
   create table sch_quantity ( prod_key integer, qty integer, price integer, product character(3));
-- inserting values into sch_quantity table

   insert into sch_quantity values (1,100, 50, 'p1');
   insert into sch_quantity values (2,200, 100, 'p2');
   insert into sch_quantity values (3,300, 200, 'p3');
   insert into sch_quantity values (4,400, 35, 'p4');
   insert into sch_quantity values (5,500, 40, 'p5');
   insert into sch_quantity values (1,150, 50, 'p1');
   insert into sch_quantity values (2,50, 100, 'p2');
   insert into sch_quantity values (3,150, 200, 'p3');
   insert into sch_quantity values (4,200, 35, 'p4');
   insert into sch_quantity values (5,300, 40, 'p5');

-- Create a new ordered aggregate with initial condition and finalfunc
   CREATE ORDERED AGGREGATE sch_array_accum_final (anyelement)
   (
      sfunc = array_append,
      stype = anyarray,
      finalfunc = array_out,
      initcond = '{}'
   );

-- Using newly created ordered aggregate array_accum_final
   select prod_key, sch_array_accum_final(qty order by prod_key,qty) from sch_quantity group by prod_key having prod_key < 5 order by prod_key;

-- rename ordered aggregate
alter aggregate sch_array_accum_final (anyelement) rename to sch_array_accum_final_new;

-- check ordered aggregate
\df sch_array_accum_final_new;

-- drop ordered aggregate
drop aggregate if exists sch_array_accum_final_new(anyelement);

-- Drop Scehma
   Drop schema oagg CASCADE;
   SET search_path to public;
