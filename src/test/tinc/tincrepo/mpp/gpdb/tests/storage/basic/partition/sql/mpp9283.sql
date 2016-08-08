-- Cannot create partition with OIDS=true
CREATE TABLE mpp9283
(
  mp_sys_no bigint NOT NULL,
  mp_as_of_date date NOT NULL,
  mp_status character varying(50),
  mp_cumunav_incp_divreinv_sor numeric(12,4),
  mp_great_mnth_rtn_incep_sor numeric(12,4),
  mp_who_created character varying(30),
  mp_when_created timestamp without time zone,
  mp_who_updated character varying(30),
  mp_when_updated timestamp without time zone,
  mp_iss_sys_no bigint NOT NULL,
  mp_geo_sys_no bigint,
  mp_per_sys_no bigint,
  mp_cur_sys_no bigint,
  mp_prfm_sys_no bigint NOT NULL,
  mp_fba_sys_no bigint,
  mp_comp_gross_mkt_2yr_sor numeric(12,4),
  mp_comp_gross_mkt_2yr_sors character varying(30),
  mp_comp_gross_mkt_2yr_mod numeric(12,4),
   mp_comp_gross_mkt_ytd_mods character varying(30),
   CONSTRAINT mpp9283_pkey1 PRIMARY KEY (mp_sys_no, mp_iss_sys_no, mp_as_of_date)
)
WITH (
  OIDS=TRUE
)
DISTRIBUTED BY (mp_sys_no, mp_iss_sys_no, mp_as_of_date)
PARTITION BY RANGE(mp_as_of_date)
          (
          PARTITION "2009q1" START ('2009-01-01'::date) END ('2009-03-31'::date) INCLUSIVE,
          PARTITION "2009q2" START ('2009-04-01'::date) END ('2009-06-30'::date) INCLUSIVE,
          PARTITION "2009q3" START ('2009-07-01'::date) END ('2009-09-30'::date) INCLUSIVE,
          PARTITION "2009q4" START ('2009-10-01'::date) END ('2009-12-31'::date) INCLUSIVE          )
;
