-- start_ignore
--
-- Greenplum Database database dump
--

SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET default_with_oids = false;

--
-- Name: SCHEMA public; Type: COMMENT; Schema: -; Owner: gpadmin
--

COMMENT ON SCHEMA public IS 'Standard public schema';


--
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: gpadmin
--

CREATE LANGUAGE plpgsql;

SET search_path = public, pg_catalog;

--
-- Name: finbint(bigint); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION finbint(pbint bigint) RETURNS bigint
    AS $$
begin
	select PBINT;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: finchar(character); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION finchar(pchar character) RETURNS character
    AS $$
begin
	select PCHAR;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: finclob(text); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION finclob(pclob text) RETURNS text
    AS $$
begin
	select PCLOB;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: findbl(double precision); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION findbl(pdbl double precision) RETURNS double precision
    AS $$
begin
	select PDBL;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: findec(numeric); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION findec(pdec numeric) RETURNS numeric
    AS $$
begin
	select PDEC;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: findt(date); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION findt(pdt date) RETURNS date
    AS $$
begin
	select PDT;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: finflt(double precision); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION finflt(pflt double precision) RETURNS double precision
    AS $$
begin
	select PFLT;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: finint(integer); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION finint(pint integer) RETURNS integer
    AS $$
begin
	select PINT;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: finnum(numeric); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION finnum(pnum numeric) RETURNS numeric
    AS $$
begin
	select PNUM;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: finrl(real); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION finrl(prl real) RETURNS real
    AS $$
begin
	select PRL;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: finsint(smallint); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION finsint(psint smallint) RETURNS smallint
    AS $$
begin
	select PSINT;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fintm(time without time zone); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fintm(ptm time without time zone) RETURNS time without time zone
    AS $$
begin
	select PTM;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fints(timestamp without time zone); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fints(pts timestamp without time zone) RETURNS timestamp without time zone
    AS $$
begin
	select PTS;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: finvchar(character varying); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION finvchar(pvchar character varying) RETURNS character varying
    AS $$
begin
	select PVCHAR;
end;
$$
    LANGUAGE plpgsql;



SET default_tablespace = '';

--
-- Name: tset1; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tset1 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(3)
) DISTRIBUTED BY (rnum);



--
-- Name: fovtbint(bigint, bigint); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtbint(pbint bigint, p2bint bigint) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtchar(character, character); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtchar(pchar character, p2char character) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtclob(text, text); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtclob(pclob text, p2clob text) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtdbl(double precision, double precision); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtdbl(pdbl double precision, p2dbl double precision) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtdec(numeric, numeric); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtdec(pdec numeric, p2dec numeric) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtdt(date, date); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtdt(pdt date, p2dt date) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtflt(double precision, double precision); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtflt(pflt double precision, p2flt double precision) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtint(integer, integer); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtint(pint integer, p2int integer) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtnum(numeric, numeric); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtnum(pnum numeric, p2num numeric) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtrl(real, real); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtrl(prl real, p2rl real) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtsint(smallint, smallint); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtsint(psint smallint, p2sint smallint) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovttm(time without time zone, time without time zone); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovttm(ptm time without time zone, p2tm time without time zone) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtts(timestamp without time zone, timestamp without time zone); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtts(pts timestamp without time zone, p2ts timestamp without time zone) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fovtvchar(character varying, character varying); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fovtvchar(pvchar character varying, p2vchar character varying) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftbint(bigint); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftbint(pbint bigint) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftchar(character); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftchar(pchar character) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftclob(text); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftclob(pclob text) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftdbl(double precision); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftdbl(pdbl double precision) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftdec(numeric); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftdec(pdec numeric) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftdt(date); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftdt(pdt date) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftflt(double precision); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftflt(pflt double precision) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftint(integer); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftint(pint integer) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftnum(numeric); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftnum(pnum numeric) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftrl(real); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftrl(prl real) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftsint(smallint); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftsint(psint smallint) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: fttm(time without time zone); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION fttm(ptm time without time zone) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftts(timestamp without time zone); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftts(pts timestamp without time zone) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: ftvchar(character varying); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION ftvchar(pvchar character varying) RETURNS SETOF tset1
    AS $$
begin
 select RNUM, C1, C2 from TSET1;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pinbint(bigint); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pinbint(pbint bigint) RETURNS void
    AS $$
declare
	P1 bigint;
begin
	P1:= PBINT;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pinchar(character); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pinchar(pchar character) RETURNS void
    AS $$
declare
	P1 char(32);
begin
	P1:= PCHAR;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pinclob(text); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pinclob(pclob text) RETURNS void
    AS $$
declare
	P1 text;
begin
	P1:= PCLOB;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pindbl(double precision); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pindbl(pdbl double precision) RETURNS void
    AS $$
declare
	P1 double precision;
begin
	P1:= PDBL;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pindec(numeric); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pindec(pdec numeric) RETURNS void
    AS $$
declare
	P1 decimal(7,2);
begin
	P1:= PDEC;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pindt(date); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pindt(pdt date) RETURNS void
    AS $$
declare
	P1 date;
begin
	P1:= PDT;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pinflt(double precision); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pinflt(pflt double precision) RETURNS void
    AS $$
declare
	P1 float;
begin
	P1:= PFLT;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pinint(integer); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pinint(pint integer) RETURNS void
    AS $$
declare
	P1 integer;
begin
	P1:= PINT;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pinnum(numeric); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pinnum(pnum numeric) RETURNS void
    AS $$
declare
	P1 numeric(7,2);
begin
	P1:= PNUM;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pinrl(real); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pinrl(prl real) RETURNS void
    AS $$
declare
	P1 real;
begin
	P1:= PRL;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pinsint(smallint); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pinsint(psint smallint) RETURNS void
    AS $$
declare
	P1 smallint;
begin
	P1:= PSINT;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pintm(time without time zone); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pintm(ptm time without time zone) RETURNS void
    AS $$
declare
	P1 time(3);
begin
	P1:= PTM;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pints(timestamp without time zone); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pints(pts timestamp without time zone) RETURNS void
    AS $$
declare
	P1 timestamp(3);
begin
	P1:= PTS;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pinvchar(character varying); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pinvchar(pvchar character varying) RETURNS void
    AS $$
declare
	P1 varchar(32);
begin
	P1:= PVCHAR;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pnoparams(); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pnoparams() RETURNS void
    AS $$
begin
	null;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: povin(smallint); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION povin(ppovin smallint) RETURNS void
    AS $$
declare
	P1 smallint;
begin
	P1:= PPOVIN;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: povin(smallint, smallint); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION povin(ppovin smallint, p2povin smallint) RETURNS void
    AS $$
declare
	P1 smallint;
begin
	P1:= PPOVIN;
end;
$$
    LANGUAGE plpgsql;



--
-- Name: pres(); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pres() RETURNS SETOF tset1
    AS $$
 select RNUM, C1, C2 from TSET1;
$$
    LANGUAGE sql;



--
-- Name: pthrow(); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE FUNCTION pthrow() RETURNS void
    AS $$
begin
	RAISE EXCEPTION 'A USER DEFINED ERROR';
end;
$$
    LANGUAGE plpgsql;



--
-- Name: bruce; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE bruce (
    rnum integer,
    value double precision
) DISTRIBUTED BY (rnum);



--
-- Name: bruce2; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE bruce2 (
    a numeric,
    b numeric(18,8),
    c numeric(38,19),
    d numeric(39,19)
) DISTRIBUTED BY (a);



--
-- Name: bruce5; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE bruce5 (
    a numeric(500,20)
) DISTRIBUTED BY (a);



--
-- Name: tbint; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tbint (
    rnum integer NOT NULL,
    cbint bigint
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tbint; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tbint IS 'This describes table TBINT.';


--
-- Name: tchar; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tchar (
    rnum integer NOT NULL,
    cchar character(32)
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tchar; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tchar IS 'This describes table TCHAR.';


--
-- Name: tclob; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tclob (
    rnum integer NOT NULL,
    cclob text
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tclob; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tclob IS 'This describes table TCLOB.';


--
-- Name: tcons1; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tcons1 (
    rnum integer NOT NULL,
    col1 integer NOT NULL
) DISTRIBUTED BY (col1);



--
-- Name: tcons2; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tcons2 (
    rnum integer NOT NULL,
    col2 integer NOT NULL,
    col3 integer NOT NULL
) DISTRIBUTED BY (col2 ,col3);



--
-- Name: tcons3; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tcons3 (
    rnum integer NOT NULL,
    col1 integer NOT NULL,
    col2 integer NOT NULL,
    col3 integer,
    col4 integer
) DISTRIBUTED BY (col1 ,col2);



--
-- Name: tcons4; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tcons4 (
    rnum integer NOT NULL,
    col1 integer NOT NULL,
    col2 integer
) DISTRIBUTED BY (col1);



--
-- Name: tdbl; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tdbl (
    rnum integer NOT NULL,
    cdbl double precision
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tdbl; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tdbl IS 'This describes table TDBL.';


--
-- Name: tdec; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tdec (
    rnum integer NOT NULL,
    cdec numeric(7,2)
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tdec; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tdec IS 'This describes table TDEC.';


--
-- Name: tdt; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tdt (
    rnum integer NOT NULL,
    cdt date
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tdt; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tdt IS 'This describes table TDT.';


--
-- Name: test_run_stage; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE test_run_stage (
    test_name_part character varying(200),
    db_version character varying(200),
    run_dt timestamp without time zone,
    pass_ind integer
) DISTRIBUTED BY (test_name_part);



--
-- Name: tests_stage; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tests_stage (
    test_name_part character varying(200)
) DISTRIBUTED BY (test_name_part);



--
-- Name: tflt; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tflt (
    rnum integer NOT NULL,
    cflt double precision
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tflt; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tflt IS 'This describes table TFLT.';


--
-- Name: tint; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tint (
    rnum integer NOT NULL,
    cint integer
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tint; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tint IS 'This describes table TINT.';


--
-- Name: tiud; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tiud (
    rnum integer NOT NULL,
    c1 character(10),
    c2 integer
) DISTRIBUTED BY (rnum);



--
-- Name: tjoin1; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tjoin1 (
    rnum integer NOT NULL,
    c1 integer,
    c2 integer
) DISTRIBUTED BY (rnum);



--
-- Name: tjoin2; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tjoin2 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(2)
) DISTRIBUTED BY (rnum);



--
-- Name: tjoin3; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tjoin3 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(2)
) DISTRIBUTED BY (rnum);



--
-- Name: tjoin4; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tjoin4 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(2)
) DISTRIBUTED BY (rnum);



--
-- Name: tlel; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlel (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlel_cy_preeuro; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlel_cy_preeuro (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlel_gr; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlel_gr (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlel_gr_preeuro; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlel_gr_preeuro (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tliw; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tliw (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tliw_il; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tliw_il (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlja; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlja (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlja_jp; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlja_jp (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlko; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlko (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlko_kr; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlko_kr (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tllatin1; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tllatin1 (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tllatin2; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tllatin2 (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlth; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlth (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlth_th; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlth_th (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlth_th_th; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlth_th_th (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tltr; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tltr (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tltr_tr; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tltr_tr (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlzh_cn; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlzh_cn (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tlzh_tw; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlzh_tw (
    rnum integer NOT NULL,
    c1 character(40),
    ord integer
) DISTRIBUTED BY (rnum);



--
-- Name: tmaxcols; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tmaxcols (
    rnum integer NOT NULL,
    c0 character(1),
    c1 character(1),
    c2 character(1),
    c3 character(1),
    c4 character(1),
    c5 character(1),
    c6 character(1),
    c7 character(1),
    c8 character(1),
    c9 character(1),
    c10 character(1),
    c11 character(1),
    c12 character(1),
    c13 character(1),
    c14 character(1),
    c15 character(1),
    c16 character(1),
    c17 character(1),
    c18 character(1),
    c19 character(1),
    c20 character(1),
    c21 character(1),
    c22 character(1),
    c23 character(1),
    c24 character(1),
    c25 character(1),
    c26 character(1),
    c27 character(1),
    c28 character(1),
    c29 character(1),
    c30 character(1),
    c31 character(1),
    c32 character(1),
    c33 character(1),
    c34 character(1),
    c35 character(1),
    c36 character(1),
    c37 character(1),
    c38 character(1),
    c39 character(1),
    c40 character(1),
    c41 character(1),
    c42 character(1),
    c43 character(1),
    c44 character(1),
    c45 character(1),
    c46 character(1),
    c47 character(1),
    c48 character(1),
    c49 character(1),
    c50 character(1),
    c51 character(1),
    c52 character(1),
    c53 character(1),
    c54 character(1),
    c55 character(1),
    c56 character(1),
    c57 character(1),
    c58 character(1),
    c59 character(1),
    c60 character(1),
    c61 character(1),
    c62 character(1),
    c63 character(1),
    c64 character(1),
    c65 character(1),
    c66 character(1),
    c67 character(1),
    c68 character(1),
    c69 character(1),
    c70 character(1),
    c71 character(1),
    c72 character(1),
    c73 character(1),
    c74 character(1),
    c75 character(1),
    c76 character(1),
    c77 character(1),
    c78 character(1),
    c79 character(1),
    c80 character(1),
    c81 character(1),
    c82 character(1),
    c83 character(1),
    c84 character(1),
    c85 character(1),
    c86 character(1),
    c87 character(1),
    c88 character(1),
    c89 character(1),
    c90 character(1),
    c91 character(1),
    c92 character(1),
    c93 character(1),
    c94 character(1),
    c95 character(1),
    c96 character(1),
    c97 character(1),
    c98 character(1),
    c99 character(1),
    c100 character(1),
    c101 character(1),
    c102 character(1),
    c103 character(1),
    c104 character(1),
    c105 character(1),
    c106 character(1),
    c107 character(1),
    c108 character(1),
    c109 character(1),
    c110 character(1),
    c111 character(1),
    c112 character(1),
    c113 character(1),
    c114 character(1),
    c115 character(1),
    c116 character(1),
    c117 character(1),
    c118 character(1),
    c119 character(1),
    c120 character(1),
    c121 character(1),
    c122 character(1),
    c123 character(1),
    c124 character(1),
    c125 character(1),
    c126 character(1),
    c127 character(1),
    c128 character(1),
    c129 character(1),
    c130 character(1),
    c131 character(1),
    c132 character(1),
    c133 character(1),
    c134 character(1),
    c135 character(1),
    c136 character(1),
    c137 character(1),
    c138 character(1),
    c139 character(1),
    c140 character(1),
    c141 character(1),
    c142 character(1),
    c143 character(1),
    c144 character(1),
    c145 character(1),
    c146 character(1),
    c147 character(1),
    c148 character(1),
    c149 character(1),
    c150 character(1),
    c151 character(1),
    c152 character(1),
    c153 character(1),
    c154 character(1),
    c155 character(1),
    c156 character(1),
    c157 character(1),
    c158 character(1),
    c159 character(1),
    c160 character(1),
    c161 character(1),
    c162 character(1),
    c163 character(1),
    c164 character(1),
    c165 character(1),
    c166 character(1),
    c167 character(1),
    c168 character(1),
    c169 character(1),
    c170 character(1),
    c171 character(1),
    c172 character(1),
    c173 character(1),
    c174 character(1),
    c175 character(1),
    c176 character(1),
    c177 character(1),
    c178 character(1),
    c179 character(1),
    c180 character(1),
    c181 character(1),
    c182 character(1),
    c183 character(1),
    c184 character(1),
    c185 character(1),
    c186 character(1),
    c187 character(1),
    c188 character(1),
    c189 character(1),
    c190 character(1),
    c191 character(1),
    c192 character(1),
    c193 character(1),
    c194 character(1),
    c195 character(1),
    c196 character(1),
    c197 character(1),
    c198 character(1),
    c199 character(1),
    c200 character(1),
    c201 character(1),
    c202 character(1),
    c203 character(1),
    c204 character(1),
    c205 character(1),
    c206 character(1),
    c207 character(1),
    c208 character(1),
    c209 character(1),
    c210 character(1),
    c211 character(1),
    c212 character(1),
    c213 character(1),
    c214 character(1),
    c215 character(1),
    c216 character(1),
    c217 character(1),
    c218 character(1),
    c219 character(1),
    c220 character(1),
    c221 character(1),
    c222 character(1),
    c223 character(1),
    c224 character(1),
    c225 character(1),
    c226 character(1),
    c227 character(1),
    c228 character(1),
    c229 character(1),
    c230 character(1),
    c231 character(1),
    c232 character(1),
    c233 character(1),
    c234 character(1),
    c235 character(1),
    c236 character(1),
    c237 character(1),
    c238 character(1),
    c239 character(1),
    c240 character(1),
    c241 character(1),
    c242 character(1),
    c243 character(1),
    c244 character(1),
    c245 character(1),
    c246 character(1),
    c247 character(1),
    c248 character(1),
    c249 character(1),
    c250 character(1),
    c251 character(1),
    c252 character(1),
    c253 character(1),
    c254 character(1),
    c255 character(1),
    c256 character(1),
    c257 character(1),
    c258 character(1),
    c259 character(1),
    c260 character(1),
    c261 character(1),
    c262 character(1),
    c263 character(1),
    c264 character(1),
    c265 character(1),
    c266 character(1),
    c267 character(1),
    c268 character(1),
    c269 character(1),
    c270 character(1),
    c271 character(1),
    c272 character(1),
    c273 character(1),
    c274 character(1),
    c275 character(1),
    c276 character(1),
    c277 character(1),
    c278 character(1),
    c279 character(1),
    c280 character(1),
    c281 character(1),
    c282 character(1),
    c283 character(1),
    c284 character(1),
    c285 character(1),
    c286 character(1),
    c287 character(1),
    c288 character(1),
    c289 character(1),
    c290 character(1),
    c291 character(1),
    c292 character(1),
    c293 character(1),
    c294 character(1),
    c295 character(1),
    c296 character(1),
    c297 character(1),
    c298 character(1),
    c299 character(1),
    c300 character(1),
    c301 character(1),
    c302 character(1),
    c303 character(1),
    c304 character(1),
    c305 character(1),
    c306 character(1),
    c307 character(1),
    c308 character(1),
    c309 character(1),
    c310 character(1),
    c311 character(1),
    c312 character(1),
    c313 character(1),
    c314 character(1),
    c315 character(1),
    c316 character(1),
    c317 character(1),
    c318 character(1),
    c319 character(1),
    c320 character(1),
    c321 character(1),
    c322 character(1),
    c323 character(1),
    c324 character(1),
    c325 character(1),
    c326 character(1),
    c327 character(1),
    c328 character(1),
    c329 character(1),
    c330 character(1),
    c331 character(1),
    c332 character(1),
    c333 character(1),
    c334 character(1),
    c335 character(1),
    c336 character(1),
    c337 character(1),
    c338 character(1),
    c339 character(1),
    c340 character(1),
    c341 character(1),
    c342 character(1),
    c343 character(1),
    c344 character(1),
    c345 character(1),
    c346 character(1),
    c347 character(1),
    c348 character(1),
    c349 character(1),
    c350 character(1),
    c351 character(1),
    c352 character(1),
    c353 character(1),
    c354 character(1),
    c355 character(1),
    c356 character(1),
    c357 character(1),
    c358 character(1),
    c359 character(1),
    c360 character(1),
    c361 character(1),
    c362 character(1),
    c363 character(1),
    c364 character(1),
    c365 character(1),
    c366 character(1),
    c367 character(1),
    c368 character(1),
    c369 character(1),
    c370 character(1),
    c371 character(1),
    c372 character(1),
    c373 character(1),
    c374 character(1),
    c375 character(1),
    c376 character(1),
    c377 character(1),
    c378 character(1),
    c379 character(1),
    c380 character(1),
    c381 character(1),
    c382 character(1),
    c383 character(1),
    c384 character(1),
    c385 character(1),
    c386 character(1),
    c387 character(1),
    c388 character(1),
    c389 character(1),
    c390 character(1),
    c391 character(1),
    c392 character(1),
    c393 character(1),
    c394 character(1),
    c395 character(1),
    c396 character(1),
    c397 character(1),
    c398 character(1),
    c399 character(1),
    c400 character(1),
    c401 character(1),
    c402 character(1),
    c403 character(1),
    c404 character(1),
    c405 character(1),
    c406 character(1),
    c407 character(1),
    c408 character(1),
    c409 character(1),
    c410 character(1),
    c411 character(1),
    c412 character(1),
    c413 character(1),
    c414 character(1),
    c415 character(1),
    c416 character(1),
    c417 character(1),
    c418 character(1),
    c419 character(1),
    c420 character(1),
    c421 character(1),
    c422 character(1),
    c423 character(1),
    c424 character(1),
    c425 character(1),
    c426 character(1),
    c427 character(1),
    c428 character(1),
    c429 character(1),
    c430 character(1),
    c431 character(1),
    c432 character(1),
    c433 character(1),
    c434 character(1),
    c435 character(1),
    c436 character(1),
    c437 character(1),
    c438 character(1),
    c439 character(1),
    c440 character(1),
    c441 character(1),
    c442 character(1),
    c443 character(1),
    c444 character(1),
    c445 character(1),
    c446 character(1),
    c447 character(1),
    c448 character(1),
    c449 character(1),
    c450 character(1),
    c451 character(1),
    c452 character(1),
    c453 character(1),
    c454 character(1),
    c455 character(1),
    c456 character(1),
    c457 character(1),
    c458 character(1),
    c459 character(1),
    c460 character(1),
    c461 character(1),
    c462 character(1),
    c463 character(1),
    c464 character(1),
    c465 character(1),
    c466 character(1),
    c467 character(1),
    c468 character(1),
    c469 character(1),
    c470 character(1),
    c471 character(1),
    c472 character(1),
    c473 character(1),
    c474 character(1),
    c475 character(1),
    c476 character(1),
    c477 character(1),
    c478 character(1),
    c479 character(1),
    c480 character(1),
    c481 character(1),
    c482 character(1),
    c483 character(1),
    c484 character(1),
    c485 character(1),
    c486 character(1),
    c487 character(1),
    c488 character(1),
    c489 character(1),
    c490 character(1),
    c491 character(1),
    c492 character(1),
    c493 character(1),
    c494 character(1),
    c495 character(1),
    c496 character(1),
    c497 character(1),
    c498 character(1),
    c499 character(1),
    c500 character(1),
    c501 character(1),
    c502 character(1),
    c503 character(1),
    c504 character(1),
    c505 character(1),
    c506 character(1),
    c507 character(1),
    c508 character(1),
    c509 character(1),
    c510 character(1),
    c511 character(1),
    c512 character(1),
    c513 character(1),
    c514 character(1),
    c515 character(1),
    c516 character(1),
    c517 character(1),
    c518 character(1),
    c519 character(1),
    c520 character(1),
    c521 character(1),
    c522 character(1),
    c523 character(1),
    c524 character(1),
    c525 character(1),
    c526 character(1),
    c527 character(1),
    c528 character(1),
    c529 character(1),
    c530 character(1),
    c531 character(1),
    c532 character(1),
    c533 character(1),
    c534 character(1),
    c535 character(1),
    c536 character(1),
    c537 character(1),
    c538 character(1),
    c539 character(1),
    c540 character(1),
    c541 character(1),
    c542 character(1),
    c543 character(1),
    c544 character(1),
    c545 character(1),
    c546 character(1),
    c547 character(1),
    c548 character(1),
    c549 character(1),
    c550 character(1),
    c551 character(1),
    c552 character(1),
    c553 character(1),
    c554 character(1),
    c555 character(1),
    c556 character(1),
    c557 character(1),
    c558 character(1),
    c559 character(1),
    c560 character(1),
    c561 character(1),
    c562 character(1),
    c563 character(1),
    c564 character(1),
    c565 character(1),
    c566 character(1),
    c567 character(1),
    c568 character(1),
    c569 character(1),
    c570 character(1),
    c571 character(1),
    c572 character(1),
    c573 character(1),
    c574 character(1),
    c575 character(1),
    c576 character(1),
    c577 character(1),
    c578 character(1),
    c579 character(1),
    c580 character(1),
    c581 character(1),
    c582 character(1),
    c583 character(1),
    c584 character(1),
    c585 character(1),
    c586 character(1),
    c587 character(1),
    c588 character(1),
    c589 character(1),
    c590 character(1),
    c591 character(1),
    c592 character(1),
    c593 character(1),
    c594 character(1),
    c595 character(1),
    c596 character(1),
    c597 character(1),
    c598 character(1),
    c599 character(1),
    c600 character(1),
    c601 character(1),
    c602 character(1),
    c603 character(1),
    c604 character(1),
    c605 character(1),
    c606 character(1),
    c607 character(1),
    c608 character(1),
    c609 character(1),
    c610 character(1),
    c611 character(1),
    c612 character(1),
    c613 character(1),
    c614 character(1),
    c615 character(1),
    c616 character(1),
    c617 character(1),
    c618 character(1),
    c619 character(1),
    c620 character(1),
    c621 character(1),
    c622 character(1),
    c623 character(1),
    c624 character(1),
    c625 character(1),
    c626 character(1),
    c627 character(1),
    c628 character(1),
    c629 character(1),
    c630 character(1),
    c631 character(1),
    c632 character(1),
    c633 character(1),
    c634 character(1),
    c635 character(1),
    c636 character(1),
    c637 character(1),
    c638 character(1),
    c639 character(1),
    c640 character(1),
    c641 character(1),
    c642 character(1),
    c643 character(1),
    c644 character(1),
    c645 character(1),
    c646 character(1),
    c647 character(1),
    c648 character(1),
    c649 character(1),
    c650 character(1),
    c651 character(1),
    c652 character(1),
    c653 character(1),
    c654 character(1),
    c655 character(1),
    c656 character(1),
    c657 character(1),
    c658 character(1),
    c659 character(1),
    c660 character(1),
    c661 character(1),
    c662 character(1),
    c663 character(1),
    c664 character(1),
    c665 character(1),
    c666 character(1),
    c667 character(1),
    c668 character(1),
    c669 character(1),
    c670 character(1),
    c671 character(1),
    c672 character(1),
    c673 character(1),
    c674 character(1),
    c675 character(1),
    c676 character(1),
    c677 character(1),
    c678 character(1),
    c679 character(1),
    c680 character(1),
    c681 character(1),
    c682 character(1),
    c683 character(1),
    c684 character(1),
    c685 character(1),
    c686 character(1),
    c687 character(1),
    c688 character(1),
    c689 character(1),
    c690 character(1),
    c691 character(1),
    c692 character(1),
    c693 character(1),
    c694 character(1),
    c695 character(1),
    c696 character(1),
    c697 character(1),
    c698 character(1),
    c699 character(1),
    c700 character(1),
    c701 character(1),
    c702 character(1),
    c703 character(1),
    c704 character(1),
    c705 character(1),
    c706 character(1),
    c707 character(1),
    c708 character(1),
    c709 character(1),
    c710 character(1),
    c711 character(1),
    c712 character(1),
    c713 character(1),
    c714 character(1),
    c715 character(1),
    c716 character(1),
    c717 character(1),
    c718 character(1),
    c719 character(1),
    c720 character(1),
    c721 character(1),
    c722 character(1),
    c723 character(1),
    c724 character(1),
    c725 character(1),
    c726 character(1),
    c727 character(1),
    c728 character(1),
    c729 character(1),
    c730 character(1),
    c731 character(1),
    c732 character(1),
    c733 character(1),
    c734 character(1),
    c735 character(1),
    c736 character(1),
    c737 character(1),
    c738 character(1),
    c739 character(1),
    c740 character(1),
    c741 character(1),
    c742 character(1),
    c743 character(1),
    c744 character(1),
    c745 character(1),
    c746 character(1),
    c747 character(1),
    c748 character(1),
    c749 character(1),
    c750 character(1),
    c751 character(1),
    c752 character(1),
    c753 character(1),
    c754 character(1),
    c755 character(1),
    c756 character(1),
    c757 character(1),
    c758 character(1),
    c759 character(1),
    c760 character(1),
    c761 character(1),
    c762 character(1),
    c763 character(1),
    c764 character(1),
    c765 character(1),
    c766 character(1),
    c767 character(1),
    c768 character(1),
    c769 character(1),
    c770 character(1),
    c771 character(1),
    c772 character(1),
    c773 character(1),
    c774 character(1),
    c775 character(1),
    c776 character(1),
    c777 character(1),
    c778 character(1),
    c779 character(1),
    c780 character(1),
    c781 character(1),
    c782 character(1),
    c783 character(1),
    c784 character(1),
    c785 character(1),
    c786 character(1),
    c787 character(1),
    c788 character(1),
    c789 character(1),
    c790 character(1),
    c791 character(1),
    c792 character(1),
    c793 character(1),
    c794 character(1),
    c795 character(1),
    c796 character(1),
    c797 character(1),
    c798 character(1),
    c799 character(1),
    c800 character(1),
    c801 character(1),
    c802 character(1),
    c803 character(1),
    c804 character(1),
    c805 character(1),
    c806 character(1),
    c807 character(1),
    c808 character(1),
    c809 character(1),
    c810 character(1),
    c811 character(1),
    c812 character(1),
    c813 character(1),
    c814 character(1),
    c815 character(1),
    c816 character(1),
    c817 character(1),
    c818 character(1),
    c819 character(1),
    c820 character(1),
    c821 character(1),
    c822 character(1),
    c823 character(1),
    c824 character(1),
    c825 character(1),
    c826 character(1),
    c827 character(1),
    c828 character(1),
    c829 character(1),
    c830 character(1),
    c831 character(1),
    c832 character(1),
    c833 character(1),
    c834 character(1),
    c835 character(1),
    c836 character(1),
    c837 character(1),
    c838 character(1),
    c839 character(1),
    c840 character(1),
    c841 character(1),
    c842 character(1),
    c843 character(1),
    c844 character(1),
    c845 character(1),
    c846 character(1),
    c847 character(1),
    c848 character(1),
    c849 character(1),
    c850 character(1),
    c851 character(1),
    c852 character(1),
    c853 character(1),
    c854 character(1),
    c855 character(1),
    c856 character(1),
    c857 character(1),
    c858 character(1),
    c859 character(1),
    c860 character(1),
    c861 character(1),
    c862 character(1),
    c863 character(1),
    c864 character(1),
    c865 character(1),
    c866 character(1),
    c867 character(1),
    c868 character(1),
    c869 character(1),
    c870 character(1),
    c871 character(1),
    c872 character(1),
    c873 character(1),
    c874 character(1),
    c875 character(1),
    c876 character(1),
    c877 character(1),
    c878 character(1),
    c879 character(1),
    c880 character(1),
    c881 character(1),
    c882 character(1),
    c883 character(1),
    c884 character(1),
    c885 character(1),
    c886 character(1),
    c887 character(1),
    c888 character(1),
    c889 character(1),
    c890 character(1),
    c891 character(1),
    c892 character(1),
    c893 character(1),
    c894 character(1),
    c895 character(1),
    c896 character(1),
    c897 character(1),
    c898 character(1),
    c899 character(1),
    c900 character(1),
    c901 character(1),
    c902 character(1),
    c903 character(1),
    c904 character(1),
    c905 character(1),
    c906 character(1),
    c907 character(1),
    c908 character(1),
    c909 character(1),
    c910 character(1),
    c911 character(1),
    c912 character(1),
    c913 character(1),
    c914 character(1),
    c915 character(1),
    c916 character(1),
    c917 character(1),
    c918 character(1),
    c919 character(1),
    c920 character(1),
    c921 character(1),
    c922 character(1),
    c923 character(1),
    c924 character(1),
    c925 character(1),
    c926 character(1),
    c927 character(1),
    c928 character(1),
    c929 character(1),
    c930 character(1),
    c931 character(1),
    c932 character(1),
    c933 character(1),
    c934 character(1),
    c935 character(1),
    c936 character(1),
    c937 character(1),
    c938 character(1),
    c939 character(1),
    c940 character(1),
    c941 character(1),
    c942 character(1),
    c943 character(1),
    c944 character(1),
    c945 character(1),
    c946 character(1),
    c947 character(1),
    c948 character(1),
    c949 character(1),
    c950 character(1),
    c951 character(1),
    c952 character(1),
    c953 character(1),
    c954 character(1),
    c955 character(1),
    c956 character(1),
    c957 character(1),
    c958 character(1),
    c959 character(1),
    c960 character(1),
    c961 character(1),
    c962 character(1),
    c963 character(1),
    c964 character(1),
    c965 character(1),
    c966 character(1),
    c967 character(1),
    c968 character(1),
    c969 character(1),
    c970 character(1),
    c971 character(1),
    c972 character(1),
    c973 character(1),
    c974 character(1),
    c975 character(1),
    c976 character(1),
    c977 character(1),
    c978 character(1),
    c979 character(1),
    c980 character(1),
    c981 character(1),
    c982 character(1),
    c983 character(1),
    c984 character(1),
    c985 character(1),
    c986 character(1),
    c987 character(1),
    c988 character(1),
    c989 character(1),
    c990 character(1),
    c991 character(1),
    c992 character(1),
    c993 character(1),
    c994 character(1),
    c995 character(1),
    c996 character(1),
    c997 character(1),
    c998 character(1),
    c999 character(1),
    c1000 character(1),
    c1001 character(1),
    c1002 character(1),
    c1003 character(1),
    c1004 character(1),
    c1005 character(1),
    c1006 character(1),
    c1007 character(1),
    c1008 character(1),
    c1009 character(1),
    c1010 character(1),
    c1011 character(1),
    c1012 character(1),
    c1013 character(1),
    c1014 character(1),
    c1015 character(1),
    c1016 character(1),
    c1017 character(1),
    c1018 character(1),
    c1019 character(1),
    c1020 character(1),
    c1021 character(1),
    c1022 character(1),
    c1023 character(1),
    c1024 character(1),
    c1025 character(1),
    c1026 character(1),
    c1027 character(1),
    c1028 character(1),
    c1029 character(1),
    c1030 character(1),
    c1031 character(1),
    c1032 character(1),
    c1033 character(1),
    c1034 character(1),
    c1035 character(1),
    c1036 character(1),
    c1037 character(1),
    c1038 character(1),
    c1039 character(1),
    c1040 character(1),
    c1041 character(1),
    c1042 character(1),
    c1043 character(1),
    c1044 character(1),
    c1045 character(1),
    c1046 character(1),
    c1047 character(1),
    c1048 character(1),
    c1049 character(1),
    c1050 character(1),
    c1051 character(1),
    c1052 character(1),
    c1053 character(1),
    c1054 character(1),
    c1055 character(1),
    c1056 character(1),
    c1057 character(1),
    c1058 character(1),
    c1059 character(1),
    c1060 character(1),
    c1061 character(1),
    c1062 character(1),
    c1063 character(1),
    c1064 character(1),
    c1065 character(1),
    c1066 character(1),
    c1067 character(1),
    c1068 character(1),
    c1069 character(1),
    c1070 character(1),
    c1071 character(1),
    c1072 character(1),
    c1073 character(1),
    c1074 character(1),
    c1075 character(1),
    c1076 character(1),
    c1077 character(1),
    c1078 character(1),
    c1079 character(1),
    c1080 character(1),
    c1081 character(1),
    c1082 character(1),
    c1083 character(1),
    c1084 character(1),
    c1085 character(1),
    c1086 character(1),
    c1087 character(1),
    c1088 character(1),
    c1089 character(1),
    c1090 character(1),
    c1091 character(1),
    c1092 character(1),
    c1093 character(1),
    c1094 character(1),
    c1095 character(1),
    c1096 character(1),
    c1097 character(1),
    c1098 character(1),
    c1099 character(1),
    c1100 character(1),
    c1101 character(1),
    c1102 character(1),
    c1103 character(1),
    c1104 character(1),
    c1105 character(1),
    c1106 character(1),
    c1107 character(1),
    c1108 character(1),
    c1109 character(1),
    c1110 character(1),
    c1111 character(1),
    c1112 character(1),
    c1113 character(1),
    c1114 character(1),
    c1115 character(1),
    c1116 character(1),
    c1117 character(1),
    c1118 character(1),
    c1119 character(1),
    c1120 character(1),
    c1121 character(1),
    c1122 character(1),
    c1123 character(1),
    c1124 character(1),
    c1125 character(1),
    c1126 character(1),
    c1127 character(1),
    c1128 character(1),
    c1129 character(1),
    c1130 character(1),
    c1131 character(1),
    c1132 character(1),
    c1133 character(1),
    c1134 character(1),
    c1135 character(1),
    c1136 character(1),
    c1137 character(1),
    c1138 character(1),
    c1139 character(1),
    c1140 character(1),
    c1141 character(1),
    c1142 character(1),
    c1143 character(1),
    c1144 character(1),
    c1145 character(1),
    c1146 character(1),
    c1147 character(1),
    c1148 character(1),
    c1149 character(1),
    c1150 character(1),
    c1151 character(1),
    c1152 character(1),
    c1153 character(1),
    c1154 character(1),
    c1155 character(1),
    c1156 character(1),
    c1157 character(1),
    c1158 character(1),
    c1159 character(1),
    c1160 character(1),
    c1161 character(1),
    c1162 character(1),
    c1163 character(1),
    c1164 character(1),
    c1165 character(1),
    c1166 character(1),
    c1167 character(1),
    c1168 character(1),
    c1169 character(1),
    c1170 character(1),
    c1171 character(1),
    c1172 character(1),
    c1173 character(1),
    c1174 character(1),
    c1175 character(1),
    c1176 character(1),
    c1177 character(1),
    c1178 character(1),
    c1179 character(1),
    c1180 character(1),
    c1181 character(1),
    c1182 character(1),
    c1183 character(1),
    c1184 character(1),
    c1185 character(1),
    c1186 character(1),
    c1187 character(1),
    c1188 character(1),
    c1189 character(1),
    c1190 character(1),
    c1191 character(1),
    c1192 character(1),
    c1193 character(1),
    c1194 character(1),
    c1195 character(1),
    c1196 character(1),
    c1197 character(1),
    c1198 character(1),
    c1199 character(1),
    c1200 character(1),
    c1201 character(1),
    c1202 character(1),
    c1203 character(1),
    c1204 character(1),
    c1205 character(1),
    c1206 character(1),
    c1207 character(1),
    c1208 character(1),
    c1209 character(1),
    c1210 character(1),
    c1211 character(1),
    c1212 character(1),
    c1213 character(1),
    c1214 character(1),
    c1215 character(1),
    c1216 character(1),
    c1217 character(1),
    c1218 character(1),
    c1219 character(1),
    c1220 character(1),
    c1221 character(1),
    c1222 character(1),
    c1223 character(1),
    c1224 character(1),
    c1225 character(1),
    c1226 character(1),
    c1227 character(1),
    c1228 character(1),
    c1229 character(1),
    c1230 character(1),
    c1231 character(1),
    c1232 character(1),
    c1233 character(1),
    c1234 character(1),
    c1235 character(1),
    c1236 character(1),
    c1237 character(1),
    c1238 character(1),
    c1239 character(1),
    c1240 character(1),
    c1241 character(1),
    c1242 character(1),
    c1243 character(1),
    c1244 character(1),
    c1245 character(1),
    c1246 character(1),
    c1247 character(1),
    c1248 character(1),
    c1249 character(1),
    c1250 character(1),
    c1251 character(1),
    c1252 character(1),
    c1253 character(1),
    c1254 character(1),
    c1255 character(1),
    c1256 character(1),
    c1257 character(1),
    c1258 character(1),
    c1259 character(1),
    c1260 character(1),
    c1261 character(1),
    c1262 character(1),
    c1263 character(1),
    c1264 character(1),
    c1265 character(1),
    c1266 character(1),
    c1267 character(1),
    c1268 character(1),
    c1269 character(1),
    c1270 character(1),
    c1271 character(1),
    c1272 character(1),
    c1273 character(1),
    c1274 character(1),
    c1275 character(1),
    c1276 character(1),
    c1277 character(1),
    c1278 character(1),
    c1279 character(1),
    c1280 character(1),
    c1281 character(1),
    c1282 character(1),
    c1283 character(1),
    c1284 character(1),
    c1285 character(1),
    c1286 character(1),
    c1287 character(1),
    c1288 character(1),
    c1289 character(1),
    c1290 character(1),
    c1291 character(1),
    c1292 character(1),
    c1293 character(1),
    c1294 character(1),
    c1295 character(1),
    c1296 character(1),
    c1297 character(1),
    c1298 character(1),
    c1299 character(1),
    c1300 character(1),
    c1301 character(1),
    c1302 character(1),
    c1303 character(1),
    c1304 character(1),
    c1305 character(1),
    c1306 character(1),
    c1307 character(1),
    c1308 character(1),
    c1309 character(1),
    c1310 character(1),
    c1311 character(1),
    c1312 character(1),
    c1313 character(1),
    c1314 character(1),
    c1315 character(1),
    c1316 character(1),
    c1317 character(1),
    c1318 character(1),
    c1319 character(1),
    c1320 character(1),
    c1321 character(1),
    c1322 character(1),
    c1323 character(1),
    c1324 character(1),
    c1325 character(1),
    c1326 character(1),
    c1327 character(1),
    c1328 character(1),
    c1329 character(1),
    c1330 character(1),
    c1331 character(1),
    c1332 character(1),
    c1333 character(1),
    c1334 character(1),
    c1335 character(1),
    c1336 character(1),
    c1337 character(1),
    c1338 character(1),
    c1339 character(1),
    c1340 character(1),
    c1341 character(1),
    c1342 character(1),
    c1343 character(1),
    c1344 character(1),
    c1345 character(1),
    c1346 character(1),
    c1347 character(1),
    c1348 character(1),
    c1349 character(1),
    c1350 character(1),
    c1351 character(1),
    c1352 character(1),
    c1353 character(1),
    c1354 character(1),
    c1355 character(1),
    c1356 character(1),
    c1357 character(1),
    c1358 character(1),
    c1359 character(1),
    c1360 character(1),
    c1361 character(1),
    c1362 character(1),
    c1363 character(1),
    c1364 character(1),
    c1365 character(1),
    c1366 character(1),
    c1367 character(1),
    c1368 character(1),
    c1369 character(1),
    c1370 character(1),
    c1371 character(1),
    c1372 character(1),
    c1373 character(1),
    c1374 character(1),
    c1375 character(1),
    c1376 character(1),
    c1377 character(1),
    c1378 character(1),
    c1379 character(1),
    c1380 character(1),
    c1381 character(1),
    c1382 character(1),
    c1383 character(1),
    c1384 character(1),
    c1385 character(1),
    c1386 character(1),
    c1387 character(1),
    c1388 character(1),
    c1389 character(1),
    c1390 character(1),
    c1391 character(1),
    c1392 character(1),
    c1393 character(1),
    c1394 character(1),
    c1395 character(1),
    c1396 character(1),
    c1397 character(1),
    c1398 character(1),
    c1399 character(1),
    c1400 character(1),
    c1401 character(1),
    c1402 character(1),
    c1403 character(1),
    c1404 character(1),
    c1405 character(1),
    c1406 character(1),
    c1407 character(1),
    c1408 character(1),
    c1409 character(1),
    c1410 character(1),
    c1411 character(1),
    c1412 character(1),
    c1413 character(1),
    c1414 character(1),
    c1415 character(1),
    c1416 character(1),
    c1417 character(1),
    c1418 character(1),
    c1419 character(1),
    c1420 character(1),
    c1421 character(1),
    c1422 character(1),
    c1423 character(1),
    c1424 character(1),
    c1425 character(1),
    c1426 character(1),
    c1427 character(1),
    c1428 character(1),
    c1429 character(1),
    c1430 character(1),
    c1431 character(1),
    c1432 character(1),
    c1433 character(1),
    c1434 character(1),
    c1435 character(1),
    c1436 character(1),
    c1437 character(1),
    c1438 character(1),
    c1439 character(1),
    c1440 character(1),
    c1441 character(1),
    c1442 character(1),
    c1443 character(1),
    c1444 character(1),
    c1445 character(1),
    c1446 character(1),
    c1447 character(1),
    c1448 character(1),
    c1449 character(1),
    c1450 character(1),
    c1451 character(1),
    c1452 character(1),
    c1453 character(1),
    c1454 character(1),
    c1455 character(1),
    c1456 character(1),
    c1457 character(1),
    c1458 character(1),
    c1459 character(1),
    c1460 character(1),
    c1461 character(1),
    c1462 character(1),
    c1463 character(1),
    c1464 character(1),
    c1465 character(1),
    c1466 character(1),
    c1467 character(1),
    c1468 character(1),
    c1469 character(1),
    c1470 character(1),
    c1471 character(1),
    c1472 character(1),
    c1473 character(1),
    c1474 character(1),
    c1475 character(1),
    c1476 character(1),
    c1477 character(1),
    c1478 character(1),
    c1479 character(1),
    c1480 character(1),
    c1481 character(1),
    c1482 character(1),
    c1483 character(1),
    c1484 character(1),
    c1485 character(1),
    c1486 character(1),
    c1487 character(1),
    c1488 character(1),
    c1489 character(1),
    c1490 character(1),
    c1491 character(1),
    c1492 character(1),
    c1493 character(1),
    c1494 character(1),
    c1495 character(1),
    c1496 character(1),
    c1497 character(1),
    c1498 character(1),
    c1499 character(1),
    c1500 character(1),
    c1501 character(1),
    c1502 character(1),
    c1503 character(1),
    c1504 character(1),
    c1505 character(1),
    c1506 character(1),
    c1507 character(1),
    c1508 character(1),
    c1509 character(1),
    c1510 character(1),
    c1511 character(1),
    c1512 character(1),
    c1513 character(1),
    c1514 character(1),
    c1515 character(1),
    c1516 character(1),
    c1517 character(1),
    c1518 character(1),
    c1519 character(1),
    c1520 character(1),
    c1521 character(1),
    c1522 character(1),
    c1523 character(1),
    c1524 character(1),
    c1525 character(1),
    c1526 character(1),
    c1527 character(1),
    c1528 character(1),
    c1529 character(1),
    c1530 character(1),
    c1531 character(1),
    c1532 character(1),
    c1533 character(1),
    c1534 character(1),
    c1535 character(1),
    c1536 character(1),
    c1537 character(1),
    c1538 character(1),
    c1539 character(1),
    c1540 character(1),
    c1541 character(1),
    c1542 character(1),
    c1543 character(1),
    c1544 character(1),
    c1545 character(1),
    c1546 character(1),
    c1547 character(1),
    c1548 character(1),
    c1549 character(1),
    c1550 character(1),
    c1551 character(1),
    c1552 character(1),
    c1553 character(1),
    c1554 character(1),
    c1555 character(1),
    c1556 character(1),
    c1557 character(1),
    c1558 character(1),
    c1559 character(1),
    c1560 character(1),
    c1561 character(1),
    c1562 character(1),
    c1563 character(1),
    c1564 character(1),
    c1565 character(1),
    c1566 character(1),
    c1567 character(1),
    c1568 character(1),
    c1569 character(1),
    c1570 character(1),
    c1571 character(1),
    c1572 character(1),
    c1573 character(1),
    c1574 character(1),
    c1575 character(1),
    c1576 character(1),
    c1577 character(1),
    c1578 character(1),
    c1579 character(1),
    c1580 character(1),
    c1581 character(1),
    c1582 character(1),
    c1583 character(1),
    c1584 character(1),
    c1585 character(1),
    c1586 character(1),
    c1587 character(1),
    c1588 character(1),
    c1589 character(1),
    c1590 character(1),
    c1591 character(1),
    c1592 character(1),
    c1593 character(1),
    c1594 character(1),
    c1595 character(1),
    c1596 character(1),
    c1597 character(1),
    c1598 character(1)
) DISTRIBUTED BY (rnum);



--
-- Name: tnum; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tnum (
    rnum integer NOT NULL,
    cnum numeric(7,2)
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tnum; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tnum IS 'This describes table TNUM.';


--
-- Name: tolap; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tolap (
    rnum integer NOT NULL,
    c1 character(3),
    c2 character(2),
    c3 integer,
    c4 integer
) DISTRIBUTED BY (rnum);



--
-- Name: trl; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE trl (
    rnum integer NOT NULL,
    crl real
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE trl; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE trl IS 'This describes table TRL.';


--
-- Name: tsdchar; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tsdchar (
    rnum integer NOT NULL,
    c1 character(27)
) DISTRIBUTED BY (rnum);



--
-- Name: tsdclob; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tsdclob (
    rnum integer NOT NULL,
    c1 text
) DISTRIBUTED BY (rnum);



--
-- Name: tset2; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tset2 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(3)
) DISTRIBUTED BY (rnum);



--
-- Name: tset3; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tset3 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(3)
) DISTRIBUTED BY (rnum);



--
-- Name: tsint; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tsint (
    rnum integer NOT NULL,
    csint smallint
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tsint; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tsint IS 'This describes table TSINT.';


--
-- Name: ttm; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE ttm (
    rnum integer NOT NULL,
    ctm time(3) without time zone
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE ttm; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE ttm IS 'This describes table TTM.';


--
-- Name: tts; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tts (
    rnum integer NOT NULL,
    cts timestamp(3) without time zone
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tts; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tts IS 'This describes table TTS.';


--
-- Name: tvchar; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tvchar (
    rnum integer NOT NULL,
    cvchar character varying(32)
) DISTRIBUTED BY (rnum);



--
-- Name: TABLE tvchar; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON TABLE tvchar IS 'This describes table TVCHAR.';


--
-- Name: tversion; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tversion (
    rnum integer NOT NULL,
    c1 integer,
    cver character(6),
    cnnull integer,
    ccnull character(1)
) DISTRIBUTED BY (rnum);



--
-- Name: vbint; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vbint AS
    SELECT tbint.rnum, tbint.cbint FROM tbint;



--
-- Name: VIEW vbint; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vbint IS 'This describes view VBINT.';


--
-- Name: vchar; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vchar AS
    SELECT tchar.rnum, tchar.cchar FROM tchar;



--
-- Name: VIEW vchar; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vchar IS 'This describes view VCHAR.';


--
-- Name: vclob; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vclob AS
    SELECT tclob.rnum, tclob.cclob FROM tclob;



--
-- Name: VIEW vclob; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vclob IS 'This describes view VCLOB.';


--
-- Name: vdbl; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vdbl AS
    SELECT tdbl.rnum, tdbl.cdbl FROM tdbl;



--
-- Name: VIEW vdbl; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vdbl IS 'This describes view VDBL.';


--
-- Name: vdec; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vdec AS
    SELECT tdec.rnum, tdec.cdec FROM tdec;



--
-- Name: VIEW vdec; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vdec IS 'This describes view VDEC.';


--
-- Name: vdt; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vdt AS
    SELECT tdt.rnum, tdt.cdt FROM tdt;



--
-- Name: VIEW vdt; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vdt IS 'This describes view VDT.';


--
-- Name: vflt; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vflt AS
    SELECT tflt.rnum, tflt.cflt FROM tflt;



--
-- Name: VIEW vflt; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vflt IS 'This describes view VFLT.';


--
-- Name: vint; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vint AS
    SELECT tint.rnum, tint.cint FROM tint;



--
-- Name: VIEW vint; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vint IS 'This describes view VINT.';


--
-- Name: vnum; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vnum AS
    SELECT tnum.rnum, tnum.cnum FROM tnum;



--
-- Name: VIEW vnum; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vnum IS 'This describes view VNUM.';


--
-- Name: vrl; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vrl AS
    SELECT trl.rnum, trl.crl FROM trl;



--
-- Name: VIEW vrl; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vrl IS 'This describes view VRL.';


--
-- Name: vsint; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vsint AS
    SELECT tsint.rnum, tsint.csint FROM tsint;



--
-- Name: VIEW vsint; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vsint IS 'This describes view VSINT.';


--
-- Name: vtm; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vtm AS
    SELECT ttm.rnum, ttm.ctm FROM ttm;



--
-- Name: VIEW vtm; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vtm IS 'This describes view VTM.';


--
-- Name: vts; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vts AS
    SELECT tts.rnum, tts.cts FROM tts;



--
-- Name: VIEW vts; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vts IS 'This describes view VTS.';


--
-- Name: vvchar; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vvchar AS
    SELECT tvchar.rnum, tvchar.cvchar FROM tvchar;


--
-- Name: VIEW vvchar; Type: COMMENT; Schema: public; Owner: gpadmin
--

COMMENT ON VIEW vvchar IS 'This describes view VVCHAR.';


--
-- Data for Name: bruce; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY bruce (rnum, value) FROM stdin;
1	1
\.


--
-- Data for Name: bruce2; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY bruce2 (a, b, c, d) FROM stdin;
\.


--
-- Data for Name: bruce5; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY bruce5 (a) FROM stdin;
1111111.00000000000000000000
1111111555555555555555555555555555555555555555555555555555555555555555555555555555555555555.00000000000000000000
0.00000000000000000000
0.33333333333333333333
0.33330000000000000000
\.


--
-- Data for Name: tbint; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tbint (rnum, cbint) FROM stdin;
3	1
0	\N
1	-1
2	0
4	10
\.


--
-- Data for Name: tchar; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tchar (rnum, cchar) FROM stdin;
3	BB
0	\N
1
2
5	FF
4	EE
\.


--
-- Data for Name: tclob; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tclob (rnum, cclob) FROM stdin;
3	BB
0	\N
1
2
5	FF
4	EE
\.


--
-- Data for Name: tcons1; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tcons1 (rnum, col1) FROM stdin;
\.


--
-- Data for Name: tcons2; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tcons2 (rnum, col2, col3) FROM stdin;
\.


--
-- Data for Name: tcons3; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tcons3 (rnum, col1, col2, col3, col4) FROM stdin;
\.


--
-- Data for Name: tcons4; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tcons4 (rnum, col1, col2) FROM stdin;
\.


--
-- Data for Name: tdbl; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tdbl (rnum, cdbl) FROM stdin;
3	1
0	\N
1	-1
2	0
5	10
4	-0.10000000000000001
\.


--
-- Data for Name: tdec; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tdec (rnum, cdec) FROM stdin;
3	1.00
0	\N
1	-1.00
2	0.00
5	10.00
4	0.10
\.


--
-- Data for Name: tdt; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tdt (rnum, cdt) FROM stdin;
3	2000-12-31
0	\N
1	1996-01-01
2	2000-01-01
\.


--
-- Data for Name: test_run_stage; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY test_run_stage (test_name_part, db_version, run_dt, pass_ind) FROM stdin;
AbsCoreApproximateNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:43.76043	1
AbsCoreApproximateNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:43.259891	1
AbsCoreApproximateNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:44.166096	1
AbsCoreApproximateNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:43.947604	1
AbsCoreApproximateNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:44.572562	1
AbsCoreApproximateNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:44.353844	1
AbsCoreExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:44.759761	1
AbsCoreExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:44.97892	1
AbsCoreExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:45.197406	1
AbsCoreExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:45.416237	1
AbsCoreInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:45.603646	1
AbsCoreInteger_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:45.822122	1
AbsCoreInteger_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:46.041461	1
AbsCoreInteger_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:46.260007	1
AbsCoreInteger_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:46.447034	1
AbsCoreInteger_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:46.634531	1
ApproximateNumericOpAdd_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:47.010586	1
AggregateInExpression_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:46.821673	1
ApproximateNumericOpAdd_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:47.385385	1
ApproximateNumericOpAdd_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:47.197031	1
ApproximateNumericOpAdd_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:47.822196	1
ApproximateNumericOpAdd_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:47.603445	1
ApproximateNumericOpDiv_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:48.199033	1
ApproximateNumericOpAdd_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:48.009694	1
ApproximateNumericOpDiv_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:48.938003	1
ApproximateNumericOpDiv_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:48.417676	1
ApproximateNumericOpDiv_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:50.651422	1
ApproximateNumericOpDiv_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:49.557827	1
ApproximateNumericOpMulNULL_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:51.196946	1
ApproximateNumericOpDiv_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:51.011608	1
ApproximateNumericOpMulNULL_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:51.60328	1
ApproximateNumericOpMulNULL_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:51.415439	1
ApproximateNumericOpMulNULL_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:52.009146	1
ApproximateNumericOpMulNULL_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:51.790374	1
ApproximateNumericOpMul_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:52.447722	1
ApproximateNumericOpMul_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:52.232166	1
ApproximateNumericOpMul_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:52.853628	1
ApproximateNumericOpMul_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:52.666112	1
ApproximateNumericOpMul_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:53.231752	1
ApproximateNumericOpMul_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:53.040911	1
ApproximateNumericOpSub_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:53.635427	1
ApproximateNumericOpSub_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:53.415975	1
ApproximateNumericOpSub_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:53.982026	1
ApproximateNumericOpSub_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:53.822135	1
ApproximateNumericOpSub_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:54.322136	1
ApproximateNumericOpSub_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:54.166697	1
AvgApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:54.509081	1
AvgApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:54.696856	1
AvgApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:54.91528	1
AvgApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:55.135749	1
AvgApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:55.352835	1
AvgApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:55.572424	1
AvgExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:55.946755	1
AvgExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:55.759278	1
AvgExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:56.416028	1
AvgExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:56.165401	1
AvgIntTruncates_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:56.853047	1
AvgIntTruncates_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:56.639453	1
AvgIntTruncates_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:57.290448	1
AvgIntTruncates_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:57.073774	1
AvgIntTruncates_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:57.731117	1
AvgIntTruncates_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:57.509241	1
AvgInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:57.946645	1
AvgInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:58.165352	1
AvgInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:58.384838	1
AvgInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:58.602851	1
AvgInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:58.821498	1
AvgInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:59.04021	1
BooleanComparisonOperatorNotOperatorAnd_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:59.415759	1
BooleanComparisonOperatorAnd_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:59.259238	1
BooleanComparisonOperatorNotOperatorOr_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:59.572408	1
BooleanComparisonOperatorOr_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:59.730697	1
CaseBasicSearchApproximateNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:19:59.916469	1
CaseBasicSearchApproximateNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:00.103529	1
CaseBasicSearchApproximateNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:00.259772	1
CaseBasicSearchApproximateNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:00.479612	1
CaseBasicSearchApproximateNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:00.634591	1
CaseBasicSearchApproximateNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:00.853391	1
CaseBasicSearchExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:01.290909	1
CaseBasicSearchExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:01.104935	1
CaseBasicSearchExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:01.635258	1
CaseBasicSearchExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:01.478999	1
CaseBasicSearchInteger_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:02.071864	1
CaseBasicSearchInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:01.853414	1
CaseBasicSearchInteger_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:02.446995	1
CaseBasicSearchInteger_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:02.231352	1
CaseBasicSearchInteger_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:02.884259	1
CaseBasicSearchInteger_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:02.665653	1
CaseComparisonsApproximateNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:03.103581	1
CaseComparisonsApproximateNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:03.292418	1
CaseComparisonsApproximateNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:03.509966	1
CaseComparisonsApproximateNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:03.697137	1
CaseComparisonsApproximateNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:03.915794	1
CaseComparisonsApproximateNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:04.134371	1
CaseComparisonsExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:04.509785	1
CaseComparisonsExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:04.321841	1
CaseComparisonsExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:04.947387	1
CaseComparisonsExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:04.728498	1
CaseComparisonsInteger_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:05.853119	1
CaseComparisonsInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:05.634994	1
CaseComparisonsInteger_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:06.228367	1
CaseComparisonsInteger_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:06.040863	1
CaseComparisonsInteger_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:06.634822	1
CaseComparisonsInteger_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:06.44718	1
CaseNestedApproximateNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:07.07192	1
CaseNestedApproximateNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:06.853532	1
CaseNestedApproximateNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:07.572547	1
CaseNestedApproximateNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:07.321951	1
CaseNestedApproximateNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:07.978408	1
CaseNestedApproximateNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:07.759602	1
CaseNestedExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:08.199166	1
CaseNestedExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:08.415973	1
CaseNestedExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:08.634597	1
CaseNestedExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:08.82343	1
CaseNestedInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:09.009306	1
CaseNestedInteger_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:09.228185	1
CaseNestedInteger_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:09.446947	1
CaseNestedInteger_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:09.636915	1
CaseNestedInteger_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:09.853236	1
CaseNestedInteger_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:10.009289	1
CaseSubqueryApproxmiateNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:10.229293	1
CaseSubqueryApproxmiateNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:10.44732	1
CaseSubqueryApproxmiateNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:10.634456	1
CaseSubqueryApproxmiateNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:10.821831	1
CaseSubqueryApproxmiateNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:11.044306	1
CaseSubqueryApproxmiateNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:11.261262	1
CaseSubQueryExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:11.697766	1
CaseSubQueryExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:11.482978	1
CaseSubQueryExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:12.103616	1
CaseSubQueryExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:11.918539	1
CaseSubQueryInteger_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:12.541091	1
CaseSubQueryInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:12.32256	1
CaseSubQueryInteger_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:12.886055	1
CaseSubQueryInteger_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:12.696859	1
CaseSubQueryInteger_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:13.376182	1
CaseSubQueryInteger_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:13.106363	1
CastBigintToBigint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:13.790751	1
CastBigintToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:13.573852	1
CastBigintToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:13.977727	1
CastBigintToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:14.197855	1
CastBigintToDecimal_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:14.603046	1
CastBigintToDecimal_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:14.38427	1
CastBigintToDouble_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:14.978273	1
CastBigintToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:14.758988	1
CastBigintToFloat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:15.16676	1
CastBigintToFloat_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:15.35312	1
CastBigintToInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:15.540268	1
CastBigintToInteger_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:15.761334	1
CastBigintToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:15.977762	1
CastBigintToSmallint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:16.198927	1
CastBigintToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:16.635208	1
CastBigintToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:16.415385	1
CastCharsToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:17.040901	1
CastCharsToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:16.852615	1
CastCharsToChar_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:17.446644	1
CastCharsToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:17.259541	1
CastCharsToDate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:17.789927	1
CastCharsToChar_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:17.602959	1
CastCharsToFloat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:18.168615	1
CastCharsToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:17.979145	1
CastCharsToInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:18.388768	1
CastCharsToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:18.978715	1
CastCharsToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:18.604843	1
CastCharsToVarchar_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:19.384084	1
CastCharsToTimestamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:18.790174	1
CastDateToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:19.821626	1
CastCharsToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:19.19661	1
CastDateToDate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:20.259165	1
CastCharsToVarchar_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:19.602693	1
CastDateToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:20.884061	1
CastDateToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:20.040405	1
CastDecimalToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:21.104889	1
CastDateToDate_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:20.448094	1
CastDecimalToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:21.759376	1
CastDateToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:20.668399	1
CastDecimalToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:22.041063	1
CastDecimalToBigint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:21.322479	1
CastDecimalToFloat_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:22.66569	1
CastDecimalToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:21.540608	1
CastDecimalToInteger_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:23.104273	1
CastDecimalToDouble_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:22.260892	1
CastDecimalToSmallint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:23.540124	1
CastDecimalToFloat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:22.446729	1
CastDecimalToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:23.790392	1
CastDecimalToInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:22.884033	1
CastDoubleToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:24.260852	1
CastDecimalToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:23.321567	1
CastDoubleToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:24.885609	1
CastDecimalToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:24.009587	1
CastDoubleToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:25.071427	1
CastDoubleToBigint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:24.477641	1
CastDoubleToFloat_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:25.633967	1
CastDoubleToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:24.69681	1
CastDoubleToSmallint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:26.010826	1
CastDoubleToDouble_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:25.258803	1
CastDoubleToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:26.165589	1
CastDoubleToFloat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:25.446305	1
CastFloatToBigint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:26.792694	1
CastDoubleToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:25.821974	1
CastFloatToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:27.010833	1
CastDoubleToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:26.385813	1
CastFloatToDouble_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:27.635335	1
CastFloatToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:26.572227	1
CastFloatToFloat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:27.821882	1
CastFloatToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:27.230949	1
CastFloatToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:28.258934	1
CastFloatToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:27.415263	1
CastFloatToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:28.852519	1
CastFloatToFloat_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:28.040868	1
CastIntegerToBigint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:29.227505	1
CastFloatToSmallint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:28.479656	1
CastIntegerToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:29.416421	1
CastFloatToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:28.696555	1
CastIntegerToDouble_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:30.010317	1
CastIntegerToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:29.03997	1
CastIntegerToFloat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:30.227509	1
CastIntegerToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:29.633858	1
CastIntegerToInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:30.602398	1
CastIntegerToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:29.821941	1
CastIntegerToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:31.009066	1
CastIntegerToFloat_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:30.425998	1
CastIntegerToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:31.571438	1
CastIntegerToInteger_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:30.789941	1
CastNumericToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:31.790093	1
CastIntegerToSmallint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:31.196022	1
CastNumericToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:32.383844	1
CastIntegerToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:31.352376	1
CastNumericToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:32.60283	1
CastNumericToBigint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:31.946251	1
CastNumericToFloat_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:33.133778	1
CastNumericToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:32.166773	1
CastNumericToInteger_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:33.477363	1
CastNumericToDouble_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:32.758744	1
CastNumericToSmallint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:33.883654	1
CastNumericToFloat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:32.946079	1
CastNumericToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:34.071335	1
CastNumericToInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:33.290312	1
CastNvarcharToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:34.414838	1
CastNumericToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:33.697957	1
CastNvarcharToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:34.602076	1
CastNumericToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:34.258835	1
CastRealToBigint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:35.352605	1
CastNvarcharToInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:34.758406	1
CastRealToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:35.508803	1
CastNvarcharToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:34.946144	1
CastRealToDouble_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:36.071053	1
CastRealToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:35.133714	1
CastRealToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:36.291051	1
CastRealToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:35.665037	1
CastRealToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:36.883621	1
CastRealToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:35.85316	1
CastSmallintToBigint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:37.321084	1
CastRealToSmallint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:36.509442	1
CastSmallintToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:37.508908	1
CastRealToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:36.696633	1
CastSmallintToDouble_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:38.103966	1
CastSmallintToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:37.102404	1
CastSmallintToFloat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:38.25872	1
CastSmallintToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:37.665172	1
CastSmallintToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:38.698166	1
CastSmallintToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:37.883479	1
CastSmallintToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:39.383499	1
CastSmallintToFloat_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:38.477378	1
CastTimestampToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:39.575257	1
CastSmallintToSmallint_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:38.953111	1
CastTimestampToDate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:40.00906	1
CastSmallintToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:39.167044	1
CastTimestampToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:40.602986	1
CastTimestampToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:39.790352	1
CastTimeToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:41.01062	1
CastTimestampToDate_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:40.229719	1
CastTimeToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:41.164582	1
CastTimestampToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:40.415497	1
CastVarcharToBigint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:41.539377	1
CastTimeToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:40.790014	1
CastVarcharToChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:41.946073	1
CastTimeToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:41.352155	1
CastVarcharToDate_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:42.322826	1
CastVarcharToChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:41.727738	1
CastVarcharToDate_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:42.761314	1
CastVarcharToDate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:42.133263	1
CastVarcharToDouble_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:43.197776	1
CastVarcharToDate_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:42.540443	1
CastVarcharToFloat_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:43.601841	1
CastVarcharToDate_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:42.978718	1
CastVarcharToVarchar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:44.385311	1
CastVarcharToFloat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:43.383262	1
CeilCoreApproximateNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:44.977355	1
CastVarcharToInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:43.820644	1
CeilCoreApproximateNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:45.383805	1
CastVarcharToSmallint_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:44.008177	1
CeilCoreApproximateNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:45.790476	1
CastVarcharToTimestamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:44.227023	1
CeilCoreExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:45.949331	1
CastVarcharToVarchar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:44.603954	1
CeilCoreExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:46.321103	1
CeilCoreApproximateNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:44.791146	1
CeilCoreIntegers_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:46.916337	1
CeilCoreApproximateNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:45.164769	1
CeilCoreIntegers_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:47.320977	1
CeilCoreApproximateNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:45.602233	1
CeilCoreIntegers_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:47.758457	1
CeilCoreExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:46.133433	1
CharacterLiteral_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:48.166371	1
CeilCoreExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:46.508367	1
CharlengthCoreFixedLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:48.762609	1
CeilCoreIntegers_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:46.69618	1
CharlengthCoreVariableLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:49.169076	1
CeilCoreIntegers_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:47.102341	1
CoalesceCore_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:49.949786	1
CeilCoreIntegers_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:47.539517	1
ConcatCoreFixedLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:50.575745	1
CharacterLiteral_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:48.383186	1
ConcatCoreMaxLengthStringPlusBlankString_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:50.981789	1
CharlengthCoreFixedLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:48.570914	1
ConcatCoreMaxLengthStringPlusBlankString_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:51.387721	1
CharlengthCoreVariableLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:48.981403	1
ConcatCoreVariableLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:51.999333	1
CoalesceCoreNullParameters_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:49.383167	1
ConcatException_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:53.605021	1
Comments1_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:50.16887	1
ConcatException_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:54.41539	1
ConcatCoreFixedLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:50.352689	1
ConcatException_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:54.852628	1
ConcatCoreMaxLengthStringPlusBlankString_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:50.789359	1
ConcatException_p8	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:55.289885	1
ConcatCoreMaxLengthStringPlusBlankString_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:51.195829	1
CountCharLiteral_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:55.508706	1
ConcatCoreVariableLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:51.539632	1
CountCharLiteral_p10	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:55.726965	1
ConcatException_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:53.275401	1
CountCharLiteral_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:56.226499	1
ConcatException_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:54.196479	1
CountCharLiteral_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:56.635071	1
ConcatException_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:54.633931	1
CountCharLiteral_p7	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:57.039162	1
ConcatException_p7	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:55.071406	1
CountCharLiteral_p9	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:57.477829	1
CountCharLiteral_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:55.999329	1
CountClob_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:57.915242	1
CountCharLiteral_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:56.414382	1
CountNumericLiteral_p11	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:58.570874	1
CountCharLiteral_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:56.853721	1
CountNumericLiteral_p13	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:59.008146	1
CountCharLiteral_p8	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:57.258061	1
CountNumericLiteral_p15	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:59.445651	1
CountClob_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:57.696002	1
CountNumericLiteral_p17	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:59.883512	1
CountNumericLiteral_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:58.132891	1
CountNumericLiteral_p19	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:00.320485	1
CountNumericLiteral_p10	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:58.351806	1
CountNumericLiteral_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:00.540611	1
CountNumericLiteral_p12	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:58.760036	1
CountNumericLiteral_p20	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:00.757893	1
CountNumericLiteral_p14	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:59.227911	1
CountNumericLiteral_p22	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:01.165917	1
CountNumericLiteral_p16	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:20:59.664049	1
CountNumericLiteral_p24	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:01.601319	1
CountNumericLiteral_p18	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:00.103158	1
CountNumericLiteral_p26	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:02.039068	1
CountNumericLiteral_p21	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:00.976781	1
CountNumericLiteral_p28	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:02.507945	1
CountNumericLiteral_p23	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:01.384017	1
CountNumericLiteral_p31	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:03.32227	1
CountNumericLiteral_p25	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:01.821117	1
CountNumericLiteral_p33	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:03.75997	1
CountNumericLiteral_p27	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:02.289268	1
CountNumericLiteral_p35	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:04.195301	1
CountNumericLiteral_p29	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:02.695441	1
CountNumericLiteral_p37	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:04.632928	1
CountNumericLiteral_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:02.91399	1
CountNumericLiteral_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:04.853487	1
CountNumericLiteral_p30	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:03.101483	1
CountNumericLiteral_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:05.258086	1
CountNumericLiteral_p32	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:03.543349	1
CountNumericLiteral_p8	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:05.695227	1
CountNumericLiteral_p34	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:03.977643	1
CountTemporalLiteral_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:06.132739	1
CountNumericLiteral_p36	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:04.414235	1
CountTemporalLiteral_p10	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:06.3513	1
CountNumericLiteral_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:05.070225	1
CountTemporalLiteral_p12	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:06.789129	1
CountNumericLiteral_p7	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:05.476516	1
CountTemporalLiteral_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:07.226558	1
CountNumericLiteral_p9	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:05.913816	1
CountTemporalLiteral_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:07.663976	1
CountTemporalLiteral_p11	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:06.570237	1
CountTemporalLiteral_p7	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:08.101428	1
CountTemporalLiteral_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:07.007756	1
CountTemporalLiteral_p9	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:08.550775	1
CountTemporalLiteral_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:07.445499	1
CountValueExpression_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:08.758496	1
CountTemporalLiteral_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:07.88264	1
DistinctAggregateInCase_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:09.195359	1
CountTemporalLiteral_p8	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:08.320876	1
DistinctAggregateInCase_p10	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:09.416137	1
DateLiteral_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:08.976535	1
DistinctAggregateInCase_p12	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:09.851606	1
DistinctAggregateInCase_p11	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:09.632536	1
DistinctAggregateInCase_p14	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:10.289221	1
DistinctAggregateInCase_p13	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:10.07037	1
DistinctAggregateInCase_p16	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:10.728	1
DistinctAggregateInCase_p15	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:10.507623	1
DistinctAggregateInCase_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:11.164463	1
DistinctAggregateInCase_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:10.947263	1
DistinctAggregateInCase_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:11.601676	1
DistinctAggregateInCase_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:11.383081	1
DistinctAggregateInCase_p7	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:12.039921	1
DistinctAggregateInCase_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:11.820659	1
DistinctAggregateInCase_p9	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:12.479072	1
DistinctAggregateInCase_p8	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:12.257562	1
EmptyStringIsNull_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:13.16933	1
DistinctCore_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:12.695543	1
ExactNumericOpAdd_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:13.390277	1
EmptyStringIsNull_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:12.915775	1
ExactNumericOpAdd_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:13.791315	1
ExactNumericOpAdd_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:13.602193	1
ExactNumericOpDiv_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:14.226867	1
ExactNumericOpAdd_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:14.011817	1
ExactNumericOpDiv_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:14.664279	1
ExactNumericOpDiv_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:14.445351	1
ExactNumericOpMulNULL_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:15.101079	1
ExactNumericOpDiv_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:14.883687	1
ExactNumericOpMulNULL_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:15.539663	1
ExactNumericOpMulNULL_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:15.31987	1
ExactNumericOpMulNULL_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:15.945012	1
ExactNumericOpMulNULL_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:15.758092	1
ExactNumericOpMul_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:16.383087	1
ExactNumericOpMul_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:16.165342	1
ExactNumericOpMul_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:16.789522	1
ExactNumericOpMul_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:16.570436	1
ExactNumericOpSub_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:17.226472	1
ExactNumericOpSub_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:17.009948	1
ExactNumericOpSub_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:17.666369	1
ExactNumericOpSub_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:17.446756	1
ExceptAll_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:17.886123	1
Except_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:18.103137	1
ExpCoreApproximateNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:18.540493	1
ExpCoreApproximateNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:18.32036	1
ExpCoreApproximateNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:18.976466	1
ExpCoreApproximateNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:18.758824	1
ExpCoreApproximateNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:19.41682	1
ExpCoreApproximateNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:19.195016	1
ExpCoreExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:19.632679	1
ExpCoreExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:19.853778	1
ExpCoreExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:20.072805	1
ExpCoreExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:20.288949	1
ExpCoreIntegers_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:20.726396	1
ExpCoreIntegers_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:20.508155	1
ExpCoreIntegers_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:21.16762	1
ExpCoreIntegers_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:20.945537	1
ExpCoreIntegers_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:21.601557	1
ExpCoreIntegers_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:21.382777	1
ExpressionInIn_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:21.790633	1
ExpressionUsingAggregate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:22.008965	1
ExtractCoreDayFromDate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:22.226285	1
ExtractCoreDayFromDate_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:22.445253	1
ExtractCoreDayFromTimestamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:22.663986	1
ExtractCoreDayFromTimestamp_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:22.852166	1
ExtractCoreHourFromTimestamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:23.07033	1
ExtractCoreHourFromTimestamp_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:23.289445	1
ExtractCoreHourFromTime_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:23.759492	1
ExtractCoreHourFromTime_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:23.541175	1
ExtractCoreMinuteFromTimestamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:24.007726	1
ExtractCoreMinuteFromTimestamp_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:24.320316	1
ExtractCoreMinuteFromTime_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:24.699389	1
ExtractCoreMinuteFromTime_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:24.538541	1
ExtractCoreMonthFromDate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:24.913963	1
ExtractCoreMonthFromDate_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:25.133197	1
ExtractCoreMonthFromTimestamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:25.352822	1
ExtractCoreMonthFromTimestamp_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:25.570372	1
ExtractCoreSecondFromTimestamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:25.789238	1
ExtractCoreSecondFromTimestamp_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:26.007965	1
ExtractCoreSecondFromTime_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:26.444942	1
ExtractCoreSecondFromTime_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:26.226119	1
ExtractCoreYearFromDate_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:26.88247	1
ExtractCoreYearFromDate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:26.66405	1
ExtractCoreYearFromTimestamp_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:27.321952	1
ExtractCoreYearFromTimestamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:27.101387	1
FloorCoreApproximateNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:27.539078	1
FloorCoreApproximateNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:27.766501	1
FloorCoreApproximateNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:27.979954	1
FloorCoreApproximateNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:28.194924	1
FloorCoreApproximateNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:28.415568	1
FloorCoreApproximateNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:28.640742	1
FloorCoreExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:29.101422	1
FloorCoreExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:28.886246	1
FloorCoreExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:29.539216	1
FloorCoreExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:29.322156	1
FloorCoreIntegers_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:29.757592	1
FloorCoreIntegers_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:30.009438	1
FloorCoreIntegers_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:30.328445	1
FloorCoreIntegers_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:30.637678	1
FloorCoreIntegers_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:30.947715	1
FloorCoreIntegers_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:31.295506	1
GroupByAlias_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:31.507787	1
GroupByExpr_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:31.773227	1
GroupByMultiply_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:32.946046	1
GroupByHaving_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:32.132842	1
GroupBy_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:33.601423	1
GroupByLiteral_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:32.460636	1
IntegerOpAdd_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:34.166521	1
GroupByMany_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:32.702021	1
IntegerOpAdd_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:34.640486	1
GroupByOrdinal_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:33.352329	1
IntegerOpAdd_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:35.19474	1
Having_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:33.85369	1
IntegerOpDiv_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:35.633076	1
IntegerOpAdd_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:34.422365	1
IntegerOpDiv_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:36.167386	1
IntegerOpAdd_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:34.851862	1
IntegerOpDiv_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:36.601237	1
IntegerOpAdd_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:35.41747	1
IntegerOpMulNULL_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:37.134074	1
IntegerOpDiv_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:35.851393	1
IntegerOpMulNULL_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:37.575833	1
IntegerOpDiv_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:36.383984	1
IntegerOpMulNULL_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:38.103569	1
IntegerOpDiv_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:36.820722	1
IntegerOpMul_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:38.573992	1
IntegerOpMulNULL_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:37.35993	1
IntegerOpMul_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:39.169239	1
IntegerOpMulNULL_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:37.796029	1
IntegerOpMul_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:39.63804	1
IntegerOpMul_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:38.32282	1
IntegerOpSub_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:40.07449	1
IntegerOpMul_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:38.797377	1
IntegerOpSub_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:40.788411	1
IntegerOpMul_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:39.413497	1
IntegerOpSub_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:41.226227	1
IntegerOpSub_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:39.859451	1
IntersectAll_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:41.445968	1
IntegerOpSub_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:40.576137	1
IsNullPredicate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:41.883931	1
IntegerOpSub_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:41.008057	1
JoinCoreCrossProduct_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:42.320652	1
Intersect_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:41.66341	1
JoinCoreEqualWithAnd_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:42.758769	1
IsNullValueExpr_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:42.101425	1
JoinCoreImplicit_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:43.007652	1
JoinCoreCross_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:42.539821	1
JoinCoreNatural_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:43.944831	1
JoinCoreIsNullPredicate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:43.22805	1
JoinCoreNonEquiJoin_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:45.462761	1
JoinCoreLeftNestedInnerTableRestrict_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:43.445817	1
JoinCoreOnConditionSubstringFunction_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:47.230233	1
JoinCoreLeftNestedOptionalTableRestrict_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:43.695479	1
JoinCoreOrPredicate_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:48.789745	1
JoinCoreNestedInnerOuter_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:44.166162	1
JoinCoreRightNestedInnerTableRestrict_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:49.638245	1
JoinCoreNestedOuterInnerTableRestrict_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:44.448847	1
JoinCoreRightNestedOptionalTableRestrict_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:49.915435	1
JoinCoreNestedOuterOptionalTableRestrict_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:44.698351	1
JoinCoreTwoSidedJoinRestrictionFilter_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:50.760266	1
JoinCoreNestedOuter_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:44.945186	1
JoinCoreTwoSidedJoinRestrict_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:51.022053	1
JoinCoreNoExpressionInOnCondition_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:45.230838	1
LnCoreNull_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:52.069341	1
JoinCoreNonEquiJoin_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:45.695178	1
LnCore_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:52.694343	1
JoinCoreNonJoinNonEquiJoin_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:45.914949	1
LowerCoreFixedLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:53.350977	1
JoinCoreNotPredicate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:46.134265	1
LowerCoreVariableLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:54.351068	1
JoinCoreNwayJoinedTable_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:46.395519	1
ModCore2ExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:57.757009	1
JoinCoreOnConditionAbsFunction_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:46.688932	1
ModCore2ExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:58.225683	1
JoinCoreOnConditionSetFunction_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:46.924026	1
ModCore2Integers_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:58.475583	1
JoinCoreOnConditionTrimFunction_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:47.451999	1
ModCore2Integers_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:58.913115	1
JoinCoreOnConditionUpperFunction_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:47.664836	1
ModCore2Integers_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:59.382289	1
JoinCoreOptionalTableFilter_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:47.884912	1
ModCoreExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:00.132099	1
JoinCoreOptionalTableJoinFilter_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:48.102313	1
ModCoreExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:00.569568	1
JoinCoreOptionalTableJoinRestrict_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:48.322731	1
ModCoreIntegers_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:00.790886	1
JoinCoreOrPredicate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:48.573526	1
ModCoreIntegers_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:01.227621	1
JoinCorePreservedTableFilter_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:49.049653	1
ModCoreIntegers_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:01.662999	1
JoinCorePreservedTableJoinFilter_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:49.322543	1
Negate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:02.508034	1
JoinCoreSelf_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:50.194446	1
NullifCoreReturnsNull_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:02.949243	1
JoinCoreSimpleAndJoinedTable_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:50.484423	1
NullifCoreReturnsOne_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:03.381496	1
JoinCoreUsing_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:51.287984	1
NumericComparisonGreaterThanOrEqual_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:03.822861	1
LikeValueExpr_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:51.694874	1
NumericComparisonLessThan_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:04.506389	1
LnCore_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:52.381625	1
NumericLiteral_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:05.006648	1
LnCore_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:53.006525	1
OlapCoreAvgMultiplePartitions_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:05.289299	1
LowerCoreFixedLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:53.695206	1
OlapCoreCountNoWindowFrame_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:06.323634	1
LowerCoreSpecial_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:54.006736	1
OlapCoreCountStar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:06.788229	1
LowerCoreVariableLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:54.694438	1
OlapCoreDenseRank_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:07.633708	1
MaxLiteralTemp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:56.038724	1
OlapCoreFirstValueNoWindowFrame_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:07.850754	1
MinLiteralTemp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:57.023509	1
OlapCoreLastValueNoWindowFrame_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:08.444852	1
ModBoundaryTinyNumber_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:57.287792	1
OlapCorePercentRank_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:10.259244	1
ModCore2ExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:57.507166	1
OlapCoreRankMultiplePartitions_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:10.508356	1
ModCore2ExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:58.007576	1
OlapCoreRowNumber_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:11.789026	1
ModCore2Integers_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:58.694758	1
OlapCoreStddevPop_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:12.319695	1
ModCore2Integers_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:59.131723	1
OlapCoreStddevSamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:12.54054	1
ModCore2Integers_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:59.63365	1
OlapCoreSumOrderby100_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:13.819632	1
ModCoreExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:21:59.851313	1
OlapCoreSum_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:14.072084	1
ModCoreExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:00.350968	1
OlapCoreVariance_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:14.289084	1
ModCoreIntegers_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:01.006676	1
OlapCoreWindowFrameRowsBetweenUnboundedFollowing_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:15.757087	1
ModCoreIntegers_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:01.444376	1
OlapCoreWindowFrameRowsBetweenUnboundedPreceding_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:16.038867	1
ModCoreIntegers_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:01.881741	1
OlapCoreWindowFrameRowsPreceding_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:16.319481	1
MultipleSumDistinct_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:02.169083	1
OrderByOrdinal_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:17.194681	1
NullifCoreReturnsNull_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:02.726397	1
PositionCoreString1Empty_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:17.787684	1
NullifCoreReturnsNull_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:03.166827	1
PositionCoreString1Empty_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:18.256559	1
NullifCoreReturnsOne_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:03.600155	1
PositionCore_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:18.537543	1
NumericComparisonGreaterThan_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:04.039235	1
PositionCore_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:19.008076	1
NumericComparisonLessThanOrEqual_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:04.258149	1
PowerCoreApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:19.881469	1
NumericComparisonNotEqual_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:04.789725	1
PowerCoreApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:20.320886	1
OlapCoreAvgNoWindowFrame_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:05.571216	1
PowerCoreApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:20.756782	1
OlapCoreCountMultiplePartitions_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:06.010178	1
PowerCoreExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:20.977192	1
OlapCoreCumedist_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:07.22608	1
PowerCoreExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:21.414935	1
OlapCoreFirstValueRowsBetween_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:08.228903	1
PowerCoreIntegers_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:22.10014	1
OlapCoreLastValueRowsBetween_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:08.819569	1
PowerCoreIntegers_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:22.568988	1
OlapCoreMax_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:09.10448	1
PowerCoreIntegers_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:23.006136	1
OlapCoreMin_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:09.351483	1
PowerCoreNegativeBaseOddExp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:23.224942	1
OlapCoreNtile_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:09.601804	1
RowSubquery_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:23.443855	1
OlapCoreRankNoWindowFrame_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:11.007072	1
ScalarSubqueryInProjList_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:23.945506	1
OlapCoreRankOrderby100_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:11.413635	1
ScalarSubquery_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:24.22514	1
OlapCoreRunningSum_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:12.0388	1
SelectCountApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:24.474941	1
OlapCoreStddev_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:12.789912	1
SelectCountApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:24.974853	1
OlapCoreSumMultiplePartitions_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:13.038933	1
SelectCountApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:25.506217	1
OlapCoreSumOfGroupedSums_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:13.540455	1
SelectCountChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:26.322607	1
OlapCoreVarPop_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:14.540542	1
SelectCountChar_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:26.756002	1
OlapCoreVarSamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:14.758787	1
SelectCountChar_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:27.225118	1
OlapCoreWindowFrameMultiplePartitions_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:14.976171	1
SelectCountDate_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:27.759048	1
OlapCoreWindowFrameRowsBetweenPrecedingFollowing_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:15.258347	1
SelectCountExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:28.225186	1
OlapCoreWindowFrameRowsBetweenPrecedingPreceding_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:15.476181	1
SelectCountExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:28.724943	1
OperatorAnd_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:16.756855	1
SelectCountInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:28.974489	1
OperatorOr_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:16.974964	1
SelectCountInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:29.443094	1
PositionCoreString1Empty_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:17.412837	1
SelectCountInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:29.945696	1
PositionCoreString1Empty_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:18.039247	1
SelectCountNullNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:30.693918	1
PositionCore_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:18.789592	1
SelectCountNull_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:30.913964	1
PositionCore_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:19.224991	1
SelectCountTimestamp_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:31.747402	1
PowerBoundary_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:19.445498	1
SelectCountTime_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:31.983643	1
PowerCoreApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:19.662772	1
SelectCountVarChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:32.478699	1
PowerCoreApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:20.10046	1
SelectDateComparisonEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:32.975674	1
PowerCoreApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:20.53987	1
SelectDateComparisonGreaterThanOrEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:33.194734	1
PowerCoreExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:21.196361	1
SelectDateComparisonGreaterThan_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:33.451387	1
PowerCoreExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:21.631368	1
SelectJapaneseColumnLower_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:34.821521	1
PowerCoreIntegers_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:21.850057	1
SelectJapaneseColumnWhere_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:35.319336	1
PowerCoreIntegers_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:22.318693	1
SelectMaxApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:36.133602	1
PowerCoreIntegers_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:22.787656	1
SelectMaxApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:36.693654	1
RowValueConstructor_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:23.726671	1
SelectMaxApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:37.320316	1
SelectCountApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:24.756227	1
SelectMaxChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:37.631297	1
SelectCountApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:25.257649	1
SelectMaxExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:38.13599	1
SelectCountApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:25.787201	1
SelectMaxExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:38.635814	1
SelectCountChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:26.068344	1
SelectMaxInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:39.381471	1
SelectCountChar_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:26.539194	1
SelectMaxInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:39.858439	1
SelectCountChar_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:26.976829	1
SelectMaxInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:40.350613	1
SelectCountDate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:27.474712	1
SelectMaxNullNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:40.789153	1
SelectCountExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:27.975109	1
SelectMaxVarChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:41.478613	1
SelectCountExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:28.444593	1
SelectMinApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:41.946986	1
SelectCountInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:29.224677	1
SelectMinApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:42.444191	1
SelectCountInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:29.694112	1
SelectMinApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:42.979856	1
SelectCountInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:30.193538	1
SelectMinChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:43.226055	1
SelectCountNullNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:30.444839	1
SelectMinExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:43.664722	1
SelectCountStar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:31.165967	1
SelectMinExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:44.072003	1
SelectCountTimestamp_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:31.433106	1
SelectMinInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:44.662107	1
SelectCountTime_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:32.259412	1
SelectMinInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:45.099137	1
SelectCountVarChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:32.741835	1
SelectMinInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:45.537353	1
SelectDateComparisonLessThanOrEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:33.761893	1
SelectMinNullNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:46.00634	1
SelectDateComparisonLessThan_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:34.007983	1
SelectMinVarChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:46.69303	1
SelectDateComparisonNotEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:34.258428	1
SelectStanDevPopApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:47.192817	1
SelectJapaneseColumnConcat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:34.538847	1
SelectStanDevPopApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:47.662692	1
SelectJapaneseColumnOrderByLocal_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:35.043826	1
SelectStanDevPopApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:48.19287	1
SelectJapaneseDistinctColumn_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:35.601078	1
SelectStanDevPopExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:48.443047	1
SelectMaxApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:35.850287	1
SelectStanDevPopExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:48.912012	1
SelectMaxApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:36.416232	1
SelectStanDevPopInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:49.662116	1
SelectMaxApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:36.977723	1
SelectStanDevPopInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:50.133703	1
SelectMaxChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:37.913729	1
SelectStanDevPopInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:50.633357	1
SelectMaxExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:38.388659	1
SelectStar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:50.883097	1
SelectMaxExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:38.884054	1
SelectSumApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:51.132286	1
SelectMaxInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:39.139236	1
SelectSumApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:51.569279	1
SelectMaxInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:39.613151	1
SelectSumApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:52.072815	1
SelectMaxInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:40.082137	1
SelectSumExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:52.787977	1
SelectMaxLit_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:40.571661	1
SelectSumExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:53.262725	1
SelectMaxNull_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:41.012316	1
SelectSumInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:53.475308	1
SelectMaxVarChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:41.227763	1
SelectSumInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:54.006847	1
SelectMinApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:41.699981	1
SelectSumInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:54.507622	1
SelectMinApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:42.225027	1
SelectThaiColumnConcat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:54.943715	1
SelectMinApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:42.740759	1
SelectThaiColumnOrderByLocal_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:55.554485	1
SelectMinChar_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:43.446185	1
SelectThaiDistinctColumn_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:56.070795	1
SelectMinExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:43.909198	1
SelectTimeComparisonGreaterThanOrEqualTo_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:56.830647	1
SelectMinExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:44.255851	1
SelectTimeComparisonGreaterThan_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:57.538024	1
SelectMinInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:44.476455	1
SelectTimeComparisonLessThanOrEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:58.405326	1
SelectMinInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:44.88083	1
SelectTimeComparisonLessThanOrEqualTo_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:59.261279	1
SelectMinInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:45.319381	1
SelectTimeComparisonLessThan_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:59.806677	1
SelectMinLit_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:45.786951	1
SelectTimeComparisonLessThan_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:00.486996	1
SelectMinNull_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:46.224567	1
SelectTimeComparisonNotEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:00.982019	1
SelectMinVarChar_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:46.443515	1
SelectTimeComparisonNotEqualTo_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:01.464052	1
SelectStanDevPopApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:46.974496	1
SelectTimestampComparisonEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:01.713437	1
SelectStanDevPopApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:47.442837	1
SelectTimestampComparisonEqualTo_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:02.177686	1
SelectStanDevPopApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:47.914017	1
SelectTimestampComparisonEqualTo_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:02.653099	1
SelectStanDevPopExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:48.693177	1
SelectTimestampComparisonGreaterThanOrEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:03.151563	1
SelectStanDevPopExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:49.13067	1
SelectTimestampComparisonGreaterThanOrEqualTo_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:03.554817	1
SelectStanDevPopInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:49.411654	1
SelectTimestampComparisonGreaterThanOrEqualTo_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:04.057164	1
SelectStanDevPopInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:49.911711	1
SelectTimestampComparisonGreaterThan_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:04.490276	1
SelectStanDevPopInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:50.412692	1
SelectTimestampComparisonGreaterThan_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:04.996702	1
SelectSumApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:51.352116	1
SelectTimestampComparisonGreaterThan_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:05.429744	1
SelectSumApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:51.82191	1
SelectTimestampComparisonLessThanOrEqualTo_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:06.146375	1
SelectSumApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:52.288442	1
SelectTimestampComparisonLessThanOrEqualTo_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:06.587306	1
SelectSumExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:52.536844	1
SelectTimestampComparisonLessThanOrEqualTo_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:07.114781	1
SelectSumExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:53.036471	1
SelectTimestampComparisonLessThan_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:07.552201	1
SelectSumInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:53.726721	1
SelectTimestampComparisonLessThan_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:07.958833	1
SelectSumInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:54.257092	1
SelectTimestampComparisonLessThan_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:08.364418	1
SelectSumInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:54.755908	1
SelectTimestampComparisonNotEqualTo_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:08.802337	1
SelectThaiColumnLower_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:55.167748	1
SelectTimestampComparisonNotEqualTo_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:09.208651	1
SelectThaiColumnWhere_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:55.851532	1
SelectTimestampComparisonNotEqualTo_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:09.645989	1
SelectTimeComparisonEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:56.35588	1
SelectTurkishColumnConcat_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:09.902328	1
SelectTimeComparisonGreaterThanOrEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:56.570022	1
SelectTurkishColumnOrderByLocal_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:10.367278	1
SelectTimeComparisonGreaterThanOrEqualTo_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:57.070831	1
SelectTurkishDistinctColumn_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:10.801956	1
SelectTimeComparisonGreaterThan_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:57.290658	1
SelectVarPopApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:11.239094	1
SelectTimeComparisonGreaterThan_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:57.767707	1
SelectVarPopApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:11.708105	1
SelectTimeComparisonLessThanOrEqualTo_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:22:58.851317	1
SelectVarPopApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:12.176715	1
SelectTimeComparisonLessThan_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:00.138858	1
SelectVarPopExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:12.489478	1
SelectTimeComparisonNotEqualTo_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:01.231875	1
SelectVarPopExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:13.052313	1
SelectTimestampComparisonEqualTo_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:01.928284	1
SelectVarPopInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:13.770751	1
SelectTimestampComparisonEqualTo_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:02.435286	1
SelectVarPopInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:14.270651	1
SelectTimestampComparisonEqualTo_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:02.930149	1
SelectVarPopInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:14.740265	1
SelectTimestampComparisonGreaterThanOrEqualTo_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:03.395912	1
SetPrecedenceNoBrackets_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:14.989841	0
SelectTimestampComparisonGreaterThanOrEqualTo_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:03.80629	1
SimpleCaseApproximateNumericElseDefaultsNULL_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:15.709317	1
SelectTimestampComparisonGreaterThanOrEqualTo_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:04.275083	1
SimpleCaseApproximateNumericElseDefaultsNULL_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:16.146712	1
SelectTimestampComparisonGreaterThan_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:04.712195	1
SimpleCaseApproximateNumericElseDefaultsNULL_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:16.646004	1
SelectTimestampComparisonGreaterThan_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:05.212526	1
SimpleCaseApproximateNumericElseExplicitNULL_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:17.208559	1
SelectTimestampComparisonGreaterThan_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:05.647683	1
SimpleCaseApproximateNumericElseExplicitNULL_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:17.646438	1
SelectTimestampComparisonLessThanOrEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:05.928291	1
SimpleCaseApproximateNumericElseExplicitNULL_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:18.08365	1
SelectTimestampComparisonLessThanOrEqualTo_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:06.365956	1
SimpleCaseApproximateNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:18.521105	1
SelectTimestampComparisonLessThanOrEqualTo_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:06.833324	1
SimpleCaseApproximateNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:19.083419	1
SelectTimestampComparisonLessThan_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:07.333471	1
SimpleCaseApproximateNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:19.427666	1
SelectTimestampComparisonLessThan_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:07.740028	1
SimpleCaseExactNumericElseDefaultsNULL_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:19.646296	1
SelectTimestampComparisonLessThan_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:08.146141	1
SimpleCaseExactNumericElseDefaultsNULL_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:20.114734	1
SelectTimestampComparisonNotEqualTo_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:08.583399	1
SimpleCaseExactNumericElseExplicitNULL_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:20.49007	1
SelectTimestampComparisonNotEqualTo_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:09.021343	1
SimpleCaseExactNumericElseExplicitNULL_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:21.052116	1
SelectTimestampComparisonNotEqualTo_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:09.427062	1
SimpleCaseExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:21.489977	1
SelectTurkishColumnLower_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:10.150534	1
SimpleCaseExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:21.927162	1
SelectTurkishColumnWhere_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:10.583265	1
SimpleCaseIntegerElseDefaultsNULL_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:22.333514	1
SelectVarPopApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:11.020719	1
SimpleCaseIntegerElseDefaultsNULL_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:22.774344	1
SelectVarPopApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:11.458511	1
SimpleCaseIntegerElseDefaultsNULL_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:23.177842	1
SelectVarPopApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:11.958472	1
SimpleCaseIntegerElseExplicitNULL_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:23.615889	1
SelectVarPopExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:12.802391	1
SimpleCaseIntegerElseExplicitNULL_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:24.114515	1
SelectVarPopExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:13.271026	1
SimpleCaseIntegerElseExplicitNULL_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:24.520568	1
SelectVarPopInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:13.490123	1
SimpleCaseInteger_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:24.990482	1
SelectVarPopInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:14.020391	1
SimpleCaseInteger_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:25.395893	1
SelectVarPopInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:14.4897	1
SimpleCaseInteger_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:25.801984	1
SetPrecedenceUnionFirst_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:15.239511	1
SqrtCoreNull_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:26.238965	1
SimpleCaseApproximateNumericElseDefaultsNULL_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:15.489967	1
SqrtCore_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:26.583769	1
SimpleCaseApproximateNumericElseDefaultsNULL_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:15.958774	1
StanDevApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:27.020385	1
SimpleCaseApproximateNumericElseDefaultsNULL_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:16.397475	1
StanDevApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:27.457859	1
SimpleCaseApproximateNumericElseExplicitNULL_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:16.927289	1
StanDevApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:27.833017	1
SimpleCaseApproximateNumericElseExplicitNULL_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:17.427482	1
StanDevExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:28.489334	1
SimpleCaseApproximateNumericElseExplicitNULL_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:17.864653	1
StanDevExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:28.927467	1
SimpleCaseApproximateNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:18.302229	1
StanDevInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:29.145923	1
SimpleCaseApproximateNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:18.771032	1
StanDevInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:29.582843	1
SimpleCaseApproximateNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:19.239711	1
StanDevInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:30.020562	1
SimpleCaseExactNumericElseDefaultsNULL_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:19.927032	1
StanDevSampApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:30.676447	1
SimpleCaseExactNumericElseDefaultsNULL_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:20.304134	1
StanDevSampApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:31.114298	1
SimpleCaseExactNumericElseExplicitNULL_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:20.833689	1
StanDevSampApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:31.489681	1
SimpleCaseExactNumericElseExplicitNULL_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:21.271769	1
StanDevSampExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:31.707861	1
SimpleCaseExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:21.709344	1
StanDevSampExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:32.145033	1
SimpleCaseExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:22.146319	1
StanDevSampInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:32.801495	1
SimpleCaseIntegerElseDefaultsNULL_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:22.520944	1
StanDevSampInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:33.238951	1
SimpleCaseIntegerElseDefaultsNULL_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:22.989676	1
StanDevSampInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:33.676395	1
SimpleCaseIntegerElseDefaultsNULL_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:23.39725	1
StringComparisonEq_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:33.864101	1
SimpleCaseIntegerElseExplicitNULL_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:23.865498	1
StringComparisonLtEq_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:34.52031	1
SimpleCaseIntegerElseExplicitNULL_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:24.334361	1
StringComparisonLt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:34.739303	1
SimpleCaseIntegerElseExplicitNULL_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:24.770758	1
StringComparisonNtEq_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:34.928162	1
SimpleCaseInteger_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:25.178726	1
StringPredicateBetween_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:35.301644	1
SimpleCaseInteger_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:25.58319	1
StringPredicateLike_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:35.92682	1
SimpleCaseInteger_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:26.021312	1
StringPredicateLike_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:36.363857	1
SqrtCore_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:26.427553	1
StringPredicateNotIn_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:36.770682	1
SqrtCore_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:26.801915	1
StringPredicateNotLike_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:36.990803	1
StanDevApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:27.239035	1
SubqueryColumnAlias_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:37.208264	1
StanDevApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:27.645463	1
SubqueryDerivedAliasOrderBy_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:37.552325	1
StanDevApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:28.051685	1
SubqueryDerivedAssignNames_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:37.77104	1
StanDevExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:28.270237	1
SubqueryDerivedMany_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:37.958828	1
StanDevExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:28.707496	1
SubqueryInAggregate_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:38.395604	1
StanDevInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:29.333017	1
SubqueryInHaving_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:39.020093	1
StanDevInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:29.80135	1
SubqueryPredicateIn_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:39.459976	1
StanDevInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:30.239289	1
SubqueryPredicateNotExists_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:39.708261	1
StanDevSampApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:30.457802	1
SubqueryQuantifiedPredicateAnyFromC2_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:40.301594	1
StanDevSampApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:30.895313	1
SubstringBoundary_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:40.957658	1
StanDevSampApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:31.301472	1
SubstringBoundary_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:41.397294	1
StanDevSampExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:31.926373	1
SubstringBoundary_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:41.833068	1
StanDevSampExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:32.36378	1
SubstringBoundary_p8	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:42.238945	1
StanDevSampInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:32.583977	1
SubstringCoreFixedLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:42.645307	1
StanDevSampInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:33.02006	1
SubstringCoreNestedFixedLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:43.520223	1
StanDevSampInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:33.457989	1
SubstringCoreNestedVariableLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:43.958412	1
StringComparisonGtEq_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:34.114226	1
SubstringCoreNoLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:44.364219	1
StringComparisonGt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:34.333029	1
SubstringCoreNoLength_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:44.802156	1
StringComparisonNtEq_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:35.082659	1
SubstringCoreNullParameters_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:45.207507	1
StringPredicateIn_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:35.490324	1
SubstringCoreVariableLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:45.395319	1
StringPredicateLike_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:35.70755	1
TableConstructor_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:46.269817	1
StringPredicateLike_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:36.145143	1
TrimBothCoreFixedLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:46.489168	1
StringPredicateNotBetween_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:36.552528	1
TrimBothCoreNullParameters_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:47.145006	1
SubqueryCorrelated_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:37.396371	1
TrimBothCoreVariableLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:47.363964	1
SubqueryDerived_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:38.176702	1
TrimBothException_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:47.769827	1
SubqueryInGroupBy_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:38.770338	1
TrimCore_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:48.426656	1
SubqueryPredicateExists_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:39.240343	1
TrimCore_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:48.863958	1
SubqueryQuantifiedPredicateAnyFromC1_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:40.082735	1
TrimLeadingSpacesCore_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:49.083334	1
SubstringBoundary_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:40.77299	1
TrimTrailingSpacesCore_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:49.270129	1
SubstringBoundary_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:41.178874	1
UnionAll_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:49.489908	0
SubstringBoundary_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:41.614001	1
UpperCoreFixedLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:50.1151	1
SubstringBoundary_p7	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:42.051826	1
UpperCoreSpecial_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:50.332776	1
SubstringCoreBlob_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:42.426975	1
UpperCoreVariableLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:50.77032	1
SubstringCoreFixedLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:42.895544	1
VarApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:50.988523	1
SubstringCoreNegativeStart_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:43.114586	1
VarApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:51.426027	1
SubstringCoreNestedFixedLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:43.303586	1
VarApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:51.864542	1
SubstringCoreNestedVariableLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:43.739576	1
VarExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:52.460938	1
SubstringCoreNoLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:44.145464	1
VarExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:52.894763	1
SubstringCoreNoLength_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:44.583557	1
VarInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:53.08224	1
SubstringCoreNullParameters_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:45.02012	1
VarInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:53.457044	1
SubstringCoreVariableLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:45.61403	1
VarInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:53.805075	1
SubstringValueExpr_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:45.834026	1
VarSampApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:54.457398	1
SumDistinct_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:46.051054	1
VarSampApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:54.863542	1
TrimBothCoreFixedLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:46.707863	1
VarSampApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:55.333248	1
TrimBothCoreNullParameters_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:46.92711	1
VarSampExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:55.550921	1
TrimBothCoreVariableLength_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:47.551545	1
VarSampExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:55.926295	1
TrimBothSpacesCore_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:47.988494	1
VarSampInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:56.551119	1
TrimCore_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:48.207947	1
VarSampInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:57.019931	1
TrimCore_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:48.645304	1
VarSampInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:57.45933	1
Union_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:49.710053	1
UpperCoreFixedLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:49.895522	1
UpperCoreVariableLength_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:50.55262	1
VarApproxNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:51.207351	1
VarApproxNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:51.645727	1
VarApproxNumeric_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:52.051291	1
VarExactNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:52.238621	1
VarExactNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:52.676409	1
VarInt_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:53.300817	1
VarInt_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:53.644737	1
VarInt_p6	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:54.019753	1
VarSampApproxNumeric_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:54.239491	1
VarSampApproxNumeric_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:54.64506	1
VarSampApproxNumeric_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:55.113726	1
VarSampExactNumeric_p2	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:55.76965	1
VarSampExactNumeric_p4	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:56.144938	1
VarSampInt_p1	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:56.36351	1
VarSampInt_p3	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:56.771946	1
VarSampInt_p5	PostgreSQL 8.2.14 (Greenplum Database 4.0.4.0 build 3) on x86_64-unknown-linux-gnu, compiled by GCC gcc (GCC) 4.1.2 20070115 (SUSE Linux) compiled on Nov 30 2010 16:40:34	2011-04-19 22:23:57.239687	1
\.


--
-- Data for Name: tests_stage; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tests_stage (test_name_part) FROM stdin;
AbsCoreApproximateNumeric_p2
AbsCoreApproximateNumeric_p1
AbsCoreApproximateNumeric_p4
AbsCoreApproximateNumeric_p3
AbsCoreApproximateNumeric_p6
AbsCoreApproximateNumeric_p5
AbsCoreExactNumeric_p1
AbsCoreExactNumeric_p2
AbsCoreExactNumeric_p3
AbsCoreExactNumeric_p4
AbsCoreInteger_p1
AbsCoreInteger_p2
AbsCoreInteger_p3
AbsCoreInteger_p4
AbsCoreInteger_p5
AbsCoreInteger_p6
ApproximateNumericOpAdd_p1
AggregateInExpression_p1
ApproximateNumericOpAdd_p3
ApproximateNumericOpAdd_p2
ApproximateNumericOpAdd_p5
ApproximateNumericOpAdd_p4
ApproximateNumericOpDiv_p1
ApproximateNumericOpAdd_p6
ApproximateNumericOpDiv_p3
ApproximateNumericOpDiv_p2
ApproximateNumericOpDiv_p5
ApproximateNumericOpDiv_p4
ApproximateNumericOpMulNULL_p1
ApproximateNumericOpDiv_p6
ApproximateNumericOpMulNULL_p3
ApproximateNumericOpMulNULL_p2
ApproximateNumericOpMulNULL_p5
ApproximateNumericOpMulNULL_p4
ApproximateNumericOpMul_p2
ApproximateNumericOpMul_p1
ApproximateNumericOpMul_p4
ApproximateNumericOpMul_p3
ApproximateNumericOpMul_p6
ApproximateNumericOpMul_p5
ApproximateNumericOpSub_p2
ApproximateNumericOpSub_p1
ApproximateNumericOpSub_p4
ApproximateNumericOpSub_p3
ApproximateNumericOpSub_p6
ApproximateNumericOpSub_p5
AvgApproxNumeric_p1
AvgApproxNumeric_p2
AvgApproxNumeric_p3
AvgApproxNumeric_p4
AvgApproxNumeric_p5
AvgApproxNumeric_p6
AvgExactNumeric_p2
AvgExactNumeric_p1
AvgExactNumeric_p4
AvgExactNumeric_p3
AvgIntTruncates_p2
AvgIntTruncates_p1
AvgIntTruncates_p4
AvgIntTruncates_p3
AvgIntTruncates_p6
AvgIntTruncates_p5
AvgInt_p1
AvgInt_p2
AvgInt_p3
AvgInt_p4
AvgInt_p5
AvgInt_p6
BooleanComparisonOperatorNotOperatorAnd_p1
BooleanComparisonOperatorAnd_p1
BooleanComparisonOperatorNotOperatorOr_p1
BooleanComparisonOperatorOr_p1
CaseBasicSearchApproximateNumeric_p1
CaseBasicSearchApproximateNumeric_p2
CaseBasicSearchApproximateNumeric_p3
CaseBasicSearchApproximateNumeric_p4
CaseBasicSearchApproximateNumeric_p5
CaseBasicSearchApproximateNumeric_p6
CaseBasicSearchExactNumeric_p2
CaseBasicSearchExactNumeric_p1
CaseBasicSearchExactNumeric_p4
CaseBasicSearchExactNumeric_p3
CaseBasicSearchInteger_p2
CaseBasicSearchInteger_p1
CaseBasicSearchInteger_p4
CaseBasicSearchInteger_p3
CaseBasicSearchInteger_p6
CaseBasicSearchInteger_p5
CaseComparisonsApproximateNumeric_p1
CaseComparisonsApproximateNumeric_p2
CaseComparisonsApproximateNumeric_p3
CaseComparisonsApproximateNumeric_p4
CaseComparisonsApproximateNumeric_p5
CaseComparisonsApproximateNumeric_p6
CaseComparisonsExactNumeric_p2
CaseComparisonsExactNumeric_p1
CaseComparisonsExactNumeric_p4
CaseComparisonsExactNumeric_p3
CaseComparisonsInteger_p2
CaseComparisonsInteger_p1
CaseComparisonsInteger_p4
CaseComparisonsInteger_p3
CaseComparisonsInteger_p6
CaseComparisonsInteger_p5
CaseNestedApproximateNumeric_p2
CaseNestedApproximateNumeric_p1
CaseNestedApproximateNumeric_p4
CaseNestedApproximateNumeric_p3
CaseNestedApproximateNumeric_p6
CaseNestedApproximateNumeric_p5
CaseNestedExactNumeric_p1
CaseNestedExactNumeric_p2
CaseNestedExactNumeric_p3
CaseNestedExactNumeric_p4
CaseNestedInteger_p1
CaseNestedInteger_p2
CaseNestedInteger_p3
CaseNestedInteger_p4
CaseNestedInteger_p5
CaseNestedInteger_p6
CaseSubqueryApproxmiateNumeric_p1
CaseSubqueryApproxmiateNumeric_p2
CaseSubqueryApproxmiateNumeric_p3
CaseSubqueryApproxmiateNumeric_p4
CaseSubqueryApproxmiateNumeric_p5
CaseSubqueryApproxmiateNumeric_p6
CaseSubQueryExactNumeric_p2
CaseSubQueryExactNumeric_p1
CaseSubQueryExactNumeric_p4
CaseSubQueryExactNumeric_p3
CaseSubQueryInteger_p2
CaseSubQueryInteger_p1
CaseSubQueryInteger_p4
CaseSubQueryInteger_p3
CaseSubQueryInteger_p6
CaseSubQueryInteger_p5
CastBigintToBigint_p2
CastBigintToBigint_p1
CastBigintToChar_p1
CastBigintToChar_p2
CastBigintToDecimal_p2
CastBigintToDecimal_p1
CastBigintToDouble_p2
CastBigintToDouble_p1
CastBigintToFloat_p1
CastBigintToFloat_p2
CastBigintToInteger_p1
CastBigintToInteger_p2
CastBigintToSmallint_p1
CastBigintToSmallint_p2
CastBigintToVarchar_p2
CastBigintToVarchar_p1
CastCharsToChar_p1
CastCharsToBigint_p1
CastCharsToChar_p3
CastCharsToChar_p2
CastCharsToDate_p1
CastCharsToChar_p4
CastCharsToFloat_p1
CastCharsToDouble_p1
CastCharsToInteger_p1
CastCharsToVarchar_p1
CastCharsToSmallint_p1
CastCharsToVarchar_p3
CastCharsToTimestamp_p1
CastDateToChar_p1
CastCharsToVarchar_p2
CastDateToDate_p1
CastCharsToVarchar_p4
CastDateToVarchar_p2
CastDateToChar_p2
CastDecimalToBigint_p1
CastDateToDate_p2
CastDecimalToChar_p2
CastDateToVarchar_p1
CastDecimalToDouble_p1
CastDecimalToBigint_p2
CastDecimalToFloat_p2
CastDecimalToChar_p1
CastDecimalToInteger_p2
CastDecimalToDouble_p2
CastDecimalToSmallint_p2
CastDecimalToFloat_p1
CastDecimalToVarchar_p1
CastDecimalToInteger_p1
CastDoubleToBigint_p1
CastDecimalToSmallint_p1
CastDoubleToChar_p2
CastDecimalToVarchar_p2
CastDoubleToDouble_p1
CastDoubleToBigint_p2
CastDoubleToFloat_p2
CastDoubleToChar_p1
CastDoubleToSmallint_p2
CastDoubleToDouble_p2
CastDoubleToVarchar_p1
CastDoubleToFloat_p1
CastFloatToBigint_p2
CastDoubleToSmallint_p1
CastFloatToChar_p1
CastDoubleToVarchar_p2
CastFloatToDouble_p2
CastFloatToBigint_p1
CastFloatToFloat_p1
CastFloatToChar_p2
CastFloatToSmallint_p1
CastFloatToDouble_p1
CastFloatToVarchar_p2
CastFloatToFloat_p2
CastIntegerToBigint_p2
CastFloatToSmallint_p2
CastIntegerToChar_p1
CastFloatToVarchar_p1
CastIntegerToDouble_p2
CastIntegerToBigint_p1
CastIntegerToFloat_p1
CastIntegerToChar_p2
CastIntegerToInteger_p1
CastIntegerToDouble_p1
CastIntegerToSmallint_p1
CastIntegerToFloat_p2
CastIntegerToVarchar_p2
CastIntegerToInteger_p2
CastNumericToBigint_p1
CastIntegerToSmallint_p2
CastNumericToChar_p2
CastIntegerToVarchar_p1
CastNumericToDouble_p1
CastNumericToBigint_p2
CastNumericToFloat_p2
CastNumericToChar_p1
CastNumericToInteger_p2
CastNumericToDouble_p2
CastNumericToSmallint_p2
CastNumericToFloat_p1
CastNumericToVarchar_p1
CastNumericToInteger_p1
CastNvarcharToBigint_p1
CastNumericToSmallint_p1
CastNvarcharToDouble_p1
CastNumericToVarchar_p2
CastRealToBigint_p2
CastNvarcharToInteger_p1
CastRealToChar_p1
CastNvarcharToSmallint_p1
CastRealToDouble_p2
CastRealToBigint_p1
CastRealToSmallint_p1
CastRealToChar_p2
CastRealToVarchar_p2
CastRealToDouble_p1
CastSmallintToBigint_p2
CastRealToSmallint_p2
CastSmallintToChar_p1
CastRealToVarchar_p1
CastSmallintToDouble_p2
CastSmallintToBigint_p1
CastSmallintToFloat_p1
CastSmallintToChar_p2
CastSmallintToSmallint_p1
CastSmallintToDouble_p1
CastSmallintToVarchar_p2
CastSmallintToFloat_p2
CastTimestampToChar_p1
CastSmallintToSmallint_p2
CastTimestampToDate_p1
CastSmallintToVarchar_p1
CastTimestampToVarchar_p2
CastTimestampToChar_p2
CastTimeToChar_p2
CastTimestampToDate_p2
CastTimeToVarchar_p1
CastTimestampToVarchar_p1
CastVarcharToBigint_p1
CastTimeToChar_p1
CastVarcharToChar_p2
CastTimeToVarchar_p2
CastVarcharToDate_p2
CastVarcharToChar_p1
CastVarcharToDate_p4
CastVarcharToDate_p1
CastVarcharToDouble_p1
CastVarcharToDate_p3
CastVarcharToFloat_p2
CastVarcharToDate_p5
CastVarcharToVarchar_p1
CastVarcharToFloat_p1
CeilCoreApproximateNumeric_p2
CastVarcharToInteger_p1
CeilCoreApproximateNumeric_p4
CastVarcharToSmallint_p1
CeilCoreApproximateNumeric_p6
CastVarcharToTimestamp_p1
CeilCoreExactNumeric_p1
CastVarcharToVarchar_p2
CeilCoreExactNumeric_p3
CeilCoreApproximateNumeric_p1
CeilCoreIntegers_p2
CeilCoreApproximateNumeric_p3
CeilCoreIntegers_p4
CeilCoreApproximateNumeric_p5
CeilCoreIntegers_p6
CeilCoreExactNumeric_p2
CharacterLiteral_p1
CeilCoreExactNumeric_p4
CharlengthCoreFixedLength_p2
CeilCoreIntegers_p1
CharlengthCoreVariableLength_p2
CeilCoreIntegers_p3
CoalesceCore_p1
CeilCoreIntegers_p5
ConcatCoreFixedLength_p2
CharacterLiteral_p2
ConcatCoreMaxLengthStringPlusBlankString_p2
CharlengthCoreFixedLength_p1
ConcatCoreMaxLengthStringPlusBlankString_p4
CharlengthCoreVariableLength_p1
ConcatCoreVariableLength_p2
CoalesceCoreNullParameters_p1
ConcatException_p2
Comments1_p1
ConcatException_p4
ConcatCoreFixedLength_p1
ConcatException_p6
ConcatCoreMaxLengthStringPlusBlankString_p1
ConcatException_p8
ConcatCoreMaxLengthStringPlusBlankString_p3
CountCharLiteral_p1
ConcatCoreVariableLength_p1
CountCharLiteral_p10
ConcatException_p1
CountCharLiteral_p3
ConcatException_p3
CountCharLiteral_p5
ConcatException_p5
CountCharLiteral_p7
ConcatException_p7
CountCharLiteral_p9
CountCharLiteral_p2
CountClob_p2
CountCharLiteral_p4
CountNumericLiteral_p11
CountCharLiteral_p6
CountNumericLiteral_p13
CountCharLiteral_p8
CountNumericLiteral_p15
CountClob_p1
CountNumericLiteral_p17
CountNumericLiteral_p1
CountNumericLiteral_p19
CountNumericLiteral_p10
CountNumericLiteral_p2
CountNumericLiteral_p12
CountNumericLiteral_p20
CountNumericLiteral_p14
CountNumericLiteral_p22
CountNumericLiteral_p16
CountNumericLiteral_p24
CountNumericLiteral_p18
CountNumericLiteral_p26
CountNumericLiteral_p21
CountNumericLiteral_p28
CountNumericLiteral_p23
CountNumericLiteral_p31
CountNumericLiteral_p25
CountNumericLiteral_p33
CountNumericLiteral_p27
CountNumericLiteral_p35
CountNumericLiteral_p29
CountNumericLiteral_p37
CountNumericLiteral_p3
CountNumericLiteral_p4
CountNumericLiteral_p30
CountNumericLiteral_p6
CountNumericLiteral_p32
CountNumericLiteral_p8
CountNumericLiteral_p34
CountTemporalLiteral_p1
CountNumericLiteral_p36
CountTemporalLiteral_p10
CountNumericLiteral_p5
CountTemporalLiteral_p12
CountNumericLiteral_p7
CountTemporalLiteral_p3
CountNumericLiteral_p9
CountTemporalLiteral_p5
CountTemporalLiteral_p11
CountTemporalLiteral_p7
CountTemporalLiteral_p2
CountTemporalLiteral_p9
CountTemporalLiteral_p4
CountValueExpression_p1
CountTemporalLiteral_p6
DistinctAggregateInCase_p1
CountTemporalLiteral_p8
DistinctAggregateInCase_p10
DateLiteral_p1
DistinctAggregateInCase_p12
DistinctAggregateInCase_p11
DistinctAggregateInCase_p14
DistinctAggregateInCase_p13
DistinctAggregateInCase_p16
DistinctAggregateInCase_p15
DistinctAggregateInCase_p3
DistinctAggregateInCase_p2
DistinctAggregateInCase_p5
DistinctAggregateInCase_p4
DistinctAggregateInCase_p7
DistinctAggregateInCase_p6
DistinctAggregateInCase_p9
DistinctAggregateInCase_p8
EmptyStringIsNull_p2
DistinctCore_p1
ExactNumericOpAdd_p1
EmptyStringIsNull_p1
ExactNumericOpAdd_p3
ExactNumericOpAdd_p2
ExactNumericOpDiv_p1
ExactNumericOpAdd_p4
ExactNumericOpDiv_p3
ExactNumericOpDiv_p2
ExactNumericOpMulNULL_p1
ExactNumericOpDiv_p4
ExactNumericOpMulNULL_p3
ExactNumericOpMulNULL_p2
ExactNumericOpMulNULL_p5
ExactNumericOpMulNULL_p4
ExactNumericOpMul_p2
ExactNumericOpMul_p1
ExactNumericOpMul_p4
ExactNumericOpMul_p3
ExactNumericOpSub_p2
ExactNumericOpSub_p1
ExactNumericOpSub_p4
ExactNumericOpSub_p3
ExceptAll_p1
Except_p1
ExpCoreApproximateNumeric_p2
ExpCoreApproximateNumeric_p1
ExpCoreApproximateNumeric_p4
ExpCoreApproximateNumeric_p3
ExpCoreApproximateNumeric_p6
ExpCoreApproximateNumeric_p5
ExpCoreExactNumeric_p1
ExpCoreExactNumeric_p2
ExpCoreExactNumeric_p3
ExpCoreExactNumeric_p4
ExpCoreIntegers_p2
ExpCoreIntegers_p1
ExpCoreIntegers_p4
ExpCoreIntegers_p3
ExpCoreIntegers_p6
ExpCoreIntegers_p5
ExpressionInIn_p1
ExpressionUsingAggregate_p1
ExtractCoreDayFromDate_p1
ExtractCoreDayFromDate_p2
ExtractCoreDayFromTimestamp_p1
ExtractCoreDayFromTimestamp_p2
ExtractCoreHourFromTimestamp_p1
ExtractCoreHourFromTimestamp_p2
ExtractCoreHourFromTime_p2
ExtractCoreHourFromTime_p1
ExtractCoreMinuteFromTimestamp_p1
ExtractCoreMinuteFromTimestamp_p2
ExtractCoreMinuteFromTime_p2
ExtractCoreMinuteFromTime_p1
ExtractCoreMonthFromDate_p1
ExtractCoreMonthFromDate_p2
ExtractCoreMonthFromTimestamp_p1
ExtractCoreMonthFromTimestamp_p2
ExtractCoreSecondFromTimestamp_p1
ExtractCoreSecondFromTimestamp_p2
ExtractCoreSecondFromTime_p2
ExtractCoreSecondFromTime_p1
ExtractCoreYearFromDate_p2
ExtractCoreYearFromDate_p1
ExtractCoreYearFromTimestamp_p2
ExtractCoreYearFromTimestamp_p1
FloorCoreApproximateNumeric_p1
FloorCoreApproximateNumeric_p2
FloorCoreApproximateNumeric_p3
FloorCoreApproximateNumeric_p4
FloorCoreApproximateNumeric_p5
FloorCoreApproximateNumeric_p6
FloorCoreExactNumeric_p2
FloorCoreExactNumeric_p1
FloorCoreExactNumeric_p4
FloorCoreExactNumeric_p3
FloorCoreIntegers_p1
FloorCoreIntegers_p2
FloorCoreIntegers_p3
FloorCoreIntegers_p4
FloorCoreIntegers_p5
FloorCoreIntegers_p6
GroupByAlias_p1
GroupByExpr_p1
GroupByMultiply_p1
GroupByHaving_p1
GroupBy_p1
GroupByLiteral_p1
IntegerOpAdd_p1
GroupByMany_p1
IntegerOpAdd_p3
GroupByOrdinal_p1
IntegerOpAdd_p5
Having_p1
IntegerOpDiv_p1
IntegerOpAdd_p2
IntegerOpDiv_p3
IntegerOpAdd_p4
IntegerOpDiv_p5
IntegerOpAdd_p6
IntegerOpMulNULL_p1
IntegerOpDiv_p2
IntegerOpMulNULL_p3
IntegerOpDiv_p4
IntegerOpMulNULL_p5
IntegerOpDiv_p6
IntegerOpMul_p2
IntegerOpMulNULL_p2
IntegerOpMul_p4
IntegerOpMulNULL_p4
IntegerOpMul_p6
IntegerOpMul_p1
IntegerOpSub_p2
IntegerOpMul_p3
IntegerOpSub_p4
IntegerOpMul_p5
IntegerOpSub_p6
IntegerOpSub_p1
IntersectAll_p1
IntegerOpSub_p3
IsNullPredicate_p1
IntegerOpSub_p5
JoinCoreCrossProduct_p1
Intersect_p1
JoinCoreEqualWithAnd_p1
IsNullValueExpr_p1
JoinCoreImplicit_p1
JoinCoreCross_p1
JoinCoreNatural_p1
JoinCoreIsNullPredicate_p1
JoinCoreNonEquiJoin_p1
JoinCoreLeftNestedInnerTableRestrict_p1
JoinCoreOnConditionSubstringFunction_p1
JoinCoreLeftNestedOptionalTableRestrict_p1
JoinCoreOrPredicate_p2
JoinCoreNestedInnerOuter_p1
JoinCoreRightNestedInnerTableRestrict_p1
JoinCoreNestedOuterInnerTableRestrict_p1
JoinCoreRightNestedOptionalTableRestrict_p1
JoinCoreNestedOuterOptionalTableRestrict_p1
JoinCoreTwoSidedJoinRestrictionFilter_p1
JoinCoreNestedOuter_p1
JoinCoreTwoSidedJoinRestrict_p1
JoinCoreNoExpressionInOnCondition_p1
LnCoreNull_p1
JoinCoreNonEquiJoin_p2
LnCore_p2
JoinCoreNonJoinNonEquiJoin_p1
LowerCoreFixedLength_p1
JoinCoreNotPredicate_p1
LowerCoreVariableLength_p1
JoinCoreNwayJoinedTable_p1
ModCore2ExactNumeric_p2
JoinCoreOnConditionAbsFunction_p1
ModCore2ExactNumeric_p4
JoinCoreOnConditionSetFunction_p1
ModCore2Integers_p1
JoinCoreOnConditionTrimFunction_p1
ModCore2Integers_p3
JoinCoreOnConditionUpperFunction_p1
ModCore2Integers_p5
JoinCoreOptionalTableFilter_p1
ModCoreExactNumeric_p2
JoinCoreOptionalTableJoinFilter_p1
ModCoreExactNumeric_p4
JoinCoreOptionalTableJoinRestrict_p1
ModCoreIntegers_p1
JoinCoreOrPredicate_p1
ModCoreIntegers_p3
JoinCorePreservedTableFilter_p1
ModCoreIntegers_p5
JoinCorePreservedTableJoinFilter_p1
Negate_p1
JoinCoreSelf_p1
NullifCoreReturnsNull_p2
JoinCoreSimpleAndJoinedTable_p1
NullifCoreReturnsOne_p1
JoinCoreUsing_p1
NumericComparisonGreaterThanOrEqual_p1
LikeValueExpr_p1
NumericComparisonLessThan_p1
LnCore_p1
NumericLiteral_p1
LnCore_p3
OlapCoreAvgMultiplePartitions_p1
LowerCoreFixedLength_p2
OlapCoreAvgRowsBetween_p1
LowerCoreSpecial_p1
OlapCoreCountNoWindowFrame_p1
LowerCoreVariableLength_p2
OlapCoreCountStar_p1
MaxLiteralTemp_p1
OlapCoreCumedistNullOrdering_p1
MinLiteralTemp_p1
OlapCoreDenseRank_p1
ModBoundaryTinyNumber_p1
OlapCoreFirstValueNoWindowFrame_p1
ModCore2ExactNumeric_p1
OlapCoreLastValueNoWindowFrame_p1
ModCore2ExactNumeric_p3
OlapCoreNullOrder_p1
ModCore2Integers_p2
OlapCorePercentRank_p1
ModCore2Integers_p4
OlapCoreRankMultiplePartitions_p1
ModCore2Integers_p6
OlapCoreRankNullOrdering_p1
ModCoreExactNumeric_p1
OlapCoreRowNumber_p1
ModCoreExactNumeric_p3
OlapCoreStddevPop_p1
ModCoreIntegers_p2
OlapCoreStddevSamp_p1
ModCoreIntegers_p4
OlapCoreSumOrderby100_p1
ModCoreIntegers_p6
OlapCoreSum_p1
MultipleSumDistinct_p1
OlapCoreVariance_p1
NullifCoreReturnsNull_p1
OlapCoreWindowFrameRowsBetweenUnboundedFollowing_p1
NullifCoreReturnsNull_p3
OlapCoreWindowFrameRowsBetweenUnboundedPreceding_p1
NullifCoreReturnsOne_p2
OlapCoreWindowFrameRowsPreceding_p1
NumericComparisonGreaterThan_p1
OrderByOrdinal_p1
NumericComparisonLessThanOrEqual_p1
PositionCoreString1Empty_p2
NumericComparisonNotEqual_p1
PositionCoreString1Empty_p4
OlapCoreAvgNoWindowFrame_p1
PositionCore_p1
OlapCoreCountMultiplePartitions_p1
PositionCore_p3
OlapCoreCountRowsBetween_p1
PowerCoreApproxNumeric_p2
OlapCoreCumedist_p1
PowerCoreApproxNumeric_p4
OlapCoreDenseRankNullOrdering_p1
PowerCoreApproxNumeric_p6
OlapCoreFirstValueNullOrdering_p1
PowerCoreExactNumeric_p1
OlapCoreFirstValueRowsBetween_p1
PowerCoreExactNumeric_p3
OlapCoreLastValueNullOrdering_p1
PowerCoreIntegers_p2
OlapCoreLastValueRowsBetween_p1
PowerCoreIntegers_p4
OlapCoreMax_p1
PowerCoreIntegers_p6
OlapCoreMin_p1
PowerCoreNegativeBaseOddExp_p1
OlapCoreNtile_p1
RowSubquery_p1
OlapCorePercentRankNullOrdering_p1
ScalarSubqueryInProjList_p1
OlapCoreRankNoWindowFrame_p1
ScalarSubquery_p1
OlapCoreRankOrderby100_p1
SelectCountApproxNumeric_p1
OlapCoreRowNumberNullOrdering_p1
SelectCountApproxNumeric_p3
OlapCoreRunningSum_p1
SelectCountApproxNumeric_p5
OlapCoreStddev_p1
SelectCountChar_p2
OlapCoreSumMultiplePartitions_p1
SelectCountChar_p4
OlapCoreSumNullOrdering_p1
SelectCountChar_p6
OlapCoreSumOfGroupedSums_p1
SelectCountDate_p2
OlapCoreVarPop_p1
SelectCountExactNumeric_p2
OlapCoreVarSamp_p1
SelectCountExactNumeric_p4
OlapCoreWindowFrameMultiplePartitions_p1
SelectCountInt_p1
OlapCoreWindowFrameRowsBetweenPrecedingFollowing_p1
SelectCountInt_p3
OlapCoreWindowFrameRowsBetweenPrecedingPreceding_p1
SelectCountInt_p5
OlapCoreWindowFrameWindowDefinition_p1
SelectCountNullNumeric_p2
OperatorAnd_p1
SelectCountNull_p1
OperatorOr_p1
SelectCountTimestamp_p2
PositionCoreString1Empty_p1
SelectCountTime_p1
PositionCoreString1Empty_p3
SelectCountVarChar_p1
PositionCore_p2
SelectDateComparisonEqualTo_p1
PositionCore_p4
SelectDateComparisonGreaterThanOrEqualTo_p1
PowerBoundary_p1
SelectDateComparisonGreaterThan_p1
PowerCoreApproxNumeric_p1
SelectJapaneseColumnLower_p1
PowerCoreApproxNumeric_p3
SelectJapaneseColumnWhere_p1
PowerCoreApproxNumeric_p5
SelectMaxApproxNumeric_p2
PowerCoreExactNumeric_p2
SelectMaxApproxNumeric_p4
PowerCoreExactNumeric_p4
SelectMaxApproxNumeric_p6
PowerCoreIntegers_p1
SelectMaxChar_p1
PowerCoreIntegers_p3
SelectMaxExactNumeric_p1
PowerCoreIntegers_p5
SelectMaxExactNumeric_p3
RowValueConstructor_p1
SelectMaxInt_p2
SelectCountApproxNumeric_p2
SelectMaxInt_p4
SelectCountApproxNumeric_p4
SelectMaxInt_p6
SelectCountApproxNumeric_p6
SelectMaxNullNumeric_p1
SelectCountChar_p1
SelectMaxVarChar_p2
SelectCountChar_p3
SelectMinApproxNumeric_p2
SelectCountChar_p5
SelectMinApproxNumeric_p4
SelectCountDate_p1
SelectMinApproxNumeric_p6
SelectCountExactNumeric_p1
SelectMinChar_p1
SelectCountExactNumeric_p3
SelectMinExactNumeric_p1
SelectCountInt_p2
SelectMinExactNumeric_p3
SelectCountInt_p4
SelectMinInt_p2
SelectCountInt_p6
SelectMinInt_p4
SelectCountNullNumeric_p1
SelectMinInt_p6
SelectCountStar_p1
SelectMinNullNumeric_p1
SelectCountTimestamp_p1
SelectMinVarChar_p2
SelectCountTime_p2
SelectStanDevPopApproxNumeric_p2
SelectCountVarChar_p2
SelectStanDevPopApproxNumeric_p4
SelectDateComparisonLessThanOrEqualTo_p1
SelectStanDevPopApproxNumeric_p6
SelectDateComparisonLessThan_p1
SelectStanDevPopExactNumeric_p1
SelectDateComparisonNotEqualTo_p1
SelectStanDevPopExactNumeric_p3
SelectJapaneseColumnConcat_p1
SelectStanDevPopInt_p2
SelectJapaneseColumnOrderByLocal_p1
SelectStanDevPopInt_p4
SelectJapaneseDistinctColumn_p1
SelectStanDevPopInt_p6
SelectMaxApproxNumeric_p1
SelectStar_p1
SelectMaxApproxNumeric_p3
SelectSumApproxNumeric_p1
SelectMaxApproxNumeric_p5
SelectSumApproxNumeric_p3
SelectMaxChar_p2
SelectSumApproxNumeric_p5
SelectMaxExactNumeric_p2
SelectSumExactNumeric_p2
SelectMaxExactNumeric_p4
SelectSumExactNumeric_p4
SelectMaxInt_p1
SelectSumInt_p1
SelectMaxInt_p3
SelectSumInt_p3
SelectMaxInt_p5
SelectSumInt_p5
SelectMaxLit_p1
SelectThaiColumnConcat_p1
SelectMaxNull_p1
SelectThaiColumnOrderByLocal_p1
SelectMaxVarChar_p1
SelectThaiDistinctColumn_p1
SelectMinApproxNumeric_p1
SelectTimeComparisonGreaterThanOrEqualTo_p2
SelectMinApproxNumeric_p3
SelectTimeComparisonGreaterThan_p2
SelectMinApproxNumeric_p5
SelectTimeComparisonLessThanOrEqualTo_p1
SelectMinChar_p2
SelectTimeComparisonLessThanOrEqualTo_p3
SelectMinExactNumeric_p2
SelectTimeComparisonLessThan_p1
SelectMinExactNumeric_p4
SelectTimeComparisonLessThan_p3
SelectMinInt_p1
SelectTimeComparisonNotEqualTo_p1
SelectMinInt_p3
SelectTimeComparisonNotEqualTo_p3
SelectMinInt_p5
SelectTimestampComparisonEqualTo_p1
SelectMinLit_p1
SelectTimestampComparisonEqualTo_p3
SelectMinNull_p1
SelectTimestampComparisonEqualTo_p5
SelectMinVarChar_p1
SelectTimestampComparisonGreaterThanOrEqualTo_p1
SelectStanDevPopApproxNumeric_p1
SelectTimestampComparisonGreaterThanOrEqualTo_p3
SelectStanDevPopApproxNumeric_p3
SelectTimestampComparisonGreaterThanOrEqualTo_p5
SelectStanDevPopApproxNumeric_p5
SelectTimestampComparisonGreaterThan_p1
SelectStanDevPopExactNumeric_p2
SelectTimestampComparisonGreaterThan_p3
SelectStanDevPopExactNumeric_p4
SelectTimestampComparisonGreaterThan_p5
SelectStanDevPopInt_p1
SelectTimestampComparisonLessThanOrEqualTo_p2
SelectStanDevPopInt_p3
SelectTimestampComparisonLessThanOrEqualTo_p4
SelectStanDevPopInt_p5
SelectTimestampComparisonLessThanOrEqualTo_p6
SelectSumApproxNumeric_p2
SelectTimestampComparisonLessThan_p2
SelectSumApproxNumeric_p4
SelectTimestampComparisonLessThan_p4
SelectSumApproxNumeric_p6
SelectTimestampComparisonLessThan_p6
SelectSumExactNumeric_p1
SelectTimestampComparisonNotEqualTo_p2
SelectSumExactNumeric_p3
SelectTimestampComparisonNotEqualTo_p4
SelectSumInt_p2
SelectTimestampComparisonNotEqualTo_p6
SelectSumInt_p4
SelectTurkishColumnConcat_p1
SelectSumInt_p6
SelectTurkishColumnOrderByLocal_p1
SelectThaiColumnLower_p1
SelectTurkishDistinctColumn_p1
SelectThaiColumnWhere_p1
SelectVarPopApproxNumeric_p2
SelectTimeComparisonEqualTo_p1
SelectVarPopApproxNumeric_p4
SelectTimeComparisonGreaterThanOrEqualTo_p1
SelectVarPopApproxNumeric_p6
SelectTimeComparisonGreaterThanOrEqualTo_p3
SelectVarPopExactNumeric_p1
SelectTimeComparisonGreaterThan_p1
SelectVarPopExactNumeric_p3
SelectTimeComparisonGreaterThan_p3
SelectVarPopInt_p2
SelectTimeComparisonLessThanOrEqualTo_p2
SelectVarPopInt_p4
SelectTimeComparisonLessThan_p2
SelectVarPopInt_p6
SelectTimeComparisonNotEqualTo_p2
SetPrecedenceNoBrackets_p1
SelectTimestampComparisonEqualTo_p2
SimpleCaseApproximateNumericElseDefaultsNULL_p2
SelectTimestampComparisonEqualTo_p4
SimpleCaseApproximateNumericElseDefaultsNULL_p4
SelectTimestampComparisonEqualTo_p6
SimpleCaseApproximateNumericElseDefaultsNULL_p6
SelectTimestampComparisonGreaterThanOrEqualTo_p2
SimpleCaseApproximateNumericElseExplicitNULL_p2
SelectTimestampComparisonGreaterThanOrEqualTo_p4
SimpleCaseApproximateNumericElseExplicitNULL_p4
SelectTimestampComparisonGreaterThanOrEqualTo_p6
SimpleCaseApproximateNumericElseExplicitNULL_p6
SelectTimestampComparisonGreaterThan_p2
SimpleCaseApproximateNumeric_p2
SelectTimestampComparisonGreaterThan_p4
SimpleCaseApproximateNumeric_p4
SelectTimestampComparisonGreaterThan_p6
SimpleCaseApproximateNumeric_p6
SelectTimestampComparisonLessThanOrEqualTo_p1
SimpleCaseExactNumericElseDefaultsNULL_p1
SelectTimestampComparisonLessThanOrEqualTo_p3
SimpleCaseExactNumericElseDefaultsNULL_p3
SelectTimestampComparisonLessThanOrEqualTo_p5
SimpleCaseExactNumericElseExplicitNULL_p1
SelectTimestampComparisonLessThan_p1
SimpleCaseExactNumericElseExplicitNULL_p3
SelectTimestampComparisonLessThan_p3
SimpleCaseExactNumeric_p1
SelectTimestampComparisonLessThan_p5
SimpleCaseExactNumeric_p3
SelectTimestampComparisonNotEqualTo_p1
SimpleCaseIntegerElseDefaultsNULL_p1
SelectTimestampComparisonNotEqualTo_p3
SimpleCaseIntegerElseDefaultsNULL_p3
SelectTimestampComparisonNotEqualTo_p5
SimpleCaseIntegerElseDefaultsNULL_p5
SelectTurkishColumnLower_p1
SimpleCaseIntegerElseExplicitNULL_p1
SelectTurkishColumnWhere_p1
SimpleCaseIntegerElseExplicitNULL_p3
SelectVarPopApproxNumeric_p1
SimpleCaseIntegerElseExplicitNULL_p5
SelectVarPopApproxNumeric_p3
SimpleCaseInteger_p1
SelectVarPopApproxNumeric_p5
SimpleCaseInteger_p3
SelectVarPopExactNumeric_p2
SimpleCaseInteger_p5
SelectVarPopExactNumeric_p4
SqrtCoreNull_p1
SelectVarPopInt_p1
SqrtCore_p2
SelectVarPopInt_p3
StanDevApproxNumeric_p1
SelectVarPopInt_p5
StanDevApproxNumeric_p3
SetPrecedenceUnionFirst_p1
StanDevApproxNumeric_p5
SimpleCaseApproximateNumericElseDefaultsNULL_p1
StanDevExactNumeric_p2
SimpleCaseApproximateNumericElseDefaultsNULL_p3
StanDevExactNumeric_p4
SimpleCaseApproximateNumericElseDefaultsNULL_p5
StanDevInt_p1
SimpleCaseApproximateNumericElseExplicitNULL_p1
StanDevInt_p3
SimpleCaseApproximateNumericElseExplicitNULL_p3
StanDevInt_p5
SimpleCaseApproximateNumericElseExplicitNULL_p5
StanDevSampApproxNumeric_p2
SimpleCaseApproximateNumeric_p1
StanDevSampApproxNumeric_p4
SimpleCaseApproximateNumeric_p3
StanDevSampApproxNumeric_p6
SimpleCaseApproximateNumeric_p5
StanDevSampExactNumeric_p1
SimpleCaseExactNumericElseDefaultsNULL_p2
StanDevSampExactNumeric_p3
SimpleCaseExactNumericElseDefaultsNULL_p4
StanDevSampInt_p2
SimpleCaseExactNumericElseExplicitNULL_p2
StanDevSampInt_p4
SimpleCaseExactNumericElseExplicitNULL_p4
StanDevSampInt_p6
SimpleCaseExactNumeric_p2
StringComparisonEq_p1
SimpleCaseExactNumeric_p4
StringComparisonLtEq_p1
SimpleCaseIntegerElseDefaultsNULL_p2
StringComparisonLt_p1
SimpleCaseIntegerElseDefaultsNULL_p4
StringComparisonNtEq_p1
SimpleCaseIntegerElseDefaultsNULL_p6
StringPredicateBetween_p1
SimpleCaseIntegerElseExplicitNULL_p2
StringPredicateLike_p2
SimpleCaseIntegerElseExplicitNULL_p4
StringPredicateLike_p4
SimpleCaseIntegerElseExplicitNULL_p6
StringPredicateNotIn_p1
SimpleCaseInteger_p2
StringPredicateNotLike_p1
SimpleCaseInteger_p4
SubqueryColumnAlias_p1
SimpleCaseInteger_p6
SubqueryDerivedAliasOrderBy_p1
SqrtCore_p1
SubqueryDerivedAssignNames_p1
SqrtCore_p3
SubqueryDerivedMany_p1
StanDevApproxNumeric_p2
SubqueryInAggregate_p1
StanDevApproxNumeric_p4
SubqueryInHaving_p1
StanDevApproxNumeric_p6
SubqueryPredicateIn_p1
StanDevExactNumeric_p1
SubqueryPredicateNotExists_p1
StanDevExactNumeric_p3
SubqueryQuantifiedPredicateAnyFromC2_p1
StanDevInt_p2
SubqueryQuantifiedPredicateEmpty_p1
StanDevInt_p4
SubqueryQuantifiedPredicateLarge_p1
StanDevInt_p6
SubstringBoundary_p2
StanDevSampApproxNumeric_p1
SubstringBoundary_p4
StanDevSampApproxNumeric_p3
SubstringBoundary_p6
StanDevSampApproxNumeric_p5
SubstringBoundary_p8
StanDevSampExactNumeric_p2
SubstringCoreFixedLength_p1
StanDevSampExactNumeric_p4
SubstringCoreNestedFixedLength_p2
StanDevSampInt_p1
SubstringCoreNestedVariableLength_p2
StanDevSampInt_p3
SubstringCoreNoLength_p2
StanDevSampInt_p5
SubstringCoreNoLength_p4
StringComparisonGtEq_p1
SubstringCoreNullParameters_p2
StringComparisonGt_p1
SubstringCoreVariableLength_p1
StringComparisonNtEq_p2
TableConstructor_p1
StringPredicateIn_p1
TrimBothCoreFixedLength_p1
StringPredicateLike_p1
TrimBothCoreNullParameters_p2
StringPredicateLike_p3
TrimBothCoreVariableLength_p1
StringPredicateNotBetween_p1
TrimBothException_p1
SubqueryCorrelated_p1
TrimCore_p2
SubqueryDerived_p1
TrimCore_p4
SubqueryInCase_p1
TrimLeadingSpacesCore_p1
SubqueryInGroupBy_p1
TrimTrailingSpacesCore_p1
SubqueryPredicateExists_p1
UnionAll_p1
SubqueryPredicateNotIn_p1
UpperCoreFixedLength_p2
SubqueryQuantifiedPredicateAnyFromC1_p1
UpperCoreSpecial_p1
SubstringBoundary_p1
UpperCoreVariableLength_p2
SubstringBoundary_p3
VarApproxNumeric_p1
SubstringBoundary_p5
VarApproxNumeric_p3
SubstringBoundary_p7
VarApproxNumeric_p5
SubstringCoreBlob_p1
VarExactNumeric_p2
SubstringCoreFixedLength_p2
VarExactNumeric_p4
SubstringCoreNegativeStart_p1
VarInt_p1
SubstringCoreNestedFixedLength_p1
VarInt_p3
SubstringCoreNestedVariableLength_p1
VarInt_p5
SubstringCoreNoLength_p1
VarSampApproxNumeric_p2
SubstringCoreNoLength_p3
VarSampApproxNumeric_p4
SubstringCoreNullParameters_p1
VarSampApproxNumeric_p6
SubstringCoreVariableLength_p2
VarSampExactNumeric_p1
SubstringValueExpr_p1
VarSampExactNumeric_p3
SumDistinct_p1
VarSampInt_p2
TrimBothCoreFixedLength_p2
VarSampInt_p4
TrimBothCoreNullParameters_p1
VarSampInt_p6
TrimBothCoreVariableLength_p2
WithClauseDerivedTable_p1
TrimBothSpacesCore_p1
TrimCore_p1
TrimCore_p3
Union_p1
UpperCoreFixedLength_p1
UpperCoreVariableLength_p1
VarApproxNumeric_p2
VarApproxNumeric_p4
VarApproxNumeric_p6
VarExactNumeric_p1
VarExactNumeric_p3
VarInt_p2
VarInt_p4
VarInt_p6
VarSampApproxNumeric_p1
VarSampApproxNumeric_p3
VarSampApproxNumeric_p5
VarSampExactNumeric_p2
VarSampExactNumeric_p4
VarSampInt_p1
VarSampInt_p3
VarSampInt_p5
\.


--
-- Data for Name: tflt; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tflt (rnum, cflt) FROM stdin;
3	1
0	\N
1	-1
2	0
5	10
4	-0.10000000000000001
\.


--
-- Data for Name: tint; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tint (rnum, cint) FROM stdin;
3	1
0	\N
1	-1
2	0
4	10
\.


--
-- Data for Name: tiud; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tiud (rnum, c1, c2) FROM stdin;
3	PATEL     	40
0	SMITH     	10
1	JONES     	20
2	\N	30
4	WONG      	50
\.


--
-- Data for Name: tjoin1; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tjoin1 (rnum, c1, c2) FROM stdin;
1	20	25
0	10	15
2	\N	50
\.


--
-- Data for Name: tjoin2; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tjoin2 (rnum, c1, c2) FROM stdin;
3	10	FF
0	10	BB
1	15	DD
2	\N	EE
\.


--
-- Data for Name: tjoin3; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tjoin3 (rnum, c1, c2) FROM stdin;
1	15	YY
0	10	XX
\.


--
-- Data for Name: tjoin4; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tjoin4 (rnum, c1, c2) FROM stdin;
0	20	ZZ
\.


--
-- Data for Name: tlel; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlel (rnum, c1, ord) FROM stdin;
47	@@@@@                                   	20
36	vicennial                               	9
53	                                        	37
30	air@@@                                  	34
31	@@@air                                  	15
48	άσκηση                                  	16
45	0000                                    	43
50	COOP                                    	24
27	999                                     	25
32	βιοτεχνία                               	30
33	verkehrt                                	7
14	co-op                                   	26
11	C.B.A.                                  	45
8	εικονοστοιχείο                          	32
17	vice-versa                              	29
34	CO-OP                                   	50
39	vice                                    	27
12	ζυγός                                   	5
37	δίκτυο                                  	53
26	vice-admiral                            	52
3	vice versa                              	47
16	Ηλιοτρόπιο                              	36
5	έμφαση                                  	33
2	γελοίος                                 	49
51	ατού                                    	22
24	ισοψηφία                                	19
9	ευωδία                                  	13
38	δάχτυλο                                 	31
43	Ζυγός                                   	35
28	λάδι                                    	48
21	θέατρο                                  	3
18	δείγμα                                  	44
19	ημερομηνία                              	6
40	νηπιαγωγείο                             	21
25	καβγάς                                  	11
6	κώλυμα                                  	10
7	ταμείο                                  	4
4	ρεπώ                                    	28
29	μάρκα                                   	14
42	όγκος                                   	38
15	φόντο                                   	2
52	υιοθέτηση                               	41
1	μετακίνηση                              	23
46	οικονομικη                              	39
35	χειρουργός                              	17
20	χέρι                                    	42
41	ξύλινη                                  	12
22	σημαία                                  	51
23	ώς                                      	1
44	ώρα                                     	18
49	παρασκήνια                              	40
10	ψησταριά                                	8
13	ωρολογιακός                             	46
\.


--
-- Data for Name: tlel_cy_preeuro; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlel_cy_preeuro (rnum, c1, ord) FROM stdin;
47	@@@@@                                   	23
36	vicennial                               	9
53	                                        	37
30	air@@@                                  	34
31	@@@air                                  	15
16	Ηλιοτρόπιο                              	26
45	0000                                    	42
50	COOP                                    	27
27	999                                     	20
48	άσκηση                                  	30
33	verkehrt                                	7
14	co-op                                   	28
11	C.B.A.                                  	44
32	βιοτεχνία                               	43
17	vice-versa                              	18
34	CO-OP                                   	49
39	vice                                    	29
8	εικονοστοιχείο                          	5
5	έμφαση                                  	47
26	vice-admiral                            	50
3	vice versa                              	46
12	ζυγός                                   	36
37	δίκτυο                                  	33
2	γελοίος                                 	53
43	Ζυγός                                   	16
24	ισοψηφία                                	22
9	ευωδία                                  	35
38	δάχτυλο                                 	32
51	ατού                                    	31
28	λάδι                                    	48
21	θέατρο                                  	3
18	δείγμα                                  	13
19	ημερομηνία                              	6
40	νηπιαγωγείο                             	24
25	καβγάς                                  	11
6	κώλυμα                                  	10
7	ταμείο                                  	52
4	ρεπώ                                    	51
29	μάρκα                                   	14
46	οικονομικη                              	38
15	φόντο                                   	40
52	υιοθέτηση                               	4
1	μετακίνηση                              	25
22	σημαία                                  	17
35	χειρουργός                              	19
20	χέρι                                    	2
41	ξύλινη                                  	12
10	ψησταριά                                	41
23	ώς                                      	1
44	ώρα                                     	45
49	παρασκήνια                              	39
42	όγκος                                   	21
13	ωρολογιακός                             	8
\.


--
-- Data for Name: tlel_gr; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlel_gr (rnum, c1, ord) FROM stdin;
47	@@@@@                                   	23
36	vicennial                               	9
53	                                        	37
30	air@@@                                  	34
31	@@@air                                  	15
16	Ηλιοτρόπιο                              	26
45	0000                                    	42
50	COOP                                    	27
27	999                                     	20
48	άσκηση                                  	30
33	verkehrt                                	7
14	co-op                                   	28
11	C.B.A.                                  	44
32	βιοτεχνία                               	43
17	vice-versa                              	18
34	CO-OP                                   	49
39	vice                                    	29
8	εικονοστοιχείο                          	5
5	έμφαση                                  	47
26	vice-admiral                            	50
3	vice versa                              	46
12	ζυγός                                   	36
37	δίκτυο                                  	33
2	γελοίος                                 	53
43	Ζυγός                                   	16
24	ισοψηφία                                	22
9	ευωδία                                  	35
38	δάχτυλο                                 	32
51	ατού                                    	31
28	λάδι                                    	48
21	θέατρο                                  	3
18	δείγμα                                  	13
19	ημερομηνία                              	6
40	νηπιαγωγείο                             	24
25	καβγάς                                  	11
6	κώλυμα                                  	10
7	ταμείο                                  	52
4	ρεπώ                                    	51
29	μάρκα                                   	14
46	οικονομικη                              	38
15	φόντο                                   	40
52	υιοθέτηση                               	4
1	μετακίνηση                              	25
22	σημαία                                  	17
35	χειρουργός                              	19
20	χέρι                                    	2
41	ξύλινη                                  	12
10	ψησταριά                                	41
23	ώς                                      	1
44	ώρα                                     	45
49	παρασκήνια                              	39
42	όγκος                                   	21
13	ωρολογιακός                             	8
\.


--
-- Data for Name: tlel_gr_preeuro; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlel_gr_preeuro (rnum, c1, ord) FROM stdin;
47	@@@@@                                   	23
36	vicennial                               	9
53	                                        	37
30	air@@@                                  	34
31	@@@air                                  	15
16	Ηλιοτρόπιο                              	26
45	0000                                    	42
50	COOP                                    	27
27	999                                     	20
48	άσκηση                                  	30
33	verkehrt                                	7
14	co-op                                   	28
11	C.B.A.                                  	44
32	βιοτεχνία                               	43
17	vice-versa                              	18
34	CO-OP                                   	49
39	vice                                    	29
8	εικονοστοιχείο                          	5
5	έμφαση                                  	47
26	vice-admiral                            	50
3	vice versa                              	46
12	ζυγός                                   	36
37	δίκτυο                                  	33
2	γελοίος                                 	53
43	Ζυγός                                   	16
24	ισοψηφία                                	22
9	ευωδία                                  	35
38	δάχτυλο                                 	32
51	ατού                                    	31
28	λάδι                                    	48
21	θέατρο                                  	3
18	δείγμα                                  	13
19	ημερομηνία                              	6
40	νηπιαγωγείο                             	24
25	καβγάς                                  	11
6	κώλυμα                                  	10
7	ταμείο                                  	52
4	ρεπώ                                    	51
29	μάρκα                                   	14
46	οικονομικη                              	38
15	φόντο                                   	40
52	υιοθέτηση                               	4
1	μετακίνηση                              	25
22	σημαία                                  	17
35	χειρουργός                              	19
20	χέρι                                    	2
41	ξύλινη                                  	12
10	ψησταριά                                	41
23	ώς                                      	1
44	ώρα                                     	45
49	παρασκήνια                              	39
42	όγκος                                   	21
13	ωρολογιακός                             	8
\.


--
-- Data for Name: tliw; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tliw (rnum, c1, ord) FROM stdin;
15	co-op                                   	14
28	@@@@@                                   	39
17	zoo                                     	30
38	0000                                    	37
39	אביב                                    	16
36	@@@air                                  	4
37	בול                                     	29
2	999                                     	5
35	גבינה                                   	38
4	air@@@                                  	36
33	דג                                      	28
34	C.B.A.                                  	7
31	הורה                                    	27
32	CO-OP                                   	34
29	ויטמין                                  	12
6	COOP                                    	35
27	זית                                     	26
8	vice versa                              	31
25	חדשות                                   	25
26	verkehrt                                	11
23	יוגורט                                  	10
12	vice-versa                              	8
21	כביש                                    	23
10	vice                                    	33
11	פסנתר                                   	20
24	טבח                                     	24
13	עיר                                     	9
22	vice-admiral                            	15
7	קילומטר                                 	19
20	לבן                                     	1
9	צלחת                                    	6
30	vicennial                               	32
3	שאלה                                    	18
16	נהג                                     	13
5	רצפה                                    	2
18	מאפיה                                   	22
19	שלום                                    	3
14	סודה                                    	21
1	תפוח                                    	17
\.


--
-- Data for Name: tliw_il; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tliw_il (rnum, c1, ord) FROM stdin;
15	co-op                                   	14
28	@@@@@                                   	39
17	zoo                                     	30
38	0000                                    	37
39	אביב                                    	16
36	@@@air                                  	4
37	בול                                     	29
2	999                                     	5
35	גבינה                                   	38
4	air@@@                                  	36
33	דג                                      	28
34	C.B.A.                                  	7
31	הורה                                    	27
32	CO-OP                                   	34
29	ויטמין                                  	12
6	COOP                                    	35
27	זית                                     	26
8	vice versa                              	31
25	חדשות                                   	25
26	verkehrt                                	11
23	יוגורט                                  	10
12	vice-versa                              	8
21	כביש                                    	23
10	vice                                    	33
11	פסנתר                                   	20
24	טבח                                     	24
13	עיר                                     	9
22	vice-admiral                            	15
7	קילומטר                                 	19
20	לבן                                     	1
9	צלחת                                    	6
30	vicennial                               	32
3	שאלה                                    	18
16	נהג                                     	13
5	רצפה                                    	2
18	מאפיה                                   	22
19	שלום                                    	3
14	סודה                                    	21
1	תפוח                                    	17
\.


--
-- Data for Name: tlja; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlja (rnum, c1, ord) FROM stdin;
11	(1)ｲﾝﾃﾞｯｸｽ                              	36
12	<5>Switches                             	37
9	666Sink                                 	40
10	400ｒａｎｋｕ                                	39
27	エコー                                     	34
28	コート                                     	3
13	R-Bench                                 	38
14	P-Cabels                                	35
31	ズボン                                     	5
40	さんしょう                                   	6
25	ガード                                     	4
26	エチャント                                   	24
39	はっぽ                                     	43
36	せんたくざい                                  	46
29	ゴム                                      	1
30	スワップ                                    	41
35	フッコク                                    	49
32	ダイエル                                    	45
41	ざぶと                                     	2
38	はつ剤                                     	44
47	音声認識                                    	7
8	「２」計画                                   	47
37	せっけい                                    	42
34	ファイル                                    	48
43	記録機                                     	11
44	記載                                      	10
33	フィルター                                   	50
46	暗視                                      	9
7	⑤号線路                                    	21
48	国立公園                                    	18
45	音楽                                      	8
42	高機能                                     	15
3	ＰＶＤＦ                                    	19
4	ＲＯＭＡＮ-８                                 	13
49	国立大学                                    	22
50	国家利益                                    	14
15	ｱﾝｶｰ                                    	12
16	ｴﾝｼﾞﾝ                                   	30
5	（Ⅰ）番号列                                  	23
2	９８０Series                               	16
19	ｶｯﾄﾏｼﾝ                                  	29
20	ｶｰﾄﾞ                                    	28
1	３５６CAL                                  	17
6	＜ⅸ＞Pattern                              	20
23	ﾌｫﾙﾀﾞｰ                                  	33
24	ｻｲﾌ                                     	27
17	ｺﾞｰﾙﾄﾞ                                  	25
18	ｺｰﾗ                                     	26
21	ﾂｰｳｨﾝｸﾞ                                 	32
22	ﾏﾝﾎﾞ                                    	31
\.


--
-- Data for Name: tlja_jp; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlja_jp (rnum, c1, ord) FROM stdin;
11	(1)ｲﾝﾃﾞｯｸｽ                              	36
12	<5>Switches                             	37
9	666Sink                                 	40
10	400ｒａｎｋｕ                                	39
7	⑤号線路                                    	7
8	「２」計画                                   	8
13	R-Bench                                 	38
14	P-Cabels                                	35
39	はっぽ                                     	6
40	さんしょう                                   	4
41	ざぶと                                     	3
38	はつ剤                                     	5
27	エコー                                     	41
36	せんたくざい                                  	2
37	せっけい                                    	1
26	エチャント                                   	42
31	ズボン                                     	48
28	コート                                     	45
25	ガード                                     	46
30	スワップ                                    	44
35	フッコク                                    	17
32	ダイエル                                    	50
29	ゴム                                      	43
34	ファイル                                    	49
43	記録機                                     	21
48	国立公園                                    	15
33	フィルター                                   	47
50	国家利益                                    	16
47	音声認識                                    	22
44	記載                                      	20
49	国立大学                                    	18
46	暗視                                      	19
3	ＰＶＤＦ                                    	13
4	ＲＯＭＡＮ-８                                 	9
45	音楽                                      	24
42	高機能                                     	23
15	ｱﾝｶｰ                                    	10
16	ｴﾝｼﾞﾝ                                   	34
5	（Ⅰ）番号列                                  	25
2	９８０Series                               	11
19	ｶｯﾄﾏｼﾝ                                  	31
20	ｶｰﾄﾞ                                    	30
1	３５６CAL                                  	12
6	＜ⅸ＞Pattern                              	14
23	ﾌｫﾙﾀﾞｰ                                  	28
24	ｻｲﾌ                                     	32
17	ｺﾞｰﾙﾄﾞ                                  	29
18	ｺｰﾗ                                     	33
21	ﾂｰｳｨﾝｸﾞ                                 	27
22	ﾏﾝﾎﾞ                                    	26
\.


--
-- Data for Name: tlko; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlko (rnum, c1, ord) FROM stdin;
11	9.Plans                                 	49
20	(아홉)배열                                  	20
21	(넷)배진                                   	1
2	(3)Solution                             	19
35	AutoKitchen                             	48
4	[1]Column                               	6
5	{4}행                                    	7
22	(열)한국                                   	5
39	m60988                                  	37
8	<6>Ｒｏｗｓ                                 	8
9	14.Type                                 	32
6	{A}Series                               	24
3	（５）Ｉｔｅｍｓ                                	3
36	Conditioner                             	31
25	626-Pipe                                	12
26	011-Cabel                               	10
23	１５편집                                    	2
40	MasterCraft                             	33
41	model－１                                 	34
18	９．표시                                    	21
7	＜８＞test                                 	22
44	v6Engine                                	23
1	（２）진열                                   	51
34	Ａｕｄｉｏ-11                                	11
43	Ｖａｎ                                     	58
24	２６５조합                                   	4
33	Ａｕｄｉｏ－２２                                	9
38	Ｃｏｏｋｅｒ                                  	52
59	ⓩ창구                                     	14
16	㉠만남                                     	26
37	Ｃａｋｅ                                    	53
42	Ｍａｃｈｉｎｅｓ                                	59
15	⑤바람                                     	27
68	가수                                      	28
17	㉰자랑                                     	25
14	Ⅴ.조                                     	57
63	가족                                      	15
48	국립대１０                                   	18
65	강산                                      	16
10	Ⅶ.열별                                    	50
47	국립대C                                    	30
12	열 아홉.항목                                 	45
45	국가                                      	17
58	ⓑ나라                                     	13
19	이.설명                                    	56
32	오십구 호 설비                                	55
49	국산형－１                                   	41
46	국립대A                                    	29
27	일 이 삼 조합                                	46
28	일이삼                                     	54
53	상표                                      	42
54	상품                                      	43
51	제Ⅱ항                                     	60
52	제4단                                     	47
13	아홉.Category                             	44
50	제１장                                     	61
31	조　합                                     	62
56	타향살이                                    	68
29	조 합                                     	36
30	조합                                      	35
55	한국사람                                    	67
64	家係                                      	39
57	한민족사랑                                   	66
66	江山                                      	40
67	家族                                      	63
60	韓國                                      	38
61	東浩                                      	64
62	南北東西                                    	65
\.


--
-- Data for Name: tlko_kr; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlko_kr (rnum, c1, ord) FROM stdin;
11	9.Plans                                 	43
20	(아홉)배열                                  	58
21	(넷)배진                                   	1
2	(3)Solution                             	57
35	AutoKitchen                             	42
4	[1]Column                               	6
5	{4}행                                    	7
22	(열)한국                                   	5
39	m60988                                  	21
8	<6>Ｒｏｗｓ                                 	8
9	14.Type                                 	20
6	{A}Series                               	62
15	⑤바람                                     	2
36	Conditioner                             	19
25	626-Pipe                                	12
26	011-Cabel                               	10
59	ⓩ창구                                     	59
40	MasterCraft                             	24
41	model－１                                 	25
14	Ⅴ.조                                     	45
67	家族                                      	46
44	v6Engine                                	61
17	㉰자랑                                     	11
10	Ⅶ.열별                                    	3
63	가족                                      	64
16	㉠만남                                     	60
61	東浩                                      	52
58	ⓑ나라                                     	4
47	국립대C                                    	65
64	家係                                      	47
65	강산                                      	63
62	南北東西                                    	9
19	이.설명                                    	35
60	韓國                                      	53
45	국가                                      	13
66	江山                                      	51
27	일 이 삼 조합                                	37
68	가수                                      	44
49	국산형－１                                   	15
46	국립대A                                    	14
51	제Ⅱ항                                     	39
48	국립대１０                                   	66
53	상표                                      	16
54	상품                                      	17
31	조　합                                     	40
12	열 아홉.항목                                 	68
13	아홉.Category                             	67
50	제１장                                     	50
55	한국사람                                    	55
32	오십구 호 설비                                	18
29	조 합                                     	48
30	조합                                      	49
3	（５）Ｉｔｅｍｓ                                	22
28	일이삼                                     	36
57	한민족사랑                                   	54
18	９．표시                                    	29
23	１５편집                                    	23
52	제4단                                     	38
1	（２）진열                                   	56
34	Ａｕｄｉｏ-11                                	33
7	＜８＞test                                 	26
56	타향살이                                    	41
33	Ａｕｄｉｏ－２２                                	27
38	Ｃｏｏｋｅｒ                                  	30
43	Ｖａｎ                                     	32
24	２６５조합                                   	31
37	Ｃａｋｅ                                    	34
42	Ｍａｃｈｉｎｅｓ                                	28
\.


--
-- Data for Name: tllatin1; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tllatin1 (rnum, c1, ord) FROM stdin;
3	1                                       	0
4	999                                     	0
1	@@@                                     	0
2	0000                                    	0
7	Ændrer                                  	0
8	ärm                                     	0
5	Aalbansk                                	0
6	åben                                    	0
11	cæcum                                   	0
12	caennais                                	0
9	año                                     	0
10	ant                                     	0
15	C.A.F.                                  	0
16	chemin                                  	0
13	cæsium                                  	0
14	ça et là                                	0
19	co-op                                   	0
20	COOP                                    	0
17	coercion                                	0
18	coop                                    	0
23	Cote                                    	0
24	COTE                                    	0
21	CO-OP                                   	0
22	cote                                    	0
27	CÔTE                                    	0
28	coté                                    	0
25	côte                                    	0
26	Côte                                    	0
31	côté                                    	0
32	Côté                                    	0
29	Coté                                    	0
30	COTÉ                                    	0
35	czar                                    	0
36	få                                      	0
33	CÔTÉ                                    	0
34	Coter                                   	0
39	før                                     	0
40	für                                     	0
37	følgende                                	0
38	för                                     	0
43	løs                                     	0
44	lyre                                    	0
41	gros                                    	0
42	groß                                    	0
47	O'hara                                  	0
48	O'Hara                                  	0
45	llave                                   	0
46	Ohara                                   	0
51	öljy                                    	0
52	Øst                                     	0
49	Omaha                                   	0
50	øger                                    	0
55	system                                  	0
56	valley                                  	0
53	süddeutsch                              	0
54	Syd                                     	0
59	                                        	0
58	zone                                    	0
57	wall                                    	0
\.


--
-- Data for Name: tllatin2; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tllatin2 (rnum, c1, ord) FROM stdin;
3	csak                                    	0
4	Się                                     	0
1	Że                                      	0
2	mezők                                   	0
7	uređivanje                              	0
8	Čara                                    	0
5	Babylon                                 	0
6	čara                                    	0
11	końca                                   	0
12	Case                                    	0
9	rozvržením                              	0
10	czar                                    	0
15	mezosfera                               	0
16	chcesz                                  	0
13	Către                                   	0
14	Äquatorial                              	0
19	lynx                                    	0
20	fax                                     	0
17	Chute                                   	0
18	fără                                    	0
23	Ćwiartki                                	0
24	Île                                     	0
21	ČARA                                    	0
22	grafikus                                	0
27	knjika                                  	0
28	bąbelkowych                             	0
25	Internet                                	0
26	knot                                    	0
31	łuk                                     	0
32	Španjolska                              	0
29	konto                                   	0
30	ścieżki                                 	0
35	Švedska                                 	0
36	źródło                                  	0
33	case                                    	0
34	może                                    	0
39	range                                   	0
40	Assistant                               	0
37	možná                                   	0
38	Položka                                 	0
43	ţară                                    	0
44	gyenge                                  	0
41	utolsó                                  	0
42	rozsah                                  	0
47	@@@air                                  	0
48	vice-admiral                            	0
45	čtení                                   	0
46	Źródło                                  	0
51	root                                    	0
52	vice-versa                              	0
49	řad                                     	0
50	środka                                  	0
55	co-op                                   	0
56	šum                                     	0
53	żadane                                  	0
54	0000                                    	0
59	uredi                                   	0
60	Între                                   	0
57	bąbelków                                	0
58	sieve                                   	0
63	verkehrt                                	0
64	vice                                    	0
61	air@@@                                  	0
62	C.B.A.                                  	0
67	że                                      	0
68	stair                                   	0
65	vicennial                               	0
66	uređaj                                  	0
71	šioku                                   	0
72	űrlap                                   	0
69	999                                     	0
70	help                                    	0
75	@@@@@                                   	0
76	zónák                                   	0
73	ŹRÓDŁO                                  	0
74	şir                                     	0
79	Šri Lanka                               	0
80	zöld                                    	0
77	COOP                                    	0
78	ütközött                                	0
83	CO-OP                                   	0
84	taz                                     	0
81	şterse                                  	0
82	Štitec                                  	0
87	vice versa                              	0
88	zabaglione                              	0
85	šiřku                                   	0
86	ţinut                                   	0
91	ŻE                                      	0
92	žádná                                   	0
89	szablon                                 	0
90	zythum                                  	0
95	zone                                    	0
96	betűtípus                               	0
93	zoo                                     	0
94	polja                                   	0
97	művelet                                 	0
98	user                                    	0
\.


--
-- Data for Name: tlth; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlth (rnum, c1, ord) FROM stdin;
39	เ                                       	29
32	ไ                                       	33
49	!                                       	28
62	๛                                       	48
71	๏                                       	67
20	-                                       	11
13	ฯ                                       	54
38	&                                       	71
27	0                                       	32
48	ๆ                                       	49
57	9                                       	5
26	1                                       	47
3	00                                      	55
24	Zulu                                    	30
25	zulu                                    	39
54	ก                                       	21
51	ก็                                      	70
16	กกขนาก                                  	46
37	ก กา                                    	19
74	กก                                      	68
23	ก๊ก                                     	56
4	ก้งง                                    	62
69	ก๊ง                                     	10
34	กกๅ                                     	3
35	กกา                                     	25
52	กังก                                    	45
1	กฏิ                                     	72
66	กง                                      	15
67	ก้ง                                     	12
64	กำ                                      	40
73	ก้ำ                                     	53
22	ก่ง                                     	14
15	กฏุก                                    	38
12	กิ่ง                                    	43
41	กิก                                     	61
70	กฏุก-                                   	34
31	กระจาบ                                  	20
8	กู้หน้า                                 	6
21	กิ๊ก                                    	65
10	กรรมสิทธิ์เครื่องหมายและยี่ห้อการค้าขาย 	2
59	-กระจาม                                 	22
44	เก่น                                    	60
61	-เกงกอย                                 	17
2	กรรมสิทธิ์ผู้แต่งหนังสือ                	74
55	กระจาย                                  	75
72	แก่                                     	13
5	เกนๆ                                    	16
58	-กระจิ๋ง                                	23
47	ก่ำ                                     	50
68	แก้                                     	37
53	โก่                                     	66
30	กระจิด                                  	9
43	เก่                                     	52
56	แก่กล้า                                 	35
45	ใกล้                                    	51
14	กัง                                     	1
11	เก็บ                                    	36
28	ไก                                      	8
65	ฃ                                       	42
42	กั้ง                                    	73
19	แก                                      	59
60	คคน-                                    	24
17	๐๙                                      	31
6	เก                                      	4
75	ขง                                      	69
40	๘                                       	44
9	๑                                       	7
50	เกน                                     	41
7	ไฮฮี                                    	26
36	 ํ                                      	63
29	๒                                       	57
46	ค                                       	64
63	๐๐                                      	27
18	๐                                       	58
33	๙                                       	18
\.


--
-- Data for Name: tlth_th; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlth_th (rnum, c1, ord) FROM stdin;
27	0                                       	39
20	-                                       	17
49	!                                       	20
38	&                                       	5
3	00                                      	52
24	Zulu                                    	71
57	9                                       	64
26	1                                       	49
35	กกา                                     	47
16	กกขนาก                                  	32
25	zulu                                    	34
54	ก                                       	21
15	กฏุก                                    	68
52	กังก                                    	63
37	ก กา                                    	27
74	กก                                      	53
31	กระจาบ                                  	36
64	กำ                                      	25
1	กฏิ                                     	70
34	กกๅ                                     	18
59	-กระจาม                                 	41
12	กิ่ง                                    	62
41	กิก                                     	22
66	กง                                      	12
55	กระจาย                                  	9
8	กู้หน้า                                 	15
21	กิ๊ก                                    	74
70	กฏุก-                                   	56
51	ก็                                      	13
4	ก้งง                                    	48
73	ก้ำ                                     	73
2	กรรมสิทธิ์ผู้แต่งหนังสือ                	1
47	ก่ำ                                     	14
60	คคน-                                    	45
69	ก๊ง                                     	29
10	กรรมสิทธิ์เครื่องหมายและยี่ห้อการค้าขาย 	33
67	ก้ง                                     	3
44	เก่น                                    	24
65	ฃ                                       	55
30	กระจิด                                  	8
23	ก๊ก                                     	31
72	แก่                                     	7
13	ฯ                                       	37
58	-กระจิ๋ง                                	6
75	ขง                                      	54
56	แก่กล้า                                 	26
61	-เกงกอย                                 	51
14	กัง                                     	4
39	เ                                       	65
68	แก้                                     	23
5	เกนๆ                                    	28
42	กั้ง                                    	72
11	เก็บ                                    	60
32	ไ                                       	75
53	โก่                                     	46
22	ก่ง                                     	66
43	เก่                                     	10
28	ไก                                      	69
45	ใกล้                                    	50
46	ค                                       	61
19	แก                                      	58
48	ๆ                                       	44
17	๐๙                                      	19
6	เก                                      	2
7	ไฮฮี                                    	30
36	 ํ                                      	16
9	๑                                       	67
50	เกน                                     	35
71	๏                                       	38
40	๘                                       	40
29	๒                                       	57
18	๐                                       	59
63	๐๐                                      	42
62	๛                                       	43
33	๙                                       	11
\.


--
-- Data for Name: tlth_th_th; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlth_th_th (rnum, c1, ord) FROM stdin;
27	0                                       	39
20	-                                       	17
49	!                                       	20
38	&                                       	5
3	00                                      	52
24	Zulu                                    	71
57	9                                       	64
26	1                                       	49
35	กกา                                     	47
16	กกขนาก                                  	32
25	zulu                                    	34
54	ก                                       	21
15	กฏุก                                    	68
52	กังก                                    	63
37	ก กา                                    	27
74	กก                                      	53
31	กระจาบ                                  	36
64	กำ                                      	25
1	กฏิ                                     	70
34	กกๅ                                     	18
59	-กระจาม                                 	41
12	กิ่ง                                    	62
41	กิก                                     	22
66	กง                                      	12
55	กระจาย                                  	9
8	กู้หน้า                                 	15
21	กิ๊ก                                    	74
70	กฏุก-                                   	56
51	ก็                                      	13
4	ก้งง                                    	48
73	ก้ำ                                     	73
2	กรรมสิทธิ์ผู้แต่งหนังสือ                	1
47	ก่ำ                                     	14
60	คคน-                                    	45
69	ก๊ง                                     	29
10	กรรมสิทธิ์เครื่องหมายและยี่ห้อการค้าขาย 	33
67	ก้ง                                     	3
44	เก่น                                    	24
65	ฃ                                       	55
30	กระจิด                                  	8
23	ก๊ก                                     	31
72	แก่                                     	7
13	ฯ                                       	37
58	-กระจิ๋ง                                	6
75	ขง                                      	54
56	แก่กล้า                                 	26
61	-เกงกอย                                 	51
14	กัง                                     	4
39	เ                                       	65
68	แก้                                     	23
5	เกนๆ                                    	28
42	กั้ง                                    	72
11	เก็บ                                    	60
32	ไ                                       	75
53	โก่                                     	46
22	ก่ง                                     	66
43	เก่                                     	10
28	ไก                                      	69
45	ใกล้                                    	50
46	ค                                       	61
19	แก                                      	58
48	ๆ                                       	44
17	๐๙                                      	19
6	เก                                      	2
7	ไฮฮี                                    	30
36	 ํ                                      	16
9	๑                                       	67
50	เกน                                     	35
71	๏                                       	38
40	๘                                       	40
29	๒                                       	57
18	๐                                       	59
63	๐๐                                      	42
62	๛                                       	43
33	๙                                       	11
\.


--
-- Data for Name: tltr; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tltr (rnum, c1, ord) FROM stdin;
51	cable                                   	8
52	                                        	52
1	                                        	1
50	Cable                                   	40
31	COOP                                    	37
44	@@@@@                                   	45
13	@@@air                                  	44
34	Czech                                   	51
47	çapraz                                  	36
48	0000                                    	43
9	C.B.A.                                  	39
42	çizgiler                                	26
35	çıkmak                                  	30
20	999                                     	42
49	caption                                 	38
38	çoklu                                   	7
43	çizgi                                   	6
24	air@@@                                  	41
41	co-op                                   	4
30	Hata                                    	34
23	icon                                    	47
36	çevir                                   	48
25	CO-OP                                   	35
22	ıptali                                  	12
19	IP                                      	27
40	çizim                                   	31
45	çift                                    	32
18	Ipucu                                   	50
27	Item                                    	15
28	hub                                     	28
37	Çok                                     	29
26	İsteği                                  	25
11	step                                    	24
16	Ölçer                                   	17
33	digit                                   	14
14	option                                  	19
7	update                                  	13
12	özellikler                              	23
21	diğer                                   	33
10	şifreleme                               	22
3	Üyelik                                  	18
8	Şarkı                                   	49
5	Üyeleri                                 	20
6	Uzantısı                                	21
39	vicennial                               	11
4	üyelik                                  	3
29	vice                                    	16
46	verkehrt                                	46
15	vice-versa                              	9
32	vice versa                              	10
17	vice-admiral                            	5
2	Zero                                    	2
\.


--
-- Data for Name: tltr_tr; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tltr_tr (rnum, c1, ord) FROM stdin;
51	cable                                   	8
52	                                        	51
1	                                        	1
50	Cable                                   	38
47	çapraz                                  	39
44	@@@@@                                   	43
13	@@@air                                  	42
42	çizgiler                                	34
43	çizgi                                   	50
48	0000                                    	41
9	C.B.A.                                  	37
38	çoklu                                   	30
31	COOP                                    	6
20	999                                     	44
49	caption                                 	36
34	Czech                                   	29
35	çıkmak                                  	7
24	air@@@                                  	40
45	çift                                    	35
30	Hata                                    	33
23	icon                                    	46
36	çevir                                   	4
37	Çok                                     	31
18	Ipucu                                   	20
19	IP                                      	27
40	çizim                                   	47
41	co-op                                   	25
26	İsteği                                  	49
27	Item                                    	26
28	hub                                     	28
25	CO-OP                                   	52
14	option                                  	24
11	step                                    	48
16	Ölçer                                   	23
21	diğer                                   	22
10	şifreleme                               	19
7	update                                  	17
12	özellikler                              	13
33	digit                                   	32
6	Uzantısı                                	3
3	Üyelik                                  	15
8	Şarkı                                   	18
5	Üyeleri                                 	21
46	verkehrt                                	14
39	vicennial                               	5
4	üyelik                                  	16
29	vice                                    	45
2	Zero                                    	9
15	vice-versa                              	10
32	vice versa                              	12
17	vice-admiral                            	11
22	ıptali                                  	2
\.


--
-- Data for Name: tlzh_cn; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlzh_cn (rnum, c1, ord) FROM stdin;
7	<6>Ｒｏｗｓ                                 	48
24	011-Cabel                               	49
1	{A}Series                               	60
6	(3)Solution                             	3
11	14.Type                                 	1
12	9.Plans                                 	18
37	Conditioner                             	16
2	[1]Column                               	2
23	626-Pipe                                	4
36	AutoKitchen                             	17
33	v6Engine                                	24
34	MasterCraft                             	8
35	m60292                                  	6
8	《8》Test                                 	14
17	ⅸ.种类                                    	21
18	Ⅵ.类别                                    	26
19	㈩.排                                     	19
20	㈨.列                                     	20
9	⒙Ｃａｔｅｇｏｒｙ                               	15
10	⒖项目                                     	25
15	一三八号设备                                  	51
16	五九排列                                    	57
13	二.说明                                    	5
50	事件                                      	7
51	世间                                      	50
40	商品                                      	59
41	商标                                      	55
14	八.计划                                    	56
47	实践                                      	58
48	实在                                      	53
49	实诚                                      	52
42	商标法则                                    	54
43	市场                                      	11
60	左右上下                                    	12
53	成型机-1                                   	9
46	实际                                      	13
55	成绩表Ｃ                                    	27
52	成本                                      	10
45	示范动作                                    	35
54	成绩表A                                    	42
39	日新月异                                    	28
56	成绩表格１０                                  	41
57	第1章                                     	47
38	日期                                      	29
59	第Ⅲ页                                     	43
44	试用                                      	32
5	（５）Items                                	31
58	第４段                                     	33
31	Ａｕｄｉｏ-11                                	37
4	（２）序列                                   	30
21	２６５组合                                   	22
22	１５编号                                    	23
27	ｍｏｄｅｌ－１                                 	45
28	Ｃｏｏｋｅｒ                                  	40
29	Ｃａｋｅ                                    	38
30	Ａｕｄｉｏ－２２                                	36
3	｛４｝行                                    	34
32	Ｖａｎ                                     	46
25	Ｍｉｘｅｒ                                   	44
26	Ｍａｃｈｉｎｅｓ                                	39
\.


--
-- Data for Name: tlzh_tw; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tlzh_tw (rnum, c1, ord) FROM stdin;
15	9.Plans                                 	51
4	[1]Column                               	4
41	(十)排列                                   	1
2	(3)Solution                             	49
23	Conditioner                             	10
8	<6>Ｒｏｗｓ                                 	54
5	{4}行                                    	5
42	(四)矩陣                                   	50
27	MasterCraft                             	11
28	model－１                                 	27
9	011-Cabel                               	7
6	{A}Series                               	6
31	v6Engine                                	26
56	一二三組合                                   	56
13	626-Pipe                                	9
10	14.Type                                 	8
51	世間                                      	55
40	二.說明                                    	13
17	十.Category                              	14
22	AutoKitchen                             	52
55	使用                                      	58
16	卅.項目                                    	15
45	商標                                      	59
26	m60988                                  	19
47	實誠                                      	17
48	實際                                      	38
49	實踐                                      	37
18	Ⅴ.組                                     	18
35	成績表１０                                   	45
52	市場                                      	39
57	左右上下                                    	40
14	Ⅶ.列別                                    	53
43	日期                                      	3
36	成型機－１                                   	36
33	成績表A                                    	46
54	事件                                      	12
39	第4段                                     	29
32	成本                                      	47
53	示範動作                                    	41
58	五十九號設備                                  	57
59	萬次計數                                    	33
44	日新月異                                    	2
37	第１章                                     	31
46	商品                                      	16
3	（５）Items                                	30
60	直線測量儀                                   	42
1	（２）陣列                                   	32
50	實在                                      	60
11	１５編號                                    	21
12	２６５組合                                   	35
21	Ａｕｄｉｏ-11                                	25
34	成績表Ｃ                                    	23
19	９.對號                                    	44
20	Ａｕｄｉｏ－２２                                	20
25	Ｃｏｏｋｅｒ                                  	24
38	第Ⅱ頁                                     	28
7	＜8＞test                                 	22
24	Ｃａｋｅ                                    	34
29	Ｍａｃｈｉｎｅｓ                                	48
30	Ｖａｎ                                     	43
\.


--
-- Data for Name: tmaxcols; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tmaxcols (rnum, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30, c31, c32, c33, c34, c35, c36, c37, c38, c39, c40, c41, c42, c43, c44, c45, c46, c47, c48, c49, c50, c51, c52, c53, c54, c55, c56, c57, c58, c59, c60, c61, c62, c63, c64, c65, c66, c67, c68, c69, c70, c71, c72, c73, c74, c75, c76, c77, c78, c79, c80, c81, c82, c83, c84, c85, c86, c87, c88, c89, c90, c91, c92, c93, c94, c95, c96, c97, c98, c99, c100, c101, c102, c103, c104, c105, c106, c107, c108, c109, c110, c111, c112, c113, c114, c115, c116, c117, c118, c119, c120, c121, c122, c123, c124, c125, c126, c127, c128, c129, c130, c131, c132, c133, c134, c135, c136, c137, c138, c139, c140, c141, c142, c143, c144, c145, c146, c147, c148, c149, c150, c151, c152, c153, c154, c155, c156, c157, c158, c159, c160, c161, c162, c163, c164, c165, c166, c167, c168, c169, c170, c171, c172, c173, c174, c175, c176, c177, c178, c179, c180, c181, c182, c183, c184, c185, c186, c187, c188, c189, c190, c191, c192, c193, c194, c195, c196, c197, c198, c199, c200, c201, c202, c203, c204, c205, c206, c207, c208, c209, c210, c211, c212, c213, c214, c215, c216, c217, c218, c219, c220, c221, c222, c223, c224, c225, c226, c227, c228, c229, c230, c231, c232, c233, c234, c235, c236, c237, c238, c239, c240, c241, c242, c243, c244, c245, c246, c247, c248, c249, c250, c251, c252, c253, c254, c255, c256, c257, c258, c259, c260, c261, c262, c263, c264, c265, c266, c267, c268, c269, c270, c271, c272, c273, c274, c275, c276, c277, c278, c279, c280, c281, c282, c283, c284, c285, c286, c287, c288, c289, c290, c291, c292, c293, c294, c295, c296, c297, c298, c299, c300, c301, c302, c303, c304, c305, c306, c307, c308, c309, c310, c311, c312, c313, c314, c315, c316, c317, c318, c319, c320, c321, c322, c323, c324, c325, c326, c327, c328, c329, c330, c331, c332, c333, c334, c335, c336, c337, c338, c339, c340, c341, c342, c343, c344, c345, c346, c347, c348, c349, c350, c351, c352, c353, c354, c355, c356, c357, c358, c359, c360, c361, c362, c363, c364, c365, c366, c367, c368, c369, c370, c371, c372, c373, c374, c375, c376, c377, c378, c379, c380, c381, c382, c383, c384, c385, c386, c387, c388, c389, c390, c391, c392, c393, c394, c395, c396, c397, c398, c399, c400, c401, c402, c403, c404, c405, c406, c407, c408, c409, c410, c411, c412, c413, c414, c415, c416, c417, c418, c419, c420, c421, c422, c423, c424, c425, c426, c427, c428, c429, c430, c431, c432, c433, c434, c435, c436, c437, c438, c439, c440, c441, c442, c443, c444, c445, c446, c447, c448, c449, c450, c451, c452, c453, c454, c455, c456, c457, c458, c459, c460, c461, c462, c463, c464, c465, c466, c467, c468, c469, c470, c471, c472, c473, c474, c475, c476, c477, c478, c479, c480, c481, c482, c483, c484, c485, c486, c487, c488, c489, c490, c491, c492, c493, c494, c495, c496, c497, c498, c499, c500, c501, c502, c503, c504, c505, c506, c507, c508, c509, c510, c511, c512, c513, c514, c515, c516, c517, c518, c519, c520, c521, c522, c523, c524, c525, c526, c527, c528, c529, c530, c531, c532, c533, c534, c535, c536, c537, c538, c539, c540, c541, c542, c543, c544, c545, c546, c547, c548, c549, c550, c551, c552, c553, c554, c555, c556, c557, c558, c559, c560, c561, c562, c563, c564, c565, c566, c567, c568, c569, c570, c571, c572, c573, c574, c575, c576, c577, c578, c579, c580, c581, c582, c583, c584, c585, c586, c587, c588, c589, c590, c591, c592, c593, c594, c595, c596, c597, c598, c599, c600, c601, c602, c603, c604, c605, c606, c607, c608, c609, c610, c611, c612, c613, c614, c615, c616, c617, c618, c619, c620, c621, c622, c623, c624, c625, c626, c627, c628, c629, c630, c631, c632, c633, c634, c635, c636, c637, c638, c639, c640, c641, c642, c643, c644, c645, c646, c647, c648, c649, c650, c651, c652, c653, c654, c655, c656, c657, c658, c659, c660, c661, c662, c663, c664, c665, c666, c667, c668, c669, c670, c671, c672, c673, c674, c675, c676, c677, c678, c679, c680, c681, c682, c683, c684, c685, c686, c687, c688, c689, c690, c691, c692, c693, c694, c695, c696, c697, c698, c699, c700, c701, c702, c703, c704, c705, c706, c707, c708, c709, c710, c711, c712, c713, c714, c715, c716, c717, c718, c719, c720, c721, c722, c723, c724, c725, c726, c727, c728, c729, c730, c731, c732, c733, c734, c735, c736, c737, c738, c739, c740, c741, c742, c743, c744, c745, c746, c747, c748, c749, c750, c751, c752, c753, c754, c755, c756, c757, c758, c759, c760, c761, c762, c763, c764, c765, c766, c767, c768, c769, c770, c771, c772, c773, c774, c775, c776, c777, c778, c779, c780, c781, c782, c783, c784, c785, c786, c787, c788, c789, c790, c791, c792, c793, c794, c795, c796, c797, c798, c799, c800, c801, c802, c803, c804, c805, c806, c807, c808, c809, c810, c811, c812, c813, c814, c815, c816, c817, c818, c819, c820, c821, c822, c823, c824, c825, c826, c827, c828, c829, c830, c831, c832, c833, c834, c835, c836, c837, c838, c839, c840, c841, c842, c843, c844, c845, c846, c847, c848, c849, c850, c851, c852, c853, c854, c855, c856, c857, c858, c859, c860, c861, c862, c863, c864, c865, c866, c867, c868, c869, c870, c871, c872, c873, c874, c875, c876, c877, c878, c879, c880, c881, c882, c883, c884, c885, c886, c887, c888, c889, c890, c891, c892, c893, c894, c895, c896, c897, c898, c899, c900, c901, c902, c903, c904, c905, c906, c907, c908, c909, c910, c911, c912, c913, c914, c915, c916, c917, c918, c919, c920, c921, c922, c923, c924, c925, c926, c927, c928, c929, c930, c931, c932, c933, c934, c935, c936, c937, c938, c939, c940, c941, c942, c943, c944, c945, c946, c947, c948, c949, c950, c951, c952, c953, c954, c955, c956, c957, c958, c959, c960, c961, c962, c963, c964, c965, c966, c967, c968, c969, c970, c971, c972, c973, c974, c975, c976, c977, c978, c979, c980, c981, c982, c983, c984, c985, c986, c987, c988, c989, c990, c991, c992, c993, c994, c995, c996, c997, c998, c999, c1000, c1001, c1002, c1003, c1004, c1005, c1006, c1007, c1008, c1009, c1010, c1011, c1012, c1013, c1014, c1015, c1016, c1017, c1018, c1019, c1020, c1021, c1022, c1023, c1024, c1025, c1026, c1027, c1028, c1029, c1030, c1031, c1032, c1033, c1034, c1035, c1036, c1037, c1038, c1039, c1040, c1041, c1042, c1043, c1044, c1045, c1046, c1047, c1048, c1049, c1050, c1051, c1052, c1053, c1054, c1055, c1056, c1057, c1058, c1059, c1060, c1061, c1062, c1063, c1064, c1065, c1066, c1067, c1068, c1069, c1070, c1071, c1072, c1073, c1074, c1075, c1076, c1077, c1078, c1079, c1080, c1081, c1082, c1083, c1084, c1085, c1086, c1087, c1088, c1089, c1090, c1091, c1092, c1093, c1094, c1095, c1096, c1097, c1098, c1099, c1100, c1101, c1102, c1103, c1104, c1105, c1106, c1107, c1108, c1109, c1110, c1111, c1112, c1113, c1114, c1115, c1116, c1117, c1118, c1119, c1120, c1121, c1122, c1123, c1124, c1125, c1126, c1127, c1128, c1129, c1130, c1131, c1132, c1133, c1134, c1135, c1136, c1137, c1138, c1139, c1140, c1141, c1142, c1143, c1144, c1145, c1146, c1147, c1148, c1149, c1150, c1151, c1152, c1153, c1154, c1155, c1156, c1157, c1158, c1159, c1160, c1161, c1162, c1163, c1164, c1165, c1166, c1167, c1168, c1169, c1170, c1171, c1172, c1173, c1174, c1175, c1176, c1177, c1178, c1179, c1180, c1181, c1182, c1183, c1184, c1185, c1186, c1187, c1188, c1189, c1190, c1191, c1192, c1193, c1194, c1195, c1196, c1197, c1198, c1199, c1200, c1201, c1202, c1203, c1204, c1205, c1206, c1207, c1208, c1209, c1210, c1211, c1212, c1213, c1214, c1215, c1216, c1217, c1218, c1219, c1220, c1221, c1222, c1223, c1224, c1225, c1226, c1227, c1228, c1229, c1230, c1231, c1232, c1233, c1234, c1235, c1236, c1237, c1238, c1239, c1240, c1241, c1242, c1243, c1244, c1245, c1246, c1247, c1248, c1249, c1250, c1251, c1252, c1253, c1254, c1255, c1256, c1257, c1258, c1259, c1260, c1261, c1262, c1263, c1264, c1265, c1266, c1267, c1268, c1269, c1270, c1271, c1272, c1273, c1274, c1275, c1276, c1277, c1278, c1279, c1280, c1281, c1282, c1283, c1284, c1285, c1286, c1287, c1288, c1289, c1290, c1291, c1292, c1293, c1294, c1295, c1296, c1297, c1298, c1299, c1300, c1301, c1302, c1303, c1304, c1305, c1306, c1307, c1308, c1309, c1310, c1311, c1312, c1313, c1314, c1315, c1316, c1317, c1318, c1319, c1320, c1321, c1322, c1323, c1324, c1325, c1326, c1327, c1328, c1329, c1330, c1331, c1332, c1333, c1334, c1335, c1336, c1337, c1338, c1339, c1340, c1341, c1342, c1343, c1344, c1345, c1346, c1347, c1348, c1349, c1350, c1351, c1352, c1353, c1354, c1355, c1356, c1357, c1358, c1359, c1360, c1361, c1362, c1363, c1364, c1365, c1366, c1367, c1368, c1369, c1370, c1371, c1372, c1373, c1374, c1375, c1376, c1377, c1378, c1379, c1380, c1381, c1382, c1383, c1384, c1385, c1386, c1387, c1388, c1389, c1390, c1391, c1392, c1393, c1394, c1395, c1396, c1397, c1398, c1399, c1400, c1401, c1402, c1403, c1404, c1405, c1406, c1407, c1408, c1409, c1410, c1411, c1412, c1413, c1414, c1415, c1416, c1417, c1418, c1419, c1420, c1421, c1422, c1423, c1424, c1425, c1426, c1427, c1428, c1429, c1430, c1431, c1432, c1433, c1434, c1435, c1436, c1437, c1438, c1439, c1440, c1441, c1442, c1443, c1444, c1445, c1446, c1447, c1448, c1449, c1450, c1451, c1452, c1453, c1454, c1455, c1456, c1457, c1458, c1459, c1460, c1461, c1462, c1463, c1464, c1465, c1466, c1467, c1468, c1469, c1470, c1471, c1472, c1473, c1474, c1475, c1476, c1477, c1478, c1479, c1480, c1481, c1482, c1483, c1484, c1485, c1486, c1487, c1488, c1489, c1490, c1491, c1492, c1493, c1494, c1495, c1496, c1497, c1498, c1499, c1500, c1501, c1502, c1503, c1504, c1505, c1506, c1507, c1508, c1509, c1510, c1511, c1512, c1513, c1514, c1515, c1516, c1517, c1518, c1519, c1520, c1521, c1522, c1523, c1524, c1525, c1526, c1527, c1528, c1529, c1530, c1531, c1532, c1533, c1534, c1535, c1536, c1537, c1538, c1539, c1540, c1541, c1542, c1543, c1544, c1545, c1546, c1547, c1548, c1549, c1550, c1551, c1552, c1553, c1554, c1555, c1556, c1557, c1558, c1559, c1560, c1561, c1562, c1563, c1564, c1565, c1566, c1567, c1568, c1569, c1570, c1571, c1572, c1573, c1574, c1575, c1576, c1577, c1578, c1579, c1580, c1581, c1582, c1583, c1584, c1585, c1586, c1587, c1588, c1589, c1590, c1591, c1592, c1593, c1594, c1595, c1596, c1597, c1598) FROM stdin;
\.


--
-- Data for Name: tnum; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tnum (rnum, cnum) FROM stdin;
3	1.00
0	\N
1	-1.00
2	0.00
5	10.00
4	0.10
\.


--
-- Data for Name: tolap; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tolap (rnum, c1, c2, c3, c4) FROM stdin;
3	BBB	BB	20	5
0	AAA	AA	10	3
1	AAA	AA	15	2
2	AAA	AB	25	1
7	\N	\N	\N	\N
4	CCC	CC	30	2
5	DDD	DD	40	1
6	\N	\N	50	6
\.


--
-- Data for Name: trl; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY trl (rnum, crl) FROM stdin;
3	1
0	\N
1	-1
2	0
5	10
4	-0.1
\.


--
-- Data for Name: tsdchar; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tsdchar (rnum, c1) FROM stdin;
3	-1.0
0	-1
1	0
2	10
7	10.0E+0
4	0.0
5	10.0
6	-1.0E-1
11	12:00:00.00+05:00
8	2000-12-31
9	23:59:30.123
10	2000-12-31 12:15:30.123
15	-1
12	2000-12-31 12:00:00.0+05:00
13	-1
14	10
19	-1
16	10
17	-01-01
18	10-10
23	-1
20	10
21	-1
22	10
27	-1 1
24	10
25	-1.5
26	10.20
31	-1 01:01:01
28	10 10
29	-1 01:01
30	10 20:30
35	-01:01:01
32	10 20:30:40
33	-1:01
34	10:20
37	-01:01
36	10:20:30
38	10:20
\.


--
-- Data for Name: tsdclob; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tsdclob (rnum, c1) FROM stdin;
3	-1.0
0	-1
1	0
2	10
7	10.0E+0
4	0.0
5	10.0
6	-1.0E-1
11	12:00:00.00+05:00
8	2000-12-31
9	23:59:30.123
10	2000-12-31 12:15:30.123
15	-1
12	2000-12-31 12:00:00.0+05:00
13	-1
14	10
19	-1
16	10
17	-01-01
18	10-10
23	-1
20	10
21	-1
22	10
27	-1 1
24	10
25	-1.5
26	10.20
31	-1 01:01:01
28	10 10
29	-1 01:01
30	10 20:30
35	-01:01:01
32	10 20:30:40
33	-1:01
34	10:20
37	-01:01
36	10:20:30
38	10:20
\.


--
-- Data for Name: tset1; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tset1 (rnum, c1, c2) FROM stdin;
3	20	BBB
0	10	AAA
1	10	AAA
2	10	AAA
7	60	\N
4	30	CCC
5	40	DDD
6	50	\N
11	\N	\N
8	\N	AAA
9	\N	AAA
10	\N	\N
\.


--
-- Data for Name: tset2; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tset2 (rnum, c1, c2) FROM stdin;
3	50	EEE
0	10	AAA
1	10	AAA
2	40	DDD
4	60	FFF
\.


--
-- Data for Name: tset3; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tset3 (rnum, c1, c2) FROM stdin;
0	10	AAA
\.


--
-- Data for Name: tsint; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tsint (rnum, csint) FROM stdin;
3	1
0	\N
1	-1
2	0
4	10
\.


--
-- Data for Name: ttm; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY ttm (rnum, ctm) FROM stdin;
3	23:59:30.123
0	\N
1	00:00:00
2	12:00:00
\.


--
-- Data for Name: tts; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tts (rnum, cts) FROM stdin;
3	1996-01-01 23:59:30.123
0	\N
1	1996-01-01 00:00:00
2	1996-01-01 12:00:00
7	2000-12-31 00:00:00
4	2000-01-01 00:00:00
5	2000-01-01 12:00:00
6	2000-01-01 23:59:30.123
9	2000-12-31 12:15:30.123
8	2000-12-31 12:00:00
\.


--
-- Data for Name: tvchar; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tvchar (rnum, cvchar) FROM stdin;
3	BB
0	\N
1
2
5	FF
4	EE
\.


--
-- Data for Name: tversion; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY tversion (rnum, c1, cver, cnnull, ccnull) FROM stdin;
0	1	1.0   	\N	\N
\.


--
-- Name: clobpk; Type: CONSTRAINT; Schema: public; Owner: gpadmin; Tablespace:
--

ALTER TABLE ONLY tclob
    ADD CONSTRAINT clobpk PRIMARY KEY (rnum);


--
-- Name: tcons1pk; Type: CONSTRAINT; Schema: public; Owner: gpadmin; Tablespace:
--

ALTER TABLE ONLY tcons1
    ADD CONSTRAINT tcons1pk PRIMARY KEY (col1);


--
-- Name: tcons2pk; Type: CONSTRAINT; Schema: public; Owner: gpadmin; Tablespace:
--

ALTER TABLE ONLY tcons2
    ADD CONSTRAINT tcons2pk PRIMARY KEY (col2, col3);


--
-- Name: tcons3pk; Type: CONSTRAINT; Schema: public; Owner: gpadmin; Tablespace:
--

ALTER TABLE ONLY tcons3
    ADD CONSTRAINT tcons3pk PRIMARY KEY (col1, col2);


--
-- Name: tcons4pk; Type: CONSTRAINT; Schema: public; Owner: gpadmin; Tablespace:
--

ALTER TABLE ONLY tcons4
    ADD CONSTRAINT tcons4pk PRIMARY KEY (col1);


--
-- Name: tmaxidx0; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx0 ON tmaxcols USING btree (c0);


--
-- Name: tmaxidx1; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx1 ON tmaxcols USING btree (c1);


--
-- Name: tmaxidx10; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx10 ON tmaxcols USING btree (c10);


--
-- Name: tmaxidx11; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx11 ON tmaxcols USING btree (c11);


--
-- Name: tmaxidx12; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx12 ON tmaxcols USING btree (c12);


--
-- Name: tmaxidx13; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx13 ON tmaxcols USING btree (c13);


--
-- Name: tmaxidx14; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx14 ON tmaxcols USING btree (c14);


--
-- Name: tmaxidx15; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx15 ON tmaxcols USING btree (c15);


--
-- Name: tmaxidx2; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx2 ON tmaxcols USING btree (c2);


--
-- Name: tmaxidx3; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx3 ON tmaxcols USING btree (c3);


--
-- Name: tmaxidx4; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx4 ON tmaxcols USING btree (c4);


--
-- Name: tmaxidx5; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx5 ON tmaxcols USING btree (c5);


--
-- Name: tmaxidx6; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx6 ON tmaxcols USING btree (c6);


--
-- Name: tmaxidx7; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx7 ON tmaxcols USING btree (c7);


--
-- Name: tmaxidx8; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx8 ON tmaxcols USING btree (c8);


--
-- Name: tmaxidx9; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxidx9 ON tmaxcols USING btree (c9);


--
-- Name: tmaxxidx; Type: INDEX; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE INDEX tmaxxidx ON tmaxcols USING btree (c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30, c31);


--
-- Name: tcons2fk3; Type: FK CONSTRAINT; Schema: public; Owner: gpadmin
--

ALTER TABLE ONLY tcons3
    ADD CONSTRAINT tcons2fk3 FOREIGN KEY (col4, col3) REFERENCES tcons2(col2, col3);


--
-- Name: tcons3fk1; Type: FK CONSTRAINT; Schema: public; Owner: gpadmin
--

ALTER TABLE ONLY tcons3
    ADD CONSTRAINT tcons3fk1 FOREIGN KEY (col1) REFERENCES tcons1(col1);


--
-- Name: tcons3fk2; Type: FK CONSTRAINT; Schema: public; Owner: gpadmin
--

ALTER TABLE ONLY tcons3
    ADD CONSTRAINT tcons3fk2 FOREIGN KEY (col1, col2) REFERENCES tcons2(col2, col3);


--
-- Name: tcons4fk; Type: FK CONSTRAINT; Schema: public; Owner: gpadmin
--

ALTER TABLE ONLY tcons4
    ADD CONSTRAINT tcons4fk FOREIGN KEY (col2) REFERENCES tcons4(col1);


--
-- Name: public; Type: ACL; Schema: -; Owner: gpadmin
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- Name: finbint(bigint); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION finbint(pbint bigint) FROM PUBLIC;
GRANT ALL ON FUNCTION finbint(pbint bigint) TO PUBLIC;


--
-- Name: finchar(character); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION finchar(pchar character) FROM PUBLIC;
GRANT ALL ON FUNCTION finchar(pchar character) TO PUBLIC;


--
-- Name: finclob(text); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION finclob(pclob text) FROM PUBLIC;
GRANT ALL ON FUNCTION finclob(pclob text) TO PUBLIC;


--
-- Name: findbl(double precision); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION findbl(pdbl double precision) FROM PUBLIC;
GRANT ALL ON FUNCTION findbl(pdbl double precision) TO PUBLIC;


--
-- Name: findec(numeric); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION findec(pdec numeric) FROM PUBLIC;
GRANT ALL ON FUNCTION findec(pdec numeric) TO PUBLIC;


--
-- Name: findt(date); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION findt(pdt date) FROM PUBLIC;
GRANT ALL ON FUNCTION findt(pdt date) TO PUBLIC;


--
-- Name: finflt(double precision); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION finflt(pflt double precision) FROM PUBLIC;
GRANT ALL ON FUNCTION finflt(pflt double precision) TO PUBLIC;


--
-- Name: finint(integer); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION finint(pint integer) FROM PUBLIC;
GRANT ALL ON FUNCTION finint(pint integer) TO PUBLIC;


--
-- Name: finnum(numeric); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION finnum(pnum numeric) FROM PUBLIC;
GRANT ALL ON FUNCTION finnum(pnum numeric) TO PUBLIC;


--
-- Name: finrl(real); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION finrl(prl real) FROM PUBLIC;
GRANT ALL ON FUNCTION finrl(prl real) TO PUBLIC;


--
-- Name: finsint(smallint); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION finsint(psint smallint) FROM PUBLIC;
GRANT ALL ON FUNCTION finsint(psint smallint) TO PUBLIC;


--
-- Name: fintm(time without time zone); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fintm(ptm time without time zone) FROM PUBLIC;
GRANT ALL ON FUNCTION fintm(ptm time without time zone) TO PUBLIC;


--
-- Name: fints(timestamp without time zone); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fints(pts timestamp without time zone) FROM PUBLIC;
GRANT ALL ON FUNCTION fints(pts timestamp without time zone) TO PUBLIC;


--
-- Name: finvchar(character varying); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION finvchar(pvchar character varying) FROM PUBLIC;
GRANT ALL ON FUNCTION finvchar(pvchar character varying) TO PUBLIC;


--
-- Name: tset1; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tset1 FROM PUBLIC;
GRANT SELECT ON TABLE tset1 TO PUBLIC;


--
-- Name: fovtbint(bigint, bigint); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtbint(pbint bigint, p2bint bigint) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtbint(pbint bigint, p2bint bigint) TO PUBLIC;


--
-- Name: fovtchar(character, character); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtchar(pchar character, p2char character) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtchar(pchar character, p2char character) TO PUBLIC;


--
-- Name: fovtclob(text, text); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtclob(pclob text, p2clob text) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtclob(pclob text, p2clob text) TO PUBLIC;


--
-- Name: fovtdbl(double precision, double precision); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtdbl(pdbl double precision, p2dbl double precision) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtdbl(pdbl double precision, p2dbl double precision) TO PUBLIC;


--
-- Name: fovtdec(numeric, numeric); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtdec(pdec numeric, p2dec numeric) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtdec(pdec numeric, p2dec numeric) TO PUBLIC;


--
-- Name: fovtdt(date, date); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtdt(pdt date, p2dt date) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtdt(pdt date, p2dt date) TO PUBLIC;


--
-- Name: fovtflt(double precision, double precision); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtflt(pflt double precision, p2flt double precision) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtflt(pflt double precision, p2flt double precision) TO PUBLIC;


--
-- Name: fovtint(integer, integer); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtint(pint integer, p2int integer) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtint(pint integer, p2int integer) TO PUBLIC;


--
-- Name: fovtnum(numeric, numeric); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtnum(pnum numeric, p2num numeric) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtnum(pnum numeric, p2num numeric) TO PUBLIC;


--
-- Name: fovtrl(real, real); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtrl(prl real, p2rl real) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtrl(prl real, p2rl real) TO PUBLIC;


--
-- Name: fovtsint(smallint, smallint); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtsint(psint smallint, p2sint smallint) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtsint(psint smallint, p2sint smallint) TO PUBLIC;


--
-- Name: fovttm(time without time zone, time without time zone); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovttm(ptm time without time zone, p2tm time without time zone) FROM PUBLIC;
GRANT ALL ON FUNCTION fovttm(ptm time without time zone, p2tm time without time zone) TO PUBLIC;


--
-- Name: fovtts(timestamp without time zone, timestamp without time zone); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtts(pts timestamp without time zone, p2ts timestamp without time zone) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtts(pts timestamp without time zone, p2ts timestamp without time zone) TO PUBLIC;


--
-- Name: fovtvchar(character varying, character varying); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fovtvchar(pvchar character varying, p2vchar character varying) FROM PUBLIC;
GRANT ALL ON FUNCTION fovtvchar(pvchar character varying, p2vchar character varying) TO PUBLIC;


--
-- Name: ftbint(bigint); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftbint(pbint bigint) FROM PUBLIC;
GRANT ALL ON FUNCTION ftbint(pbint bigint) TO PUBLIC;


--
-- Name: ftchar(character); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftchar(pchar character) FROM PUBLIC;
GRANT ALL ON FUNCTION ftchar(pchar character) TO PUBLIC;


--
-- Name: ftclob(text); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftclob(pclob text) FROM PUBLIC;
GRANT ALL ON FUNCTION ftclob(pclob text) TO PUBLIC;


--
-- Name: ftdbl(double precision); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftdbl(pdbl double precision) FROM PUBLIC;
GRANT ALL ON FUNCTION ftdbl(pdbl double precision) TO PUBLIC;


--
-- Name: ftdec(numeric); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftdec(pdec numeric) FROM PUBLIC;
GRANT ALL ON FUNCTION ftdec(pdec numeric) TO PUBLIC;


--
-- Name: ftdt(date); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftdt(pdt date) FROM PUBLIC;
GRANT ALL ON FUNCTION ftdt(pdt date) TO PUBLIC;


--
-- Name: ftflt(double precision); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftflt(pflt double precision) FROM PUBLIC;
GRANT ALL ON FUNCTION ftflt(pflt double precision) TO PUBLIC;


--
-- Name: ftint(integer); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftint(pint integer) FROM PUBLIC;
GRANT ALL ON FUNCTION ftint(pint integer) TO PUBLIC;


--
-- Name: ftnum(numeric); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftnum(pnum numeric) FROM PUBLIC;
GRANT ALL ON FUNCTION ftnum(pnum numeric) TO PUBLIC;


--
-- Name: ftrl(real); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftrl(prl real) FROM PUBLIC;
GRANT ALL ON FUNCTION ftrl(prl real) TO PUBLIC;


--
-- Name: ftsint(smallint); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftsint(psint smallint) FROM PUBLIC;
GRANT ALL ON FUNCTION ftsint(psint smallint) TO PUBLIC;


--
-- Name: fttm(time without time zone); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION fttm(ptm time without time zone) FROM PUBLIC;
GRANT ALL ON FUNCTION fttm(ptm time without time zone) TO PUBLIC;


--
-- Name: ftts(timestamp without time zone); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftts(pts timestamp without time zone) FROM PUBLIC;
GRANT ALL ON FUNCTION ftts(pts timestamp without time zone) TO PUBLIC;


--
-- Name: ftvchar(character varying); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION ftvchar(pvchar character varying) FROM PUBLIC;
GRANT ALL ON FUNCTION ftvchar(pvchar character varying) TO PUBLIC;


--
-- Name: pinbint(bigint); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pinbint(pbint bigint) FROM PUBLIC;
GRANT ALL ON FUNCTION pinbint(pbint bigint) TO PUBLIC;


--
-- Name: pinchar(character); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pinchar(pchar character) FROM PUBLIC;
GRANT ALL ON FUNCTION pinchar(pchar character) TO PUBLIC;


--
-- Name: pinclob(text); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pinclob(pclob text) FROM PUBLIC;
GRANT ALL ON FUNCTION pinclob(pclob text) TO PUBLIC;


--
-- Name: pindbl(double precision); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pindbl(pdbl double precision) FROM PUBLIC;
GRANT ALL ON FUNCTION pindbl(pdbl double precision) TO PUBLIC;


--
-- Name: pindec(numeric); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pindec(pdec numeric) FROM PUBLIC;
GRANT ALL ON FUNCTION pindec(pdec numeric) TO PUBLIC;


--
-- Name: pindt(date); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pindt(pdt date) FROM PUBLIC;
GRANT ALL ON FUNCTION pindt(pdt date) TO PUBLIC;


--
-- Name: pinflt(double precision); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pinflt(pflt double precision) FROM PUBLIC;
GRANT ALL ON FUNCTION pinflt(pflt double precision) TO PUBLIC;


--
-- Name: pinint(integer); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pinint(pint integer) FROM PUBLIC;
GRANT ALL ON FUNCTION pinint(pint integer) TO PUBLIC;


--
-- Name: pinnum(numeric); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pinnum(pnum numeric) FROM PUBLIC;
GRANT ALL ON FUNCTION pinnum(pnum numeric) TO PUBLIC;


--
-- Name: pinrl(real); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pinrl(prl real) FROM PUBLIC;
GRANT ALL ON FUNCTION pinrl(prl real) TO PUBLIC;


--
-- Name: pinsint(smallint); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pinsint(psint smallint) FROM PUBLIC;
GRANT ALL ON FUNCTION pinsint(psint smallint) TO PUBLIC;


--
-- Name: pintm(time without time zone); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pintm(ptm time without time zone) FROM PUBLIC;
GRANT ALL ON FUNCTION pintm(ptm time without time zone) TO PUBLIC;


--
-- Name: pints(timestamp without time zone); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pints(pts timestamp without time zone) FROM PUBLIC;
GRANT ALL ON FUNCTION pints(pts timestamp without time zone) TO PUBLIC;


--
-- Name: pinvchar(character varying); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pinvchar(pvchar character varying) FROM PUBLIC;
GRANT ALL ON FUNCTION pinvchar(pvchar character varying) TO PUBLIC;


--
-- Name: pnoparams(); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pnoparams() FROM PUBLIC;
GRANT ALL ON FUNCTION pnoparams() TO PUBLIC;


--
-- Name: povin(smallint); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION povin(ppovin smallint) FROM PUBLIC;
GRANT ALL ON FUNCTION povin(ppovin smallint) TO PUBLIC;


--
-- Name: povin(smallint, smallint); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION povin(ppovin smallint, p2povin smallint) FROM PUBLIC;
GRANT ALL ON FUNCTION povin(ppovin smallint, p2povin smallint) TO PUBLIC;


--
-- Name: pres(); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pres() FROM PUBLIC;
GRANT ALL ON FUNCTION pres() TO PUBLIC;


--
-- Name: pthrow(); Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON FUNCTION pthrow() FROM PUBLIC;
GRANT ALL ON FUNCTION pthrow() TO PUBLIC;


--
-- Name: tbint; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tbint FROM PUBLIC;
GRANT SELECT ON TABLE tbint TO PUBLIC;


--
-- Name: tchar; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tchar FROM PUBLIC;
GRANT SELECT ON TABLE tchar TO PUBLIC;


--
-- Name: tclob; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tclob FROM PUBLIC;
GRANT SELECT ON TABLE tclob TO PUBLIC;


--
-- Name: tcons1; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tcons1 FROM PUBLIC;
GRANT SELECT ON TABLE tcons1 TO PUBLIC;


--
-- Name: tcons2; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tcons2 FROM PUBLIC;
GRANT SELECT ON TABLE tcons2 TO PUBLIC;


--
-- Name: tcons3; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tcons3 FROM PUBLIC;
GRANT SELECT ON TABLE tcons3 TO PUBLIC;


--
-- Name: tcons4; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tcons4 FROM PUBLIC;
GRANT SELECT ON TABLE tcons4 TO PUBLIC;


--
-- Name: tdbl; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tdbl FROM PUBLIC;
GRANT SELECT ON TABLE tdbl TO PUBLIC;


--
-- Name: tdec; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tdec FROM PUBLIC;
GRANT SELECT ON TABLE tdec TO PUBLIC;


--
-- Name: tdt; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tdt FROM PUBLIC;
GRANT SELECT ON TABLE tdt TO PUBLIC;


--
-- Name: tflt; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tflt FROM PUBLIC;
GRANT SELECT ON TABLE tflt TO PUBLIC;


--
-- Name: tint; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tint FROM PUBLIC;
GRANT SELECT ON TABLE tint TO PUBLIC;


--
-- Name: tiud; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tiud FROM PUBLIC;
GRANT SELECT ON TABLE tiud TO PUBLIC;


--
-- Name: tjoin1; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tjoin1 FROM PUBLIC;
GRANT SELECT ON TABLE tjoin1 TO PUBLIC;


--
-- Name: tjoin2; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tjoin2 FROM PUBLIC;
GRANT SELECT ON TABLE tjoin2 TO PUBLIC;


--
-- Name: tjoin3; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tjoin3 FROM PUBLIC;
GRANT SELECT ON TABLE tjoin3 TO PUBLIC;


--
-- Name: tjoin4; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tjoin4 FROM PUBLIC;
GRANT SELECT ON TABLE tjoin4 TO PUBLIC;


--
-- Name: tlel; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlel FROM PUBLIC;
GRANT SELECT ON TABLE tlel TO PUBLIC;


--
-- Name: tlel_cy_preeuro; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlel_cy_preeuro FROM PUBLIC;
GRANT SELECT ON TABLE tlel_cy_preeuro TO PUBLIC;


--
-- Name: tlel_gr; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlel_gr FROM PUBLIC;
GRANT SELECT ON TABLE tlel_gr TO PUBLIC;


--
-- Name: tlel_gr_preeuro; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlel_gr_preeuro FROM PUBLIC;
GRANT SELECT ON TABLE tlel_gr_preeuro TO PUBLIC;


--
-- Name: tliw; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tliw FROM PUBLIC;
GRANT SELECT ON TABLE tliw TO PUBLIC;


--
-- Name: tliw_il; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tliw_il FROM PUBLIC;
GRANT SELECT ON TABLE tliw_il TO PUBLIC;


--
-- Name: tlja; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlja FROM PUBLIC;
GRANT SELECT ON TABLE tlja TO PUBLIC;


--
-- Name: tlja_jp; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlja_jp FROM PUBLIC;
GRANT SELECT ON TABLE tlja_jp TO PUBLIC;


--
-- Name: tlko; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlko FROM PUBLIC;
GRANT SELECT ON TABLE tlko TO PUBLIC;


--
-- Name: tlko_kr; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlko_kr FROM PUBLIC;
GRANT SELECT ON TABLE tlko_kr TO PUBLIC;


--
-- Name: tllatin1; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tllatin1 FROM PUBLIC;
GRANT SELECT ON TABLE tllatin1 TO PUBLIC;


--
-- Name: tllatin2; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tllatin2 FROM PUBLIC;
GRANT SELECT ON TABLE tllatin2 TO PUBLIC;


--
-- Name: tlth; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlth FROM PUBLIC;
GRANT SELECT ON TABLE tlth TO PUBLIC;


--
-- Name: tlth_th; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlth_th FROM PUBLIC;
GRANT SELECT ON TABLE tlth_th TO PUBLIC;


--
-- Name: tlth_th_th; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlth_th_th FROM PUBLIC;
GRANT SELECT ON TABLE tlth_th_th TO PUBLIC;


--
-- Name: tltr; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tltr FROM PUBLIC;
GRANT SELECT ON TABLE tltr TO PUBLIC;


--
-- Name: tltr_tr; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tltr_tr FROM PUBLIC;
GRANT SELECT ON TABLE tltr_tr TO PUBLIC;


--
-- Name: tlzh_cn; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlzh_cn FROM PUBLIC;
GRANT SELECT ON TABLE tlzh_cn TO PUBLIC;


--
-- Name: tlzh_tw; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tlzh_tw FROM PUBLIC;
GRANT SELECT ON TABLE tlzh_tw TO PUBLIC;


--
-- Name: tmaxcols; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tmaxcols FROM PUBLIC;
GRANT SELECT ON TABLE tmaxcols TO PUBLIC;


--
-- Name: tnum; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tnum FROM PUBLIC;
GRANT SELECT ON TABLE tnum TO PUBLIC;


--
-- Name: tolap; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tolap FROM PUBLIC;
GRANT SELECT ON TABLE tolap TO PUBLIC;


--
-- Name: trl; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE trl FROM PUBLIC;
GRANT SELECT ON TABLE trl TO PUBLIC;


--
-- Name: tsdchar; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tsdchar FROM PUBLIC;
GRANT SELECT ON TABLE tsdchar TO PUBLIC;


--
-- Name: tsdclob; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tsdclob FROM PUBLIC;
GRANT SELECT ON TABLE tsdclob TO PUBLIC;


--
-- Name: tset2; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tset2 FROM PUBLIC;
GRANT SELECT ON TABLE tset2 TO PUBLIC;


--
-- Name: tset3; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tset3 FROM PUBLIC;
GRANT SELECT ON TABLE tset3 TO PUBLIC;


--
-- Name: tsint; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tsint FROM PUBLIC;
GRANT SELECT ON TABLE tsint TO PUBLIC;


--
-- Name: ttm; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE ttm FROM PUBLIC;
GRANT SELECT ON TABLE ttm TO PUBLIC;


--
-- Name: tts; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tts FROM PUBLIC;
GRANT SELECT ON TABLE tts TO PUBLIC;


--
-- Name: tvchar; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tvchar FROM PUBLIC;
GRANT SELECT ON TABLE tvchar TO PUBLIC;


--
-- Name: tversion; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE tversion FROM PUBLIC;
GRANT SELECT ON TABLE tversion TO PUBLIC;


--
-- Name: vbint; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vbint FROM PUBLIC;
GRANT SELECT ON TABLE vbint TO PUBLIC;


--
-- Name: vchar; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vchar FROM PUBLIC;
GRANT SELECT ON TABLE vchar TO PUBLIC;


--
-- Name: vclob; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vclob FROM PUBLIC;
GRANT SELECT ON TABLE vclob TO PUBLIC;


--
-- Name: vdbl; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vdbl FROM PUBLIC;
GRANT SELECT ON TABLE vdbl TO PUBLIC;


--
-- Name: vdec; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vdec FROM PUBLIC;
GRANT SELECT ON TABLE vdec TO PUBLIC;


--
-- Name: vdt; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vdt FROM PUBLIC;
GRANT SELECT ON TABLE vdt TO PUBLIC;


--
-- Name: vflt; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vflt FROM PUBLIC;
GRANT SELECT ON TABLE vflt TO PUBLIC;


--
-- Name: vint; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vint FROM PUBLIC;
GRANT SELECT ON TABLE vint TO PUBLIC;


--
-- Name: vnum; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vnum FROM PUBLIC;
GRANT SELECT ON TABLE vnum TO PUBLIC;


--
-- Name: vrl; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vrl FROM PUBLIC;
GRANT SELECT ON TABLE vrl TO PUBLIC;


--
-- Name: vsint; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vsint FROM PUBLIC;
GRANT SELECT ON TABLE vsint TO PUBLIC;


--
-- Name: vtm; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vtm FROM PUBLIC;
GRANT SELECT ON TABLE vtm TO PUBLIC;


--
-- Name: vts; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vts FROM PUBLIC;
GRANT SELECT ON TABLE vts TO PUBLIC;


--
-- Name: vvchar; Type: ACL; Schema: public; Owner: gpadmin
--

REVOKE ALL ON TABLE vvchar FROM PUBLIC;
GRANT SELECT ON TABLE vvchar TO PUBLIC;


--
-- Greenplum Database database dump complete
--

-- end_ignore





--------------------
--- END OF SETUP ---
--------------------




--- AbsCoreApproximateNumeric_p1
select 'AbsCoreApproximateNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 1.000000000000000e+000 f2 from tversion union
select 4 f1, 1.000000000000000e-001 f2 from tversion union
select 5 f1, 1.000000000000000e+001 f2 from tversion union
select rnum, abs( vflt.cflt ) from vflt
) Q
group by
f1,f2
) Q ) P;
--- AbsCoreExactNumeric_p4
select 'AbsCoreExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 1.000000000000000e+000 f2 from tversion union
select 4 f1, 1.000000000000000e-001 f2 from tversion union
select 5 f1, 1.000000000000000e+001 f2 from tversion union
select rnum, abs( tnum.cnum ) from tnum
) Q
group by
f1,f2
) Q ) P;
--- CaseComparisonsInteger_p2
select 'CaseComparisonsInteger_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'gt' f2 from tversion union
select tint.rnum,case  when tint.cint =  1  then '=' when tint.cint >  9  then 'gt' when tint.cint < 0 then 'lt' when tint.cint in  (0,11)  then 'in' when tint.cint between 6 and 8  then 'between'  else 'other' end from tint
) Q
group by
f1,f2
) Q ) P;
--- StringPredicateLike_p2
select 'StringPredicateLike_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 like '%B%'
) Q
group by
f1,f2
) Q ) P;
--- StringPredicateLike_p3
select 'StringPredicateLike_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 like '_B'
) Q
group by
f1,f2
) Q ) P;
--- StringPredicateLike_p4
select 'StringPredicateLike_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 like 'BB%'
) Q
group by
f1,f2
) Q ) P;
--- StringPredicateNotBetween_p1
select 'StringPredicateNotBetween_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum,tjoin2.c1, tjoin2.c2 from tjoin2 where not tjoin2.c2 between 'AA' and 'CC'
) Q
group by
f1,f2,f3
) Q ) P;
--- StringPredicateNotIn_p1
select 'StringPredicateNotIn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum,tjoin2.c1, tjoin2.c2 from tjoin2 where not tjoin2.c2 in ('ZZ','BB','EE')
) Q
group by
f1,f2,f3
) Q ) P;
--- StringPredicateNotLike_p1
select 'StringPredicateNotLike_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum,tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 not like 'B%'
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryColumnAlias_p1
select 'SubqueryColumnAlias_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'AAA' f3 from tversion union
select 1 f1, 10 f2, 'AAA' f3 from tversion union
select 2 f1, 10 f2, 'AAA' f3 from tversion union
select 5 f1, 40 f2, 'DDD' f3 from tversion union
select 6 f1, 50 f2, null f3 from tversion union
select 7 f1, 60 f2, null f3 from tversion union
select rnum, c1, c2 from tset1 as t1 where exists ( select * from tset2 where c1=t1.c1 )
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryCorrelated_p1
select 'SubqueryCorrelated_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 15 f2 from tversion union
select tjoin1.c1, tjoin1.c2 from tjoin1 where tjoin1.c1 = any (select tjoin2.c1 from tjoin2 where tjoin2.c1 = tjoin1.c1)
) Q
group by
f1,f2
) Q ) P;
--- SubqueryDerivedAliasOrderBy_p1
select 'SubqueryDerivedAliasOrderBy_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 15 f3 from tversion union
select 1 f1, 20 f2, 25 f3 from tversion union
select 2 f1, null f2, 50 f3 from tversion union
select tx.rnum, tx.c1, tx.c2 from (select rnum, c1, c2 from tjoin1 order by c1) tx
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryDerivedAssignNames_p1
select 'SubqueryDerivedAssignNames_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 15 f3 from tversion union
select 1 f1, 20 f2, 25 f3 from tversion union
select 2 f1, null f2, 50 f3 from tversion union
select tx.rnumx, tx.c1x, tx.c2x from (select rnum, c1, c2 from tjoin1) tx (rnumx, c1x, c2x)
) Q
group by
f1,f2,f3
) Q ) P;
--- CaseComparisonsInteger_p3
select 'CaseComparisonsInteger_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'gt' f2 from tversion union
select vsint.rnum,case  when vsint.csint =  1  then '=' when vsint.csint >  9  then 'gt' when vsint.csint < 0 then 'lt' when vsint.csint in  (0,11)  then 'in' when vsint.csint between 6 and 8  then 'between'  else 'other' end from vsint
) Q
group by
f1,f2
) Q ) P;
--- SubqueryDerivedMany_p1
select 'SubqueryDerivedMany_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 0 f1, 10 f2, 15 f3, 'BB' f4 from tversion union
select 0 f1, 10 f2, 15 f3, 'DD' f4 from tversion union
select 0 f1, 10 f2, 15 f3, 'EE' f4 from tversion union
select 0 f1, 10 f2, 15 f3, 'FF' f4 from tversion union
select 1 f1, 20 f2, 25 f3, 'BB' f4 from tversion union
select 1 f1, 20 f2, 25 f3, 'DD' f4 from tversion union
select 1 f1, 20 f2, 25 f3, 'EE' f4 from tversion union
select 1 f1, 20 f2, 25 f3, 'FF' f4 from tversion union
select 2 f1, null f2, 50 f3, 'BB' f4 from tversion union
select 2 f1, null f2, 50 f3, 'DD' f4 from tversion union
select 2 f1, null f2, 50 f3, 'EE' f4 from tversion union
select 2 f1, null f2, 50 f3, 'FF' f4 from tversion union
select tx.rnum, tx.c1, tx.c2, ty.c2 from (select tjoin1.rnum, tjoin1.c1, tjoin1.c2 from tjoin1) tx, (select tjoin2.c2 from tjoin2) ty
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- SubqueryDerived_p1
select 'SubqueryDerived_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 15 f3 from tversion union
select 1 f1, 20 f2, 25 f3 from tversion union
select 2 f1, null f2, 50 f3 from tversion union
select tx.rnum, tx.c1, tx.c2 from (select rnum, c1, c2 from tjoin1) tx
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryInAggregate_p1
select 'SubqueryInAggregate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 1 f3 from tversion union
select 1 f1, 20 f2, 1 f3 from tversion union
select 2 f1, null f2, 1 f3 from tversion union
select rnum, c1, sum( (select 1 from tversion) ) from tjoin1 group by rnum, c1
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryInCase_p1
--- test expected to fail until function supported in GPDB
--- GPDB Limitation ERROR:  Greenplum Database does not yet support that query.  DETAIL:  The query contains a multi-row subquery.
select 'SubqueryInCase_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'no' f3 from tversion union
select 1 f1, 20 f2, 'no' f3 from tversion union
select 2 f1, null f2, 'no' f3 from tversion union
select tjoin1.rnum, tjoin1.c1, case when 10 in ( select 1 from tversion ) then 'yes' else 'no' end from tjoin1
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryInGroupBy_p1
select 'SubqueryInGroupBy_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 3 f1, 4 f2 from tversion union
select (select count(*) from tjoin1),count(*) from tjoin2 group by ( select count(*) from tjoin1 )
) Q
group by
f1,f2
) Q ) P;
--- SubqueryInHaving_p1
select 'SubqueryInHaving_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 3 f1 from tversion union
select count(*) from tjoin1 having count(*) > ( select count(*) from tversion )
) Q
group by
f1
) Q ) P;
--- SubqueryPredicateExists_p1
select 'SubqueryPredicateExists_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum, c1, c2 from tjoin2 where exists ( select c1 from tjoin1)
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryPredicateIn_p1
select 'SubqueryPredicateIn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum, c1, c2 from tjoin2 where 50 in ( select c2 from tjoin1 where c2=50)
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryPredicateNotExists_p1
select 'SubqueryPredicateNotExists_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum, c1, c2 from tjoin2 where not exists ( select c1 from tjoin1 where c2=500)
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryPredicateNotIn_p1
--- test expected to fail until function supported in GPDB
--- GPDB Limitation ERROR:  Greenplum Database does not yet support that query.  DETAIL:  The query contains a multi-row subquery.
select 'SubqueryPredicateNotIn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum, c1, c2 from tjoin2 where 50 not in ( select c2 from tjoin1 where c2=25)
) Q
group by
f1,f2,f3
) Q ) P;
--- CaseComparisonsInteger_p4
select 'CaseComparisonsInteger_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'gt' f2 from tversion union
select tsint.rnum,case  when tsint.csint =  1  then '=' when tsint.csint >  9  then 'gt' when tsint.csint < 0 then 'lt' when tsint.csint in  (0,11)  then 'in' when tsint.csint between 6 and 8  then 'between'  else 'other' end from tsint
) Q
group by
f1,f2
) Q ) P;
--- SubqueryQuantifiedPredicateAnyFromC1_p1
select 'SubqueryQuantifiedPredicateAnyFromC1_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum, c1, c2 from tjoin2 where 20 > any ( select c1 from tjoin1)
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryQuantifiedPredicateAnyFromC2_p1
select 'SubqueryQuantifiedPredicateAnyFromC2_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum, c1, c2 from tjoin2 where 20 > any ( select c2 from tjoin1)
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryQuantifiedPredicateEmpty_p1
--- test expected to fail until GPDB support this function
--- GPDB Limitation ERROR:  Greenplum Database does not yet support this query.  DETAIL:  The query contains a multi-row subquery.
select 'SubqueryQuantifiedPredicateEmpty_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum, c1, c2 from tjoin2 where 20 > all ( select c1 from tjoin1 where c1 = 100)
) Q
group by
f1,f2,f3
) Q ) P;
--- SubqueryQuantifiedPredicateLarge_p1
--- test expected to fail until GPDB supports this function
--- GPDB Limitation ERROR:  Greenplum Database does not yet support that query.  DETAIL:  The query contains a multi-row subquery.
select 'SubqueryQuantifiedPredicateLarge_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum, c1, c2 from tjoin2 where 75 > all ( select c2 from tjoin1)
) Q
group by
f1,f2,f3
) Q ) P;
--- SubstringBoundary_p1
select 'SubstringBoundary_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, '' f2 from tversion union
select 4 f1, '' f2 from tversion union
select 5 f1, '' f2 from tversion union
select rnum, substring( vchar.cchar from 0 for 0)  from vchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringBoundary_p2
select 'SubstringBoundary_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, '' f2 from tversion union
select 4 f1, '' f2 from tversion union
select 5 f1, '' f2 from tversion union
select rnum, substring( vchar.cchar from 100 for 1)  from vchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringBoundary_p3
select 'SubstringBoundary_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, '' f2 from tversion union
select 4 f1, '' f2 from tversion union
select 5 f1, '' f2 from tversion union
select rnum, substring( tchar.cchar from 0 for 0)  from tchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringBoundary_p4
select 'SubstringBoundary_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, '' f2 from tversion union
select 4 f1, '' f2 from tversion union
select 5 f1, '' f2 from tversion union
select rnum, substring( tchar.cchar from 100 for 1)  from tchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringBoundary_p5
select 'SubstringBoundary_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, '' f2 from tversion union
select 4 f1, '' f2 from tversion union
select 5 f1, '' f2 from tversion union
select rnum, substring( vvchar.cvchar from 0 for 0)  from vvchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringBoundary_p6
select 'SubstringBoundary_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, '' f2 from tversion union
select 4 f1, '' f2 from tversion union
select 5 f1, '' f2 from tversion union
select rnum, substring( vvchar.cvchar from 100 for 1)  from vvchar
) Q
group by
f1,f2
) Q ) P;
--- CaseComparisonsInteger_p5
select 'CaseComparisonsInteger_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'gt' f2 from tversion union
select vbint.rnum,case  when vbint.cbint =  1  then '=' when vbint.cbint >  9  then 'gt' when vbint.cbint < 0 then 'lt' when vbint.cbint in  (0,11)  then 'in' when vbint.cbint between 6 and 8  then 'between'  else 'other' end from vbint
) Q
group by
f1,f2
) Q ) P;
--- SubstringBoundary_p7
select 'SubstringBoundary_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, '' f2 from tversion union
select 4 f1, '' f2 from tversion union
select 5 f1, '' f2 from tversion union
select rnum, substring( tvchar.cvchar from 0 for 0)  from tvchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringBoundary_p8
select 'SubstringBoundary_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, '' f2 from tversion union
select 4 f1, '' f2 from tversion union
select 5 f1, '' f2 from tversion union
select rnum, substring( tvchar.cvchar from 100 for 1)  from tvchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreBlob_p1
select 'SubstringCoreBlob_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select rnum, substring( tclob.cclob from 2 for 1)  from tclob
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreFixedLength_p1
select 'SubstringCoreFixedLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, ' ' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select rnum, substring( vchar.cchar from 2 for 1)  from vchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreFixedLength_p2
select 'SubstringCoreFixedLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, ' ' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select rnum, substring( tchar.cchar from 2 for 1)  from tchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreNegativeStart_p1
select 'SubstringCoreNegativeStart_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '12' f1 from tversion union
select substring( '1234567890' from -2 for 5)  from tversion
) Q
group by
f1
) Q ) P;
--- SubstringCoreNestedFixedLength_p1
select 'SubstringCoreNestedFixedLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, ' ' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select vchar.rnum, substring( substring(vchar.cchar from 1 for 2) from 1 for 1) from vchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreNestedFixedLength_p2
select 'SubstringCoreNestedFixedLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, ' ' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select tchar.rnum, substring( substring(tchar.cchar from 1 for 2) from 1 for 1) from tchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreNestedVariableLength_p1
select 'SubstringCoreNestedVariableLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select vvchar.rnum, substring( substring(vvchar.cvchar from 1 for 2) from 1 for 1) from vvchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreNestedVariableLength_p2
select 'SubstringCoreNestedVariableLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select tvchar.rnum, substring( substring(tvchar.cvchar from 1 for 2) from 1 for 1) from tvchar
) Q
group by
f1,f2
) Q ) P;
--- CaseComparisonsInteger_p6
select 'CaseComparisonsInteger_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'gt' f2 from tversion union
select tbint.rnum,case  when tbint.cbint =  1  then '=' when tbint.cbint >  9  then 'gt' when tbint.cbint < 0 then 'lt' when tbint.cbint in  (0,11)  then 'in' when tbint.cbint between 6 and 8  then 'between'  else 'other' end from tbint
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreNoLength_p1
select 'SubstringCoreNoLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select rnum, substring( vchar.cchar from 2)  from vchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreNoLength_p2
select 'SubstringCoreNoLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select rnum, substring( tchar.cchar from 2)  from tchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreNoLength_p3
select 'SubstringCoreNoLength_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select rnum, substring( vvchar.cvchar from 2)  from vvchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreNoLength_p4
select 'SubstringCoreNoLength_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select rnum, substring( tvchar.cvchar from 2)  from tvchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreNullParameters_p1
select 'SubstringCoreNullParameters_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select substring( '1234567890' from cnnull for 1)  from tversion
) Q
group by
f1
) Q ) P;
--- SubstringCoreNullParameters_p2
select 'SubstringCoreNullParameters_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select substring( '1234567890' from 1 for cnnull)  from tversion
) Q
group by
f1
) Q ) P;
--- SubstringCoreVariableLength_p1
select 'SubstringCoreVariableLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select rnum, substring( vvchar.cvchar from 2 for 1)  from vvchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreVariableLength_p2
select 'SubstringCoreVariableLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'B' f2 from tversion union
select 4 f1, 'E' f2 from tversion union
select 5 f1, 'F' f2 from tversion union
select rnum, substring( tvchar.cvchar from 2 for 1)  from tvchar
) Q
group by
f1,f2
) Q ) P;
--- SubstringValueExpr_p1
select 'SubstringValueExpr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where substring(tjoin2.c2 from 1 for 1) = 'B'
) Q
group by
f1,f2
) Q ) P;
--- SumDistinct_p1
select 'SumDistinct_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 210 f1 from tversion union
select sum( distinct c1 ) from tset1
) Q
group by
f1
) Q ) P;
--- CaseNestedApproximateNumeric_p1
select 'CaseNestedApproximateNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested else' f2 from tversion union
select 5 f1, 'nested inner' f2 from tversion union
select vflt.rnum,case  when vflt.cflt > -1  then case  when vflt.cflt > 1 then  'nested inner' else 'nested else' end else 'else'  end from vflt
) Q
group by
f1,f2
) Q ) P;
--- TableConstructor_p1
select 'TableConstructor_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select t1.c1, t1.c2 from (values (10,'BB')) t1(c1,c2)
) Q
group by
f1,f2
) Q ) P;
--- TrimBothCoreFixedLength_p1
select 'TrimBothCoreFixedLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '                                ' f2 from tversion union
select 2 f1, '                                ' f2 from tversion union
select 3 f1, '                              ' f2 from tversion union
select 4 f1, 'EE                              ' f2 from tversion union
select 5 f1, 'FF                              ' f2 from tversion union
select vchar.rnum, trim(both 'B' from vchar.cchar )  from vchar
) Q
group by
f1,f2
) Q ) P;
--- TrimBothCoreFixedLength_p2
select 'TrimBothCoreFixedLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '                                ' f2 from tversion union
select 2 f1, '                                ' f2 from tversion union
select 3 f1, '                              ' f2 from tversion union
select 4 f1, 'EE                              ' f2 from tversion union
select 5 f1, 'FF                              ' f2 from tversion union
select tchar.rnum, trim(both 'B' from tchar.cchar )  from tchar
) Q
group by
f1,f2
) Q ) P;
--- TrimBothCoreNullParameters_p1
select 'TrimBothCoreNullParameters_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select trim(both ccnull from '++1234567890++' )  from tversion
) Q
group by
f1
) Q ) P;
--- TrimBothCoreNullParameters_p2
select 'TrimBothCoreNullParameters_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select trim(both '+' from ccnull )  from tversion
) Q
group by
f1
) Q ) P;
--- TrimBothCoreVariableLength_p1
select 'TrimBothCoreVariableLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, '' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select vvchar.rnum, trim(both 'B' from vvchar.cvchar )  from vvchar
) Q
group by
f1,f2
) Q ) P;
--- TrimBothCoreVariableLength_p2
select 'TrimBothCoreVariableLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, '' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select tvchar.rnum, trim(both 'B' from tvchar.cvchar )  from tvchar
) Q
group by
f1,f2
) Q ) P;
--- TrimBothException_p1
select 'TrimBothException_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'error' f1 from tversion union
select trim(both '++' from '++1234567890++' )  from tversion
) Q
group by
f1
) Q ) P;
--- TrimBothSpacesCore_p1
select 'TrimBothSpacesCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '1234567890' f1 from tversion union
select trim(both from '  1234567890  ' )  from tversion
) Q
group by
f1
) Q ) P;
--- TrimCore_p1
select 'TrimCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, trim( vchar.cchar )  from vchar
) Q
group by
f1,f2
) Q ) P;
--- CaseNestedApproximateNumeric_p2
select 'CaseNestedApproximateNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested else' f2 from tversion union
select 5 f1, 'nested inner' f2 from tversion union
select tflt.rnum,case  when tflt.cflt > -1  then case  when tflt.cflt > 1 then  'nested inner' else 'nested else' end else 'else'  end from tflt
) Q
group by
f1,f2
) Q ) P;
--- TrimCore_p2
select 'TrimCore_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, trim( tchar.cchar )  from tchar
) Q
group by
f1,f2
) Q ) P;
--- TrimCore_p3
select 'TrimCore_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, trim( vvchar.cvchar )  from vvchar
) Q
group by
f1,f2
) Q ) P;
--- TrimCore_p4
select 'TrimCore_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, trim( tvchar.cvchar )  from tvchar
) Q
group by
f1,f2
) Q ) P;
--- TrimLeadingSpacesCore_p1
select 'TrimLeadingSpacesCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '1234567890  ' f1 from tversion union
select trim(leading from '  1234567890  ' )  from tversion
) Q
group by
f1
) Q ) P;
--- TrimTrailingSpacesCore_p1
select 'TrimTrailingSpacesCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '  1234567890' f1 from tversion union
select trim(trailing from '  1234567890  ' )  from tversion
) Q
group by
f1
) Q ) P;
--- UnionAll_p1
select 'UnionAll_p1' test_name_part, case when d = 1 then 1 else 0 end pass_ind from (
select count(distinct d) d from (
select t,f1,f2,c,count(*) d from (
select T, f1,f2,count(*) c  from (
select 'X' T, 10 f1, 'AAA' f2 from tversion union all
select 'X', 10 f1, 'AAA' f2 from tversion union all
select 'X', 10 f1, 'AAA' f2 from tversion union all
select 'X', 10 f1, 'AAA' f2 from tversion union all
select 'X', 10 f1, 'AAA' f2 from tversion union all
select 'X', 20 f1, 'BBB' f2 from tversion union all
select 'X', 30 f1, 'CCC' f2 from tversion union all
select 'X', 40 f1, 'DDD' f2 from tversion union all
select 'X', 40 f1, 'DDD' f2 from tversion union all
select 'X', 50 f1, 'EEE' f2 from tversion union all
select 'X', 50 f1, null f2 from tversion union all
select 'X', 60 f1, 'FFF' f2 from tversion union all
select  'X', 60 f1, null f2 from tversion union all
select 'X',  null f1, 'AAA' f2 from tversion union all
select 'X',  null f1, 'AAA' f2 from tversion union all
select  'X', null f1, null f2 from tversion union all
select 'X',  null f1, null f2 from tversion union all
select 'A' , c1, c2 from tset1 union all select 'A', c1, c2 from tset2
) Q
group by
T, f1,f2
) P
group by t,f1,f2,c
) O
) N;
--- Union_p1
select 'Union_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'AAA' f2 from tversion union
select 20 f1, 'BBB' f2 from tversion union
select 30 f1, 'CCC' f2 from tversion union
select 40 f1, 'DDD' f2 from tversion union
select 50 f1, 'EEE' f2 from tversion union
select 50 f1, null f2 from tversion union
select 60 f1, 'FFF' f2 from tversion union
select 60 f1, null f2 from tversion union
select null f1, 'AAA' f2 from tversion union
select null f1, null f2 from tversion union
select c1, c2 from tset1 union select c1, c2 from tset2
) Q
group by
f1,f2
) Q ) P;
--- UpperCoreFixedLength_p1
select 'UpperCoreFixedLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, null f2, 'BB' f3 from tversion union
select 1 f1, '                                ' f2, 'BB' f3 from tversion union
select 2 f1, '                                ' f2, 'BB' f3 from tversion union
select 3 f1, 'BB                              ' f2, 'BB' f3 from tversion union
select 4 f1, 'EE                              ' f2, 'BB' f3 from tversion union
select 5 f1, 'FF                              ' f2, 'BB' f3 from tversion union
select vchar.rnum, upper( vchar.cchar ),upper('bb') from vchar
) Q
group by
f1,f2,f3
) Q ) P;
--- UpperCoreFixedLength_p2
select 'UpperCoreFixedLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, null f2, 'BB' f3 from tversion union
select 1 f1, '                                ' f2, 'BB' f3 from tversion union
select 2 f1, '                                ' f2, 'BB' f3 from tversion union
select 3 f1, 'BB                              ' f2, 'BB' f3 from tversion union
select 4 f1, 'EE                              ' f2, 'BB' f3 from tversion union
select 5 f1, 'FF                              ' f2, 'BB' f3 from tversion union
select tchar.rnum, upper( tchar.cchar ),upper('bb') from tchar
) Q
group by
f1,f2,f3
) Q ) P;
--- UpperCoreSpecial_p1
select 'UpperCoreSpecial_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'ß' f1 from tversion union
select upper( 'ß' ) from tversion
) Q
group by
f1
) Q ) P;
--- CaseNestedApproximateNumeric_p3
select 'CaseNestedApproximateNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested else' f2 from tversion union
select 5 f1, 'nested inner' f2 from tversion union
select vdbl.rnum,case  when vdbl.cdbl > -1  then case  when vdbl.cdbl > 1 then  'nested inner' else 'nested else' end else 'else'  end from vdbl
) Q
group by
f1,f2
) Q ) P;
--- UpperCoreVariableLength_p1
select 'UpperCoreVariableLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, null f2, 'BB' f3 from tversion union
select 1 f1, '' f2, 'BB' f3 from tversion union
select 2 f1, ' ' f2, 'BB' f3 from tversion union
select 3 f1, 'BB' f2, 'BB' f3 from tversion union
select 4 f1, 'EE' f2, 'BB' f3 from tversion union
select 5 f1, 'FF' f2, 'BB' f3 from tversion union
select vvchar.rnum, upper( vvchar.cvchar ), upper('bb') from vvchar
) Q
group by
f1,f2,f3
) Q ) P;
--- UpperCoreVariableLength_p2
select 'UpperCoreVariableLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, null f2, 'BB' f3 from tversion union
select 1 f1, '' f2, 'BB' f3 from tversion union
select 2 f1, ' ' f2, 'BB' f3 from tversion union
select 3 f1, 'BB' f2, 'BB' f3 from tversion union
select 4 f1, 'EE' f2, 'BB' f3 from tversion union
select 5 f1, 'FF' f2, 'BB' f3 from tversion union
select tvchar.rnum, upper( tvchar.cvchar ), upper('bb') from tvchar
) Q
group by
f1,f2,f3
) Q ) P;
--- VarApproxNumeric_p1
select 'VarApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
--- VarApproxNumeric_p2
select 'VarApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
--- VarApproxNumeric_p3
select 'VarApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
--- VarApproxNumeric_p4
select 'VarApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
--- VarApproxNumeric_p5
select 'VarApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
--- VarApproxNumeric_p6
select 'VarApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
--- VarExactNumeric_p1
select 'VarExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select variance( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
--- VarExactNumeric_p2
select 'VarExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select variance( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
--- CaseNestedApproximateNumeric_p4
select 'CaseNestedApproximateNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested else' f2 from tversion union
select 5 f1, 'nested inner' f2 from tversion union
select tdbl.rnum,case  when tdbl.cdbl > -1  then case  when tdbl.cdbl > 1 then  'nested inner' else 'nested else' end else 'else'  end from tdbl
) Q
group by
f1,f2
) Q ) P;
--- VarExactNumeric_p3
select 'VarExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select variance( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
--- VarExactNumeric_p4
select 'VarExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select variance( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
--- VarInt_p1
select 'VarInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
--- VarInt_p2
select 'VarInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
--- VarInt_p3
select 'VarInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
--- VarInt_p4
select 'VarInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
--- VarInt_p5
select 'VarInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
--- VarInt_p6
select 'VarInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
--- VarSampApproxNumeric_p1
select 'VarSampApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
--- VarSampApproxNumeric_p2
select 'VarSampApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
--- CaseNestedApproximateNumeric_p5
select 'CaseNestedApproximateNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested else' f2 from tversion union
select 5 f1, 'nested inner' f2 from tversion union
select vrl.rnum,case  when vrl.crl > -1  then case  when vrl.crl > 1 then  'nested inner' else 'nested else' end else 'else'  end from vrl
) Q
group by
f1,f2
) Q ) P;
--- VarSampApproxNumeric_p3
select 'VarSampApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
--- VarSampApproxNumeric_p4
select 'VarSampApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
--- VarSampApproxNumeric_p5
select 'VarSampApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
--- VarSampApproxNumeric_p6
select 'VarSampApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
--- VarSampExactNumeric_p1
select 'VarSampExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select var_samp( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
--- VarSampExactNumeric_p2
select 'VarSampExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select var_samp( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
--- VarSampExactNumeric_p3
select 'VarSampExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select var_samp( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
--- VarSampExactNumeric_p4
select 'VarSampExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select var_samp( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
--- VarSampInt_p1
select 'VarSampInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
--- VarSampInt_p2
select 'VarSampInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
--- AbsCoreInteger_p1
select 'AbsCoreInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select vint.rnum, abs( vint.cint ) from vint
) Q
group by
f1,f2
) Q ) P;
--- CaseNestedApproximateNumeric_p6
select 'CaseNestedApproximateNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested else' f2 from tversion union
select 5 f1, 'nested inner' f2 from tversion union
select trl.rnum,case  when trl.crl > -1  then case  when trl.crl > 1 then  'nested inner' else 'nested else' end else 'else'  end from trl
) Q
group by
f1,f2
) Q ) P;
--- VarSampInt_p3
select 'VarSampInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
--- VarSampInt_p4
select 'VarSampInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
--- VarSampInt_p5
select 'VarSampInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
--- VarSampInt_p6
select 'VarSampInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
--- WithClauseDerivedTable_p1
--- test exepected to fail until GPDB supports function
--- GPDB Limitation syntax not supported WITH clause
select 'WithClauseDerivedTable_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'AAA' f3 from tversion union
select 1 f1, 10 f2, 'AAA' f3 from tversion union
select 2 f1, 10 f2, 'AAA' f3 from tversion union
select 3 f1, 20 f2, 'BBB' f3 from tversion union
select 4 f1, 30 f2, 'CCC' f3 from tversion union
select 5 f1, 40 f2, 'DDD' f3 from tversion union
select 6 f1, 50 f2, null f3 from tversion union
select 7 f1, 60 f2, null f3 from tversion union
select 8 f1, null f2, 'AAA' f3 from tversion union
select 9 f1, null f2, 'AAA' f3 from tversion union
select 10 f1, null f2, null f3 from tversion union
select 11 f1, null f2, null f3 from tversion union
select * from ( with t_cte as ( select tset1.rnum, tset1.c1, tset1.c2 from tset1 ) select * from t_cte ) tx
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreLikePredicate_gp_p1
select 'JoinCoreLikePredicate_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select tjoin1.rnum, tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2 from tjoin1 inner join tjoin2 on ( tjoin1.c1 = tjoin2.c1 and tjoin2.c2 like 'A%' ))
) Q
group by
f1
) Q ) P;
--- JoinCoreNestedInner_gp_p1
select 'JoinCoreNestedInner_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select tjoin1.c1, tjoin2.c2, tjoin3.c2 as c2j3 from (( tjoin1 inner join tjoin2 on tjoin1.c1=tjoin2.c1 ) inner join tjoin3 on tjoin3.c1=tjoin1.c1) inner join tjoin4 on tjoin4.c1=tjoin3.c1)
) Q
group by
f1
) Q ) P;
--- NumericComparisonEqual_gp_p1
select 'NumericComparisonEqual_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select 1 from tversion where 7 = 210.3 union select 1 from tversion where 7 = cnnull)
) Q
group by
f1
) Q ) P;
--- SelectWhere_gp_p1
select 'SelectWhere_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 0, 1 f1 from tversion union
select rnum, c1  from tversion where rnum=0
) Q
group by
f1
) Q ) P;
--- SimpleSelect_gp_p1
select 'SimpleSelect_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select c1  from tversion
) Q
group by
f1
) Q ) P;
--- CaseNestedExactNumeric_p1
select 'CaseNestedExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested else' f2 from tversion union
select 5 f1, 'nested inner' f2 from tversion union
select vdec.rnum,case  when vdec.cdec > -1  then case  when vdec.cdec > 1 then  'nested inner' else 'nested else' end else 'else'  end from vdec
) Q
group by
f1,f2
) Q ) P;
--- StringPredicateLikeEscape_gp_p1
select 'StringPredicateLikeEscape_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where ('%%' || tjoin2.c2) like '!%%B' escape '!'
) Q
group by
f1,f2
) Q ) P;
--- StringPredicateLikeUnderscore_gp_p1
select 'StringPredicateLikeUnderscore_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where ('__' || tjoin2.c2) like '!__BB' escape '!'
) Q
group by
f1,f2
) Q ) P;
--- SubqueryNotIn_gp_p1
select 'SubqueryNotIn_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select tjoin1.rnum, tjoin1.c1 from tjoin1 where tjoin1.c1 not in (select tjoin2.c1 from tjoin2))
) Q
group by
f1
) Q ) P;
--- SubqueryOnCondition_gp_p1
select 'SubqueryOnCondition_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select tjoin1.c1, tjoin2.c2 from tjoin1 inner join tjoin2 on ( tjoin1.c1 = tjoin2.c1 and tjoin1.c2 < ( select count(*) from tversion)))
) Q
group by
f1
) Q ) P;
--- SubqueryPredicateWhereIn_gp_p1
select 'SubqueryPredicateWhereIn_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select rnum, c1, c2 from tjoin2 where 50 in ( select c1 from tjoin1))
) Q
group by
f1
) Q ) P;
--- SubqueryQuantifiedPredicateNull_gp_p1
--- test expected to fail until GPDB support function
--- GPDB Limitation ERROR:  Greenplum Database does not yet support this query.  DETAIL:  The query contains a multi-row subquery.
select 'SubqueryQuantifiedPredicateNull_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select rnum, c1, c2 from tjoin2 where 20 > all ( select c1 from tjoin1))
) Q
group by
f1
) Q ) P;
--- SubqueryQuantifiedPredicateSmall_gp_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation ERROR:  Greenplum Database does not yet support this query.  DETAIL:  The query contains a multi-row subquery.
select 'SubqueryQuantifiedPredicateSmall_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select rnum, c1, c2 from tjoin2 where 20 > all ( select c2 from tjoin1))
) Q
group by
f1
) Q ) P;
--- SubstringCoreLiteral_gp_p1
select 'SubstringCoreLiteral_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, NULL f2 from tversion union
select 0 f1, 'B' f2 from tversion union
select rnum, substring( '' from 2 for 1)  from tversion
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreLiteral_gp_p2
select 'SubstringCoreLiteral_gp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, NULL f2 from tversion union
select 0 f1, 'B' f2 from tversion union
select rnum, substring( ' ' from 2 for 1)  from tversion
) Q
group by
f1,f2
) Q ) P;
--- SubstringCoreLiteral_gp_p3
select 'SubstringCoreLiteral_gp_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, NULL f2 from tversion union
select 0 f1, 'B' f2 from tversion union
select rnum, substring( 'BB' from 2 for 1)  from tversion
) Q
group by
f1,f2
) Q ) P;
--- CaseNestedExactNumeric_p2
select 'CaseNestedExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested else' f2 from tversion union
select 5 f1, 'nested inner' f2 from tversion union
select tdec.rnum,case  when tdec.cdec > -1  then case  when tdec.cdec > 1 then  'nested inner' else 'nested else' end else 'else'  end from tdec
) Q
group by
f1,f2
) Q ) P;
--- TrimCoreLiteral_gp_p1
select 'TrimCoreLiteral_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select  length(trim( 'BB ' ))  from tversion
) Q
group by
f1
) Q ) P;
--- TrimCoreLiteral_gp_p2
select 'TrimCoreLiteral_gp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select  length(trim( 'EE  ' ))  from tversion
) Q
group by
f1
) Q ) P;
--- TrimCoreLiteral_gp_p3
select 'TrimCoreLiteral_gp_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select  length(trim( '  FF      ' ))  from tversion
) Q
group by
f1
) Q ) P;
--- CaseNestedExactNumeric_p3
select 'CaseNestedExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested else' f2 from tversion union
select 5 f1, 'nested inner' f2 from tversion union
select vnum.rnum,case  when vnum.cnum > -1  then case  when vnum.cnum > 1 then  'nested inner' else 'nested else' end else 'else'  end from vnum
) Q
group by
f1,f2
) Q ) P;
--- CaseNestedExactNumeric_p4
select 'CaseNestedExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested else' f2 from tversion union
select 5 f1, 'nested inner' f2 from tversion union
select tnum.rnum,case  when tnum.cnum > -1  then case  when tnum.cnum > 1 then  'nested inner' else 'nested else' end else 'else'  end from tnum
) Q
group by
f1,f2
) Q ) P;
--- CaseNestedInteger_p1
select 'CaseNestedInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested inner' f2 from tversion union
select vint.rnum,case  when vint.cint > -1  then case  when vint.cint > 1 then  'nested inner' else 'nested else' end else 'else'  end from vint
) Q
group by
f1,f2
) Q ) P;
--- CaseNestedInteger_p2
select 'CaseNestedInteger_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested inner' f2 from tversion union
select tint.rnum,case  when tint.cint > -1  then case  when tint.cint > 1 then  'nested inner' else 'nested else' end else 'else'  end from tint
) Q
group by
f1,f2
) Q ) P;
--- CaseNestedInteger_p3
select 'CaseNestedInteger_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested inner' f2 from tversion union
select vsint.rnum,case  when vsint.csint > -1  then case  when vsint.csint > 1 then  'nested inner' else 'nested else' end else 'else'  end from vsint
) Q
group by
f1,f2
) Q ) P;
--- CaseNestedInteger_p4
select 'CaseNestedInteger_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested inner' f2 from tversion union
select tsint.rnum,case  when tsint.csint > -1  then case  when tsint.csint > 1 then  'nested inner' else 'nested else' end else 'else'  end from tsint
) Q
group by
f1,f2
) Q ) P;
--- CaseNestedInteger_p5
select 'CaseNestedInteger_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested inner' f2 from tversion union
select vbint.rnum,case  when vbint.cbint > -1  then case  when vbint.cbint > 1 then  'nested inner' else 'nested else' end else 'else'  end from vbint
) Q
group by
f1,f2
) Q ) P;
--- AbsCoreInteger_p2
select 'AbsCoreInteger_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select tint.rnum, abs( tint.cint ) from tint
) Q
group by
f1,f2
) Q ) P;
--- CaseNestedInteger_p6
select 'CaseNestedInteger_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'nested else' f2 from tversion union
select 3 f1, 'nested else' f2 from tversion union
select 4 f1, 'nested inner' f2 from tversion union
select tbint.rnum,case  when tbint.cbint > -1  then case  when tbint.cbint > 1 then  'nested inner' else 'nested else' end else 'else'  end from tbint
) Q
group by
f1,f2
) Q ) P;
--- CaseSubqueryApproxmiateNumeric_p1
select 'CaseSubqueryApproxmiateNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'true' f2 from tversion union
select vflt.rnum,case  when vflt.cflt= (select max( vflt.cflt) from vflt)    then 'true' else 'else' 	end from vflt
) Q
group by
f1,f2
) Q ) P;
--- CaseSubqueryApproxmiateNumeric_p2
select 'CaseSubqueryApproxmiateNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'true' f2 from tversion union
select tflt.rnum,case  when tflt.cflt= (select max( tflt.cflt) from tflt)    then 'true' else 'else' 	end from tflt
) Q
group by
f1,f2
) Q ) P;
--- CaseSubqueryApproxmiateNumeric_p3
select 'CaseSubqueryApproxmiateNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'true' f2 from tversion union
select vdbl.rnum,case  when vdbl.cdbl= (select max( vdbl.cdbl) from vdbl)    then 'true' else 'else' 	end from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CaseSubqueryApproxmiateNumeric_p4
select 'CaseSubqueryApproxmiateNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'true' f2 from tversion union
select tdbl.rnum,case  when tdbl.cdbl= (select max( tdbl.cdbl) from tdbl)    then 'true' else 'else' 	end from tdbl
) Q
group by
f1,f2
) Q ) P;
--- CaseSubqueryApproxmiateNumeric_p5
select 'CaseSubqueryApproxmiateNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'true' f2 from tversion union
select vrl.rnum,case  when vrl.crl= (select max( vrl.crl) from vrl)    then 'true' else 'else' 	end from vrl
) Q
group by
f1,f2
) Q ) P;
--- CaseSubqueryApproxmiateNumeric_p6
select 'CaseSubqueryApproxmiateNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'true' f2 from tversion union
select trl.rnum,case  when trl.crl= (select max( trl.crl) from trl)    then 'true' else 'else' 	end from trl
) Q
group by
f1,f2
) Q ) P;
--- CaseSubQueryExactNumeric_p1
select 'CaseSubQueryExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'true' f2 from tversion union
select vdec.rnum,case  when vdec.cdec= (select max( vdec.cdec) from vdec)    then 'true' else 'else' 	end from vdec
) Q
group by
f1,f2
) Q ) P;
--- CaseSubQueryExactNumeric_p2
select 'CaseSubQueryExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'true' f2 from tversion union
select tdec.rnum,case  when tdec.cdec= (select max( tdec.cdec) from tdec)    then 'true' else 'else' 	end from tdec
) Q
group by
f1,f2
) Q ) P;
--- CaseSubQueryExactNumeric_p3
select 'CaseSubQueryExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'true' f2 from tversion union
select vnum.rnum,case  when vnum.cnum= (select max( vnum.cnum) from vnum)    then 'true' else 'else' 	end from vnum
) Q
group by
f1,f2
) Q ) P;
--- AbsCoreInteger_p3
select 'AbsCoreInteger_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select vsint.rnum, abs( vsint.csint ) from vsint
) Q
group by
f1,f2
) Q ) P;
--- CaseSubQueryExactNumeric_p4
select 'CaseSubQueryExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'true' f2 from tversion union
select tnum.rnum,case  when tnum.cnum= (select max( tnum.cnum) from tnum)    then 'true' else 'else' 	end from tnum
) Q
group by
f1,f2
) Q ) P;
--- CaseSubQueryInteger_p1
select 'CaseSubQueryInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'true' f2 from tversion union
select vint.rnum,case  when vint.cint= (select max( vint.cint) from vint)    then 'true' else 'else' 	end from vint
) Q
group by
f1,f2
) Q ) P;
--- CaseSubQueryInteger_p2
select 'CaseSubQueryInteger_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'true' f2 from tversion union
select tint.rnum,case  when tint.cint= (select max( tint.cint) from tint)    then 'true' else 'else' 	end from tint
) Q
group by
f1,f2
) Q ) P;
--- CaseSubQueryInteger_p3
select 'CaseSubQueryInteger_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'true' f2 from tversion union
select vsint.rnum,case  when vsint.csint= (select max( vsint.csint) from vsint)    then 'true' else 'else' 	end from vsint
) Q
group by
f1,f2
) Q ) P;
--- CaseSubQueryInteger_p4
select 'CaseSubQueryInteger_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'true' f2 from tversion union
select tsint.rnum,case  when tsint.csint= (select max( tsint.csint) from tsint)    then 'true' else 'else' 	end from tsint
) Q
group by
f1,f2
) Q ) P;
--- CaseSubQueryInteger_p5
select 'CaseSubQueryInteger_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'true' f2 from tversion union
select vbint.rnum,case  when vbint.cbint= (select max( vbint.cbint) from vbint)    then 'true' else 'else' 	end from vbint
) Q
group by
f1,f2
) Q ) P;
--- CaseSubQueryInteger_p6
select 'CaseSubQueryInteger_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'true' f2 from tversion union
select tbint.rnum,case  when tbint.cbint= (select max( tbint.cbint) from tbint)    then 'true' else 'else' 	end from tbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToBigint_p1
select 'CastBigintToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vbint.cbint as bigint) from vbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToBigint_p2
select 'CastBigintToBigint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tbint.cbint as bigint) from tbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToChar_p1
select 'CastBigintToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0 ' f2 from tversion union
select 3 f1, '1 ' f2 from tversion union
select 4 f1, '10' f2 from tversion union
select rnum, cast(vbint.cbint as char(2)) from vbint
) Q
group by
f1,f2
) Q ) P;
--- AbsCoreInteger_p4
select 'AbsCoreInteger_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select tsint.rnum, abs( tsint.csint ) from tsint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToChar_p2
select 'CastBigintToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0 ' f2 from tversion union
select 3 f1, '1 ' f2 from tversion union
select 4 f1, '10' f2 from tversion union
select rnum, cast(tbint.cbint as char(2)) from tbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToDecimal_p1
select 'CastBigintToDecimal_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vbint.cbint as decimal(10,0)) from vbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToDecimal_p2
select 'CastBigintToDecimal_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tbint.cbint as decimal(10,0)) from tbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToDouble_p1
select 'CastBigintToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vbint.cbint as double precision) from vbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToDouble_p2
select 'CastBigintToDouble_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tbint.cbint as double precision) from tbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToFloat_p1
select 'CastBigintToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vbint.cbint as float) from vbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToFloat_p2
select 'CastBigintToFloat_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tbint.cbint as float) from tbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToInteger_p1
select 'CastBigintToInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vbint.cbint as integer) from vbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToInteger_p2
select 'CastBigintToInteger_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tbint.cbint as integer) from tbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToSmallint_p1
select 'CastBigintToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vbint.cbint as smallint) from vbint
) Q
group by
f1,f2
) Q ) P;
--- AbsCoreInteger_p5
select 'AbsCoreInteger_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select vbint.rnum, abs( vbint.cbint ) from vbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToSmallint_p2
select 'CastBigintToSmallint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tbint.cbint as smallint) from tbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToVarchar_p1
select 'CastBigintToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0' f2 from tversion union
select 3 f1, '1' f2 from tversion union
select 4 f1, '10' f2 from tversion union
select rnum, cast(vbint.cbint as varchar(2)) from vbint
) Q
group by
f1,f2
) Q ) P;
--- CastBigintToVarchar_p2
select 'CastBigintToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0' f2 from tversion union
select 3 f1, '1' f2 from tversion union
select 4 f1, '10' f2 from tversion union
select rnum, cast(tbint.cbint as varchar(2)) from tbint
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToBigint_p1
select 'CastCharsToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, -1 f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 10 f2 from tversion union
select rnum, cast(c1 as bigint) from tsdchar where rnum in (0,1,2)
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToChar_p1
select 'CastCharsToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '  ' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, cast(vchar.cchar as char(34)) from vchar
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToChar_p2
select 'CastCharsToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '  ' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, cast(tchar.cchar as char(34)) from tchar
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToChar_p3
select 'CastCharsToChar_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '  ' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, cast(vvchar.cvchar as char(34)) from vvchar
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToChar_p4
select 'CastCharsToChar_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, '  ' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, cast(tvchar.cvchar as char(34)) from tvchar
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToDate_p1
select 'CastCharsToDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
--- CastCharsToDouble_p1
select 'CastCharsToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.3 f1 from tversion union
select cast('10.3' as double precision) from tversion
) Q
group by
f1
) Q ) P;
--- AbsCoreInteger_p6
select 'AbsCoreInteger_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select tbint.rnum, abs( tbint.cbint ) from tbint
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToFloat_p1
select 'CastCharsToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.3 f1 from tversion union
select cast('10.3' as float) from tversion
) Q
group by
f1
) Q ) P;
--- CastCharsToInteger_p1
select 'CastCharsToInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select cast('10' as integer) from tversion
) Q
group by
f1
) Q ) P;
--- CastCharsToSmallint_p1
select 'CastCharsToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, -1 f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 10 f2 from tversion union
select rnum, cast(c1 as smallint) from tsdchar where rnum in (0,1,2)
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToTimestamp_p1
select 'CastCharsToTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01 12:30:40' as timestamp) f1 from tversion union
select cast('2000-01-01 12:30:40' as timestamp) from tversion
) Q
group by
f1
) Q ) P;
--- CastCharsToVarchar_p1
select 'CastCharsToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '     ' f2 from tversion union
select 2 f1, '     ' f2 from tversion union
select 3 f1, 'BB   ' f2 from tversion union
select 4 f1, 'EE   ' f2 from tversion union
select 5 f1, 'FF   ' f2 from tversion union
select vchar.rnum, cast(vchar.cchar as varchar(32)) from vchar
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToVarchar_p2
select 'CastCharsToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '     ' f2 from tversion union
select 2 f1, '     ' f2 from tversion union
select 3 f1, 'BB   ' f2 from tversion union
select 4 f1, 'EE   ' f2 from tversion union
select 5 f1, 'FF   ' f2 from tversion union
select tchar.rnum, cast(tchar.cchar as varchar(32)) from tchar
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToVarchar_p3
select 'CastCharsToVarchar_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '     ' f2 from tversion union
select 2 f1, '     ' f2 from tversion union
select 3 f1, 'BB   ' f2 from tversion union
select 4 f1, 'EE   ' f2 from tversion union
select 5 f1, 'FF   ' f2 from tversion union
select vvchar.rnum, cast(vvchar.cvchar as varchar(32)) from vvchar
) Q
group by
f1,f2
) Q ) P;
--- CastCharsToVarchar_p4
select 'CastCharsToVarchar_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '     ' f2 from tversion union
select 2 f1, '     ' f2 from tversion union
select 3 f1, 'BB   ' f2 from tversion union
select 4 f1, 'EE   ' f2 from tversion union
select 5 f1, 'FF   ' f2 from tversion union
select tvchar.rnum, cast(tvchar.cvchar as varchar(32)) from tvchar
) Q
group by
f1,f2
) Q ) P;
--- CastDateToChar_p1
select 'CastDateToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1996-01-01' f2 from tversion union
select 2 f1, '2000-01-01' f2 from tversion union
select 3 f1, '2000-12-31' f2 from tversion union
select vdt.rnum, cast(vdt.cdt as char(10)) from vdt
) Q
group by
f1,f2
) Q ) P;
--- CastDateToChar_p2
select 'CastDateToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1996-01-01' f2 from tversion union
select 2 f1, '2000-01-01' f2 from tversion union
select 3 f1, '2000-12-31' f2 from tversion union
select tdt.rnum, cast(tdt.cdt as char(10)) from tdt
) Q
group by
f1,f2
) Q ) P;
--- AggregateInExpression_p1
select 'AggregateInExpression_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select 10 * count( 1 ) from tversion
) Q
group by
f1
) Q ) P;
--- CastDateToDate_p1
select 'CastDateToDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, cast('1996-01-01' as date) f2 from tversion union
select 2 f1, cast('2000-01-01' as date) f2 from tversion union
select 3 f1, cast('2000-12-31' as date) f2 from tversion union
select vdt.rnum, cast(vdt.cdt as date) from vdt
) Q
group by
f1,f2
) Q ) P;
--- CastDateToDate_p2
select 'CastDateToDate_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, cast('1996-01-01' as date) f2 from tversion union
select 2 f1, cast('2000-01-01' as date) f2 from tversion union
select 3 f1, cast('2000-12-31' as date) f2 from tversion union
select tdt.rnum, cast(tdt.cdt as date) from tdt
) Q
group by
f1,f2
) Q ) P;
--- CastDateToVarchar_p1
select 'CastDateToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1996-01-01' f2 from tversion union
select 2 f1, '2000-01-01' f2 from tversion union
select 3 f1, '2000-12-31' f2 from tversion union
select vdt.rnum, cast(vdt.cdt as varchar(10)) from vdt
) Q
group by
f1,f2
) Q ) P;
--- CastDateToVarchar_p2
select 'CastDateToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1996-01-01' f2 from tversion union
select 2 f1, '2000-01-01' f2 from tversion union
select 3 f1, '2000-12-31' f2 from tversion union
select tdt.rnum, cast(tdt.cdt as varchar(10)) from tdt
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToBigint_p1
select 'CastDecimalToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vdec.cdec as bigint) from vdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToBigint_p2
select 'CastDecimalToBigint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(tdec.cdec as bigint) from tdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToChar_p1
select 'CastDecimalToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1.00' f2 from tversion union
select 2 f1, '0.00 ' f2 from tversion union
select 3 f1, '1.00 ' f2 from tversion union
select 4 f1, '0.10 ' f2 from tversion union
select 5 f1, '10.00' f2 from tversion union
select rnum, cast(vdec.cdec as char(5)) from vdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToChar_p2
select 'CastDecimalToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1.00' f2 from tversion union
select 2 f1, '0.00 ' f2 from tversion union
select 3 f1, '1.00 ' f2 from tversion union
select 4 f1, '0.10 ' f2 from tversion union
select 5 f1, '10.00' f2 from tversion union
select rnum, cast(tdec.cdec as char(5)) from tdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToDouble_p1
select 'CastDecimalToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, 0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(vdec.cdec as double precision) from vdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToDouble_p2
select 'CastDecimalToDouble_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, 0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(tdec.cdec as double precision) from tdec
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpAdd_p1
select 'ApproximateNumericOpAdd_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 2.000000000000000e+000 f2 from tversion union
select 3 f1, 3.000000000000000e+000 f2 from tversion union
select 4 f1, 1.900000000000000e+000 f2 from tversion union
select 5 f1, 1.200000000000000e+001 f2 from tversion union
select vflt.rnum,vflt.cflt + 2 from vflt
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToFloat_p1
select 'CastDecimalToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, 0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(vdec.cdec as float) from vdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToFloat_p2
select 'CastDecimalToFloat_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, 0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(tdec.cdec as float) from tdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToInteger_p1
select 'CastDecimalToInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vdec.cdec as integer) from vdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToInteger_p2
select 'CastDecimalToInteger_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(tdec.cdec as integer) from tdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToSmallint_p1
select 'CastDecimalToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vdec.cdec as smallint) from vdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToSmallint_p2
select 'CastDecimalToSmallint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(tdec.cdec as smallint) from tdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToVarchar_p1
select 'CastDecimalToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1.00' f2 from tversion union
select 2 f1, '0.00' f2 from tversion union
select 3 f1, '1.00' f2 from tversion union
select 4 f1, '0.10' f2 from tversion union
select 5 f1, '10.00' f2 from tversion union
select rnum, cast(vdec.cdec as varchar(5)) from vdec
) Q
group by
f1,f2
) Q ) P;
--- CastDecimalToVarchar_p2
select 'CastDecimalToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1.00' f2 from tversion union
select 2 f1, '0.00' f2 from tversion union
select 3 f1, '1.00' f2 from tversion union
select 4 f1, '0.10' f2 from tversion union
select 5 f1, '10.00' f2 from tversion union
select rnum, cast(tdec.cdec as varchar(5)) from tdec
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToBigint_p1
select 'CastDoubleToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vdbl.cdbl as bigint) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToBigint_p2
select 'CastDoubleToBigint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(tdbl.cdbl as bigint) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpAdd_p2
select 'ApproximateNumericOpAdd_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 2.000000000000000e+000 f2 from tversion union
select 3 f1, 3.000000000000000e+000 f2 from tversion union
select 4 f1, 1.900000000000000e+000 f2 from tversion union
select 5 f1, 1.200000000000000e+001 f2 from tversion union
select tflt.rnum,tflt.cflt + 2 from tflt
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToChar_p1
select 'CastDoubleToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1E+0               ' f2 from tversion union
select 2 f1, '0E0                 ' f2 from tversion union
select 3 f1, '1E0                 ' f2 from tversion union
select 4 f1, '-1E-1               ' f2 from tversion union
select 5 f1, '1E1                 ' f2 from tversion union
select rnum, cast(vdbl.cdbl as char(20)) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToChar_p2
select 'CastDoubleToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1E+0               ' f2 from tversion union
select 2 f1, '0E0                 ' f2 from tversion union
select 3 f1, '1E0                 ' f2 from tversion union
select 4 f1, '-1E-1               ' f2 from tversion union
select 5 f1, '1E1                 ' f2 from tversion union
select rnum, cast(tdbl.cdbl as char(20)) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToDouble_p1
select 'CastDoubleToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, -0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(vdbl.cdbl as double precision) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToDouble_p2
select 'CastDoubleToDouble_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, -0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(tdbl.cdbl as double precision) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToFloat_p1
select 'CastDoubleToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, -0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(vdbl.cdbl as float) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToFloat_p2
select 'CastDoubleToFloat_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, -0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(tdbl.cdbl as float) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToSmallint_p1
select 'CastDoubleToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vdbl.cdbl as smallint) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToSmallint_p2
select 'CastDoubleToSmallint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(tdbl.cdbl as smallint) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToVarchar_p1
select 'CastDoubleToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1E+0' f2 from tversion union
select 2 f1, '0E0' f2 from tversion union
select 3 f1, '1E0' f2 from tversion union
select 4 f1, '-1E-1' f2 from tversion union
select 5 f1, '1E1' f2 from tversion union
select rnum, cast(vdbl.cdbl as varchar(20)) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CastDoubleToVarchar_p2
select 'CastDoubleToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1E+0' f2 from tversion union
select 2 f1, '0E0' f2 from tversion union
select 3 f1, '1E0' f2 from tversion union
select 4 f1, '-1E-1' f2 from tversion union
select 5 f1, '1E1' f2 from tversion union
select rnum, cast(tdbl.cdbl as varchar(20)) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- AbsCoreApproximateNumeric_p2
select 'AbsCoreApproximateNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 1.000000000000000e+000 f2 from tversion union
select 4 f1, 1.000000000000000e-001 f2 from tversion union
select 5 f1, 1.000000000000000e+001 f2 from tversion union
select rnum, abs( tflt.cflt ) from tflt
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpAdd_p3
select 'ApproximateNumericOpAdd_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 2.000000000000000e+000 f2 from tversion union
select 3 f1, 3.000000000000000e+000 f2 from tversion union
select 4 f1, 1.900000000000000e+000 f2 from tversion union
select 5 f1, 1.200000000000000e+001 f2 from tversion union
select vdbl.rnum,vdbl.cdbl + 2 from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToBigint_p1
select 'CastFloatToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vflt.cflt as bigint) from vflt
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToBigint_p2
select 'CastFloatToBigint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(tflt.cflt as bigint) from tflt
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToChar_p1
select 'CastFloatToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1E+0               ' f2 from tversion union
select 2 f1, '0E0                 ' f2 from tversion union
select 3 f1, '1E0                 ' f2 from tversion union
select 4 f1, '-1E-1               ' f2 from tversion union
select 5 f1, '1E1                 ' f2 from tversion union
select rnum, cast(vflt.cflt as char(20)) from vflt
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToChar_p2
select 'CastFloatToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1E+0               ' f2 from tversion union
select 2 f1, '0E0                 ' f2 from tversion union
select 3 f1, '1E0                 ' f2 from tversion union
select 4 f1, '-1E-1               ' f2 from tversion union
select 5 f1, '1E1                 ' f2 from tversion union
select rnum, cast(tflt.cflt as char(20)) from tflt
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToDouble_p1
select 'CastFloatToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, -0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(vflt.cflt as double precision) from vflt
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToDouble_p2
select 'CastFloatToDouble_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, -0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(tflt.cflt as double precision) from tflt
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToFloat_p1
select 'CastFloatToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, -0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(vflt.cflt as float) from vflt
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToFloat_p2
select 'CastFloatToFloat_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, -0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(tflt.cflt as float) from tflt
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToSmallint_p1
select 'CastFloatToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vflt.cflt as smallint) from vflt
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToSmallint_p2
select 'CastFloatToSmallint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(tflt.cflt as smallint) from tflt
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpAdd_p4
select 'ApproximateNumericOpAdd_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 2.000000000000000e+000 f2 from tversion union
select 3 f1, 3.000000000000000e+000 f2 from tversion union
select 4 f1, 1.900000000000000e+000 f2 from tversion union
select 5 f1, 1.200000000000000e+001 f2 from tversion union
select tdbl.rnum,tdbl.cdbl + 2 from tdbl
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToVarchar_p1
select 'CastFloatToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1E+0' f2 from tversion union
select 2 f1, '0E0' f2 from tversion union
select 3 f1, '1E0' f2 from tversion union
select 4 f1, '-1E-1' f2 from tversion union
select 5 f1, '1E1' f2 from tversion union
select rnum, cast(vflt.cflt as varchar(10)) from vflt
) Q
group by
f1,f2
) Q ) P;
--- CastFloatToVarchar_p2
select 'CastFloatToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1E+0' f2 from tversion union
select 2 f1, '0E0' f2 from tversion union
select 3 f1, '1E0' f2 from tversion union
select 4 f1, '-1E-1' f2 from tversion union
select 5 f1, '1E1' f2 from tversion union
select rnum, cast(tflt.cflt as varchar(10)) from tflt
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToBigint_p1
select 'CastIntegerToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vint.cint as bigint) from vint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToBigint_p2
select 'CastIntegerToBigint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tint.cint as bigint) from tint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToChar_p1
select 'CastIntegerToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1   ' f2 from tversion union
select 2 f1, '0    ' f2 from tversion union
select 3 f1, '1    ' f2 from tversion union
select 4 f1, '10   ' f2 from tversion union
select rnum, cast(vint.cint as char(5)) from vint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToChar_p2
select 'CastIntegerToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1   ' f2 from tversion union
select 2 f1, '0    ' f2 from tversion union
select 3 f1, '1    ' f2 from tversion union
select 4 f1, '10   ' f2 from tversion union
select rnum, cast(tint.cint as char(5)) from tint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToDouble_p1
select 'CastIntegerToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vint.cint as double precision) from vint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToDouble_p2
select 'CastIntegerToDouble_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tint.cint as double precision) from tint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToFloat_p1
select 'CastIntegerToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vint.cint as float) from vint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToFloat_p2
select 'CastIntegerToFloat_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tint.cint as float) from tint
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpAdd_p5
select 'ApproximateNumericOpAdd_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 2.000000000000000e+000 f2 from tversion union
select 3 f1, 3.000000000000000e+000 f2 from tversion union
select 4 f1, 1.900000000000000e+000 f2 from tversion union
select 5 f1, 1.200000000000000e+001 f2 from tversion union
select vrl.rnum,vrl.crl + 2 from vrl
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToInteger_p1
select 'CastIntegerToInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vint.cint as integer) from vint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToInteger_p2
select 'CastIntegerToInteger_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tint.cint as integer) from tint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToSmallint_p1
select 'CastIntegerToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vint.cint as smallint) from vint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToSmallint_p2
select 'CastIntegerToSmallint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tint.cint as smallint) from tint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToVarchar_p1
select 'CastIntegerToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0' f2 from tversion union
select 3 f1, '1' f2 from tversion union
select 4 f1, '10' f2 from tversion union
select rnum, cast(vint.cint as varchar(5)) from vint
) Q
group by
f1,f2
) Q ) P;
--- CastIntegerToVarchar_p2
select 'CastIntegerToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0' f2 from tversion union
select 3 f1, '1' f2 from tversion union
select 4 f1, '10' f2 from tversion union
select rnum, cast(tint.cint as varchar(5)) from tint
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToBigint_p1
select 'CastNumericToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vnum.cnum as bigint) from vnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToBigint_p2
select 'CastNumericToBigint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(tnum.cnum as bigint) from tnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToChar_p1
select 'CastNumericToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1.00' f2 from tversion union
select 2 f1, '0.00 ' f2 from tversion union
select 3 f1, '1.00 ' f2 from tversion union
select 4 f1, '0.10 ' f2 from tversion union
select 5 f1, '10.00' f2 from tversion union
select rnum, cast(vnum.cnum as char(5)) from vnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToChar_p2
select 'CastNumericToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1.00' f2 from tversion union
select 2 f1, '0.00 ' f2 from tversion union
select 3 f1, '1.00 ' f2 from tversion union
select 4 f1, '0.10 ' f2 from tversion union
select 5 f1, '10.00' f2 from tversion union
select rnum, cast(tnum.cnum as char(5)) from tnum
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpAdd_p6
select 'ApproximateNumericOpAdd_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 2.000000000000000e+000 f2 from tversion union
select 3 f1, 3.000000000000000e+000 f2 from tversion union
select 4 f1, 1.900000000000000e+000 f2 from tversion union
select 5 f1, 1.200000000000000e+001 f2 from tversion union
select trl.rnum,trl.crl + 2 from trl
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToDouble_p1
select 'CastNumericToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, 0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(vnum.cnum as double precision) from vnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToDouble_p2
select 'CastNumericToDouble_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, 0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(tnum.cnum as double precision) from tnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToFloat_p1
select 'CastNumericToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, 0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(vnum.cnum as float) from vnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToFloat_p2
select 'CastNumericToFloat_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, 0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(tnum.cnum as float) from tnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToInteger_p1
select 'CastNumericToInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vnum.cnum as integer) from vnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToInteger_p2
select 'CastNumericToInteger_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(tnum.cnum as integer) from tnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToSmallint_p1
select 'CastNumericToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vnum.cnum as smallint) from vnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToSmallint_p2
select 'CastNumericToSmallint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(tnum.cnum as smallint) from tnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToVarchar_p1
select 'CastNumericToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1.00' f2 from tversion union
select 2 f1, '0.00' f2 from tversion union
select 3 f1, '1.00' f2 from tversion union
select 4 f1, '0.10' f2 from tversion union
select 5 f1, '10.00' f2 from tversion union
select rnum, cast(vnum.cnum as varchar(5)) from vnum
) Q
group by
f1,f2
) Q ) P;
--- CastNumericToVarchar_p2
select 'CastNumericToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1.00' f2 from tversion union
select 2 f1, '0.00' f2 from tversion union
select 3 f1, '1.00' f2 from tversion union
select 4 f1, '0.10' f2 from tversion union
select 5 f1, '10.00' f2 from tversion union
select rnum, cast(tnum.cnum as varchar(5)) from tnum
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpDiv_p1
select 'ApproximateNumericOpDiv_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -5.000000000000000e-001 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 5.000000000000000e-001 f2 from tversion union
select 4 f1, -5.000000000000000e-002 f2 from tversion union
select 5 f1, 5.000000000000000e+000 f2 from tversion union
select vflt.rnum,vflt.cflt / 2 from vflt
) Q
group by
f1,f2
) Q ) P;
--- CastNvarcharToBigint_p1
select 'CastNvarcharToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast(n'1' as bigint) from tversion
) Q
group by
f1
) Q ) P;
--- CastNvarcharToDouble_p1
select 'CastNvarcharToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.000000000000000e+000 f1 from tversion union
select cast(n'1.0' as double precision) from tversion
) Q
group by
f1
) Q ) P;
--- CastNvarcharToInteger_p1
select 'CastNvarcharToInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast(n'1' as integer) from tversion
) Q
group by
f1
) Q ) P;
--- CastNvarcharToSmallint_p1
select 'CastNvarcharToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast(n'1' as smallint) from tversion
) Q
group by
f1
) Q ) P;
--- CastRealToBigint_p1
select 'CastRealToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vrl.crl as bigint) from vrl
) Q
group by
f1,f2
) Q ) P;
--- CastRealToBigint_p2
select 'CastRealToBigint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(trl.crl as bigint) from trl
) Q
group by
f1,f2
) Q ) P;
--- CastRealToChar_p1
select 'CastRealToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1        ' f2 from tversion union
select 2 f1, '0         ' f2 from tversion union
select 3 f1, '1         ' f2 from tversion union
select 4 f1, '-0.1      ' f2 from tversion union
select 5 f1, '10        ' f2 from tversion union
select rnum, cast(vrl.crl as char(10)) from vrl
) Q
group by
f1,f2
) Q ) P;
--- CastRealToChar_p2
select 'CastRealToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1        ' f2 from tversion union
select 2 f1, '0         ' f2 from tversion union
select 3 f1, '1         ' f2 from tversion union
select 4 f1, '-0.1      ' f2 from tversion union
select 5 f1, '10        ' f2 from tversion union
select rnum, cast(trl.crl as char(10)) from trl
) Q
group by
f1,f2
) Q ) P;
--- CastRealToDouble_p1
select 'CastRealToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, -0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(vrl.crl as double precision) from vrl
) Q
group by
f1,f2
) Q ) P;
--- CastRealToDouble_p2
select 'CastRealToDouble_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1.0 f2 from tversion union
select 2 f1, 0.0 f2 from tversion union
select 3 f1, 1.0 f2 from tversion union
select 4 f1, -0.1 f2 from tversion union
select 5 f1, 10.0 f2 from tversion union
select rnum, cast(trl.crl as double precision) from trl
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpDiv_p2
select 'ApproximateNumericOpDiv_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -5.000000000000000e-001 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 5.000000000000000e-001 f2 from tversion union
select 4 f1, -5.000000000000000e-002 f2 from tversion union
select 5 f1, 5.000000000000000e+000 f2 from tversion union
select tflt.rnum,tflt.cflt / 2 from tflt
) Q
group by
f1,f2
) Q ) P;
--- CastRealToSmallint_p1
select 'CastRealToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(vrl.crl as smallint) from vrl
) Q
group by
f1,f2
) Q ) P;
--- CastRealToSmallint_p2
select 'CastRealToSmallint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, cast(trl.crl as smallint) from trl
) Q
group by
f1,f2
) Q ) P;
--- CastRealToVarchar_p1
select 'CastRealToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0' f2 from tversion union
select 3 f1, '1' f2 from tversion union
select 4 f1, '-0.1' f2 from tversion union
select 5 f1, '10' f2 from tversion union
select rnum, cast(vrl.crl as varchar(10)) from vrl
) Q
group by
f1,f2
) Q ) P;
--- CastRealToVarchar_p2
select 'CastRealToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0' f2 from tversion union
select 3 f1, '1' f2 from tversion union
select 4 f1, '-0.1' f2 from tversion union
select 5 f1, '10' f2 from tversion union
select rnum, cast(trl.crl as varchar(10)) from trl
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToBigint_p1
select 'CastSmallintToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vsint.csint as bigint) from vsint
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToBigint_p2
select 'CastSmallintToBigint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tsint.csint as bigint) from tsint
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToChar_p1
select 'CastSmallintToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0 ' f2 from tversion union
select 3 f1, '1 ' f2 from tversion union
select 4 f1, '10' f2 from tversion union
select rnum, cast(vsint.csint as char(2)) from vsint
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToChar_p2
select 'CastSmallintToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0 ' f2 from tversion union
select 3 f1, '1 ' f2 from tversion union
select 4 f1, '10' f2 from tversion union
select rnum, cast(tsint.csint as char(2)) from tsint
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToDouble_p1
select 'CastSmallintToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vsint.csint as double precision) from vsint
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToDouble_p2
select 'CastSmallintToDouble_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tsint.csint as double precision) from tsint
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpDiv_p3
select 'ApproximateNumericOpDiv_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -5.000000000000000e-001 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 5.000000000000000e-001 f2 from tversion union
select 4 f1, -5.000000000000000e-002 f2 from tversion union
select 5 f1, 5.000000000000000e+000 f2 from tversion union
select vdbl.rnum,vdbl.cdbl / 2 from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToFloat_p1
select 'CastSmallintToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vsint.csint as float) from vsint
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToFloat_p2
select 'CastSmallintToFloat_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tsint.csint as float) from tsint
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToSmallint_p1
select 'CastSmallintToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(vsint.csint as smallint) from vsint
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToSmallint_p2
select 'CastSmallintToSmallint_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, cast(tsint.csint as smallint) from tsint
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToVarchar_p1
select 'CastSmallintToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0' f2 from tversion union
select 3 f1, '1' f2 from tversion union
select 4 f1, '10' f2 from tversion union
select rnum, cast(vsint.csint as varchar(10)) from vsint
) Q
group by
f1,f2
) Q ) P;
--- CastSmallintToVarchar_p2
select 'CastSmallintToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '-1' f2 from tversion union
select 2 f1, '0' f2 from tversion union
select 3 f1, '1' f2 from tversion union
select 4 f1, '10' f2 from tversion union
select rnum, cast(tsint.csint as varchar(10)) from tsint
) Q
group by
f1,f2
) Q ) P;
--- CastTimestampToChar_p1
select 'CastTimestampToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1996-01-01 00:00:00.000       ' f2 from tversion union
select 2 f1, '1996-01-01 12:00:00.000       ' f2 from tversion union
select 3 f1, '1996-01-01 23:59:30.123       ' f2 from tversion union
select 4 f1, '2000-01-01 00:00:00.000       ' f2 from tversion union
select 5 f1, '2000-01-01 12:00:00.000       ' f2 from tversion union
select 6 f1, '2000-01-01 23:59:30.123       ' f2 from tversion union
select 7 f1, '2000-12-31 00:00:00.000       ' f2 from tversion union
select 8 f1, '2000-12-31 12:00:00.000       ' f2 from tversion union
select 9 f1, '2000-12-31 12:15:30.123       ' f2 from tversion union
select vts.rnum, cast(vts.cts as char(30)) from vts
) Q
group by
f1,f2
) Q ) P;
--- CastTimestampToChar_p2
select 'CastTimestampToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1996-01-01 00:00:00.000       ' f2 from tversion union
select 2 f1, '1996-01-01 12:00:00.000       ' f2 from tversion union
select 3 f1, '1996-01-01 23:59:30.123       ' f2 from tversion union
select 4 f1, '2000-01-01 00:00:00.000       ' f2 from tversion union
select 5 f1, '2000-01-01 12:00:00.000       ' f2 from tversion union
select 6 f1, '2000-01-01 23:59:30.123       ' f2 from tversion union
select 7 f1, '2000-12-31 00:00:00.000       ' f2 from tversion union
select 8 f1, '2000-12-31 12:00:00.000       ' f2 from tversion union
select 9 f1, '2000-12-31 12:15:30.123       ' f2 from tversion union
select tts.rnum, cast(tts.cts as char(30)) from tts
) Q
group by
f1,f2
) Q ) P;
--- CastTimestampToDate_p1
select 'CastTimestampToDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, cast('1996-01-01' as date) f2 from tversion union
select 2 f1, cast('1996-01-01' as date) f2 from tversion union
select 3 f1, cast('1996-01-01' as date) f2 from tversion union
select 4 f1, cast('2000-01-01' as date) f2 from tversion union
select 5 f1, cast('2000-01-01' as date) f2 from tversion union
select 6 f1, cast('2000-01-01' as date) f2 from tversion union
select 7 f1, cast('2000-12-31' as date) f2 from tversion union
select 8 f1, cast('2000-12-31' as date) f2 from tversion union
select 9 f1, cast('2000-12-31' as date) f2 from tversion union
select vts.rnum, cast(vts.cts as date) from vts
) Q
group by
f1,f2
) Q ) P;
--- CastTimestampToDate_p2
select 'CastTimestampToDate_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, cast('1996-01-01' as date) f2 from tversion union
select 2 f1, cast('1996-01-01' as date) f2 from tversion union
select 3 f1, cast('1996-01-01' as date) f2 from tversion union
select 4 f1, cast('2000-01-01' as date) f2 from tversion union
select 5 f1, cast('2000-01-01' as date) f2 from tversion union
select 6 f1, cast('2000-01-01' as date) f2 from tversion union
select 7 f1, cast('2000-12-31' as date) f2 from tversion union
select 8 f1, cast('2000-12-31' as date) f2 from tversion union
select 9 f1, cast('2000-12-31' as date) f2 from tversion union
select tts.rnum, cast(tts.cts as date) from tts
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpDiv_p4
select 'ApproximateNumericOpDiv_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -5.000000000000000e-001 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 5.000000000000000e-001 f2 from tversion union
select 4 f1, -5.000000000000000e-002 f2 from tversion union
select 5 f1, 5.000000000000000e+000 f2 from tversion union
select tdbl.rnum,tdbl.cdbl / 2 from tdbl
) Q
group by
f1,f2
) Q ) P;
--- CastTimestampToVarchar_p1
select 'CastTimestampToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1996-01-01 00:00:00.000' f2 from tversion union
select 2 f1, '1996-01-01 12:00:00.000' f2 from tversion union
select 3 f1, '1996-01-01 23:59:30.123' f2 from tversion union
select 4 f1, '2000-01-01 00:00:00.000' f2 from tversion union
select 5 f1, '2000-01-01 12:00:00.000' f2 from tversion union
select 6 f1, '2000-01-01 23:59:30.123' f2 from tversion union
select 7 f1, '2000-12-31 00:00:00.000' f2 from tversion union
select 8 f1, '2000-12-31 12:00:00.000000000' f2 from tversion union
select 9 f1, '2000-12-31 12:15:30.123000000' f2 from tversion union
select vts.rnum,cast(vts.cts as varchar(100)) from vts
) Q
group by
f1,f2
) Q ) P;
--- CastTimestampToVarchar_p2
select 'CastTimestampToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1996-01-01 00:00:00.000' f2 from tversion union
select 2 f1, '1996-01-01 12:00:00.000' f2 from tversion union
select 3 f1, '1996-01-01 23:59:30.123' f2 from tversion union
select 4 f1, '2000-01-01 00:00:00.000' f2 from tversion union
select 5 f1, '2000-01-01 12:00:00.000' f2 from tversion union
select 6 f1, '2000-01-01 23:59:30.123' f2 from tversion union
select 7 f1, '2000-12-31 00:00:00.000' f2 from tversion union
select 8 f1, '2000-12-31 12:00:00.000000000' f2 from tversion union
select 9 f1, '2000-12-31 12:15:30.123000000' f2 from tversion union
select tts.rnum,cast(tts.cts as varchar(100)) from tts
) Q
group by
f1,f2
) Q ) P;
--- CastTimeToChar_p1
select 'CastTimeToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '00:00:00.000        ' f2 from tversion union
select 2 f1, '12:00:00.000        ' f2 from tversion union
select 3 f1, '23:59:30.123        ' f2 from tversion union
select vtm.rnum, cast(vtm.ctm as char(20)) from vtm
) Q
group by
f1,f2
) Q ) P;
--- CastTimeToChar_p2
select 'CastTimeToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '00:00:00.000        ' f2 from tversion union
select 2 f1, '12:00:00.000        ' f2 from tversion union
select 3 f1, '23:59:30.123        ' f2 from tversion union
select ttm.rnum, cast(ttm.ctm as char(20)) from ttm
) Q
group by
f1,f2
) Q ) P;
--- CastTimeToVarchar_p1
select 'CastTimeToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '00:00:00.000' f2 from tversion union
select 2 f1, '12:00:00.000' f2 from tversion union
select 3 f1, '23:59:30.123' f2 from tversion union
select vtm.rnum,cast(vtm.ctm as varchar(100)) from vtm
) Q
group by
f1,f2
) Q ) P;
--- CastTimeToVarchar_p2
select 'CastTimeToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '00:00:00.000' f2 from tversion union
select 2 f1, '12:00:00.000' f2 from tversion union
select 3 f1, '23:59:30.123' f2 from tversion union
select ttm.rnum,cast(ttm.ctm as varchar(100)) from ttm
) Q
group by
f1,f2
) Q ) P;
--- CastVarcharToBigint_p1
select 'CastVarcharToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast('1' as bigint) from tversion
) Q
group by
f1
) Q ) P;
--- CastVarcharToChar_p1
select 'CastVarcharToChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, cast(vvchar.cvchar as char(2)) from vvchar
) Q
group by
f1,f2
) Q ) P;
--- CastVarcharToChar_p2
select 'CastVarcharToChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, cast(tvchar.cvchar as char(2)) from tvchar
) Q
group by
f1,f2
) Q ) P;
--- CastVarcharToDate_p1
select 'CastVarcharToDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpDiv_p5
select 'ApproximateNumericOpDiv_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -5.000000000000000e-001 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 5.000000000000000e-001 f2 from tversion union
select 4 f1, -5.000000000000000e-002 f2 from tversion union
select 5 f1, 5.000000000000000e+000 f2 from tversion union
select vrl.rnum,vrl.crl / 2 from vrl
) Q
group by
f1,f2
) Q ) P;
--- CastVarcharToDate_p2
select 'CastVarcharToDate_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
--- CastVarcharToDate_p3
select 'CastVarcharToDate_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
--- CastVarcharToDate_p4
select 'CastVarcharToDate_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
--- CastVarcharToDate_p5
select 'CastVarcharToDate_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
--- CastVarcharToDouble_p1
select 'CastVarcharToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.000000000000000e+000 f1 from tversion union
select cast('1.0' as double precision) from tversion
) Q
group by
f1
) Q ) P;
--- CastVarcharToFloat_p1
select 'CastVarcharToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.000000000000000e+000 f1 from tversion union
select cast('1.0' as float) from tversion
) Q
group by
f1
) Q ) P;
--- CastVarcharToFloat_p2
select 'CastVarcharToFloat_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.000000000000000e+000 f1 from tversion union
select cast('1.0' as float) from tversion
) Q
group by
f1
) Q ) P;
--- CastVarcharToInteger_p1
select 'CastVarcharToInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast('1' as integer) from tversion
) Q
group by
f1
) Q ) P;
--- CastVarcharToSmallint_p1
select 'CastVarcharToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast('1' as smallint) from tversion
) Q
group by
f1
) Q ) P;
--- CastVarcharToTimestamp_p1
select 'CastVarcharToTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01 12:00:00.000000000' as timestamp) f1 from tversion union
select cast('2000-01-01 12:00:00' as timestamp) from tversion
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpDiv_p6
select 'ApproximateNumericOpDiv_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -5.000000000000000e-001 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 5.000000000000000e-001 f2 from tversion union
select 4 f1, -5.000000000000000e-002 f2 from tversion union
select 5 f1, 5.000000000000000e+000 f2 from tversion union
select trl.rnum,trl.crl / 2 from trl
) Q
group by
f1,f2
) Q ) P;
--- CastVarcharToVarchar_p1
select 'CastVarcharToVarchar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, cast(vvchar.cvchar as varchar(10)) from vvchar
) Q
group by
f1,f2
) Q ) P;
--- CastVarcharToVarchar_p2
select 'CastVarcharToVarchar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'BB' f2 from tversion union
select 4 f1, 'EE' f2 from tversion union
select 5 f1, 'FF' f2 from tversion union
select rnum, cast(tvchar.cvchar as varchar(10)) from tvchar
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreApproximateNumeric_p1
select 'CeilCoreApproximateNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, ceil( vflt.cflt ) from vflt
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreApproximateNumeric_p2
select 'CeilCoreApproximateNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, ceil( tflt.cflt ) from tflt
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreApproximateNumeric_p3
select 'CeilCoreApproximateNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, ceil( vdbl.cdbl ) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreApproximateNumeric_p4
select 'CeilCoreApproximateNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, ceil( tdbl.cdbl ) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreApproximateNumeric_p5
select 'CeilCoreApproximateNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, ceil( vrl.crl ) from vrl
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreApproximateNumeric_p6
select 'CeilCoreApproximateNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, ceil( trl.crl ) from trl
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreExactNumeric_p1
select 'CeilCoreExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, ceil( vdec.cdec ) from vdec
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreExactNumeric_p2
select 'CeilCoreExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, ceil( tdec.cdec ) from tdec
) Q
group by
f1,f2
) Q ) P;
--- AbsCoreApproximateNumeric_p3
select 'AbsCoreApproximateNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 1.000000000000000e+000 f2 from tversion union
select 4 f1, 1.000000000000000e-001 f2 from tversion union
select 5 f1, 1.000000000000000e+001 f2 from tversion union
select rnum, abs( vdbl.cdbl ) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpMulNULL_p1
select 'ApproximateNumericOpMulNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select -1.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- CeilCoreExactNumeric_p3
select 'CeilCoreExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, ceil( vnum.cnum ) from vnum
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreExactNumeric_p4
select 'CeilCoreExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, ceil( tnum.cnum ) from tnum
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreIntegers_p1
select 'CeilCoreIntegers_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select vint.rnum, ceil( vint.cint ) from vint
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreIntegers_p2
select 'CeilCoreIntegers_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select tint.rnum, ceil( tint.cint ) from tint
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreIntegers_p3
select 'CeilCoreIntegers_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select vsint.rnum, ceil( vsint.csint ) from vsint
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreIntegers_p4
select 'CeilCoreIntegers_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select tsint.rnum, ceil( tsint.csint ) from tsint
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreIntegers_p5
select 'CeilCoreIntegers_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select vbint.rnum, ceil( vbint.cbint ) from vbint
) Q
group by
f1,f2
) Q ) P;
--- CeilCoreIntegers_p6
select 'CeilCoreIntegers_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select tbint.rnum, ceil( tbint.cbint ) from tbint
) Q
group by
f1,f2
) Q ) P;
--- CharacterLiteral_p1
select 'CharacterLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'BB' f1 from tversion union
select 'BB' from tversion
) Q
group by
f1
) Q ) P;
--- CharacterLiteral_p2
select 'CharacterLiteral_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'BB' f1 from tversion union
select 'BB' from tversion
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpMulNULL_p2
select 'ApproximateNumericOpMulNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 0.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- CharlengthCoreFixedLength_p1
select 'CharlengthCoreFixedLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 32 f2 from tversion union
select 2 f1, 32 f2 from tversion union
select 3 f1, 32 f2 from tversion union
select 4 f1, 32 f2 from tversion union
select 5 f1, 32 f2 from tversion union
select vchar.rnum, char_length( vchar.cchar ) from vchar
) Q
group by
f1,f2
) Q ) P;
--- CharlengthCoreFixedLength_p2
select 'CharlengthCoreFixedLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 32 f2 from tversion union
select 2 f1, 32 f2 from tversion union
select 3 f1, 32 f2 from tversion union
select 4 f1, 32 f2 from tversion union
select 5 f1, 32 f2 from tversion union
select tchar.rnum, char_length( tchar.cchar ) from tchar
) Q
group by
f1,f2
) Q ) P;
--- CharlengthCoreVariableLength_p1
select 'CharlengthCoreVariableLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2 f2 from tversion union
select 4 f1, 2 f2 from tversion union
select 5 f1, 2 f2 from tversion union
select vvchar.rnum, char_length( vvchar.cvchar ) from vvchar
) Q
group by
f1,f2
) Q ) P;
--- CharlengthCoreVariableLength_p2
select 'CharlengthCoreVariableLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2 f2 from tversion union
select 4 f1, 2 f2 from tversion union
select 5 f1, 2 f2 from tversion union
select tvchar.rnum, char_length( tvchar.cvchar ) from tvchar
) Q
group by
f1,f2
) Q ) P;
--- CoalesceCoreNullParameters_p1
select 'CoalesceCoreNullParameters_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select coalesce( ccnull, ccnull, ccnull ) from tversion
) Q
group by
f1
) Q ) P;
--- CoalesceCore_p1
select 'CoalesceCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'A' f1 from tversion union
select coalesce( ccnull, 'A', 'B' ) from tversion
) Q
group by
f1
) Q ) P;
--- Comments1_p1
select 'Comments1_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select /* hello */ 1 from tversion
) Q
group by
f1
) Q ) P;
--- ConcatCoreFixedLength_p1
select 'ConcatCoreFixedLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1234567890                                ' f2 from tversion union
select 2 f1, '1234567890                                ' f2 from tversion union
select 3 f1, '1234567890BB                              ' f2 from tversion union
select 4 f1, '1234567890EE                              ' f2 from tversion union
select 5 f1, '1234567890FF                              ' f2 from tversion union
select vchar.rnum, '1234567890' || vchar.cchar  from vchar
) Q
group by
f1,f2
) Q ) P;
--- ConcatCoreFixedLength_p2
select 'ConcatCoreFixedLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1234567890                                ' f2 from tversion union
select 2 f1, '1234567890                                ' f2 from tversion union
select 3 f1, '1234567890BB                              ' f2 from tversion union
select 4 f1, '1234567890EE                              ' f2 from tversion union
select 5 f1, '1234567890FF                              ' f2 from tversion union
select tchar.rnum, '1234567890' || tchar.cchar  from tchar
) Q
group by
f1,f2
) Q ) P;
--- ConcatCoreMaxLengthStringPlusBlankString_p1
select 'ConcatCoreMaxLengthStringPlusBlankString_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'UDA_VARIABLE_LENGTH_MAX_STRING                                ' f1 from tversion union
select 'UDA_VARIABLE_LENGTH_MAX_STRING' || vchar.cchar  from vchar where vchar.rnum = 2
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpMulNULL_p3
select 'ApproximateNumericOpMulNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 1.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- ConcatCoreMaxLengthStringPlusBlankString_p2
select 'ConcatCoreMaxLengthStringPlusBlankString_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'UDA_VARIABLE_LENGTH_MAX_STRING                                ' f1 from tversion union
select 'UDA_VARIABLE_LENGTH_MAX_STRING' || tchar.cchar  from tchar where tchar.rnum = 2
) Q
group by
f1
) Q ) P;
--- ConcatCoreMaxLengthStringPlusBlankString_p3
select 'ConcatCoreMaxLengthStringPlusBlankString_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'UDA_VARIABLE_LENGTH_MAX_STRING                                ' f1 from tversion union
select 'UDA_VARIABLE_LENGTH_MAX_STRING' || vvchar.cvchar  from vvchar where vvchar.rnum = 2
) Q
group by
f1
) Q ) P;
--- ConcatCoreMaxLengthStringPlusBlankString_p4
select 'ConcatCoreMaxLengthStringPlusBlankString_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'UDA_VARIABLE_LENGTH_MAX_STRING                                ' f1 from tversion union
select 'UDA_VARIABLE_LENGTH_MAX_STRING' || tvchar.cvchar  from tvchar where tvchar.rnum = 2
) Q
group by
f1
) Q ) P;
--- ConcatCoreVariableLength_p1
select 'ConcatCoreVariableLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1234567890' f2 from tversion union
select 2 f1, '1234567890 ' f2 from tversion union
select 3 f1, '1234567890BB' f2 from tversion union
select 4 f1, '1234567890EE' f2 from tversion union
select 5 f1, '1234567890FF' f2 from tversion union
select rnum, '1234567890' || vvchar.cvchar  from vvchar
) Q
group by
f1,f2
) Q ) P;
--- ConcatCoreVariableLength_p2
select 'ConcatCoreVariableLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '1234567890' f2 from tversion union
select 2 f1, '1234567890 ' f2 from tversion union
select 3 f1, '1234567890BB' f2 from tversion union
select 4 f1, '1234567890EE' f2 from tversion union
select 5 f1, '1234567890FF' f2 from tversion union
select rnum, '1234567890' || tvchar.cvchar  from tvchar
) Q
group by
f1,f2
) Q ) P;
--- ConcatException_p1
select 'ConcatException_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select vchar.rnum,'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || vchar.cchar  from vchar where vchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
--- ConcatException_p2
select 'ConcatException_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select vchar.rnum, 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || vchar.cchar  from vchar where vchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
--- ConcatException_p3
select 'ConcatException_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select tchar.rnum,'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || tchar.cchar  from tchar where tchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
--- ConcatException_p4
select 'ConcatException_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select tchar.rnum, 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || tchar.cchar  from tchar where tchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
--- ConcatException_p5
select 'ConcatException_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select vvchar.rnum,'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || vvchar.cvchar  from vvchar where vvchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpMulNULL_p4
select 'ApproximateNumericOpMulNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select -1.0e-1 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- ConcatException_p6
select 'ConcatException_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select vvchar.rnum, 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || vvchar.cvchar  from vvchar where vvchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
--- ConcatException_p7
select 'ConcatException_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select tvchar.rnum,'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || tvchar.cvchar  from tvchar where tvchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
--- ConcatException_p8
select 'ConcatException_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select tvchar.rnum, 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || tvchar.cvchar  from tvchar where tvchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
--- CountCharLiteral_p1
select 'CountCharLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('') from tversion
) Q
group by
f1
) Q ) P;
--- CountCharLiteral_p10
select 'CountCharLiteral_p10' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('FF') from tversion
) Q
group by
f1
) Q ) P;
--- CountCharLiteral_p2
select 'CountCharLiteral_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(' ') from tversion
) Q
group by
f1
) Q ) P;
--- CountCharLiteral_p3
select 'CountCharLiteral_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('BB') from tversion
) Q
group by
f1
) Q ) P;
--- CountCharLiteral_p4
select 'CountCharLiteral_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('EE') from tversion
) Q
group by
f1
) Q ) P;
--- CountCharLiteral_p5
select 'CountCharLiteral_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('FF') from tversion
) Q
group by
f1
) Q ) P;
--- CountCharLiteral_p6
select 'CountCharLiteral_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('') from tversion
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpMulNULL_p5
select 'ApproximateNumericOpMulNULL_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 10.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- CountCharLiteral_p7
select 'CountCharLiteral_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(' ') from tversion
) Q
group by
f1
) Q ) P;
--- CountCharLiteral_p8
select 'CountCharLiteral_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('BB') from tversion
) Q
group by
f1
) Q ) P;
--- CountCharLiteral_p9
select 'CountCharLiteral_p9' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('EE') from tversion
) Q
group by
f1
) Q ) P;
--- CountClob_p1
select 'CountClob_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vclob.cclob) from vclob
) Q
group by
f1
) Q ) P;
--- CountClob_p2
select 'CountClob_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tclob.cclob) from tclob
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p1
select 'CountNumericLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p10
select 'CountNumericLiteral_p10' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p11
select 'CountNumericLiteral_p11' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p12
select 'CountNumericLiteral_p12' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p13
select 'CountNumericLiteral_p13' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpMul_p1
select 'ApproximateNumericOpMul_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 2.000000000000000e+000 f2 from tversion union
select 4 f1, -2.000000000000000e-001 f2 from tversion union
select 5 f1, 2.000000000000000e+001 f2 from tversion union
select vflt.rnum,vflt.cflt * 2 from vflt
) Q
group by
f1,f2
) Q ) P;
--- CountNumericLiteral_p14
select 'CountNumericLiteral_p14' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e-1) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p15
select 'CountNumericLiteral_p15' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p16
select 'CountNumericLiteral_p16' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p17
select 'CountNumericLiteral_p17' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p18
select 'CountNumericLiteral_p18' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1.0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p19
select 'CountNumericLiteral_p19' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.1) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p2
select 'CountNumericLiteral_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p20
select 'CountNumericLiteral_p20' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10.0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p21
select 'CountNumericLiteral_p21' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p22
select 'CountNumericLiteral_p22' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.0) from tversion
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpMul_p2
select 'ApproximateNumericOpMul_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 2.000000000000000e+000 f2 from tversion union
select 4 f1, -2.000000000000000e-001 f2 from tversion union
select 5 f1, 2.000000000000000e+001 f2 from tversion union
select tflt.rnum,tflt.cflt * 2 from tflt
) Q
group by
f1,f2
) Q ) P;
--- CountNumericLiteral_p23
select 'CountNumericLiteral_p23' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1.0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p24
select 'CountNumericLiteral_p24' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.1) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p25
select 'CountNumericLiteral_p25' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10.0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p26
select 'CountNumericLiteral_p26' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p27
select 'CountNumericLiteral_p27' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p28
select 'CountNumericLiteral_p28' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p29
select 'CountNumericLiteral_p29' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p3
select 'CountNumericLiteral_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p30
select 'CountNumericLiteral_p30' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p31
select 'CountNumericLiteral_p31' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0) from tversion
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpMul_p3
select 'ApproximateNumericOpMul_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 2.000000000000000e+000 f2 from tversion union
select 4 f1, -2.000000000000000e-001 f2 from tversion union
select 5 f1, 2.000000000000000e+001 f2 from tversion union
select vdbl.rnum,vdbl.cdbl * 2 from vdbl
) Q
group by
f1,f2
) Q ) P;
--- CountNumericLiteral_p32
select 'CountNumericLiteral_p32' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p33
select 'CountNumericLiteral_p33' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p34
select 'CountNumericLiteral_p34' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p35
select 'CountNumericLiteral_p35' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p36
select 'CountNumericLiteral_p36' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p37
select 'CountNumericLiteral_p37' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p4
select 'CountNumericLiteral_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e-1) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p5
select 'CountNumericLiteral_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p6
select 'CountNumericLiteral_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p7
select 'CountNumericLiteral_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpMul_p4
select 'ApproximateNumericOpMul_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 2.000000000000000e+000 f2 from tversion union
select 4 f1, -2.000000000000000e-001 f2 from tversion union
select 5 f1, 2.000000000000000e+001 f2 from tversion union
select tdbl.rnum,tdbl.cdbl * 2 from tdbl
) Q
group by
f1,f2
) Q ) P;
--- CountNumericLiteral_p8
select 'CountNumericLiteral_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
--- CountNumericLiteral_p9
select 'CountNumericLiteral_p9' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e-1) from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p1
select 'CountTemporalLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(date '1996-01-01') from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p10
select 'CountTemporalLiteral_p10' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-12-31 00:00:00') from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p11
select 'CountTemporalLiteral_p11' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-12-31 12:00:00') from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p12
select 'CountTemporalLiteral_p12' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-12-31 23:59:30.123') from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p2
select 'CountTemporalLiteral_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(date '2000-01-01') from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p3
select 'CountTemporalLiteral_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(date '2000-12-31') from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p4
select 'CountTemporalLiteral_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(time '00:00:00.000') from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p5
select 'CountTemporalLiteral_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(time '12:00:00.000') from tversion
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpMul_p5
select 'ApproximateNumericOpMul_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 2.000000000000000e+000 f2 from tversion union
select 4 f1, -2.000000000000000e-001 f2 from tversion union
select 5 f1, 2.000000000000000e+001 f2 from tversion union
select vrl.rnum,vrl.crl * 2 from vrl
) Q
group by
f1,f2
) Q ) P;
--- CountTemporalLiteral_p6
select 'CountTemporalLiteral_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(time '23:59:30.123') from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p7
select 'CountTemporalLiteral_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-01-01 00:00:00.0') from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p8
select 'CountTemporalLiteral_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-01-01 12:00:00') from tversion
) Q
group by
f1
) Q ) P;
--- CountTemporalLiteral_p9
select 'CountTemporalLiteral_p9' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-01-01 23:59:30.123') from tversion
) Q
group by
f1
) Q ) P;
--- CountValueExpression_p1
select 'CountValueExpression_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count( 1 * 10 ) from tversion
) Q
group by
f1
) Q ) P;
--- DateLiteral_p1
select 'DateLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '1996-01-01' f1 from tversion union
select date '1996-01-01' from tversion
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p1
select 'DistinctAggregateInCase_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vflt.cflt))=-1 then 'test1' else 'else' end from vflt
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p10
select 'DistinctAggregateInCase_p10' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tnum.cnum))=-1 then 'test1' else 'else' end from tnum
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p11
select 'DistinctAggregateInCase_p11' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vint.cint))=-1 then 'test1' else 'else' end from vint
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p12
select 'DistinctAggregateInCase_p12' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tint.cint))=-1 then 'test1' else 'else' end from tint
) Q
group by
f1
) Q ) P;
--- AbsCoreApproximateNumeric_p4
select 'AbsCoreApproximateNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 1.000000000000000e+000 f2 from tversion union
select 4 f1, 1.000000000000000e-001 f2 from tversion union
select 5 f1, 1.000000000000000e+001 f2 from tversion union
select rnum, abs( tdbl.cdbl ) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpMul_p6
select 'ApproximateNumericOpMul_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 2.000000000000000e+000 f2 from tversion union
select 4 f1, -2.000000000000000e-001 f2 from tversion union
select 5 f1, 2.000000000000000e+001 f2 from tversion union
select trl.rnum,trl.crl * 2 from trl
) Q
group by
f1,f2
) Q ) P;
--- DistinctAggregateInCase_p13
select 'DistinctAggregateInCase_p13' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vsint.csint))=-1 then 'test1' else 'else' end from vsint
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p14
select 'DistinctAggregateInCase_p14' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tsint.csint))=-1 then 'test1' else 'else' end from tsint
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p15
select 'DistinctAggregateInCase_p15' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vbint.cbint))=-1 then 'test1' else 'else' end from vbint
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p16
select 'DistinctAggregateInCase_p16' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tbint.cbint))=-1 then 'test1' else 'else' end from tbint
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p2
select 'DistinctAggregateInCase_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tflt.cflt))=-1 then 'test1' else 'else' end from tflt
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p3
select 'DistinctAggregateInCase_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vdbl.cdbl))=-1 then 'test1' else 'else' end from vdbl
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p4
select 'DistinctAggregateInCase_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tdbl.cdbl))=-1 then 'test1' else 'else' end from tdbl
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p5
select 'DistinctAggregateInCase_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vrl.crl))=-1 then 'test1' else 'else' end from vrl
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p6
select 'DistinctAggregateInCase_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(trl.crl))=-1 then 'test1' else 'else' end from trl
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p7
select 'DistinctAggregateInCase_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vdec.cdec))=-1 then 'test1' else 'else' end from vdec
) Q
group by
f1
) Q ) P;
--- ApproximateNumericOpSub_p1
select 'ApproximateNumericOpSub_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3.000000000000000e+000 f2 from tversion union
select 2 f1, -2.000000000000000e+000 f2 from tversion union
select 3 f1, -1.000000000000000e+000 f2 from tversion union
select 4 f1, -2.100000000000000e+000 f2 from tversion union
select 5 f1, 8.000000000000000e+000 f2 from tversion union
select vflt.rnum,vflt.cflt - 2 from vflt
) Q
group by
f1,f2
) Q ) P;
--- DistinctAggregateInCase_p8
select 'DistinctAggregateInCase_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tdec.cdec))=-1 then 'test1' else 'else' end from tdec
) Q
group by
f1
) Q ) P;
--- DistinctAggregateInCase_p9
select 'DistinctAggregateInCase_p9' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vnum.cnum))=-1 then 'test1' else 'else' end from vnum
) Q
group by
f1
) Q ) P;
--- DistinctCore_p1
select 'DistinctCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'AAA' f2 from tversion union
select 20 f1, 'BBB' f2 from tversion union
select 30 f1, 'CCC' f2 from tversion union
select 40 f1, 'DDD' f2 from tversion union
select 50 f1, null f2 from tversion union
select 60 f1, null f2 from tversion union
select null f1, 'AAA' f2 from tversion union
select null f1, null f2 from tversion union
select distinct c1, c2 from tset1
) Q
group by
f1,f2
) Q ) P;
--- EmptyStringIsNull_p1
select 'EmptyStringIsNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select vvchar.rnum, vvchar.cvchar  from vvchar where vvchar.cvchar is null
) Q
group by
f1,f2
) Q ) P;
--- EmptyStringIsNull_p2
select 'EmptyStringIsNull_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select tvchar.rnum, tvchar.cvchar  from tvchar where tvchar.cvchar is null
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpAdd_p1
select 'ExactNumericOpAdd_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1,  000001.00 f2 from tversion union
select 2 f1,  000002.00 f2 from tversion union
select 3 f1,  000003.00 f2 from tversion union
select 4 f1,  000002.10 f2 from tversion union
select 5 f1,  000012.00 f2 from tversion union
select vdec.rnum,vdec.cdec + 2 from vdec
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpAdd_p2
select 'ExactNumericOpAdd_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1,  000001.00 f2 from tversion union
select 2 f1,  000002.00 f2 from tversion union
select 3 f1,  000003.00 f2 from tversion union
select 4 f1,  000002.10 f2 from tversion union
select 5 f1,  000012.00 f2 from tversion union
select tdec.rnum,tdec.cdec + 2 from tdec
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpAdd_p3
select 'ExactNumericOpAdd_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1,  000001.00 f2 from tversion union
select 2 f1,  000002.00 f2 from tversion union
select 3 f1,  000003.00 f2 from tversion union
select 4 f1,  000002.10 f2 from tversion union
select 5 f1,  000012.00 f2 from tversion union
select vnum.rnum,vnum.cnum + 2 from vnum
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpAdd_p4
select 'ExactNumericOpAdd_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1,  000001.00 f2 from tversion union
select 2 f1,  000002.00 f2 from tversion union
select 3 f1,  000003.00 f2 from tversion union
select 4 f1,  000002.10 f2 from tversion union
select 5 f1,  000012.00 f2 from tversion union
select tnum.rnum,tnum.cnum + 2 from tnum
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpDiv_p1
select 'ExactNumericOpDiv_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -00000.500000 f2 from tversion union
select 2 f1,  00000.000000 f2 from tversion union
select 3 f1,  00000.500000 f2 from tversion union
select 4 f1,  00000.050000 f2 from tversion union
select 5 f1,  00005.000000 f2 from tversion union
select vdec.rnum,vdec.cdec / 2 from vdec
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpSub_p2
select 'ApproximateNumericOpSub_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3.000000000000000e+000 f2 from tversion union
select 2 f1, -2.000000000000000e+000 f2 from tversion union
select 3 f1, -1.000000000000000e+000 f2 from tversion union
select 4 f1, -2.100000000000000e+000 f2 from tversion union
select 5 f1, 8.000000000000000e+000 f2 from tversion union
select tflt.rnum,tflt.cflt - 2 from tflt
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpDiv_p2
select 'ExactNumericOpDiv_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -00000.500000 f2 from tversion union
select 2 f1,  00000.000000 f2 from tversion union
select 3 f1,  00000.500000 f2 from tversion union
select 4 f1,  00000.050000 f2 from tversion union
select 5 f1,  00005.000000 f2 from tversion union
select tdec.rnum,tdec.cdec / 2 from tdec
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpDiv_p3
select 'ExactNumericOpDiv_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -00000.500000 f2 from tversion union
select 2 f1,  00000.000000 f2 from tversion union
select 3 f1,  00000.500000 f2 from tversion union
select 4 f1,  00000.050000 f2 from tversion union
select 5 f1,  00005.000000 f2 from tversion union
select vnum.rnum,vnum.cnum / 2 from vnum
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpDiv_p4
select 'ExactNumericOpDiv_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -00000.500000 f2 from tversion union
select 2 f1,  00000.000000 f2 from tversion union
select 3 f1,  00000.500000 f2 from tversion union
select 4 f1,  00000.050000 f2 from tversion union
select 5 f1,  00005.000000 f2 from tversion union
select tnum.rnum,tnum.cnum / 2 from tnum
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpMulNULL_p1
select 'ExactNumericOpMulNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select -1.0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- ExactNumericOpMulNULL_p2
select 'ExactNumericOpMulNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 0.0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- ExactNumericOpMulNULL_p3
select 'ExactNumericOpMulNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 1.0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- ExactNumericOpMulNULL_p4
select 'ExactNumericOpMulNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 0.1 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- ExactNumericOpMulNULL_p5
select 'ExactNumericOpMulNULL_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 10.0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- ExactNumericOpMul_p1
select 'ExactNumericOpMul_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -0000002.00 f2 from tversion union
select 2 f1,  0000000.00 f2 from tversion union
select 3 f1,  0000002.00 f2 from tversion union
select 4 f1,  0000000.20 f2 from tversion union
select 5 f1,  0000020.00 f2 from tversion union
select vdec.rnum,vdec.cdec * 2 from vdec
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpMul_p2
select 'ExactNumericOpMul_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -0000002.00 f2 from tversion union
select 2 f1,  0000000.00 f2 from tversion union
select 3 f1,  0000002.00 f2 from tversion union
select 4 f1,  0000000.20 f2 from tversion union
select 5 f1,  0000020.00 f2 from tversion union
select tdec.rnum,tdec.cdec * 2 from tdec
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpSub_p3
select 'ApproximateNumericOpSub_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3.000000000000000e+000 f2 from tversion union
select 2 f1, -2.000000000000000e+000 f2 from tversion union
select 3 f1, -1.000000000000000e+000 f2 from tversion union
select 4 f1, -2.100000000000000e+000 f2 from tversion union
select 5 f1, 8.000000000000000e+000 f2 from tversion union
select vdbl.rnum,vdbl.cdbl - 2 from vdbl
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpMul_p3
select 'ExactNumericOpMul_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -0000002.00 f2 from tversion union
select 2 f1,  0000000.00 f2 from tversion union
select 3 f1,  0000002.00 f2 from tversion union
select 4 f1,  0000000.20 f2 from tversion union
select 5 f1,  0000020.00 f2 from tversion union
select vnum.rnum,vnum.cnum * 2 from vnum
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpMul_p4
select 'ExactNumericOpMul_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -0000002.00 f2 from tversion union
select 2 f1,  0000000.00 f2 from tversion union
select 3 f1,  0000002.00 f2 from tversion union
select 4 f1,  0000000.20 f2 from tversion union
select 5 f1,  0000020.00 f2 from tversion union
select tnum.rnum,tnum.cnum * 2 from tnum
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpSub_p1
select 'ExactNumericOpSub_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -000003.00 f2 from tversion union
select 2 f1, -000002.00 f2 from tversion union
select 3 f1, -000001.00 f2 from tversion union
select 4 f1, -000001.90 f2 from tversion union
select 5 f1,  000008.00 f2 from tversion union
select vdec.rnum,vdec.cdec - 2 from vdec
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpSub_p2
select 'ExactNumericOpSub_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -000003.00 f2 from tversion union
select 2 f1, -000002.00 f2 from tversion union
select 3 f1, -000001.00 f2 from tversion union
select 4 f1, -000001.90 f2 from tversion union
select 5 f1,  000008.00 f2 from tversion union
select tdec.rnum,tdec.cdec - 2 from tdec
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpSub_p3
select 'ExactNumericOpSub_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -000003.00 f2 from tversion union
select 2 f1, -000002.00 f2 from tversion union
select 3 f1, -000001.00 f2 from tversion union
select 4 f1, -000001.90 f2 from tversion union
select 5 f1,  000008.00 f2 from tversion union
select vnum.rnum,vnum.cnum - 2 from vnum
) Q
group by
f1,f2
) Q ) P;
--- ExactNumericOpSub_p4
select 'ExactNumericOpSub_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -000003.00 f2 from tversion union
select 2 f1, -000002.00 f2 from tversion union
select 3 f1, -000001.00 f2 from tversion union
select 4 f1, -000001.90 f2 from tversion union
select 5 f1,  000008.00 f2 from tversion union
select tnum.rnum,tnum.cnum - 2 from tnum
) Q
group by
f1,f2
) Q ) P;
--- ExceptAll_p1
select 'ExceptAll_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'AAA' f2 from tversion union
select 20 f1, 'BBB' f2 from tversion union
select 30 f1, 'CCC' f2 from tversion union
select 50 f1, null f2 from tversion union
select 60 f1, null f2 from tversion union
select null f1, 'AAA' f2 from tversion union
select null f1, 'AAA' f2 from tversion union
select null f1, null f2 from tversion union
select null f1, null f2 from tversion union
select c1, c2 from tset1 except all select c1, c2 from tset2
) Q
group by
f1,f2
) Q ) P;
--- Except_p1
select 'Except_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 20 f1, 'BBB' f2 from tversion union
select 30 f1, 'CCC' f2 from tversion union
select 50 f1, null f2 from tversion union
select 60 f1, null f2 from tversion union
select null f1, 'AAA' f2 from tversion union
select null f1, null f2 from tversion union
select c1, c2 from tset1 except select c1, c2 from tset2
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreApproximateNumeric_p1
select 'ExpCoreApproximateNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, .904837418 f2 from tversion union
select 5 f1, 22026.4658 f2 from tversion union
select vflt.rnum, exp( vflt.cflt ) from vflt
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreApproximateNumeric_p2
select 'ExpCoreApproximateNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, .904837418 f2 from tversion union
select 5 f1, 22026.4658 f2 from tversion union
select tflt.rnum, exp( tflt.cflt ) from tflt
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpSub_p4
select 'ApproximateNumericOpSub_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3.000000000000000e+000 f2 from tversion union
select 2 f1, -2.000000000000000e+000 f2 from tversion union
select 3 f1, -1.000000000000000e+000 f2 from tversion union
select 4 f1, -2.100000000000000e+000 f2 from tversion union
select 5 f1, 8.000000000000000e+000 f2 from tversion union
select tdbl.rnum,tdbl.cdbl - 2 from tdbl
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreApproximateNumeric_p3
select 'ExpCoreApproximateNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, .904837418 f2 from tversion union
select 5 f1, 22026.4658 f2 from tversion union
select vdbl.rnum, exp( vdbl.cdbl ) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreApproximateNumeric_p4
select 'ExpCoreApproximateNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, .904837418 f2 from tversion union
select 5 f1, 22026.4658 f2 from tversion union
select tdbl.rnum, exp( tdbl.cdbl ) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreApproximateNumeric_p5
select 'ExpCoreApproximateNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, .904837418 f2 from tversion union
select 5 f1, 22026.4658 f2 from tversion union
select vrl.rnum, exp( vrl.crl ) from vrl
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreApproximateNumeric_p6
select 'ExpCoreApproximateNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, .904837418 f2 from tversion union
select 5 f1, 22026.4658 f2 from tversion union
select trl.rnum, exp( trl.crl ) from trl
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreExactNumeric_p1
select 'ExpCoreExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, 1.10517092 f2 from tversion union
select 5 f1, 22026.4658 f2 from tversion union
select rnum, exp( vdec.cdec ) from vdec
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreExactNumeric_p2
select 'ExpCoreExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, 1.10517092 f2 from tversion union
select 5 f1, 22026.4658 f2 from tversion union
select rnum, exp( tdec.cdec ) from tdec
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreExactNumeric_p3
select 'ExpCoreExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, 1.10517092 f2 from tversion union
select 5 f1, 22026.4658 f2 from tversion union
select rnum, exp( vnum.cnum ) from vnum
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreExactNumeric_p4
select 'ExpCoreExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, 1.10517092 f2 from tversion union
select 5 f1, 22026.4658 f2 from tversion union
select rnum, exp( tnum.cnum ) from tnum
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreIntegers_p1
select 'ExpCoreIntegers_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, 22026.4658 f2 from tversion union
select vint.rnum, exp( vint.cint ) from vint
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreIntegers_p2
select 'ExpCoreIntegers_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, 22026.4658 f2 from tversion union
select tint.rnum, exp( tint.cint ) from tint
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpSub_p5
select 'ApproximateNumericOpSub_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3.000000000000000e+000 f2 from tversion union
select 2 f1, -2.000000000000000e+000 f2 from tversion union
select 3 f1, -1.000000000000000e+000 f2 from tversion union
select 4 f1, -2.100000000000000e+000 f2 from tversion union
select 5 f1, 8.000000000000000e+000 f2 from tversion union
select vrl.rnum,vrl.crl - 2 from vrl
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreIntegers_p3
select 'ExpCoreIntegers_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, 22026.4658 f2 from tversion union
select vsint.rnum, exp( vsint.csint ) from vsint
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreIntegers_p4
select 'ExpCoreIntegers_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, 22026.4658 f2 from tversion union
select tsint.rnum, exp( tsint.csint ) from tsint
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreIntegers_p5
select 'ExpCoreIntegers_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, 22026.4658 f2 from tversion union
select vbint.rnum, exp( vbint.cbint ) from vbint
) Q
group by
f1,f2
) Q ) P;
--- ExpCoreIntegers_p6
select 'ExpCoreIntegers_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, .367879441 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 2.71828183 f2 from tversion union
select 4 f1, 22026.4658 f2 from tversion union
select tbint.rnum, exp( tbint.cbint ) from tbint
) Q
group by
f1,f2
) Q ) P;
--- ExpressionInIn_p1
select 'ExpressionInIn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.rnum in (1 - 1)
) Q
group by
f1,f2
) Q ) P;
--- ExpressionUsingAggregate_p1
select 'ExpressionUsingAggregate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 51 f1 from tversion union
select (1 + max(c1) - min(c1) ) from tset1
) Q
group by
f1
) Q ) P;
--- ExtractCoreDayFromDate_p1
select 'ExtractCoreDayFromDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 01 f2 from tversion union
select 2 f1, 01 f2 from tversion union
select 3 f1, 31 f2 from tversion union
select vdt.rnum, extract( day from vdt.cdt ) from vdt
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreDayFromDate_p2
select 'ExtractCoreDayFromDate_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 01 f2 from tversion union
select 2 f1, 01 f2 from tversion union
select 3 f1, 31 f2 from tversion union
select tdt.rnum, extract( day from tdt.cdt ) from tdt
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreDayFromTimestamp_p1
select 'ExtractCoreDayFromTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 01 f2 from tversion union
select 2 f1, 01 f2 from tversion union
select 3 f1, 01 f2 from tversion union
select 4 f1, 01 f2 from tversion union
select 5 f1, 01 f2 from tversion union
select 6 f1, 01 f2 from tversion union
select 7 f1, 31 f2 from tversion union
select 8 f1, 31 f2 from tversion union
select 9 f1, 31 f2 from tversion union
select vts.rnum, extract( day from vts.cts ) from vts
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreDayFromTimestamp_p2
select 'ExtractCoreDayFromTimestamp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 01 f2 from tversion union
select 2 f1, 01 f2 from tversion union
select 3 f1, 01 f2 from tversion union
select 4 f1, 01 f2 from tversion union
select 5 f1, 01 f2 from tversion union
select 6 f1, 01 f2 from tversion union
select 7 f1, 31 f2 from tversion union
select 8 f1, 31 f2 from tversion union
select 9 f1, 31 f2 from tversion union
select tts.rnum, extract( day from tts.cts ) from tts
) Q
group by
f1,f2
) Q ) P;
--- ApproximateNumericOpSub_p6
select 'ApproximateNumericOpSub_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3.000000000000000e+000 f2 from tversion union
select 2 f1, -2.000000000000000e+000 f2 from tversion union
select 3 f1, -1.000000000000000e+000 f2 from tversion union
select 4 f1, -2.100000000000000e+000 f2 from tversion union
select 5 f1, 8.000000000000000e+000 f2 from tversion union
select trl.rnum,trl.crl - 2 from trl
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreHourFromTimestamp_p1
select 'ExtractCoreHourFromTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 12 f2 from tversion union
select 3 f1, 23 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 12 f2 from tversion union
select 6 f1, 23 f2 from tversion union
select 7 f1, 0 f2 from tversion union
select 8 f1, 12 f2 from tversion union
select 9 f1, 12 f2 from tversion union
select vts.rnum, extract( hour from vts.cts ) from vts
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreHourFromTimestamp_p2
select 'ExtractCoreHourFromTimestamp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 12 f2 from tversion union
select 3 f1, 23 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 12 f2 from tversion union
select 6 f1, 23 f2 from tversion union
select 7 f1, 0 f2 from tversion union
select 8 f1, 12 f2 from tversion union
select 9 f1, 12 f2 from tversion union
select tts.rnum, extract( hour from tts.cts ) from tts
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreHourFromTime_p1
select 'ExtractCoreHourFromTime_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 12 f2 from tversion union
select 3 f1, 23 f2 from tversion union
select vtm.rnum, extract( hour from vtm.ctm ) from vtm
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreHourFromTime_p2
select 'ExtractCoreHourFromTime_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 12 f2 from tversion union
select 3 f1, 23 f2 from tversion union
select ttm.rnum, extract( hour from ttm.ctm ) from ttm
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreMinuteFromTimestamp_p1
select 'ExtractCoreMinuteFromTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 59 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 0 f2 from tversion union
select 6 f1, 59 f2 from tversion union
select 7 f1, 0 f2 from tversion union
select 8 f1, 0 f2 from tversion union
select 9 f1, 15 f2 from tversion union
select vts.rnum, extract( minute from vts.cts ) from vts
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreMinuteFromTimestamp_p2
select 'ExtractCoreMinuteFromTimestamp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 59 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 0 f2 from tversion union
select 6 f1, 59 f2 from tversion union
select 7 f1, 0 f2 from tversion union
select 8 f1, 0 f2 from tversion union
select 9 f1, 15 f2 from tversion union
select tts.rnum, extract( minute from tts.cts ) from tts
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreMinuteFromTime_p1
select 'ExtractCoreMinuteFromTime_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 59 f2 from tversion union
select vtm.rnum, extract( minute from vtm.ctm ) from vtm
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreMinuteFromTime_p2
select 'ExtractCoreMinuteFromTime_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 59 f2 from tversion union
select ttm.rnum, extract( minute from ttm.ctm ) from ttm
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreMonthFromDate_p1
select 'ExtractCoreMonthFromDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 01 f2 from tversion union
select 2 f1, 01 f2 from tversion union
select 3 f1, 12 f2 from tversion union
select vdt.rnum, extract( month from vdt.cdt ) from vdt
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreMonthFromDate_p2
select 'ExtractCoreMonthFromDate_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 01 f2 from tversion union
select 2 f1, 01 f2 from tversion union
select 3 f1, 12 f2 from tversion union
select tdt.rnum, extract( month from tdt.cdt ) from tdt
) Q
group by
f1,f2
) Q ) P;
--- AvgApproxNumeric_p1
select 'AvgApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(vflt.cflt) from vflt
) Q
group by
f1
) Q ) P;
--- ExtractCoreMonthFromTimestamp_p1
select 'ExtractCoreMonthFromTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 01 f2 from tversion union
select 2 f1, 01 f2 from tversion union
select 3 f1, 01 f2 from tversion union
select 4 f1, 01 f2 from tversion union
select 5 f1, 01 f2 from tversion union
select 6 f1, 01 f2 from tversion union
select 7 f1, 12 f2 from tversion union
select 8 f1, 12 f2 from tversion union
select 9 f1, 12 f2 from tversion union
select vts.rnum, extract( month from vts.cts ) from vts
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreMonthFromTimestamp_p2
select 'ExtractCoreMonthFromTimestamp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 01 f2 from tversion union
select 2 f1, 01 f2 from tversion union
select 3 f1, 01 f2 from tversion union
select 4 f1, 01 f2 from tversion union
select 5 f1, 01 f2 from tversion union
select 6 f1, 01 f2 from tversion union
select 7 f1, 12 f2 from tversion union
select 8 f1, 12 f2 from tversion union
select 9 f1, 12 f2 from tversion union
select tts.rnum, extract( month from tts.cts ) from tts
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreSecondFromTimestamp_p1
select 'ExtractCoreSecondFromTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1,  0000.000000 f2 from tversion union
select 2 f1,  0000.000000 f2 from tversion union
select 3 f1,  0030.123000 f2 from tversion union
select 4 f1,  0000.000000 f2 from tversion union
select 5 f1,  0000.000000 f2 from tversion union
select 6 f1,  0030.123000 f2 from tversion union
select 7 f1,  0000.000000 f2 from tversion union
select 8 f1,  0000.000000 f2 from tversion union
select 9 f1,  0030.123000 f2 from tversion union
select vts.rnum, extract( second from vts.cts ) from vts
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreSecondFromTimestamp_p2
select 'ExtractCoreSecondFromTimestamp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1,  0000.000000 f2 from tversion union
select 2 f1,  0000.000000 f2 from tversion union
select 3 f1,  0030.123000 f2 from tversion union
select 4 f1,  0000.000000 f2 from tversion union
select 5 f1,  0000.000000 f2 from tversion union
select 6 f1,  0030.123000 f2 from tversion union
select 7 f1,  0000.000000 f2 from tversion union
select 8 f1,  0000.000000 f2 from tversion union
select 9 f1,  0030.123000 f2 from tversion union
select tts.rnum, extract( second from tts.cts ) from tts
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreSecondFromTime_p1
select 'ExtractCoreSecondFromTime_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 30.123 f2 from tversion union
select vtm.rnum, extract( second from vtm.ctm ) from vtm
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreSecondFromTime_p2
select 'ExtractCoreSecondFromTime_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 30.123 f2 from tversion union
select ttm.rnum, extract( second from ttm.ctm ) from ttm
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreYearFromDate_p1
select 'ExtractCoreYearFromDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1996 f2 from tversion union
select 2 f1, 2000 f2 from tversion union
select 3 f1, 2000 f2 from tversion union
select vdt.rnum, extract( year from vdt.cdt ) from vdt
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreYearFromDate_p2
select 'ExtractCoreYearFromDate_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1996 f2 from tversion union
select 2 f1, 2000 f2 from tversion union
select 3 f1, 2000 f2 from tversion union
select tdt.rnum, extract( year from tdt.cdt ) from tdt
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreYearFromTimestamp_p1
select 'ExtractCoreYearFromTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1996 f2 from tversion union
select 2 f1, 1996 f2 from tversion union
select 3 f1, 1996 f2 from tversion union
select 4 f1, 2000 f2 from tversion union
select 5 f1, 2000 f2 from tversion union
select 6 f1, 2000 f2 from tversion union
select 7 f1, 2000 f2 from tversion union
select 8 f1, 2000 f2 from tversion union
select 9 f1, 2000 f2 from tversion union
select vts.rnum, extract( year from vts.cts ) from vts
) Q
group by
f1,f2
) Q ) P;
--- ExtractCoreYearFromTimestamp_p2
select 'ExtractCoreYearFromTimestamp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1996 f2 from tversion union
select 2 f1, 1996 f2 from tversion union
select 3 f1, 1996 f2 from tversion union
select 4 f1, 2000 f2 from tversion union
select 5 f1, 2000 f2 from tversion union
select 6 f1, 2000 f2 from tversion union
select 7 f1, 2000 f2 from tversion union
select 8 f1, 2000 f2 from tversion union
select 9 f1, 2000 f2 from tversion union
select tts.rnum, extract( year from tts.cts ) from tts
) Q
group by
f1,f2
) Q ) P;
--- AvgApproxNumeric_p2
select 'AvgApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(tflt.cflt) from tflt
) Q
group by
f1
) Q ) P;
--- FloorCoreApproximateNumeric_p1
select 'FloorCoreApproximateNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, -1 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, floor( vflt.cflt ) from vflt
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreApproximateNumeric_p2
select 'FloorCoreApproximateNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, -1 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, floor( tflt.cflt ) from tflt
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreApproximateNumeric_p3
select 'FloorCoreApproximateNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, -1 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, floor( vdbl.cdbl ) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreApproximateNumeric_p4
select 'FloorCoreApproximateNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, -1 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, floor( tdbl.cdbl ) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreApproximateNumeric_p5
select 'FloorCoreApproximateNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, -1 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, floor( vrl.crl ) from vrl
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreApproximateNumeric_p6
select 'FloorCoreApproximateNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, -1 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, floor( trl.crl ) from trl
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreExactNumeric_p1
select 'FloorCoreExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, floor( vdec.cdec ) from vdec
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreExactNumeric_p2
select 'FloorCoreExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, floor( tdec.cdec ) from tdec
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreExactNumeric_p3
select 'FloorCoreExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, floor( vnum.cnum ) from vnum
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreExactNumeric_p4
select 'FloorCoreExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 10 f2 from tversion union
select rnum, floor( tnum.cnum ) from tnum
) Q
group by
f1,f2
) Q ) P;
--- AvgApproxNumeric_p3
select 'AvgApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(vdbl.cdbl) from vdbl
) Q
group by
f1
) Q ) P;
--- FloorCoreIntegers_p1
select 'FloorCoreIntegers_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, floor( vint.cint ) from vint
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreIntegers_p2
select 'FloorCoreIntegers_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, floor( tint.cint ) from tint
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreIntegers_p3
select 'FloorCoreIntegers_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, floor( vsint.csint ) from vsint
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreIntegers_p4
select 'FloorCoreIntegers_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, floor( tsint.csint ) from tsint
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreIntegers_p5
select 'FloorCoreIntegers_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, floor( vbint.cbint ) from vbint
) Q
group by
f1,f2
) Q ) P;
--- FloorCoreIntegers_p6
select 'FloorCoreIntegers_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 10 f2 from tversion union
select rnum, floor( tbint.cbint ) from tbint
) Q
group by
f1,f2
) Q ) P;
--- GroupByAlias_p1
select 'GroupByAlias_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 50 f1, 'AAA' f2 from tversion union
select 100 f1, 'BBB' f2 from tversion union
select 150 f1, 'CCC' f2 from tversion union
select 200 f1, 'DDD' f2 from tversion union
select 250 f1, null f2 from tversion union
select 300 f1, null f2 from tversion union
select null f1, 'AAA' f2 from tversion union
select null f1, null f2 from tversion union
select c1*5 as calc, c2 from tset1 group by calc, c2
) Q
group by
f1,f2
) Q ) P;
--- GroupByExpr_p1
select 'GroupByExpr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 50 f1, 'AAA' f2 from tversion union
select 100 f1, 'BBB' f2 from tversion union
select 150 f1, 'CCC' f2 from tversion union
select 200 f1, 'DDD' f2 from tversion union
select 250 f1, null f2 from tversion union
select 300 f1, null f2 from tversion union
select null f1, 'AAA' f2 from tversion union
select null f1, null f2 from tversion union
select c1*5, c2 from tset1 group by c1*5, c2
) Q
group by
f1,f2
) Q ) P;
--- GroupByHaving_p1
select 'GroupByHaving_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 3 f2 from tversion union
select null f1, 4 f2 from tversion union
select c1, count(*) from tset1 group by c1 having count(*) > 2
) Q
group by
f1,f2
) Q ) P;
--- GroupByLiteral_p1
select 'GroupByLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select 10 f1 from tversion union
select 10 f1 from tversion union
select 10 f1 from tversion union
select 10 f1 from tversion union
select 10 f1 from tversion union
select 10 f1 from tversion union
select 10 from tset1 group by tset1.c1
) Q
group by
f1
) Q ) P;
--- AbsCoreApproximateNumeric_p5
select 'AbsCoreApproximateNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 1.000000000000000e+000 f2 from tversion union
select 4 f1, 1.000000000000000e-001 f2 from tversion union
select 5 f1, 1.000000000000000e+001 f2 from tversion union
select rnum, abs( vrl.crl ) from vrl
) Q
group by
f1,f2
) Q ) P;
--- AvgApproxNumeric_p4
select 'AvgApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(tdbl.cdbl) from tdbl
) Q
group by
f1
) Q ) P;
--- GroupByMany_p1
select 'GroupByMany_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'AAA' f2 from tversion union
select 20 f1, 'BBB' f2 from tversion union
select 30 f1, 'CCC' f2 from tversion union
select 40 f1, 'DDD' f2 from tversion union
select 50 f1, null f2 from tversion union
select 60 f1, null f2 from tversion union
select null f1, 'AAA' f2 from tversion union
select null f1, null f2 from tversion union
select c1, c2 from tset1 group by c1, c2
) Q
group by
f1,f2
) Q ) P;
--- GroupByMultiply_p1
select 'GroupByMultiply_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 50 f1 from tversion union
select 100 f1 from tversion union
select 150 f1 from tversion union
select 200 f1 from tversion union
select 250 f1 from tversion union
select 300 f1 from tversion union
select null f1 from tversion union
select c1 * 5 from tset1 group by c1
) Q
group by
f1
) Q ) P;
--- GroupByOrdinal_p1
select 'GroupByOrdinal_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 50 f1, 'AAA' f2 from tversion union
select 100 f1, 'BBB' f2 from tversion union
select 150 f1, 'CCC' f2 from tversion union
select 200 f1, 'DDD' f2 from tversion union
select 250 f1, null f2 from tversion union
select 300 f1, null f2 from tversion union
select null f1, 'AAA' f2 from tversion union
select null f1, null f2 from tversion union
select c1*5, c2 from tset1 group by 1,2
) Q
group by
f1,f2
) Q ) P;
--- GroupBy_p1
select 'GroupBy_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'AAA' f1 from tversion union
select 'BBB' f1 from tversion union
select 'CCC' f1 from tversion union
select 'DDD' f1 from tversion union
select null f1 from tversion union
select c2 from tset1 group by c2
) Q
group by
f1
) Q ) P;
--- Having_p1
select 'Having_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 8 f1 from tversion union
select count(c1) from tset1 having count(*) > 2
) Q
group by
f1
) Q ) P;
--- IntegerOpAdd_p1
select 'IntegerOpAdd_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 2 f2 from tversion union
select 3 f1, 3 f2 from tversion union
select 4 f1, 12 f2 from tversion union
select vint.rnum,vint.cint + 2 from vint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpAdd_p2
select 'IntegerOpAdd_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 2 f2 from tversion union
select 3 f1, 3 f2 from tversion union
select 4 f1, 12 f2 from tversion union
select tint.rnum,tint.cint + 2 from tint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpAdd_p3
select 'IntegerOpAdd_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 2 f2 from tversion union
select 3 f1, 3 f2 from tversion union
select 4 f1, 12 f2 from tversion union
select vsint.rnum,vsint.csint + 2 from vsint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpAdd_p4
select 'IntegerOpAdd_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 2 f2 from tversion union
select 3 f1, 3 f2 from tversion union
select 4 f1, 12 f2 from tversion union
select tsint.rnum,tsint.csint + 2 from tsint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpAdd_p5
select 'IntegerOpAdd_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 2 f2 from tversion union
select 3 f1, 3 f2 from tversion union
select 4 f1, 12 f2 from tversion union
select vbint.rnum,vbint.cbint + 2 from vbint
) Q
group by
f1,f2
) Q ) P;
--- AvgApproxNumeric_p5
select 'AvgApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(vrl.crl) from vrl
) Q
group by
f1
) Q ) P;
--- IntegerOpAdd_p6
select 'IntegerOpAdd_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 2 f2 from tversion union
select 3 f1, 3 f2 from tversion union
select 4 f1, 12 f2 from tversion union
select tbint.rnum,tbint.cbint + 2 from tbint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpDiv_p1
select 'IntegerOpDiv_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 5 f2 from tversion union
select vint.rnum,vint.cint / 2 from vint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpDiv_p2
select 'IntegerOpDiv_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 5 f2 from tversion union
select tint.rnum,tint.cint / 2 from tint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpDiv_p3
select 'IntegerOpDiv_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 5 f2 from tversion union
select vsint.rnum,vsint.csint / 2 from vsint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpDiv_p4
select 'IntegerOpDiv_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 5 f2 from tversion union
select tsint.rnum,tsint.csint / 2 from tsint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpDiv_p5
select 'IntegerOpDiv_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 5 f2 from tversion union
select vbint.rnum,vbint.cbint / 2 from vbint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpDiv_p6
select 'IntegerOpDiv_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 5 f2 from tversion union
select tbint.rnum,tbint.cbint / 2 from tbint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpMulNULL_p1
select 'IntegerOpMulNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select -1.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- IntegerOpMulNULL_p2
select 'IntegerOpMulNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 0.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- IntegerOpMulNULL_p3
select 'IntegerOpMulNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 1.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- AvgApproxNumeric_p6
select 'AvgApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(trl.crl) from trl
) Q
group by
f1
) Q ) P;
--- IntegerOpMulNULL_p4
select 'IntegerOpMulNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select -1.0e-1 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- IntegerOpMulNULL_p5
select 'IntegerOpMulNULL_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 10.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
--- IntegerOpMul_p1
select 'IntegerOpMul_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 2 f2 from tversion union
select 4 f1, 20 f2 from tversion union
select vint.rnum,vint.cint * 2 from vint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpMul_p2
select 'IntegerOpMul_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 2 f2 from tversion union
select 4 f1, 20 f2 from tversion union
select tint.rnum,tint.cint * 2 from tint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpMul_p3
select 'IntegerOpMul_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 2 f2 from tversion union
select 4 f1, 20 f2 from tversion union
select vsint.rnum,vsint.csint * 2 from vsint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpMul_p4
select 'IntegerOpMul_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 2 f2 from tversion union
select 4 f1, 20 f2 from tversion union
select tsint.rnum,tsint.csint * 2 from tsint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpMul_p5
select 'IntegerOpMul_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 2 f2 from tversion union
select 4 f1, 20 f2 from tversion union
select vbint.rnum,vbint.cbint * 2 from vbint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpMul_p6
select 'IntegerOpMul_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -2 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 2 f2 from tversion union
select 4 f1, 20 f2 from tversion union
select tbint.rnum,tbint.cbint * 2 from tbint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpSub_p1
select 'IntegerOpSub_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3 f2 from tversion union
select 2 f1, -2 f2 from tversion union
select 3 f1, -1 f2 from tversion union
select 4 f1, 8 f2 from tversion union
select vint.rnum,vint.cint - 2 from vint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpSub_p2
select 'IntegerOpSub_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3 f2 from tversion union
select 2 f1, -2 f2 from tversion union
select 3 f1, -1 f2 from tversion union
select 4 f1, 8 f2 from tversion union
select tint.rnum,tint.cint - 2 from tint
) Q
group by
f1,f2
) Q ) P;
--- AvgExactNumeric_p1
select 'AvgExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.02 f1 from tversion union
select avg(vdec.cdec) from vdec
) Q
group by
f1
) Q ) P;
--- IntegerOpSub_p3
select 'IntegerOpSub_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3 f2 from tversion union
select 2 f1, -2 f2 from tversion union
select 3 f1, -1 f2 from tversion union
select 4 f1, 8 f2 from tversion union
select vsint.rnum,vsint.csint - 2 from vsint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpSub_p4
select 'IntegerOpSub_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3 f2 from tversion union
select 2 f1, -2 f2 from tversion union
select 3 f1, -1 f2 from tversion union
select 4 f1, 8 f2 from tversion union
select tsint.rnum,tsint.csint - 2 from tsint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpSub_p5
select 'IntegerOpSub_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3 f2 from tversion union
select 2 f1, -2 f2 from tversion union
select 3 f1, -1 f2 from tversion union
select 4 f1, 8 f2 from tversion union
select vbint.rnum,vbint.cbint - 2 from vbint
) Q
group by
f1,f2
) Q ) P;
--- IntegerOpSub_p6
select 'IntegerOpSub_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -3 f2 from tversion union
select 2 f1, -2 f2 from tversion union
select 3 f1, -1 f2 from tversion union
select 4 f1, 8 f2 from tversion union
select tbint.rnum,tbint.cbint - 2 from tbint
) Q
group by
f1,f2
) Q ) P;
--- IntersectAll_p1
select 'IntersectAll_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'AAA' f2 from tversion union
select 10 f1, 'AAA' f2 from tversion union
select 40 f1, 'DDD' f2 from tversion union
select c1, c2 from tset1 intersect all select c1, c2 from tset2
) Q
group by
f1,f2
) Q ) P;
--- Intersect_p1
select 'Intersect_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'AAA' f2 from tversion union
select 40 f1, 'DDD' f2 from tversion union
select c1, c2 from tset1 intersect select c1, c2 from tset2
) Q
group by
f1,f2
) Q ) P;
--- IsNullPredicate_p1
select 'IsNullPredicate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select null f1, 'EE' f2 from tversion union
select c1, c2 from tjoin2 where c1 is null
) Q
group by
f1,f2
) Q ) P;
--- IsNullValueExpr_p1
select 'IsNullValueExpr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select c1 from tversion where 1 * cnnull is null
) Q
group by
f1
) Q ) P;
--- JoinCoreCrossProduct_p1
select 'JoinCoreCrossProduct_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 0 f2, 10 f3, 10 f4, 'BB' f5 from tversion union
select 0 f1, 1 f2, 10 f3, 15 f4, 'DD' f5 from tversion union
select 0 f1, 2 f2, 10 f3, null f4, 'EE' f5 from tversion union
select 0 f1, 3 f2, 10 f3, 10 f4, 'FF' f5 from tversion union
select 1 f1, 0 f2, 20 f3, 10 f4, 'BB' f5 from tversion union
select 1 f1, 1 f2, 20 f3, 15 f4, 'DD' f5 from tversion union
select 1 f1, 2 f2, 20 f3, null f4, 'EE' f5 from tversion union
select 1 f1, 3 f2, 20 f3, 10 f4, 'FF' f5 from tversion union
select 2 f1, 0 f2, null f3, 10 f4, 'BB' f5 from tversion union
select 2 f1, 1 f2, null f3, 15 f4, 'DD' f5 from tversion union
select 2 f1, 2 f2, null f3, null f4, 'EE' f5 from tversion union
select 2 f1, 3 f2, null f3, 10 f4, 'FF' f5 from tversion union
select tjoin1.rnum, tjoin2.rnum, tjoin1.c1, tjoin2.c1 as c1j2, tjoin2.c2 from tjoin1, tjoin2
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- JoinCoreCross_p1
select 'JoinCoreCross_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 10 f3 from tversion union
select 3 f1, 10 f2, 10 f3 from tversion union
select tjoin2.rnum, tjoin1.c1, tjoin2.c1 as c1j2 from tjoin1 cross join tjoin2 where tjoin1.c1=tjoin2.c1
) Q
group by
f1,f2,f3
) Q ) P;
--- AvgExactNumeric_p2
select 'AvgExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.02 f1 from tversion union
select avg(tdec.cdec) from tdec
) Q
group by
f1
) Q ) P;
--- JoinCoreEqualWithAnd_p1
select 'JoinCoreEqualWithAnd_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin1.c1, tjoin2.c2 from tjoin1 inner join tjoin2 on ( tjoin1.c1 = tjoin2.c1 and tjoin2.c2='BB' )
) Q
group by
f1,f2
) Q ) P;
--- JoinCoreImplicit_p1
select 'JoinCoreImplicit_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 0 f1, 10 f2, 10 f3, 'BB' f4 from tversion union
select 3 f1, 10 f2, 10 f3, 'FF' f4 from tversion union
select tjoin2.rnum, tjoin1.c1, tjoin2.c1 as c1j2, tjoin2.c2 from tjoin1, tjoin2 where tjoin1.c1=tjoin2.c1
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- JoinCoreIsNullPredicate_p1
select 'JoinCoreIsNullPredicate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 0 f1, 10 f2, 15 f3, null f4 from tversion union
select 1 f1, 20 f2, 25 f3, null f4 from tversion union
select 2 f1, null f2, 50 f3, null f4 from tversion union
select tjoin1.rnum, tjoin1.c1, tjoin1.c2, tjoin2.c2  as c2j2 from tjoin1 left outer join tjoin2 on ( tjoin1.c1 = tjoin2.c1 and tjoin1.c2 is null )
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- JoinCoreLeftNestedInnerTableRestrict_p1
select 'JoinCoreLeftNestedInnerTableRestrict_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 0 f2, 0 f3 from tversion union
select 0 f1, 3 f2, 0 f3 from tversion union
select 1 f1, null f2, null f3 from tversion union
select 2 f1, null f2, null f3 from tversion union
select tjoin1.rnum, tjoin2.rnum as rnumt2, tjoin3.rnum as rnumt3 from  (tjoin1 left outer join tjoin2 on tjoin1.c1=tjoin2.c1) left outer join tjoin3 on tjoin2.c1=tjoin3.c1
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreLeftNestedOptionalTableRestrict_p1
select 'JoinCoreLeftNestedOptionalTableRestrict_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 10 f1, 15 f2, 'BB' f3, 'XX' f4 from tversion union
select 10 f1, 15 f2, 'FF' f3, 'XX' f4 from tversion union
select 20 f1, 25 f2, null f3, null f4 from tversion union
select null f1, 50 f2, null f3, null f4 from tversion union
select tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2, tjoin3.c2 as c2j3 from  tjoin1 left outer join (tjoin2 left outer join tjoin3 on tjoin2.c1=tjoin3.c1) on tjoin1.c1=tjoin3.c1
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- JoinCoreNatural_p1
select 'JoinCoreNatural_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 0 f2 from tversion union
select 1 f1, 1 f2 from tversion union
select tset1.rnum, tset2.rnum as rnumt2 from tset1 natural join tset2
) Q
group by
f1,f2
) Q ) P;
--- JoinCoreNestedInnerOuter_p1
select 'JoinCoreNestedInnerOuter_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5,f6, count(*) c  from (
select 0 f1, 10 f2, 15 f3, 'BB' f4, 'XX' f5, null f6 from tversion union
select 3 f1, 10 f2, 15 f3, 'FF' f4, 'XX' f5, null f6 from tversion union
select tjoin2.rnum, tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2, tjoin3.c2 as c2j3,tjoin4.c2 as c2j4 from (tjoin1 inner join (tjoin2 left outer join tjoin3 on tjoin2.c1=tjoin3.c1) on (tjoin1.c1=tjoin2.c1)) left outer join tjoin4 on (tjoin1.c1=tjoin4.c1)
) Q
group by
f1,f2,f3,f4,f5,f6
) Q ) P;
--- JoinCoreNestedOuterInnerTableRestrict_p1
select 'JoinCoreNestedOuterInnerTableRestrict_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 0 f2, 0 f3 from tversion union
select 0 f1, 3 f2, 0 f3 from tversion union
select null f1, 1 f2, null f3 from tversion union
select null f1, 2 f2, null f3 from tversion union
select tjoin1.rnum, tjoin2.rnum as rnumt2, tjoin3.rnum as rnumt3 from  (tjoin1 right outer join tjoin2 on tjoin1.c1=tjoin2.c1) left outer join tjoin3 on tjoin1.c1=tjoin3.c1
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreNestedOuterOptionalTableRestrict_p1
select 'JoinCoreNestedOuterOptionalTableRestrict_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 10 f1, 15 f2, 'BB' f3, 'XX' f4 from tversion union
select 10 f1, 15 f2, 'FF' f3, 'XX' f4 from tversion union
select null f1, null f2, null f3, 'YY' f4 from tversion union
select tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2, tjoin3.c2 as c2j3 from  (tjoin1 left outer join tjoin2 on tjoin1.c1=tjoin2.c1) right outer join tjoin3 on tjoin2.c1=tjoin3.c1
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- JoinCoreNestedOuter_p1
select 'JoinCoreNestedOuter_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5,f6, count(*) c  from (
select 0 f1, 10 f2, 15 f3, 'BB' f4, 'XX' f5, null f6 from tversion union
select 1 f1, null f2, null f3, 'DD' f4, 'YY' f5, null f6 from tversion union
select 2 f1, null f2, null f3, 'EE' f4, null f5, null f6 from tversion union
select 3 f1, 10 f2, 15 f3, 'FF' f4, 'XX' f5, null f6 from tversion union
select tjoin2.rnum, tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2, tjoin3.c2 as c2j3,tjoin4.c2 as c2j4 from  (tjoin1 right outer join (tjoin2 left outer join tjoin3 on tjoin2.c1=tjoin3.c1) on (tjoin1.c1=tjoin2.c1)) left outer join tjoin4 on (tjoin1.c1=tjoin4.c1)
) Q
group by
f1,f2,f3,f4,f5,f6
) Q ) P;
--- AvgExactNumeric_p3
select 'AvgExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.02 f1 from tversion union
select avg(vnum.cnum) from vnum
) Q
group by
f1
) Q ) P;
--- JoinCoreNoExpressionInOnCondition_p1
select 'JoinCoreNoExpressionInOnCondition_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 10 f2, 15 f3, 10 f4, 'XX' f5 from tversion union
select 0 f1, 10 f2, 15 f3, 15 f4, 'YY' f5 from tversion union
select tjoin1.rnum, tjoin1.c1,tjoin1.c2,tjoin3.c1, tjoin3.c2 from tjoin1 inner join tjoin3 on tjoin1.c1 = 9 + 1
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- JoinCoreNonEquiJoin_p1
select 'JoinCoreNonEquiJoin_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 0 f2, 0 f3 from tversion union
select 1 f1, 1 f2, 1 f3 from tversion union
select 2 f1, 2 f2, null f3 from tversion union
select 3 f1, 3 f2, 0 f3 from tversion union
select tjoin2.rnum, tjoin2.rnum as rnumt2, tjoin3.rnum as rnumt3  from tjoin2 left outer join tjoin3 on tjoin2.c2 <> tjoin3.c2 and tjoin2.c1=tjoin3.c1
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreNonEquiJoin_p2
select 'JoinCoreNonEquiJoin_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 0 f2, 0 f3 from tversion union
select 1 f1, 1 f2, 1 f3 from tversion union
select 2 f1, 2 f2, null f3 from tversion union
select 3 f1, 3 f2, 0 f3 from tversion union
select tjoin2.rnum, tjoin2.rnum as rnumt2, tjoin3.rnum as rnumt3  from tjoin2 left outer join tjoin3 on tjoin2.c2 <> tjoin3.c2 and tjoin2.c1=tjoin3.c1
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreNonJoinNonEquiJoin_p1
select 'JoinCoreNonJoinNonEquiJoin_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 0 f1, 10 f2, 'DD' f3 from tversion union
select 0 f1, 10 f2, 'EE' f3 from tversion union
select 0 f1, 10 f2, 'FF' f3 from tversion union
select 1 f1, 20 f2, null f3 from tversion union
select 2 f1, null f2, null f3 from tversion union
select tjoin1.rnum, tjoin1.c1,tjoin2.c2 from tjoin1 left outer join tjoin2 on tjoin1.c1 < 20
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreNotPredicate_p1
select 'JoinCoreNotPredicate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 0 f1, 10 f2, 15 f3, 'BB' f4 from tversion union
select 3 f1, 10 f2, 15 f3, 'FF' f4 from tversion union
select tjoin2.rnum, tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2 from tjoin1 inner join tjoin2 on ( tjoin1.c1 = tjoin2.c1 and not tjoin2.c2 = 'AA' )
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- JoinCoreNwayJoinedTable_p1
select 'JoinCoreNwayJoinedTable_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3, 'XX' f4 from tversion union
select 1 f1, null f2, 'DD' f3, null f4 from tversion union
select 2 f1, null f2, 'EE' f3, null f4 from tversion union
select 3 f1, 10 f2, 'FF' f3, 'XX' f4 from tversion union
select tjoin2.rnum, tjoin1.c1, tjoin2.c2 as c2j2, tjoin3.c2 as c2j3 from tjoin1 right outer join  tjoin2 on tjoin1.c1 = tjoin2.c1 left outer join tjoin3 on tjoin1.c1 = tjoin3.c1
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- JoinCoreOnConditionAbsFunction_p1
select 'JoinCoreOnConditionAbsFunction_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 0 f1, 10 f2, 'FF' f3 from tversion union
select 1 f1, 20 f2, null f3 from tversion union
select 2 f1, null f2, null f3 from tversion union
select tjoin1.rnum, tjoin1.c1,tjoin2.c2 from tjoin1 left outer join tjoin2 on abs(tjoin1.c1)=tjoin2.c1
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreOnConditionSetFunction_p1
select 'JoinCoreOnConditionSetFunction_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, null f2 from tversion union
select 20 f1, null f2 from tversion union
select null f1, null f2 from tversion union
select tjoin1.c1,tjoin2.c2 from tjoin1 left outer join tjoin2 on tjoin1.c1=tjoin2.c1 and tjoin1.c1 >= ((select sum(c1) from tjoin1)/2)
) Q
group by
f1,f2
) Q ) P;
--- JoinCoreOnConditionSubstringFunction_p1
select 'JoinCoreOnConditionSubstringFunction_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 20 f2, null f3 from tversion union
select 2 f1, null f2, null f3 from tversion union
select tjoin1.rnum, tjoin1.c1,tjoin2.c2 from tjoin1 left outer join tjoin2 on tjoin1.c1=tjoin2.c1 and substring(tjoin2.c2 from 1 for 2)='BB'
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreOnConditionTrimFunction_p1
select 'JoinCoreOnConditionTrimFunction_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 20 f2, null f3 from tversion union
select 2 f1, null f2, null f3 from tversion union
select tjoin1.rnum, tjoin1.c1,tjoin2.c2 from tjoin1 left outer join tjoin2 on tjoin1.c1=tjoin2.c1 and trim(tjoin2.c2)='BB'
) Q
group by
f1,f2,f3
) Q ) P;
--- AvgExactNumeric_p4
select 'AvgExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.02 f1 from tversion union
select avg(tnum.cnum) from tnum
) Q
group by
f1
) Q ) P;
--- JoinCoreOnConditionUpperFunction_p1
select 'JoinCoreOnConditionUpperFunction_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 20 f2, null f3 from tversion union
select 2 f1, null f2, null f3 from tversion union
select tjoin1.rnum, tjoin1.c1,tjoin2.c2 from tjoin1 left outer join tjoin2 on tjoin1.c1=tjoin2.c1 and upper(tjoin2.c2)='BB'
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreOptionalTableFilter_p1
select 'JoinCoreOptionalTableFilter_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'FF' f2 from tversion union
select tjoin1.c1, tjoin2.c2 from tjoin1 left outer join tjoin2 on tjoin1.c1 = tjoin2.c1 where tjoin2.c2 > 'C'
) Q
group by
f1,f2
) Q ) P;
--- JoinCoreOptionalTableJoinFilter_p1
select 'JoinCoreOptionalTableJoinFilter_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'FF' f3 from tversion union
select 1 f1, 20 f2, null f3 from tversion union
select 2 f1, null f2, null f3 from tversion union
select tjoin1.rnum, tjoin1.c1, tjoin2.c2 from tjoin1 left outer join tjoin2 on ( tjoin1.c1 = tjoin2.c1 and tjoin2.c2 > 'C' )
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreOptionalTableJoinRestrict_p1
select 'JoinCoreOptionalTableJoinRestrict_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5,f6,f7,f8,f9, count(*) c  from (
select 0 f1, 10 f2, 15 f3, 0 f4, 10 f5, 'BB' f6, 0 f7, 10 f8, 'XX' f9 from tversion union
select 0 f1, 10 f2, 15 f3, 3 f4, 10 f5, 'FF' f6, 0 f7, 10 f8, 'XX' f9 from tversion union
select tjoin1.rnum as tj1rnum, tjoin1.c1 as tj1c1, tjoin1.c2 as tj1c2, tjoin2.rnum as tj2rnum, tjoin2.c1 as tj2c1, tjoin2.c2 as tj2c2, tjoin3.rnum as tj3rnum, tjoin3.c1 as tj3c1, tjoin3.c2 as tj3c2 from  (tjoin1 left outer join tjoin2 on tjoin1.c1=tjoin2.c1)  , tjoin3 where tjoin2.c1=tjoin3.c1
) Q
group by
f1,f2,f3,f4,f5,f6,f7,f8,f9
) Q ) P;
--- JoinCoreOrPredicate_p1
select 'JoinCoreOrPredicate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 10 f1, 15 f2, 'BB' f3 from tversion union
select 10 f1, 15 f2, 'DD' f3 from tversion union
select 10 f1, 15 f2, 'EE' f3 from tversion union
select 10 f1, 15 f2, 'FF' f3 from tversion union
select 20 f1, 25 f2, 'BB' f3 from tversion union
select 20 f1, 25 f2, 'DD' f3 from tversion union
select 20 f1, 25 f2, 'EE' f3 from tversion union
select 20 f1, 25 f2, 'FF' f3 from tversion union
select tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2 from tjoin1 inner join tjoin2 on ( tjoin1.c1 = 10 or tjoin1.c1=20 )
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreOrPredicate_p2
select 'JoinCoreOrPredicate_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 10 f1, 15 f2, 'BB' f3 from tversion union
select 10 f1, 15 f2, 'DD' f3 from tversion union
select 10 f1, 15 f2, 'EE' f3 from tversion union
select 10 f1, 15 f2, 'FF' f3 from tversion union
select 20 f1, 25 f2, 'BB' f3 from tversion union
select 20 f1, 25 f2, 'DD' f3 from tversion union
select 20 f1, 25 f2, 'EE' f3 from tversion union
select 20 f1, 25 f2, 'FF' f3 from tversion union
select tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2 from tjoin1 inner join tjoin2 on ( tjoin1.c1 = 10 or tjoin1.c1=20 )
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCorePreservedTableFilter_p1
select 'JoinCorePreservedTableFilter_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 1 f1, 20 f2, 25 f3, null f4 from tversion union
select 2 f1, null f2, 50 f3, null f4 from tversion union
select tjoin1.rnum, tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2 from tjoin1 left outer join tjoin2 on tjoin1.c1 = tjoin2.c1 where tjoin1.c2 > 15
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- JoinCorePreservedTableJoinFilter_p1
select 'JoinCorePreservedTableJoinFilter_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 0 f1, 10 f2, 15 f3, null f4 from tversion union
select 1 f1, 20 f2, 25 f3, null f4 from tversion union
select 2 f1, null f2, 50 f3, null f4 from tversion union
select tjoin1.rnum, tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2 from tjoin1 left outer join tjoin2 on ( tjoin1.c1 = tjoin2.c1 and tjoin1.c2 > 15 )
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- JoinCoreRightNestedInnerTableRestrict_p1
select 'JoinCoreRightNestedInnerTableRestrict_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 0 f2, 0 f3 from tversion union
select 0 f1, 3 f2, 0 f3 from tversion union
select null f1, 1 f2, 1 f3 from tversion union
select tjoin1.rnum, tjoin2.rnum as rnumt2, tjoin3.rnum as rnumt3 from  (tjoin1 right outer join tjoin2 on tjoin1.c1=tjoin2.c1) right outer join tjoin3 on tjoin2.c1=tjoin3.c1
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreRightNestedOptionalTableRestrict_p1
select 'JoinCoreRightNestedOptionalTableRestrict_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 10 f1, 15 f2, 'BB' f3, 'XX' f4 from tversion union
select 10 f1, 15 f2, 'FF' f3, 'XX' f4 from tversion union
select null f1, null f2, null f3, 'YY' f4 from tversion union
select tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2, tjoin3.c2 as c2j3 from  (tjoin1 right outer join tjoin2 on tjoin1.c1=tjoin2.c1) right outer join tjoin3 on tjoin1.c1=tjoin3.c1
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- AvgIntTruncates_p1
select 'AvgIntTruncates_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(vint.cint) from vint
) Q
group by
f1
) Q ) P;
--- JoinCoreSelf_p1
select 'JoinCoreSelf_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 10 f3 from tversion union
select 1 f1, 20 f2, 20 f3 from tversion union
select tjoin1.rnum, tjoin1.c1, tjoin1a.c1 from tjoin1, tjoin1 tjoin1a where tjoin1.c1=tjoin1a.c1
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreSimpleAndJoinedTable_p1
select 'JoinCoreSimpleAndJoinedTable_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 0 f1, 10 f2, 15 f3, 'BB' f4 from tversion union
select 3 f1, 10 f2, 15 f3, 'FF' f4 from tversion union
select tjoin2.rnum, tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2 from tjoin1 left outer join tjoin2 on ( tjoin1.c1 = tjoin2.c1 ), tjoin3 where tjoin3.c1 = tjoin1.c1
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- JoinCoreTwoSidedJoinRestrictionFilter_p1
select 'JoinCoreTwoSidedJoinRestrictionFilter_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 0 f2, 0 f3 from tversion union
select 0 f1, 3 f2, 0 f3 from tversion union
select tjoin1.rnum, tjoin2.rnum as rnumt2, tjoin3.rnum as rnumt3 from  (tjoin1 left outer join tjoin2 on tjoin1.c1=tjoin2.c1) left outer join tjoin3 on tjoin1.c1=tjoin3.c1 where tjoin2.c1=tjoin3.c1
) Q
group by
f1,f2,f3
) Q ) P;
--- JoinCoreTwoSidedJoinRestrict_p1
select 'JoinCoreTwoSidedJoinRestrict_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5,f6,f7,f8,f9, count(*) c  from (
select 0 f1, 10 f2, 15 f3, 0 f4, 10 f5, 'BB' f6, 0 f7, 10 f8, 'XX' f9 from tversion union
select 0 f1, 10 f2, 15 f3, 3 f4, 10 f5, 'FF' f6, 0 f7, 10 f8, 'XX' f9 from tversion union
select 1 f1, 20 f2, 25 f3, null f4, null f5, null f6, null f7, null f8, null f9 from tversion union
select 2 f1, null f2, 50 f3, null f4, null f5, null f6, null f7, null f8, null f9 from tversion union
select tjoin1.rnum as tj1rnum, tjoin1.c1 as tj1c1, tjoin1.c2 as tj1c2, tjoin2.rnum as tj2rnum, tjoin2.c1 as tj2c1, tjoin2.c2 as tj2c2, tjoin3.rnum as tj3rnum, tjoin3.c1 as tj3c1, tjoin3.c2 as tj3c2 from (tjoin1 left outer join tjoin2 on tjoin1.c1=tjoin2.c1) left outer join tjoin3 on tjoin2.c1=tjoin3.c1
) Q
group by
f1,f2,f3,f4,f5,f6,f7,f8,f9
) Q ) P;
--- JoinCoreUsing_p1
select 'JoinCoreUsing_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 10 f3 from tversion union
select 3 f1, 10 f2, 10 f3 from tversion union
select tjoin2.rnum, tjoin1.c1, tjoin2.c1 as c1j2 from tjoin1 join tjoin2 using ( c1 )
) Q
group by
f1,f2,f3
) Q ) P;
--- LikeValueExpr_p1
select 'LikeValueExpr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 like upper('BB')
) Q
group by
f1,f2
) Q ) P;
--- LnCoreNull_p1
select 'LnCoreNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select ln( null ) from tversion
) Q
group by
f1
) Q ) P;
--- LnCore_p1
select 'LnCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.302585092994050e+000 f1 from tversion union
select ln( 10 ) from tversion
) Q
group by
f1
) Q ) P;
--- LnCore_p2
select 'LnCore_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.302585092994050e+000 f1 from tversion union
select ln( 10.0e+0 ) from tversion
) Q
group by
f1
) Q ) P;
--- LnCore_p3
select 'LnCore_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.302585092994050e+000 f1 from tversion union
select ln( 10.0 ) from tversion
) Q
group by
f1
) Q ) P;
--- AvgIntTruncates_p2
select 'AvgIntTruncates_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(tint.cint) from tint
) Q
group by
f1
) Q ) P;
--- LowerCoreFixedLength_p1
select 'LowerCoreFixedLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '                                ' f2 from tversion union
select 2 f1, '                                ' f2 from tversion union
select 3 f1, 'bb                              ' f2 from tversion union
select 4 f1, 'ee                              ' f2 from tversion union
select 5 f1, 'ff                              ' f2 from tversion union
select rnum, lower( vchar.cchar ) from vchar
) Q
group by
f1,f2
) Q ) P;
--- LowerCoreFixedLength_p2
select 'LowerCoreFixedLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '                                ' f2 from tversion union
select 2 f1, '                                ' f2 from tversion union
select 3 f1, 'bb                              ' f2 from tversion union
select 4 f1, 'ee                              ' f2 from tversion union
select 5 f1, 'ff                              ' f2 from tversion union
select rnum, lower( tchar.cchar ) from tchar
) Q
group by
f1,f2
) Q ) P;
--- LowerCoreSpecial_p1
select 'LowerCoreSpecial_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'ß' f1 from tversion union
select lower( 'ß' ) from tversion
) Q
group by
f1
) Q ) P;
--- LowerCoreVariableLength_p1
select 'LowerCoreVariableLength_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'bb' f2 from tversion union
select 4 f1, 'ee' f2 from tversion union
select 5 f1, 'ff' f2 from tversion union
select rnum, lower( vvchar.cvchar ) from vvchar
) Q
group by
f1,f2
) Q ) P;
--- LowerCoreVariableLength_p2
select 'LowerCoreVariableLength_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, '' f2 from tversion union
select 2 f1, ' ' f2 from tversion union
select 3 f1, 'bb' f2 from tversion union
select 4 f1, 'ee' f2 from tversion union
select 5 f1, 'ff' f2 from tversion union
select rnum, lower( tvchar.cvchar ) from tvchar
) Q
group by
f1,f2
) Q ) P;
--- MaxLiteralTemp_p1
select 'MaxLiteralTemp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '2000-01-01' f1 from tversion union
select max( '2000-01-01' ) from tversion
) Q
group by
f1
) Q ) P;
--- MinLiteralTemp_p1
select 'MinLiteralTemp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '2000-01-01' f1 from tversion union
select min( '2000-01-01' ) from tversion
) Q
group by
f1
) Q ) P;
--- ModBoundaryTinyNumber_p1
select 'ModBoundaryTinyNumber_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 0 f1 from tversion union
select mod( 35, 0.000000000001 ) from tversion
) Q
group by
f1
) Q ) P;
--- ModCore2ExactNumeric_p1
select 'ModCore2ExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 3 f2 from tversion union
select vdec.rnum, mod( 3,vdec.cdec ) from vdec where vdec.rnum <> 2
) Q
group by
f1,f2
) Q ) P;
--- ModCore2ExactNumeric_p2
select 'ModCore2ExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 3 f2 from tversion union
select tdec.rnum, mod( 3,tdec.cdec ) from tdec where tdec.rnum <> 2
) Q
group by
f1,f2
) Q ) P;
--- AvgIntTruncates_p3
select 'AvgIntTruncates_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(vsint.csint) from vsint
) Q
group by
f1
) Q ) P;
--- ModCore2ExactNumeric_p3
select 'ModCore2ExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 3 f2 from tversion union
select vnum.rnum, mod( 3,vnum.cnum ) from vnum where vnum.rnum <> 2
) Q
group by
f1,f2
) Q ) P;
--- ModCore2ExactNumeric_p4
select 'ModCore2ExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 3 f2 from tversion union
select tnum.rnum, mod( 3,tnum.cnum ) from tnum where tnum.rnum <> 2
) Q
group by
f1,f2
) Q ) P;
--- ModCore2Integers_p1
select 'ModCore2Integers_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 3 f2 from tversion union
select vint.rnum, mod( 3,vint.cint ) from vint where vint.rnum <> 2
) Q
group by
f1,f2
) Q ) P;
--- ModCore2Integers_p2
select 'ModCore2Integers_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 3 f2 from tversion union
select tint.rnum, mod( 3,tint.cint ) from tint where tint.rnum <> 2
) Q
group by
f1,f2
) Q ) P;
--- ModCore2Integers_p3
select 'ModCore2Integers_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 3 f2 from tversion union
select vsint.rnum, mod( 3,vsint.csint ) from vsint where vsint.rnum <> 2
) Q
group by
f1,f2
) Q ) P;
--- ModCore2Integers_p4
select 'ModCore2Integers_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 3 f2 from tversion union
select tsint.rnum, mod( 3,tsint.csint ) from tsint where tsint.rnum <> 2
) Q
group by
f1,f2
) Q ) P;
--- ModCore2Integers_p5
select 'ModCore2Integers_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 3 f2 from tversion union
select vbint.rnum, mod( 3,vbint.cbint ) from vbint where vbint.rnum <> 2
) Q
group by
f1,f2
) Q ) P;
--- ModCore2Integers_p6
select 'ModCore2Integers_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 3 f1, 0 f2 from tversion union
select 4 f1, 3 f2 from tversion union
select tbint.rnum, mod( 3,tbint.cbint ) from tbint where tbint.rnum <> 2
) Q
group by
f1,f2
) Q ) P;
--- ModCoreExactNumeric_p1
select 'ModCoreExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .1 f2 from tversion union
select 5 f1, 1 f2 from tversion union
select rnum, mod( vdec.cdec, 3 ) from vdec
) Q
group by
f1,f2
) Q ) P;
--- ModCoreExactNumeric_p2
select 'ModCoreExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .1 f2 from tversion union
select 5 f1, 1 f2 from tversion union
select rnum, mod( tdec.cdec, 3 ) from tdec
) Q
group by
f1,f2
) Q ) P;
--- AbsCoreApproximateNumeric_p6
select 'AbsCoreApproximateNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 1.000000000000000e+000 f2 from tversion union
select 4 f1, 1.000000000000000e-001 f2 from tversion union
select 5 f1, 1.000000000000000e+001 f2 from tversion union
select rnum, abs( trl.crl ) from trl
) Q
group by
f1,f2
) Q ) P;
--- AvgIntTruncates_p4
select 'AvgIntTruncates_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(tsint.csint) from tsint
) Q
group by
f1
) Q ) P;
--- ModCoreExactNumeric_p3
select 'ModCoreExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .1 f2 from tversion union
select 5 f1, 1 f2 from tversion union
select rnum, mod( vnum.cnum, 3 ) from vnum
) Q
group by
f1,f2
) Q ) P;
--- ModCoreExactNumeric_p4
select 'ModCoreExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .1 f2 from tversion union
select 5 f1, 1 f2 from tversion union
select rnum, mod( tnum.cnum, 3 ) from tnum
) Q
group by
f1,f2
) Q ) P;
--- ModCoreIntegers_p1
select 'ModCoreIntegers_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select rnum, mod( vint.cint, 3 ) from vint
) Q
group by
f1,f2
) Q ) P;
--- ModCoreIntegers_p2
select 'ModCoreIntegers_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select rnum, mod( tint.cint, 3 ) from tint
) Q
group by
f1,f2
) Q ) P;
--- ModCoreIntegers_p3
select 'ModCoreIntegers_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select rnum, mod( vsint.csint, 3 ) from vsint
) Q
group by
f1,f2
) Q ) P;
--- ModCoreIntegers_p4
select 'ModCoreIntegers_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select rnum, mod( tsint.csint, 3 ) from tsint
) Q
group by
f1,f2
) Q ) P;
--- ModCoreIntegers_p5
select 'ModCoreIntegers_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select rnum, mod( vbint.cbint, 3 ) from vbint
) Q
group by
f1,f2
) Q ) P;
--- ModCoreIntegers_p6
select 'ModCoreIntegers_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, -1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select rnum, mod( tbint.cbint, 3 ) from tbint
) Q
group by
f1,f2
) Q ) P;
--- MultipleSumDistinct_p1
select 'MultipleSumDistinct_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 210 f1, 4 f2 from tversion union
select sum( distinct c1 ), count( distinct c2 ) from tset1
) Q
group by
f1,f2
) Q ) P;
--- Negate_p1
select 'Negate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -4 f1 from tversion union
select -(2 * 2) from tversion
) Q
group by
f1
) Q ) P;
--- AvgIntTruncates_p5
select 'AvgIntTruncates_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(vbint.cbint) from vbint
) Q
group by
f1
) Q ) P;
--- NullifCoreReturnsNull_p1
select 'NullifCoreReturnsNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select nullif(cnnull, cnnull) from tversion
) Q
group by
f1
) Q ) P;
--- NullifCoreReturnsNull_p2
select 'NullifCoreReturnsNull_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select nullif(1,1) from tversion
) Q
group by
f1
) Q ) P;
--- NullifCoreReturnsNull_p3
select 'NullifCoreReturnsNull_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select nullif(cnnull, 1) from tversion
) Q
group by
f1
) Q ) P;
--- NullifCoreReturnsOne_p1
select 'NullifCoreReturnsOne_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select nullif(1,2) from tversion
) Q
group by
f1
) Q ) P;
--- NumericComparisonGreaterThanOrEqual_p1
select 'NumericComparisonGreaterThanOrEqual_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where 210.3 >= 7
) Q
group by
f1
) Q ) P;
--- NumericComparisonGreaterThan_p1
select 'NumericComparisonGreaterThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where 210.3 > 7
) Q
group by
f1
) Q ) P;
--- NumericComparisonLessThanOrEqual_p1
select 'NumericComparisonLessThanOrEqual_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where 7 <= 210.3
) Q
group by
f1
) Q ) P;
--- NumericComparisonLessThan_p1
select 'NumericComparisonLessThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where 7 < 210.3
) Q
group by
f1
) Q ) P;
--- NumericComparisonNotEqual_p1
select 'NumericComparisonNotEqual_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where 7 <> 210.3
) Q
group by
f1
) Q ) P;
--- AvgIntTruncates_p6
select 'AvgIntTruncates_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(tbint.cbint) from tbint
) Q
group by
f1
) Q ) P;
--- NumericLiteral_p1
select 'NumericLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select +1.000000000000000e+000 f1 from tversion union
select 1.0 from tversion
) Q
group by
f1
) Q ) P;
--- OlapCoreAvgMultiplePartitions_p1
select 'OlapCoreAvgMultiplePartitions_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5,f6,f7, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4,  00000000000000000000000000000027.142857 f5,  00000000000000000000000000000016.666667 f6,  00000000000000000000000000000012.500000 f7 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4,  00000000000000000000000000000027.142857 f5,  00000000000000000000000000000016.666667 f6,  00000000000000000000000000000012.500000 f7 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4,  00000000000000000000000000000027.142857 f5,  00000000000000000000000000000016.666667 f6,  00000000000000000000000000000025.000000 f7 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4,  00000000000000000000000000000027.142857 f5,  00000000000000000000000000000020.000000 f6,  00000000000000000000000000000020.000000 f7 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4,  00000000000000000000000000000027.142857 f5,  00000000000000000000000000000030.000000 f6,  00000000000000000000000000000030.000000 f7 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4,  00000000000000000000000000000027.142857 f5,  00000000000000000000000000000040.000000 f6,  00000000000000000000000000000040.000000 f7 from tversion union
select 6 f1, null f2, null f3, 50 f4,  00000000000000000000000000000027.142857 f5,  00000000000000000000000000000050.000000 f6,  00000000000000000000000000000050.000000 f7 from tversion union
select 7 f1, null f2, null f3, null f4,  00000000000000000000000000000027.142857 f5,  00000000000000000000000000000050.000000 f6,  00000000000000000000000000000050.000000 f7 from tversion union
select rnum, c1, c2, c3, avg(c3) over (), avg( c3 ) over(partition by c1), avg( c3 ) over(partition by c1,c2) from tolap
) Q
group by
f1,f2,f3,f4,f5,f6,f7
) Q ) P;
--- OlapCoreAvgNoWindowFrame_p1
select 'OlapCoreAvgNoWindowFrame_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 1.666666666666667e+001 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 1.666666666666667e+001 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 1.666666666666667e+001 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 2.000000000000000e+001 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 3.000000000000000e+001 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 4.000000000000000e+001 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 5.000000000000000e+001 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 5.000000000000000e+001 f5 from tversion union
select rnum, c1, c2, c3, avg( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreAvgRowsBetween_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation: ERROR: window specifications with a framing clause must have an ORDER BY clause
select 'OlapCoreAvgRowsBetween_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 1.666666666666667e+001 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 1.666666666666667e+001 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 1.666666666666667e+001 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 2.000000000000000e+001 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 3.000000000000000e+001 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 4.000000000000000e+001 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 5.000000000000000e+001 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 5.000000000000000e+001 f5 from tversion union
select rnum, c1, c2, c3, avg( c3 ) over(partition by c1 rows between unbounded preceding and unbounded following) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreCountMultiplePartitions_p1
select 'OlapCoreCountMultiplePartitions_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5,f6,f7, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 7 f5, 3 f6, 2 f7 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 7 f5, 3 f6, 2 f7 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 7 f5, 3 f6, 1 f7 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 7 f5, 1 f6, 1 f7 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 7 f5, 1 f6, 1 f7 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 7 f5, 1 f6, 1 f7 from tversion union
select 6 f1, null f2, null f3, 50 f4, 7 f5, 1 f6, 1 f7 from tversion union
select 7 f1, null f2, null f3, null f4, 7 f5, 1 f6, 1 f7 from tversion union
select rnum, c1, c2, c3, count(c3) over (), count( c3 ) over(partition by c1), count( c3 ) over(partition by c1,c2) from tolap
) Q
group by
f1,f2,f3,f4,f5,f6,f7
) Q ) P;
--- OlapCoreCountNoWindowFrame_p1
select 'OlapCoreCountNoWindowFrame_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 3 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 3 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 3 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 1 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 1 f5 from tversion union
select rnum, c1, c2, c3, count( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreCountRowsBetween_p2
--- test expected to fail until GPDB supports function
--- GPDB Limitation: ERROR:  window specifications with a framing clause must have an ORDER BY clause
select 'OlapCoreCountRowsBetween_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 3 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 3 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 3 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 1 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 1 f5 from tversion union
select rnum, c1, c2, c3, count( c3 ) over(partition by c1 rows between unbounded preceding and unbounded following) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreCountStar_p1
select 'OlapCoreCountStar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 3 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 3 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 3 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 2 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 2 f5 from tversion union
select rnum, c1, c2, c3, count(*) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreCumedistNullOrdering_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation syntax not supported; NULLS LAST
select 'OlapCoreCumedistNullOrdering_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 1.000000000000000e+000 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 6.666666666666670e-001 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 3.333333333333330e-001 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1.000000000000000e+000 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1.000000000000000e+000 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1.000000000000000e+000 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 5.000000000000000e-001 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 1.000000000000000e+000 f5 from tversion union
select rnum, c1, c2, c3, cume_dist() over(partition by c1 order by c3 desc nulls last) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreCumedist_p1
select 'OlapCoreCumedist_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 1.000000000000000e+000 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 6.666666666666670e-001 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 3.333333333333330e-001 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1.000000000000000e+000 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1.000000000000000e+000 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1.000000000000000e+000 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 5.000000000000000e-001 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 1.000000000000000e+000 f5 from tversion union
select rnum, c1, c2, c3, cume_dist() over(partition by c1 order by c3 desc) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- AvgInt_p1
select 'AvgInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(vint.cint) from vint
) Q
group by
f1
) Q ) P;
--- OlapCoreDenseRankNullOrdering_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation syntax not supported; NULLS LAST
select 'OlapCoreDenseRankNullOrdering_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 3 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 2 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 1 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 1 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 2 f5 from tversion union
select rnum, c1, c2, c3, dense_rank() over(partition by c1 order by c3 desc nulls last) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreDenseRank_p1
select 'OlapCoreDenseRank_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 3 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 2 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 1 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 2 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 1 f5 from tversion union
select rnum, c1, c2, c3, dense_rank() over(partition by c1 order by c3 desc ) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreFirstValueNullOrdering_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation syntax not supported; NULLS LAST
select 'OlapCoreFirstValueNullOrdering_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 10 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 10 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 10 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 50 f5 from tversion union
select rnum, c1, c2, c3, first_value( c3 ) over(partition by c1 order by c3 asc nulls last) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreFirstValueRowsBetween_p1
select 'OlapCoreFirstValueRowsBetween_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 10 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 10 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 10 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 50 f5 from tversion union
select rnum, c1, c2, c3, first_value( c3 ) over(partition by c1 order by c3 rows between unbounded preceding and unbounded following) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreLastValueNoWindowFrame_p1
select 'OlapCoreLastValueNoWindowFrame_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 25 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 25 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 25 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, null f5 from tversion union
select 7 f1, null f2, null f3, null f4, null f5 from tversion union
select rnum, c1, c2, c3, last_value( c3 ) over( partition by c1 order by c3 ) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreLastValueNullOrdering_p1
--- test expected to fail until GPDB support function
--- GPDB Limitation syntax not supported; NULLS LAST
select 'OlapCoreLastValueNullOrdering_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 25 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 25 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 25 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, null f5 from tversion union
select 7 f1, null f2, null f3, null f4, null f5 from tversion union
select rnum, c1, c2, c3, last_value( c3 ) over(partition by c1 order by c3 asc nulls last) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreLastValueRowsBetween_p1
select 'OlapCoreLastValueRowsBetween_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 25 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 25 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 25 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, null f5 from tversion union
select 7 f1, null f2, null f3, null f4, null f5 from tversion union
select rnum, c1, c2, c3, last_value( c3 ) over(partition by c1 order by c3 rows between unbounded preceding and unbounded following) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreMax_p1
select 'OlapCoreMax_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 25 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 25 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 25 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 50 f5 from tversion union
select rnum, c1, c2, c3, max( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreMin_p1
select 'OlapCoreMin_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 10 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 10 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 10 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 50 f5 from tversion union
select rnum, c1, c2, c3, min( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- AvgInt_p2
select 'AvgInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(tint.cint) from tint
) Q
group by
f1
) Q ) P;
--- OlapCoreNtile_p1
select 'OlapCoreNtile_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select  00000000000000000000000000000000000000 f1, 'AAA' f2, 'AA' f3,  00000000000000000000000000000000000010 f4, 1.000000000000000e+000 f5 from tversion union
select  00000000000000000000000000000000000001 f1, 'AAA' f2, 'AA' f3,  00000000000000000000000000000000000015 f4, 1.000000000000000e+000 f5 from tversion union
select  00000000000000000000000000000000000002 f1, 'AAA' f2, 'AB' f3,  00000000000000000000000000000000000025 f4, 2.000000000000000e+000 f5 from tversion union
select  00000000000000000000000000000000000003 f1, 'BBB' f2, 'BB' f3,  00000000000000000000000000000000000020 f4, 2.000000000000000e+000 f5 from tversion union
select  00000000000000000000000000000000000004 f1, 'CCC' f2, 'CC' f3,  00000000000000000000000000000000000030 f4, 3.000000000000000e+000 f5 from tversion union
select  00000000000000000000000000000000000005 f1, 'DDD' f2, 'DD' f3,  00000000000000000000000000000000000040 f4, 3.000000000000000e+000 f5 from tversion union
select  00000000000000000000000000000000000006 f1, null f2, null f3,  00000000000000000000000000000000000050 f4, 4.000000000000000e+000 f5 from tversion union
select  00000000000000000000000000000000000007 f1, null f2, null f3, null f4, 4.000000000000000e+000 f5 from tversion union
select rnum, c1, c2, c3, ntile(4) over(order by c3) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreNullOrder_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation syntax not supported; NULLS FIRST
select 'OlapCoreNullOrder_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 50 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 50 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 50 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 50 f5 from tversion union
select rnum, c1, c2, c3, sum( c3 ) over(partition by c1 order by c1 asc nulls first) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCorePercentRankNullOrdering_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation syntax not supported; NULLS LAST
select 'OlapCorePercentRankNullOrdering_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 1.000000000000000e+000 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 5.000000000000000e-001 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 0.000000000000000e+000 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 0.000000000000000e+000 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 0.000000000000000e+000 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 0.000000000000000e+000 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 0.000000000000000e+000 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 1.000000000000000e+000 f5 from tversion union
select rnum, c1, c2, c3, percent_rank() over(partition by c1 order by c3 desc nulls last) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCorePercentRank_p1
select 'OlapCorePercentRank_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 1.000000000000000e+000 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 5.000000000000000e-001 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 0.000000000000000e+000 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 0.000000000000000e+000 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 0.000000000000000e+000 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 0.000000000000000e+000 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 0.000000000000000e+000 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 1.000000000000000e+000 f5 from tversion union
select rnum, c1, c2, c3, percent_rank() over(partition by c1 order by c3 desc) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreRankMultiplePartitions_p1
select 'OlapCoreRankMultiplePartitions_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5,f6,f7, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 8 f5, 3 f6, 2 f7 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 7 f5, 2 f6, 1 f7 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 5 f5, 1 f6, 1 f7 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 6 f5, 1 f6, 1 f7 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 4 f5, 1 f6, 1 f7 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 3 f5, 1 f6, 1 f7 from tversion union
select 6 f1, null f2, null f3, 50 f4, 2 f5, 2 f6, 2 f7 from tversion union
select 7 f1, null f2, null f3, null f4, 1 f5, 1 f6, 1 f7 from tversion union
select rnum, c1, c2, c3, rank() over(order by c3 desc),rank() over(partition by c1 order by c3 desc),rank() over(partition by c1,c2 order by c3 desc) from tolap
) Q
group by
f1,f2,f3,f4,f5,f6,f7
) Q ) P;
--- OlapCoreRankNoWindowFrame_p1
select 'OlapCoreRankNoWindowFrame_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 2 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 1 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 1 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 2 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 1 f5 from tversion union
select rnum, c1, c2, c3, rank() over(partition by c1,c2 order by c3 desc) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreRankNullOrdering_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation syntax not supported; NULLS LAST
select 'OlapCoreRankNullOrdering_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 2 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 1 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 1 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 1 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 2 f5 from tversion union
select rnum, c1, c2, c3, rank() over(partition by c1,c2 order by c3 desc nulls last) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreRankOrderby100_p1
select 'OlapCoreRankOrderby100_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 1 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 1 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 1 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 1 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 1 f5 from tversion union
select rnum, c1, c2, c3, rank( ) over(partition by c1 order by 100) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreRowNumberNullOrdering_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation: syntax not supported; NULLS LAST
select 'OlapCoreRowNumberNullOrdering_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 2 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 1 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 1 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 1 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 2 f5 from tversion union
select rnum, c1, c2, c3, row_number() over(partition by c1,c2 order by c3 desc nulls last) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreRowNumber_p1
select 'OlapCoreRowNumber_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 2 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 1 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 1 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 1 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 1 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 1 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 2 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 1 f5 from tversion union
select rnum, c1, c2, c3, row_number() over(partition by c1,c2 order by c3 desc) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- AvgInt_p3
select 'AvgInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(vsint.csint) from vsint
) Q
group by
f1
) Q ) P;
--- OlapCoreRunningSum_p1
select 'OlapCoreRunningSum_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 'AAA' f1, 'AA' f2,  00000000000000000000000000000000000025 f3,  00000000000000000000000000000000000025 f4 from tversion union
select 'AAA' f1, 'AB' f2,  00000000000000000000000000000000000025 f3,  00000000000000000000000000000000000050 f4 from tversion union
select 'BBB' f1, 'BB' f2,  00000000000000000000000000000000000020 f3,  00000000000000000000000000000000000020 f4 from tversion union
select 'CCC' f1, 'CC' f2,  00000000000000000000000000000000000030 f3,  00000000000000000000000000000000000030 f4 from tversion union
select 'DDD' f1, 'DD' f2,  00000000000000000000000000000000000040 f3,  00000000000000000000000000000000000040 f4 from tversion union
select null f1, null f2,  00000000000000000000000000000000000050 f3,  00000000000000000000000000000000000050 f4 from tversion union
select c1, c2, sum (c3), sum(sum(c3)) over(partition by c1 order by c1,c2 rows unbounded preceding) from tolap group by c1,c2
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- OlapCoreStddevPop_p1
select 'OlapCoreStddevPop_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 6.236095644623235e+000 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 6.236095644623235e+000 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 6.236095644623235e+000 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 0.000000000000000e+000 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 0.000000000000000e+000 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 0.000000000000000e+000 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 0.000000000000000e+000 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 0.000000000000000e+000 f5 from tversion union
select rnum, c1, c2, c3, stddev_pop( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreStddevSamp_p1
select 'OlapCoreStddevSamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 7.637626158259730e+000 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 7.637626158259730e+000 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 7.637626158259730e+000 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, null f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, null f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, null f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, null f5 from tversion union
select 7 f1, null f2, null f3, null f4, null f5 from tversion union
select rnum, c1, c2, c3, stddev_samp( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreStddev_p1
select 'OlapCoreStddev_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 7.637626158259730e+000 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 7.637626158259730e+000 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 7.637626158259730e+000 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, null f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, null f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, null f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, null f5 from tversion union
select 7 f1, null f2, null f3, null f4, null f5 from tversion union
select rnum, c1, c2, c3, stddev( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreSumMultiplePartitions_p1
select 'OlapCoreSumMultiplePartitions_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5,f6,f7, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4,  00000000000000000000000000000000000190 f5,  00000000000000000000000000000000000050 f6,  00000000000000000000000000000000000025 f7 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4,  00000000000000000000000000000000000190 f5,  00000000000000000000000000000000000050 f6,  00000000000000000000000000000000000025 f7 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4,  00000000000000000000000000000000000190 f5,  00000000000000000000000000000000000050 f6,  00000000000000000000000000000000000025 f7 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4,  00000000000000000000000000000000000190 f5,  00000000000000000000000000000000000020 f6,  00000000000000000000000000000000000020 f7 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4,  00000000000000000000000000000000000190 f5,  00000000000000000000000000000000000030 f6,  00000000000000000000000000000000000030 f7 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4,  00000000000000000000000000000000000190 f5,  00000000000000000000000000000000000040 f6,  00000000000000000000000000000000000040 f7 from tversion union
select 6 f1, null f2, null f3, 50 f4,  00000000000000000000000000000000000190 f5,  00000000000000000000000000000000000050 f6,  00000000000000000000000000000000000050 f7 from tversion union
select 7 f1, null f2, null f3, null f4,  00000000000000000000000000000000000190 f5,  00000000000000000000000000000000000050 f6,  00000000000000000000000000000000000050 f7 from tversion union
select rnum, c1, c2, c3, sum(c3) over (), sum( c3 ) over(partition by c1), sum( c3 ) over(partition by c1,c2) from tolap
) Q
group by
f1,f2,f3,f4,f5,f6,f7
) Q ) P;
--- OlapCoreSumNullOrdering_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation syntax not supported; NULLS LAST
select 'OlapCoreSumNullOrdering_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 50 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 50 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 50 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 50 f5 from tversion union
select rnum, c1, c2, c3, sum( c3 ) over(partition by c1 order by c1 asc nulls last) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreSumOfGroupedSums_p1
select 'OlapCoreSumOfGroupedSums_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 'AAA' f1, 'AA' f2,  00000000000000000000000000000000000025 f3,  00000000000000000000000000000000000050 f4 from tversion union
select 'AAA' f1, 'AB' f2,  00000000000000000000000000000000000025 f3,  00000000000000000000000000000000000050 f4 from tversion union
select 'BBB' f1, 'BB' f2,  00000000000000000000000000000000000020 f3,  00000000000000000000000000000000000020 f4 from tversion union
select 'CCC' f1, 'CC' f2,  00000000000000000000000000000000000030 f3,  00000000000000000000000000000000000030 f4 from tversion union
select 'DDD' f1, 'DD' f2,  00000000000000000000000000000000000040 f3,  00000000000000000000000000000000000040 f4 from tversion union
select null f1, null f2,  00000000000000000000000000000000000050 f3,  00000000000000000000000000000000000050 f4 from tversion union
select c1, c2, sum ( c3 ), sum(sum(c3)) over(partition by c1) from tolap group by c1,c2
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- OlapCoreSumOrderby100_p1
select 'OlapCoreSumOrderby100_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 50 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 50 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 50 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 50 f5 from tversion union
select rnum, c1, c2, c3, sum( c3 ) over(partition by c1 order by 100) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreSum_p1
select 'OlapCoreSum_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 50 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 50 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 50 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 50 f5 from tversion union
select rnum, c1, c2, c3, sum( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreVariance_p1
select 'OlapCoreVariance_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 5.833333333333331e+001 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 5.833333333333331e+001 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 5.833333333333331e+001 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, null f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, null f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, null f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, null f5 from tversion union
select 7 f1, null f2, null f3, null f4, null f5 from tversion union
select rnum, c1, c2, c3, variance( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- AvgInt_p4
select 'AvgInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(tsint.csint) from tsint
) Q
group by
f1
) Q ) P;
--- OlapCoreVarPop_p1
select 'OlapCoreVarPop_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 3.888888888888889e+001 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 3.888888888888889e+001 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 3.888888888888889e+001 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 0.000000000000000e+000 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 0.000000000000000e+000 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 0.000000000000000e+000 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 0.000000000000000e+000 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 0.000000000000000e+000 f5 from tversion union
select rnum, c1, c2, c3, var_pop( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreVarSamp_p1
select 'OlapCoreVarSamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 5.833333333333331e+001 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 5.833333333333331e+001 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 5.833333333333331e+001 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, null f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, null f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, null f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, null f5 from tversion union
select 7 f1, null f2, null f3, null f4, null f5 from tversion union
select rnum, c1, c2, c3, var_samp( c3 ) over(partition by c1) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreWindowFrameMultiplePartitions_p1
select 'OlapCoreWindowFrameMultiplePartitions_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5,f6, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 50 f4, 25 f5, 190 f6 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 50 f4, 25 f5, 190 f6 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 50 f4, 25 f5, 190 f6 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5, 190 f6 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5, 190 f6 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5, 190 f6 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5, 190 f6 from tversion union
select 7 f1, null f2, null f3, 50 f4, 50 f5, 190 f6 from tversion union
select rnum, c1, c2, sum(c3) over (partition by c1), sum(c3) over (partition by c2), sum(c3) over () from tolap
) Q
group by
f1,f2,f3,f4,f5,f6
) Q ) P;
--- OlapCoreWindowFrameRowsBetweenPrecedingFollowing_p1
select 'OlapCoreWindowFrameRowsBetweenPrecedingFollowing_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 25 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 45 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 75 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 60 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 95 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 120 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 90 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 50 f5 from tversion union
select rnum, c1, c2, c3, sum(c3) over ( order by c3 rows between 1 preceding and 1 following ) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreWindowFrameRowsBetweenPrecedingPreceding_p1
select 'OlapCoreWindowFrameRowsBetweenPrecedingPreceding_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, null f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 10 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 20 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 15 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 25 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 30 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 40 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 50 f5 from tversion union
select rnum, c1, c2, c3, sum(c3) over ( order by c3 rows between 1 preceding and 1 preceding ) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreWindowFrameRowsBetweenUnboundedFollowing_p1
select 'OlapCoreWindowFrameRowsBetweenUnboundedFollowing_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 190 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 180 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 145 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 165 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 120 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 90 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5 from tversion union
select 7 f1, null f2, null f3, null f4, null f5 from tversion union
select rnum, c1, c2, c3, sum(c3) over ( order by c3 rows between current row and unbounded following ) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreWindowFrameRowsBetweenUnboundedPreceding_p1
select 'OlapCoreWindowFrameRowsBetweenUnboundedPreceding_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 10 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 25 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 70 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 45 f5 from tversion union
select 3 f1, 'CCC' f2, 'CC' f3, 30 f4, 100 f5 from tversion union
select 4 f1, 'DDD' f2, 'DD' f3, 40 f4, 140 f5 from tversion union
select 5 f1, null f2, null f3, 50 f4, 190 f5 from tversion union
select 6 f1, null f2, null f3, null f4, 190 f5 from tversion union
select rnum, c1, c2, c3, sum(c3) over ( order by c3 rows between unbounded preceding and current row ) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreWindowFrameRowsPreceding_p1
select 'OlapCoreWindowFrameRowsPreceding_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 10 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 15 f4, 25 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 25 f4, 60 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 45 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 75 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 95 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 120 f5 from tversion union
select 7 f1, null f2, null f3, null f4, 90 f5 from tversion union
select rnum, c1, c2, c3, sum(c3) over ( order by c3 rows 2 preceding ) from tolap
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OlapCoreWindowFrameWindowDefinition_p1
--- test expected to fail until GPDB supports function
--- GPDB Limitation: syntax not supported
select 'OlapCoreWindowFrameWindowDefinition_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 'AAA' f2, 'AA' f3, 10 f4, 10 f5 from tversion union
select 1 f1, 'AAA' f2, 'AA' f3, 25 f4, 15 f5 from tversion union
select 2 f1, 'AAA' f2, 'AB' f3, 50 f4, 25 f5 from tversion union
select 3 f1, 'BBB' f2, 'BB' f3, 20 f4, 20 f5 from tversion union
select 4 f1, 'CCC' f2, 'CC' f3, 30 f4, 30 f5 from tversion union
select 5 f1, 'DDD' f2, 'DD' f3, 40 f4, 40 f5 from tversion union
select 6 f1, null f2, null f3, 50 f4, 50 f5 from tversion union
select 7 f1, null f2, null f3, 50 f4, null f5 from tversion union
select rnum, c1, c2, sum(c3) over w1, sum (c3) over w2 from tolap  window w1 as (partition by c1 order by c3), w2 as ( w1 rows between unbounded preceding and current row)
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- OperatorAnd_p1
select 'OperatorAnd_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'AAA' f2 from tversion union
select 10 f1, 'AAA' f2 from tversion union
select 10 f1, 'AAA' f2 from tversion union
select tset1.c1, tset1.c2  from tset1 where c1=10 and c2='AAA'
) Q
group by
f1,f2
) Q ) P;
--- AvgInt_p5
select 'AvgInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(vbint.cbint) from vbint
) Q
group by
f1
) Q ) P;
--- OperatorOr_p1
select 'OperatorOr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 30 f1, 'CCC' f2 from tversion union
select 40 f1, 'DDD' f2 from tversion union
select tset1.c1, tset1.c2  from tset1 where c1=30 or c2='DDD'
) Q
group by
f1,f2
) Q ) P;
--- OrderByOrdinal_p1
select 'OrderByOrdinal_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'AAA' f3 from tversion union
select 1 f1, 10 f2, 'AAA' f3 from tversion union
select 2 f1, 10 f2, 'AAA' f3 from tversion union
select 3 f1, 20 f2, 'BBB' f3 from tversion union
select 4 f1, 30 f2, 'CCC' f3 from tversion union
select 5 f1, 40 f2, 'DDD' f3 from tversion union
select 6 f1, 50 f2, null f3 from tversion union
select 7 f1, 60 f2, null f3 from tversion union
select 8 f1, null f2, 'AAA' f3 from tversion union
select 9 f1, null f2, 'AAA' f3 from tversion union
select 10 f1, null f2, null f3 from tversion union
select 11 f1, null f2, null f3 from tversion union
select rnum, c1, c2 from tset1 order by 1,2
) Q
group by
f1,f2,f3
) Q ) P;
--- PositionCoreString1Empty_p1
select 'PositionCoreString1Empty_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select 5 f1, 1 f2 from tversion union
select rnum, position( '' in vchar.cchar ) from vchar
) Q
group by
f1,f2
) Q ) P;
--- PositionCoreString1Empty_p2
select 'PositionCoreString1Empty_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select 5 f1, 1 f2 from tversion union
select rnum, position( '' in tchar.cchar ) from tchar
) Q
group by
f1,f2
) Q ) P;
--- PositionCoreString1Empty_p3
select 'PositionCoreString1Empty_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select 5 f1, 1 f2 from tversion union
select rnum, position( '' in vvchar.cvchar ) from vvchar
) Q
group by
f1,f2
) Q ) P;
--- PositionCoreString1Empty_p4
select 'PositionCoreString1Empty_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 1 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 1 f2 from tversion union
select 5 f1, 1 f2 from tversion union
select rnum, position( '' in tvchar.cvchar ) from tvchar
) Q
group by
f1,f2
) Q ) P;
--- PositionCore_p1
select 'PositionCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 0 f2 from tversion union
select rnum, position( 'B' in vchar.cchar ) from vchar
) Q
group by
f1,f2
) Q ) P;
--- PositionCore_p2
select 'PositionCore_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 0 f2 from tversion union
select rnum, position( 'B' in tchar.cchar ) from tchar
) Q
group by
f1,f2
) Q ) P;
--- PositionCore_p3
select 'PositionCore_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 0 f2 from tversion union
select rnum, position( 'B' in vvchar.cvchar ) from vvchar
) Q
group by
f1,f2
) Q ) P;
--- PositionCore_p4
select 'PositionCore_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 0 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 0 f2 from tversion union
select 5 f1, 0 f2 from tversion union
select rnum, position( 'B' in tvchar.cvchar ) from tvchar
) Q
group by
f1,f2
) Q ) P;
--- AvgInt_p6
select 'AvgInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(tbint.cbint) from tbint
) Q
group by
f1
) Q ) P;
--- PowerBoundary_p1
select 'PowerBoundary_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select power( 0,0 ) from tversion
) Q
group by
f1
) Q ) P;
--- PowerCoreApproxNumeric_p1
select 'PowerCoreApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .01 f2 from tversion union
select 5 f1, 100 f2 from tversion union
select vflt.rnum, power( vflt.cflt,2 ) from vflt
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreApproxNumeric_p2
select 'PowerCoreApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .01 f2 from tversion union
select 5 f1, 100 f2 from tversion union
select tflt.rnum, power( tflt.cflt,2 ) from tflt
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreApproxNumeric_p3
select 'PowerCoreApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .01 f2 from tversion union
select 5 f1, 100 f2 from tversion union
select vdbl.rnum, power( vdbl.cdbl,2 ) from vdbl
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreApproxNumeric_p4
select 'PowerCoreApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .01 f2 from tversion union
select 5 f1, 100 f2 from tversion union
select tdbl.rnum, power( tdbl.cdbl,2 ) from tdbl
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreApproxNumeric_p5
select 'PowerCoreApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .01 f2 from tversion union
select 5 f1, 100 f2 from tversion union
select vrl.rnum, power( vrl.crl,2 ) from vrl
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreApproxNumeric_p6
select 'PowerCoreApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .01 f2 from tversion union
select 5 f1, 100 f2 from tversion union
select trl.rnum, power( trl.crl,2 ) from trl
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreExactNumeric_p1
select 'PowerCoreExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .01 f2 from tversion union
select 5 f1, 100 f2 from tversion union
select vdec.rnum, power( vdec.cdec,2 ) from vdec
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreExactNumeric_p2
select 'PowerCoreExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .01 f2 from tversion union
select 5 f1, 100 f2 from tversion union
select tdec.rnum, power( tdec.cdec,2 ) from tdec
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreExactNumeric_p3
select 'PowerCoreExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .01 f2 from tversion union
select 5 f1, 100 f2 from tversion union
select vnum.rnum, power( vnum.cnum,2 ) from vnum
) Q
group by
f1,f2
) Q ) P;
--- BooleanComparisonOperatorAnd_p1
select 'BooleanComparisonOperatorAnd_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where (1 < 2) and (3 < 4)
) Q
group by
f1
) Q ) P;
--- PowerCoreExactNumeric_p4
select 'PowerCoreExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, .01 f2 from tversion union
select 5 f1, 100 f2 from tversion union
select tnum.rnum, power( tnum.cnum,2 ) from tnum
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreIntegers_p1
select 'PowerCoreIntegers_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 100 f2 from tversion union
select vint.rnum, power( vint.cint,2 ) from vint
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreIntegers_p2
select 'PowerCoreIntegers_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 100 f2 from tversion union
select tint.rnum, power( tint.cint,2 ) from tint
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreIntegers_p3
select 'PowerCoreIntegers_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 100 f2 from tversion union
select vsint.rnum, power( vsint.csint,2 ) from vsint
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreIntegers_p4
select 'PowerCoreIntegers_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 100 f2 from tversion union
select tsint.rnum, power( tsint.csint,2 ) from tsint
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreIntegers_p5
select 'PowerCoreIntegers_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 100 f2 from tversion union
select vbint.rnum, power( vbint.cbint,2 ) from vbint
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreIntegers_p6
select 'PowerCoreIntegers_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1 f2 from tversion union
select 2 f1, 0 f2 from tversion union
select 3 f1, 1 f2 from tversion union
select 4 f1, 100 f2 from tversion union
select tbint.rnum, power( tbint.cbint,2 ) from tbint
) Q
group by
f1,f2
) Q ) P;
--- PowerCoreNegativeBaseOddExp_p1
select 'PowerCoreNegativeBaseOddExp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -64 f1 from tversion union
select power( -4,3 ) from tversion
) Q
group by
f1
) Q ) P;
--- RowSubquery_p1
select 'RowSubquery_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 15 f3 from tversion union
select rnum, c1, c2 from tjoin1 where (c1,'BB') in (select c1, c2 from tjoin2 where c2='BB')
) Q
group by
f1,f2,f3
) Q ) P;
--- RowValueConstructor_p1
select 'RowValueConstructor_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'AAA' f3 from tversion union
select 1 f1, 10 f2, 'AAA' f3 from tversion union
select 2 f1, 10 f2, 'AAA' f3 from tversion union
select 5 f1, 40 f2, 'DDD' f3 from tversion union
select * from tset1 where (c1,c2) in (select c1,c2 from tset2)
) Q
group by
f1,f2,f3
) Q ) P;
--- AbsCoreExactNumeric_p1
select 'AbsCoreExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 1.000000000000000e+000 f2 from tversion union
select 4 f1, 1.000000000000000e-001 f2 from tversion union
select 5 f1, 1.000000000000000e+001 f2 from tversion union
select rnum, abs( vdec.cdec ) from vdec
) Q
group by
f1,f2
) Q ) P;
--- BooleanComparisonOperatorNotOperatorAnd_p1
select 'BooleanComparisonOperatorNotOperatorAnd_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not (2 < 1) and (3 < 4)
) Q
group by
f1
) Q ) P;
--- ScalarSubqueryInProjList_p1
select 'ScalarSubqueryInProjList_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4, count(*) c  from (
select 0 f1, 10 f2, 15 f3, 15 f4 from tversion union
select 1 f1, 20 f2, 25 f3, 15 f4 from tversion union
select 2 f1, null f2, 50 f3, 15 f4 from tversion union
select tjoin1.rnum, tjoin1.c1, tjoin1.c2, (select max(tjoin2.c1) from tjoin2) csub from tjoin1
) Q
group by
f1,f2,f3,f4
) Q ) P;
--- ScalarSubquery_p1
select 'ScalarSubquery_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 15 f3 from tversion union
select rnum, c1, c2 from tjoin1 where c1 = ( select min(c1) from tjoin1)
) Q
group by
f1,f2,f3
) Q ) P;
--- SelectCountApproxNumeric_p1
select 'SelectCountApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vflt.cflt) from vflt
) Q
group by
f1
) Q ) P;
--- SelectCountApproxNumeric_p2
select 'SelectCountApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tflt.cflt) from tflt
) Q
group by
f1
) Q ) P;
--- SelectCountApproxNumeric_p3
select 'SelectCountApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vdbl.cdbl) from vdbl
) Q
group by
f1
) Q ) P;
--- SelectCountApproxNumeric_p4
select 'SelectCountApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tdbl.cdbl) from tdbl
) Q
group by
f1
) Q ) P;
--- SelectCountApproxNumeric_p5
select 'SelectCountApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vrl.crl) from vrl
) Q
group by
f1
) Q ) P;
--- SelectCountApproxNumeric_p6
select 'SelectCountApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(trl.crl) from trl
) Q
group by
f1
) Q ) P;
--- SelectCountChar_p1
select 'SelectCountChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vchar.cchar) from vchar
) Q
group by
f1
) Q ) P;
--- SelectCountChar_p2
select 'SelectCountChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tchar.cchar) from tchar
) Q
group by
f1
) Q ) P;
--- BooleanComparisonOperatorNotOperatorOr_p1
select 'BooleanComparisonOperatorNotOperatorOr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not (2 < 1) or (3 < 4)
) Q
group by
f1
) Q ) P;
--- SelectCountChar_p3
select 'SelectCountChar_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vchar.cchar) from vchar
) Q
group by
f1
) Q ) P;
--- SelectCountChar_p4
select 'SelectCountChar_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tchar.cchar) from tchar
) Q
group by
f1
) Q ) P;
--- SelectCountChar_p5
select 'SelectCountChar_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vvchar.cvchar) from vvchar
) Q
group by
f1
) Q ) P;
--- SelectCountChar_p6
select 'SelectCountChar_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tvchar.cvchar) from tvchar
) Q
group by
f1
) Q ) P;
--- SelectCountDate_p1
select 'SelectCountDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 3 f1 from tversion union
select count(vdt.cdt) from vdt
) Q
group by
f1
) Q ) P;
--- SelectCountDate_p2
select 'SelectCountDate_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 3 f1 from tversion union
select count(tdt.cdt) from tdt
) Q
group by
f1
) Q ) P;
--- SelectCountExactNumeric_p1
select 'SelectCountExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vdec.cdec) from vdec
) Q
group by
f1
) Q ) P;
--- SelectCountExactNumeric_p2
select 'SelectCountExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tdec.cdec) from tdec
) Q
group by
f1
) Q ) P;
--- SelectCountExactNumeric_p3
select 'SelectCountExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vnum.cnum) from vnum
) Q
group by
f1
) Q ) P;
--- SelectCountExactNumeric_p4
select 'SelectCountExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tnum.cnum) from tnum
) Q
group by
f1
) Q ) P;
--- BooleanComparisonOperatorOr_p1
select 'BooleanComparisonOperatorOr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where (1 < 2) or (4 < 3)
) Q
group by
f1
) Q ) P;
--- SelectCountInt_p1
select 'SelectCountInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(vint.cint) from vint
) Q
group by
f1
) Q ) P;
--- SelectCountInt_p2
select 'SelectCountInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(tint.cint) from tint
) Q
group by
f1
) Q ) P;
--- SelectCountInt_p3
select 'SelectCountInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(vsint.csint) from vsint
) Q
group by
f1
) Q ) P;
--- SelectCountInt_p4
select 'SelectCountInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(tsint.csint) from tsint
) Q
group by
f1
) Q ) P;
--- SelectCountInt_p5
select 'SelectCountInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(vbint.cbint) from vbint
) Q
group by
f1
) Q ) P;
--- SelectCountInt_p6
select 'SelectCountInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(tbint.cbint) from tbint
) Q
group by
f1
) Q ) P;
--- SelectCountNullNumeric_p1
select 'SelectCountNullNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 0 f1 from tversion union
select count(cnnull) from tversion
) Q
group by
f1
) Q ) P;
--- SelectCountNullNumeric_p2
select 'SelectCountNullNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 0 f1 from tversion union
select count(cnnull) from tversion
) Q
group by
f1
) Q ) P;
--- SelectCountNull_p1
select 'SelectCountNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 0 f1 from tversion union
select count(ccnull) from tversion
) Q
group by
f1
) Q ) P;
--- SelectCountStar_p1
select 'SelectCountStar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(*) from tversion
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchApproximateNumeric_p1
select 'CaseBasicSearchApproximateNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'other' f2 from tversion union
select 4 f1, 'other' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vflt.rnum,case when vflt.cflt in ( -1,10,0.1 )  then 'test1' else 'other' end from vflt
) Q
group by
f1,f2
) Q ) P;
--- SelectCountTimestamp_p1
select 'SelectCountTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9 f1 from tversion union
select count(vts.cts) from vts
) Q
group by
f1
) Q ) P;
--- SelectCountTimestamp_p2
select 'SelectCountTimestamp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9 f1 from tversion union
select count(tts.cts) from tts
) Q
group by
f1
) Q ) P;
--- SelectCountTime_p1
select 'SelectCountTime_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 3 f1 from tversion union
select count(vtm.ctm) from vtm
) Q
group by
f1
) Q ) P;
--- SelectCountTime_p2
select 'SelectCountTime_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 3 f1 from tversion union
select count(ttm.ctm) from ttm
) Q
group by
f1
) Q ) P;
--- SelectCountVarChar_p1
select 'SelectCountVarChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vvchar.cvchar) from vvchar
) Q
group by
f1
) Q ) P;
--- SelectCountVarChar_p2
select 'SelectCountVarChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tvchar.cvchar) from tvchar
) Q
group by
f1
) Q ) P;
--- SelectDateComparisonEqualTo_p1
select 'SelectDateComparisonEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2001-01-01' = date '2001-01-01'
) Q
group by
f1
) Q ) P;
--- SelectDateComparisonGreaterThanOrEqualTo_p1
select 'SelectDateComparisonGreaterThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2001-01-01' >= date '2000-01-01'
) Q
group by
f1
) Q ) P;
--- SelectDateComparisonGreaterThan_p1
select 'SelectDateComparisonGreaterThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2001-01-01' > date '2000-01-01'
) Q
group by
f1
) Q ) P;
--- SelectDateComparisonLessThanOrEqualTo_p1
select 'SelectDateComparisonLessThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2000-01-01' <= date '2001-01-01'
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchApproximateNumeric_p2
select 'CaseBasicSearchApproximateNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'other' f2 from tversion union
select 4 f1, 'other' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tflt.rnum,case when tflt.cflt in ( -1,10,0.1 )  then 'test1' else 'other' end from tflt
) Q
group by
f1,f2
) Q ) P;
--- SelectDateComparisonLessThan_p1
select 'SelectDateComparisonLessThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2000-01-01' < date '2001-01-01'
) Q
group by
f1
) Q ) P;
--- SelectDateComparisonNotEqualTo_p1
select 'SelectDateComparisonNotEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2001-01-01' <> date '2000-01-01'
) Q
group by
f1
) Q ) P;
--- SelectJapaneseColumnConcat_p1
select 'SelectJapaneseColumnConcat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 47 f1, '「２」計画音声認識                  ' f2 from tversion union
select rnum, '「２」計画' || c1 from tlja where rnum = 47
) Q
group by
f1,f2
) Q ) P;
--- SelectJapaneseColumnLower_p1
select 'SelectJapaneseColumnLower_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 47 f1, '音声認識                  ' f2 from tversion union
select rnum, lower(c1) from tlja where rnum = 47
) Q
group by
f1,f2
) Q ) P;
--- SelectJapaneseColumnOrderByLocal_p1
select 'SelectJapaneseColumnOrderByLocal_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select  00000000000000000000000000000000000011 f1, '(1)ｲﾝﾃﾞｯｸｽ      ' f2 from tversion union
select  00000000000000000000000000000000000010 f1, '400ｒａｎｋｕ            ' f2 from tversion union
select  00000000000000000000000000000000000009 f1, '666Sink                       ' f2 from tversion union
select  00000000000000000000000000000000000014 f1, 'P-Cabels                      ' f2 from tversion union
select  00000000000000000000000000000000000013 f1, 'R-Bench                       ' f2 from tversion union
select  00000000000000000000000000000000000007 f1, '⑤号線路                  ' f2 from tversion union
select  00000000000000000000000000000000000008 f1, '「２」計画               ' f2 from tversion union
select  00000000000000000000000000000000000040 f1, 'さんしょう               ' f2 from tversion union
select  00000000000000000000000000000000000041 f1, 'ざぶと                     ' f2 from tversion union
select  00000000000000000000000000000000000037 f1, 'せっけい                  ' f2 from tversion union
select  00000000000000000000000000000000000036 f1, 'せんたくざい            ' f2 from tversion union
select  00000000000000000000000000000000000039 f1, 'はっぽ                     ' f2 from tversion union
select  00000000000000000000000000000000000038 f1, 'はつ剤                     ' f2 from tversion union
select  00000000000000000000000000000000000027 f1, 'エコー                     ' f2 from tversion union
select  00000000000000000000000000000000000026 f1, 'エチャント               ' f2 from tversion union
select  00000000000000000000000000000000000025 f1, 'ガード                     ' f2 from tversion union
select  00000000000000000000000000000000000028 f1, 'コート                     ' f2 from tversion union
select  00000000000000000000000000000000000029 f1, 'ゴム                        ' f2 from tversion union
select  00000000000000000000000000000000000030 f1, 'スワップ                  ' f2 from tversion union
select  00000000000000000000000000000000000031 f1, 'ズボン                     ' f2 from tversion union
select  00000000000000000000000000000000000032 f1, 'ダイエル                  ' f2 from tversion union
select  00000000000000000000000000000000000034 f1, 'ファイル                  ' f2 from tversion union
select  00000000000000000000000000000000000033 f1, 'フィルター               ' f2 from tversion union
select  00000000000000000000000000000000000035 f1, 'フッコク                  ' f2 from tversion union
select  00000000000000000000000000000000000050 f1, '国家利益                  ' f2 from tversion union
select  00000000000000000000000000000000000048 f1, '国立公園                  ' f2 from tversion union
select  00000000000000000000000000000000000049 f1, '国立大学                  ' f2 from tversion union
select  00000000000000000000000000000000000046 f1, '暗視                        ' f2 from tversion union
select  00000000000000000000000000000000000044 f1, '記載                        ' f2 from tversion union
select  00000000000000000000000000000000000043 f1, '記録機                     ' f2 from tversion union
select  00000000000000000000000000000000000047 f1, '音声認識                  ' f2 from tversion union
select  00000000000000000000000000000000000045 f1, '音楽                        ' f2 from tversion union
select  00000000000000000000000000000000000042 f1, '高機能                     ' f2 from tversion union
select  00000000000000000000000000000000000005 f1, '（Ⅰ）番号列            ' f2 from tversion union
select  00000000000000000000000000000000000001 f1, '３５６CAL                  ' f2 from tversion union
select  00000000000000000000000000000000000002 f1, '９８０Series               ' f2 from tversion union
select  00000000000000000000000000000000000006 f1, '＜ⅸ＞Pattern              ' f2 from tversion union
select  00000000000000000000000000000000000003 f1, 'ＰＶＤＦ                  ' f2 from tversion union
select  00000000000000000000000000000000000004 f1, 'ＲＯＭＡＮ-８           ' f2 from tversion union
select  00000000000000000000000000000000000015 f1, 'ｱﾝｶｰ                  ' f2 from tversion union
select  00000000000000000000000000000000000016 f1, 'ｴﾝｼﾞﾝ               ' f2 from tversion union
select  00000000000000000000000000000000000019 f1, 'ｶｯﾄﾏｼﾝ            ' f2 from tversion union
select  00000000000000000000000000000000000020 f1, 'ｶｰﾄﾞ                  ' f2 from tversion union
select  00000000000000000000000000000000000018 f1, 'ｺｰﾗ                     ' f2 from tversion union
select  00000000000000000000000000000000000017 f1, 'ｺﾞｰﾙﾄﾞ            ' f2 from tversion union
select  00000000000000000000000000000000000024 f1, 'ｻｲﾌ                     ' f2 from tversion union
select  00000000000000000000000000000000000021 f1, 'ﾂｰｳｨﾝｸﾞ         ' f2 from tversion union
select  00000000000000000000000000000000000023 f1, 'ﾌｫﾙﾀﾞｰ            ' f2 from tversion union
select  00000000000000000000000000000000000022 f1, 'ﾏﾝﾎﾞ                  ' f2 from tversion union
select rnum, c1 from tlja where rnum <> 12
) Q
group by
f1,f2
) Q ) P;
--- SelectJapaneseColumnWhere_p1
select 'SelectJapaneseColumnWhere_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 47 f1, '音声認識                                ' f2 from tversion union
select rnum, c1  from tlja where c1='音声認識'
) Q
group by
f1,f2
) Q ) P;
--- SelectJapaneseDistinctColumn_p1
select 'SelectJapaneseDistinctColumn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 50 f1 from tversion union
select count (distinct c1)  from tlja
) Q
group by
f1
) Q ) P;
--- SelectMaxApproxNumeric_p1
select 'SelectMaxApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
--- SelectMaxApproxNumeric_p2
select 'SelectMaxApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
--- SelectMaxApproxNumeric_p3
select 'SelectMaxApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchApproximateNumeric_p3
select 'CaseBasicSearchApproximateNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'other' f2 from tversion union
select 4 f1, 'other' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vdbl.rnum,case when vdbl.cdbl in ( -1,10,0.1 )  then 'test1' else 'other' end from vdbl
) Q
group by
f1,f2
) Q ) P;
--- SelectMaxApproxNumeric_p4
select 'SelectMaxApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
--- SelectMaxApproxNumeric_p5
select 'SelectMaxApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
--- SelectMaxApproxNumeric_p6
select 'SelectMaxApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
--- SelectMaxChar_p1
select 'SelectMaxChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'FF                              ' f1 from tversion union
select max( vchar.cchar ) from vchar
) Q
group by
f1
) Q ) P;
--- SelectMaxChar_p2
select 'SelectMaxChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'FF                              ' f1 from tversion union
select max( tchar.cchar ) from tchar
) Q
group by
f1
) Q ) P;
--- SelectMaxExactNumeric_p1
select 'SelectMaxExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
--- SelectMaxExactNumeric_p2
select 'SelectMaxExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
--- SelectMaxExactNumeric_p3
select 'SelectMaxExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
--- SelectMaxExactNumeric_p4
select 'SelectMaxExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
--- SelectMaxInt_p1
select 'SelectMaxInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchApproximateNumeric_p4
select 'CaseBasicSearchApproximateNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'other' f2 from tversion union
select 4 f1, 'other' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tdbl.rnum,case when tdbl.cdbl in ( -1,10,0.1 )  then 'test1' else 'other' end from tdbl
) Q
group by
f1,f2
) Q ) P;
--- SelectMaxInt_p2
select 'SelectMaxInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
--- SelectMaxInt_p3
select 'SelectMaxInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
--- SelectMaxInt_p4
select 'SelectMaxInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
--- SelectMaxInt_p5
select 'SelectMaxInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
--- SelectMaxInt_p6
select 'SelectMaxInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
--- SelectMaxLit_p1
select 'SelectMaxLit_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'A' f1 from tversion union
select max( 'A' ) from tversion
) Q
group by
f1
) Q ) P;
--- SelectMaxNullNumeric_p1
select 'SelectMaxNullNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select max( cnnull ) from tversion
) Q
group by
f1
) Q ) P;
--- SelectMaxNull_p1
select 'SelectMaxNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select max( ccnull ) from tversion
) Q
group by
f1
) Q ) P;
--- SelectMaxVarChar_p1
select 'SelectMaxVarChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'FF' f1 from tversion union
select max( vvchar.cvchar ) from vvchar
) Q
group by
f1
) Q ) P;
--- SelectMaxVarChar_p2
select 'SelectMaxVarChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'FF' f1 from tversion union
select max( tvchar.cvchar ) from tvchar
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchApproximateNumeric_p5
select 'CaseBasicSearchApproximateNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'other' f2 from tversion union
select 4 f1, 'other' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vrl.rnum,case when vrl.crl in ( -1,10,0.1 )  then 'test1' else 'other' end from vrl
) Q
group by
f1,f2
) Q ) P;
--- SelectMinApproxNumeric_p1
select 'SelectMinApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
--- SelectMinApproxNumeric_p2
select 'SelectMinApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
--- SelectMinApproxNumeric_p3
select 'SelectMinApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
--- SelectMinApproxNumeric_p4
select 'SelectMinApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
--- SelectMinApproxNumeric_p5
select 'SelectMinApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
--- SelectMinApproxNumeric_p6
select 'SelectMinApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
--- SelectMinChar_p1
select 'SelectMinChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '                                ' f1 from tversion union
select min( vchar.cchar ) from vchar
) Q
group by
f1
) Q ) P;
--- SelectMinChar_p2
select 'SelectMinChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '                                ' f1 from tversion union
select min( tchar.cchar ) from tchar
) Q
group by
f1
) Q ) P;
--- SelectMinExactNumeric_p1
select 'SelectMinExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
--- SelectMinExactNumeric_p2
select 'SelectMinExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchApproximateNumeric_p6
select 'CaseBasicSearchApproximateNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'other' f2 from tversion union
select 4 f1, 'other' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select trl.rnum,case when trl.crl in ( -1,10,0.1 )  then 'test1' else 'other' end from trl
) Q
group by
f1,f2
) Q ) P;
--- SelectMinExactNumeric_p3
select 'SelectMinExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
--- SelectMinExactNumeric_p4
select 'SelectMinExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
--- SelectMinInt_p1
select 'SelectMinInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
--- SelectMinInt_p2
select 'SelectMinInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
--- SelectMinInt_p3
select 'SelectMinInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
--- SelectMinInt_p4
select 'SelectMinInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
--- SelectMinInt_p5
select 'SelectMinInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
--- SelectMinInt_p6
select 'SelectMinInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
--- SelectMinLit_p1
select 'SelectMinLit_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'A' f1 from tversion union
select min( 'A' ) from tversion
) Q
group by
f1
) Q ) P;
--- SelectMinNullNumeric_p1
select 'SelectMinNullNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select min( cnnull ) from tversion
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchExactNumeric_p1
select 'CaseBasicSearchExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'other' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vdec.rnum,case when vdec.cdec in ( -1,10,0.1 )  then 'test1' else 'other' end from vdec
) Q
group by
f1,f2
) Q ) P;
--- SelectMinNull_p1
select 'SelectMinNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select min( ccnull ) from tversion
) Q
group by
f1
) Q ) P;
--- SelectMinVarChar_p1
select 'SelectMinVarChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select ' ' f1 from tversion union
select min( vvchar.cvchar ) from vvchar
) Q
group by
f1
) Q ) P;
--- SelectMinVarChar_p2
select 'SelectMinVarChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select ' ' f1 from tversion union
select min( tvchar.cvchar ) from tvchar
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopApproxNumeric_p1
select 'SelectStanDevPopApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopApproxNumeric_p2
select 'SelectStanDevPopApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopApproxNumeric_p3
select 'SelectStanDevPopApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopApproxNumeric_p4
select 'SelectStanDevPopApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopApproxNumeric_p5
select 'SelectStanDevPopApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopApproxNumeric_p6
select 'SelectStanDevPopApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopExactNumeric_p1
select 'SelectStanDevPopExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.04 f1 from tversion union
select stddev_pop( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
--- AbsCoreExactNumeric_p2
select 'AbsCoreExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 1.000000000000000e+000 f2 from tversion union
select 4 f1, 1.000000000000000e-001 f2 from tversion union
select 5 f1, 1.000000000000000e+001 f2 from tversion union
select rnum, abs( tdec.cdec ) from tdec
) Q
group by
f1,f2
) Q ) P;
--- CaseBasicSearchExactNumeric_p2
select 'CaseBasicSearchExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'other' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tdec.rnum,case when tdec.cdec in ( -1,10,0.1 )  then 'test1' else 'other' end from tdec
) Q
group by
f1,f2
) Q ) P;
--- SelectStanDevPopExactNumeric_p2
select 'SelectStanDevPopExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.04 f1 from tversion union
select stddev_pop( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopExactNumeric_p3
select 'SelectStanDevPopExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.04 f1 from tversion union
select stddev_pop( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopExactNumeric_p4
select 'SelectStanDevPopExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.04 f1 from tversion union
select stddev_pop( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopInt_p1
select 'SelectStanDevPopInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopInt_p2
select 'SelectStanDevPopInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopInt_p3
select 'SelectStanDevPopInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopInt_p4
select 'SelectStanDevPopInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopInt_p5
select 'SelectStanDevPopInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
--- SelectStanDevPopInt_p6
select 'SelectStanDevPopInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
--- SelectStar_p1
select 'SelectStar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 1 f2, '1.0   ' f3, null f4, null f5 from tversion union
select * from tversion
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
--- CaseBasicSearchExactNumeric_p3
select 'CaseBasicSearchExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'other' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vnum.rnum,case when vnum.cnum in ( -1,10,0.1 )  then 'test1' else 'other' end from vnum
) Q
group by
f1,f2
) Q ) P;
--- SelectSumApproxNumeric_p1
select 'SelectSumApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
--- SelectSumApproxNumeric_p2
select 'SelectSumApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
--- SelectSumApproxNumeric_p3
select 'SelectSumApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
--- SelectSumApproxNumeric_p4
select 'SelectSumApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
--- SelectSumApproxNumeric_p5
select 'SelectSumApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
--- SelectSumApproxNumeric_p6
select 'SelectSumApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
--- SelectSumExactNumeric_p1
select 'SelectSumExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.1 f1 from tversion union
select sum( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
--- SelectSumExactNumeric_p2
select 'SelectSumExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.1 f1 from tversion union
select sum( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
--- SelectSumExactNumeric_p3
select 'SelectSumExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.1 f1 from tversion union
select sum( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
--- SelectSumExactNumeric_p4
select 'SelectSumExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.1 f1 from tversion union
select sum( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchExactNumeric_p4
select 'CaseBasicSearchExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'other' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tnum.rnum,case when tnum.cnum in ( -1,10,0.1 )  then 'test1' else 'other' end from tnum
) Q
group by
f1,f2
) Q ) P;
--- SelectSumInt_p1
select 'SelectSumInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
--- SelectSumInt_p2
select 'SelectSumInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
--- SelectSumInt_p3
select 'SelectSumInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
--- SelectSumInt_p4
select 'SelectSumInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
--- SelectSumInt_p5
select 'SelectSumInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
--- SelectSumInt_p6
select 'SelectSumInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
--- SelectThaiColumnConcat_p1
select 'SelectThaiColumnConcat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 47 f1, '๛ก่ำ                               ' f2 from tversion union
select rnum, '๛' || c1 from tlth where rnum = 47
) Q
group by
f1,f2
) Q ) P;
--- SelectThaiColumnLower_p1
select 'SelectThaiColumnLower_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 47 f1, 'ก่ำ' f2 from tversion union
select rnum, lower(c1) from tlth where rnum=47
) Q
group by
f1,f2
) Q ) P;
--- SelectThaiColumnOrderByLocal_p1
select 'SelectThaiColumnOrderByLocal_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select  00000000000000000000000000000000000036 f1, ' ํ                                      ' f2 from tversion union
select  00000000000000000000000000000000000049 f1, '!                                       ' f2 from tversion union
select  00000000000000000000000000000000000020 f1, '-                                       ' f2 from tversion union
select  00000000000000000000000000000000000059 f1, '-กระจาม                                 ' f2 from tversion union
select  00000000000000000000000000000000000058 f1, '-กระจิ๋ง                                ' f2 from tversion union
select  00000000000000000000000000000000000061 f1, '-เกงกอย                                 ' f2 from tversion union
select  00000000000000000000000000000000000027 f1, '0                                       ' f2 from tversion union
select  00000000000000000000000000000000000003 f1, '00                                      ' f2 from tversion union
select  00000000000000000000000000000000000026 f1, '1                                       ' f2 from tversion union
select  00000000000000000000000000000000000057 f1, '9                                       ' f2 from tversion union
select  00000000000000000000000000000000000024 f1, 'Zulu                                    ' f2 from tversion union
select  00000000000000000000000000000000000025 f1, 'zulu                                    ' f2 from tversion union
select  00000000000000000000000000000000000054 f1, 'ก                                       ' f2 from tversion union
select  00000000000000000000000000000000000037 f1, 'ก กา                                    ' f2 from tversion union
select  00000000000000000000000000000000000074 f1, 'กก                                      ' f2 from tversion union
select  00000000000000000000000000000000000016 f1, 'กกขนาก                                  ' f2 from tversion union
select  00000000000000000000000000000000000035 f1, 'กกา                                     ' f2 from tversion union
select  00000000000000000000000000000000000034 f1, 'กกๅ                                     ' f2 from tversion union
select  00000000000000000000000000000000000066 f1, 'กง                                      ' f2 from tversion union
select  00000000000000000000000000000000000001 f1, 'กฏิ                                     ' f2 from tversion union
select  00000000000000000000000000000000000015 f1, 'กฏุก                                    ' f2 from tversion union
select  00000000000000000000000000000000000070 f1, 'กฏุก-                                   ' f2 from tversion union
select  00000000000000000000000000000000000002 f1, 'กรรมสิทธิ์ผู้แต่งหนังสือ                ' f2 from tversion union
select  00000000000000000000000000000000000010 f1, 'กรรมสิทธิ์เครื่องหมายและยี่ห้อการค้าขาย ' f2 from tversion union
select  00000000000000000000000000000000000031 f1, 'กระจาบ                                  ' f2 from tversion union
select  00000000000000000000000000000000000055 f1, 'กระจาย                                  ' f2 from tversion union
select  00000000000000000000000000000000000030 f1, 'กระจิด                                  ' f2 from tversion union
select  00000000000000000000000000000000000014 f1, 'กัง                                     ' f2 from tversion union
select  00000000000000000000000000000000000052 f1, 'กังก                                    ' f2 from tversion union
select  00000000000000000000000000000000000042 f1, 'กั้ง                                    ' f2 from tversion union
select  00000000000000000000000000000000000064 f1, 'กำ                                      ' f2 from tversion union
select  00000000000000000000000000000000000041 f1, 'กิก                                     ' f2 from tversion union
select  00000000000000000000000000000000000012 f1, 'กิ่ง                                    ' f2 from tversion union
select  00000000000000000000000000000000000021 f1, 'กิ๊ก                                    ' f2 from tversion union
select  00000000000000000000000000000000000008 f1, 'กู้หน้า                                 ' f2 from tversion union
select  00000000000000000000000000000000000051 f1, 'ก็                                      ' f2 from tversion union
select  00000000000000000000000000000000000022 f1, 'ก่ง                                     ' f2 from tversion union
select  00000000000000000000000000000000000047 f1, 'ก่ำ                                     ' f2 from tversion union
select  00000000000000000000000000000000000067 f1, 'ก้ง                                     ' f2 from tversion union
select  00000000000000000000000000000000000004 f1, 'ก้งง                                    ' f2 from tversion union
select  00000000000000000000000000000000000073 f1, 'ก้ำ                                     ' f2 from tversion union
select  00000000000000000000000000000000000023 f1, 'ก๊ก                                     ' f2 from tversion union
select  00000000000000000000000000000000000069 f1, 'ก๊ง                                     ' f2 from tversion union
select  00000000000000000000000000000000000075 f1, 'ขง                                      ' f2 from tversion union
select  00000000000000000000000000000000000065 f1, 'ฃ                                       ' f2 from tversion union
select  00000000000000000000000000000000000046 f1, 'ค                                       ' f2 from tversion union
select  00000000000000000000000000000000000060 f1, 'คคน-                                    ' f2 from tversion union
select  00000000000000000000000000000000000013 f1, 'ฯ                                       ' f2 from tversion union
select  00000000000000000000000000000000000039 f1, 'เ                                       ' f2 from tversion union
select  00000000000000000000000000000000000006 f1, 'เก                                      ' f2 from tversion union
select  00000000000000000000000000000000000050 f1, 'เกน                                     ' f2 from tversion union
select  00000000000000000000000000000000000005 f1, 'เกนๆ                                    ' f2 from tversion union
select  00000000000000000000000000000000000011 f1, 'เก็บ                                    ' f2 from tversion union
select  00000000000000000000000000000000000043 f1, 'เก่                                     ' f2 from tversion union
select  00000000000000000000000000000000000044 f1, 'เก่น                                    ' f2 from tversion union
select  00000000000000000000000000000000000019 f1, 'แก                                      ' f2 from tversion union
select  00000000000000000000000000000000000072 f1, 'แก่                                     ' f2 from tversion union
select  00000000000000000000000000000000000056 f1, 'แก่กล้า                                 ' f2 from tversion union
select  00000000000000000000000000000000000068 f1, 'แก้                                     ' f2 from tversion union
select  00000000000000000000000000000000000053 f1, 'โก่                                     ' f2 from tversion union
select  00000000000000000000000000000000000045 f1, 'ใกล้                                    ' f2 from tversion union
select  00000000000000000000000000000000000032 f1, 'ไ                                       ' f2 from tversion union
select  00000000000000000000000000000000000028 f1, 'ไก                                      ' f2 from tversion union
select  00000000000000000000000000000000000007 f1, 'ไฮฮี                                    ' f2 from tversion union
select  00000000000000000000000000000000000048 f1, 'ๆ                                       ' f2 from tversion union
select  00000000000000000000000000000000000071 f1, '๏                                       ' f2 from tversion union
select  00000000000000000000000000000000000018 f1, '๐                                       ' f2 from tversion union
select  00000000000000000000000000000000000063 f1, '๐๐                                      ' f2 from tversion union
select  00000000000000000000000000000000000017 f1, '๐๙                                      ' f2 from tversion union
select  00000000000000000000000000000000000009 f1, '๑                                       ' f2 from tversion union
select  00000000000000000000000000000000000029 f1, '๒                                       ' f2 from tversion union
select  00000000000000000000000000000000000040 f1, '๘                                       ' f2 from tversion union
select  00000000000000000000000000000000000033 f1, '๙                                       ' f2 from tversion union
select  00000000000000000000000000000000000062 f1, '๛                                       ' f2 from tversion union
select rnum, c1 from tlth where rnum <> 38
) Q
group by
f1,f2
) Q ) P;
--- SelectThaiColumnWhere_p1
select 'SelectThaiColumnWhere_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 62 f1, '๛                                     ' f2 from tversion union
select rnum, c1  from tlth where c1='๛'
) Q
group by
f1,f2
) Q ) P;
--- CaseBasicSearchInteger_p1
select 'CaseBasicSearchInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'test1' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vint.rnum,case when vint.cint in ( -1,10,1 )  then 'test1' else 'other' end from vint
) Q
group by
f1,f2
) Q ) P;
--- SelectThaiDistinctColumn_p1
select 'SelectThaiDistinctColumn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 75 f1 from tversion union
select count (distinct c1)  from tlth
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonEqualTo_p1
select 'SelectTimeComparisonEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '10:20:30' = time '10:20:30'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonGreaterThanOrEqualTo_p1
select 'SelectTimeComparisonGreaterThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' >= time '00:00:00.000'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonGreaterThanOrEqualTo_p2
select 'SelectTimeComparisonGreaterThanOrEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' >= time '12:00:00.000'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonGreaterThanOrEqualTo_p3
select 'SelectTimeComparisonGreaterThanOrEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' >= time '23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonGreaterThan_p1
select 'SelectTimeComparisonGreaterThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' > time '00:00:00.000'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonGreaterThan_p2
select 'SelectTimeComparisonGreaterThan_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' > time '12:00:00.000'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonGreaterThan_p3
select 'SelectTimeComparisonGreaterThan_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' > time '23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonLessThanOrEqualTo_p1
select 'SelectTimeComparisonLessThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '00:00:00' <= time '00:00:00.000'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonLessThanOrEqualTo_p2
select 'SelectTimeComparisonLessThanOrEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '00:00:00' <= time '12:00:00.000'
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchInteger_p2
select 'CaseBasicSearchInteger_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'test1' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tint.rnum,case when tint.cint in ( -1,10,1 )  then 'test1' else 'other' end from tint
) Q
group by
f1,f2
) Q ) P;
--- SelectTimeComparisonLessThanOrEqualTo_p3
select 'SelectTimeComparisonLessThanOrEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '00:00:00' <= time '23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonLessThan_p1
select 'SelectTimeComparisonLessThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '00:00:00.000' < time '23:59:40'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonLessThan_p2
select 'SelectTimeComparisonLessThan_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '12:00:00.000' < time '23:59:40'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonLessThan_p3
select 'SelectTimeComparisonLessThan_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:30.123' < time '23:59:40'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonNotEqualTo_p1
select 'SelectTimeComparisonNotEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '22:20:30' <> time '00:00:00.000'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonNotEqualTo_p2
select 'SelectTimeComparisonNotEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '22:20:30' <> time '12:00:00.000'
) Q
group by
f1
) Q ) P;
--- SelectTimeComparisonNotEqualTo_p3
select 'SelectTimeComparisonNotEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '22:20:30' <> time '23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonEqualTo_p1
select 'SelectTimestampComparisonEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-01-01 00:00:00.0' = timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonEqualTo_p2
select 'SelectTimestampComparisonEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-01-01 12:00:00' = timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonEqualTo_p3
select 'SelectTimestampComparisonEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-01-01 23:59:30.123' = timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchInteger_p3
select 'CaseBasicSearchInteger_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'test1' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vsint.rnum,case when vsint.csint in ( -1,10,1 )  then 'test1' else 'other' end from vsint
) Q
group by
f1,f2
) Q ) P;
--- SelectTimestampComparisonEqualTo_p4
select 'SelectTimestampComparisonEqualTo_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-12-31 00:00:00' = timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonEqualTo_p5
select 'SelectTimestampComparisonEqualTo_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-12-31 12:00:00' = timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonEqualTo_p6
select 'SelectTimestampComparisonEqualTo_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-12-31 23:59:30.123' = timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThanOrEqualTo_p1
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThanOrEqualTo_p2
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThanOrEqualTo_p3
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThanOrEqualTo_p4
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThanOrEqualTo_p5
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThanOrEqualTo_p6
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThan_p1
select 'SelectTimestampComparisonGreaterThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchInteger_p4
select 'CaseBasicSearchInteger_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'test1' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tsint.rnum,case when tsint.csint in ( -1,10,1 )  then 'test1' else 'other' end from tsint
) Q
group by
f1,f2
) Q ) P;
--- SelectTimestampComparisonGreaterThan_p2
select 'SelectTimestampComparisonGreaterThan_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThan_p3
select 'SelectTimestampComparisonGreaterThan_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThan_p4
select 'SelectTimestampComparisonGreaterThan_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThan_p5
select 'SelectTimestampComparisonGreaterThan_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonGreaterThan_p6
select 'SelectTimestampComparisonGreaterThan_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThanOrEqualTo_p1
select 'SelectTimestampComparisonLessThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThanOrEqualTo_p2
select 'SelectTimestampComparisonLessThanOrEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThanOrEqualTo_p3
select 'SelectTimestampComparisonLessThanOrEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThanOrEqualTo_p4
select 'SelectTimestampComparisonLessThanOrEqualTo_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThanOrEqualTo_p5
select 'SelectTimestampComparisonLessThanOrEqualTo_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchInteger_p5
select 'CaseBasicSearchInteger_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'test1' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vbint.rnum,case when vbint.cbint in ( -1,10,1 )  then 'test1' else 'other' end from vbint
) Q
group by
f1,f2
) Q ) P;
--- SelectTimestampComparisonLessThanOrEqualTo_p6
select 'SelectTimestampComparisonLessThanOrEqualTo_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThan_p1
select 'SelectTimestampComparisonLessThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThan_p2
select 'SelectTimestampComparisonLessThan_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThan_p3
select 'SelectTimestampComparisonLessThan_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThan_p4
select 'SelectTimestampComparisonLessThan_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThan_p5
select 'SelectTimestampComparisonLessThan_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonLessThan_p6
select 'SelectTimestampComparisonLessThan_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonNotEqualTo_p1
select 'SelectTimestampComparisonNotEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonNotEqualTo_p2
select 'SelectTimestampComparisonNotEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonNotEqualTo_p3
select 'SelectTimestampComparisonNotEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- CaseBasicSearchInteger_p6
select 'CaseBasicSearchInteger_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'test1' f2 from tversion union
select 2 f1, 'other' f2 from tversion union
select 3 f1, 'test1' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tbint.rnum,case when tbint.cbint in ( -1,10,1 )  then 'test1' else 'other' end from tbint
) Q
group by
f1,f2
) Q ) P;
--- SelectTimestampComparisonNotEqualTo_p4
select 'SelectTimestampComparisonNotEqualTo_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonNotEqualTo_p5
select 'SelectTimestampComparisonNotEqualTo_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
--- SelectTimestampComparisonNotEqualTo_p6
select 'SelectTimestampComparisonNotEqualTo_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
--- SelectTurkishColumnConcat_p1
select 'SelectTurkishColumnConcat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select  00000000000000000000000000000000000001 f1, 'Ã§Ä±kmak' f2 from tversion union
select  00000000000000000000000000000000000002 f1, 'Ã§Ä±kmakZero                          ' f2 from tversion union
select  00000000000000000000000000000000000003 f1, 'Ã§Ä±kmakÜyelik                       ' f2 from tversion union
select  00000000000000000000000000000000000004 f1, 'Ã§Ä±kmaküyelik                       ' f2 from tversion union
select  00000000000000000000000000000000000005 f1, 'Ã§Ä±kmakÜyeleri                      ' f2 from tversion union
select  00000000000000000000000000000000000006 f1, 'Ã§Ä±kmakUzantısı                    ' f2 from tversion union
select  00000000000000000000000000000000000007 f1, 'Ã§Ä±kmakupdate                        ' f2 from tversion union
select  00000000000000000000000000000000000008 f1, 'Ã§Ä±kmakŞarkı                       ' f2 from tversion union
select  00000000000000000000000000000000000009 f1, 'Ã§Ä±kmakC.B.A.                        ' f2 from tversion union
select  00000000000000000000000000000000000010 f1, 'Ã§Ä±kmakşifreleme                    ' f2 from tversion union
select  00000000000000000000000000000000000011 f1, 'Ã§Ä±kmakstep                          ' f2 from tversion union
select  00000000000000000000000000000000000012 f1, 'Ã§Ä±kmaközellikler                   ' f2 from tversion union
select  00000000000000000000000000000000000013 f1, 'Ã§Ä±kmak@@@air                        ' f2 from tversion union
select  00000000000000000000000000000000000014 f1, 'Ã§Ä±kmakoption                        ' f2 from tversion union
select  00000000000000000000000000000000000015 f1, 'Ã§Ä±kmakvice-versa                    ' f2 from tversion union
select  00000000000000000000000000000000000016 f1, 'Ã§Ä±kmakÖlçer                       ' f2 from tversion union
select  00000000000000000000000000000000000017 f1, 'Ã§Ä±kmakvice-admiral                  ' f2 from tversion union
select  00000000000000000000000000000000000018 f1, 'Ã§Ä±kmakIpucu                         ' f2 from tversion union
select  00000000000000000000000000000000000019 f1, 'Ã§Ä±kmakIP                            ' f2 from tversion union
select  00000000000000000000000000000000000020 f1, 'Ã§Ä±kmak999                           ' f2 from tversion union
select  00000000000000000000000000000000000021 f1, 'Ã§Ä±kmakdiğer                        ' f2 from tversion union
select  00000000000000000000000000000000000022 f1, 'Ã§Ä±kmakıptali                       ' f2 from tversion union
select  00000000000000000000000000000000000023 f1, 'Ã§Ä±kmakicon                          ' f2 from tversion union
select  00000000000000000000000000000000000024 f1, 'Ã§Ä±kmakair@@@                        ' f2 from tversion union
select  00000000000000000000000000000000000025 f1, 'Ã§Ä±kmakCO-OP                         ' f2 from tversion union
select  00000000000000000000000000000000000026 f1, 'Ã§Ä±kmakİsteği                      ' f2 from tversion union
select  00000000000000000000000000000000000027 f1, 'Ã§Ä±kmakItem                          ' f2 from tversion union
select  00000000000000000000000000000000000028 f1, 'Ã§Ä±kmakhub                           ' f2 from tversion union
select  00000000000000000000000000000000000029 f1, 'Ã§Ä±kmakvice                          ' f2 from tversion union
select  00000000000000000000000000000000000030 f1, 'Ã§Ä±kmakHata                          ' f2 from tversion union
select  00000000000000000000000000000000000031 f1, 'Ã§Ä±kmakCOOP                          ' f2 from tversion union
select  00000000000000000000000000000000000032 f1, 'Ã§Ä±kmakvice versa                    ' f2 from tversion union
select  00000000000000000000000000000000000033 f1, 'Ã§Ä±kmakdigit                         ' f2 from tversion union
select  00000000000000000000000000000000000034 f1, 'Ã§Ä±kmakCzech                         ' f2 from tversion union
select  00000000000000000000000000000000000035 f1, 'Ã§Ä±kmakçıkmak                      ' f2 from tversion union
select  00000000000000000000000000000000000036 f1, 'Ã§Ä±kmakçevir                        ' f2 from tversion union
select  00000000000000000000000000000000000037 f1, 'Ã§Ä±kmakÇok                          ' f2 from tversion union
select  00000000000000000000000000000000000038 f1, 'Ã§Ä±kmakçoklu                        ' f2 from tversion union
select  00000000000000000000000000000000000039 f1, 'Ã§Ä±kmakvicennial                     ' f2 from tversion union
select  00000000000000000000000000000000000040 f1, 'Ã§Ä±kmakçizim                        ' f2 from tversion union
select  00000000000000000000000000000000000041 f1, 'Ã§Ä±kmakco-op                         ' f2 from tversion union
select  00000000000000000000000000000000000042 f1, 'Ã§Ä±kmakçizgiler                     ' f2 from tversion union
select  00000000000000000000000000000000000043 f1, 'Ã§Ä±kmakçizgi                        ' f2 from tversion union
select  00000000000000000000000000000000000044 f1, 'Ã§Ä±kmak@@@@@                         ' f2 from tversion union
select  00000000000000000000000000000000000045 f1, 'Ã§Ä±kmakçift                         ' f2 from tversion union
select  00000000000000000000000000000000000046 f1, 'Ã§Ä±kmakverkehrt                      ' f2 from tversion union
select  00000000000000000000000000000000000047 f1, 'Ã§Ä±kmakçapraz                       ' f2 from tversion union
select  00000000000000000000000000000000000048 f1, 'Ã§Ä±kmak0000                          ' f2 from tversion union
select  00000000000000000000000000000000000049 f1, 'Ã§Ä±kmakcaption                       ' f2 from tversion union
select  00000000000000000000000000000000000050 f1, 'Ã§Ä±kmakCable                         ' f2 from tversion union
select  00000000000000000000000000000000000051 f1, 'Ã§Ä±kmakcable                         ' f2 from tversion union
select  00000000000000000000000000000000000052 f1, 'Ã§Ä±kmak' f2 from tversion union
select rnum, 'Ã§Ä±kmak' || c1 from tltr
) Q
group by
f1,f2
) Q ) P;
--- SelectTurkishColumnLower_p1
select 'SelectTurkishColumnLower_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select  00000000000000000000000000000000000001 f1, null f2 from tversion union
select  00000000000000000000000000000000000002 f1, 'zero                          ' f2 from tversion union
select  00000000000000000000000000000000000003 f1, 'üyelik                       ' f2 from tversion union
select  00000000000000000000000000000000000004 f1, 'üyelik                       ' f2 from tversion union
select  00000000000000000000000000000000000005 f1, 'üyeleri                      ' f2 from tversion union
select  00000000000000000000000000000000000006 f1, 'uzantısı                    ' f2 from tversion union
select  00000000000000000000000000000000000007 f1, 'update                        ' f2 from tversion union
select  00000000000000000000000000000000000008 f1, 'şarkı                       ' f2 from tversion union
select  00000000000000000000000000000000000009 f1, 'c.b.a.                        ' f2 from tversion union
select  00000000000000000000000000000000000010 f1, 'şifreleme                    ' f2 from tversion union
select  00000000000000000000000000000000000011 f1, 'step                          ' f2 from tversion union
select  00000000000000000000000000000000000012 f1, 'özellikler                   ' f2 from tversion union
select  00000000000000000000000000000000000013 f1, '@@@air                        ' f2 from tversion union
select  00000000000000000000000000000000000014 f1, 'option                        ' f2 from tversion union
select  00000000000000000000000000000000000015 f1, 'vice-versa                    ' f2 from tversion union
select  00000000000000000000000000000000000016 f1, 'ölçer                       ' f2 from tversion union
select  00000000000000000000000000000000000017 f1, 'vice-admiral                  ' f2 from tversion union
select  00000000000000000000000000000000000018 f1, 'ipucu                         ' f2 from tversion union
select  00000000000000000000000000000000000019 f1, 'ip                            ' f2 from tversion union
select  00000000000000000000000000000000000020 f1, '999                           ' f2 from tversion union
select  00000000000000000000000000000000000021 f1, 'diğer                        ' f2 from tversion union
select  00000000000000000000000000000000000022 f1, 'ıptali                       ' f2 from tversion union
select  00000000000000000000000000000000000023 f1, 'icon                          ' f2 from tversion union
select  00000000000000000000000000000000000024 f1, 'air@@@                        ' f2 from tversion union
select  00000000000000000000000000000000000025 f1, 'co-op                         ' f2 from tversion union
select  00000000000000000000000000000000000026 f1, 'isteği                      ' f2 from tversion union
select  00000000000000000000000000000000000027 f1, 'item                          ' f2 from tversion union
select  00000000000000000000000000000000000028 f1, 'hub                           ' f2 from tversion union
select  00000000000000000000000000000000000029 f1, 'vice                          ' f2 from tversion union
select  00000000000000000000000000000000000030 f1, 'hata                          ' f2 from tversion union
select  00000000000000000000000000000000000031 f1, 'coop                          ' f2 from tversion union
select  00000000000000000000000000000000000032 f1, 'vice versa                    ' f2 from tversion union
select  00000000000000000000000000000000000033 f1, 'digit                         ' f2 from tversion union
select  00000000000000000000000000000000000034 f1, 'czech                         ' f2 from tversion union
select  00000000000000000000000000000000000035 f1, 'çıkmak                      ' f2 from tversion union
select  00000000000000000000000000000000000036 f1, 'çevir                        ' f2 from tversion union
select  00000000000000000000000000000000000037 f1, 'çok                          ' f2 from tversion union
select  00000000000000000000000000000000000038 f1, 'çoklu                        ' f2 from tversion union
select  00000000000000000000000000000000000039 f1, 'vicennial                     ' f2 from tversion union
select  00000000000000000000000000000000000040 f1, 'çizim                        ' f2 from tversion union
select  00000000000000000000000000000000000041 f1, 'co-op                         ' f2 from tversion union
select  00000000000000000000000000000000000042 f1, 'çizgiler                     ' f2 from tversion union
select  00000000000000000000000000000000000043 f1, 'çizgi                        ' f2 from tversion union
select  00000000000000000000000000000000000044 f1, '@@@@@                         ' f2 from tversion union
select  00000000000000000000000000000000000045 f1, 'çift                         ' f2 from tversion union
select  00000000000000000000000000000000000046 f1, 'verkehrt                      ' f2 from tversion union
select  00000000000000000000000000000000000047 f1, 'çapraz                       ' f2 from tversion union
select  00000000000000000000000000000000000048 f1, '0000                          ' f2 from tversion union
select  00000000000000000000000000000000000049 f1, 'caption                       ' f2 from tversion union
select  00000000000000000000000000000000000050 f1, 'cable                         ' f2 from tversion union
select  00000000000000000000000000000000000051 f1, 'cable                         ' f2 from tversion union
select  00000000000000000000000000000000000052 f1, '' f2 from tversion union
select rnum, lower(c1) from tltr
) Q
group by
f1,f2
) Q ) P;
--- SelectTurkishColumnOrderByLocal_p1
select 'SelectTurkishColumnOrderByLocal_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select  00000000000000000000000000000000000048 f1, '0000                          ' f2 from tversion union
select  00000000000000000000000000000000000020 f1, '999                           ' f2 from tversion union
select  00000000000000000000000000000000000044 f1, '@@@@@                         ' f2 from tversion union
select  00000000000000000000000000000000000013 f1, '@@@air                        ' f2 from tversion union
select  00000000000000000000000000000000000009 f1, 'C.B.A.                        ' f2 from tversion union
select  00000000000000000000000000000000000025 f1, 'CO-OP                         ' f2 from tversion union
select  00000000000000000000000000000000000031 f1, 'COOP                          ' f2 from tversion union
select  00000000000000000000000000000000000050 f1, 'Cable                         ' f2 from tversion union
select  00000000000000000000000000000000000034 f1, 'Czech                         ' f2 from tversion union
select  00000000000000000000000000000000000030 f1, 'Hata                          ' f2 from tversion union
select  00000000000000000000000000000000000019 f1, 'IP                            ' f2 from tversion union
select  00000000000000000000000000000000000018 f1, 'Ipucu                         ' f2 from tversion union
select  00000000000000000000000000000000000027 f1, 'Item                          ' f2 from tversion union
select  00000000000000000000000000000000000006 f1, 'Uzantısı                    ' f2 from tversion union
select  00000000000000000000000000000000000002 f1, 'Zero                          ' f2 from tversion union
select  00000000000000000000000000000000000024 f1, 'air@@@                        ' f2 from tversion union
select  00000000000000000000000000000000000051 f1, 'cable                         ' f2 from tversion union
select  00000000000000000000000000000000000049 f1, 'caption                       ' f2 from tversion union
select  00000000000000000000000000000000000041 f1, 'co-op                         ' f2 from tversion union
select  00000000000000000000000000000000000033 f1, 'digit                         ' f2 from tversion union
select  00000000000000000000000000000000000021 f1, 'diğer                        ' f2 from tversion union
select  00000000000000000000000000000000000028 f1, 'hub                           ' f2 from tversion union
select  00000000000000000000000000000000000023 f1, 'icon                          ' f2 from tversion union
select  00000000000000000000000000000000000014 f1, 'option                        ' f2 from tversion union
select  00000000000000000000000000000000000011 f1, 'step                          ' f2 from tversion union
select  00000000000000000000000000000000000007 f1, 'update                        ' f2 from tversion union
select  00000000000000000000000000000000000046 f1, 'verkehrt                      ' f2 from tversion union
select  00000000000000000000000000000000000029 f1, 'vice                          ' f2 from tversion union
select  00000000000000000000000000000000000032 f1, 'vice versa                    ' f2 from tversion union
select  00000000000000000000000000000000000017 f1, 'vice-admiral                  ' f2 from tversion union
select  00000000000000000000000000000000000015 f1, 'vice-versa                    ' f2 from tversion union
select  00000000000000000000000000000000000039 f1, 'vicennial                     ' f2 from tversion union
select  00000000000000000000000000000000000037 f1, 'Çok                          ' f2 from tversion union
select  00000000000000000000000000000000000016 f1, 'Ölçer                       ' f2 from tversion union
select  00000000000000000000000000000000000005 f1, 'Üyeleri                      ' f2 from tversion union
select  00000000000000000000000000000000000003 f1, 'Üyelik                       ' f2 from tversion union
select  00000000000000000000000000000000000047 f1, 'çapraz                       ' f2 from tversion union
select  00000000000000000000000000000000000036 f1, 'çevir                        ' f2 from tversion union
select  00000000000000000000000000000000000045 f1, 'çift                         ' f2 from tversion union
select  00000000000000000000000000000000000043 f1, 'çizgi                        ' f2 from tversion union
select  00000000000000000000000000000000000042 f1, 'çizgiler                     ' f2 from tversion union
select  00000000000000000000000000000000000040 f1, 'çizim                        ' f2 from tversion union
select  00000000000000000000000000000000000038 f1, 'çoklu                        ' f2 from tversion union
select  00000000000000000000000000000000000035 f1, 'çıkmak                      ' f2 from tversion union
select  00000000000000000000000000000000000012 f1, 'özellikler                   ' f2 from tversion union
select  00000000000000000000000000000000000004 f1, 'üyelik                       ' f2 from tversion union
select  00000000000000000000000000000000000026 f1, 'İsteği                      ' f2 from tversion union
select  00000000000000000000000000000000000022 f1, 'ıptali                       ' f2 from tversion union
select  00000000000000000000000000000000000008 f1, 'Şarkı                       ' f2 from tversion union
select  00000000000000000000000000000000000010 f1, 'şifreleme                    ' f2 from tversion union
select  00000000000000000000000000000000000052 f1, null f2 from tversion union
select  00000000000000000000000000000000000001 f1, null f2 from tversion union
select rnum, c1 from tltr
) Q
group by
f1,f2
) Q ) P;
--- SelectTurkishColumnWhere_p1
select 'SelectTurkishColumnWhere_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 35 f1, 'çıkmak                                ' f2 from tversion union
select rnum, c1  from tltr where c1='çıkmak'
) Q
group by
f1,f2
) Q ) P;
--- SelectTurkishDistinctColumn_p1
select 'SelectTurkishDistinctColumn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 51 f1 from tversion union
select count (distinct c1)  from tltr
) Q
group by
f1
) Q ) P;
--- SelectVarPopApproxNumeric_p1
select 'SelectVarPopApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
--- SelectVarPopApproxNumeric_p2
select 'SelectVarPopApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
--- CaseComparisonsApproximateNumeric_p1
select 'CaseComparisonsApproximateNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'between' f2 from tversion union
select 5 f1, 'gt' f2 from tversion union
select vflt.rnum,case  when vflt.cflt =  1  then '=' when vflt.cflt >  9  then 'gt' when vflt.cflt < -0.2 then 'lt' when vflt.cflt in  (0,11)  then 'in' when vflt.cflt between -1 and 0  then 'between' else 'other' end from vflt
) Q
group by
f1,f2
) Q ) P;
--- SelectVarPopApproxNumeric_p3
select 'SelectVarPopApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
--- SelectVarPopApproxNumeric_p4
select 'SelectVarPopApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
--- SelectVarPopApproxNumeric_p5
select 'SelectVarPopApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
--- SelectVarPopApproxNumeric_p6
select 'SelectVarPopApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
--- SelectVarPopExactNumeric_p1
select 'SelectVarPopExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.3216 f1 from tversion union
select var_pop( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
--- SelectVarPopExactNumeric_p2
select 'SelectVarPopExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.3216 f1 from tversion union
select var_pop( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
--- SelectVarPopExactNumeric_p3
select 'SelectVarPopExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.3216 f1 from tversion union
select var_pop( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
--- SelectVarPopExactNumeric_p4
select 'SelectVarPopExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.3216 f1 from tversion union
select var_pop( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
--- SelectVarPopInt_p1
select 'SelectVarPopInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
--- SelectVarPopInt_p2
select 'SelectVarPopInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
--- AbsCoreExactNumeric_p3
select 'AbsCoreExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 1.000000000000000e+000 f2 from tversion union
select 2 f1, 0.000000000000000e+000 f2 from tversion union
select 3 f1, 1.000000000000000e+000 f2 from tversion union
select 4 f1, 1.000000000000000e-001 f2 from tversion union
select 5 f1, 1.000000000000000e+001 f2 from tversion union
select rnum, abs( vnum.cnum ) from vnum
) Q
group by
f1,f2
) Q ) P;
--- CaseComparisonsApproximateNumeric_p2
select 'CaseComparisonsApproximateNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'between' f2 from tversion union
select 5 f1, 'gt' f2 from tversion union
select tflt.rnum,case  when tflt.cflt =  1  then '=' when tflt.cflt >  9  then 'gt' when tflt.cflt < -0.2 then 'lt' when tflt.cflt in  (0,11)  then 'in' when tflt.cflt between -1 and 0  then 'between' else 'other' end from tflt
) Q
group by
f1,f2
) Q ) P;
--- SelectVarPopInt_p3
select 'SelectVarPopInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
--- SelectVarPopInt_p4
select 'SelectVarPopInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
--- SelectVarPopInt_p5
select 'SelectVarPopInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
--- SelectVarPopInt_p6
select 'SelectVarPopInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
--- SetPrecedenceNoBrackets_p1
select 'SetPrecedenceNoBrackets_p1' test_name_part, case when d = 1 then 1 else 0 end pass_ind from (
select count(distinct d) d from (
select t,f1,c,count(*) d from (
select t, f1, count(*) c  from (
select 'X' T, 10 f1 from tversion union all
select 'X' T, 10 f1 from tversion union all
select 'X' T,40 f1 from tversion union all
select 'X' T,50 f1 from tversion union all
select 'X' T,60 f1 from tversion union all
select 'A', c1 from tset1 intersect select 'A', c1 from tset2 union all select 'A', c1 from tset3
) Q
group by
t, f1
) P
group by t, f1, c
) O
) N;
--- SetPrecedenceUnionFirst_p1
select 'SetPrecedenceUnionFirst_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select 40 f1 from tversion union
select 50 f1 from tversion union
select 60 f1 from tversion union
select c1 from tset1 intersect (select c1 from tset2 union all select c1 from tset3)
) Q
group by
f1
) Q ) P;
--- SimpleCaseApproximateNumericElseDefaultsNULL_p1
select 'SimpleCaseApproximateNumericElseDefaultsNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vflt.rnum,case when vflt.cflt > 1 then 'test1' when vflt.cflt < 0 then 'test2' end from vflt
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseDefaultsNULL_p2
select 'SimpleCaseApproximateNumericElseDefaultsNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tflt.rnum,case when tflt.cflt > 1 then 'test1' when tflt.cflt < 0 then 'test2' end from tflt
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseDefaultsNULL_p3
select 'SimpleCaseApproximateNumericElseDefaultsNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vdbl.rnum,case when vdbl.cdbl > 1 then 'test1' when vdbl.cdbl < 0 then 'test2' end from vdbl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseDefaultsNULL_p4
select 'SimpleCaseApproximateNumericElseDefaultsNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tdbl.rnum,case when tdbl.cdbl > 1 then 'test1' when tdbl.cdbl < 0 then 'test2' end from tdbl
) Q
group by
f1,f2
) Q ) P;
--- CaseComparisonsApproximateNumeric_p3
select 'CaseComparisonsApproximateNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'between' f2 from tversion union
select 5 f1, 'gt' f2 from tversion union
select vdbl.rnum,case  when vdbl.cdbl =  1  then '=' when vdbl.cdbl >  9  then 'gt' when vdbl.cdbl < -0.2 then 'lt' when vdbl.cdbl in  (0,11)  then 'in' when vdbl.cdbl between -1 and 0  then 'between' else 'other' end from vdbl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseDefaultsNULL_p5
select 'SimpleCaseApproximateNumericElseDefaultsNULL_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vrl.rnum,case when vrl.crl > 1 then 'test1' when vrl.crl < 0 then 'test2' end from vrl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseDefaultsNULL_p6
select 'SimpleCaseApproximateNumericElseDefaultsNULL_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select trl.rnum,case when trl.crl > 1 then 'test1' when trl.crl < 0 then 'test2' end from trl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseExplicitNULL_p1
select 'SimpleCaseApproximateNumericElseExplicitNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vflt.rnum,case when vflt.cflt > 1 then 'test1' when vflt.cflt < 0 then 'test2' else null end from vflt
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseExplicitNULL_p2
select 'SimpleCaseApproximateNumericElseExplicitNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tflt.rnum,case when tflt.cflt > 1 then 'test1' when tflt.cflt < 0 then 'test2' else null end from tflt
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseExplicitNULL_p3
select 'SimpleCaseApproximateNumericElseExplicitNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vdbl.rnum,case when vdbl.cdbl > 1 then 'test1' when vdbl.cdbl < 0 then 'test2' else null end from vdbl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseExplicitNULL_p4
select 'SimpleCaseApproximateNumericElseExplicitNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tdbl.rnum,case when tdbl.cdbl > 1 then 'test1' when tdbl.cdbl < 0 then 'test2' else null end from tdbl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseExplicitNULL_p5
select 'SimpleCaseApproximateNumericElseExplicitNULL_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vrl.rnum,case when vrl.crl > 1 then 'test1' when vrl.crl < 0 then 'test2' else null end from vrl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumericElseExplicitNULL_p6
select 'SimpleCaseApproximateNumericElseExplicitNULL_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select trl.rnum,case when trl.crl > 1 then 'test1' when trl.crl < 0 then 'test2' else null end from trl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumeric_p1
select 'SimpleCaseApproximateNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vflt.rnum,case when vflt.cflt > 1 then 'test1' when vflt.cflt < 0 then 'test2' else 'else' end from vflt
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumeric_p2
select 'SimpleCaseApproximateNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tflt.rnum,case when tflt.cflt > 1 then 'test1' when tflt.cflt < 0 then 'test2' else 'else' end from tflt
) Q
group by
f1,f2
) Q ) P;
--- CaseComparisonsApproximateNumeric_p4
select 'CaseComparisonsApproximateNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'between' f2 from tversion union
select 5 f1, 'gt' f2 from tversion union
select tdbl.rnum,case  when tdbl.cdbl =  1  then '=' when tdbl.cdbl >  9  then 'gt' when tdbl.cdbl < -0.2 then 'lt' when tdbl.cdbl in  (0,11)  then 'in' when tdbl.cdbl between -1 and 0  then 'between' else 'other' end from tdbl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumeric_p3
select 'SimpleCaseApproximateNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vdbl.rnum,case when vdbl.cdbl > 1 then 'test1' when vdbl.cdbl < 0 then 'test2' else 'else' end from vdbl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumeric_p4
select 'SimpleCaseApproximateNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tdbl.rnum,case when tdbl.cdbl > 1 then 'test1' when tdbl.cdbl < 0 then 'test2' else 'else' end from tdbl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumeric_p5
select 'SimpleCaseApproximateNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vrl.rnum,case when vrl.crl > 1 then 'test1' when vrl.crl < 0 then 'test2' else 'else' end from vrl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseApproximateNumeric_p6
select 'SimpleCaseApproximateNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test2' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select trl.rnum,case when trl.crl > 1 then 'test1' when trl.crl < 0 then 'test2' else 'else' end from trl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumericElseDefaultsNULL_p1
select 'SimpleCaseExactNumericElseDefaultsNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, null f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, null f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vdec.rnum,case when vdec.cdec=10 then 'test1' when vdec.cdec=-0.1 then 'test2'  end from vdec
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumericElseDefaultsNULL_p2
select 'SimpleCaseExactNumericElseDefaultsNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, null f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, null f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tdec.rnum,case when tdec.cdec=10 then 'test1' when tdec.cdec=-0.1 then 'test2'  end from tdec
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumericElseDefaultsNULL_p3
select 'SimpleCaseExactNumericElseDefaultsNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, null f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, null f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vnum.rnum,case when vnum.cnum=10 then 'test1' when vnum.cnum=-0.1 then 'test2'  end from vnum
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumericElseDefaultsNULL_p4
select 'SimpleCaseExactNumericElseDefaultsNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, null f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, null f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tnum.rnum,case when tnum.cnum=10 then 'test1' when tnum.cnum=-0.1 then 'test2'  end from tnum
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumericElseExplicitNULL_p1
select 'SimpleCaseExactNumericElseExplicitNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, null f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, null f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vdec.rnum,case when vdec.cdec=10 then 'test1' when vdec.cdec=-0.1 then 'test2'  else null end from vdec
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumericElseExplicitNULL_p2
select 'SimpleCaseExactNumericElseExplicitNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, null f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, null f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tdec.rnum,case when tdec.cdec=10 then 'test1' when tdec.cdec=-0.1 then 'test2'  else null end from tdec
) Q
group by
f1,f2
) Q ) P;
--- CaseComparisonsApproximateNumeric_p5
select 'CaseComparisonsApproximateNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'between' f2 from tversion union
select 5 f1, 'gt' f2 from tversion union
select vrl.rnum,case  when vrl.crl =  1  then '=' when vrl.crl >  9  then 'gt' when vrl.crl < -0.2 then 'lt' when vrl.crl in  (0,11)  then 'in' when vrl.crl between -1 and 0  then 'between' else 'other' end from vrl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumericElseExplicitNULL_p3
select 'SimpleCaseExactNumericElseExplicitNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, null f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, null f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vnum.rnum,case when vnum.cnum=10 then 'test1' when vnum.cnum=-0.1 then 'test2'  else null end from vnum
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumericElseExplicitNULL_p4
select 'SimpleCaseExactNumericElseExplicitNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, null f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, null f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tnum.rnum,case when tnum.cnum=10 then 'test1' when tnum.cnum=-0.1 then 'test2'  else null end from tnum
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumeric_p1
select 'SimpleCaseExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vdec.rnum,case when vdec.cdec=10 then 'test1' when vdec.cdec=-0.1 then 'test2' else 'else' end from vdec
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumeric_p2
select 'SimpleCaseExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tdec.rnum,case when tdec.cdec=10 then 'test1' when tdec.cdec=-0.1 then 'test2' else 'else' end from tdec
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumeric_p3
select 'SimpleCaseExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select vnum.rnum,case when vnum.cnum=10 then 'test1' when vnum.cnum=-0.1 then 'test2' else 'else' end from vnum
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseExactNumeric_p4
select 'SimpleCaseExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'else' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'else' f2 from tversion union
select 5 f1, 'test1' f2 from tversion union
select tnum.rnum,case when tnum.cnum=10 then 'test1' when tnum.cnum=-0.1 then 'test2' else 'else' end from tnum
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseDefaultsNULL_p1
select 'SimpleCaseIntegerElseDefaultsNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vint.rnum,case when vint.cint=10 then 'test1' when vint.cint=-1 then 'test2' end from vint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseDefaultsNULL_p2
select 'SimpleCaseIntegerElseDefaultsNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tint.rnum,case when tint.cint=10 then 'test1' when tint.cint=-1 then 'test2' end from tint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseDefaultsNULL_p3
select 'SimpleCaseIntegerElseDefaultsNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vsint.rnum,case when vsint.csint=10 then 'test1' when vsint.csint=-1 then 'test2' end from vsint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseDefaultsNULL_p4
select 'SimpleCaseIntegerElseDefaultsNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tsint.rnum,case when tsint.csint=10 then 'test1' when tsint.csint=-1 then 'test2' end from tsint
) Q
group by
f1,f2
) Q ) P;
--- CaseComparisonsApproximateNumeric_p6
select 'CaseComparisonsApproximateNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'between' f2 from tversion union
select 5 f1, 'gt' f2 from tversion union
select trl.rnum,case  when trl.crl =  1  then '=' when trl.crl >  9  then 'gt' when trl.crl < -0.2 then 'lt' when trl.crl in  (0,11)  then 'in' when trl.crl between -1 and 0  then 'between' else 'other' end from trl
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseDefaultsNULL_p5
select 'SimpleCaseIntegerElseDefaultsNULL_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vbint.rnum,case when vbint.cbint=10 then 'test1' when vbint.cbint=-1 then 'test2' end from vbint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseDefaultsNULL_p6
select 'SimpleCaseIntegerElseDefaultsNULL_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tbint.rnum,case when tbint.cbint=10 then 'test1' when tbint.cbint=-1 then 'test2' end from tbint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseExplicitNULL_p1
select 'SimpleCaseIntegerElseExplicitNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vint.rnum,case when vint.cint=10 then 'test1' when vint.cint=-1 then 'test2' else null end from vint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseExplicitNULL_p2
select 'SimpleCaseIntegerElseExplicitNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tint.rnum,case when tint.cint=10 then 'test1' when tint.cint=-1 then 'test2' else null end from tint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseExplicitNULL_p3
select 'SimpleCaseIntegerElseExplicitNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vsint.rnum,case when vsint.csint=10 then 'test1' when vsint.csint=-1 then 'test2' else null end from vsint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseExplicitNULL_p4
select 'SimpleCaseIntegerElseExplicitNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tsint.rnum,case when tsint.csint=10 then 'test1' when tsint.csint=-1 then 'test2' else null end from tsint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseExplicitNULL_p5
select 'SimpleCaseIntegerElseExplicitNULL_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vbint.rnum,case when vbint.cbint=10 then 'test1' when vbint.cbint=-1 then 'test2' else null end from vbint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseIntegerElseExplicitNULL_p6
select 'SimpleCaseIntegerElseExplicitNULL_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, null f2 from tversion union
select 3 f1, null f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tbint.rnum,case when tbint.cbint=10 then 'test1' when tbint.cbint=-1 then 'test2' else null end from tbint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseInteger_p1
select 'SimpleCaseInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vint.rnum,case when vint.cint=10 then 'test1' when vint.cint=-1 then 'test2' else 'else' end from vint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseInteger_p2
select 'SimpleCaseInteger_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tint.rnum,case when tint.cint=10 then 'test1' when tint.cint=-1 then 'test2' else 'else' end from tint
) Q
group by
f1,f2
) Q ) P;
--- CaseComparisonsExactNumeric_p1
select 'CaseComparisonsExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'between' f2 from tversion union
select 5 f1, 'gt' f2 from tversion union
select vdec.rnum,case  when vdec.cdec =  1  then '=' when vdec.cdec >  9  then 'gt' when vdec.cdec < -0.1 then 'lt' when vdec.cdec in  (0,11)  then 'in' when vdec.cdec between 0 and 1  then 'between' else 'other' end from vdec
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseInteger_p3
select 'SimpleCaseInteger_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vsint.rnum,case when vsint.csint=10 then 'test1' when vsint.csint=-1 then 'test2' else 'else' end from vsint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseInteger_p4
select 'SimpleCaseInteger_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tsint.rnum,case when tsint.csint=10 then 'test1' when tsint.csint=-1 then 'test2' else 'else' end from tsint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseInteger_p5
select 'SimpleCaseInteger_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select vbint.rnum,case when vbint.cbint=10 then 'test1' when vbint.cbint=-1 then 'test2' else 'else' end from vbint
) Q
group by
f1,f2
) Q ) P;
--- SimpleCaseInteger_p6
select 'SimpleCaseInteger_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'else' f2 from tversion union
select 1 f1, 'test2' f2 from tversion union
select 2 f1, 'else' f2 from tversion union
select 3 f1, 'else' f2 from tversion union
select 4 f1, 'test1' f2 from tversion union
select tbint.rnum,case when tbint.cbint=10 then 'test1' when tbint.cbint=-1 then 'test2' else 'else' end from tbint
) Q
group by
f1,f2
) Q ) P;
--- SqrtCoreNull_p1
select 'SqrtCoreNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select sqrt( null ) from tversion
) Q
group by
f1
) Q ) P;
--- SqrtCore_p1
select 'SqrtCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select sqrt( 4 ) from tversion
) Q
group by
f1
) Q ) P;
--- SqrtCore_p2
select 'SqrtCore_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select sqrt( 4.0e+0 ) from tversion
) Q
group by
f1
) Q ) P;
--- SqrtCore_p3
select 'SqrtCore_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select sqrt( 4.0 ) from tversion
) Q
group by
f1
) Q ) P;
--- StanDevApproxNumeric_p1
select 'StanDevApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
--- StanDevApproxNumeric_p2
select 'StanDevApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
--- CaseComparisonsExactNumeric_p2
select 'CaseComparisonsExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'between' f2 from tversion union
select 5 f1, 'gt' f2 from tversion union
select tdec.rnum,case  when tdec.cdec =  1  then '=' when tdec.cdec >  9  then 'gt' when tdec.cdec < -0.1 then 'lt' when tdec.cdec in  (0,11)  then 'in' when tdec.cdec between 0 and 1  then 'between' else 'other' end from tdec
) Q
group by
f1,f2
) Q ) P;
--- StanDevApproxNumeric_p3
select 'StanDevApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
--- StanDevApproxNumeric_p4
select 'StanDevApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
--- StanDevApproxNumeric_p5
select 'StanDevApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
--- StanDevApproxNumeric_p6
select 'StanDevApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
--- StanDevExactNumeric_p1
select 'StanDevExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
--- StanDevExactNumeric_p2
select 'StanDevExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
--- StanDevExactNumeric_p3
select 'StanDevExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
--- StanDevExactNumeric_p4
select 'StanDevExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
--- StanDevInt_p1
select 'StanDevInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
--- StanDevInt_p2
select 'StanDevInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
--- CaseComparisonsExactNumeric_p3
select 'CaseComparisonsExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'between' f2 from tversion union
select 5 f1, 'gt' f2 from tversion union
select vnum.rnum,case  when vnum.cnum =  1  then '=' when vnum.cnum >  9  then 'gt' when vnum.cnum < -0.1 then 'lt' when vnum.cnum in  (0,11)  then 'in' when vnum.cnum between 0 and 1  then 'between' else 'other' end from vnum
) Q
group by
f1,f2
) Q ) P;
--- StanDevInt_p3
select 'StanDevInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
--- StanDevInt_p4
select 'StanDevInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
--- StanDevInt_p5
select 'StanDevInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
--- StanDevInt_p6
select 'StanDevInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
--- StanDevSampApproxNumeric_p1
select 'StanDevSampApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
--- StanDevSampApproxNumeric_p2
select 'StanDevSampApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
--- StanDevSampApproxNumeric_p3
select 'StanDevSampApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
--- StanDevSampApproxNumeric_p4
select 'StanDevSampApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
--- StanDevSampApproxNumeric_p5
select 'StanDevSampApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
--- StanDevSampApproxNumeric_p6
select 'StanDevSampApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
--- CaseComparisonsExactNumeric_p4
select 'CaseComparisonsExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'between' f2 from tversion union
select 5 f1, 'gt' f2 from tversion union
select tnum.rnum,case  when tnum.cnum =  1  then '=' when tnum.cnum >  9  then 'gt' when tnum.cnum < -0.1 then 'lt' when tnum.cnum in  (0,11)  then 'in' when tnum.cnum between 0 and 1  then 'between' else 'other' end from tnum
) Q
group by
f1,f2
) Q ) P;
--- StanDevSampExactNumeric_p1
select 'StanDevSampExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev_samp( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
--- StanDevSampExactNumeric_p2
select 'StanDevSampExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev_samp( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
--- StanDevSampExactNumeric_p3
select 'StanDevSampExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev_samp( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
--- StanDevSampExactNumeric_p4
select 'StanDevSampExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev_samp( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
--- StanDevSampInt_p1
select 'StanDevSampInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
--- StanDevSampInt_p2
select 'StanDevSampInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
--- StanDevSampInt_p3
select 'StanDevSampInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
--- StanDevSampInt_p4
select 'StanDevSampInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
--- StanDevSampInt_p5
select 'StanDevSampInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
--- StanDevSampInt_p6
select 'StanDevSampInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
--- CaseComparisonsInteger_p1
select 'CaseComparisonsInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, 'other' f2 from tversion union
select 1 f1, 'lt' f2 from tversion union
select 2 f1, 'in' f2 from tversion union
select 3 f1, '=' f2 from tversion union
select 4 f1, 'gt' f2 from tversion union
select vint.rnum,case  when vint.cint =  1  then '=' when vint.cint >  9  then 'gt' when vint.cint < 0 then 'lt' when vint.cint in  (0,11)  then 'in' when vint.cint between 6 and 8  then 'between'  else 'other' end from vint
) Q
group by
f1,f2
) Q ) P;
--- StringComparisonEq_p1
select 'StringComparisonEq_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2='BB'
) Q
group by
f1,f2
) Q ) P;
--- StringComparisonGtEq_p1
select 'StringComparisonGtEq_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum,tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 >= 'DD'
) Q
group by
f1,f2,f3
) Q ) P;
--- StringComparisonGt_p1
select 'StringComparisonGt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum,tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 > 'DD'
) Q
group by
f1,f2,f3
) Q ) P;
--- StringComparisonLtEq_p1
select 'StringComparisonLtEq_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select rnum,tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 <= 'EE'
) Q
group by
f1,f2,f3
) Q ) P;
--- StringComparisonLt_p1
select 'StringComparisonLt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 1 f1, 15 f2, 'DD' f3 from tversion union
select rnum,tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 < 'EE'
) Q
group by
f1,f2,f3
) Q ) P;
--- StringComparisonNtEq_p1
select 'StringComparisonNtEq_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum,tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 <> 'BB'
) Q
group by
f1,f2,f3
) Q ) P;
--- StringComparisonNtEq_p2
select 'StringComparisonNtEq_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 1 f1, 15 f2, 'DD' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select 3 f1, 10 f2, 'FF' f3 from tversion union
select rnum,tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 <> 'BB'
) Q
group by
f1,f2,f3
) Q ) P;
--- StringPredicateBetween_p1
select 'StringPredicateBetween_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 between 'AA' and 'CC'
) Q
group by
f1,f2
) Q ) P;
--- StringPredicateIn_p1
select 'StringPredicateIn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 'BB' f3 from tversion union
select 2 f1, null f2, 'EE' f3 from tversion union
select rnum,tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 in ('ZZ','BB','EE')
) Q
group by
f1,f2,f3
) Q ) P;
--- StringPredicateLike_p1
select 'StringPredicateLike_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 like 'B%'
) Q
group by
f1,f2
) Q ) P;
