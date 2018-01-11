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
-- Name: plpgsql; Type: PROCEDURAL LANGUAGE; Schema: -; Owner: gpadmin
--

CREATE LANGUAGE plpgsql;

SET search_path = public, pg_catalog;

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
-- Name: tbint; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tbint (
    rnum integer NOT NULL,
    cbint bigint
) DISTRIBUTED BY (rnum);



--
-- Name: tchar; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tchar (
    rnum integer NOT NULL,
    cchar character(32)
) DISTRIBUTED BY (rnum);



--
-- Name: tclob; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tclob (
    rnum integer NOT NULL,
    cclob text
) DISTRIBUTED BY (rnum);



--
-- Name: tdbl; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tdbl (
    rnum integer NOT NULL,
    cdbl double precision
) DISTRIBUTED BY (rnum);



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
-- Name: tflt; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tflt (
    rnum integer NOT NULL,
    cflt double precision
) DISTRIBUTED BY (rnum);



--
-- Name: tint; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tint (
    rnum integer NOT NULL,
    cint integer
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
-- Name: tlth; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tlth (
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
-- Name: tnum; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tnum (
    rnum integer NOT NULL,
    cnum numeric(7,2)
) DISTRIBUTED BY (rnum);



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
-- Name: ttm; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE ttm (
    rnum integer NOT NULL,
    ctm time(3) without time zone
) DISTRIBUTED BY (rnum);



--
-- Name: tts; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tts (
    rnum integer NOT NULL,
    cts timestamp(3) without time zone
) DISTRIBUTED BY (rnum);



--
-- Name: tvchar; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace:
--

CREATE TABLE tvchar (
    rnum integer NOT NULL,
    cvchar character varying(32)
) DISTRIBUTED BY (rnum);



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
-- Name: vchar; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vchar AS
    SELECT tchar.rnum, tchar.cchar FROM tchar;



--
-- Name: vclob; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vclob AS
    SELECT tclob.rnum, tclob.cclob FROM tclob;



--
-- Name: vdbl; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vdbl AS
    SELECT tdbl.rnum, tdbl.cdbl FROM tdbl;



--
-- Name: vdec; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vdec AS
    SELECT tdec.rnum, tdec.cdec FROM tdec;



--
-- Name: vdt; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vdt AS
    SELECT tdt.rnum, tdt.cdt FROM tdt;



--
-- Name: vflt; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vflt AS
    SELECT tflt.rnum, tflt.cflt FROM tflt;



--
-- Name: vint; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vint AS
    SELECT tint.rnum, tint.cint FROM tint;



--
-- Name: vnum; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vnum AS
    SELECT tnum.rnum, tnum.cnum FROM tnum;



--
-- Name: vrl; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vrl AS
    SELECT trl.rnum, trl.crl FROM trl;



--
-- Name: vsint; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vsint AS
    SELECT tsint.rnum, tsint.csint FROM tsint;



--
-- Name: vtm; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vtm AS
    SELECT ttm.rnum, ttm.ctm FROM ttm;



--
-- Name: vts; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vts AS
    SELECT tts.rnum, tts.cts FROM tts;



--
-- Name: vvchar; Type: VIEW; Schema: public; Owner: gpadmin
--

CREATE VIEW vvchar AS
    SELECT tvchar.rnum, tvchar.cvchar FROM tvchar;


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
-- Greenplum Database database dump complete
--

-- end_ignore

-- END OF SETUP

-- AbsCoreApproximateNumeric_p1
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
-- AbsCoreExactNumeric_p4
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
-- CaseComparisonsInteger_p2
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
-- StringPredicateLike_p2
select 'StringPredicateLike_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 like '%B%'
) Q
group by
f1,f2
) Q ) P;
-- StringPredicateLike_p3
select 'StringPredicateLike_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 like '_B'
) Q
group by
f1,f2
) Q ) P;
-- StringPredicateLike_p4
select 'StringPredicateLike_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 like 'BB%'
) Q
group by
f1,f2
) Q ) P;
-- StringPredicateNotBetween_p1
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
-- StringPredicateNotIn_p1
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
-- StringPredicateNotLike_p1
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
-- SubqueryColumnAlias_p1
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
-- SubqueryCorrelated_p1
select 'SubqueryCorrelated_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 15 f2 from tversion union
select tjoin1.c1, tjoin1.c2 from tjoin1 where tjoin1.c1 = any (select tjoin2.c1 from tjoin2 where tjoin2.c1 = tjoin1.c1)
) Q
group by
f1,f2
) Q ) P;
-- SubqueryDerivedAliasOrderBy_p1
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
-- SubqueryDerivedAssignNames_p1
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
-- CaseComparisonsInteger_p3
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
-- SubqueryDerivedMany_p1
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
-- SubqueryDerived_p1
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
-- SubqueryInAggregate_p1
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
-- SubqueryInCase_p1
-- test expected to fail until function supported in GPDB
-- GPDB Limitation ERROR:  Greenplum Database does not yet support that query.  DETAIL:  The query contains a multi-row subquery.
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
-- SubqueryInGroupBy_p1
select 'SubqueryInGroupBy_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 3 f1, 4 f2 from tversion union
select (select count(*) from tjoin1),count(*) from tjoin2 group by ( select count(*) from tjoin1 )
) Q
group by
f1,f2
) Q ) P;
-- SubqueryInHaving_p1
select 'SubqueryInHaving_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 3 f1 from tversion union
select count(*) from tjoin1 having count(*) > ( select count(*) from tversion )
) Q
group by
f1
) Q ) P;
-- SubqueryPredicateExists_p1
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
-- SubqueryPredicateIn_p1
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
-- SubqueryPredicateNotExists_p1
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
-- SubqueryPredicateNotIn_p1
-- test expected to fail until function supported in GPDB
-- GPDB Limitation ERROR:  Greenplum Database does not yet support that query.  DETAIL:  The query contains a multi-row subquery.
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
-- CaseComparisonsInteger_p4
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
-- SubqueryQuantifiedPredicateAnyFromC1_p1
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
-- SubqueryQuantifiedPredicateAnyFromC2_p1
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
-- SubqueryQuantifiedPredicateEmpty_p1
-- test expected to fail until GPDB support this function
-- GPDB Limitation ERROR:  Greenplum Database does not yet support this query.  DETAIL:  The query contains a multi-row subquery.
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
-- SubqueryQuantifiedPredicateLarge_p1
-- test expected to fail until GPDB supports this function
-- GPDB Limitation ERROR:  Greenplum Database does not yet support that query.  DETAIL:  The query contains a multi-row subquery.
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
-- SubstringBoundary_p1
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
-- SubstringBoundary_p2
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
-- SubstringBoundary_p3
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
-- SubstringBoundary_p4
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
-- SubstringBoundary_p5
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
-- SubstringBoundary_p6
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
-- CaseComparisonsInteger_p5
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
-- SubstringBoundary_p7
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
-- SubstringBoundary_p8
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
-- SubstringCoreBlob_p1
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
-- SubstringCoreFixedLength_p1
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
-- SubstringCoreFixedLength_p2
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
-- SubstringCoreNegativeStart_p1
select 'SubstringCoreNegativeStart_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '12' f1 from tversion union
select substring( '1234567890' from -2 for 5)  from tversion
) Q
group by
f1
) Q ) P;
-- SubstringCoreNestedFixedLength_p1
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
-- SubstringCoreNestedFixedLength_p2
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
-- SubstringCoreNestedVariableLength_p1
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
-- SubstringCoreNestedVariableLength_p2
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
-- CaseComparisonsInteger_p6
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
-- SubstringCoreNoLength_p1
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
-- SubstringCoreNoLength_p2
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
-- SubstringCoreNoLength_p3
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
-- SubstringCoreNoLength_p4
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
-- SubstringCoreNullParameters_p1
select 'SubstringCoreNullParameters_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select substring( '1234567890' from cnnull for 1)  from tversion
) Q
group by
f1
) Q ) P;
-- SubstringCoreNullParameters_p2
select 'SubstringCoreNullParameters_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select substring( '1234567890' from 1 for cnnull)  from tversion
) Q
group by
f1
) Q ) P;
-- SubstringCoreVariableLength_p1
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
-- SubstringCoreVariableLength_p2
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
-- SubstringValueExpr_p1
select 'SubstringValueExpr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where substring(tjoin2.c2 from 1 for 1) = 'B'
) Q
group by
f1,f2
) Q ) P;
-- SumDistinct_p1
select 'SumDistinct_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 210 f1 from tversion union
select sum( distinct c1 ) from tset1
) Q
group by
f1
) Q ) P;
-- CaseNestedApproximateNumeric_p1
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
-- TableConstructor_p1
select 'TableConstructor_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select t1.c1, t1.c2 from (values (10,'BB')) t1(c1,c2)
) Q
group by
f1,f2
) Q ) P;
-- TrimBothCoreFixedLength_p1
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
-- TrimBothCoreFixedLength_p2
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
-- TrimBothCoreNullParameters_p1
select 'TrimBothCoreNullParameters_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select trim(both ccnull from '++1234567890++' )  from tversion
) Q
group by
f1
) Q ) P;
-- TrimBothCoreNullParameters_p2
select 'TrimBothCoreNullParameters_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select trim(both '+' from ccnull )  from tversion
) Q
group by
f1
) Q ) P;
-- TrimBothCoreVariableLength_p1
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
-- TrimBothCoreVariableLength_p2
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
-- TrimBothException_p1
select 'TrimBothException_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'error' f1 from tversion union
select trim(both '++' from '++1234567890++' )  from tversion
) Q
group by
f1
) Q ) P;
-- TrimBothSpacesCore_p1
select 'TrimBothSpacesCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '1234567890' f1 from tversion union
select trim(both from '  1234567890  ' )  from tversion
) Q
group by
f1
) Q ) P;
-- TrimCore_p1
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
-- CaseNestedApproximateNumeric_p2
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
-- TrimCore_p2
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
-- TrimCore_p3
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
-- TrimCore_p4
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
-- TrimLeadingSpacesCore_p1
select 'TrimLeadingSpacesCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '1234567890  ' f1 from tversion union
select trim(leading from '  1234567890  ' )  from tversion
) Q
group by
f1
) Q ) P;
-- TrimTrailingSpacesCore_p1
select 'TrimTrailingSpacesCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '  1234567890' f1 from tversion union
select trim(trailing from '  1234567890  ' )  from tversion
) Q
group by
f1
) Q ) P;
-- UnionAll_p1
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
-- Union_p1
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
-- UpperCoreFixedLength_p1
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
-- UpperCoreFixedLength_p2
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
-- UpperCoreSpecial_p1
select 'UpperCoreSpecial_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'ß' f1 from tversion union
select upper( 'ß' ) from tversion
) Q
group by
f1
) Q ) P;
-- CaseNestedApproximateNumeric_p3
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
-- UpperCoreVariableLength_p1
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
-- UpperCoreVariableLength_p2
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
-- VarApproxNumeric_p1
select 'VarApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
-- VarApproxNumeric_p2
select 'VarApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
-- VarApproxNumeric_p3
select 'VarApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
-- VarApproxNumeric_p4
select 'VarApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
-- VarApproxNumeric_p5
select 'VarApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
-- VarApproxNumeric_p6
select 'VarApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select variance( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
-- VarExactNumeric_p1
select 'VarExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select variance( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
-- VarExactNumeric_p2
select 'VarExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select variance( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
-- CaseNestedApproximateNumeric_p4
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
-- VarExactNumeric_p3
select 'VarExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select variance( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
-- VarExactNumeric_p4
select 'VarExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select variance( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
-- VarInt_p1
select 'VarInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
-- VarInt_p2
select 'VarInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
-- VarInt_p3
select 'VarInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
-- VarInt_p4
select 'VarInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
-- VarInt_p5
select 'VarInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
-- VarInt_p6
select 'VarInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select variance( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
-- VarSampApproxNumeric_p1
select 'VarSampApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
-- VarSampApproxNumeric_p2
select 'VarSampApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
-- CaseNestedApproximateNumeric_p5
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
-- VarSampApproxNumeric_p3
select 'VarSampApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
-- VarSampApproxNumeric_p4
select 'VarSampApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
-- VarSampApproxNumeric_p5
select 'VarSampApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
-- VarSampApproxNumeric_p6
select 'VarSampApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.602 f1 from tversion union
select var_samp( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
-- VarSampExactNumeric_p1
select 'VarSampExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select var_samp( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
-- VarSampExactNumeric_p2
select 'VarSampExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select var_samp( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
-- VarSampExactNumeric_p3
select 'VarSampExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select var_samp( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
-- VarSampExactNumeric_p4
select 'VarSampExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 20.402 f1 from tversion union
select var_samp( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
-- VarSampInt_p1
select 'VarSampInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
-- VarSampInt_p2
select 'VarSampInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
-- AbsCoreInteger_p1
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
-- CaseNestedApproximateNumeric_p6
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
-- VarSampInt_p3
select 'VarSampInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
-- VarSampInt_p4
select 'VarSampInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
-- VarSampInt_p5
select 'VarSampInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
-- VarSampInt_p6
select 'VarSampInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 25.66667 f1 from tversion union
select var_samp( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
-- WithClauseDerivedTable_p1
-- test exepected to fail until GPDB supports function
-- GPDB Limitation syntax not supported WITH clause
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
-- JoinCoreLikePredicate_gp_p1
select 'JoinCoreLikePredicate_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select tjoin1.rnum, tjoin1.c1, tjoin1.c2, tjoin2.c2 as c2j2 from tjoin1 inner join tjoin2 on ( tjoin1.c1 = tjoin2.c1 and tjoin2.c2 like 'A%' ))
) Q
group by
f1
) Q ) P;
-- JoinCoreNestedInner_gp_p1
select 'JoinCoreNestedInner_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select tjoin1.c1, tjoin2.c2, tjoin3.c2 as c2j3 from (( tjoin1 inner join tjoin2 on tjoin1.c1=tjoin2.c1 ) inner join tjoin3 on tjoin3.c1=tjoin1.c1) inner join tjoin4 on tjoin4.c1=tjoin3.c1)
) Q
group by
f1
) Q ) P;
-- NumericComparisonEqual_gp_p1
select 'NumericComparisonEqual_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select 1 from tversion where 7 = 210.3 union select 1 from tversion where 7 = cnnull)
) Q
group by
f1
) Q ) P;
-- SelectWhere_gp_p1
select 'SelectWhere_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 0, 1 f1 from tversion union
select rnum, c1  from tversion where rnum=0
) Q
group by
f1
) Q ) P;
-- SimpleSelect_gp_p1
select 'SimpleSelect_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select c1  from tversion
) Q
group by
f1
) Q ) P;
-- CaseNestedExactNumeric_p1
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
-- StringPredicateLikeEscape_gp_p1
select 'StringPredicateLikeEscape_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where ('%%' || tjoin2.c2) like '!%%B' escape '!'
) Q
group by
f1,f2
) Q ) P;
-- StringPredicateLikeUnderscore_gp_p1
select 'StringPredicateLikeUnderscore_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where ('__' || tjoin2.c2) like '!__BB' escape '!'
) Q
group by
f1,f2
) Q ) P;
-- SubqueryNotIn_gp_p1
select 'SubqueryNotIn_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select tjoin1.rnum, tjoin1.c1 from tjoin1 where tjoin1.c1 not in (select tjoin2.c1 from tjoin2))
) Q
group by
f1
) Q ) P;
-- SubqueryOnCondition_gp_p1
select 'SubqueryOnCondition_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select tjoin1.c1, tjoin2.c2 from tjoin1 inner join tjoin2 on ( tjoin1.c1 = tjoin2.c1 and tjoin1.c2 < ( select count(*) from tversion)))
) Q
group by
f1
) Q ) P;
-- SubqueryPredicateWhereIn_gp_p1
select 'SubqueryPredicateWhereIn_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select rnum, c1, c2 from tjoin2 where 50 in ( select c1 from tjoin1))
) Q
group by
f1
) Q ) P;
-- SubqueryQuantifiedPredicateNull_gp_p1
-- test expected to fail until GPDB support function
-- GPDB Limitation ERROR:  Greenplum Database does not yet support this query.  DETAIL:  The query contains a multi-row subquery.
select 'SubqueryQuantifiedPredicateNull_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select rnum, c1, c2 from tjoin2 where 20 > all ( select c1 from tjoin1))
) Q
group by
f1
) Q ) P;
-- SubqueryQuantifiedPredicateSmall_gp_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation ERROR:  Greenplum Database does not yet support this query.  DETAIL:  The query contains a multi-row subquery.
select 'SubqueryQuantifiedPredicateSmall_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not exists (select rnum, c1, c2 from tjoin2 where 20 > all ( select c2 from tjoin1))
) Q
group by
f1
) Q ) P;
-- SubstringCoreLiteral_gp_p1
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
-- SubstringCoreLiteral_gp_p2
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
-- SubstringCoreLiteral_gp_p3
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
-- CaseNestedExactNumeric_p2
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
-- TrimCoreLiteral_gp_p1
select 'TrimCoreLiteral_gp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select  length(trim( 'BB ' ))  from tversion
) Q
group by
f1
) Q ) P;
-- TrimCoreLiteral_gp_p2
select 'TrimCoreLiteral_gp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select  length(trim( 'EE  ' ))  from tversion
) Q
group by
f1
) Q ) P;
-- TrimCoreLiteral_gp_p3
select 'TrimCoreLiteral_gp_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select  length(trim( '  FF      ' ))  from tversion
) Q
group by
f1
) Q ) P;
-- CaseNestedExactNumeric_p3
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
-- CaseNestedExactNumeric_p4
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
-- CaseNestedInteger_p1
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
-- CaseNestedInteger_p2
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
-- CaseNestedInteger_p3
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
-- CaseNestedInteger_p4
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
-- CaseNestedInteger_p5
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
-- AbsCoreInteger_p2
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
-- CaseNestedInteger_p6
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
-- CaseSubqueryApproxmiateNumeric_p1
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
-- CaseSubqueryApproxmiateNumeric_p2
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
-- CaseSubqueryApproxmiateNumeric_p3
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
-- CaseSubqueryApproxmiateNumeric_p4
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
-- CaseSubqueryApproxmiateNumeric_p5
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
-- CaseSubqueryApproxmiateNumeric_p6
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
-- CaseSubQueryExactNumeric_p1
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
-- CaseSubQueryExactNumeric_p2
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
-- CaseSubQueryExactNumeric_p3
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
-- AbsCoreInteger_p3
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
-- CaseSubQueryExactNumeric_p4
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
-- CaseSubQueryInteger_p1
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
-- CaseSubQueryInteger_p2
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
-- CaseSubQueryInteger_p3
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
-- CaseSubQueryInteger_p4
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
-- CaseSubQueryInteger_p5
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
-- CaseSubQueryInteger_p6
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
-- CastBigintToBigint_p1
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
-- CastBigintToBigint_p2
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
-- CastBigintToChar_p1
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
-- AbsCoreInteger_p4
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
-- CastBigintToChar_p2
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
-- CastBigintToDecimal_p1
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
-- CastBigintToDecimal_p2
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
-- CastBigintToDouble_p1
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
-- CastBigintToDouble_p2
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
-- CastBigintToFloat_p1
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
-- CastBigintToFloat_p2
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
-- CastBigintToInteger_p1
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
-- CastBigintToInteger_p2
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
-- CastBigintToSmallint_p1
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
-- AbsCoreInteger_p5
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
-- CastBigintToSmallint_p2
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
-- CastBigintToVarchar_p1
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
-- CastBigintToVarchar_p2
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
-- CastCharsToBigint_p1
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
-- CastCharsToChar_p1
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
-- CastCharsToChar_p2
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
-- CastCharsToChar_p3
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
-- CastCharsToChar_p4
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
-- CastCharsToDate_p1
select 'CastCharsToDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
-- CastCharsToDouble_p1
select 'CastCharsToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.3 f1 from tversion union
select cast('10.3' as double precision) from tversion
) Q
group by
f1
) Q ) P;
-- AbsCoreInteger_p6
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
-- CastCharsToFloat_p1
select 'CastCharsToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.3 f1 from tversion union
select cast('10.3' as float) from tversion
) Q
group by
f1
) Q ) P;
-- CastCharsToInteger_p1
select 'CastCharsToInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select cast('10' as integer) from tversion
) Q
group by
f1
) Q ) P;
-- CastCharsToSmallint_p1
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
-- CastCharsToTimestamp_p1
select 'CastCharsToTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01 12:30:40' as timestamp) f1 from tversion union
select cast('2000-01-01 12:30:40' as timestamp) from tversion
) Q
group by
f1
) Q ) P;
-- CastCharsToVarchar_p1
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
-- CastCharsToVarchar_p2
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
-- CastCharsToVarchar_p3
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
-- CastCharsToVarchar_p4
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
-- CastDateToChar_p1
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
-- CastDateToChar_p2
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
-- AggregateInExpression_p1
select 'AggregateInExpression_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select 10 * count( 1 ) from tversion
) Q
group by
f1
) Q ) P;
-- CastDateToDate_p1
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
-- CastDateToDate_p2
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
-- CastDateToVarchar_p1
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
-- CastDateToVarchar_p2
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
-- CastDecimalToBigint_p1
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
-- CastDecimalToBigint_p2
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
-- CastDecimalToChar_p1
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
-- CastDecimalToChar_p2
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
-- CastDecimalToDouble_p1
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
-- CastDecimalToDouble_p2
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
-- ApproximateNumericOpAdd_p1
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
-- CastDecimalToFloat_p1
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
-- CastDecimalToFloat_p2
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
-- CastDecimalToInteger_p1
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
-- CastDecimalToInteger_p2
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
-- CastDecimalToSmallint_p1
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
-- CastDecimalToSmallint_p2
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
-- CastDecimalToVarchar_p1
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
-- CastDecimalToVarchar_p2
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
-- CastDoubleToBigint_p1
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
-- CastDoubleToBigint_p2
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
-- ApproximateNumericOpAdd_p2
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
-- CastDoubleToChar_p1
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
-- CastDoubleToChar_p2
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
-- CastDoubleToDouble_p1
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
-- CastDoubleToDouble_p2
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
-- CastDoubleToFloat_p1
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
-- CastDoubleToFloat_p2
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
-- CastDoubleToSmallint_p1
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
-- CastDoubleToSmallint_p2
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
-- CastDoubleToVarchar_p1
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
-- CastDoubleToVarchar_p2
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
-- AbsCoreApproximateNumeric_p2
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
-- ApproximateNumericOpAdd_p3
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
-- CastFloatToBigint_p1
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
-- CastFloatToBigint_p2
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
-- CastFloatToChar_p1
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
-- CastFloatToChar_p2
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
-- CastFloatToDouble_p1
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
-- CastFloatToDouble_p2
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
-- CastFloatToFloat_p1
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
-- CastFloatToFloat_p2
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
-- CastFloatToSmallint_p1
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
-- CastFloatToSmallint_p2
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
-- ApproximateNumericOpAdd_p4
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
-- CastFloatToVarchar_p1
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
-- CastFloatToVarchar_p2
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
-- CastIntegerToBigint_p1
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
-- CastIntegerToBigint_p2
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
-- CastIntegerToChar_p1
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
-- CastIntegerToChar_p2
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
-- CastIntegerToDouble_p1
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
-- CastIntegerToDouble_p2
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
-- CastIntegerToFloat_p1
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
-- CastIntegerToFloat_p2
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
-- ApproximateNumericOpAdd_p5
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
-- CastIntegerToInteger_p1
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
-- CastIntegerToInteger_p2
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
-- CastIntegerToSmallint_p1
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
-- CastIntegerToSmallint_p2
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
-- CastIntegerToVarchar_p1
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
-- CastIntegerToVarchar_p2
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
-- CastNumericToBigint_p1
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
-- CastNumericToBigint_p2
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
-- CastNumericToChar_p1
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
-- CastNumericToChar_p2
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
-- ApproximateNumericOpAdd_p6
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
-- CastNumericToDouble_p1
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
-- CastNumericToDouble_p2
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
-- CastNumericToFloat_p1
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
-- CastNumericToFloat_p2
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
-- CastNumericToInteger_p1
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
-- CastNumericToInteger_p2
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
-- CastNumericToSmallint_p1
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
-- CastNumericToSmallint_p2
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
-- CastNumericToVarchar_p1
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
-- CastNumericToVarchar_p2
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
-- ApproximateNumericOpDiv_p1
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
-- CastNvarcharToBigint_p1
select 'CastNvarcharToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast(n'1' as bigint) from tversion
) Q
group by
f1
) Q ) P;
-- CastNvarcharToDouble_p1
select 'CastNvarcharToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.000000000000000e+000 f1 from tversion union
select cast(n'1.0' as double precision) from tversion
) Q
group by
f1
) Q ) P;
-- CastNvarcharToInteger_p1
select 'CastNvarcharToInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast(n'1' as integer) from tversion
) Q
group by
f1
) Q ) P;
-- CastNvarcharToSmallint_p1
select 'CastNvarcharToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast(n'1' as smallint) from tversion
) Q
group by
f1
) Q ) P;
-- CastRealToBigint_p1
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
-- CastRealToBigint_p2
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
-- CastRealToChar_p1
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
-- CastRealToChar_p2
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
-- CastRealToDouble_p1
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
-- CastRealToDouble_p2
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
-- ApproximateNumericOpDiv_p2
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
-- CastRealToSmallint_p1
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
-- CastRealToSmallint_p2
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
-- CastRealToVarchar_p1
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
-- CastRealToVarchar_p2
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
-- CastSmallintToBigint_p1
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
-- CastSmallintToBigint_p2
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
-- CastSmallintToChar_p1
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
-- CastSmallintToChar_p2
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
-- CastSmallintToDouble_p1
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
-- CastSmallintToDouble_p2
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
-- ApproximateNumericOpDiv_p3
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
-- CastSmallintToFloat_p1
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
-- CastSmallintToFloat_p2
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
-- CastSmallintToSmallint_p1
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
-- CastSmallintToSmallint_p2
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
-- CastSmallintToVarchar_p1
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
-- CastSmallintToVarchar_p2
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
-- CastTimestampToChar_p1
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
-- CastTimestampToChar_p2
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
-- CastTimestampToDate_p1
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
-- CastTimestampToDate_p2
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
-- ApproximateNumericOpDiv_p4
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
-- CastTimestampToVarchar_p1
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
-- CastTimestampToVarchar_p2
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
-- CastTimeToChar_p1
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
-- CastTimeToChar_p2
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
-- CastTimeToVarchar_p1
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
-- CastTimeToVarchar_p2
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
-- CastVarcharToBigint_p1
select 'CastVarcharToBigint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast('1' as bigint) from tversion
) Q
group by
f1
) Q ) P;
-- CastVarcharToChar_p1
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
-- CastVarcharToChar_p2
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
-- CastVarcharToDate_p1
select 'CastVarcharToDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpDiv_p5
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
-- CastVarcharToDate_p2
select 'CastVarcharToDate_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
-- CastVarcharToDate_p3
select 'CastVarcharToDate_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
-- CastVarcharToDate_p4
select 'CastVarcharToDate_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
-- CastVarcharToDate_p5
select 'CastVarcharToDate_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01' as date) f1 from tversion union
select cast('2000-01-01' as date) from tversion
) Q
group by
f1
) Q ) P;
-- CastVarcharToDouble_p1
select 'CastVarcharToDouble_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.000000000000000e+000 f1 from tversion union
select cast('1.0' as double precision) from tversion
) Q
group by
f1
) Q ) P;
-- CastVarcharToFloat_p1
select 'CastVarcharToFloat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.000000000000000e+000 f1 from tversion union
select cast('1.0' as float) from tversion
) Q
group by
f1
) Q ) P;
-- CastVarcharToFloat_p2
select 'CastVarcharToFloat_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.000000000000000e+000 f1 from tversion union
select cast('1.0' as float) from tversion
) Q
group by
f1
) Q ) P;
-- CastVarcharToInteger_p1
select 'CastVarcharToInteger_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast('1' as integer) from tversion
) Q
group by
f1
) Q ) P;
-- CastVarcharToSmallint_p1
select 'CastVarcharToSmallint_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select cast('1' as smallint) from tversion
) Q
group by
f1
) Q ) P;
-- CastVarcharToTimestamp_p1
select 'CastVarcharToTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select cast('2000-01-01 12:00:00.000000000' as timestamp) f1 from tversion union
select cast('2000-01-01 12:00:00' as timestamp) from tversion
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpDiv_p6
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
-- CastVarcharToVarchar_p1
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
-- CastVarcharToVarchar_p2
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
-- CeilCoreApproximateNumeric_p1
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
-- CeilCoreApproximateNumeric_p2
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
-- CeilCoreApproximateNumeric_p3
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
-- CeilCoreApproximateNumeric_p4
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
-- CeilCoreApproximateNumeric_p5
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
-- CeilCoreApproximateNumeric_p6
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
-- CeilCoreExactNumeric_p1
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
-- CeilCoreExactNumeric_p2
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
-- AbsCoreApproximateNumeric_p3
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
-- ApproximateNumericOpMulNULL_p1
select 'ApproximateNumericOpMulNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select -1.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- CeilCoreExactNumeric_p3
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
-- CeilCoreExactNumeric_p4
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
-- CeilCoreIntegers_p1
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
-- CeilCoreIntegers_p2
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
-- CeilCoreIntegers_p3
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
-- CeilCoreIntegers_p4
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
-- CeilCoreIntegers_p5
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
-- CeilCoreIntegers_p6
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
-- CharacterLiteral_p1
select 'CharacterLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'BB' f1 from tversion union
select 'BB' from tversion
) Q
group by
f1
) Q ) P;
-- CharacterLiteral_p2
select 'CharacterLiteral_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'BB' f1 from tversion union
select 'BB' from tversion
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpMulNULL_p2
select 'ApproximateNumericOpMulNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 0.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- CharlengthCoreFixedLength_p1
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
-- CharlengthCoreFixedLength_p2
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
-- CharlengthCoreVariableLength_p1
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
-- CharlengthCoreVariableLength_p2
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
-- CoalesceCoreNullParameters_p1
select 'CoalesceCoreNullParameters_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select coalesce( ccnull, ccnull, ccnull ) from tversion
) Q
group by
f1
) Q ) P;
-- CoalesceCore_p1
select 'CoalesceCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'A' f1 from tversion union
select coalesce( ccnull, 'A', 'B' ) from tversion
) Q
group by
f1
) Q ) P;
-- Comments1_p1
select 'Comments1_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select /* hello */ 1 from tversion
) Q
group by
f1
) Q ) P;
-- ConcatCoreFixedLength_p1
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
-- ConcatCoreFixedLength_p2
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
-- ConcatCoreMaxLengthStringPlusBlankString_p1
select 'ConcatCoreMaxLengthStringPlusBlankString_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'UDA_VARIABLE_LENGTH_MAX_STRING                                ' f1 from tversion union
select 'UDA_VARIABLE_LENGTH_MAX_STRING' || vchar.cchar  from vchar where vchar.rnum = 2
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpMulNULL_p3
select 'ApproximateNumericOpMulNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 1.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- ConcatCoreMaxLengthStringPlusBlankString_p2
select 'ConcatCoreMaxLengthStringPlusBlankString_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'UDA_VARIABLE_LENGTH_MAX_STRING                                ' f1 from tversion union
select 'UDA_VARIABLE_LENGTH_MAX_STRING' || tchar.cchar  from tchar where tchar.rnum = 2
) Q
group by
f1
) Q ) P;
-- ConcatCoreMaxLengthStringPlusBlankString_p3
select 'ConcatCoreMaxLengthStringPlusBlankString_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'UDA_VARIABLE_LENGTH_MAX_STRING                                ' f1 from tversion union
select 'UDA_VARIABLE_LENGTH_MAX_STRING' || vvchar.cvchar  from vvchar where vvchar.rnum = 2
) Q
group by
f1
) Q ) P;
-- ConcatCoreMaxLengthStringPlusBlankString_p4
select 'ConcatCoreMaxLengthStringPlusBlankString_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'UDA_VARIABLE_LENGTH_MAX_STRING                                ' f1 from tversion union
select 'UDA_VARIABLE_LENGTH_MAX_STRING' || tvchar.cvchar  from tvchar where tvchar.rnum = 2
) Q
group by
f1
) Q ) P;
-- ConcatCoreVariableLength_p1
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
-- ConcatCoreVariableLength_p2
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
-- ConcatException_p1
select 'ConcatException_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select vchar.rnum,'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || vchar.cchar  from vchar where vchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
-- ConcatException_p2
select 'ConcatException_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select vchar.rnum, 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || vchar.cchar  from vchar where vchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
-- ConcatException_p3
select 'ConcatException_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select tchar.rnum,'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || tchar.cchar  from tchar where tchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
-- ConcatException_p4
select 'ConcatException_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select tchar.rnum, 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || tchar.cchar  from tchar where tchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
-- ConcatException_p5
select 'ConcatException_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select vvchar.rnum,'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || vvchar.cvchar  from vvchar where vvchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
-- ApproximateNumericOpMulNULL_p4
select 'ApproximateNumericOpMulNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select -1.0e-1 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- ConcatException_p6
select 'ConcatException_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select vvchar.rnum, 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || vvchar.cvchar  from vvchar where vvchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
-- ConcatException_p7
select 'ConcatException_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select tvchar.rnum,'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || tvchar.cvchar  from tvchar where tvchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
-- ConcatException_p8
select 'ConcatException_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 2 f1, 'should throw error' f2 from tversion union
select tvchar.rnum, 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' || tvchar.cvchar  from tvchar where tvchar.rnum > 2
) Q
group by
f1,f2
) Q ) P;
-- CountCharLiteral_p1
select 'CountCharLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('') from tversion
) Q
group by
f1
) Q ) P;
-- CountCharLiteral_p10
select 'CountCharLiteral_p10' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('FF') from tversion
) Q
group by
f1
) Q ) P;
-- CountCharLiteral_p2
select 'CountCharLiteral_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(' ') from tversion
) Q
group by
f1
) Q ) P;
-- CountCharLiteral_p3
select 'CountCharLiteral_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('BB') from tversion
) Q
group by
f1
) Q ) P;
-- CountCharLiteral_p4
select 'CountCharLiteral_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('EE') from tversion
) Q
group by
f1
) Q ) P;
-- CountCharLiteral_p5
select 'CountCharLiteral_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('FF') from tversion
) Q
group by
f1
) Q ) P;
-- CountCharLiteral_p6
select 'CountCharLiteral_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('') from tversion
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpMulNULL_p5
select 'ApproximateNumericOpMulNULL_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 10.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- CountCharLiteral_p7
select 'CountCharLiteral_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(' ') from tversion
) Q
group by
f1
) Q ) P;
-- CountCharLiteral_p8
select 'CountCharLiteral_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('BB') from tversion
) Q
group by
f1
) Q ) P;
-- CountCharLiteral_p9
select 'CountCharLiteral_p9' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count('EE') from tversion
) Q
group by
f1
) Q ) P;
-- CountClob_p1
select 'CountClob_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vclob.cclob) from vclob
) Q
group by
f1
) Q ) P;
-- CountClob_p2
select 'CountClob_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tclob.cclob) from tclob
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p1
select 'CountNumericLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p10
select 'CountNumericLiteral_p10' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p11
select 'CountNumericLiteral_p11' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p12
select 'CountNumericLiteral_p12' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p13
select 'CountNumericLiteral_p13' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpMul_p1
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
-- CountNumericLiteral_p14
select 'CountNumericLiteral_p14' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e-1) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p15
select 'CountNumericLiteral_p15' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p16
select 'CountNumericLiteral_p16' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p17
select 'CountNumericLiteral_p17' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p18
select 'CountNumericLiteral_p18' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1.0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p19
select 'CountNumericLiteral_p19' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.1) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p2
select 'CountNumericLiteral_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p20
select 'CountNumericLiteral_p20' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10.0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p21
select 'CountNumericLiteral_p21' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p22
select 'CountNumericLiteral_p22' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.0) from tversion
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpMul_p2
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
-- CountNumericLiteral_p23
select 'CountNumericLiteral_p23' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1.0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p24
select 'CountNumericLiteral_p24' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.1) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p25
select 'CountNumericLiteral_p25' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10.0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p26
select 'CountNumericLiteral_p26' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p27
select 'CountNumericLiteral_p27' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p28
select 'CountNumericLiteral_p28' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p29
select 'CountNumericLiteral_p29' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p3
select 'CountNumericLiteral_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p30
select 'CountNumericLiteral_p30' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p31
select 'CountNumericLiteral_p31' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0) from tversion
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpMul_p3
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
-- CountNumericLiteral_p32
select 'CountNumericLiteral_p32' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p33
select 'CountNumericLiteral_p33' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p34
select 'CountNumericLiteral_p34' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p35
select 'CountNumericLiteral_p35' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p36
select 'CountNumericLiteral_p36' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p37
select 'CountNumericLiteral_p37' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p4
select 'CountNumericLiteral_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e-1) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p5
select 'CountNumericLiteral_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(10.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p6
select 'CountNumericLiteral_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p7
select 'CountNumericLiteral_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(0.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpMul_p4
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
-- CountNumericLiteral_p8
select 'CountNumericLiteral_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(1.0e+0) from tversion
) Q
group by
f1
) Q ) P;
-- CountNumericLiteral_p9
select 'CountNumericLiteral_p9' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(-1.0e-1) from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p1
select 'CountTemporalLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(date '1996-01-01') from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p10
select 'CountTemporalLiteral_p10' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-12-31 00:00:00') from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p11
select 'CountTemporalLiteral_p11' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-12-31 12:00:00') from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p12
select 'CountTemporalLiteral_p12' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-12-31 23:59:30.123') from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p2
select 'CountTemporalLiteral_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(date '2000-01-01') from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p3
select 'CountTemporalLiteral_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(date '2000-12-31') from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p4
select 'CountTemporalLiteral_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(time '00:00:00.000') from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p5
select 'CountTemporalLiteral_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(time '12:00:00.000') from tversion
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpMul_p5
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
-- CountTemporalLiteral_p6
select 'CountTemporalLiteral_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(time '23:59:30.123') from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p7
select 'CountTemporalLiteral_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-01-01 00:00:00.0') from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p8
select 'CountTemporalLiteral_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-01-01 12:00:00') from tversion
) Q
group by
f1
) Q ) P;
-- CountTemporalLiteral_p9
select 'CountTemporalLiteral_p9' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(timestamp '2000-01-01 23:59:30.123') from tversion
) Q
group by
f1
) Q ) P;
-- CountValueExpression_p1
select 'CountValueExpression_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count( 1 * 10 ) from tversion
) Q
group by
f1
) Q ) P;
-- DateLiteral_p1
select 'DateLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '1996-01-01' f1 from tversion union
select date '1996-01-01' from tversion
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p1
select 'DistinctAggregateInCase_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vflt.cflt))=-1 then 'test1' else 'else' end from vflt
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p10
select 'DistinctAggregateInCase_p10' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tnum.cnum))=-1 then 'test1' else 'else' end from tnum
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p11
select 'DistinctAggregateInCase_p11' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vint.cint))=-1 then 'test1' else 'else' end from vint
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p12
select 'DistinctAggregateInCase_p12' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tint.cint))=-1 then 'test1' else 'else' end from tint
) Q
group by
f1
) Q ) P;
-- AbsCoreApproximateNumeric_p4
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
-- ApproximateNumericOpMul_p6
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
-- DistinctAggregateInCase_p13
select 'DistinctAggregateInCase_p13' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vsint.csint))=-1 then 'test1' else 'else' end from vsint
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p14
select 'DistinctAggregateInCase_p14' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tsint.csint))=-1 then 'test1' else 'else' end from tsint
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p15
select 'DistinctAggregateInCase_p15' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vbint.cbint))=-1 then 'test1' else 'else' end from vbint
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p16
select 'DistinctAggregateInCase_p16' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tbint.cbint))=-1 then 'test1' else 'else' end from tbint
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p2
select 'DistinctAggregateInCase_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tflt.cflt))=-1 then 'test1' else 'else' end from tflt
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p3
select 'DistinctAggregateInCase_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vdbl.cdbl))=-1 then 'test1' else 'else' end from vdbl
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p4
select 'DistinctAggregateInCase_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tdbl.cdbl))=-1 then 'test1' else 'else' end from tdbl
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p5
select 'DistinctAggregateInCase_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vrl.crl))=-1 then 'test1' else 'else' end from vrl
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p6
select 'DistinctAggregateInCase_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(trl.crl))=-1 then 'test1' else 'else' end from trl
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p7
select 'DistinctAggregateInCase_p7' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vdec.cdec))=-1 then 'test1' else 'else' end from vdec
) Q
group by
f1
) Q ) P;
-- ApproximateNumericOpSub_p1
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
-- DistinctAggregateInCase_p8
select 'DistinctAggregateInCase_p8' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(tdec.cdec))=-1 then 'test1' else 'else' end from tdec
) Q
group by
f1
) Q ) P;
-- DistinctAggregateInCase_p9
select 'DistinctAggregateInCase_p9' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'test1' f1 from tversion union
select case when min(distinct(vnum.cnum))=-1 then 'test1' else 'else' end from vnum
) Q
group by
f1
) Q ) P;
-- DistinctCore_p1
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
-- EmptyStringIsNull_p1
select 'EmptyStringIsNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select vvchar.rnum, vvchar.cvchar  from vvchar where vvchar.cvchar is null
) Q
group by
f1,f2
) Q ) P;
-- EmptyStringIsNull_p2
select 'EmptyStringIsNull_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 0 f1, null f2 from tversion union
select tvchar.rnum, tvchar.cvchar  from tvchar where tvchar.cvchar is null
) Q
group by
f1,f2
) Q ) P;
-- ExactNumericOpAdd_p1
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
-- ExactNumericOpAdd_p2
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
-- ExactNumericOpAdd_p3
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
-- ExactNumericOpAdd_p4
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
-- ExactNumericOpDiv_p1
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
-- ApproximateNumericOpSub_p2
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
-- ExactNumericOpDiv_p2
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
-- ExactNumericOpDiv_p3
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
-- ExactNumericOpDiv_p4
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
-- ExactNumericOpMulNULL_p1
select 'ExactNumericOpMulNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select -1.0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- ExactNumericOpMulNULL_p2
select 'ExactNumericOpMulNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 0.0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- ExactNumericOpMulNULL_p3
select 'ExactNumericOpMulNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 1.0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- ExactNumericOpMulNULL_p4
select 'ExactNumericOpMulNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 0.1 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- ExactNumericOpMulNULL_p5
select 'ExactNumericOpMulNULL_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 10.0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- ExactNumericOpMul_p1
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
-- ExactNumericOpMul_p2
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
-- ApproximateNumericOpSub_p3
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
-- ExactNumericOpMul_p3
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
-- ExactNumericOpMul_p4
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
-- ExactNumericOpSub_p1
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
-- ExactNumericOpSub_p2
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
-- ExactNumericOpSub_p3
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
-- ExactNumericOpSub_p4
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
-- ExceptAll_p1
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
-- Except_p1
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
-- ExpCoreApproximateNumeric_p1
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
-- ExpCoreApproximateNumeric_p2
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
-- ApproximateNumericOpSub_p4
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
-- ExpCoreApproximateNumeric_p3
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
-- ExpCoreApproximateNumeric_p4
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
-- ExpCoreApproximateNumeric_p5
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
-- ExpCoreApproximateNumeric_p6
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
-- ExpCoreExactNumeric_p1
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
-- ExpCoreExactNumeric_p2
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
-- ExpCoreExactNumeric_p3
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
-- ExpCoreExactNumeric_p4
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
-- ExpCoreIntegers_p1
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
-- ExpCoreIntegers_p2
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
-- ApproximateNumericOpSub_p5
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
-- ExpCoreIntegers_p3
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
-- ExpCoreIntegers_p4
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
-- ExpCoreIntegers_p5
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
-- ExpCoreIntegers_p6
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
-- ExpressionInIn_p1
select 'ExpressionInIn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.rnum in (1 - 1)
) Q
group by
f1,f2
) Q ) P;
-- ExpressionUsingAggregate_p1
select 'ExpressionUsingAggregate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 51 f1 from tversion union
select (1 + max(c1) - min(c1) ) from tset1
) Q
group by
f1
) Q ) P;
-- ExtractCoreDayFromDate_p1
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
-- ExtractCoreDayFromDate_p2
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
-- ExtractCoreDayFromTimestamp_p1
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
-- ExtractCoreDayFromTimestamp_p2
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
-- ApproximateNumericOpSub_p6
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
-- ExtractCoreHourFromTimestamp_p1
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
-- ExtractCoreHourFromTimestamp_p2
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
-- ExtractCoreHourFromTime_p1
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
-- ExtractCoreHourFromTime_p2
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
-- ExtractCoreMinuteFromTimestamp_p1
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
-- ExtractCoreMinuteFromTimestamp_p2
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
-- ExtractCoreMinuteFromTime_p1
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
-- ExtractCoreMinuteFromTime_p2
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
-- ExtractCoreMonthFromDate_p1
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
-- ExtractCoreMonthFromDate_p2
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
-- AvgApproxNumeric_p1
select 'AvgApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(vflt.cflt) from vflt
) Q
group by
f1
) Q ) P;
-- ExtractCoreMonthFromTimestamp_p1
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
-- ExtractCoreMonthFromTimestamp_p2
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
-- ExtractCoreSecondFromTimestamp_p1
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
-- ExtractCoreSecondFromTimestamp_p2
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
-- ExtractCoreSecondFromTime_p1
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
-- ExtractCoreSecondFromTime_p2
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
-- ExtractCoreYearFromDate_p1
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
-- ExtractCoreYearFromDate_p2
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
-- ExtractCoreYearFromTimestamp_p1
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
-- ExtractCoreYearFromTimestamp_p2
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
-- AvgApproxNumeric_p2
select 'AvgApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(tflt.cflt) from tflt
) Q
group by
f1
) Q ) P;
-- FloorCoreApproximateNumeric_p1
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
-- FloorCoreApproximateNumeric_p2
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
-- FloorCoreApproximateNumeric_p3
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
-- FloorCoreApproximateNumeric_p4
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
-- FloorCoreApproximateNumeric_p5
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
-- FloorCoreApproximateNumeric_p6
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
-- FloorCoreExactNumeric_p1
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
-- FloorCoreExactNumeric_p2
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
-- FloorCoreExactNumeric_p3
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
-- FloorCoreExactNumeric_p4
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
-- AvgApproxNumeric_p3
select 'AvgApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(vdbl.cdbl) from vdbl
) Q
group by
f1
) Q ) P;
-- FloorCoreIntegers_p1
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
-- FloorCoreIntegers_p2
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
-- FloorCoreIntegers_p3
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
-- FloorCoreIntegers_p4
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
-- FloorCoreIntegers_p5
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
-- FloorCoreIntegers_p6
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
-- GroupByAlias_p1
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
-- GroupByExpr_p1
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
-- GroupByHaving_p1
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
-- GroupByLiteral_p1
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
-- AbsCoreApproximateNumeric_p5
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
-- AvgApproxNumeric_p4
select 'AvgApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(tdbl.cdbl) from tdbl
) Q
group by
f1
) Q ) P;
-- GroupByMany_p1
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
-- GroupByMultiply_p1
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
-- GroupByOrdinal_p1
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
-- GroupBy_p1
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
-- Having_p1
select 'Having_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 8 f1 from tversion union
select count(c1) from tset1 having count(*) > 2
) Q
group by
f1
) Q ) P;
-- IntegerOpAdd_p1
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
-- IntegerOpAdd_p2
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
-- IntegerOpAdd_p3
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
-- IntegerOpAdd_p4
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
-- IntegerOpAdd_p5
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
-- AvgApproxNumeric_p5
select 'AvgApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(vrl.crl) from vrl
) Q
group by
f1
) Q ) P;
-- IntegerOpAdd_p6
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
-- IntegerOpDiv_p1
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
-- IntegerOpDiv_p2
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
-- IntegerOpDiv_p3
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
-- IntegerOpDiv_p4
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
-- IntegerOpDiv_p5
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
-- IntegerOpDiv_p6
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
-- IntegerOpMulNULL_p1
select 'IntegerOpMulNULL_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select -1.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- IntegerOpMulNULL_p2
select 'IntegerOpMulNULL_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 0.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- IntegerOpMulNULL_p3
select 'IntegerOpMulNULL_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 1.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- AvgApproxNumeric_p6
select 'AvgApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1.98 f1 from tversion union
select avg(trl.crl) from trl
) Q
group by
f1
) Q ) P;
-- IntegerOpMulNULL_p4
select 'IntegerOpMulNULL_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select -1.0e-1 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- IntegerOpMulNULL_p5
select 'IntegerOpMulNULL_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select 10.0e+0 * cnnull from tversion
) Q
group by
f1
) Q ) P;
-- IntegerOpMul_p1
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
-- IntegerOpMul_p2
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
-- IntegerOpMul_p3
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
-- IntegerOpMul_p4
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
-- IntegerOpMul_p5
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
-- IntegerOpMul_p6
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
-- IntegerOpSub_p1
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
-- IntegerOpSub_p2
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
-- AvgExactNumeric_p1
select 'AvgExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.02 f1 from tversion union
select avg(vdec.cdec) from vdec
) Q
group by
f1
) Q ) P;
-- IntegerOpSub_p3
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
-- IntegerOpSub_p4
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
-- IntegerOpSub_p5
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
-- IntegerOpSub_p6
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
-- IntersectAll_p1
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
-- Intersect_p1
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
-- IsNullPredicate_p1
select 'IsNullPredicate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select null f1, 'EE' f2 from tversion union
select c1, c2 from tjoin2 where c1 is null
) Q
group by
f1,f2
) Q ) P;
-- IsNullValueExpr_p1
select 'IsNullValueExpr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select c1 from tversion where 1 * cnnull is null
) Q
group by
f1
) Q ) P;
-- JoinCoreCrossProduct_p1
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
-- JoinCoreCross_p1
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
-- AvgExactNumeric_p2
select 'AvgExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.02 f1 from tversion union
select avg(tdec.cdec) from tdec
) Q
group by
f1
) Q ) P;
-- JoinCoreEqualWithAnd_p1
select 'JoinCoreEqualWithAnd_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin1.c1, tjoin2.c2 from tjoin1 inner join tjoin2 on ( tjoin1.c1 = tjoin2.c1 and tjoin2.c2='BB' )
) Q
group by
f1,f2
) Q ) P;
-- JoinCoreImplicit_p1
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
-- JoinCoreIsNullPredicate_p1
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
-- JoinCoreLeftNestedInnerTableRestrict_p1
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
-- JoinCoreLeftNestedOptionalTableRestrict_p1
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
-- JoinCoreNatural_p1
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
-- JoinCoreNestedInnerOuter_p1
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
-- JoinCoreNestedOuterInnerTableRestrict_p1
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
-- JoinCoreNestedOuterOptionalTableRestrict_p1
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
-- JoinCoreNestedOuter_p1
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
-- AvgExactNumeric_p3
select 'AvgExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.02 f1 from tversion union
select avg(vnum.cnum) from vnum
) Q
group by
f1
) Q ) P;
-- JoinCoreNoExpressionInOnCondition_p1
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
-- JoinCoreNonEquiJoin_p1
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
-- JoinCoreNonEquiJoin_p2
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
-- JoinCoreNonJoinNonEquiJoin_p1
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
-- JoinCoreNotPredicate_p1
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
-- JoinCoreNwayJoinedTable_p1
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
-- JoinCoreOnConditionAbsFunction_p1
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
-- JoinCoreOnConditionSetFunction_p1
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
-- JoinCoreOnConditionSubstringFunction_p1
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
-- JoinCoreOnConditionTrimFunction_p1
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
-- AvgExactNumeric_p4
select 'AvgExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.02 f1 from tversion union
select avg(tnum.cnum) from tnum
) Q
group by
f1
) Q ) P;
-- JoinCoreOnConditionUpperFunction_p1
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
-- JoinCoreOptionalTableFilter_p1
select 'JoinCoreOptionalTableFilter_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'FF' f2 from tversion union
select tjoin1.c1, tjoin2.c2 from tjoin1 left outer join tjoin2 on tjoin1.c1 = tjoin2.c1 where tjoin2.c2 > 'C'
) Q
group by
f1,f2
) Q ) P;
-- JoinCoreOptionalTableJoinFilter_p1
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
-- JoinCoreOptionalTableJoinRestrict_p1
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
-- JoinCoreOrPredicate_p1
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
-- JoinCoreOrPredicate_p2
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
-- JoinCorePreservedTableFilter_p1
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
-- JoinCorePreservedTableJoinFilter_p1
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
-- JoinCoreRightNestedInnerTableRestrict_p1
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
-- JoinCoreRightNestedOptionalTableRestrict_p1
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
-- AvgIntTruncates_p1
select 'AvgIntTruncates_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(vint.cint) from vint
) Q
group by
f1
) Q ) P;
-- JoinCoreSelf_p1
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
-- JoinCoreSimpleAndJoinedTable_p1
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
-- JoinCoreTwoSidedJoinRestrictionFilter_p1
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
-- JoinCoreTwoSidedJoinRestrict_p1
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
-- JoinCoreUsing_p1
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
-- LikeValueExpr_p1
select 'LikeValueExpr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 like upper('BB')
) Q
group by
f1,f2
) Q ) P;
-- LnCoreNull_p1
select 'LnCoreNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select ln( null ) from tversion
) Q
group by
f1
) Q ) P;
-- LnCore_p1
select 'LnCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.302585092994050e+000 f1 from tversion union
select ln( 10 ) from tversion
) Q
group by
f1
) Q ) P;
-- LnCore_p2
select 'LnCore_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.302585092994050e+000 f1 from tversion union
select ln( 10.0e+0 ) from tversion
) Q
group by
f1
) Q ) P;
-- LnCore_p3
select 'LnCore_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.302585092994050e+000 f1 from tversion union
select ln( 10.0 ) from tversion
) Q
group by
f1
) Q ) P;
-- AvgIntTruncates_p2
select 'AvgIntTruncates_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(tint.cint) from tint
) Q
group by
f1
) Q ) P;
-- LowerCoreFixedLength_p1
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
-- LowerCoreFixedLength_p2
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
-- LowerCoreSpecial_p1
select 'LowerCoreSpecial_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'ß' f1 from tversion union
select lower( 'ß' ) from tversion
) Q
group by
f1
) Q ) P;
-- LowerCoreVariableLength_p1
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
-- LowerCoreVariableLength_p2
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
-- MaxLiteralTemp_p1
select 'MaxLiteralTemp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '2000-01-01' f1 from tversion union
select max( '2000-01-01' ) from tversion
) Q
group by
f1
) Q ) P;
-- MinLiteralTemp_p1
select 'MinLiteralTemp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '2000-01-01' f1 from tversion union
select min( '2000-01-01' ) from tversion
) Q
group by
f1
) Q ) P;
-- ModBoundaryTinyNumber_p1
select 'ModBoundaryTinyNumber_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 0 f1 from tversion union
select mod( 35, 0.000000000001 ) from tversion
) Q
group by
f1
) Q ) P;
-- ModCore2ExactNumeric_p1
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
-- ModCore2ExactNumeric_p2
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
-- AvgIntTruncates_p3
select 'AvgIntTruncates_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(vsint.csint) from vsint
) Q
group by
f1
) Q ) P;
-- ModCore2ExactNumeric_p3
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
-- ModCore2ExactNumeric_p4
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
-- ModCore2Integers_p1
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
-- ModCore2Integers_p2
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
-- ModCore2Integers_p3
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
-- ModCore2Integers_p4
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
-- ModCore2Integers_p5
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
-- ModCore2Integers_p6
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
-- ModCoreExactNumeric_p1
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
-- ModCoreExactNumeric_p2
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
-- AbsCoreApproximateNumeric_p6
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
-- AvgIntTruncates_p4
select 'AvgIntTruncates_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(tsint.csint) from tsint
) Q
group by
f1
) Q ) P;
-- ModCoreExactNumeric_p3
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
-- ModCoreExactNumeric_p4
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
-- ModCoreIntegers_p1
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
-- ModCoreIntegers_p2
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
-- ModCoreIntegers_p3
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
-- ModCoreIntegers_p4
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
-- ModCoreIntegers_p5
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
-- ModCoreIntegers_p6
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
-- MultipleSumDistinct_p1
select 'MultipleSumDistinct_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 210 f1, 4 f2 from tversion union
select sum( distinct c1 ), count( distinct c2 ) from tset1
) Q
group by
f1,f2
) Q ) P;
-- Negate_p1
select 'Negate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -4 f1 from tversion union
select -(2 * 2) from tversion
) Q
group by
f1
) Q ) P;
-- AvgIntTruncates_p5
select 'AvgIntTruncates_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(vbint.cbint) from vbint
) Q
group by
f1
) Q ) P;
-- NullifCoreReturnsNull_p1
select 'NullifCoreReturnsNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select nullif(cnnull, cnnull) from tversion
) Q
group by
f1
) Q ) P;
-- NullifCoreReturnsNull_p2
select 'NullifCoreReturnsNull_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select nullif(1,1) from tversion
) Q
group by
f1
) Q ) P;
-- NullifCoreReturnsNull_p3
select 'NullifCoreReturnsNull_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select nullif(cnnull, 1) from tversion
) Q
group by
f1
) Q ) P;
-- NullifCoreReturnsOne_p1
select 'NullifCoreReturnsOne_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select nullif(1,2) from tversion
) Q
group by
f1
) Q ) P;
-- NumericComparisonGreaterThanOrEqual_p1
select 'NumericComparisonGreaterThanOrEqual_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where 210.3 >= 7
) Q
group by
f1
) Q ) P;
-- NumericComparisonGreaterThan_p1
select 'NumericComparisonGreaterThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where 210.3 > 7
) Q
group by
f1
) Q ) P;
-- NumericComparisonLessThanOrEqual_p1
select 'NumericComparisonLessThanOrEqual_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where 7 <= 210.3
) Q
group by
f1
) Q ) P;
-- NumericComparisonLessThan_p1
select 'NumericComparisonLessThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where 7 < 210.3
) Q
group by
f1
) Q ) P;
-- NumericComparisonNotEqual_p1
select 'NumericComparisonNotEqual_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where 7 <> 210.3
) Q
group by
f1
) Q ) P;
-- AvgIntTruncates_p6
select 'AvgIntTruncates_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select avg(tbint.cbint) from tbint
) Q
group by
f1
) Q ) P;
-- NumericLiteral_p1
select 'NumericLiteral_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select +1.000000000000000e+000 f1 from tversion union
select 1.0 from tversion
) Q
group by
f1
) Q ) P;
-- OlapCoreAvgMultiplePartitions_p1
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
-- OlapCoreAvgNoWindowFrame_p1
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
-- OlapCoreAvgRowsBetween_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation: ERROR: window specifications with a framing clause must have an ORDER BY clause
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
-- OlapCoreCountMultiplePartitions_p1
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
-- OlapCoreCountNoWindowFrame_p1
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
-- OlapCoreCountRowsBetween_p2
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
-- OlapCoreCountStar_p1
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
-- OlapCoreCumedistNullOrdering_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation syntax not supported; NULLS LAST
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
-- OlapCoreCumedist_p1
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
-- AvgInt_p1
select 'AvgInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(vint.cint) from vint
) Q
group by
f1
) Q ) P;
-- OlapCoreDenseRankNullOrdering_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation syntax not supported; NULLS LAST
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
-- OlapCoreDenseRank_p1
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
-- OlapCoreFirstValueNullOrdering_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation syntax not supported; NULLS LAST
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
-- OlapCoreFirstValueRowsBetween_p1
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
-- OlapCoreLastValueNoWindowFrame_p1
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
-- OlapCoreLastValueNullOrdering_p1
-- test expected to fail until GPDB support function
-- GPDB Limitation syntax not supported; NULLS LAST
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
-- OlapCoreLastValueRowsBetween_p1
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
-- OlapCoreMax_p1
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
-- OlapCoreMin_p1
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
-- AvgInt_p2
select 'AvgInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(tint.cint) from tint
) Q
group by
f1
) Q ) P;
-- OlapCoreNtile_p1
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
-- OlapCoreNullOrder_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation syntax not supported; NULLS FIRST
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
-- OlapCorePercentRankNullOrdering_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation syntax not supported; NULLS LAST
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
-- OlapCorePercentRank_p1
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
-- OlapCoreRankMultiplePartitions_p1
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
-- OlapCoreRankNoWindowFrame_p1
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
-- OlapCoreRankNullOrdering_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation syntax not supported; NULLS LAST
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
-- OlapCoreRankOrderby100_p1
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
-- OlapCoreRowNumberNullOrdering_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation: syntax not supported; NULLS LAST
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
-- OlapCoreRowNumber_p1
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
-- AvgInt_p3
select 'AvgInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(vsint.csint) from vsint
) Q
group by
f1
) Q ) P;
-- OlapCoreRunningSum_p1
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
-- OlapCoreStddevPop_p1
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
-- OlapCoreStddevSamp_p1
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
-- OlapCoreStddev_p1
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
-- OlapCoreSumMultiplePartitions_p1
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
-- OlapCoreSumNullOrdering_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation syntax not supported; NULLS LAST
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
-- OlapCoreSumOfGroupedSums_p1
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
-- OlapCoreSumOrderby100_p1
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
-- OlapCoreSum_p1
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
-- OlapCoreVariance_p1
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
-- AvgInt_p4
select 'AvgInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(tsint.csint) from tsint
) Q
group by
f1
) Q ) P;
-- OlapCoreVarPop_p1
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
-- OlapCoreVarSamp_p1
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
-- OlapCoreWindowFrameMultiplePartitions_p1
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
-- OlapCoreWindowFrameRowsBetweenPrecedingFollowing_p1
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
-- OlapCoreWindowFrameRowsBetweenPrecedingPreceding_p1
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
-- OlapCoreWindowFrameRowsBetweenUnboundedFollowing_p1
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
-- OlapCoreWindowFrameRowsBetweenUnboundedPreceding_p1
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
-- OlapCoreWindowFrameRowsPreceding_p1
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
-- OlapCoreWindowFrameWindowDefinition_p1
-- test expected to fail until GPDB supports function
-- GPDB Limitation: syntax not supported
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
-- OperatorAnd_p1
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
-- AvgInt_p5
select 'AvgInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(vbint.cbint) from vbint
) Q
group by
f1
) Q ) P;
-- OperatorOr_p1
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
-- OrderByOrdinal_p1
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
-- PositionCoreString1Empty_p1
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
-- PositionCoreString1Empty_p2
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
-- PositionCoreString1Empty_p3
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
-- PositionCoreString1Empty_p4
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
-- PositionCore_p1
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
-- PositionCore_p2
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
-- PositionCore_p3
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
-- PositionCore_p4
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
-- AvgInt_p6
select 'AvgInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2.500000000000000e+000 f1 from tversion union
select avg(tbint.cbint) from tbint
) Q
group by
f1
) Q ) P;
-- PowerBoundary_p1
select 'PowerBoundary_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select power( 0,0 ) from tversion
) Q
group by
f1
) Q ) P;
-- PowerCoreApproxNumeric_p1
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
-- PowerCoreApproxNumeric_p2
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
-- PowerCoreApproxNumeric_p3
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
-- PowerCoreApproxNumeric_p4
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
-- PowerCoreApproxNumeric_p5
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
-- PowerCoreApproxNumeric_p6
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
-- PowerCoreExactNumeric_p1
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
-- PowerCoreExactNumeric_p2
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
-- PowerCoreExactNumeric_p3
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
-- BooleanComparisonOperatorAnd_p1
select 'BooleanComparisonOperatorAnd_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where (1 < 2) and (3 < 4)
) Q
group by
f1
) Q ) P;
-- PowerCoreExactNumeric_p4
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
-- PowerCoreIntegers_p1
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
-- PowerCoreIntegers_p2
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
-- PowerCoreIntegers_p3
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
-- PowerCoreIntegers_p4
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
-- PowerCoreIntegers_p5
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
-- PowerCoreIntegers_p6
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
-- PowerCoreNegativeBaseOddExp_p1
select 'PowerCoreNegativeBaseOddExp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -64 f1 from tversion union
select power( -4,3 ) from tversion
) Q
group by
f1
) Q ) P;
-- RowSubquery_p1
select 'RowSubquery_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 15 f3 from tversion union
select rnum, c1, c2 from tjoin1 where (c1,'BB') in (select c1, c2 from tjoin2 where c2='BB')
) Q
group by
f1,f2,f3
) Q ) P;
-- RowValueConstructor_p1
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
-- AbsCoreExactNumeric_p1
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
-- BooleanComparisonOperatorNotOperatorAnd_p1
select 'BooleanComparisonOperatorNotOperatorAnd_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not (2 < 1) and (3 < 4)
) Q
group by
f1
) Q ) P;
-- ScalarSubqueryInProjList_p1
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
-- ScalarSubquery_p1
select 'ScalarSubquery_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3, count(*) c  from (
select 0 f1, 10 f2, 15 f3 from tversion union
select rnum, c1, c2 from tjoin1 where c1 = ( select min(c1) from tjoin1)
) Q
group by
f1,f2,f3
) Q ) P;
-- SelectCountApproxNumeric_p1
select 'SelectCountApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vflt.cflt) from vflt
) Q
group by
f1
) Q ) P;
-- SelectCountApproxNumeric_p2
select 'SelectCountApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tflt.cflt) from tflt
) Q
group by
f1
) Q ) P;
-- SelectCountApproxNumeric_p3
select 'SelectCountApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vdbl.cdbl) from vdbl
) Q
group by
f1
) Q ) P;
-- SelectCountApproxNumeric_p4
select 'SelectCountApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tdbl.cdbl) from tdbl
) Q
group by
f1
) Q ) P;
-- SelectCountApproxNumeric_p5
select 'SelectCountApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vrl.crl) from vrl
) Q
group by
f1
) Q ) P;
-- SelectCountApproxNumeric_p6
select 'SelectCountApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(trl.crl) from trl
) Q
group by
f1
) Q ) P;
-- SelectCountChar_p1
select 'SelectCountChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vchar.cchar) from vchar
) Q
group by
f1
) Q ) P;
-- SelectCountChar_p2
select 'SelectCountChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tchar.cchar) from tchar
) Q
group by
f1
) Q ) P;
-- BooleanComparisonOperatorNotOperatorOr_p1
select 'BooleanComparisonOperatorNotOperatorOr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where not (2 < 1) or (3 < 4)
) Q
group by
f1
) Q ) P;
-- SelectCountChar_p3
select 'SelectCountChar_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vchar.cchar) from vchar
) Q
group by
f1
) Q ) P;
-- SelectCountChar_p4
select 'SelectCountChar_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tchar.cchar) from tchar
) Q
group by
f1
) Q ) P;
-- SelectCountChar_p5
select 'SelectCountChar_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vvchar.cvchar) from vvchar
) Q
group by
f1
) Q ) P;
-- SelectCountChar_p6
select 'SelectCountChar_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tvchar.cvchar) from tvchar
) Q
group by
f1
) Q ) P;
-- SelectCountDate_p1
select 'SelectCountDate_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 3 f1 from tversion union
select count(vdt.cdt) from vdt
) Q
group by
f1
) Q ) P;
-- SelectCountDate_p2
select 'SelectCountDate_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 3 f1 from tversion union
select count(tdt.cdt) from tdt
) Q
group by
f1
) Q ) P;
-- SelectCountExactNumeric_p1
select 'SelectCountExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vdec.cdec) from vdec
) Q
group by
f1
) Q ) P;
-- SelectCountExactNumeric_p2
select 'SelectCountExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tdec.cdec) from tdec
) Q
group by
f1
) Q ) P;
-- SelectCountExactNumeric_p3
select 'SelectCountExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vnum.cnum) from vnum
) Q
group by
f1
) Q ) P;
-- SelectCountExactNumeric_p4
select 'SelectCountExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tnum.cnum) from tnum
) Q
group by
f1
) Q ) P;
-- BooleanComparisonOperatorOr_p1
select 'BooleanComparisonOperatorOr_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where (1 < 2) or (4 < 3)
) Q
group by
f1
) Q ) P;
-- SelectCountInt_p1
select 'SelectCountInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(vint.cint) from vint
) Q
group by
f1
) Q ) P;
-- SelectCountInt_p2
select 'SelectCountInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(tint.cint) from tint
) Q
group by
f1
) Q ) P;
-- SelectCountInt_p3
select 'SelectCountInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(vsint.csint) from vsint
) Q
group by
f1
) Q ) P;
-- SelectCountInt_p4
select 'SelectCountInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(tsint.csint) from tsint
) Q
group by
f1
) Q ) P;
-- SelectCountInt_p5
select 'SelectCountInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(vbint.cbint) from vbint
) Q
group by
f1
) Q ) P;
-- SelectCountInt_p6
select 'SelectCountInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4 f1 from tversion union
select count(tbint.cbint) from tbint
) Q
group by
f1
) Q ) P;
-- SelectCountNullNumeric_p1
select 'SelectCountNullNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 0 f1 from tversion union
select count(cnnull) from tversion
) Q
group by
f1
) Q ) P;
-- SelectCountNullNumeric_p2
select 'SelectCountNullNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 0 f1 from tversion union
select count(cnnull) from tversion
) Q
group by
f1
) Q ) P;
-- SelectCountNull_p1
select 'SelectCountNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 0 f1 from tversion union
select count(ccnull) from tversion
) Q
group by
f1
) Q ) P;
-- SelectCountStar_p1
select 'SelectCountStar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select count(*) from tversion
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchApproximateNumeric_p1
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
-- SelectCountTimestamp_p1
select 'SelectCountTimestamp_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9 f1 from tversion union
select count(vts.cts) from vts
) Q
group by
f1
) Q ) P;
-- SelectCountTimestamp_p2
select 'SelectCountTimestamp_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9 f1 from tversion union
select count(tts.cts) from tts
) Q
group by
f1
) Q ) P;
-- SelectCountTime_p1
select 'SelectCountTime_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 3 f1 from tversion union
select count(vtm.ctm) from vtm
) Q
group by
f1
) Q ) P;
-- SelectCountTime_p2
select 'SelectCountTime_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 3 f1 from tversion union
select count(ttm.ctm) from ttm
) Q
group by
f1
) Q ) P;
-- SelectCountVarChar_p1
select 'SelectCountVarChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(vvchar.cvchar) from vvchar
) Q
group by
f1
) Q ) P;
-- SelectCountVarChar_p2
select 'SelectCountVarChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5 f1 from tversion union
select count(tvchar.cvchar) from tvchar
) Q
group by
f1
) Q ) P;
-- SelectDateComparisonEqualTo_p1
select 'SelectDateComparisonEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2001-01-01' = date '2001-01-01'
) Q
group by
f1
) Q ) P;
-- SelectDateComparisonGreaterThanOrEqualTo_p1
select 'SelectDateComparisonGreaterThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2001-01-01' >= date '2000-01-01'
) Q
group by
f1
) Q ) P;
-- SelectDateComparisonGreaterThan_p1
select 'SelectDateComparisonGreaterThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2001-01-01' > date '2000-01-01'
) Q
group by
f1
) Q ) P;
-- SelectDateComparisonLessThanOrEqualTo_p1
select 'SelectDateComparisonLessThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2000-01-01' <= date '2001-01-01'
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchApproximateNumeric_p2
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
-- SelectDateComparisonLessThan_p1
select 'SelectDateComparisonLessThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2000-01-01' < date '2001-01-01'
) Q
group by
f1
) Q ) P;
-- SelectDateComparisonNotEqualTo_p1
select 'SelectDateComparisonNotEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where date '2001-01-01' <> date '2000-01-01'
) Q
group by
f1
) Q ) P;
-- SelectJapaneseColumnConcat_p1
select 'SelectJapaneseColumnConcat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 47 f1, '「２」計画音声認識                  ' f2 from tversion union
select rnum, '「２」計画' || c1 from tlja where rnum = 47
) Q
group by
f1,f2
) Q ) P;
-- SelectJapaneseColumnLower_p1
select 'SelectJapaneseColumnLower_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 47 f1, '音声認識                  ' f2 from tversion union
select rnum, lower(c1) from tlja where rnum = 47
) Q
group by
f1,f2
) Q ) P;
-- SelectJapaneseColumnOrderByLocal_p1
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
-- SelectJapaneseColumnWhere_p1
select 'SelectJapaneseColumnWhere_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 47 f1, '音声認識                                ' f2 from tversion union
select rnum, c1  from tlja where c1='音声認識'
) Q
group by
f1,f2
) Q ) P;
-- SelectJapaneseDistinctColumn_p1
select 'SelectJapaneseDistinctColumn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 50 f1 from tversion union
select count (distinct c1)  from tlja
) Q
group by
f1
) Q ) P;
-- SelectMaxApproxNumeric_p1
select 'SelectMaxApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
-- SelectMaxApproxNumeric_p2
select 'SelectMaxApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
-- SelectMaxApproxNumeric_p3
select 'SelectMaxApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchApproximateNumeric_p3
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
-- SelectMaxApproxNumeric_p4
select 'SelectMaxApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
-- SelectMaxApproxNumeric_p5
select 'SelectMaxApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
-- SelectMaxApproxNumeric_p6
select 'SelectMaxApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
-- SelectMaxChar_p1
select 'SelectMaxChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'FF                              ' f1 from tversion union
select max( vchar.cchar ) from vchar
) Q
group by
f1
) Q ) P;
-- SelectMaxChar_p2
select 'SelectMaxChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'FF                              ' f1 from tversion union
select max( tchar.cchar ) from tchar
) Q
group by
f1
) Q ) P;
-- SelectMaxExactNumeric_p1
select 'SelectMaxExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
-- SelectMaxExactNumeric_p2
select 'SelectMaxExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
-- SelectMaxExactNumeric_p3
select 'SelectMaxExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
-- SelectMaxExactNumeric_p4
select 'SelectMaxExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
-- SelectMaxInt_p1
select 'SelectMaxInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchApproximateNumeric_p4
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
-- SelectMaxInt_p2
select 'SelectMaxInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
-- SelectMaxInt_p3
select 'SelectMaxInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
-- SelectMaxInt_p4
select 'SelectMaxInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
-- SelectMaxInt_p5
select 'SelectMaxInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
-- SelectMaxInt_p6
select 'SelectMaxInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select max( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
-- SelectMaxLit_p1
select 'SelectMaxLit_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'A' f1 from tversion union
select max( 'A' ) from tversion
) Q
group by
f1
) Q ) P;
-- SelectMaxNullNumeric_p1
select 'SelectMaxNullNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select max( cnnull ) from tversion
) Q
group by
f1
) Q ) P;
-- SelectMaxNull_p1
select 'SelectMaxNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select max( ccnull ) from tversion
) Q
group by
f1
) Q ) P;
-- SelectMaxVarChar_p1
select 'SelectMaxVarChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'FF' f1 from tversion union
select max( vvchar.cvchar ) from vvchar
) Q
group by
f1
) Q ) P;
-- SelectMaxVarChar_p2
select 'SelectMaxVarChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'FF' f1 from tversion union
select max( tvchar.cvchar ) from tvchar
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchApproximateNumeric_p5
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
-- SelectMinApproxNumeric_p1
select 'SelectMinApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
-- SelectMinApproxNumeric_p2
select 'SelectMinApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
-- SelectMinApproxNumeric_p3
select 'SelectMinApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
-- SelectMinApproxNumeric_p4
select 'SelectMinApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
-- SelectMinApproxNumeric_p5
select 'SelectMinApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
-- SelectMinApproxNumeric_p6
select 'SelectMinApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
-- SelectMinChar_p1
select 'SelectMinChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '                                ' f1 from tversion union
select min( vchar.cchar ) from vchar
) Q
group by
f1
) Q ) P;
-- SelectMinChar_p2
select 'SelectMinChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select '                                ' f1 from tversion union
select min( tchar.cchar ) from tchar
) Q
group by
f1
) Q ) P;
-- SelectMinExactNumeric_p1
select 'SelectMinExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
-- SelectMinExactNumeric_p2
select 'SelectMinExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchApproximateNumeric_p6
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
-- SelectMinExactNumeric_p3
select 'SelectMinExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
-- SelectMinExactNumeric_p4
select 'SelectMinExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
-- SelectMinInt_p1
select 'SelectMinInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
-- SelectMinInt_p2
select 'SelectMinInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
-- SelectMinInt_p3
select 'SelectMinInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
-- SelectMinInt_p4
select 'SelectMinInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
-- SelectMinInt_p5
select 'SelectMinInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
-- SelectMinInt_p6
select 'SelectMinInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select -1 f1 from tversion union
select min( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
-- SelectMinLit_p1
select 'SelectMinLit_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 'A' f1 from tversion union
select min( 'A' ) from tversion
) Q
group by
f1
) Q ) P;
-- SelectMinNullNumeric_p1
select 'SelectMinNullNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select min( cnnull ) from tversion
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchExactNumeric_p1
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
-- SelectMinNull_p1
select 'SelectMinNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select min( ccnull ) from tversion
) Q
group by
f1
) Q ) P;
-- SelectMinVarChar_p1
select 'SelectMinVarChar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select ' ' f1 from tversion union
select min( vvchar.cvchar ) from vvchar
) Q
group by
f1
) Q ) P;
-- SelectMinVarChar_p2
select 'SelectMinVarChar_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select ' ' f1 from tversion union
select min( tvchar.cvchar ) from tvchar
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopApproxNumeric_p1
select 'SelectStanDevPopApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopApproxNumeric_p2
select 'SelectStanDevPopApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopApproxNumeric_p3
select 'SelectStanDevPopApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopApproxNumeric_p4
select 'SelectStanDevPopApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopApproxNumeric_p5
select 'SelectStanDevPopApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopApproxNumeric_p6
select 'SelectStanDevPopApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.05975 f1 from tversion union
select stddev_pop( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopExactNumeric_p1
select 'SelectStanDevPopExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.04 f1 from tversion union
select stddev_pop( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
-- AbsCoreExactNumeric_p2
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
-- CaseBasicSearchExactNumeric_p2
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
-- SelectStanDevPopExactNumeric_p2
select 'SelectStanDevPopExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.04 f1 from tversion union
select stddev_pop( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopExactNumeric_p3
select 'SelectStanDevPopExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.04 f1 from tversion union
select stddev_pop( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopExactNumeric_p4
select 'SelectStanDevPopExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.04 f1 from tversion union
select stddev_pop( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopInt_p1
select 'SelectStanDevPopInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopInt_p2
select 'SelectStanDevPopInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopInt_p3
select 'SelectStanDevPopInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopInt_p4
select 'SelectStanDevPopInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopInt_p5
select 'SelectStanDevPopInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
-- SelectStanDevPopInt_p6
select 'SelectStanDevPopInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.38748 f1 from tversion union
select stddev_pop( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
-- SelectStar_p1
select 'SelectStar_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2,f3,f4,f5, count(*) c  from (
select 0 f1, 1 f2, '1.0   ' f3, null f4, null f5 from tversion union
select * from tversion
) Q
group by
f1,f2,f3,f4,f5
) Q ) P;
-- CaseBasicSearchExactNumeric_p3
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
-- SelectSumApproxNumeric_p1
select 'SelectSumApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
-- SelectSumApproxNumeric_p2
select 'SelectSumApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
-- SelectSumApproxNumeric_p3
select 'SelectSumApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
-- SelectSumApproxNumeric_p4
select 'SelectSumApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
-- SelectSumApproxNumeric_p5
select 'SelectSumApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
-- SelectSumApproxNumeric_p6
select 'SelectSumApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 9.9 f1 from tversion union
select sum( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
-- SelectSumExactNumeric_p1
select 'SelectSumExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.1 f1 from tversion union
select sum( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
-- SelectSumExactNumeric_p2
select 'SelectSumExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.1 f1 from tversion union
select sum( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
-- SelectSumExactNumeric_p3
select 'SelectSumExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.1 f1 from tversion union
select sum( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
-- SelectSumExactNumeric_p4
select 'SelectSumExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10.1 f1 from tversion union
select sum( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchExactNumeric_p4
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
-- SelectSumInt_p1
select 'SelectSumInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
-- SelectSumInt_p2
select 'SelectSumInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
-- SelectSumInt_p3
select 'SelectSumInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
-- SelectSumInt_p4
select 'SelectSumInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
-- SelectSumInt_p5
select 'SelectSumInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
-- SelectSumInt_p6
select 'SelectSumInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 10 f1 from tversion union
select sum( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
-- SelectThaiColumnConcat_p1
select 'SelectThaiColumnConcat_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 47 f1, '๛ก่ำ                               ' f2 from tversion union
select rnum, '๛' || c1 from tlth where rnum = 47
) Q
group by
f1,f2
) Q ) P;
-- SelectThaiColumnLower_p1
select 'SelectThaiColumnLower_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 47 f1, 'ก่ำ' f2 from tversion union
select rnum, lower(c1) from tlth where rnum=47
) Q
group by
f1,f2
) Q ) P;
-- SelectThaiColumnOrderByLocal_p1
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
-- SelectThaiColumnWhere_p1
select 'SelectThaiColumnWhere_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 62 f1, '๛                                     ' f2 from tversion union
select rnum, c1  from tlth where c1='๛'
) Q
group by
f1,f2
) Q ) P;
-- CaseBasicSearchInteger_p1
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
-- SelectThaiDistinctColumn_p1
select 'SelectThaiDistinctColumn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 75 f1 from tversion union
select count (distinct c1)  from tlth
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonEqualTo_p1
select 'SelectTimeComparisonEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '10:20:30' = time '10:20:30'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonGreaterThanOrEqualTo_p1
select 'SelectTimeComparisonGreaterThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' >= time '00:00:00.000'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonGreaterThanOrEqualTo_p2
select 'SelectTimeComparisonGreaterThanOrEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' >= time '12:00:00.000'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonGreaterThanOrEqualTo_p3
select 'SelectTimeComparisonGreaterThanOrEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' >= time '23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonGreaterThan_p1
select 'SelectTimeComparisonGreaterThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' > time '00:00:00.000'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonGreaterThan_p2
select 'SelectTimeComparisonGreaterThan_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' > time '12:00:00.000'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonGreaterThan_p3
select 'SelectTimeComparisonGreaterThan_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:40' > time '23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonLessThanOrEqualTo_p1
select 'SelectTimeComparisonLessThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '00:00:00' <= time '00:00:00.000'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonLessThanOrEqualTo_p2
select 'SelectTimeComparisonLessThanOrEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '00:00:00' <= time '12:00:00.000'
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchInteger_p2
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
-- SelectTimeComparisonLessThanOrEqualTo_p3
select 'SelectTimeComparisonLessThanOrEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '00:00:00' <= time '23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonLessThan_p1
select 'SelectTimeComparisonLessThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '00:00:00.000' < time '23:59:40'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonLessThan_p2
select 'SelectTimeComparisonLessThan_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '12:00:00.000' < time '23:59:40'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonLessThan_p3
select 'SelectTimeComparisonLessThan_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '23:59:30.123' < time '23:59:40'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonNotEqualTo_p1
select 'SelectTimeComparisonNotEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '22:20:30' <> time '00:00:00.000'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonNotEqualTo_p2
select 'SelectTimeComparisonNotEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '22:20:30' <> time '12:00:00.000'
) Q
group by
f1
) Q ) P;
-- SelectTimeComparisonNotEqualTo_p3
select 'SelectTimeComparisonNotEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where time '22:20:30' <> time '23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonEqualTo_p1
select 'SelectTimestampComparisonEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-01-01 00:00:00.0' = timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonEqualTo_p2
select 'SelectTimestampComparisonEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-01-01 12:00:00' = timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonEqualTo_p3
select 'SelectTimestampComparisonEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-01-01 23:59:30.123' = timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchInteger_p3
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
-- SelectTimestampComparisonEqualTo_p4
select 'SelectTimestampComparisonEqualTo_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-12-31 00:00:00' = timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonEqualTo_p5
select 'SelectTimestampComparisonEqualTo_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-12-31 12:00:00' = timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonEqualTo_p6
select 'SelectTimestampComparisonEqualTo_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2000-12-31 23:59:30.123' = timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThanOrEqualTo_p1
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThanOrEqualTo_p2
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThanOrEqualTo_p3
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThanOrEqualTo_p4
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThanOrEqualTo_p5
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThanOrEqualTo_p6
select 'SelectTimestampComparisonGreaterThanOrEqualTo_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' >= timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThan_p1
select 'SelectTimestampComparisonGreaterThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchInteger_p4
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
-- SelectTimestampComparisonGreaterThan_p2
select 'SelectTimestampComparisonGreaterThan_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThan_p3
select 'SelectTimestampComparisonGreaterThan_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThan_p4
select 'SelectTimestampComparisonGreaterThan_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThan_p5
select 'SelectTimestampComparisonGreaterThan_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonGreaterThan_p6
select 'SelectTimestampComparisonGreaterThan_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '2010-01-01 10:20:30' > timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThanOrEqualTo_p1
select 'SelectTimestampComparisonLessThanOrEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThanOrEqualTo_p2
select 'SelectTimestampComparisonLessThanOrEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThanOrEqualTo_p3
select 'SelectTimestampComparisonLessThanOrEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThanOrEqualTo_p4
select 'SelectTimestampComparisonLessThanOrEqualTo_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThanOrEqualTo_p5
select 'SelectTimestampComparisonLessThanOrEqualTo_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchInteger_p5
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
-- SelectTimestampComparisonLessThanOrEqualTo_p6
select 'SelectTimestampComparisonLessThanOrEqualTo_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <= timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThan_p1
select 'SelectTimestampComparisonLessThan_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThan_p2
select 'SelectTimestampComparisonLessThan_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThan_p3
select 'SelectTimestampComparisonLessThan_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThan_p4
select 'SelectTimestampComparisonLessThan_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThan_p5
select 'SelectTimestampComparisonLessThan_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonLessThan_p6
select 'SelectTimestampComparisonLessThan_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' < timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonNotEqualTo_p1
select 'SelectTimestampComparisonNotEqualTo_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-01-01 00:00:00.0'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonNotEqualTo_p2
select 'SelectTimestampComparisonNotEqualTo_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-01-01 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonNotEqualTo_p3
select 'SelectTimestampComparisonNotEqualTo_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-01-01 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- CaseBasicSearchInteger_p6
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
-- SelectTimestampComparisonNotEqualTo_p4
select 'SelectTimestampComparisonNotEqualTo_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-12-31 00:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonNotEqualTo_p5
select 'SelectTimestampComparisonNotEqualTo_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-12-31 12:00:00'
) Q
group by
f1
) Q ) P;
-- SelectTimestampComparisonNotEqualTo_p6
select 'SelectTimestampComparisonNotEqualTo_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 1 f1 from tversion union
select 1 from tversion where timestamp '1989-01-01 10:20:30' <> timestamp '2000-12-31 23:59:30.123'
) Q
group by
f1
) Q ) P;
-- SelectTurkishColumnConcat_p1
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
-- SelectTurkishColumnLower_p1
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
-- SelectTurkishColumnOrderByLocal_p1
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
-- SelectTurkishColumnWhere_p1
select 'SelectTurkishColumnWhere_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 35 f1, 'çıkmak                                ' f2 from tversion union
select rnum, c1  from tltr where c1='çıkmak'
) Q
group by
f1,f2
) Q ) P;
-- SelectTurkishDistinctColumn_p1
select 'SelectTurkishDistinctColumn_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 51 f1 from tversion union
select count (distinct c1)  from tltr
) Q
group by
f1
) Q ) P;
-- SelectVarPopApproxNumeric_p1
select 'SelectVarPopApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
-- SelectVarPopApproxNumeric_p2
select 'SelectVarPopApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
-- CaseComparisonsApproximateNumeric_p1
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
-- SelectVarPopApproxNumeric_p3
select 'SelectVarPopApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
-- SelectVarPopApproxNumeric_p4
select 'SelectVarPopApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
-- SelectVarPopApproxNumeric_p5
select 'SelectVarPopApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
-- SelectVarPopApproxNumeric_p6
select 'SelectVarPopApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.4816 f1 from tversion union
select var_pop( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
-- SelectVarPopExactNumeric_p1
select 'SelectVarPopExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.3216 f1 from tversion union
select var_pop( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
-- SelectVarPopExactNumeric_p2
select 'SelectVarPopExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.3216 f1 from tversion union
select var_pop( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
-- SelectVarPopExactNumeric_p3
select 'SelectVarPopExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.3216 f1 from tversion union
select var_pop( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
-- SelectVarPopExactNumeric_p4
select 'SelectVarPopExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 16.3216 f1 from tversion union
select var_pop( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
-- SelectVarPopInt_p1
select 'SelectVarPopInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
-- SelectVarPopInt_p2
select 'SelectVarPopInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
-- AbsCoreExactNumeric_p3
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
-- CaseComparisonsApproximateNumeric_p2
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
-- SelectVarPopInt_p3
select 'SelectVarPopInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
-- SelectVarPopInt_p4
select 'SelectVarPopInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
-- SelectVarPopInt_p5
select 'SelectVarPopInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
-- SelectVarPopInt_p6
select 'SelectVarPopInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 19.25 f1 from tversion union
select var_pop( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
-- SetPrecedenceNoBrackets_p1
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
-- SetPrecedenceUnionFirst_p1
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
-- SimpleCaseApproximateNumericElseDefaultsNULL_p1
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
-- SimpleCaseApproximateNumericElseDefaultsNULL_p2
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
-- SimpleCaseApproximateNumericElseDefaultsNULL_p3
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
-- SimpleCaseApproximateNumericElseDefaultsNULL_p4
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
-- CaseComparisonsApproximateNumeric_p3
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
-- SimpleCaseApproximateNumericElseDefaultsNULL_p5
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
-- SimpleCaseApproximateNumericElseDefaultsNULL_p6
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
-- SimpleCaseApproximateNumericElseExplicitNULL_p1
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
-- SimpleCaseApproximateNumericElseExplicitNULL_p2
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
-- SimpleCaseApproximateNumericElseExplicitNULL_p3
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
-- SimpleCaseApproximateNumericElseExplicitNULL_p4
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
-- SimpleCaseApproximateNumericElseExplicitNULL_p5
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
-- SimpleCaseApproximateNumericElseExplicitNULL_p6
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
-- SimpleCaseApproximateNumeric_p1
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
-- SimpleCaseApproximateNumeric_p2
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
-- CaseComparisonsApproximateNumeric_p4
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
-- SimpleCaseApproximateNumeric_p3
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
-- SimpleCaseApproximateNumeric_p4
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
-- SimpleCaseApproximateNumeric_p5
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
-- SimpleCaseApproximateNumeric_p6
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
-- SimpleCaseExactNumericElseDefaultsNULL_p1
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
-- SimpleCaseExactNumericElseDefaultsNULL_p2
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
-- SimpleCaseExactNumericElseDefaultsNULL_p3
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
-- SimpleCaseExactNumericElseDefaultsNULL_p4
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
-- SimpleCaseExactNumericElseExplicitNULL_p1
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
-- SimpleCaseExactNumericElseExplicitNULL_p2
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
-- CaseComparisonsApproximateNumeric_p5
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
-- SimpleCaseExactNumericElseExplicitNULL_p3
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
-- SimpleCaseExactNumericElseExplicitNULL_p4
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
-- SimpleCaseExactNumeric_p1
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
-- SimpleCaseExactNumeric_p2
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
-- SimpleCaseExactNumeric_p3
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
-- SimpleCaseExactNumeric_p4
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
-- SimpleCaseIntegerElseDefaultsNULL_p1
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
-- SimpleCaseIntegerElseDefaultsNULL_p2
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
-- SimpleCaseIntegerElseDefaultsNULL_p3
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
-- SimpleCaseIntegerElseDefaultsNULL_p4
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
-- CaseComparisonsApproximateNumeric_p6
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
-- SimpleCaseIntegerElseDefaultsNULL_p5
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
-- SimpleCaseIntegerElseDefaultsNULL_p6
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
-- SimpleCaseIntegerElseExplicitNULL_p1
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
-- SimpleCaseIntegerElseExplicitNULL_p2
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
-- SimpleCaseIntegerElseExplicitNULL_p3
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
-- SimpleCaseIntegerElseExplicitNULL_p4
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
-- SimpleCaseIntegerElseExplicitNULL_p5
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
-- SimpleCaseIntegerElseExplicitNULL_p6
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
-- SimpleCaseInteger_p1
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
-- SimpleCaseInteger_p2
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
-- CaseComparisonsExactNumeric_p1
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
-- SimpleCaseInteger_p3
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
-- SimpleCaseInteger_p4
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
-- SimpleCaseInteger_p5
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
-- SimpleCaseInteger_p6
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
-- SqrtCoreNull_p1
select 'SqrtCoreNull_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select null f1 from tversion union
select sqrt( null ) from tversion
) Q
group by
f1
) Q ) P;
-- SqrtCore_p1
select 'SqrtCore_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select sqrt( 4 ) from tversion
) Q
group by
f1
) Q ) P;
-- SqrtCore_p2
select 'SqrtCore_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select sqrt( 4.0e+0 ) from tversion
) Q
group by
f1
) Q ) P;
-- SqrtCore_p3
select 'SqrtCore_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 2 f1 from tversion union
select sqrt( 4.0 ) from tversion
) Q
group by
f1
) Q ) P;
-- StanDevApproxNumeric_p1
select 'StanDevApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
-- StanDevApproxNumeric_p2
select 'StanDevApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
-- CaseComparisonsExactNumeric_p2
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
-- StanDevApproxNumeric_p3
select 'StanDevApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
-- StanDevApproxNumeric_p4
select 'StanDevApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
-- StanDevApproxNumeric_p5
select 'StanDevApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
-- StanDevApproxNumeric_p6
select 'StanDevApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
-- StanDevExactNumeric_p1
select 'StanDevExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
-- StanDevExactNumeric_p2
select 'StanDevExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
-- StanDevExactNumeric_p3
select 'StanDevExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
-- StanDevExactNumeric_p4
select 'StanDevExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
-- StanDevInt_p1
select 'StanDevInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
-- StanDevInt_p2
select 'StanDevInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
-- CaseComparisonsExactNumeric_p3
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
-- StanDevInt_p3
select 'StanDevInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
-- StanDevInt_p4
select 'StanDevInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
-- StanDevInt_p5
select 'StanDevInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
-- StanDevInt_p6
select 'StanDevInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
-- StanDevSampApproxNumeric_p1
select 'StanDevSampApproxNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( vflt.cflt ) from vflt
) Q
group by
f1
) Q ) P;
-- StanDevSampApproxNumeric_p2
select 'StanDevSampApproxNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( tflt.cflt ) from tflt
) Q
group by
f1
) Q ) P;
-- StanDevSampApproxNumeric_p3
select 'StanDevSampApproxNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( vdbl.cdbl ) from vdbl
) Q
group by
f1
) Q ) P;
-- StanDevSampApproxNumeric_p4
select 'StanDevSampApproxNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( tdbl.cdbl ) from tdbl
) Q
group by
f1
) Q ) P;
-- StanDevSampApproxNumeric_p5
select 'StanDevSampApproxNumeric_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( vrl.crl ) from vrl
) Q
group by
f1
) Q ) P;
-- StanDevSampApproxNumeric_p6
select 'StanDevSampApproxNumeric_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.53894 f1 from tversion union
select stddev_samp( trl.crl ) from trl
) Q
group by
f1
) Q ) P;
-- CaseComparisonsExactNumeric_p4
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
-- StanDevSampExactNumeric_p1
select 'StanDevSampExactNumeric_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev_samp( vdec.cdec ) from vdec
) Q
group by
f1
) Q ) P;
-- StanDevSampExactNumeric_p2
select 'StanDevSampExactNumeric_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev_samp( tdec.cdec ) from tdec
) Q
group by
f1
) Q ) P;
-- StanDevSampExactNumeric_p3
select 'StanDevSampExactNumeric_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev_samp( vnum.cnum ) from vnum
) Q
group by
f1
) Q ) P;
-- StanDevSampExactNumeric_p4
select 'StanDevSampExactNumeric_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 4.516857 f1 from tversion union
select stddev_samp( tnum.cnum ) from tnum
) Q
group by
f1
) Q ) P;
-- StanDevSampInt_p1
select 'StanDevSampInt_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( vint.cint ) from vint
) Q
group by
f1
) Q ) P;
-- StanDevSampInt_p2
select 'StanDevSampInt_p2' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( tint.cint ) from tint
) Q
group by
f1
) Q ) P;
-- StanDevSampInt_p3
select 'StanDevSampInt_p3' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( vsint.csint ) from vsint
) Q
group by
f1
) Q ) P;
-- StanDevSampInt_p4
select 'StanDevSampInt_p4' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( tsint.csint ) from tsint
) Q
group by
f1
) Q ) P;
-- StanDevSampInt_p5
select 'StanDevSampInt_p5' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( vbint.cbint ) from vbint
) Q
group by
f1
) Q ) P;
-- StanDevSampInt_p6
select 'StanDevSampInt_p6' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1, count(*) c  from (
select 5.066228 f1 from tversion union
select stddev_samp( tbint.cbint ) from tbint
) Q
group by
f1
) Q ) P;
-- CaseComparisonsInteger_p1
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
-- StringComparisonEq_p1
select 'StringComparisonEq_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2='BB'
) Q
group by
f1,f2
) Q ) P;
-- StringComparisonGtEq_p1
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
-- StringComparisonGt_p1
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
-- StringComparisonLtEq_p1
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
-- StringComparisonLt_p1
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
-- StringComparisonNtEq_p1
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
-- StringComparisonNtEq_p2
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
-- StringPredicateBetween_p1
select 'StringPredicateBetween_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 between 'AA' and 'CC'
) Q
group by
f1,f2
) Q ) P;
-- StringPredicateIn_p1
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
-- StringPredicateLike_p1
select 'StringPredicateLike_p1' test_name_part, case when c = 1 then 1 else 0 end pass_ind from (
select count(distinct c) c from (
select f1,f2, count(*) c  from (
select 10 f1, 'BB' f2 from tversion union
select tjoin2.c1, tjoin2.c2 from tjoin2 where tjoin2.c2 like 'B%'
) Q
group by
f1,f2
) Q ) P;
