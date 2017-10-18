--start_ignore
drop schema mpp6979_parent cascade;
drop schema mpp6979_child cascade;
create schema mpp6979_parent;
create schema mpp6979_child;
--end_ignore

CREATE TABLE mpp6979_parent.ad_impsn_fact (
    ad_impsn_dttm timestamp without time zone NOT NULL,
    user_id integer NOT NULL,
    site_sk smallint DEFAULT -1 NOT NULL,
    user_hash_key smallint DEFAULT 0 NOT NULL,
    advtsr_camp_id integer DEFAULT -1 NOT NULL,
    advtsr_crtv_id integer DEFAULT -1 NOT NULL,
    publ_loctn_id integer DEFAULT -1 NOT NULL,
    publ_camp_id integer DEFAULT -1 NOT NULL,
    ad_ntwrk_sk smallint DEFAULT -1 NOT NULL,
    dstn_user_id integer DEFAULT -1 NOT NULL,
    crtv_postn_id integer DEFAULT -1 NOT NULL,
    ip_isp_sk smallint DEFAULT -1 NOT NULL,
    ip_geo_sk integer DEFAULT -1 NOT NULL,
    user_ip integer DEFAULT -1 NOT NULL
) distributed by (user_hash_key);


CREATE TABLE mpp6979_child.ad_impsn_fact2_c_d_1_m_1_1610396_month (CONSTRAINT ad_impsn_fact2_c_d_1_m_1_1610396_month_ad_impsn_dttm_check CHECK (((ad_impsn_dttm >= '2007-10-01 00:00:00'::timestamp without time zone) AND (ad_impsn_dttm <
'2007-11-01 00:00:00'::timestamp without time zone)))
)
INHERITS (mpp6979_parent.ad_impsn_fact) distributed by (user_hash_key);

CREATE RULE rule_ad_impsn_fact2_c_d_1_m_1_1610396_month AS ON INSERT TO mpp6979_parent.ad_impsn_fact WHERE ((new.ad_impsn_dttm >= '2007-10-01 00:00:00'::timestamp without time zone) AND (new.ad_impsn_dttm < '2007-11-01 00:00:00'::timestamp without time zone)) DO INSTEAD INSERT INTO mpp6979_child.ad_impsn_fact2_c_d_1_m_1_1610396_month (ad_impsn_dttm, user_id, site_sk, user_hash_key, advtsr_camp_id, advtsr_crtv_id, publ_loctn_id, publ_camp_id, ad_ntwrk_sk, dstn_user_id, crtv_postn_id, ip_isp_sk, ip_geo_sk, user_ip) VALUES (new.ad_impsn_dttm, new.user_id, new.site_sk, new.user_hash_key, new.advtsr_camp_id, new.advtsr_crtv_id, new.publ_loctn_id, new.publ_camp_id, new.ad_ntwrk_sk, new.dstn_user_id, new.crtv_postn_id, new.ip_isp_sk, new.ip_geo_sk, new.user_ip);

CREATE TABLE mpp6979_parent.ad_impsn_fact_XnewX (LIKE mpp6979_parent.ad_impsn_fact)
PARTITION BY RANGE (ad_impsn_dttm)
(
  -- p1 derived from: (mpp6979_child.ad_impsn_fact2_c_d_1_m_1_1610396_month)
  PARTITION p1  START ('2007-10-01 00:00:00'::timestamp without time zone) INCLUSIVE END ('2007-11-01 00:00:00'::timestamp without time zone) EXCLUSIVE
);

-- EXCHANGE DATA TO PARTITIONED TABLES

ALTER TABLE mpp6979_child.ad_impsn_fact2_c_d_1_m_1_1610396_month  NO INHERIT mpp6979_parent.ad_impsn_fact ;
ALTER TABLE mpp6979_child.ad_impsn_fact2_c_d_1_m_1_1610396_month  DROP CONSTRAINT ad_impsn_fact2_c_d_1_m_1_1610396_month_ad_impsn_dttm_check ;
ALTER TABLE mpp6979_parent.ad_impsn_fact_XnewX EXCHANGE PARTITION p1 WITH TABLE mpp6979_child.ad_impsn_fact2_c_d_1_m_1_1610396_month;
ALTER TABLE mpp6979_child.ad_impsn_fact2_c_d_1_m_1_1610396_month  INHERIT mpp6979_parent.ad_impsn_fact ;

select * from pg_partitions where schemaname like 'mpp6979%';
select schemaname, tablename from pg_tables where schemaname like 'mpp6979%' order by 1, 2;
