--
-- Greenplum Database database dump
--
drop schema if exists gpdemo cascade;
psql:/path/sql_file:1: NOTICE:  drop cascades to table gpdemo.plr_mdls
psql:/path/sql_file:1: NOTICE:  drop cascades to type gpdemo.glm_result_type
psql:/path/sql_file:1: NOTICE:  drop cascades to table gpdemo.patient_history_test
psql:/path/sql_file:1: NOTICE:  drop cascades to table gpdemo.patient_history_train
psql:/path/sql_file:1: NOTICE:  drop cascades to table gpdemo.patient_history_all
DROP SCHEMA
create schema gpdemo;
CREATE SCHEMA
set search_path = gpdemo, public, pg_catalog;
SET
SET statement_timeout = 0;
SET
SET client_encoding = 'UTF8';
SET
SET standard_conforming_strings = off;
SET
SET check_function_bodies = false;
SET
SET client_min_messages = warning;
SET
SET escape_string_warning = off;
SET
SET default_tablespace = '';
SET
SET default_with_oids = false;
SET
--
-- Name: patient_history_all; Type: TABLE; Schema: public; Owner: @user@; Tablespace: 
--
CREATE TABLE patient_history_all (
    patientid bigint,
    age integer,
    gender text,
    race text,
    marital_status text,
    bmi double precision,
    smoker integer,
    income_level text,
    employed integer,
    education_level text,
    number_of_kids integer,
    retired integer,
    nbr_of_hospitalization_last12mos integer,
    med_cond1 integer,
    med_cond2 integer,
    med_cond3 integer,
    med_cond4 integer,
    med_cond5 integer,
    med_cond6 integer,
    med_cond7 integer,
    surgery_place_code text,
    diagnosis_code text,
    los_last12mos integer,
    treatment_code text,
    infection integer,
    infection_cost double precision,
    temp_for_splitting double precision
) DISTRIBUTED BY (patientid);
CREATE TABLE
ALTER TABLE gpdemo.patient_history_all OWNER TO @user@;
ALTER TABLE
--
-- Data for Name: patient_history_all; Type: TABLE DATA; Schema: public; Owner: @user@
--
COPY patient_history_all (patientid, age, gender, race, marital_status, bmi, smoker, income_level, employed, education_level, number_of_kids, retired, nbr_of_hospitalization_last12mos, med_cond1, med_cond2, med_cond3, med_cond4, med_cond5, med_cond6, med_cond7, surgery_place_code, diagnosis_code, los_last12mos, treatment_code, infection, infection_cost, temp_for_splitting) FROM stdin;
--
-- Greenplum Database database dump complete
--
