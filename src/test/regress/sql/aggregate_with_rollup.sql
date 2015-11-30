--- 
--- Drop existing table
---

DROP TABLE IF EXISTS dim_area;
DROP TABLE IF EXISTS dim_prod_type;
DROP TABLE IF EXISTS dmt_charge_month;
--
-- Name: dim_area; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE dim_area (
    area_id integer,
    dist_name character varying(16),
    dist_code character varying(16),
    city_name character varying(16),
    city_code character varying(16),
    prvc_name character varying(16),
    prvc_code character varying(16)
) DISTRIBUTED BY (area_id);


--
-- Name: dim_prod_type; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE dim_prod_type (
    prod_type_id integer,
    prod_type_name character varying(32)
) DISTRIBUTED BY (prod_type_id);

-- Name: dmt_charge_month; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE dmt_charge_month (
    prd_id integer,
    postpaid smallint,
    area_code character varying,
    voice_charge bigint,
    sms_charge bigint,
    data_charge bigint,
    vas_charge bigint,
    prod_type integer,
    stat_date character varying
) DISTRIBUTED BY (prd_id);

--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = public, pg_catalog;

SET default_with_oids = false;

--
-- Data for Name: dim_area; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

INSERT INTO dim_area VALUES (15, 'Zzzzz Hhh', 'ZH', 'Nnnn Bb', 'NB', 'Zzz Jjjjjj', 'ZJ');
INSERT INTO dim_area VALUES (9, 'Cccc Aa', 'CA', 'Ww Xx', 'WX', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (12, 'Xx Hh', 'XH', 'Hhhh Zzzz', 'HZ', 'Zzz Jjjjjj', 'ZJ');
INSERT INTO dim_area VALUES (6, 'Jjjj Nnnn', 'JN', 'Nnn Jjjjj', 'NJ', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (10, 'Xx Ssss', 'XS', 'Ww Xx', 'WX', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (16, 'Jjjjj Dddd', 'JD', 'Nnnn Bb', 'NB', 'Zzz Jjjjjj', 'ZJ');
INSERT INTO dim_area VALUES (3, 'Qqq Hhhh', 'QH', 'Nnn Jjjjj', 'NJ', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (13, 'Sssss Ccccc', 'SC', 'Hhhh Zzzz', 'HZ', 'Zzz Jjjjjj', 'ZJ');
INSERT INTO dim_area VALUES (5, 'Yy Hhh Ttt', 'YHT', 'Nnn Jjjjj', 'NJ', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (2, 'Jjjj Yy', 'JY', 'Nnn Jjjjj', 'NJ', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (8, 'Ggg Gggg', 'GG', 'Ttt Zzzz', 'TZ', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (11, 'Hhh Ssss', 'HS', 'Ww Xx', 'WX', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (7, 'Hhh Llll', 'HL', 'Ttt Zzzz', 'TZ', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (14, 'Xxx Cccc', 'XC', 'Hhhh Zzzz', 'HZ', 'Zzz Jjjjjj', 'ZJ');
INSERT INTO dim_area VALUES (4, 'Xxxx Ww', 'XW', 'Nnn Jjjjj', 'NJ', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (1, 'Ggg Lll', 'GL', 'Nnn Jjjjj', 'NJ', 'Jjjjj Ss', 'JS');
INSERT INTO dim_area VALUES (17, 'Jjjjj Bbb', 'JB', 'Nnnn Bb', 'NB', 'Zzz Jjjjjj', 'ZJ');


--
-- Greenplum Database database dump complete
--

--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = public, pg_catalog;

SET default_with_oids = false;

--
-- Data for Name: dim_prod_type; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

INSERT INTO dim_prod_type VALUES (6, 'cccc pppp ');
INSERT INTO dim_prod_type VALUES (3, 'jjjj a tttt');
INSERT INTO dim_prod_type VALUES (2, 'Cccccc fffff');
INSERT INTO dim_prod_type VALUES (5, 'ppppppp 5');
INSERT INTO dim_prod_type VALUES (4, 'Aaaaaaa pppppp');
INSERT INTO dim_prod_type VALUES (1, 'gggggg pppppp');


--
-- Greenplum Database database dump complete
--

--
-- Greenplum Database database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = public, pg_catalog;

SET default_with_oids = false;

--
-- Data for Name: dmt_charge_month; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

INSERT INTO dmt_charge_month VALUES (41, 0, 'JY', 3248270, 2365120, 5455080, 1431302, 2, '201505');
INSERT INTO dmt_charge_month VALUES (249, 1, 'XS', 2592980, 3787880, 3366820, 6543278, 5, '201505');
INSERT INTO dmt_charge_month VALUES (270, 1, 'XS', 3288200, 3777800, 3347740, 6544420, 1, '201506');
INSERT INTO dmt_charge_month VALUES (73, 0, 'SC', 3869860, 1098460, 6767860, 1689886, 4, '201506');
INSERT INTO dmt_charge_month VALUES (89, 0, 'QH', 4435100, 2023510, 5500300, 483105, 5, '201508');
INSERT INTO dmt_charge_month VALUES (105, 0, 'JN', 4436900, 2023690, 5602100, 483123, 1, '201509');
INSERT INTO dmt_charge_month VALUES (302, 1, 'QH', 4456700, 1375670, 5792000, 483322, 3, '201510');
INSERT INTO dmt_charge_month VALUES (286, 1, 'SC', 3582720, 395520, 7180720, 1691172, 2, '201507');
INSERT INTO dmt_charge_month VALUES (233, 1, 'HL', 6390610, 3365960, 3878660, 5778326, 4, '201504');
INSERT INTO dmt_charge_month VALUES (57, 0, 'XS', 2023230, 1732320, 3544420, 5423378, 4, '201504');
INSERT INTO dmt_charge_month VALUES (25, 0, 'XH', 1691500, 2488300, 3479500, 1235050, 1, '201503');
INSERT INTO dmt_charge_month VALUES (201, 1, 'GL', 1653650, 2594620, 3363880, 3231822, 2, '201502');
INSERT INTO dmt_charge_month VALUES (9, 0, 'CA', 2537520, 2912580, 6669050, 4328098, 5, '201502');
INSERT INTO dmt_charge_month VALUES (217, 1, 'XW', 4448200, 1374820, 5783500, 483237, 2, '201510');
INSERT INTO dmt_charge_month VALUES (15, 0, 'JY', 3946350, 2442100, 5438860, 1432730, 1, '201506');
INSERT INTO dmt_charge_month VALUES (255, 1, 'GL', 2723230, 1332320, 4344420, 5423378, 1, '201506');
INSERT INTO dmt_charge_month VALUES (260, 1, 'HL', 3632300, 1339320, 4378420, 5423378, 2, '201506');
INSERT INTO dmt_charge_month VALUES (63, 0, 'QH', 3056320, 3565360, 5436330, 6576566, 4, '201506');
INSERT INTO dmt_charge_month VALUES (95, 0, 'CA', 4435700, 2023570, 5500900, 483111, 1, '201508');
INSERT INTO dmt_charge_month VALUES (111, 0, 'XH', 4437500, 1623750, 5602700, 483129, 2, '201509');
INSERT INTO dmt_charge_month VALUES (308, 1, 'CA', 4457200, 1625720, 5622400, 483326, 4, '201509');
INSERT INTO dmt_charge_month VALUES (292, 1, 'JN', 4455700, 1375570, 5791000, 483312, 3, '201510');
INSERT INTO dmt_charge_month VALUES (79, 0, 'JN', 6980840, 2742980, 4939640, 3446018, 5, '201507');
INSERT INTO dmt_charge_month VALUES (276, 1, 'QH', 2769180, 2862420, 5849190, 6577852, 2, '201507');
INSERT INTO dmt_charge_month VALUES (239, 1, 'SC', 3182280, 1117280, 5988080, 1689158, 5, '201504');
INSERT INTO dmt_charge_month VALUES (31, 0, 'YHT', 7370590, 3564520, 1651750, 3241582, 2, '201504');
INSERT INTO dmt_charge_month VALUES (223, 1, 'XS', 2591060, 3864860, 2500600, 6544706, 4, '201503');
INSERT INTO dmt_charge_month VALUES (47, 0, 'GG', 4431200, 1373120, 5766500, 483067, 2, '201510');
INSERT INTO dmt_charge_month VALUES (207, 1, 'HL', 4447200, 1874720, 5782500, 483227, 2, '201510');
INSERT INTO dmt_charge_month VALUES (246, 1, 'HL', 6392970, 3367220, 4687520, 5778912, 2, '201505');
INSERT INTO dmt_charge_month VALUES (70, 0, 'XS', 3278200, 3767800, 3337740, 6543420, 1, '201506');
INSERT INTO dmt_charge_month VALUES (273, 1, 'SC', 3879860, 1108460, 6777860, 1690886, 4, '201506');
INSERT INTO dmt_charge_month VALUES (289, 1, 'QH', 4455100, 2025510, 5520300, 483305, 5, '201508');
INSERT INTO dmt_charge_month VALUES (305, 1, 'JN', 4456900, 2025690, 5622100, 483323, 1, '201509');
INSERT INTO dmt_charge_month VALUES (102, 0, 'QH', 4436700, 1373670, 5772000, 483122, 3, '201510');
INSERT INTO dmt_charge_month VALUES (86, 0, 'SC', 3572720, 385520, 7170720, 1690172, 2, '201507');
INSERT INTO dmt_charge_month VALUES (38, 0, 'XH', 1691060, 2410060, 3536860, 1233036, 4, '201504');
INSERT INTO dmt_charge_month VALUES (230, 1, 'XW', 5592190, 2917500, 5786860, 5977960, 1, '201504');
INSERT INTO dmt_charge_month VALUES (214, 1, 'GL', 2256510, 2551680, 5516740, 2232108, 5, '201503');
INSERT INTO dmt_charge_month VALUES (54, 0, 'SC', 2023230, 1732320, 3494420, 5423378, 2, '201503');
INSERT INTO dmt_charge_month VALUES (6, 0, 'JN', 6077980, 2585920, 3536780, 3445732, 2, '201502');
INSERT INTO dmt_charge_month VALUES (22, 0, 'CA', 4428700, 1372870, 5764000, 483042, 2, '201510');
INSERT INTO dmt_charge_month VALUES (257, 1, 'HS', 4452200, 1375220, 5787500, 483277, 4, '201510');
INSERT INTO dmt_charge_month VALUES (44, 0, 'YHT', 7372950, 3565780, 2460610, 3242168, 5, '201505');
INSERT INTO dmt_charge_month VALUES (60, 0, 'GG', 2023230, 1732320, 4344420, 5423378, 4, '201505');
INSERT INTO dmt_charge_month VALUES (263, 1, 'QH', 3066320, 3575360, 5446330, 6577566, 4, '201506');
INSERT INTO dmt_charge_month VALUES (295, 1, 'CA', 4455700, 2025570, 5520900, 483311, 1, '201508');
INSERT INTO dmt_charge_month VALUES (108, 0, 'CA', 4437200, 2023720, 5602400, 483126, 4, '201509');
INSERT INTO dmt_charge_month VALUES (311, 1, 'XH', 4457500, 2025750, 5622700, 483329, 2, '201509');
INSERT INTO dmt_charge_month VALUES (92, 0, 'JN', 4435700, 1373570, 5771000, 483112, 3, '201510');
INSERT INTO dmt_charge_month VALUES (279, 1, 'JN', 6990840, 2752980, 4949640, 3447018, 5, '201507');
INSERT INTO dmt_charge_month VALUES (76, 0, 'QH', 2759180, 2852420, 5839190, 6576852, 2, '201507');
INSERT INTO dmt_charge_month VALUES (28, 0, 'JY', 3245910, 2363860, 4646220, 1430716, 4, '201504');
INSERT INTO dmt_charge_month VALUES (236, 1, 'XS', 2590620, 3786620, 2557960, 6542692, 2, '201504');
INSERT INTO dmt_charge_month VALUES (220, 1, 'HL', 6391050, 3444200, 3821300, 5780340, 1, '201503');
INSERT INTO dmt_charge_month VALUES (204, 1, 'XW', 5089770, 2038680, 5576640, 5979688, 5, '201502');
INSERT INTO dmt_charge_month VALUES (12, 0, 'XH', 4427700, 1372770, 5763000, 483032, 2, '201510');
INSERT INTO dmt_charge_month VALUES (252, 1, 'SC', 4451700, 1375170, 5787000, 483272, 2, '201510');
INSERT INTO dmt_charge_month VALUES (250, 1, 'HS', 2993200, 3877000, 2907500, 2497700, 1, '201505');
INSERT INTO dmt_charge_month VALUES (58, 0, 'XW', 2023230, 1732320, 4344420, 5423378, 2, '201505');
INSERT INTO dmt_charge_month VALUES (261, 1, 'GL', 2853650, 3464620, 4363880, 3231822, 2, '201506');
INSERT INTO dmt_charge_month VALUES (90, 0, 'XW', 4435200, 2023520, 5500400, 483106, 1, '201508');
INSERT INTO dmt_charge_month VALUES (293, 1, 'HL', 4455500, 2025550, 5520700, 483309, 4, '201508');
INSERT INTO dmt_charge_month VALUES (106, 0, 'HL', 4437000, 2023700, 5602200, 483124, 2, '201509');
INSERT INTO dmt_charge_month VALUES (309, 1, 'XS', 4457300, 2025730, 5622500, 483327, 5, '201509');
INSERT INTO dmt_charge_month VALUES (277, 1, 'XW', 4454200, 1375420, 5789500, 483297, 3, '201510');
INSERT INTO dmt_charge_month VALUES (74, 0, 'GL', 2646510, 1741680, 6756740, 2231108, 5, '201507');
INSERT INTO dmt_charge_month VALUES (234, 1, 'GG', 3091160, 1610480, 5100870, 2345048, 5, '201504');
INSERT INTO dmt_charge_month VALUES (26, 0, 'SC', 3172720, 1185520, 5920720, 1690172, 2, '201503');
INSERT INTO dmt_charge_month VALUES (218, 1, 'YHT', 7381030, 3652760, 1604390, 3244596, 4, '201503');
INSERT INTO dmt_charge_month VALUES (10, 0, 'XS', 2078200, 2897800, 2337740, 6543420, 1, '201502');
INSERT INTO dmt_charge_month VALUES (42, 0, 'QH', 4430700, 1373070, 5766000, 483062, 2, '201510');
INSERT INTO dmt_charge_month VALUES (202, 1, 'JY', 4446700, 1374670, 5782000, 483222, 2, '201510');
INSERT INTO dmt_charge_month VALUES (45, 0, 'JN', 6582760, 3466000, 4555860, 3444590, 1, '201505');
INSERT INTO dmt_charge_month VALUES (61, 0, 'GL', 2843650, 3454620, 4353880, 3230822, 2, '201506');
INSERT INTO dmt_charge_month VALUES (253, 1, 'CA', 2723230, 1732320, 4344420, 5423378, 4, '201506');
INSERT INTO dmt_charge_month VALUES (93, 0, 'HL', 4435500, 2023550, 5500700, 483109, 4, '201508');
INSERT INTO dmt_charge_month VALUES (290, 1, 'XW', 4455200, 2025520, 5520400, 483306, 1, '201508');
INSERT INTO dmt_charge_month VALUES (109, 0, 'XS', 4437300, 2023730, 5602500, 483127, 5, '201509');
INSERT INTO dmt_charge_month VALUES (306, 1, 'HL', 4457000, 2025700, 5622200, 483324, 2, '201509');
INSERT INTO dmt_charge_month VALUES (77, 0, 'XW', 4434200, 1373420, 5769500, 483097, 3, '201510');
INSERT INTO dmt_charge_month VALUES (274, 1, 'GL', 2656510, 1751680, 6766740, 2232108, 5, '201507');
INSERT INTO dmt_charge_month VALUES (29, 0, 'QH', 2358740, 3574180, 4646550, 6574838, 5, '201504');
INSERT INTO dmt_charge_month VALUES (221, 1, 'GG', 3091600, 1688720, 5043510, 2347062, 2, '201503');
INSERT INTO dmt_charge_month VALUES (13, 0, 'SC', 2669860, 228460, 5767860, 1689886, 4, '201502');
INSERT INTO dmt_charge_month VALUES (205, 1, 'YHT', 6878170, 2695700, 1451530, 3244310, 1, '201502');
INSERT INTO dmt_charge_month VALUES (237, 1, 'HS', 4450200, 1375020, 5785500, 483257, 2, '201510');
INSERT INTO dmt_charge_month VALUES (258, 1, 'SC', 4452300, 1375230, 5787600, 483278, 4, '201510');
INSERT INTO dmt_charge_month VALUES (251, 1, 'XH', 1703420, 2421320, 4355720, 1234622, 2, '201505');
INSERT INTO dmt_charge_month VALUES (43, 0, 'XW', 5584550, 2908760, 6585720, 5977546, 4, '201505');
INSERT INTO dmt_charge_month VALUES (59, 0, 'GG', 2023230, 1732320, 4344420, 5423378, 2, '201505');
INSERT INTO dmt_charge_month VALUES (75, 0, 'JY', 3946350, 2442100, 5438860, 1432730, 2, '201506');
INSERT INTO dmt_charge_month VALUES (256, 1, 'HS', 2723230, 1332320, 4378420, 5423378, 1, '201506');
INSERT INTO dmt_charge_month VALUES (91, 0, 'YHT', 4435300, 2023530, 5500500, 483107, 2, '201508');
INSERT INTO dmt_charge_month VALUES (288, 1, 'JY', 4455000, 2025500, 5520200, 483304, 4, '201508');
INSERT INTO dmt_charge_month VALUES (304, 1, 'YHT', 4456800, 2025680, 5622000, 483322, 5, '201509');
INSERT INTO dmt_charge_month VALUES (107, 0, 'GG', 4437200, 1373720, 5772500, 483127, 3, '201510');
INSERT INTO dmt_charge_month VALUES (272, 1, 'XH', 4453700, 1375370, 5789000, 483292, 3, '201510');
INSERT INTO dmt_charge_month VALUES (235, 1, 'CA', 3049940, 3801400, 6889270, 4327370, 1, '201504');
INSERT INTO dmt_charge_month VALUES (219, 1, 'JN', 6590840, 3552980, 3699640, 3447018, 5, '201503');
INSERT INTO dmt_charge_month VALUES (203, 1, 'QH', 1866320, 2705360, 4446330, 6577566, 4, '201502');
INSERT INTO dmt_charge_month VALUES (11, 0, 'HS', 2478420, 2986920, 1878420, 2497842, 2, '201502');
INSERT INTO dmt_charge_month VALUES (27, 0, 'GL', 4429200, 1372920, 5764500, 483047, 2, '201510');
INSERT INTO dmt_charge_month VALUES (40, 0, 'GL', 2148430, 3464700, 4372960, 3229680, 1, '201505');
INSERT INTO dmt_charge_month VALUES (248, 1, 'CA', 3052300, 3802660, 7698130, 4327956, 4, '201505');
INSERT INTO dmt_charge_month VALUES (275, 1, 'JY', 3956350, 2452100, 5448860, 1433730, 2, '201506');
INSERT INTO dmt_charge_month VALUES (259, 1, 'HL', 3632300, 1339320, 4378420, 5423378, 1, '201506');
INSERT INTO dmt_charge_month VALUES (88, 0, 'JY', 4435000, 2023500, 5500200, 483104, 4, '201508');
INSERT INTO dmt_charge_month VALUES (291, 1, 'YHT', 4455300, 2025530, 5520500, 483307, 2, '201508');
INSERT INTO dmt_charge_month VALUES (104, 0, 'YHT', 4436800, 2023680, 5602000, 483122, 5, '201509');
INSERT INTO dmt_charge_month VALUES (72, 0, 'XH', 4433700, 1373370, 5769000, 483092, 3, '201510');
INSERT INTO dmt_charge_month VALUES (56, 0, 'HL', 2023230, 1732320, 3544420, 5423378, 2, '201504');
INSERT INTO dmt_charge_month VALUES (56, 0, 'XS', 2023230, 1732320, 3544420, 5423378, 1, '201504');
INSERT INTO dmt_charge_month VALUES (216, 1, 'QH', 2369180, 3662420, 4599190, 6577852, 2, '201503');
INSERT INTO dmt_charge_month VALUES (24, 0, 'HS', 2981280, 3943980, 2031280, 2498128, 5, '201503');
INSERT INTO dmt_charge_month VALUES (8, 0, 'GG', 2578740, 721660, 4880650, 2345776, 4, '201502');
INSERT INTO dmt_charge_month VALUES (232, 1, 'JN', 4449700, 1374970, 5785000, 483252, 2, '201510');
INSERT INTO dmt_charge_month VALUES (307, 1, 'GG', 4457200, 1375720, 5792500, 483327, 4, '201510');
INSERT INTO dmt_charge_month VALUES (245, 1, 'JN', 6592760, 3476000, 4565860, 3445590, 1, '201505');
INSERT INTO dmt_charge_month VALUES (266, 1, 'JN', 7287980, 3465920, 4546780, 3446732, 2, '201506');
INSERT INTO dmt_charge_month VALUES (298, 1, 'XH', 4456000, 2025600, 5521200, 483314, 4, '201508');
INSERT INTO dmt_charge_month VALUES (101, 0, 'JY', 4436500, 2023650, 5601700, 483119, 2, '201509');
INSERT INTO dmt_charge_month VALUES (282, 1, 'CA', 4454700, 1375470, 5790000, 483302, 3, '201510');
INSERT INTO dmt_charge_month VALUES (85, 0, 'XH', 2091500, 1688300, 4729500, 1235050, 1, '201507');
INSERT INTO dmt_charge_month VALUES (229, 1, 'QH', 2368740, 3584180, 4656550, 6575838, 5, '201504');
INSERT INTO dmt_charge_month VALUES (21, 0, 'GG', 3081600, 1678720, 5033510, 2346062, 2, '201503');
INSERT INTO dmt_charge_month VALUES (213, 1, 'SC', 2679860, 238460, 5777860, 1690886, 4, '201502');
INSERT INTO dmt_charge_month VALUES (5, 0, 'YHT', 6868170, 2685700, 1441530, 3243310, 1, '201502');
INSERT INTO dmt_charge_month VALUES (37, 0, 'HS', 4430200, 1373020, 5765500, 483057, 2, '201510');
INSERT INTO dmt_charge_month VALUES (53, 0, 'SC', 4431800, 1373180, 5767100, 483073, 4, '201510');
INSERT INTO dmt_charge_month VALUES (69, 0, 'CA', 4433400, 1373340, 5768700, 483089, 4, '201510');
INSERT INTO dmt_charge_month VALUES (50, 0, 'HS', 2983200, 3867000, 2897500, 2496700, 1, '201505');
INSERT INTO dmt_charge_month VALUES (66, 0, 'JN', 7277980, 3455920, 4536780, 3445732, 2, '201506');
INSERT INTO dmt_charge_month VALUES (269, 1, 'CA', 3747520, 3792580, 7679050, 4329098, 5, '201506');
INSERT INTO dmt_charge_month VALUES (98, 0, 'XH', 4436000, 2023600, 5501200, 483114, 4, '201508');
INSERT INTO dmt_charge_month VALUES (301, 1, 'JY', 4456500, 2025650, 5621700, 483319, 2, '201509');
INSERT INTO dmt_charge_month VALUES (82, 0, 'CA', 4434700, 1373470, 5770000, 483102, 3, '201510');
INSERT INTO dmt_charge_month VALUES (285, 1, 'XH', 2101500, 1698300, 4739500, 1236050, 1, '201507');
INSERT INTO dmt_charge_month VALUES (34, 0, 'GG', 3081160, 1600480, 5090870, 2344048, 5, '201504');
INSERT INTO dmt_charge_month VALUES (226, 1, 'SC', 3182720, 1195520, 5930720, 1691172, 2, '201503');
INSERT INTO dmt_charge_month VALUES (18, 0, 'YHT', 7371030, 3642760, 1594390, 3243596, 4, '201503');
INSERT INTO dmt_charge_month VALUES (210, 1, 'XS', 2088200, 2907800, 2347740, 6544420, 1, '201502');
INSERT INTO dmt_charge_month VALUES (2, 0, 'JY', 4426700, 1372670, 5762000, 483022, 2, '201510');
INSERT INTO dmt_charge_month VALUES (242, 1, 'QH', 4450700, 1375070, 5786000, 483262, 2, '201510');
INSERT INTO dmt_charge_month VALUES (51, 0, 'XH', 1693420, 2411320, 4345720, 1233622, 2, '201505');
INSERT INTO dmt_charge_month VALUES (243, 1, 'XW', 5594550, 2918760, 6595720, 5978546, 4, '201505');
INSERT INTO dmt_charge_month VALUES (264, 1, 'XW', 6289770, 2908680, 6576640, 5979688, 5, '201506');
INSERT INTO dmt_charge_month VALUES (99, 0, 'SC', 4436100, 2023610, 5501300, 483115, 5, '201508');
INSERT INTO dmt_charge_month VALUES (296, 1, 'XS', 4455800, 2025580, 5521000, 483312, 2, '201508');
INSERT INTO dmt_charge_month VALUES (67, 0, 'HL', 4433200, 1373320, 5768500, 483087, 3, '201510');
INSERT INTO dmt_charge_month VALUES (312, 1, 'SC', 4457700, 1375770, 5793000, 483332, 3, '201510');
INSERT INTO dmt_charge_month VALUES (280, 1, 'HL', 6791050, 2644200, 5071300, 5780340, 1, '201507');
INSERT INTO dmt_charge_month VALUES (83, 0, 'XS', 2981060, 3054860, 3740600, 6543706, 4, '201507');
INSERT INTO dmt_charge_month VALUES (35, 0, 'CA', 3039940, 3791400, 6879270, 4326370, 1, '201504');
INSERT INTO dmt_charge_month VALUES (19, 0, 'JN', 6580840, 3542980, 3689640, 3446018, 5, '201503');
INSERT INTO dmt_charge_month VALUES (3, 0, 'QH', 1856320, 2695360, 4436330, 6576566, 4, '201502');
INSERT INTO dmt_charge_month VALUES (211, 1, 'HS', 2488420, 2996920, 1888420, 2498842, 2, '201502');
INSERT INTO dmt_charge_month VALUES (227, 1, 'GL', 4449200, 1374920, 5784500, 483247, 2, '201510');
INSERT INTO dmt_charge_month VALUES (240, 1, 'GL', 2158430, 3474700, 4382960, 3230680, 1, '201505');
INSERT INTO dmt_charge_month VALUES (48, 0, 'CA', 3042300, 3792660, 7688130, 4326956, 4, '201505');
INSERT INTO dmt_charge_month VALUES (64, 0, 'XW', 6279770, 2898680, 6566640, 5978688, 5, '201506');
INSERT INTO dmt_charge_month VALUES (96, 0, 'XS', 4435800, 2023580, 5501000, 483112, 2, '201508');
INSERT INTO dmt_charge_month VALUES (299, 1, 'SC', 4456100, 2025610, 5521300, 483315, 5, '201508');
INSERT INTO dmt_charge_month VALUES (112, 0, 'SC', 4437700, 1373770, 5773000, 483132, 3, '201510');
INSERT INTO dmt_charge_month VALUES (267, 1, 'HL', 4453200, 1375320, 5788500, 483287, 3, '201510');
INSERT INTO dmt_charge_month VALUES (80, 0, 'HL', 6781050, 2634200, 5061300, 5779340, 1, '201507');
INSERT INTO dmt_charge_month VALUES (283, 1, 'XS', 2991060, 3064860, 3750600, 6544706, 4, '201507');
INSERT INTO dmt_charge_month VALUES (16, 0, 'QH', 2359180, 3652420, 4589190, 6576852, 2, '201503');
INSERT INTO dmt_charge_month VALUES (224, 1, 'HS', 2991280, 3953980, 2041280, 2499128, 5, '201503');
INSERT INTO dmt_charge_month VALUES (208, 1, 'GG', 2588740, 731660, 4890650, 2346776, 4, '201502');
INSERT INTO dmt_charge_month VALUES (32, 0, 'JN', 4429700, 1372970, 5765000, 483052, 2, '201510');
INSERT INTO dmt_charge_month VALUES (215, 1, 'JY', 3956350, 2452100, 5448860, 1433730, 1, '201506');
INSERT INTO dmt_charge_month VALUES (71, 0, 'HS', 3678420, 3856920, 2878420, 2497842, 2, '201506');
INSERT INTO dmt_charge_month VALUES (268, 1, 'GG', 3788740, 1601660, 5890650, 2346776, 4, '201506');
INSERT INTO dmt_charge_month VALUES (103, 0, 'XW', 4436700, 2023670, 5601900, 483121, 4, '201509');
INSERT INTO dmt_charge_month VALUES (300, 1, 'GL', 4456400, 2025640, 5621600, 483318, 1, '201509');
INSERT INTO dmt_charge_month VALUES (87, 0, 'GL', 4435200, 1373520, 5770500, 483107, 3, '201510');
INSERT INTO dmt_charge_month VALUES (284, 1, 'HS', 3391280, 3153980, 3291280, 2499128, 5, '201507');
INSERT INTO dmt_charge_month VALUES (39, 0, 'SC', 3172280, 1107280, 5978080, 1688158, 5, '201504');
INSERT INTO dmt_charge_month VALUES (231, 1, 'YHT', 7380590, 3574520, 1661750, 3242582, 2, '201504');
INSERT INTO dmt_charge_month VALUES (23, 0, 'XS', 2581060, 3854860, 2490600, 6543706, 4, '201503');
INSERT INTO dmt_charge_month VALUES (55, 0, 'SC', 2023230, 1732320, 3494420, 5423378, 1, '201503');
INSERT INTO dmt_charge_month VALUES (55, 0, 'HL', 2023230, 1732320, 3494420, 5423378, 2, '201503');
INSERT INTO dmt_charge_month VALUES (247, 1, 'GG', 4451200, 1375120, 5786500, 483267, 2, '201510');
INSERT INTO dmt_charge_month VALUES (7, 0, 'HL', 4427200, 1372720, 5762500, 483027, 2, '201510');
INSERT INTO dmt_charge_month VALUES (244, 1, 'YHT', 7382950, 3575780, 2470610, 3243168, 5, '201505');
INSERT INTO dmt_charge_month VALUES (271, 1, 'HS', 3688420, 3866920, 2888420, 2498842, 2, '201506');
INSERT INTO dmt_charge_month VALUES (68, 0, 'GG', 3778740, 1591660, 5880650, 2345776, 2, '201506');
INSERT INTO dmt_charge_month VALUES (100, 0, 'GL', 4436400, 2023640, 5601600, 483118, 1, '201509');
INSERT INTO dmt_charge_month VALUES (303, 1, 'XW', 4456700, 2025670, 5621900, 483321, 4, '201509');
INSERT INTO dmt_charge_month VALUES (287, 1, 'GL', 4455200, 1375520, 5790500, 483307, 3, '201510');
INSERT INTO dmt_charge_month VALUES (84, 0, 'HS', 3381280, 3143980, 3281280, 2498128, 2, '201507');
INSERT INTO dmt_charge_month VALUES (228, 1, 'JY', 3255910, 2373860, 4656220, 1431716, 4, '201504');
INSERT INTO dmt_charge_month VALUES (36, 0, 'XS', 2580620, 3776620, 2547960, 6541692, 2, '201504');
INSERT INTO dmt_charge_month VALUES (20, 0, 'HL', 6381050, 3434200, 3811300, 5779340, 1, '201503');
INSERT INTO dmt_charge_month VALUES (4, 0, 'XW', 5079770, 2028680, 5566640, 5978688, 5, '201502');
INSERT INTO dmt_charge_month VALUES (52, 0, 'SC', 4431700, 1373170, 5767000, 483072, 2, '201510');
INSERT INTO dmt_charge_month VALUES (212, 1, 'XH', 4447700, 1374770, 5783000, 483232, 2, '201510');
INSERT INTO dmt_charge_month VALUES (241, 1, 'JY', 3258270, 2375120, 5465080, 1432302, 2, '201505');
INSERT INTO dmt_charge_month VALUES (49, 0, 'XS', 2582980, 3777880, 3356820, 6542278, 5, '201505');
INSERT INTO dmt_charge_month VALUES (65, 0, 'YHT', 8068170, 3555700, 2441530, 3243310, 1, '201506');
INSERT INTO dmt_charge_month VALUES (294, 1, 'GG', 4455600, 2025560, 5520800, 483310, 5, '201508');
INSERT INTO dmt_charge_month VALUES (310, 1, 'HS', 4457400, 2025740, 5622600, 483328, 1, '201509');
INSERT INTO dmt_charge_month VALUES (97, 0, 'HS', 4436200, 1373620, 5771500, 483117, 3, '201510');
INSERT INTO dmt_charge_month VALUES (262, 1, 'JY', 4452700, 1375270, 5788000, 483282, 3, '201510');
INSERT INTO dmt_charge_month VALUES (278, 1, 'YHT', 7781030, 2852760, 2854390, 3244596, 4, '201507');
INSERT INTO dmt_charge_month VALUES (81, 0, 'GG', 3481600, 878720, 6283510, 2346062, 2, '201507');
INSERT INTO dmt_charge_month VALUES (33, 0, 'HL', 6380610, 3355960, 3868660, 5777326, 4, '201504');
INSERT INTO dmt_charge_month VALUES (225, 1, 'XH', 1701500, 2498300, 3489500, 1236050, 1, '201503');
INSERT INTO dmt_charge_month VALUES (1, 0, 'GL', 1643650, 2584620, 3353880, 3230822, 2, '201502');
INSERT INTO dmt_charge_month VALUES (209, 1, 'CA', 2547520, 2922580, 6679050, 4329098, 5, '201502');
INSERT INTO dmt_charge_month VALUES (17, 0, 'XW', 4428200, 1372820, 5763500, 483037, 2, '201510');
INSERT INTO dmt_charge_month VALUES (46, 0, 'HL', 6382970, 3357220, 4677520, 5777912, 2, '201505');
INSERT INTO dmt_charge_month VALUES (265, 1, 'YHT', 8078170, 3565700, 2451530, 3244310, 1, '201506');
INSERT INTO dmt_charge_month VALUES (254, 1, 'CA', 2723230, 1732320, 4344420, 5423378, 1, '201506');
INSERT INTO dmt_charge_month VALUES (94, 0, 'GG', 4435600, 2023560, 5500800, 483110, 5, '201508');
INSERT INTO dmt_charge_month VALUES (110, 0, 'HS', 4437400, 2023740, 5602600, 483128, 1, '201509');
INSERT INTO dmt_charge_month VALUES (62, 0, 'JY', 4432700, 1373270, 5768000, 483082, 3, '201510');
INSERT INTO dmt_charge_month VALUES (297, 1, 'HS', 4456200, 1375620, 5791500, 483317, 3, '201510');
INSERT INTO dmt_charge_month VALUES (78, 0, 'YHT', 7771030, 2842760, 2844390, 3243596, 4, '201507');
INSERT INTO dmt_charge_month VALUES (281, 1, 'GG', 3491600, 888720, 6293510, 2347062, 2, '201507');
INSERT INTO dmt_charge_month VALUES (238, 1, 'XH', 1701060, 2420060, 3546860, 1234036, 4, '201504');
INSERT INTO dmt_charge_month VALUES (30, 0, 'XW', 5582190, 2907500, 5776860, 5976960, 1, '201504');
INSERT INTO dmt_charge_month VALUES (14, 0, 'GL', 2246510, 2541680, 5506740, 2231108, 5, '201503');
INSERT INTO dmt_charge_month VALUES (206, 1, 'JN', 6087980, 2595920, 3546780, 3446732, 2, '201502');
INSERT INTO dmt_charge_month VALUES (222, 1, 'CA', 4448700, 1374870, 5784000, 483242, 2, '201510');


--
-- Greenplum Database database dump complete
--

--
-- Following sql cause seg fault
--

SELECT T5.*
FROM
  (SELECT T_protype,T_area_dist,T_voice, T_data,  T_cal_indcat,
                                                  T_prd_id,
                                                  RANK() OVER(
                                                              ORDER BY CAL_1981 DESC) AS CAL_1981
   FROM
     (SELECT T_protype, T_area_dist, SUM(T_voice) T_voice, SUM(T_data) T_data, SUM(T_cal_indcat) T_cal_indcat,  SUM(T_prd_id) T_prd_id,
                                                                                                                SUM(CAL_1981) CAL_1981
      FROM
        (SELECT DIM_PROD_TYPE.prod_type_name T_protype,
                DIM_AREA.dist_name           T_area_dist,
                                             T_voice,
                                             T_data,
                                             T_cal_indcat,
                                             T_prd_id,
                                             CAL_1981
         FROM
           (SELECT T_protype,
                   T_area_dist,
                   SUM(T_voice) T_voice,
                   SUM(T_data) T_data,
                   SUM(T_cal_indcat) T_cal_indcat,
                   SUM(T_prd_id) T_prd_id,
                   SUM(T_cal_indcat) CAL_1981
            FROM
              (SELECT T1.*
               FROM
                 (SELECT prod_type T_protype,area_code T_area_dist,(voice_charge + data_charge) T_cal_indcat,
                                                                                   data_charge T_data,
                                                                                   prd_id T_prd_id,
                                                                                   voice_charge T_voice
                  FROM public.dmt_charge_month
                  WHERE STAT_DATE >= '201507'
                    AND STAT_DATE <= '201509') T1) T2
            GROUP BY (T_protype,
                      T_area_dist)) T3
         LEFT JOIN DIM_PROD_TYPE ON T3.T_protype = DIM_PROD_TYPE.prod_type_id
         LEFT JOIN DIM_AREA ON T3.T_area_dist = DIM_AREA.dist_code
         ORDER BY T_protype ASC LIMIT 36) T4
      GROUP BY
      GROUPING SETS((T_protype, T_area_dist),(T_protype))) T4) T5
