-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE AGGREGATE resync_scube1(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 


CREATE AGGREGATE resync_scube2(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 


CREATE AGGREGATE resync_scube3(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 


DROP AGGREGATE sync1_scube6(numeric);
DROP AGGREGATE ck_sync1_scube5(numeric);
DROP AGGREGATE ct_scube3(numeric);
DROP AGGREGATE resync_scube1(numeric);
