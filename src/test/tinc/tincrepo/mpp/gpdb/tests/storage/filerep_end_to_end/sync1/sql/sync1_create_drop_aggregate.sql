-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE FUNCTION scube_accum(numeric, numeric) RETURNS 
numeric 
    AS 'select $1 + $2 * $2 * $2' 
    LANGUAGE SQL 
    IMMUTABLE 
    RETURNS NULL ON NULL INPUT; 

CREATE FUNCTION pre_accum(numeric, numeric) RETURNS 
numeric 
    AS 'select $1 + $2 * $2 * $2 * $2' 
    LANGUAGE SQL 
    IMMUTABLE 
    RETURNS NULL ON NULL INPUT; 


CREATE FUNCTION final_accum(numeric) RETURNS 
numeric 
    AS 'select $1 + $1 * $1' 
    LANGUAGE SQL 
    IMMUTABLE 
    RETURNS NULL ON NULL INPUT; 


CREATE AGGREGATE sync1_scube1(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 


CREATE AGGREGATE sync1_scube2(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 


CREATE AGGREGATE sync1_scube3(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 


CREATE AGGREGATE sync1_scube4(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 



CREATE AGGREGATE sync1_scube5(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 



CREATE AGGREGATE sync1_scube6(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 



CREATE AGGREGATE sync1_scube7(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 


CREATE AGGREGATE sync1_scube8(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 


DROP AGGREGATE sync1_scube1(numeric);
