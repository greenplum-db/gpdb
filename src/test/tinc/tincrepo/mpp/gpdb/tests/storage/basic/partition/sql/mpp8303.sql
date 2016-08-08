-- MPP-8303
-- Added by Johnny Soedomo
-- Script by Deepesh
-- This DDL is to create Source Table definition.
drop table mpp8303;
CREATE TABLE mpp8303 (
		rollup_eff_dt    date    ,
		ad_ntwrk_sk      smallint 
)
WITH (appendonly=true, compresslevel=1) DISTRIBUTED RANDOMLY PARTITION BY RANGE(rollup_eff_dt)
          SUBPARTITION BY RANGE(rollup_eff_dt) 
          (
		  

          PARTITION m_2010_01 START (date '2010-01-01') END (date '2010-01-02') WITH (appendonly=true, compresslevel=1) 
                  (
                  START (date '2010-01-01') END (date '2010-01-02') EVERY (INTERVAL '1 day') WITH (appendonly=true, compresslevel=1) 
                  )
          );



--  Insert 2 rows into mpp8303 parent

insert INTO mpp8303(rollup_eff_dt) values ('2010-01-01'),('2010-01-01');



SELECT COUNT(*) FROM  mpp8303;

SELECT COUNT(*) FROM  mpp8303_1_prt_m_2010_01 WHERE rollup_eff_dt = '2010-01-01';

SELECT COUNT(*) FROM  only mpp8303_1_prt_m_2010_01_2_prt_1;

insert INTO mpp8303_1_prt_m_2010_01(rollup_eff_dt) values ('2010-01-01'),('2010-01-01');

SELECT COUNT(*) FROM  only mpp8303_1_prt_m_2010_01; -- We should have ZERO records

SELECT COUNT(*) FROM  mpp8303_1_prt_m_2010_01_2_prt_1; -- We should have 4 records

SELECT COUNT(*) FROM  mpp8303; -- We should have 4 records

drop table mpp8303;
