-- MPP-7734
-- Johnny Soedomo
--start_ignore
drop table if exists smntg_odr;
drop table if exists wiat_ola_tree;
drop table if exists rtng_mtch;
--end_ignore

CREATE TABLE smntg_odr (
    ads_smntg_odr_id numeric,
    trd_dt timestamp without time zone,
    entry_dt timestamp without time zone,
    msg_ctgry_cd character varying(1),
    msg_type_cd character varying(1),
    src_cd character varying(3),
    sssn_cd character varying(1),
    rtrsm_cd character varying(2),
    msg_src_id character varying(2),
    msg_seq_nb numeric(10,0),
    trans_cd character varying(6),
    odr_updt_tm character varying(12),
    odr_rfrnc_nb character varying(12),
    oe_brnch_seq_id character varying(8),
    odr_entry_dt timestamp without time zone,
    odr_entry_tm character varying(12),
    odr_entry_src_cd character varying(1),
    odr_st character varying(1),
    odr_ctgry_cd character varying(1),
    prnt_odr_fl character varying(1),
    oe_mp_id character varying(5),
    ovvrd_fl character varying(1),
    issue_sym_id character varying(14),
    side_cd character varying(1),
    shrt_sale_cd character varying(1),
    entrd_pr numeric(18,8),
    mkt_odr_fl character varying(1),
    rndd_odr_pr numeric(18,8),
    odr_entry_qt numeric(11,0),
    odr_qt numeric(11,0),
    pndng_cncl_qt numeric(11,0),
    odr_rsrv_size_qt numeric(11,0),
    odr_rfrsh_size_qt numeric(11,0),
    oe_cpcty_cd character varying(1),
    atrbl_odr_fl character varying(1),
    odr_entry_i1i2_id character varying(4),
    tif_cd character varying(5),
    oe_gvp_firm_id character varying(5),
    clrg_nb character varying(4),
    altrn_clrg_nb character varying(4),
    prtzn_algrm_cd character varying(1),
    afpii_fl character varying(1),
    user_odr_id character varying(32),
    odr_rcvd_dt timestamp without time zone,
    dni_fl character varying(1),
    dnr_fl character varying(1),
    bnchd_odr_fl character varying(1),
    odr_rank_dt timestamp without time zone,
    odr_rank_tm character varying(12),
    anti_ntlzn_fl character varying(1),
    prfcd_mp_id character varying(5),
    dlvry_qt numeric(11,0),
    exctn_qt numeric(11,0),
    systm_quote_fl character varying(1),
    ats_cd character varying(1),
    trans_i1i2_id character varying(4),
    trans_src_cd character varying(1),
    trans_soes_fl character varying(1),
    oe_prmry_mp_id character varying(4),
    oe_prmry_mpid_nb numeric(10,0),
    afpii_pr numeric(18,8),
    host_updt_tm character varying(12),
    cncl_qt numeric(11,0),
    peg_type_cd character varying(1),
    peg_offst_pr numeric(18,8),
    peg_cap_pr numeric(18,8),
    dcrny_odr_fl character varying(1),
    dcrny_offst_pr numeric(18,8),
    early_late_cd character varying(1),
    rtng_seq_nb character varying(13),
    route_cnt numeric(5,0),
    rtng_fl character varying(5),
    dlvry_cnt numeric(5,0),
    mp_mtch_elgby_cd character varying(1),
    dstnt_type_cd character varying(1),
    nbbo_bid_pr numeric(18,8),
    nbbo_ask_pr numeric(18,8),
    hs_msg_type_cd character varying(1),
    hs_prnt_odr_fl character varying(1),
    hs_odr_entry_dt timestamp without time zone,
    hs_odr_entry_tm character varying(12),
    hs_odr_qt numeric(11,0),
    hs_exctn_qt numeric(11,0),
    hs_dlvry_qt numeric(11,0),
    exctn_rfrnc_nb character varying(12),
    opng_cross_elgbl_fl character varying(1),
    clsg_cross_elgbl_fl character varying(1),
    core_link_id character varying(15),
    core_seq_nb numeric(12,0),
    crash_tm character varying(12),
    core_updt_tm character varying(12),
    core_actn_cd character varying(1),
    iso_fl character varying(1),
    cross_fl character varying(1),
    rplcd_odr_rfrnc_nb character varying(12)
) distributed by (ads_smntg_odr_id) PARTITION BY RANGE(trd_dt)
          (
          PARTITION d20060102 START ('2006-01-02 00:00:00'::timestamp without time zone) END ('2006-01-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20061002 START ('2006-10-02 00:00:00'::timestamp without time zone) END ('2006-10-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20061003 START ('2006-10-03 00:00:00'::timestamp without time zone) END ('2006-10-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20061004 START ('2006-10-04 00:00:00'::timestamp without time zone) END ('2006-10-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20061005 START ('2006-10-05 00:00:00'::timestamp without time zone) END ('2006-10-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20061006 START ('2006-10-06 00:00:00'::timestamp without time zone) END ('2006-10-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20061009 START ('2006-10-09 00:00:00'::timestamp without time zone) END ('2006-10-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20061010 START ('2006-10-10 00:00:00'::timestamp without time zone) END ('2006-10-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20061011 START ('2006-10-11 00:00:00'::timestamp without time zone) END ('2006-10-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20061012 START ('2006-10-12 00:00:00'::timestamp without time zone) END ('2006-10-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20061013 START ('2006-10-13 00:00:00'::timestamp without time zone) END ('2006-10-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20061016 START ('2006-10-16 00:00:00'::timestamp without time zone) END ('2006-10-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20061017 START ('2006-10-17 00:00:00'::timestamp without time zone) END ('2006-10-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20061018 START ('2006-10-18 00:00:00'::timestamp without time zone) END ('2006-10-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20061019 START ('2006-10-19 00:00:00'::timestamp without time zone) END ('2006-10-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20061020 START ('2006-10-20 00:00:00'::timestamp without time zone) END ('2006-10-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20061023 START ('2006-10-23 00:00:00'::timestamp without time zone) END ('2006-10-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20061024 START ('2006-10-24 00:00:00'::timestamp without time zone) END ('2006-10-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20061025 START ('2006-10-25 00:00:00'::timestamp without time zone) END ('2006-10-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20061026 START ('2006-10-26 00:00:00'::timestamp without time zone) END ('2006-10-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20061027 START ('2006-10-27 00:00:00'::timestamp without time zone) END ('2006-10-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20061030 START ('2006-10-30 00:00:00'::timestamp without time zone) END ('2006-10-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20061031 START ('2006-10-31 00:00:00'::timestamp without time zone) END ('2006-11-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20061101 START ('2006-11-01 00:00:00'::timestamp without time zone) END ('2006-11-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20061102 START ('2006-11-02 00:00:00'::timestamp without time zone) END ('2006-11-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20061103 START ('2006-11-03 00:00:00'::timestamp without time zone) END ('2006-11-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20061106 START ('2006-11-06 00:00:00'::timestamp without time zone) END ('2006-11-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20061107 START ('2006-11-07 00:00:00'::timestamp without time zone) END ('2006-11-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20061108 START ('2006-11-08 00:00:00'::timestamp without time zone) END ('2006-11-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20061109 START ('2006-11-09 00:00:00'::timestamp without time zone) END ('2006-11-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20061110 START ('2006-11-10 00:00:00'::timestamp without time zone) END ('2006-11-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20061113 START ('2006-11-13 00:00:00'::timestamp without time zone) END ('2006-11-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20061114 START ('2006-11-14 00:00:00'::timestamp without time zone) END ('2006-11-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20061115 START ('2006-11-15 00:00:00'::timestamp without time zone) END ('2006-11-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20061116 START ('2006-11-16 00:00:00'::timestamp without time zone) END ('2006-11-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20061117 START ('2006-11-17 00:00:00'::timestamp without time zone) END ('2006-11-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20061120 START ('2006-11-20 00:00:00'::timestamp without time zone) END ('2006-11-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20061121 START ('2006-11-21 00:00:00'::timestamp without time zone) END ('2006-11-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20061122 START ('2006-11-22 00:00:00'::timestamp without time zone) END ('2006-11-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20061124 START ('2006-11-24 00:00:00'::timestamp without time zone) END ('2006-11-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20061127 START ('2006-11-27 00:00:00'::timestamp without time zone) END ('2006-11-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20061128 START ('2006-11-28 00:00:00'::timestamp without time zone) END ('2006-11-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20061129 START ('2006-11-29 00:00:00'::timestamp without time zone) END ('2006-11-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20061130 START ('2006-11-30 00:00:00'::timestamp without time zone) END ('2006-12-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20061201 START ('2006-12-01 00:00:00'::timestamp without time zone) END ('2006-12-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20061204 START ('2006-12-04 00:00:00'::timestamp without time zone) END ('2006-12-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20061205 START ('2006-12-05 00:00:00'::timestamp without time zone) END ('2006-12-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20061206 START ('2006-12-06 00:00:00'::timestamp without time zone) END ('2006-12-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20061207 START ('2006-12-07 00:00:00'::timestamp without time zone) END ('2006-12-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20061208 START ('2006-12-08 00:00:00'::timestamp without time zone) END ('2006-12-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20061211 START ('2006-12-11 00:00:00'::timestamp without time zone) END ('2006-12-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20061212 START ('2006-12-12 00:00:00'::timestamp without time zone) END ('2006-12-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20061213 START ('2006-12-13 00:00:00'::timestamp without time zone) END ('2006-12-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20061214 START ('2006-12-14 00:00:00'::timestamp without time zone) END ('2006-12-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20061215 START ('2006-12-15 00:00:00'::timestamp without time zone) END ('2006-12-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20061218 START ('2006-12-18 00:00:00'::timestamp without time zone) END ('2006-12-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20061219 START ('2006-12-19 00:00:00'::timestamp without time zone) END ('2006-12-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20061220 START ('2006-12-20 00:00:00'::timestamp without time zone) END ('2006-12-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20061221 START ('2006-12-21 00:00:00'::timestamp without time zone) END ('2006-12-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20061222 START ('2006-12-22 00:00:00'::timestamp without time zone) END ('2006-12-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20061226 START ('2006-12-26 00:00:00'::timestamp without time zone) END ('2006-12-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20061227 START ('2006-12-27 00:00:00'::timestamp without time zone) END ('2006-12-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20061228 START ('2006-12-28 00:00:00'::timestamp without time zone) END ('2006-12-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20061229 START ('2006-12-29 00:00:00'::timestamp without time zone) END ('2007-01-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20070103 START ('2007-01-03 00:00:00'::timestamp without time zone) END ('2007-01-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20070104 START ('2007-01-04 00:00:00'::timestamp without time zone) END ('2007-01-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070105 START ('2007-01-05 00:00:00'::timestamp without time zone) END ('2007-01-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070108 START ('2007-01-08 00:00:00'::timestamp without time zone) END ('2007-01-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070109 START ('2007-01-09 00:00:00'::timestamp without time zone) END ('2007-01-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070110 START ('2007-01-10 00:00:00'::timestamp without time zone) END ('2007-01-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070111 START ('2007-01-11 00:00:00'::timestamp without time zone) END ('2007-01-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070112 START ('2007-01-12 00:00:00'::timestamp without time zone) END ('2007-01-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070116 START ('2007-01-16 00:00:00'::timestamp without time zone) END ('2007-01-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070117 START ('2007-01-17 00:00:00'::timestamp without time zone) END ('2007-01-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070118 START ('2007-01-18 00:00:00'::timestamp without time zone) END ('2007-01-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070119 START ('2007-01-19 00:00:00'::timestamp without time zone) END ('2007-01-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070122 START ('2007-01-22 00:00:00'::timestamp without time zone) END ('2007-01-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070123 START ('2007-01-23 00:00:00'::timestamp without time zone) END ('2007-01-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070124 START ('2007-01-24 00:00:00'::timestamp without time zone) END ('2007-01-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070125 START ('2007-01-25 00:00:00'::timestamp without time zone) END ('2007-01-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070126 START ('2007-01-26 00:00:00'::timestamp without time zone) END ('2007-01-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20070129 START ('2007-01-29 00:00:00'::timestamp without time zone) END ('2007-01-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070130 START ('2007-01-30 00:00:00'::timestamp without time zone) END ('2007-01-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20070131 START ('2007-01-31 00:00:00'::timestamp without time zone) END ('2007-02-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20070201 START ('2007-02-01 00:00:00'::timestamp without time zone) END ('2007-02-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070202 START ('2007-02-02 00:00:00'::timestamp without time zone) END ('2007-02-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070205 START ('2007-02-05 00:00:00'::timestamp without time zone) END ('2007-02-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070206 START ('2007-02-06 00:00:00'::timestamp without time zone) END ('2007-02-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070207 START ('2007-02-07 00:00:00'::timestamp without time zone) END ('2007-02-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070208 START ('2007-02-08 00:00:00'::timestamp without time zone) END ('2007-02-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070209 START ('2007-02-09 00:00:00'::timestamp without time zone) END ('2007-02-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070212 START ('2007-02-12 00:00:00'::timestamp without time zone) END ('2007-02-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070213 START ('2007-02-13 00:00:00'::timestamp without time zone) END ('2007-02-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070214 START ('2007-02-14 00:00:00'::timestamp without time zone) END ('2007-02-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20070215 START ('2007-02-15 00:00:00'::timestamp without time zone) END ('2007-02-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070216 START ('2007-02-16 00:00:00'::timestamp without time zone) END ('2007-02-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070220 START ('2007-02-20 00:00:00'::timestamp without time zone) END ('2007-02-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070221 START ('2007-02-21 00:00:00'::timestamp without time zone) END ('2007-02-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070222 START ('2007-02-22 00:00:00'::timestamp without time zone) END ('2007-02-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070223 START ('2007-02-23 00:00:00'::timestamp without time zone) END ('2007-02-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070226 START ('2007-02-26 00:00:00'::timestamp without time zone) END ('2007-02-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070227 START ('2007-02-27 00:00:00'::timestamp without time zone) END ('2007-02-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20070228 START ('2007-02-28 00:00:00'::timestamp without time zone) END ('2007-03-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20070301 START ('2007-03-01 00:00:00'::timestamp without time zone) END ('2007-03-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070302 START ('2007-03-02 00:00:00'::timestamp without time zone) END ('2007-03-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070305 START ('2007-03-05 00:00:00'::timestamp without time zone) END ('2007-03-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070306 START ('2007-03-06 00:00:00'::timestamp without time zone) END ('2007-03-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070307 START ('2007-03-07 00:00:00'::timestamp without time zone) END ('2007-03-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070308 START ('2007-03-08 00:00:00'::timestamp without time zone) END ('2007-03-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070309 START ('2007-03-09 00:00:00'::timestamp without time zone) END ('2007-03-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070312 START ('2007-03-12 00:00:00'::timestamp without time zone) END ('2007-03-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070313 START ('2007-03-13 00:00:00'::timestamp without time zone) END ('2007-03-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070314 START ('2007-03-14 00:00:00'::timestamp without time zone) END ('2007-03-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20070315 START ('2007-03-15 00:00:00'::timestamp without time zone) END ('2007-03-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070316 START ('2007-03-16 00:00:00'::timestamp without time zone) END ('2007-03-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070319 START ('2007-03-19 00:00:00'::timestamp without time zone) END ('2007-03-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070320 START ('2007-03-20 00:00:00'::timestamp without time zone) END ('2007-03-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070321 START ('2007-03-21 00:00:00'::timestamp without time zone) END ('2007-03-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070322 START ('2007-03-22 00:00:00'::timestamp without time zone) END ('2007-03-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070323 START ('2007-03-23 00:00:00'::timestamp without time zone) END ('2007-03-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070326 START ('2007-03-26 00:00:00'::timestamp without time zone) END ('2007-03-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070327 START ('2007-03-27 00:00:00'::timestamp without time zone) END ('2007-03-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20070328 START ('2007-03-28 00:00:00'::timestamp without time zone) END ('2007-03-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20070329 START ('2007-03-29 00:00:00'::timestamp without time zone) END ('2007-03-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070330 START ('2007-03-30 00:00:00'::timestamp without time zone) END ('2007-04-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070402 START ('2007-04-02 00:00:00'::timestamp without time zone) END ('2007-04-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20070403 START ('2007-04-03 00:00:00'::timestamp without time zone) END ('2007-04-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20070404 START ('2007-04-04 00:00:00'::timestamp without time zone) END ('2007-04-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070405 START ('2007-04-05 00:00:00'::timestamp without time zone) END ('2007-04-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070409 START ('2007-04-09 00:00:00'::timestamp without time zone) END ('2007-04-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070410 START ('2007-04-10 00:00:00'::timestamp without time zone) END ('2007-04-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070411 START ('2007-04-11 00:00:00'::timestamp without time zone) END ('2007-04-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070412 START ('2007-04-12 00:00:00'::timestamp without time zone) END ('2007-04-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070413 START ('2007-04-13 00:00:00'::timestamp without time zone) END ('2007-04-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070416 START ('2007-04-16 00:00:00'::timestamp without time zone) END ('2007-04-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070417 START ('2007-04-17 00:00:00'::timestamp without time zone) END ('2007-04-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070418 START ('2007-04-18 00:00:00'::timestamp without time zone) END ('2007-04-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070419 START ('2007-04-19 00:00:00'::timestamp without time zone) END ('2007-04-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070420 START ('2007-04-20 00:00:00'::timestamp without time zone) END ('2007-04-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070423 START ('2007-04-23 00:00:00'::timestamp without time zone) END ('2007-04-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070424 START ('2007-04-24 00:00:00'::timestamp without time zone) END ('2007-04-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070425 START ('2007-04-25 00:00:00'::timestamp without time zone) END ('2007-04-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070426 START ('2007-04-26 00:00:00'::timestamp without time zone) END ('2007-04-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070427 START ('2007-04-27 00:00:00'::timestamp without time zone) END ('2007-04-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070430 START ('2007-04-30 00:00:00'::timestamp without time zone) END ('2007-05-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20070501 START ('2007-05-01 00:00:00'::timestamp without time zone) END ('2007-05-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070502 START ('2007-05-02 00:00:00'::timestamp without time zone) END ('2007-05-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20070503 START ('2007-05-03 00:00:00'::timestamp without time zone) END ('2007-05-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20070504 START ('2007-05-04 00:00:00'::timestamp without time zone) END ('2007-05-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070507 START ('2007-05-07 00:00:00'::timestamp without time zone) END ('2007-05-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070508 START ('2007-05-08 00:00:00'::timestamp without time zone) END ('2007-05-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070509 START ('2007-05-09 00:00:00'::timestamp without time zone) END ('2007-05-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070510 START ('2007-05-10 00:00:00'::timestamp without time zone) END ('2007-05-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070511 START ('2007-05-11 00:00:00'::timestamp without time zone) END ('2007-05-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070514 START ('2007-05-14 00:00:00'::timestamp without time zone) END ('2007-05-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20070515 START ('2007-05-15 00:00:00'::timestamp without time zone) END ('2007-05-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070516 START ('2007-05-16 00:00:00'::timestamp without time zone) END ('2007-05-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070517 START ('2007-05-17 00:00:00'::timestamp without time zone) END ('2007-05-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070518 START ('2007-05-18 00:00:00'::timestamp without time zone) END ('2007-05-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070521 START ('2007-05-21 00:00:00'::timestamp without time zone) END ('2007-05-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070522 START ('2007-05-22 00:00:00'::timestamp without time zone) END ('2007-05-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070523 START ('2007-05-23 00:00:00'::timestamp without time zone) END ('2007-05-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070524 START ('2007-05-24 00:00:00'::timestamp without time zone) END ('2007-05-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070525 START ('2007-05-25 00:00:00'::timestamp without time zone) END ('2007-05-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20070529 START ('2007-05-29 00:00:00'::timestamp without time zone) END ('2007-05-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070530 START ('2007-05-30 00:00:00'::timestamp without time zone) END ('2007-05-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20070531 START ('2007-05-31 00:00:00'::timestamp without time zone) END ('2007-06-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20070601 START ('2007-06-01 00:00:00'::timestamp without time zone) END ('2007-06-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20070604 START ('2007-06-04 00:00:00'::timestamp without time zone) END ('2007-06-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070605 START ('2007-06-05 00:00:00'::timestamp without time zone) END ('2007-06-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070606 START ('2007-06-06 00:00:00'::timestamp without time zone) END ('2007-06-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070607 START ('2007-06-07 00:00:00'::timestamp without time zone) END ('2007-06-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070608 START ('2007-06-08 00:00:00'::timestamp without time zone) END ('2007-06-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070611 START ('2007-06-11 00:00:00'::timestamp without time zone) END ('2007-06-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070612 START ('2007-06-12 00:00:00'::timestamp without time zone) END ('2007-06-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070613 START ('2007-06-13 00:00:00'::timestamp without time zone) END ('2007-06-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070614 START ('2007-06-14 00:00:00'::timestamp without time zone) END ('2007-06-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20070615 START ('2007-06-15 00:00:00'::timestamp without time zone) END ('2007-06-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070618 START ('2007-06-18 00:00:00'::timestamp without time zone) END ('2007-06-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070619 START ('2007-06-19 00:00:00'::timestamp without time zone) END ('2007-06-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070620 START ('2007-06-20 00:00:00'::timestamp without time zone) END ('2007-06-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070621 START ('2007-06-21 00:00:00'::timestamp without time zone) END ('2007-06-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070622 START ('2007-06-22 00:00:00'::timestamp without time zone) END ('2007-06-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070625 START ('2007-06-25 00:00:00'::timestamp without time zone) END ('2007-06-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070626 START ('2007-06-26 00:00:00'::timestamp without time zone) END ('2007-06-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070627 START ('2007-06-27 00:00:00'::timestamp without time zone) END ('2007-06-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20070628 START ('2007-06-28 00:00:00'::timestamp without time zone) END ('2007-06-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20070629 START ('2007-06-29 00:00:00'::timestamp without time zone) END ('2007-07-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070702 START ('2007-07-02 00:00:00'::timestamp without time zone) END ('2007-07-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20070703 START ('2007-07-03 00:00:00'::timestamp without time zone) END ('2007-07-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070705 START ('2007-07-05 00:00:00'::timestamp without time zone) END ('2007-07-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070706 START ('2007-07-06 00:00:00'::timestamp without time zone) END ('2007-07-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070709 START ('2007-07-09 00:00:00'::timestamp without time zone) END ('2007-07-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070710 START ('2007-07-10 00:00:00'::timestamp without time zone) END ('2007-07-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070711 START ('2007-07-11 00:00:00'::timestamp without time zone) END ('2007-07-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070712 START ('2007-07-12 00:00:00'::timestamp without time zone) END ('2007-07-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070713 START ('2007-07-13 00:00:00'::timestamp without time zone) END ('2007-07-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070716 START ('2007-07-16 00:00:00'::timestamp without time zone) END ('2007-07-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070717 START ('2007-07-17 00:00:00'::timestamp without time zone) END ('2007-07-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070718 START ('2007-07-18 00:00:00'::timestamp without time zone) END ('2007-07-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070719 START ('2007-07-19 00:00:00'::timestamp without time zone) END ('2007-07-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070720 START ('2007-07-20 00:00:00'::timestamp without time zone) END ('2007-07-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070723 START ('2007-07-23 00:00:00'::timestamp without time zone) END ('2007-07-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070724 START ('2007-07-24 00:00:00'::timestamp without time zone) END ('2007-07-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070725 START ('2007-07-25 00:00:00'::timestamp without time zone) END ('2007-07-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070726 START ('2007-07-26 00:00:00'::timestamp without time zone) END ('2007-07-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070727 START ('2007-07-27 00:00:00'::timestamp without time zone) END ('2007-07-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070730 START ('2007-07-30 00:00:00'::timestamp without time zone) END ('2007-07-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20070731 START ('2007-07-31 00:00:00'::timestamp without time zone) END ('2007-08-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20070801 START ('2007-08-01 00:00:00'::timestamp without time zone) END ('2007-08-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070802 START ('2007-08-02 00:00:00'::timestamp without time zone) END ('2007-08-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20070803 START ('2007-08-03 00:00:00'::timestamp without time zone) END ('2007-08-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070806 START ('2007-08-06 00:00:00'::timestamp without time zone) END ('2007-08-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070807 START ('2007-08-07 00:00:00'::timestamp without time zone) END ('2007-08-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070808 START ('2007-08-08 00:00:00'::timestamp without time zone) END ('2007-08-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070809 START ('2007-08-09 00:00:00'::timestamp without time zone) END ('2007-08-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070810 START ('2007-08-10 00:00:00'::timestamp without time zone) END ('2007-08-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070813 START ('2007-08-13 00:00:00'::timestamp without time zone) END ('2007-08-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070814 START ('2007-08-14 00:00:00'::timestamp without time zone) END ('2007-08-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20070815 START ('2007-08-15 00:00:00'::timestamp without time zone) END ('2007-08-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070816 START ('2007-08-16 00:00:00'::timestamp without time zone) END ('2007-08-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070817 START ('2007-08-17 00:00:00'::timestamp without time zone) END ('2007-08-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070820 START ('2007-08-20 00:00:00'::timestamp without time zone) END ('2007-08-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070821 START ('2007-08-21 00:00:00'::timestamp without time zone) END ('2007-08-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070822 START ('2007-08-22 00:00:00'::timestamp without time zone) END ('2007-08-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070823 START ('2007-08-23 00:00:00'::timestamp without time zone) END ('2007-08-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070824 START ('2007-08-24 00:00:00'::timestamp without time zone) END ('2007-08-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070827 START ('2007-08-27 00:00:00'::timestamp without time zone) END ('2007-08-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20070828 START ('2007-08-28 00:00:00'::timestamp without time zone) END ('2007-08-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20070829 START ('2007-08-29 00:00:00'::timestamp without time zone) END ('2007-08-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070830 START ('2007-08-30 00:00:00'::timestamp without time zone) END ('2007-08-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20070831 START ('2007-08-31 00:00:00'::timestamp without time zone) END ('2007-09-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20070904 START ('2007-09-04 00:00:00'::timestamp without time zone) END ('2007-09-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070905 START ('2007-09-05 00:00:00'::timestamp without time zone) END ('2007-09-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070906 START ('2007-09-06 00:00:00'::timestamp without time zone) END ('2007-09-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070907 START ('2007-09-07 00:00:00'::timestamp without time zone) END ('2007-09-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070910 START ('2007-09-10 00:00:00'::timestamp without time zone) END ('2007-09-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070911 START ('2007-09-11 00:00:00'::timestamp without time zone) END ('2007-09-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070912 START ('2007-09-12 00:00:00'::timestamp without time zone) END ('2007-09-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070913 START ('2007-09-13 00:00:00'::timestamp without time zone) END ('2007-09-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070914 START ('2007-09-14 00:00:00'::timestamp without time zone) END ('2007-09-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070917 START ('2007-09-17 00:00:00'::timestamp without time zone) END ('2007-09-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070918 START ('2007-09-18 00:00:00'::timestamp without time zone) END ('2007-09-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070919 START ('2007-09-19 00:00:00'::timestamp without time zone) END ('2007-09-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070920 START ('2007-09-20 00:00:00'::timestamp without time zone) END ('2007-09-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070921 START ('2007-09-21 00:00:00'::timestamp without time zone) END ('2007-09-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070924 START ('2007-09-24 00:00:00'::timestamp without time zone) END ('2007-09-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070925 START ('2007-09-25 00:00:00'::timestamp without time zone) END ('2007-09-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070926 START ('2007-09-26 00:00:00'::timestamp without time zone) END ('2007-09-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070927 START ('2007-09-27 00:00:00'::timestamp without time zone) END ('2007-09-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20070928 START ('2007-09-28 00:00:00'::timestamp without time zone) END ('2007-10-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20071001 START ('2007-10-01 00:00:00'::timestamp without time zone) END ('2007-10-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20071002 START ('2007-10-02 00:00:00'::timestamp without time zone) END ('2007-10-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20071003 START ('2007-10-03 00:00:00'::timestamp without time zone) END ('2007-10-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20071004 START ('2007-10-04 00:00:00'::timestamp without time zone) END ('2007-10-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20071005 START ('2007-10-05 00:00:00'::timestamp without time zone) END ('2007-10-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20071008 START ('2007-10-08 00:00:00'::timestamp without time zone) END ('2007-10-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20071009 START ('2007-10-09 00:00:00'::timestamp without time zone) END ('2007-10-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20071010 START ('2007-10-10 00:00:00'::timestamp without time zone) END ('2007-10-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20071011 START ('2007-10-11 00:00:00'::timestamp without time zone) END ('2007-10-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20071012 START ('2007-10-12 00:00:00'::timestamp without time zone) END ('2007-10-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20071015 START ('2007-10-15 00:00:00'::timestamp without time zone) END ('2007-10-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20071016 START ('2007-10-16 00:00:00'::timestamp without time zone) END ('2007-10-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20071017 START ('2007-10-17 00:00:00'::timestamp without time zone) END ('2007-10-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20071018 START ('2007-10-18 00:00:00'::timestamp without time zone) END ('2007-10-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20071019 START ('2007-10-19 00:00:00'::timestamp without time zone) END ('2007-10-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20071022 START ('2007-10-22 00:00:00'::timestamp without time zone) END ('2007-10-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20071023 START ('2007-10-23 00:00:00'::timestamp without time zone) END ('2007-10-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20071024 START ('2007-10-24 00:00:00'::timestamp without time zone) END ('2007-10-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20071025 START ('2007-10-25 00:00:00'::timestamp without time zone) END ('2007-10-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20071026 START ('2007-10-26 00:00:00'::timestamp without time zone) END ('2007-10-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20071029 START ('2007-10-29 00:00:00'::timestamp without time zone) END ('2007-10-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20071030 START ('2007-10-30 00:00:00'::timestamp without time zone) END ('2007-10-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20071031 START ('2007-10-31 00:00:00'::timestamp without time zone) END ('2007-11-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20071101 START ('2007-11-01 00:00:00'::timestamp without time zone) END ('2007-11-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20071102 START ('2007-11-02 00:00:00'::timestamp without time zone) END ('2007-11-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20071105 START ('2007-11-05 00:00:00'::timestamp without time zone) END ('2007-11-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20071106 START ('2007-11-06 00:00:00'::timestamp without time zone) END ('2007-11-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20071107 START ('2007-11-07 00:00:00'::timestamp without time zone) END ('2007-11-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20071108 START ('2007-11-08 00:00:00'::timestamp without time zone) END ('2007-11-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20071109 START ('2007-11-09 00:00:00'::timestamp without time zone) END ('2007-11-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20071112 START ('2007-11-12 00:00:00'::timestamp without time zone) END ('2007-11-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20071113 START ('2007-11-13 00:00:00'::timestamp without time zone) END ('2007-11-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20071114 START ('2007-11-14 00:00:00'::timestamp without time zone) END ('2007-11-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20071115 START ('2007-11-15 00:00:00'::timestamp without time zone) END ('2007-11-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20071116 START ('2007-11-16 00:00:00'::timestamp without time zone) END ('2007-11-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20071119 START ('2007-11-19 00:00:00'::timestamp without time zone) END ('2007-11-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20071120 START ('2007-11-20 00:00:00'::timestamp without time zone) END ('2007-11-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20071121 START ('2007-11-21 00:00:00'::timestamp without time zone) END ('2007-11-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20071123 START ('2007-11-23 00:00:00'::timestamp without time zone) END ('2007-11-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20071126 START ('2007-11-26 00:00:00'::timestamp without time zone) END ('2007-11-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20071127 START ('2007-11-27 00:00:00'::timestamp without time zone) END ('2007-11-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20071128 START ('2007-11-28 00:00:00'::timestamp without time zone) END ('2007-11-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20071129 START ('2007-11-29 00:00:00'::timestamp without time zone) END ('2007-11-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20071130 START ('2007-11-30 00:00:00'::timestamp without time zone) END ('2007-12-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20071203 START ('2007-12-03 00:00:00'::timestamp without time zone) END ('2007-12-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20071204 START ('2007-12-04 00:00:00'::timestamp without time zone) END ('2007-12-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20071205 START ('2007-12-05 00:00:00'::timestamp without time zone) END ('2007-12-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20071206 START ('2007-12-06 00:00:00'::timestamp without time zone) END ('2007-12-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20071207 START ('2007-12-07 00:00:00'::timestamp without time zone) END ('2007-12-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20071210 START ('2007-12-10 00:00:00'::timestamp without time zone) END ('2007-12-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20071211 START ('2007-12-11 00:00:00'::timestamp without time zone) END ('2007-12-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20071212 START ('2007-12-12 00:00:00'::timestamp without time zone) END ('2007-12-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20071213 START ('2007-12-13 00:00:00'::timestamp without time zone) END ('2007-12-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20071214 START ('2007-12-14 00:00:00'::timestamp without time zone) END ('2007-12-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20071217 START ('2007-12-17 00:00:00'::timestamp without time zone) END ('2007-12-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20071218 START ('2007-12-18 00:00:00'::timestamp without time zone) END ('2007-12-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20071219 START ('2007-12-19 00:00:00'::timestamp without time zone) END ('2007-12-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20071220 START ('2007-12-20 00:00:00'::timestamp without time zone) END ('2007-12-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20071221 START ('2007-12-21 00:00:00'::timestamp without time zone) END ('2007-12-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20071224 START ('2007-12-24 00:00:00'::timestamp without time zone) END ('2007-12-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20071226 START ('2007-12-26 00:00:00'::timestamp without time zone) END ('2007-12-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20071227 START ('2007-12-27 00:00:00'::timestamp without time zone) END ('2007-12-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20071228 START ('2007-12-28 00:00:00'::timestamp without time zone) END ('2007-12-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20071231 START ('2007-12-31 00:00:00'::timestamp without time zone) END ('2008-01-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080102 START ('2008-01-02 00:00:00'::timestamp without time zone) END ('2008-01-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080103 START ('2008-01-03 00:00:00'::timestamp without time zone) END ('2008-01-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080104 START ('2008-01-04 00:00:00'::timestamp without time zone) END ('2008-01-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080107 START ('2008-01-07 00:00:00'::timestamp without time zone) END ('2008-01-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080108 START ('2008-01-08 00:00:00'::timestamp without time zone) END ('2008-01-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080109 START ('2008-01-09 00:00:00'::timestamp without time zone) END ('2008-01-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080110 START ('2008-01-10 00:00:00'::timestamp without time zone) END ('2008-01-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080111 START ('2008-01-11 00:00:00'::timestamp without time zone) END ('2008-01-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080114 START ('2008-01-14 00:00:00'::timestamp without time zone) END ('2008-01-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080115 START ('2008-01-15 00:00:00'::timestamp without time zone) END ('2008-01-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080116 START ('2008-01-16 00:00:00'::timestamp without time zone) END ('2008-01-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080117 START ('2008-01-17 00:00:00'::timestamp without time zone) END ('2008-01-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080118 START ('2008-01-18 00:00:00'::timestamp without time zone) END ('2008-01-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080122 START ('2008-01-22 00:00:00'::timestamp without time zone) END ('2008-01-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080123 START ('2008-01-23 00:00:00'::timestamp without time zone) END ('2008-01-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080124 START ('2008-01-24 00:00:00'::timestamp without time zone) END ('2008-01-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080125 START ('2008-01-25 00:00:00'::timestamp without time zone) END ('2008-01-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080128 START ('2008-01-28 00:00:00'::timestamp without time zone) END ('2008-01-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080129 START ('2008-01-29 00:00:00'::timestamp without time zone) END ('2008-01-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080130 START ('2008-01-30 00:00:00'::timestamp without time zone) END ('2008-01-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20080131 START ('2008-01-31 00:00:00'::timestamp without time zone) END ('2008-02-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20080201 START ('2008-02-01 00:00:00'::timestamp without time zone) END ('2008-02-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080204 START ('2008-02-04 00:00:00'::timestamp without time zone) END ('2008-02-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080205 START ('2008-02-05 00:00:00'::timestamp without time zone) END ('2008-02-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20080206 START ('2008-02-06 00:00:00'::timestamp without time zone) END ('2008-02-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080207 START ('2008-02-07 00:00:00'::timestamp without time zone) END ('2008-02-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080208 START ('2008-02-08 00:00:00'::timestamp without time zone) END ('2008-02-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080211 START ('2008-02-11 00:00:00'::timestamp without time zone) END ('2008-02-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080212 START ('2008-02-12 00:00:00'::timestamp without time zone) END ('2008-02-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20080213 START ('2008-02-13 00:00:00'::timestamp without time zone) END ('2008-02-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080214 START ('2008-02-14 00:00:00'::timestamp without time zone) END ('2008-02-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080215 START ('2008-02-15 00:00:00'::timestamp without time zone) END ('2008-02-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080219 START ('2008-02-19 00:00:00'::timestamp without time zone) END ('2008-02-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20080220 START ('2008-02-20 00:00:00'::timestamp without time zone) END ('2008-02-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20080221 START ('2008-02-21 00:00:00'::timestamp without time zone) END ('2008-02-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080222 START ('2008-02-22 00:00:00'::timestamp without time zone) END ('2008-02-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080225 START ('2008-02-25 00:00:00'::timestamp without time zone) END ('2008-02-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20080226 START ('2008-02-26 00:00:00'::timestamp without time zone) END ('2008-02-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20080227 START ('2008-02-27 00:00:00'::timestamp without time zone) END ('2008-02-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080228 START ('2008-02-28 00:00:00'::timestamp without time zone) END ('2008-02-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080229 START ('2008-02-29 00:00:00'::timestamp without time zone) END ('2008-03-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080303 START ('2008-03-03 00:00:00'::timestamp without time zone) END ('2008-03-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080304 START ('2008-03-04 00:00:00'::timestamp without time zone) END ('2008-03-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080305 START ('2008-03-05 00:00:00'::timestamp without time zone) END ('2008-03-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20080306 START ('2008-03-06 00:00:00'::timestamp without time zone) END ('2008-03-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080307 START ('2008-03-07 00:00:00'::timestamp without time zone) END ('2008-03-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080310 START ('2008-03-10 00:00:00'::timestamp without time zone) END ('2008-03-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080311 START ('2008-03-11 00:00:00'::timestamp without time zone) END ('2008-03-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080312 START ('2008-03-12 00:00:00'::timestamp without time zone) END ('2008-03-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20080313 START ('2008-03-13 00:00:00'::timestamp without time zone) END ('2008-03-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080314 START ('2008-03-14 00:00:00'::timestamp without time zone) END ('2008-03-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080317 START ('2008-03-17 00:00:00'::timestamp without time zone) END ('2008-03-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080318 START ('2008-03-18 00:00:00'::timestamp without time zone) END ('2008-03-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080319 START ('2008-03-19 00:00:00'::timestamp without time zone) END ('2008-03-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20080320 START ('2008-03-20 00:00:00'::timestamp without time zone) END ('2008-03-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080324 START ('2008-03-24 00:00:00'::timestamp without time zone) END ('2008-03-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080325 START ('2008-03-25 00:00:00'::timestamp without time zone) END ('2008-03-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20080326 START ('2008-03-26 00:00:00'::timestamp without time zone) END ('2008-03-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20080327 START ('2008-03-27 00:00:00'::timestamp without time zone) END ('2008-03-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080328 START ('2008-03-28 00:00:00'::timestamp without time zone) END ('2008-03-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20080331 START ('2008-03-31 00:00:00'::timestamp without time zone) END ('2008-04-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20080401 START ('2008-04-01 00:00:00'::timestamp without time zone) END ('2008-04-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080402 START ('2008-04-02 00:00:00'::timestamp without time zone) END ('2008-04-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080403 START ('2008-04-03 00:00:00'::timestamp without time zone) END ('2008-04-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080404 START ('2008-04-04 00:00:00'::timestamp without time zone) END ('2008-04-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080407 START ('2008-04-07 00:00:00'::timestamp without time zone) END ('2008-04-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080408 START ('2008-04-08 00:00:00'::timestamp without time zone) END ('2008-04-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080409 START ('2008-04-09 00:00:00'::timestamp without time zone) END ('2008-04-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080410 START ('2008-04-10 00:00:00'::timestamp without time zone) END ('2008-04-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080411 START ('2008-04-11 00:00:00'::timestamp without time zone) END ('2008-04-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080414 START ('2008-04-14 00:00:00'::timestamp without time zone) END ('2008-04-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080415 START ('2008-04-15 00:00:00'::timestamp without time zone) END ('2008-04-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080416 START ('2008-04-16 00:00:00'::timestamp without time zone) END ('2008-04-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080417 START ('2008-04-17 00:00:00'::timestamp without time zone) END ('2008-04-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080418 START ('2008-04-18 00:00:00'::timestamp without time zone) END ('2008-04-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20080421 START ('2008-04-21 00:00:00'::timestamp without time zone) END ('2008-04-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080422 START ('2008-04-22 00:00:00'::timestamp without time zone) END ('2008-04-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080423 START ('2008-04-23 00:00:00'::timestamp without time zone) END ('2008-04-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080424 START ('2008-04-24 00:00:00'::timestamp without time zone) END ('2008-04-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080425 START ('2008-04-25 00:00:00'::timestamp without time zone) END ('2008-04-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080428 START ('2008-04-28 00:00:00'::timestamp without time zone) END ('2008-04-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080429 START ('2008-04-29 00:00:00'::timestamp without time zone) END ('2008-04-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080430 START ('2008-04-30 00:00:00'::timestamp without time zone) END ('2008-05-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20080501 START ('2008-05-01 00:00:00'::timestamp without time zone) END ('2008-05-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080502 START ('2008-05-02 00:00:00'::timestamp without time zone) END ('2008-05-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080505 START ('2008-05-05 00:00:00'::timestamp without time zone) END ('2008-05-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20080506 START ('2008-05-06 00:00:00'::timestamp without time zone) END ('2008-05-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080507 START ('2008-05-07 00:00:00'::timestamp without time zone) END ('2008-05-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080508 START ('2008-05-08 00:00:00'::timestamp without time zone) END ('2008-05-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080509 START ('2008-05-09 00:00:00'::timestamp without time zone) END ('2008-05-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080512 START ('2008-05-12 00:00:00'::timestamp without time zone) END ('2008-05-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20080513 START ('2008-05-13 00:00:00'::timestamp without time zone) END ('2008-05-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080514 START ('2008-05-14 00:00:00'::timestamp without time zone) END ('2008-05-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080515 START ('2008-05-15 00:00:00'::timestamp without time zone) END ('2008-05-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080516 START ('2008-05-16 00:00:00'::timestamp without time zone) END ('2008-05-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080519 START ('2008-05-19 00:00:00'::timestamp without time zone) END ('2008-05-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20080520 START ('2008-05-20 00:00:00'::timestamp without time zone) END ('2008-05-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20080521 START ('2008-05-21 00:00:00'::timestamp without time zone) END ('2008-05-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080522 START ('2008-05-22 00:00:00'::timestamp without time zone) END ('2008-05-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080523 START ('2008-05-23 00:00:00'::timestamp without time zone) END ('2008-05-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20080527 START ('2008-05-27 00:00:00'::timestamp without time zone) END ('2008-05-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080528 START ('2008-05-28 00:00:00'::timestamp without time zone) END ('2008-05-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080529 START ('2008-05-29 00:00:00'::timestamp without time zone) END ('2008-05-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080530 START ('2008-05-30 00:00:00'::timestamp without time zone) END ('2008-06-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080602 START ('2008-06-02 00:00:00'::timestamp without time zone) END ('2008-06-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080603 START ('2008-06-03 00:00:00'::timestamp without time zone) END ('2008-06-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080604 START ('2008-06-04 00:00:00'::timestamp without time zone) END ('2008-06-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080605 START ('2008-06-05 00:00:00'::timestamp without time zone) END ('2008-06-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20080606 START ('2008-06-06 00:00:00'::timestamp without time zone) END ('2008-06-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080609 START ('2008-06-09 00:00:00'::timestamp without time zone) END ('2008-06-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080610 START ('2008-06-10 00:00:00'::timestamp without time zone) END ('2008-06-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080611 START ('2008-06-11 00:00:00'::timestamp without time zone) END ('2008-06-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080612 START ('2008-06-12 00:00:00'::timestamp without time zone) END ('2008-06-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20080613 START ('2008-06-13 00:00:00'::timestamp without time zone) END ('2008-06-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080616 START ('2008-06-16 00:00:00'::timestamp without time zone) END ('2008-06-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080617 START ('2008-06-17 00:00:00'::timestamp without time zone) END ('2008-06-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080618 START ('2008-06-18 00:00:00'::timestamp without time zone) END ('2008-06-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080619 START ('2008-06-19 00:00:00'::timestamp without time zone) END ('2008-06-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20080620 START ('2008-06-20 00:00:00'::timestamp without time zone) END ('2008-06-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080623 START ('2008-06-23 00:00:00'::timestamp without time zone) END ('2008-06-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080624 START ('2008-06-24 00:00:00'::timestamp without time zone) END ('2008-06-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080625 START ('2008-06-25 00:00:00'::timestamp without time zone) END ('2008-06-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20080626 START ('2008-06-26 00:00:00'::timestamp without time zone) END ('2008-06-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20080627 START ('2008-06-27 00:00:00'::timestamp without time zone) END ('2008-06-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080630 START ('2008-06-30 00:00:00'::timestamp without time zone) END ('2008-07-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20080701 START ('2008-07-01 00:00:00'::timestamp without time zone) END ('2008-07-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080702 START ('2008-07-02 00:00:00'::timestamp without time zone) END ('2008-07-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080703 START ('2008-07-03 00:00:00'::timestamp without time zone) END ('2008-07-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080707 START ('2008-07-07 00:00:00'::timestamp without time zone) END ('2008-07-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080708 START ('2008-07-08 00:00:00'::timestamp without time zone) END ('2008-07-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080709 START ('2008-07-09 00:00:00'::timestamp without time zone) END ('2008-07-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080710 START ('2008-07-10 00:00:00'::timestamp without time zone) END ('2008-07-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080711 START ('2008-07-11 00:00:00'::timestamp without time zone) END ('2008-07-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080714 START ('2008-07-14 00:00:00'::timestamp without time zone) END ('2008-07-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080715 START ('2008-07-15 00:00:00'::timestamp without time zone) END ('2008-07-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080716 START ('2008-07-16 00:00:00'::timestamp without time zone) END ('2008-07-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080717 START ('2008-07-17 00:00:00'::timestamp without time zone) END ('2008-07-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080718 START ('2008-07-18 00:00:00'::timestamp without time zone) END ('2008-07-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20080721 START ('2008-07-21 00:00:00'::timestamp without time zone) END ('2008-07-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080722 START ('2008-07-22 00:00:00'::timestamp without time zone) END ('2008-07-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080723 START ('2008-07-23 00:00:00'::timestamp without time zone) END ('2008-07-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080724 START ('2008-07-24 00:00:00'::timestamp without time zone) END ('2008-07-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080725 START ('2008-07-25 00:00:00'::timestamp without time zone) END ('2008-07-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080728 START ('2008-07-28 00:00:00'::timestamp without time zone) END ('2008-07-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080729 START ('2008-07-29 00:00:00'::timestamp without time zone) END ('2008-07-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080730 START ('2008-07-30 00:00:00'::timestamp without time zone) END ('2008-07-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20080731 START ('2008-07-31 00:00:00'::timestamp without time zone) END ('2008-08-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20080801 START ('2008-08-01 00:00:00'::timestamp without time zone) END ('2008-08-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080804 START ('2008-08-04 00:00:00'::timestamp without time zone) END ('2008-08-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080805 START ('2008-08-05 00:00:00'::timestamp without time zone) END ('2008-08-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20080806 START ('2008-08-06 00:00:00'::timestamp without time zone) END ('2008-08-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080807 START ('2008-08-07 00:00:00'::timestamp without time zone) END ('2008-08-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080808 START ('2008-08-08 00:00:00'::timestamp without time zone) END ('2008-08-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080811 START ('2008-08-11 00:00:00'::timestamp without time zone) END ('2008-08-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080812 START ('2008-08-12 00:00:00'::timestamp without time zone) END ('2008-08-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20080813 START ('2008-08-13 00:00:00'::timestamp without time zone) END ('2008-08-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080814 START ('2008-08-14 00:00:00'::timestamp without time zone) END ('2008-08-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080815 START ('2008-08-15 00:00:00'::timestamp without time zone) END ('2008-08-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080818 START ('2008-08-18 00:00:00'::timestamp without time zone) END ('2008-08-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080819 START ('2008-08-19 00:00:00'::timestamp without time zone) END ('2008-08-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20080820 START ('2008-08-20 00:00:00'::timestamp without time zone) END ('2008-08-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20080821 START ('2008-08-21 00:00:00'::timestamp without time zone) END ('2008-08-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080822 START ('2008-08-22 00:00:00'::timestamp without time zone) END ('2008-08-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080825 START ('2008-08-25 00:00:00'::timestamp without time zone) END ('2008-08-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20080826 START ('2008-08-26 00:00:00'::timestamp without time zone) END ('2008-08-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20080827 START ('2008-08-27 00:00:00'::timestamp without time zone) END ('2008-08-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080828 START ('2008-08-28 00:00:00'::timestamp without time zone) END ('2008-08-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080829 START ('2008-08-29 00:00:00'::timestamp without time zone) END ('2008-09-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080902 START ('2008-09-02 00:00:00'::timestamp without time zone) END ('2008-09-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080903 START ('2008-09-03 00:00:00'::timestamp without time zone) END ('2008-09-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080904 START ('2008-09-04 00:00:00'::timestamp without time zone) END ('2008-09-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080905 START ('2008-09-05 00:00:00'::timestamp without time zone) END ('2008-09-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080908 START ('2008-09-08 00:00:00'::timestamp without time zone) END ('2008-09-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080909 START ('2008-09-09 00:00:00'::timestamp without time zone) END ('2008-09-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080910 START ('2008-09-10 00:00:00'::timestamp without time zone) END ('2008-09-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080911 START ('2008-09-11 00:00:00'::timestamp without time zone) END ('2008-09-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080912 START ('2008-09-12 00:00:00'::timestamp without time zone) END ('2008-09-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080915 START ('2008-09-15 00:00:00'::timestamp without time zone) END ('2008-09-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080916 START ('2008-09-16 00:00:00'::timestamp without time zone) END ('2008-09-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080917 START ('2008-09-17 00:00:00'::timestamp without time zone) END ('2008-09-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080918 START ('2008-09-18 00:00:00'::timestamp without time zone) END ('2008-09-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080919 START ('2008-09-19 00:00:00'::timestamp without time zone) END ('2008-09-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080922 START ('2008-09-22 00:00:00'::timestamp without time zone) END ('2008-09-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080923 START ('2008-09-23 00:00:00'::timestamp without time zone) END ('2008-09-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080924 START ('2008-09-24 00:00:00'::timestamp without time zone) END ('2008-09-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080925 START ('2008-09-25 00:00:00'::timestamp without time zone) END ('2008-09-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20080926 START ('2008-09-26 00:00:00'::timestamp without time zone) END ('2008-09-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080929 START ('2008-09-29 00:00:00'::timestamp without time zone) END ('2008-09-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080930 START ('2008-09-30 00:00:00'::timestamp without time zone) END ('2008-10-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20081001 START ('2008-10-01 00:00:00'::timestamp without time zone) END ('2008-10-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20081002 START ('2008-10-02 00:00:00'::timestamp without time zone) END ('2008-10-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20081003 START ('2008-10-03 00:00:00'::timestamp without time zone) END ('2008-10-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20081006 START ('2008-10-06 00:00:00'::timestamp without time zone) END ('2008-10-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20081007 START ('2008-10-07 00:00:00'::timestamp without time zone) END ('2008-10-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20081008 START ('2008-10-08 00:00:00'::timestamp without time zone) END ('2008-10-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20081009 START ('2008-10-09 00:00:00'::timestamp without time zone) END ('2008-10-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20081010 START ('2008-10-10 00:00:00'::timestamp without time zone) END ('2008-10-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20081013 START ('2008-10-13 00:00:00'::timestamp without time zone) END ('2008-10-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20081014 START ('2008-10-14 00:00:00'::timestamp without time zone) END ('2008-10-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20081015 START ('2008-10-15 00:00:00'::timestamp without time zone) END ('2008-10-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20081016 START ('2008-10-16 00:00:00'::timestamp without time zone) END ('2008-10-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20081017 START ('2008-10-17 00:00:00'::timestamp without time zone) END ('2008-10-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20081020 START ('2008-10-20 00:00:00'::timestamp without time zone) END ('2008-10-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20081021 START ('2008-10-21 00:00:00'::timestamp without time zone) END ('2008-10-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20081022 START ('2008-10-22 00:00:00'::timestamp without time zone) END ('2008-10-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20081023 START ('2008-10-23 00:00:00'::timestamp without time zone) END ('2008-10-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20081024 START ('2008-10-24 00:00:00'::timestamp without time zone) END ('2008-10-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20081027 START ('2008-10-27 00:00:00'::timestamp without time zone) END ('2008-10-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20081028 START ('2008-10-28 00:00:00'::timestamp without time zone) END ('2008-10-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20081029 START ('2008-10-29 00:00:00'::timestamp without time zone) END ('2008-10-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20081030 START ('2008-10-30 00:00:00'::timestamp without time zone) END ('2008-10-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20081031 START ('2008-10-31 00:00:00'::timestamp without time zone) END ('2008-11-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20081103 START ('2008-11-03 00:00:00'::timestamp without time zone) END ('2008-11-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20081104 START ('2008-11-04 00:00:00'::timestamp without time zone) END ('2008-11-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20081105 START ('2008-11-05 00:00:00'::timestamp without time zone) END ('2008-11-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20081106 START ('2008-11-06 00:00:00'::timestamp without time zone) END ('2008-11-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20081107 START ('2008-11-07 00:00:00'::timestamp without time zone) END ('2008-11-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20081110 START ('2008-11-10 00:00:00'::timestamp without time zone) END ('2008-11-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20081111 START ('2008-11-11 00:00:00'::timestamp without time zone) END ('2008-11-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20081112 START ('2008-11-12 00:00:00'::timestamp without time zone) END ('2008-11-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20081113 START ('2008-11-13 00:00:00'::timestamp without time zone) END ('2008-11-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20081114 START ('2008-11-14 00:00:00'::timestamp without time zone) END ('2008-11-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20081117 START ('2008-11-17 00:00:00'::timestamp without time zone) END ('2008-11-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20081118 START ('2008-11-18 00:00:00'::timestamp without time zone) END ('2008-11-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20081119 START ('2008-11-19 00:00:00'::timestamp without time zone) END ('2008-11-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20081120 START ('2008-11-20 00:00:00'::timestamp without time zone) END ('2008-11-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20081121 START ('2008-11-21 00:00:00'::timestamp without time zone) END ('2008-11-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20081124 START ('2008-11-24 00:00:00'::timestamp without time zone) END ('2008-11-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20081125 START ('2008-11-25 00:00:00'::timestamp without time zone) END ('2008-11-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20081126 START ('2008-11-26 00:00:00'::timestamp without time zone) END ('2008-11-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20081128 START ('2008-11-28 00:00:00'::timestamp without time zone) END ('2008-12-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20081201 START ('2008-12-01 00:00:00'::timestamp without time zone) END ('2008-12-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20081202 START ('2008-12-02 00:00:00'::timestamp without time zone) END ('2008-12-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20081203 START ('2008-12-03 00:00:00'::timestamp without time zone) END ('2008-12-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20081204 START ('2008-12-04 00:00:00'::timestamp without time zone) END ('2008-12-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20081205 START ('2008-12-05 00:00:00'::timestamp without time zone) END ('2008-12-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20081208 START ('2008-12-08 00:00:00'::timestamp without time zone) END ('2008-12-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20081209 START ('2008-12-09 00:00:00'::timestamp without time zone) END ('2008-12-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20081210 START ('2008-12-10 00:00:00'::timestamp without time zone) END ('2008-12-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20081211 START ('2008-12-11 00:00:00'::timestamp without time zone) END ('2008-12-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20081212 START ('2008-12-12 00:00:00'::timestamp without time zone) END ('2008-12-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20081215 START ('2008-12-15 00:00:00'::timestamp without time zone) END ('2008-12-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20081216 START ('2008-12-16 00:00:00'::timestamp without time zone) END ('2008-12-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20081217 START ('2008-12-17 00:00:00'::timestamp without time zone) END ('2008-12-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20081218 START ('2008-12-18 00:00:00'::timestamp without time zone) END ('2008-12-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20081219 START ('2008-12-19 00:00:00'::timestamp without time zone) END ('2008-12-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20081222 START ('2008-12-22 00:00:00'::timestamp without time zone) END ('2008-12-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20081223 START ('2008-12-23 00:00:00'::timestamp without time zone) END ('2008-12-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20081224 START ('2008-12-24 00:00:00'::timestamp without time zone) END ('2008-12-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20081226 START ('2008-12-26 00:00:00'::timestamp without time zone) END ('2008-12-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20081229 START ('2008-12-29 00:00:00'::timestamp without time zone) END ('2008-12-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20081230 START ('2008-12-30 00:00:00'::timestamp without time zone) END ('2008-12-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20081231 START ('2008-12-31 00:00:00'::timestamp without time zone) END ('2009-01-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20090102 START ('2009-01-02 00:00:00'::timestamp without time zone) END ('2009-01-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20090105 START ('2009-01-05 00:00:00'::timestamp without time zone) END ('2009-01-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20090106 START ('2009-01-06 00:00:00'::timestamp without time zone) END ('2009-01-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20090107 START ('2009-01-07 00:00:00'::timestamp without time zone) END ('2009-01-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20090108 START ('2009-01-08 00:00:00'::timestamp without time zone) END ('2009-01-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20090109 START ('2009-01-09 00:00:00'::timestamp without time zone) END ('2009-01-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20090112 START ('2009-01-12 00:00:00'::timestamp without time zone) END ('2009-01-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20090113 START ('2009-01-13 00:00:00'::timestamp without time zone) END ('2009-01-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20090114 START ('2009-01-14 00:00:00'::timestamp without time zone) END ('2009-01-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20090115 START ('2009-01-15 00:00:00'::timestamp without time zone) END ('2009-01-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20090116 START ('2009-01-16 00:00:00'::timestamp without time zone) END ('2009-01-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20090120 START ('2009-01-20 00:00:00'::timestamp without time zone) END ('2009-01-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20090121 START ('2009-01-21 00:00:00'::timestamp without time zone) END ('2009-01-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20090122 START ('2009-01-22 00:00:00'::timestamp without time zone) END ('2009-01-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20090123 START ('2009-01-23 00:00:00'::timestamp without time zone) END ('2009-01-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20090126 START ('2009-01-26 00:00:00'::timestamp without time zone) END ('2009-01-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20090127 START ('2009-01-27 00:00:00'::timestamp without time zone) END ('2009-01-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20090128 START ('2009-01-28 00:00:00'::timestamp without time zone) END ('2009-01-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20090129 START ('2009-01-29 00:00:00'::timestamp without time zone) END ('2009-01-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20090130 START ('2009-01-30 00:00:00'::timestamp without time zone) END ('2009-02-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20090202 START ('2009-02-02 00:00:00'::timestamp without time zone) END ('2009-02-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20090203 START ('2009-02-03 00:00:00'::timestamp without time zone) END ('2009-02-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20090204 START ('2009-02-04 00:00:00'::timestamp without time zone) END ('2009-02-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20090205 START ('2009-02-05 00:00:00'::timestamp without time zone) END ('2009-02-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20090206 START ('2009-02-06 00:00:00'::timestamp without time zone) END ('2009-02-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20090209 START ('2009-02-09 00:00:00'::timestamp without time zone) END ('2009-02-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20090210 START ('2009-02-10 00:00:00'::timestamp without time zone) END ('2009-02-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20090211 START ('2009-02-11 00:00:00'::timestamp without time zone) END ('2009-02-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20090212 START ('2009-02-12 00:00:00'::timestamp without time zone) END ('2009-02-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20090213 START ('2009-02-13 00:00:00'::timestamp without time zone) END ('2009-02-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20090217 START ('2009-02-17 00:00:00'::timestamp without time zone) END ('2009-02-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20090218 START ('2009-02-18 00:00:00'::timestamp without time zone) END ('2009-02-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20090219 START ('2009-02-19 00:00:00'::timestamp without time zone) END ('2009-02-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20090220 START ('2009-02-20 00:00:00'::timestamp without time zone) END ('2009-02-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20090223 START ('2009-02-23 00:00:00'::timestamp without time zone) END ('2009-02-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20090224 START ('2009-02-24 00:00:00'::timestamp without time zone) END ('2009-02-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20090225 START ('2009-02-25 00:00:00'::timestamp without time zone) END ('2009-02-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20090226 START ('2009-02-26 00:00:00'::timestamp without time zone) END ('2009-02-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20090227 START ('2009-02-27 00:00:00'::timestamp without time zone) END ('2009-03-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20090302 START ('2009-03-02 00:00:00'::timestamp without time zone) END ('2009-03-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20090303 START ('2009-03-03 00:00:00'::timestamp without time zone) END ('2009-03-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20090304 START ('2009-03-04 00:00:00'::timestamp without time zone) END ('2009-03-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20090305 START ('2009-03-05 00:00:00'::timestamp without time zone) END ('2009-03-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20090306 START ('2009-03-06 00:00:00'::timestamp without time zone) END ('2009-03-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20090309 START ('2009-03-09 00:00:00'::timestamp without time zone) END ('2009-03-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20090310 START ('2009-03-10 00:00:00'::timestamp without time zone) END ('2009-03-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20090311 START ('2009-03-11 00:00:00'::timestamp without time zone) END ('2009-03-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20090312 START ('2009-03-12 00:00:00'::timestamp without time zone) END ('2009-03-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20090313 START ('2009-03-13 00:00:00'::timestamp without time zone) END ('2009-03-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20090316 START ('2009-03-16 00:00:00'::timestamp without time zone) END ('2009-03-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20090317 START ('2009-03-17 00:00:00'::timestamp without time zone) END ('2009-03-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20090318 START ('2009-03-18 00:00:00'::timestamp without time zone) END ('2009-03-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20090319 START ('2009-03-19 00:00:00'::timestamp without time zone) END ('2009-03-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20090320 START ('2009-03-20 00:00:00'::timestamp without time zone) END ('2009-03-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20090323 START ('2009-03-23 00:00:00'::timestamp without time zone) END ('2009-03-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20090324 START ('2009-03-24 00:00:00'::timestamp without time zone) END ('2009-03-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20090325 START ('2009-03-25 00:00:00'::timestamp without time zone) END ('2009-03-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20090326 START ('2009-03-26 00:00:00'::timestamp without time zone) END ('2009-03-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20090327 START ('2009-03-27 00:00:00'::timestamp without time zone) END ('2009-03-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20090330 START ('2009-03-30 00:00:00'::timestamp without time zone) END ('2009-03-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20090331 START ('2009-03-31 00:00:00'::timestamp without time zone) END ('2009-04-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20090401 START ('2009-04-01 00:00:00'::timestamp without time zone) END ('2009-04-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20090402 START ('2009-04-02 00:00:00'::timestamp without time zone) END ('2009-04-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20090403 START ('2009-04-03 00:00:00'::timestamp without time zone) END ('2009-04-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20090406 START ('2009-04-06 00:00:00'::timestamp without time zone) END ('2009-04-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20090407 START ('2009-04-07 00:00:00'::timestamp without time zone) END ('2009-04-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20090408 START ('2009-04-08 00:00:00'::timestamp without time zone) END ('2009-04-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20090409 START ('2009-04-09 00:00:00'::timestamp without time zone) END ('2009-04-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20090413 START ('2009-04-13 00:00:00'::timestamp without time zone) END ('2009-04-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20090414 START ('2009-04-14 00:00:00'::timestamp without time zone) END ('2009-04-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20090415 START ('2009-04-15 00:00:00'::timestamp without time zone) END ('2009-04-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20090416 START ('2009-04-16 00:00:00'::timestamp without time zone) END ('2009-04-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20090417 START ('2009-04-17 00:00:00'::timestamp without time zone) END ('2009-04-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20090420 START ('2009-04-20 00:00:00'::timestamp without time zone) END ('2009-04-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20090421 START ('2009-04-21 00:00:00'::timestamp without time zone) END ('2009-04-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20090422 START ('2009-04-22 00:00:00'::timestamp without time zone) END ('2009-04-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20090423 START ('2009-04-23 00:00:00'::timestamp without time zone) END ('2009-04-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20090424 START ('2009-04-24 00:00:00'::timestamp without time zone) END ('2009-04-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20090427 START ('2009-04-27 00:00:00'::timestamp without time zone) END ('2009-04-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20090428 START ('2009-04-28 00:00:00'::timestamp without time zone) END ('2009-04-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20090429 START ('2009-04-29 00:00:00'::timestamp without time zone) END ('2009-04-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20090430 START ('2009-04-30 00:00:00'::timestamp without time zone) END ('2009-05-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20090501 START ('2009-05-01 00:00:00'::timestamp without time zone) END ('2009-05-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20090504 START ('2009-05-04 00:00:00'::timestamp without time zone) END ('2009-05-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20090505 START ('2009-05-05 00:00:00'::timestamp without time zone) END ('2009-05-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20090506 START ('2009-05-06 00:00:00'::timestamp without time zone) END ('2009-05-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20090507 START ('2009-05-07 00:00:00'::timestamp without time zone) END ('2009-05-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20090508 START ('2009-05-08 00:00:00'::timestamp without time zone) END ('2009-05-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20090511 START ('2009-05-11 00:00:00'::timestamp without time zone) END ('2009-05-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20090512 START ('2009-05-12 00:00:00'::timestamp without time zone) END ('2009-05-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20090513 START ('2009-05-13 00:00:00'::timestamp without time zone) END ('2009-05-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20090514 START ('2009-05-14 00:00:00'::timestamp without time zone) END ('2009-05-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20090515 START ('2009-05-15 00:00:00'::timestamp without time zone) END ('2009-05-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20090518 START ('2009-05-18 00:00:00'::timestamp without time zone) END ('2009-05-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20090519 START ('2009-05-19 00:00:00'::timestamp without time zone) END ('2009-05-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20090520 START ('2009-05-20 00:00:00'::timestamp without time zone) END ('2009-05-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20090521 START ('2009-05-21 00:00:00'::timestamp without time zone) END ('2009-05-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20090522 START ('2009-05-22 00:00:00'::timestamp without time zone) END ('2009-05-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20090526 START ('2009-05-26 00:00:00'::timestamp without time zone) END ('2009-05-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20090527 START ('2009-05-27 00:00:00'::timestamp without time zone) END ('2009-05-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20090528 START ('2009-05-28 00:00:00'::timestamp without time zone) END ('2009-05-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20090529 START ('2009-05-29 00:00:00'::timestamp without time zone) END ('2009-06-01 00:00:00'::timestamp without
 time zone)
          );


CREATE TABLE wiat_ola_tree (
    ola_tree_id numeric,
    root_roe_id numeric,
    parent_id numeric,
    child_id numeric,
    child_type_cd character varying(2),
    child_part_dt date,
    load_ts timestamp without time zone DEFAULT now() NOT NULL
) distributed by (root_roe_id ,parent_id ,child_id);

 

CREATE TABLE rtng_mtch (
    order_rcvd_dt timestamp without time zone,
    rtng_order_roe_id numeric(15,0),
    dstnt_cd character varying(2),
    odr_rfrnc_nb character varying(12),
    odr_entry_dt timestamp without time zone,
    route_match_prcsg_dt timestamp without time zone
) distributed by (rtng_order_roe_id) PARTITION BY RANGE(route_match_prcsg_dt)
          (
          PARTITION d20060102 START ('2006-01-02 00:00:00'::timestamp without time zone) END ('2006-01-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20061002 START ('2006-10-02 00:00:00'::timestamp without time zone) END ('2006-10-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20061003 START ('2006-10-03 00:00:00'::timestamp without time zone) END ('2006-10-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20061004 START ('2006-10-04 00:00:00'::timestamp without time zone) END ('2006-10-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20061005 START ('2006-10-05 00:00:00'::timestamp without time zone) END ('2006-10-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20061006 START ('2006-10-06 00:00:00'::timestamp without time zone) END ('2006-10-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20061009 START ('2006-10-09 00:00:00'::timestamp without time zone) END ('2006-10-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20061010 START ('2006-10-10 00:00:00'::timestamp without time zone) END ('2006-10-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20061011 START ('2006-10-11 00:00:00'::timestamp without time zone) END ('2006-10-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20061012 START ('2006-10-12 00:00:00'::timestamp without time zone) END ('2006-10-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20061013 START ('2006-10-13 00:00:00'::timestamp without time zone) END ('2006-10-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20061016 START ('2006-10-16 00:00:00'::timestamp without time zone) END ('2006-10-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20061017 START ('2006-10-17 00:00:00'::timestamp without time zone) END ('2006-10-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20061018 START ('2006-10-18 00:00:00'::timestamp without time zone) END ('2006-10-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20061019 START ('2006-10-19 00:00:00'::timestamp without time zone) END ('2006-10-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20061020 START ('2006-10-20 00:00:00'::timestamp without time zone) END ('2006-10-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20061023 START ('2006-10-23 00:00:00'::timestamp without time zone) END ('2006-10-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20061024 START ('2006-10-24 00:00:00'::timestamp without time zone) END ('2006-10-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20061025 START ('2006-10-25 00:00:00'::timestamp without time zone) END ('2006-10-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20061026 START ('2006-10-26 00:00:00'::timestamp without time zone) END ('2006-10-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20061027 START ('2006-10-27 00:00:00'::timestamp without time zone) END ('2006-10-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20061030 START ('2006-10-30 00:00:00'::timestamp without time zone) END ('2006-10-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20061031 START ('2006-10-31 00:00:00'::timestamp without time zone) END ('2006-11-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20061101 START ('2006-11-01 00:00:00'::timestamp without time zone) END ('2006-11-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20061102 START ('2006-11-02 00:00:00'::timestamp without time zone) END ('2006-11-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20061103 START ('2006-11-03 00:00:00'::timestamp without time zone) END ('2006-11-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20061106 START ('2006-11-06 00:00:00'::timestamp without time zone) END ('2006-11-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20061107 START ('2006-11-07 00:00:00'::timestamp without time zone) END ('2006-11-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20061108 START ('2006-11-08 00:00:00'::timestamp without time zone) END ('2006-11-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20061109 START ('2006-11-09 00:00:00'::timestamp without time zone) END ('2006-11-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20061110 START ('2006-11-10 00:00:00'::timestamp without time zone) END ('2006-11-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20061113 START ('2006-11-13 00:00:00'::timestamp without time zone) END ('2006-11-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20061114 START ('2006-11-14 00:00:00'::timestamp without time zone) END ('2006-11-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20061115 START ('2006-11-15 00:00:00'::timestamp without time zone) END ('2006-11-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20061116 START ('2006-11-16 00:00:00'::timestamp without time zone) END ('2006-11-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20061117 START ('2006-11-17 00:00:00'::timestamp without time zone) END ('2006-11-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20061120 START ('2006-11-20 00:00:00'::timestamp without time zone) END ('2006-11-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20061121 START ('2006-11-21 00:00:00'::timestamp without time zone) END ('2006-11-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20061122 START ('2006-11-22 00:00:00'::timestamp without time zone) END ('2006-11-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20061124 START ('2006-11-24 00:00:00'::timestamp without time zone) END ('2006-11-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20061127 START ('2006-11-27 00:00:00'::timestamp without time zone) END ('2006-11-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20061128 START ('2006-11-28 00:00:00'::timestamp without time zone) END ('2006-11-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20061129 START ('2006-11-29 00:00:00'::timestamp without time zone) END ('2006-11-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20061130 START ('2006-11-30 00:00:00'::timestamp without time zone) END ('2006-12-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20061201 START ('2006-12-01 00:00:00'::timestamp without time zone) END ('2006-12-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20061204 START ('2006-12-04 00:00:00'::timestamp without time zone) END ('2006-12-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20061205 START ('2006-12-05 00:00:00'::timestamp without time zone) END ('2006-12-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20061206 START ('2006-12-06 00:00:00'::timestamp without time zone) END ('2006-12-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20061207 START ('2006-12-07 00:00:00'::timestamp without time zone) END ('2006-12-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20061208 START ('2006-12-08 00:00:00'::timestamp without time zone) END ('2006-12-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20061211 START ('2006-12-11 00:00:00'::timestamp without time zone) END ('2006-12-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20061212 START ('2006-12-12 00:00:00'::timestamp without time zone) END ('2006-12-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20061213 START ('2006-12-13 00:00:00'::timestamp without time zone) END ('2006-12-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20061214 START ('2006-12-14 00:00:00'::timestamp without time zone) END ('2006-12-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20061215 START ('2006-12-15 00:00:00'::timestamp without time zone) END ('2006-12-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20061218 START ('2006-12-18 00:00:00'::timestamp without time zone) END ('2006-12-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20061219 START ('2006-12-19 00:00:00'::timestamp without time zone) END ('2006-12-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20061220 START ('2006-12-20 00:00:00'::timestamp without time zone) END ('2006-12-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20061221 START ('2006-12-21 00:00:00'::timestamp without time zone) END ('2006-12-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20061222 START ('2006-12-22 00:00:00'::timestamp without time zone) END ('2006-12-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20061226 START ('2006-12-26 00:00:00'::timestamp without time zone) END ('2006-12-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20061227 START ('2006-12-27 00:00:00'::timestamp without time zone) END ('2006-12-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20061228 START ('2006-12-28 00:00:00'::timestamp without time zone) END ('2006-12-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20061229 START ('2006-12-29 00:00:00'::timestamp without time zone) END ('2007-01-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20070103 START ('2007-01-03 00:00:00'::timestamp without time zone) END ('2007-01-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20070104 START ('2007-01-04 00:00:00'::timestamp without time zone) END ('2007-01-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070105 START ('2007-01-05 00:00:00'::timestamp without time zone) END ('2007-01-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070108 START ('2007-01-08 00:00:00'::timestamp without time zone) END ('2007-01-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070109 START ('2007-01-09 00:00:00'::timestamp without time zone) END ('2007-01-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070110 START ('2007-01-10 00:00:00'::timestamp without time zone) END ('2007-01-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070111 START ('2007-01-11 00:00:00'::timestamp without time zone) END ('2007-01-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070112 START ('2007-01-12 00:00:00'::timestamp without time zone) END ('2007-01-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070116 START ('2007-01-16 00:00:00'::timestamp without time zone) END ('2007-01-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070117 START ('2007-01-17 00:00:00'::timestamp without time zone) END ('2007-01-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070118 START ('2007-01-18 00:00:00'::timestamp without time zone) END ('2007-01-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070119 START ('2007-01-19 00:00:00'::timestamp without time zone) END ('2007-01-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070122 START ('2007-01-22 00:00:00'::timestamp without time zone) END ('2007-01-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070124 START ('2007-01-24 00:00:00'::timestamp without time zone) END ('2007-01-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070125 START ('2007-01-25 00:00:00'::timestamp without time zone) END ('2007-01-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070126 START ('2007-01-26 00:00:00'::timestamp without time zone) END ('2007-01-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20070129 START ('2007-01-29 00:00:00'::timestamp without time zone) END ('2007-01-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070130 START ('2007-01-30 00:00:00'::timestamp without time zone) END ('2007-01-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20070131 START ('2007-01-31 00:00:00'::timestamp without time zone) END ('2007-02-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20070201 START ('2007-02-01 00:00:00'::timestamp without time zone) END ('2007-02-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070202 START ('2007-02-02 00:00:00'::timestamp without time zone) END ('2007-02-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070205 START ('2007-02-05 00:00:00'::timestamp without time zone) END ('2007-02-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070206 START ('2007-02-06 00:00:00'::timestamp without time zone) END ('2007-02-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070207 START ('2007-02-07 00:00:00'::timestamp without time zone) END ('2007-02-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070208 START ('2007-02-08 00:00:00'::timestamp without time zone) END ('2007-02-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070209 START ('2007-02-09 00:00:00'::timestamp without time zone) END ('2007-02-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070212 START ('2007-02-12 00:00:00'::timestamp without time zone) END ('2007-02-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070213 START ('2007-02-13 00:00:00'::timestamp without time zone) END ('2007-02-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070214 START ('2007-02-14 00:00:00'::timestamp without time zone) END ('2007-02-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20070215 START ('2007-02-15 00:00:00'::timestamp without time zone) END ('2007-02-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070216 START ('2007-02-16 00:00:00'::timestamp without time zone) END ('2007-02-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070220 START ('2007-02-20 00:00:00'::timestamp without time zone) END ('2007-02-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070221 START ('2007-02-21 00:00:00'::timestamp without time zone) END ('2007-02-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070222 START ('2007-02-22 00:00:00'::timestamp without time zone) END ('2007-02-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070223 START ('2007-02-23 00:00:00'::timestamp without time zone) END ('2007-02-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070226 START ('2007-02-26 00:00:00'::timestamp without time zone) END ('2007-02-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070227 START ('2007-02-27 00:00:00'::timestamp without time zone) END ('2007-02-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20070228 START ('2007-02-28 00:00:00'::timestamp without time zone) END ('2007-03-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20070301 START ('2007-03-01 00:00:00'::timestamp without time zone) END ('2007-03-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070302 START ('2007-03-02 00:00:00'::timestamp without time zone) END ('2007-03-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070305 START ('2007-03-05 00:00:00'::timestamp without time zone) END ('2007-03-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070306 START ('2007-03-06 00:00:00'::timestamp without time zone) END ('2007-03-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070307 START ('2007-03-07 00:00:00'::timestamp without time zone) END ('2007-03-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070308 START ('2007-03-08 00:00:00'::timestamp without time zone) END ('2007-03-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070309 START ('2007-03-09 00:00:00'::timestamp without time zone) END ('2007-03-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070312 START ('2007-03-12 00:00:00'::timestamp without time zone) END ('2007-03-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070313 START ('2007-03-13 00:00:00'::timestamp without time zone) END ('2007-03-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070314 START ('2007-03-14 00:00:00'::timestamp without time zone) END ('2007-03-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20070315 START ('2007-03-15 00:00:00'::timestamp without time zone) END ('2007-03-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070316 START ('2007-03-16 00:00:00'::timestamp without time zone) END ('2007-03-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070319 START ('2007-03-19 00:00:00'::timestamp without time zone) END ('2007-03-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070320 START ('2007-03-20 00:00:00'::timestamp without time zone) END ('2007-03-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070321 START ('2007-03-21 00:00:00'::timestamp without time zone) END ('2007-03-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070322 START ('2007-03-22 00:00:00'::timestamp without time zone) END ('2007-03-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070323 START ('2007-03-23 00:00:00'::timestamp without time zone) END ('2007-03-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070326 START ('2007-03-26 00:00:00'::timestamp without time zone) END ('2007-03-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070327 START ('2007-03-27 00:00:00'::timestamp without time zone) END ('2007-03-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20070328 START ('2007-03-28 00:00:00'::timestamp without time zone) END ('2007-03-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20070329 START ('2007-03-29 00:00:00'::timestamp without time zone) END ('2007-03-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070330 START ('2007-03-30 00:00:00'::timestamp without time zone) END ('2007-04-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070402 START ('2007-04-02 00:00:00'::timestamp without time zone) END ('2007-04-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20070403 START ('2007-04-03 00:00:00'::timestamp without time zone) END ('2007-04-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20070404 START ('2007-04-04 00:00:00'::timestamp without time zone) END ('2007-04-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070405 START ('2007-04-05 00:00:00'::timestamp without time zone) END ('2007-04-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070409 START ('2007-04-09 00:00:00'::timestamp without time zone) END ('2007-04-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070410 START ('2007-04-10 00:00:00'::timestamp without time zone) END ('2007-04-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070411 START ('2007-04-11 00:00:00'::timestamp without time zone) END ('2007-04-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070412 START ('2007-04-12 00:00:00'::timestamp without time zone) END ('2007-04-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070413 START ('2007-04-13 00:00:00'::timestamp without time zone) END ('2007-04-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070416 START ('2007-04-16 00:00:00'::timestamp without time zone) END ('2007-04-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070417 START ('2007-04-17 00:00:00'::timestamp without time zone) END ('2007-04-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070418 START ('2007-04-18 00:00:00'::timestamp without time zone) END ('2007-04-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070419 START ('2007-04-19 00:00:00'::timestamp without time zone) END ('2007-04-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070420 START ('2007-04-20 00:00:00'::timestamp without time zone) END ('2007-04-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070423 START ('2007-04-23 00:00:00'::timestamp without time zone) END ('2007-04-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070424 START ('2007-04-24 00:00:00'::timestamp without time zone) END ('2007-04-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070425 START ('2007-04-25 00:00:00'::timestamp without time zone) END ('2007-04-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070426 START ('2007-04-26 00:00:00'::timestamp without time zone) END ('2007-04-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070427 START ('2007-04-27 00:00:00'::timestamp without time zone) END ('2007-04-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070430 START ('2007-04-30 00:00:00'::timestamp without time zone) END ('2007-05-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20070501 START ('2007-05-01 00:00:00'::timestamp without time zone) END ('2007-05-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070502 START ('2007-05-02 00:00:00'::timestamp without time zone) END ('2007-05-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20070503 START ('2007-05-03 00:00:00'::timestamp without time zone) END ('2007-05-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20070504 START ('2007-05-04 00:00:00'::timestamp without time zone) END ('2007-05-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070507 START ('2007-05-07 00:00:00'::timestamp without time zone) END ('2007-05-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070508 START ('2007-05-08 00:00:00'::timestamp without time zone) END ('2007-05-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070509 START ('2007-05-09 00:00:00'::timestamp without time zone) END ('2007-05-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070510 START ('2007-05-10 00:00:00'::timestamp without time zone) END ('2007-05-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070511 START ('2007-05-11 00:00:00'::timestamp without time zone) END ('2007-05-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070514 START ('2007-05-14 00:00:00'::timestamp without time zone) END ('2007-05-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20070515 START ('2007-05-15 00:00:00'::timestamp without time zone) END ('2007-05-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070516 START ('2007-05-16 00:00:00'::timestamp without time zone) END ('2007-05-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070517 START ('2007-05-17 00:00:00'::timestamp without time zone) END ('2007-05-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070518 START ('2007-05-18 00:00:00'::timestamp without time zone) END ('2007-05-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070521 START ('2007-05-21 00:00:00'::timestamp without time zone) END ('2007-05-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070522 START ('2007-05-22 00:00:00'::timestamp without time zone) END ('2007-05-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070523 START ('2007-05-23 00:00:00'::timestamp without time zone) END ('2007-05-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070524 START ('2007-05-24 00:00:00'::timestamp without time zone) END ('2007-05-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070525 START ('2007-05-25 00:00:00'::timestamp without time zone) END ('2007-05-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20070529 START ('2007-05-29 00:00:00'::timestamp without time zone) END ('2007-05-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070530 START ('2007-05-30 00:00:00'::timestamp without time zone) END ('2007-05-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20070531 START ('2007-05-31 00:00:00'::timestamp without time zone) END ('2007-06-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20070601 START ('2007-06-01 00:00:00'::timestamp without time zone) END ('2007-06-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20070604 START ('2007-06-04 00:00:00'::timestamp without time zone) END ('2007-06-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070605 START ('2007-06-05 00:00:00'::timestamp without time zone) END ('2007-06-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070606 START ('2007-06-06 00:00:00'::timestamp without time zone) END ('2007-06-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070607 START ('2007-06-07 00:00:00'::timestamp without time zone) END ('2007-06-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070608 START ('2007-06-08 00:00:00'::timestamp without time zone) END ('2007-06-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070611 START ('2007-06-11 00:00:00'::timestamp without time zone) END ('2007-06-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070612 START ('2007-06-12 00:00:00'::timestamp without time zone) END ('2007-06-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070613 START ('2007-06-13 00:00:00'::timestamp without time zone) END ('2007-06-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070614 START ('2007-06-14 00:00:00'::timestamp without time zone) END ('2007-06-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20070615 START ('2007-06-15 00:00:00'::timestamp without time zone) END ('2007-06-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070618 START ('2007-06-18 00:00:00'::timestamp without time zone) END ('2007-06-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070619 START ('2007-06-19 00:00:00'::timestamp without time zone) END ('2007-06-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070620 START ('2007-06-20 00:00:00'::timestamp without time zone) END ('2007-06-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070621 START ('2007-06-21 00:00:00'::timestamp without time zone) END ('2007-06-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070622 START ('2007-06-22 00:00:00'::timestamp without time zone) END ('2007-06-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070625 START ('2007-06-25 00:00:00'::timestamp without time zone) END ('2007-06-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070626 START ('2007-06-26 00:00:00'::timestamp without time zone) END ('2007-06-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070627 START ('2007-06-27 00:00:00'::timestamp without time zone) END ('2007-06-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20070628 START ('2007-06-28 00:00:00'::timestamp without time zone) END ('2007-06-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20070629 START ('2007-06-29 00:00:00'::timestamp without time zone) END ('2007-07-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070702 START ('2007-07-02 00:00:00'::timestamp without time zone) END ('2007-07-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20070703 START ('2007-07-03 00:00:00'::timestamp without time zone) END ('2007-07-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070705 START ('2007-07-05 00:00:00'::timestamp without time zone) END ('2007-07-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070706 START ('2007-07-06 00:00:00'::timestamp without time zone) END ('2007-07-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070709 START ('2007-07-09 00:00:00'::timestamp without time zone) END ('2007-07-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070710 START ('2007-07-10 00:00:00'::timestamp without time zone) END ('2007-07-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070711 START ('2007-07-11 00:00:00'::timestamp without time zone) END ('2007-07-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070712 START ('2007-07-12 00:00:00'::timestamp without time zone) END ('2007-07-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070713 START ('2007-07-13 00:00:00'::timestamp without time zone) END ('2007-07-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070716 START ('2007-07-16 00:00:00'::timestamp without time zone) END ('2007-07-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070717 START ('2007-07-17 00:00:00'::timestamp without time zone) END ('2007-07-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070718 START ('2007-07-18 00:00:00'::timestamp without time zone) END ('2007-07-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070719 START ('2007-07-19 00:00:00'::timestamp without time zone) END ('2007-07-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070720 START ('2007-07-20 00:00:00'::timestamp without time zone) END ('2007-07-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070723 START ('2007-07-23 00:00:00'::timestamp without time zone) END ('2007-07-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070724 START ('2007-07-24 00:00:00'::timestamp without time zone) END ('2007-07-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070725 START ('2007-07-25 00:00:00'::timestamp without time zone) END ('2007-07-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070726 START ('2007-07-26 00:00:00'::timestamp without time zone) END ('2007-07-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070727 START ('2007-07-27 00:00:00'::timestamp without time zone) END ('2007-07-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070730 START ('2007-07-30 00:00:00'::timestamp without time zone) END ('2007-07-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20070731 START ('2007-07-31 00:00:00'::timestamp without time zone) END ('2007-08-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20070801 START ('2007-08-01 00:00:00'::timestamp without time zone) END ('2007-08-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20070802 START ('2007-08-02 00:00:00'::timestamp without time zone) END ('2007-08-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20070803 START ('2007-08-03 00:00:00'::timestamp without time zone) END ('2007-08-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070806 START ('2007-08-06 00:00:00'::timestamp without time zone) END ('2007-08-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070807 START ('2007-08-07 00:00:00'::timestamp without time zone) END ('2007-08-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20070808 START ('2007-08-08 00:00:00'::timestamp without time zone) END ('2007-08-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20070809 START ('2007-08-09 00:00:00'::timestamp without time zone) END ('2007-08-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070810 START ('2007-08-10 00:00:00'::timestamp without time zone) END ('2007-08-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070813 START ('2007-08-13 00:00:00'::timestamp without time zone) END ('2007-08-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070814 START ('2007-08-14 00:00:00'::timestamp without time zone) END ('2007-08-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20070815 START ('2007-08-15 00:00:00'::timestamp without time zone) END ('2007-08-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20070816 START ('2007-08-16 00:00:00'::timestamp without time zone) END ('2007-08-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070817 START ('2007-08-17 00:00:00'::timestamp without time zone) END ('2007-08-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070820 START ('2007-08-20 00:00:00'::timestamp without time zone) END ('2007-08-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070821 START ('2007-08-21 00:00:00'::timestamp without time zone) END ('2007-08-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20070822 START ('2007-08-22 00:00:00'::timestamp without time zone) END ('2007-08-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20070823 START ('2007-08-23 00:00:00'::timestamp without time zone) END ('2007-08-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070824 START ('2007-08-24 00:00:00'::timestamp without time zone) END ('2007-08-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070827 START ('2007-08-27 00:00:00'::timestamp without time zone) END ('2007-08-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20070828 START ('2007-08-28 00:00:00'::timestamp without time zone) END ('2007-08-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20070829 START ('2007-08-29 00:00:00'::timestamp without time zone) END ('2007-08-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20070830 START ('2007-08-30 00:00:00'::timestamp without time zone) END ('2007-08-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20070831 START ('2007-08-31 00:00:00'::timestamp without time zone) END ('2007-09-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20070904 START ('2007-09-04 00:00:00'::timestamp without time zone) END ('2007-09-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20070905 START ('2007-09-05 00:00:00'::timestamp without time zone) END ('2007-09-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20070906 START ('2007-09-06 00:00:00'::timestamp without time zone) END ('2007-09-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20070907 START ('2007-09-07 00:00:00'::timestamp without time zone) END ('2007-09-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20070910 START ('2007-09-10 00:00:00'::timestamp without time zone) END ('2007-09-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20070911 START ('2007-09-11 00:00:00'::timestamp without time zone) END ('2007-09-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20070912 START ('2007-09-12 00:00:00'::timestamp without time zone) END ('2007-09-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20070913 START ('2007-09-13 00:00:00'::timestamp without time zone) END ('2007-09-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20070914 START ('2007-09-14 00:00:00'::timestamp without time zone) END ('2007-09-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20070917 START ('2007-09-17 00:00:00'::timestamp without time zone) END ('2007-09-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20070918 START ('2007-09-18 00:00:00'::timestamp without time zone) END ('2007-09-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20070919 START ('2007-09-19 00:00:00'::timestamp without time zone) END ('2007-09-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20070920 START ('2007-09-20 00:00:00'::timestamp without time zone) END ('2007-09-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20070921 START ('2007-09-21 00:00:00'::timestamp without time zone) END ('2007-09-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20070924 START ('2007-09-24 00:00:00'::timestamp without time zone) END ('2007-09-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20070925 START ('2007-09-25 00:00:00'::timestamp without time zone) END ('2007-09-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20070926 START ('2007-09-26 00:00:00'::timestamp without time zone) END ('2007-09-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20070927 START ('2007-09-27 00:00:00'::timestamp without time zone) END ('2007-09-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20070928 START ('2007-09-28 00:00:00'::timestamp without time zone) END ('2007-10-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20071001 START ('2007-10-01 00:00:00'::timestamp without time zone) END ('2007-10-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20071002 START ('2007-10-02 00:00:00'::timestamp without time zone) END ('2007-10-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20071003 START ('2007-10-03 00:00:00'::timestamp without time zone) END ('2007-10-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20071004 START ('2007-10-04 00:00:00'::timestamp without time zone) END ('2007-10-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20071005 START ('2007-10-05 00:00:00'::timestamp without time zone) END ('2007-10-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20071008 START ('2007-10-08 00:00:00'::timestamp without time zone) END ('2007-10-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20071009 START ('2007-10-09 00:00:00'::timestamp without time zone) END ('2007-10-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20071010 START ('2007-10-10 00:00:00'::timestamp without time zone) END ('2007-10-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20071011 START ('2007-10-11 00:00:00'::timestamp without time zone) END ('2007-10-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20071012 START ('2007-10-12 00:00:00'::timestamp without time zone) END ('2007-10-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20071015 START ('2007-10-15 00:00:00'::timestamp without time zone) END ('2007-10-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20071016 START ('2007-10-16 00:00:00'::timestamp without time zone) END ('2007-10-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20071017 START ('2007-10-17 00:00:00'::timestamp without time zone) END ('2007-10-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20071018 START ('2007-10-18 00:00:00'::timestamp without time zone) END ('2007-10-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20071019 START ('2007-10-19 00:00:00'::timestamp without time zone) END ('2007-10-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20071022 START ('2007-10-22 00:00:00'::timestamp without time zone) END ('2007-10-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20071023 START ('2007-10-23 00:00:00'::timestamp without time zone) END ('2007-10-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20071024 START ('2007-10-24 00:00:00'::timestamp without time zone) END ('2007-10-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20071025 START ('2007-10-25 00:00:00'::timestamp without time zone) END ('2007-10-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20071026 START ('2007-10-26 00:00:00'::timestamp without time zone) END ('2007-10-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20071029 START ('2007-10-29 00:00:00'::timestamp without time zone) END ('2007-10-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20071030 START ('2007-10-30 00:00:00'::timestamp without time zone) END ('2007-10-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20071031 START ('2007-10-31 00:00:00'::timestamp without time zone) END ('2007-11-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20071101 START ('2007-11-01 00:00:00'::timestamp without time zone) END ('2007-11-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20071102 START ('2007-11-02 00:00:00'::timestamp without time zone) END ('2007-11-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20071105 START ('2007-11-05 00:00:00'::timestamp without time zone) END ('2007-11-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20071106 START ('2007-11-06 00:00:00'::timestamp without time zone) END ('2007-11-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20071107 START ('2007-11-07 00:00:00'::timestamp without time zone) END ('2007-11-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20071108 START ('2007-11-08 00:00:00'::timestamp without time zone) END ('2007-11-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20071109 START ('2007-11-09 00:00:00'::timestamp without time zone) END ('2007-11-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20071112 START ('2007-11-12 00:00:00'::timestamp without time zone) END ('2007-11-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20071113 START ('2007-11-13 00:00:00'::timestamp without time zone) END ('2007-11-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20071114 START ('2007-11-14 00:00:00'::timestamp without time zone) END ('2007-11-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20071115 START ('2007-11-15 00:00:00'::timestamp without time zone) END ('2007-11-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20071116 START ('2007-11-16 00:00:00'::timestamp without time zone) END ('2007-11-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20071119 START ('2007-11-19 00:00:00'::timestamp without time zone) END ('2007-11-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20071120 START ('2007-11-20 00:00:00'::timestamp without time zone) END ('2007-11-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20071121 START ('2007-11-21 00:00:00'::timestamp without time zone) END ('2007-11-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20071123 START ('2007-11-23 00:00:00'::timestamp without time zone) END ('2007-11-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20071126 START ('2007-11-26 00:00:00'::timestamp without time zone) END ('2007-11-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20071127 START ('2007-11-27 00:00:00'::timestamp without time zone) END ('2007-11-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20071128 START ('2007-11-28 00:00:00'::timestamp without time zone) END ('2007-11-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20071129 START ('2007-11-29 00:00:00'::timestamp without time zone) END ('2007-11-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20071130 START ('2007-11-30 00:00:00'::timestamp without time zone) END ('2007-12-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20071203 START ('2007-12-03 00:00:00'::timestamp without time zone) END ('2007-12-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20071204 START ('2007-12-04 00:00:00'::timestamp without time zone) END ('2007-12-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20071205 START ('2007-12-05 00:00:00'::timestamp without time zone) END ('2007-12-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20071206 START ('2007-12-06 00:00:00'::timestamp without time zone) END ('2007-12-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20071207 START ('2007-12-07 00:00:00'::timestamp without time zone) END ('2007-12-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20071210 START ('2007-12-10 00:00:00'::timestamp without time zone) END ('2007-12-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20071211 START ('2007-12-11 00:00:00'::timestamp without time zone) END ('2007-12-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20071212 START ('2007-12-12 00:00:00'::timestamp without time zone) END ('2007-12-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20071213 START ('2007-12-13 00:00:00'::timestamp without time zone) END ('2007-12-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20071214 START ('2007-12-14 00:00:00'::timestamp without time zone) END ('2007-12-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20071217 START ('2007-12-17 00:00:00'::timestamp without time zone) END ('2007-12-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20071218 START ('2007-12-18 00:00:00'::timestamp without time zone) END ('2007-12-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20071219 START ('2007-12-19 00:00:00'::timestamp without time zone) END ('2007-12-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20071220 START ('2007-12-20 00:00:00'::timestamp without time zone) END ('2007-12-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20071221 START ('2007-12-21 00:00:00'::timestamp without time zone) END ('2007-12-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20071224 START ('2007-12-24 00:00:00'::timestamp without time zone) END ('2007-12-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20071226 START ('2007-12-26 00:00:00'::timestamp without time zone) END ('2007-12-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20071227 START ('2007-12-27 00:00:00'::timestamp without time zone) END ('2007-12-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20071228 START ('2007-12-28 00:00:00'::timestamp without time zone) END ('2007-12-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20071231 START ('2007-12-31 00:00:00'::timestamp without time zone) END ('2008-01-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080102 START ('2008-01-02 00:00:00'::timestamp without time zone) END ('2008-01-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080103 START ('2008-01-03 00:00:00'::timestamp without time zone) END ('2008-01-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080104 START ('2008-01-04 00:00:00'::timestamp without time zone) END ('2008-01-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080107 START ('2008-01-07 00:00:00'::timestamp without time zone) END ('2008-01-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080108 START ('2008-01-08 00:00:00'::timestamp without time zone) END ('2008-01-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080109 START ('2008-01-09 00:00:00'::timestamp without time zone) END ('2008-01-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080110 START ('2008-01-10 00:00:00'::timestamp without time zone) END ('2008-01-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080111 START ('2008-01-11 00:00:00'::timestamp without time zone) END ('2008-01-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080114 START ('2008-01-14 00:00:00'::timestamp without time zone) END ('2008-01-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080115 START ('2008-01-15 00:00:00'::timestamp without time zone) END ('2008-01-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080116 START ('2008-01-16 00:00:00'::timestamp without time zone) END ('2008-01-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080117 START ('2008-01-17 00:00:00'::timestamp without time zone) END ('2008-01-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080118 START ('2008-01-18 00:00:00'::timestamp without time zone) END ('2008-01-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080122 START ('2008-01-22 00:00:00'::timestamp without time zone) END ('2008-01-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080123 START ('2008-01-23 00:00:00'::timestamp without time zone) END ('2008-01-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080124 START ('2008-01-24 00:00:00'::timestamp without time zone) END ('2008-01-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080125 START ('2008-01-25 00:00:00'::timestamp without time zone) END ('2008-01-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080128 START ('2008-01-28 00:00:00'::timestamp without time zone) END ('2008-01-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080129 START ('2008-01-29 00:00:00'::timestamp without time zone) END ('2008-01-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080130 START ('2008-01-30 00:00:00'::timestamp without time zone) END ('2008-01-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20080131 START ('2008-01-31 00:00:00'::timestamp without time zone) END ('2008-02-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20080201 START ('2008-02-01 00:00:00'::timestamp without time zone) END ('2008-02-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080204 START ('2008-02-04 00:00:00'::timestamp without time zone) END ('2008-02-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080205 START ('2008-02-05 00:00:00'::timestamp without time zone) END ('2008-02-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20080206 START ('2008-02-06 00:00:00'::timestamp without time zone) END ('2008-02-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080207 START ('2008-02-07 00:00:00'::timestamp without time zone) END ('2008-02-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080208 START ('2008-02-08 00:00:00'::timestamp without time zone) END ('2008-02-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080211 START ('2008-02-11 00:00:00'::timestamp without time zone) END ('2008-02-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080212 START ('2008-02-12 00:00:00'::timestamp without time zone) END ('2008-02-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20080213 START ('2008-02-13 00:00:00'::timestamp without time zone) END ('2008-02-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080214 START ('2008-02-14 00:00:00'::timestamp without time zone) END ('2008-02-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080215 START ('2008-02-15 00:00:00'::timestamp without time zone) END ('2008-02-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080219 START ('2008-02-19 00:00:00'::timestamp without time zone) END ('2008-02-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20080220 START ('2008-02-20 00:00:00'::timestamp without time zone) END ('2008-02-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20080221 START ('2008-02-21 00:00:00'::timestamp without time zone) END ('2008-02-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080222 START ('2008-02-22 00:00:00'::timestamp without time zone) END ('2008-02-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080225 START ('2008-02-25 00:00:00'::timestamp without time zone) END ('2008-02-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20080226 START ('2008-02-26 00:00:00'::timestamp without time zone) END ('2008-02-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20080227 START ('2008-02-27 00:00:00'::timestamp without time zone) END ('2008-02-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080228 START ('2008-02-28 00:00:00'::timestamp without time zone) END ('2008-02-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080229 START ('2008-02-29 00:00:00'::timestamp without time zone) END ('2008-03-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080303 START ('2008-03-03 00:00:00'::timestamp without time zone) END ('2008-03-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080304 START ('2008-03-04 00:00:00'::timestamp without time zone) END ('2008-03-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080305 START ('2008-03-05 00:00:00'::timestamp without time zone) END ('2008-03-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20080306 START ('2008-03-06 00:00:00'::timestamp without time zone) END ('2008-03-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080307 START ('2008-03-07 00:00:00'::timestamp without time zone) END ('2008-03-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080310 START ('2008-03-10 00:00:00'::timestamp without time zone) END ('2008-03-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080311 START ('2008-03-11 00:00:00'::timestamp without time zone) END ('2008-03-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080312 START ('2008-03-12 00:00:00'::timestamp without time zone) END ('2008-03-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20080313 START ('2008-03-13 00:00:00'::timestamp without time zone) END ('2008-03-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080314 START ('2008-03-14 00:00:00'::timestamp without time zone) END ('2008-03-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080317 START ('2008-03-17 00:00:00'::timestamp without time zone) END ('2008-03-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080318 START ('2008-03-18 00:00:00'::timestamp without time zone) END ('2008-03-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080319 START ('2008-03-19 00:00:00'::timestamp without time zone) END ('2008-03-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20080320 START ('2008-03-20 00:00:00'::timestamp without time zone) END ('2008-03-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080324 START ('2008-03-24 00:00:00'::timestamp without time zone) END ('2008-03-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080325 START ('2008-03-25 00:00:00'::timestamp without time zone) END ('2008-03-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20080326 START ('2008-03-26 00:00:00'::timestamp without time zone) END ('2008-03-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20080327 START ('2008-03-27 00:00:00'::timestamp without time zone) END ('2008-03-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080328 START ('2008-03-28 00:00:00'::timestamp without time zone) END ('2008-03-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20080331 START ('2008-03-31 00:00:00'::timestamp without time zone) END ('2008-04-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20080401 START ('2008-04-01 00:00:00'::timestamp without time zone) END ('2008-04-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080402 START ('2008-04-02 00:00:00'::timestamp without time zone) END ('2008-04-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080403 START ('2008-04-03 00:00:00'::timestamp without time zone) END ('2008-04-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080404 START ('2008-04-04 00:00:00'::timestamp without time zone) END ('2008-04-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080407 START ('2008-04-07 00:00:00'::timestamp without time zone) END ('2008-04-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080408 START ('2008-04-08 00:00:00'::timestamp without time zone) END ('2008-04-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080409 START ('2008-04-09 00:00:00'::timestamp without time zone) END ('2008-04-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080410 START ('2008-04-10 00:00:00'::timestamp without time zone) END ('2008-04-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080411 START ('2008-04-11 00:00:00'::timestamp without time zone) END ('2008-04-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080414 START ('2008-04-14 00:00:00'::timestamp without time zone) END ('2008-04-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080415 START ('2008-04-15 00:00:00'::timestamp without time zone) END ('2008-04-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080416 START ('2008-04-16 00:00:00'::timestamp without time zone) END ('2008-04-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080417 START ('2008-04-17 00:00:00'::timestamp without time zone) END ('2008-04-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080418 START ('2008-04-18 00:00:00'::timestamp without time zone) END ('2008-04-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20080421 START ('2008-04-21 00:00:00'::timestamp without time zone) END ('2008-04-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080422 START ('2008-04-22 00:00:00'::timestamp without time zone) END ('2008-04-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080423 START ('2008-04-23 00:00:00'::timestamp without time zone) END ('2008-04-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080424 START ('2008-04-24 00:00:00'::timestamp without time zone) END ('2008-04-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080425 START ('2008-04-25 00:00:00'::timestamp without time zone) END ('2008-04-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080428 START ('2008-04-28 00:00:00'::timestamp without time zone) END ('2008-04-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080429 START ('2008-04-29 00:00:00'::timestamp without time zone) END ('2008-04-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080430 START ('2008-04-30 00:00:00'::timestamp without time zone) END ('2008-05-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20080501 START ('2008-05-01 00:00:00'::timestamp without time zone) END ('2008-05-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080502 START ('2008-05-02 00:00:00'::timestamp without time zone) END ('2008-05-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080505 START ('2008-05-05 00:00:00'::timestamp without time zone) END ('2008-05-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20080506 START ('2008-05-06 00:00:00'::timestamp without time zone) END ('2008-05-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080507 START ('2008-05-07 00:00:00'::timestamp without time zone) END ('2008-05-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080508 START ('2008-05-08 00:00:00'::timestamp without time zone) END ('2008-05-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080509 START ('2008-05-09 00:00:00'::timestamp without time zone) END ('2008-05-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080512 START ('2008-05-12 00:00:00'::timestamp without time zone) END ('2008-05-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20080513 START ('2008-05-13 00:00:00'::timestamp without time zone) END ('2008-05-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080514 START ('2008-05-14 00:00:00'::timestamp without time zone) END ('2008-05-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080515 START ('2008-05-15 00:00:00'::timestamp without time zone) END ('2008-05-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080516 START ('2008-05-16 00:00:00'::timestamp without time zone) END ('2008-05-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080519 START ('2008-05-19 00:00:00'::timestamp without time zone) END ('2008-05-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20080520 START ('2008-05-20 00:00:00'::timestamp without time zone) END ('2008-05-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20080521 START ('2008-05-21 00:00:00'::timestamp without time zone) END ('2008-05-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080522 START ('2008-05-22 00:00:00'::timestamp without time zone) END ('2008-05-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080523 START ('2008-05-23 00:00:00'::timestamp without time zone) END ('2008-05-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20080527 START ('2008-05-27 00:00:00'::timestamp without time zone) END ('2008-05-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080528 START ('2008-05-28 00:00:00'::timestamp without time zone) END ('2008-05-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080529 START ('2008-05-29 00:00:00'::timestamp without time zone) END ('2008-05-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080530 START ('2008-05-30 00:00:00'::timestamp without time zone) END ('2008-06-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080602 START ('2008-06-02 00:00:00'::timestamp without time zone) END ('2008-06-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080603 START ('2008-06-03 00:00:00'::timestamp without time zone) END ('2008-06-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080604 START ('2008-06-04 00:00:00'::timestamp without time zone) END ('2008-06-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080605 START ('2008-06-05 00:00:00'::timestamp without time zone) END ('2008-06-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20080606 START ('2008-06-06 00:00:00'::timestamp without time zone) END ('2008-06-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080609 START ('2008-06-09 00:00:00'::timestamp without time zone) END ('2008-06-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080610 START ('2008-06-10 00:00:00'::timestamp without time zone) END ('2008-06-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080611 START ('2008-06-11 00:00:00'::timestamp without time zone) END ('2008-06-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080612 START ('2008-06-12 00:00:00'::timestamp without time zone) END ('2008-06-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20080613 START ('2008-06-13 00:00:00'::timestamp without time zone) END ('2008-06-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080616 START ('2008-06-16 00:00:00'::timestamp without time zone) END ('2008-06-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080617 START ('2008-06-17 00:00:00'::timestamp without time zone) END ('2008-06-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080618 START ('2008-06-18 00:00:00'::timestamp without time zone) END ('2008-06-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080619 START ('2008-06-19 00:00:00'::timestamp without time zone) END ('2008-06-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20080620 START ('2008-06-20 00:00:00'::timestamp without time zone) END ('2008-06-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080623 START ('2008-06-23 00:00:00'::timestamp without time zone) END ('2008-06-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080624 START ('2008-06-24 00:00:00'::timestamp without time zone) END ('2008-06-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080625 START ('2008-06-25 00:00:00'::timestamp without time zone) END ('2008-06-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20080626 START ('2008-06-26 00:00:00'::timestamp without time zone) END ('2008-06-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20080627 START ('2008-06-27 00:00:00'::timestamp without time zone) END ('2008-06-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080630 START ('2008-06-30 00:00:00'::timestamp without time zone) END ('2008-07-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20080701 START ('2008-07-01 00:00:00'::timestamp without time zone) END ('2008-07-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080702 START ('2008-07-02 00:00:00'::timestamp without time zone) END ('2008-07-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080703 START ('2008-07-03 00:00:00'::timestamp without time zone) END ('2008-07-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080707 START ('2008-07-07 00:00:00'::timestamp without time zone) END ('2008-07-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080708 START ('2008-07-08 00:00:00'::timestamp without time zone) END ('2008-07-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080709 START ('2008-07-09 00:00:00'::timestamp without time zone) END ('2008-07-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080710 START ('2008-07-10 00:00:00'::timestamp without time zone) END ('2008-07-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080711 START ('2008-07-11 00:00:00'::timestamp without time zone) END ('2008-07-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080714 START ('2008-07-14 00:00:00'::timestamp without time zone) END ('2008-07-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080715 START ('2008-07-15 00:00:00'::timestamp without time zone) END ('2008-07-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080716 START ('2008-07-16 00:00:00'::timestamp without time zone) END ('2008-07-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080717 START ('2008-07-17 00:00:00'::timestamp without time zone) END ('2008-07-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080718 START ('2008-07-18 00:00:00'::timestamp without time zone) END ('2008-07-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20080721 START ('2008-07-21 00:00:00'::timestamp without time zone) END ('2008-07-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080722 START ('2008-07-22 00:00:00'::timestamp without time zone) END ('2008-07-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080723 START ('2008-07-23 00:00:00'::timestamp without time zone) END ('2008-07-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080724 START ('2008-07-24 00:00:00'::timestamp without time zone) END ('2008-07-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080725 START ('2008-07-25 00:00:00'::timestamp without time zone) END ('2008-07-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080728 START ('2008-07-28 00:00:00'::timestamp without time zone) END ('2008-07-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080729 START ('2008-07-29 00:00:00'::timestamp without time zone) END ('2008-07-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080730 START ('2008-07-30 00:00:00'::timestamp without time zone) END ('2008-07-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20080731 START ('2008-07-31 00:00:00'::timestamp without time zone) END ('2008-08-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20080801 START ('2008-08-01 00:00:00'::timestamp without time zone) END ('2008-08-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080804 START ('2008-08-04 00:00:00'::timestamp without time zone) END ('2008-08-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080805 START ('2008-08-05 00:00:00'::timestamp without time zone) END ('2008-08-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20080806 START ('2008-08-06 00:00:00'::timestamp without time zone) END ('2008-08-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20080807 START ('2008-08-07 00:00:00'::timestamp without time zone) END ('2008-08-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080808 START ('2008-08-08 00:00:00'::timestamp without time zone) END ('2008-08-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080811 START ('2008-08-11 00:00:00'::timestamp without time zone) END ('2008-08-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080812 START ('2008-08-12 00:00:00'::timestamp without time zone) END ('2008-08-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20080813 START ('2008-08-13 00:00:00'::timestamp without time zone) END ('2008-08-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20080814 START ('2008-08-14 00:00:00'::timestamp without time zone) END ('2008-08-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080815 START ('2008-08-15 00:00:00'::timestamp without time zone) END ('2008-08-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080818 START ('2008-08-18 00:00:00'::timestamp without time zone) END ('2008-08-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080819 START ('2008-08-19 00:00:00'::timestamp without time zone) END ('2008-08-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20080820 START ('2008-08-20 00:00:00'::timestamp without time zone) END ('2008-08-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20080821 START ('2008-08-21 00:00:00'::timestamp without time zone) END ('2008-08-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080822 START ('2008-08-22 00:00:00'::timestamp without time zone) END ('2008-08-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080825 START ('2008-08-25 00:00:00'::timestamp without time zone) END ('2008-08-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20080826 START ('2008-08-26 00:00:00'::timestamp without time zone) END ('2008-08-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20080827 START ('2008-08-27 00:00:00'::timestamp without time zone) END ('2008-08-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20080828 START ('2008-08-28 00:00:00'::timestamp without time zone) END ('2008-08-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080829 START ('2008-08-29 00:00:00'::timestamp without time zone) END ('2008-09-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20080902 START ('2008-09-02 00:00:00'::timestamp without time zone) END ('2008-09-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20080903 START ('2008-09-03 00:00:00'::timestamp without time zone) END ('2008-09-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20080904 START ('2008-09-04 00:00:00'::timestamp without time zone) END ('2008-09-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20080905 START ('2008-09-05 00:00:00'::timestamp without time zone) END ('2008-09-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20080908 START ('2008-09-08 00:00:00'::timestamp without time zone) END ('2008-09-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20080909 START ('2008-09-09 00:00:00'::timestamp without time zone) END ('2008-09-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20080910 START ('2008-09-10 00:00:00'::timestamp without time zone) END ('2008-09-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20080911 START ('2008-09-11 00:00:00'::timestamp without time zone) END ('2008-09-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20080912 START ('2008-09-12 00:00:00'::timestamp without time zone) END ('2008-09-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20080915 START ('2008-09-15 00:00:00'::timestamp without time zone) END ('2008-09-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20080916 START ('2008-09-16 00:00:00'::timestamp without time zone) END ('2008-09-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20080917 START ('2008-09-17 00:00:00'::timestamp without time zone) END ('2008-09-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20080918 START ('2008-09-18 00:00:00'::timestamp without time zone) END ('2008-09-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20080919 START ('2008-09-19 00:00:00'::timestamp without time zone) END ('2008-09-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20080922 START ('2008-09-22 00:00:00'::timestamp without time zone) END ('2008-09-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20080923 START ('2008-09-23 00:00:00'::timestamp without time zone) END ('2008-09-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20080924 START ('2008-09-24 00:00:00'::timestamp without time zone) END ('2008-09-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20080925 START ('2008-09-25 00:00:00'::timestamp without time zone) END ('2008-09-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20080926 START ('2008-09-26 00:00:00'::timestamp without time zone) END ('2008-09-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20080929 START ('2008-09-29 00:00:00'::timestamp without time zone) END ('2008-09-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20080930 START ('2008-09-30 00:00:00'::timestamp without time zone) END ('2008-10-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20081001 START ('2008-10-01 00:00:00'::timestamp without time zone) END ('2008-10-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20081002 START ('2008-10-02 00:00:00'::timestamp without time zone) END ('2008-10-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20081003 START ('2008-10-03 00:00:00'::timestamp without time zone) END ('2008-10-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20081006 START ('2008-10-06 00:00:00'::timestamp without time zone) END ('2008-10-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20081007 START ('2008-10-07 00:00:00'::timestamp without time zone) END ('2008-10-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20081008 START ('2008-10-08 00:00:00'::timestamp without time zone) END ('2008-10-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20081009 START ('2008-10-09 00:00:00'::timestamp without time zone) END ('2008-10-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20081010 START ('2008-10-10 00:00:00'::timestamp without time zone) END ('2008-10-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20081013 START ('2008-10-13 00:00:00'::timestamp without time zone) END ('2008-10-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20081014 START ('2008-10-14 00:00:00'::timestamp without time zone) END ('2008-10-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20081015 START ('2008-10-15 00:00:00'::timestamp without time zone) END ('2008-10-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20081016 START ('2008-10-16 00:00:00'::timestamp without time zone) END ('2008-10-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20081017 START ('2008-10-17 00:00:00'::timestamp without time zone) END ('2008-10-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20081020 START ('2008-10-20 00:00:00'::timestamp without time zone) END ('2008-10-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20081021 START ('2008-10-21 00:00:00'::timestamp without time zone) END ('2008-10-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20081022 START ('2008-10-22 00:00:00'::timestamp without time zone) END ('2008-10-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20081023 START ('2008-10-23 00:00:00'::timestamp without time zone) END ('2008-10-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20081024 START ('2008-10-24 00:00:00'::timestamp without time zone) END ('2008-10-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20081027 START ('2008-10-27 00:00:00'::timestamp without time zone) END ('2008-10-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20081028 START ('2008-10-28 00:00:00'::timestamp without time zone) END ('2008-10-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20081029 START ('2008-10-29 00:00:00'::timestamp without time zone) END ('2008-10-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20081030 START ('2008-10-30 00:00:00'::timestamp without time zone) END ('2008-10-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20081031 START ('2008-10-31 00:00:00'::timestamp without time zone) END ('2008-11-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20081103 START ('2008-11-03 00:00:00'::timestamp without time zone) END ('2008-11-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20081104 START ('2008-11-04 00:00:00'::timestamp without time zone) END ('2008-11-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20081105 START ('2008-11-05 00:00:00'::timestamp without time zone) END ('2008-11-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20081106 START ('2008-11-06 00:00:00'::timestamp without time zone) END ('2008-11-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20081107 START ('2008-11-07 00:00:00'::timestamp without time zone) END ('2008-11-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20081110 START ('2008-11-10 00:00:00'::timestamp without time zone) END ('2008-11-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20081111 START ('2008-11-11 00:00:00'::timestamp without time zone) END ('2008-11-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20081112 START ('2008-11-12 00:00:00'::timestamp without time zone) END ('2008-11-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20081113 START ('2008-11-13 00:00:00'::timestamp without time zone) END ('2008-11-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20081114 START ('2008-11-14 00:00:00'::timestamp without time zone) END ('2008-11-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20081117 START ('2008-11-17 00:00:00'::timestamp without time zone) END ('2008-11-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20081118 START ('2008-11-18 00:00:00'::timestamp without time zone) END ('2008-11-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20081119 START ('2008-11-19 00:00:00'::timestamp without time zone) END ('2008-11-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20081120 START ('2008-11-20 00:00:00'::timestamp without time zone) END ('2008-11-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20081121 START ('2008-11-21 00:00:00'::timestamp without time zone) END ('2008-11-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20081124 START ('2008-11-24 00:00:00'::timestamp without time zone) END ('2008-11-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20081125 START ('2008-11-25 00:00:00'::timestamp without time zone) END ('2008-11-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20081126 START ('2008-11-26 00:00:00'::timestamp without time zone) END ('2008-11-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20081128 START ('2008-11-28 00:00:00'::timestamp without time zone) END ('2008-12-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20081201 START ('2008-12-01 00:00:00'::timestamp without time zone) END ('2008-12-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20081202 START ('2008-12-02 00:00:00'::timestamp without time zone) END ('2008-12-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20081203 START ('2008-12-03 00:00:00'::timestamp without time zone) END ('2008-12-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20081204 START ('2008-12-04 00:00:00'::timestamp without time zone) END ('2008-12-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20081205 START ('2008-12-05 00:00:00'::timestamp without time zone) END ('2008-12-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20081208 START ('2008-12-08 00:00:00'::timestamp without time zone) END ('2008-12-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20081209 START ('2008-12-09 00:00:00'::timestamp without time zone) END ('2008-12-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20081210 START ('2008-12-10 00:00:00'::timestamp without time zone) END ('2008-12-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20081211 START ('2008-12-11 00:00:00'::timestamp without time zone) END ('2008-12-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20081212 START ('2008-12-12 00:00:00'::timestamp without time zone) END ('2008-12-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20081215 START ('2008-12-15 00:00:00'::timestamp without time zone) END ('2008-12-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20081216 START ('2008-12-16 00:00:00'::timestamp without time zone) END ('2008-12-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20081217 START ('2008-12-17 00:00:00'::timestamp without time zone) END ('2008-12-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20081218 START ('2008-12-18 00:00:00'::timestamp without time zone) END ('2008-12-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20081219 START ('2008-12-19 00:00:00'::timestamp without time zone) END ('2008-12-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20081222 START ('2008-12-22 00:00:00'::timestamp without time zone) END ('2008-12-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20081223 START ('2008-12-23 00:00:00'::timestamp without time zone) END ('2008-12-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20081224 START ('2008-12-24 00:00:00'::timestamp without time zone) END ('2008-12-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20081226 START ('2008-12-26 00:00:00'::timestamp without time zone) END ('2008-12-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20081229 START ('2008-12-29 00:00:00'::timestamp without time zone) END ('2008-12-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20081230 START ('2008-12-30 00:00:00'::timestamp without time zone) END ('2008-12-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20081231 START ('2008-12-31 00:00:00'::timestamp without time zone) END ('2009-01-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20090102 START ('2009-01-02 00:00:00'::timestamp without time zone) END ('2009-01-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20090105 START ('2009-01-05 00:00:00'::timestamp without time zone) END ('2009-01-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20090106 START ('2009-01-06 00:00:00'::timestamp without time zone) END ('2009-01-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20090107 START ('2009-01-07 00:00:00'::timestamp without time zone) END ('2009-01-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20090108 START ('2009-01-08 00:00:00'::timestamp without time zone) END ('2009-01-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20090109 START ('2009-01-09 00:00:00'::timestamp without time zone) END ('2009-01-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20090112 START ('2009-01-12 00:00:00'::timestamp without time zone) END ('2009-01-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20090113 START ('2009-01-13 00:00:00'::timestamp without time zone) END ('2009-01-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20090114 START ('2009-01-14 00:00:00'::timestamp without time zone) END ('2009-01-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20090115 START ('2009-01-15 00:00:00'::timestamp without time zone) END ('2009-01-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20090116 START ('2009-01-16 00:00:00'::timestamp without time zone) END ('2009-01-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20090120 START ('2009-01-20 00:00:00'::timestamp without time zone) END ('2009-01-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20090121 START ('2009-01-21 00:00:00'::timestamp without time zone) END ('2009-01-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20090122 START ('2009-01-22 00:00:00'::timestamp without time zone) END ('2009-01-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20090123 START ('2009-01-23 00:00:00'::timestamp without time zone) END ('2009-01-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20090126 START ('2009-01-26 00:00:00'::timestamp without time zone) END ('2009-01-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20090127 START ('2009-01-27 00:00:00'::timestamp without time zone) END ('2009-01-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20090128 START ('2009-01-28 00:00:00'::timestamp without time zone) END ('2009-01-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20090129 START ('2009-01-29 00:00:00'::timestamp without time zone) END ('2009-01-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20090130 START ('2009-01-30 00:00:00'::timestamp without time zone) END ('2009-02-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20090202 START ('2009-02-02 00:00:00'::timestamp without time zone) END ('2009-02-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20090203 START ('2009-02-03 00:00:00'::timestamp without time zone) END ('2009-02-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20090204 START ('2009-02-04 00:00:00'::timestamp without time zone) END ('2009-02-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20090205 START ('2009-02-05 00:00:00'::timestamp without time zone) END ('2009-02-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20090206 START ('2009-02-06 00:00:00'::timestamp without time zone) END ('2009-02-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20090209 START ('2009-02-09 00:00:00'::timestamp without time zone) END ('2009-02-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20090210 START ('2009-02-10 00:00:00'::timestamp without time zone) END ('2009-02-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20090211 START ('2009-02-11 00:00:00'::timestamp without time zone) END ('2009-02-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20090212 START ('2009-02-12 00:00:00'::timestamp without time zone) END ('2009-02-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20090213 START ('2009-02-13 00:00:00'::timestamp without time zone) END ('2009-02-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20090217 START ('2009-02-17 00:00:00'::timestamp without time zone) END ('2009-02-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20090218 START ('2009-02-18 00:00:00'::timestamp without time zone) END ('2009-02-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20090219 START ('2009-02-19 00:00:00'::timestamp without time zone) END ('2009-02-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20090220 START ('2009-02-20 00:00:00'::timestamp without time zone) END ('2009-02-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20090223 START ('2009-02-23 00:00:00'::timestamp without time zone) END ('2009-02-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20090224 START ('2009-02-24 00:00:00'::timestamp without time zone) END ('2009-02-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20090225 START ('2009-02-25 00:00:00'::timestamp without time zone) END ('2009-02-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20090226 START ('2009-02-26 00:00:00'::timestamp without time zone) END ('2009-02-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20090227 START ('2009-02-27 00:00:00'::timestamp without time zone) END ('2009-03-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20090302 START ('2009-03-02 00:00:00'::timestamp without time zone) END ('2009-03-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20090303 START ('2009-03-03 00:00:00'::timestamp without time zone) END ('2009-03-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20090304 START ('2009-03-04 00:00:00'::timestamp without time zone) END ('2009-03-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20090305 START ('2009-03-05 00:00:00'::timestamp without time zone) END ('2009-03-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20090306 START ('2009-03-06 00:00:00'::timestamp without time zone) END ('2009-03-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20090309 START ('2009-03-09 00:00:00'::timestamp without time zone) END ('2009-03-10 00:00:00'::timestamp without
 time zone),
          PARTITION d20090310 START ('2009-03-10 00:00:00'::timestamp without time zone) END ('2009-03-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20090311 START ('2009-03-11 00:00:00'::timestamp without time zone) END ('2009-03-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20090312 START ('2009-03-12 00:00:00'::timestamp without time zone) END ('2009-03-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20090313 START ('2009-03-13 00:00:00'::timestamp without time zone) END ('2009-03-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20090316 START ('2009-03-16 00:00:00'::timestamp without time zone) END ('2009-03-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20090317 START ('2009-03-17 00:00:00'::timestamp without time zone) END ('2009-03-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20090318 START ('2009-03-18 00:00:00'::timestamp without time zone) END ('2009-03-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20090319 START ('2009-03-19 00:00:00'::timestamp without time zone) END ('2009-03-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20090320 START ('2009-03-20 00:00:00'::timestamp without time zone) END ('2009-03-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20090323 START ('2009-03-23 00:00:00'::timestamp without time zone) END ('2009-03-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20090324 START ('2009-03-24 00:00:00'::timestamp without time zone) END ('2009-03-25 00:00:00'::timestamp without
 time zone),
          PARTITION d20090325 START ('2009-03-25 00:00:00'::timestamp without time zone) END ('2009-03-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20090326 START ('2009-03-26 00:00:00'::timestamp without time zone) END ('2009-03-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20090327 START ('2009-03-27 00:00:00'::timestamp without time zone) END ('2009-03-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20090330 START ('2009-03-30 00:00:00'::timestamp without time zone) END ('2009-03-31 00:00:00'::timestamp without
 time zone),
          PARTITION d20090331 START ('2009-03-31 00:00:00'::timestamp without time zone) END ('2009-04-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20090401 START ('2009-04-01 00:00:00'::timestamp without time zone) END ('2009-04-02 00:00:00'::timestamp without
 time zone),
          PARTITION d20090402 START ('2009-04-02 00:00:00'::timestamp without time zone) END ('2009-04-03 00:00:00'::timestamp without
 time zone),
          PARTITION d20090403 START ('2009-04-03 00:00:00'::timestamp without time zone) END ('2009-04-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20090406 START ('2009-04-06 00:00:00'::timestamp without time zone) END ('2009-04-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20090407 START ('2009-04-07 00:00:00'::timestamp without time zone) END ('2009-04-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20090408 START ('2009-04-08 00:00:00'::timestamp without time zone) END ('2009-04-09 00:00:00'::timestamp without
 time zone),
          PARTITION d20090409 START ('2009-04-09 00:00:00'::timestamp without time zone) END ('2009-04-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20090413 START ('2009-04-13 00:00:00'::timestamp without time zone) END ('2009-04-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20090414 START ('2009-04-14 00:00:00'::timestamp without time zone) END ('2009-04-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20090415 START ('2009-04-15 00:00:00'::timestamp without time zone) END ('2009-04-16 00:00:00'::timestamp without
 time zone),
          PARTITION d20090416 START ('2009-04-16 00:00:00'::timestamp without time zone) END ('2009-04-17 00:00:00'::timestamp without
 time zone),
          PARTITION d20090417 START ('2009-04-17 00:00:00'::timestamp without time zone) END ('2009-04-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20090420 START ('2009-04-20 00:00:00'::timestamp without time zone) END ('2009-04-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20090421 START ('2009-04-21 00:00:00'::timestamp without time zone) END ('2009-04-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20090422 START ('2009-04-22 00:00:00'::timestamp without time zone) END ('2009-04-23 00:00:00'::timestamp without
 time zone),
          PARTITION d20090423 START ('2009-04-23 00:00:00'::timestamp without time zone) END ('2009-04-24 00:00:00'::timestamp without
 time zone),
          PARTITION d20090424 START ('2009-04-24 00:00:00'::timestamp without time zone) END ('2009-04-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20090427 START ('2009-04-27 00:00:00'::timestamp without time zone) END ('2009-04-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20090428 START ('2009-04-28 00:00:00'::timestamp without time zone) END ('2009-04-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20090429 START ('2009-04-29 00:00:00'::timestamp without time zone) END ('2009-04-30 00:00:00'::timestamp without
 time zone),
          PARTITION d20090430 START ('2009-04-30 00:00:00'::timestamp without time zone) END ('2009-05-01 00:00:00'::timestamp without
 time zone),
          PARTITION d20090501 START ('2009-05-01 00:00:00'::timestamp without time zone) END ('2009-05-04 00:00:00'::timestamp without
 time zone),
          PARTITION d20090504 START ('2009-05-04 00:00:00'::timestamp without time zone) END ('2009-05-05 00:00:00'::timestamp without
 time zone),
          PARTITION d20090505 START ('2009-05-05 00:00:00'::timestamp without time zone) END ('2009-05-06 00:00:00'::timestamp without
 time zone),
          PARTITION d20090506 START ('2009-05-06 00:00:00'::timestamp without time zone) END ('2009-05-07 00:00:00'::timestamp without
 time zone),
          PARTITION d20090507 START ('2009-05-07 00:00:00'::timestamp without time zone) END ('2009-05-08 00:00:00'::timestamp without
 time zone),
          PARTITION d20090508 START ('2009-05-08 00:00:00'::timestamp without time zone) END ('2009-05-11 00:00:00'::timestamp without
 time zone),
          PARTITION d20090511 START ('2009-05-11 00:00:00'::timestamp without time zone) END ('2009-05-12 00:00:00'::timestamp without
 time zone),
          PARTITION d20090512 START ('2009-05-12 00:00:00'::timestamp without time zone) END ('2009-05-13 00:00:00'::timestamp without
 time zone),
          PARTITION d20090513 START ('2009-05-13 00:00:00'::timestamp without time zone) END ('2009-05-14 00:00:00'::timestamp without
 time zone),
          PARTITION d20090514 START ('2009-05-14 00:00:00'::timestamp without time zone) END ('2009-05-15 00:00:00'::timestamp without
 time zone),
          PARTITION d20090515 START ('2009-05-15 00:00:00'::timestamp without time zone) END ('2009-05-18 00:00:00'::timestamp without
 time zone),
          PARTITION d20090518 START ('2009-05-18 00:00:00'::timestamp without time zone) END ('2009-05-19 00:00:00'::timestamp without
 time zone),
          PARTITION d20090519 START ('2009-05-19 00:00:00'::timestamp without time zone) END ('2009-05-20 00:00:00'::timestamp without
 time zone),
          PARTITION d20090520 START ('2009-05-20 00:00:00'::timestamp without time zone) END ('2009-05-21 00:00:00'::timestamp without
 time zone),
          PARTITION d20090521 START ('2009-05-21 00:00:00'::timestamp without time zone) END ('2009-05-22 00:00:00'::timestamp without
 time zone),
          PARTITION d20090522 START ('2009-05-22 00:00:00'::timestamp without time zone) END ('2009-05-26 00:00:00'::timestamp without
 time zone),
          PARTITION d20090526 START ('2009-05-26 00:00:00'::timestamp without time zone) END ('2009-05-27 00:00:00'::timestamp without
 time zone),
          PARTITION d20090527 START ('2009-05-27 00:00:00'::timestamp without time zone) END ('2009-05-28 00:00:00'::timestamp without
 time zone),
          PARTITION d20090528 START ('2009-05-28 00:00:00'::timestamp without time zone) END ('2009-05-29 00:00:00'::timestamp without
 time zone),
          PARTITION d20090529 START ('2009-05-29 00:00:00'::timestamp without time zone) END ('2009-06-01 00:00:00'::timestamp without
 time zone)
          );

set statement_mem='400MB';

/* Formatted on 12/22/2009 4:33:26 PM (QP5 v5.115.810.9015) */
SELECT                                           /*greenplum needs this cast*/
      CAST ('Q' AS VARCHAR (1)) AS mkt,          /*greenplum needs this cast*/
       CAST ('SBESO' AS VARCHAR (10))
          AS trns_type,
       TO_CHAR (fact.trd_dt, 'DD-Mon-YYYY') AS event_dt,
       fact.odr_updt_tm AS event_start_tm,
       fact.issue_sym_id AS sym,
       fact.msg_seq_nb AS seq_num,
       fact.odr_rfrnc_nb AS odr_rfrnc_nb,
       fact.msg_type_cd AS msg_type,                         /*fact.entry_dt*/
       TO_CHAR (fact.entry_dt, 'DD-Mon-YYYY') AS entry_dt, /*fact.odr_entry_dt*/
       TO_CHAR (fact.odr_entry_dt, 'DD-Mon-YYYY')
          AS odr_entry_dt,
       fact.odr_entry_tm AS odr_entry_tm,
       fact.msg_ctgry_cd AS msg_category,
       fact.trans_cd AS trans_cd,
       fact.odr_st AS odr_st,
       fact.odr_ctgry_cd AS odr_ctgry_cd,
       fact.oe_mp_id AS oe_mpid,
       fact.side_cd AS side_cd,
       fact.shrt_sale_cd AS shrt_sale_cd,
       fact.entrd_pr AS entered_pr,
       fact.mkt_odr_fl AS mkt_odr_fl,
       fact.odr_entry_qt AS odr_entry_qt,
       fact.odr_qt AS odr_qt,
       fact.oe_cpcty_cd AS oe_capacity,
       fact.atrbl_odr_fl AS attributable,
       fact.tif_cd AS tif_cd,
       fact.user_odr_id AS usr_odr_id,                    /*fact.odr_rank_dt*/
       TO_CHAR (fact.odr_rank_dt, 'DD-Mon-YYYY')
          AS odr_rank_dt,
       fact.odr_rank_tm AS odr_rank_tm,
       fact.exctn_rfrnc_nb AS exctn_rfrnc_nb,
       fact.exctn_qt AS exctn_qty,
       fact.cncl_qt AS cncl_qty,
       fact.peg_type_cd AS peg_type,
       fact.peg_offst_pr AS peg_offset_pr,
       fact.peg_cap_pr AS peg_cap_pr,
       fact.dcrny_odr_fl AS dcrny_odr_fl,
       fact.dcrny_offst_pr AS dcrny_offset_pr,
       fact.clsg_cross_elgbl_fl AS clsng_cross_elgbl,
       fact.opng_cross_elgbl_fl AS opng_cross_elgbl,
       fact.rtng_fl AS routing_fl,
       fact.oe_prmry_mp_id AS oe_prmry_mpid,
       fact.oe_prmry_mpid_nb AS oe_prmry_mpid__,
       fact.odr_rsrv_size_qt AS rsrv_sz,
       fact.rndd_odr_pr AS rnd_prc,
       fact.ads_smntg_odr_id AS rec_id,
       fact.core_actn_cd AS core_actn_cd,
       fact.core_link_id AS core_link_id,
       fact.core_seq_nb AS core_seq_num,
       fact.core_updt_tm AS core_updt_tm,
       fact.cross_fl AS cross_fl,
       fact.host_updt_tm AS host_updt_tm,
       fact.iso_fl AS iso_fl,
       fact.rtng_seq_nb AS rtng_seq_num
FROM /*CR 111848 Greenplum 3.3 needs the specific OR condition in order to trigger correct index usage */
     /*keep_fL is derived here since using the odr_entry_dt directly in the join condition seems to invalidate proper index selection in Greenplum 3.3*/
     (SELECT *
      FROM (SELECT smntg_odr.*,
                   CASE
                      WHEN smntg_odr.odr_entry_dt =
                              ola.odr_entry_dt
                      THEN
                         1
                      ELSE
                         0
                   END
                      AS keep_fl
            FROM smntg_odr,
                 (SELECT DISTINCT
                         m.odr_rfrnc_nb,
                         odr_entry_dt
                  FROM wiat_ola_tree t,
                       rtng_mtch m
                  WHERE m.rtng_order_roe_id =
                           t.child_id
                        AND t.ola_tree_id =
                              101153
                        AND m.dstnt_cd IN
                                 ('XQ',
                                  'U'))
                 ola
            WHERE (smntg_odr.odr_rfrnc_nb =
                      'Gp3.3Fix'
                   OR smntg_odr.odr_rfrnc_nb =
                        ola.odr_rfrnc_nb))
           a
      WHERE keep_fl =
               1)
     fact
WHERE                                                            /*SPLITDATE*/
      (                                  /*_DS_NAME is : ads2Gp33DataSource */
       fact.trd_dt <
          '01-Jun-2009')
ORDER BY fact.trd_dt ASC, fact.odr_updt_tm ASC, fact.msg_seq_nb ASC
;

drop table smntg_odr;
drop table wiat_ola_tree;
drop table rtng_mtch;
