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


CREATE AGGREGATE scube(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 , 
          SORTOP = >); 


CREATE AGGREGATE old_scube ( 
    BASETYPE = numeric,
    SFUNC = scube_accum, 
    STYPE = numeric, 
       FINALFUNC =final_accum,
    INITCOND = 0 , 
          SORTOP = >); 

CREATE TABLE tbl_aggregate (i int);
