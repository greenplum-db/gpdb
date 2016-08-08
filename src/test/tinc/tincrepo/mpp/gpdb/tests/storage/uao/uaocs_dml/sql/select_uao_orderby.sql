-- @description : Create Data and execute select statements on UAO tables ORDER BY ASC DESC USING 
-- 
--

DROP TABLE IF EXISTS city_uao_using cascade;

DROP TABLE IF EXISTS country_uao_using cascade;

DROP TABLE IF EXISTS countrylanguage_uao_using cascade;


BEGIN;



CREATE TABLE city_uao_using (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) with (appendonly=true, orientation=column) distributed by(id);



CREATE TABLE country_uao_using (
    code character(3) NOT NULL,
    name text NOT NULL,
    continent text NOT NULL,
    region text NOT NULL,
    surfacearea real NOT NULL,
    indepyear smallint,
    population integer NOT NULL,
    lifeexpectancy real,
    gnp numeric(10,2),
    gnpold numeric(10,2),
    localname text NOT NULL,
    governmentform text NOT NULL,
    headofstate text,
    capital integer,
    code2 character(2) NOT NULL
) with (appendonly=true, orientation=column)  distributed by (code);



CREATE TABLE countrylanguage_uao_using (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
)  with (appendonly=true, orientation=column) distributed by (countrycode,language);

COPY city_uao_using (id, name, countrycode, district, population) FROM stdin;
1	Kabul	AFG	Kabol	1780000
2	Qandahar	AFG	Qandahar	237500
3	Herat	AFG	Herat	186800
4	Mazar-e-Sharif	AFG	Balkh	127800
5	Amsterdam	NLD	Noord-Holland	731200
6	Rotterdam	NLD	Zuid-Holland	593321
7	Haag	NLD	Zuid-Holland	440900
8	Utrecht	NLD	Utrecht	234323
9	Eindhoven	NLD	Noord-Brabant	201843
10	Tilburg	NLD	Noord-Brabant	193238
11	Groningen	NLD	Groningen	172701
12	Breda	NLD	Noord-Brabant	160398
13	Apeldoorn	NLD	Gelderland	153491
14	Nijmegen	NLD	Gelderland	152463
15	Enschede	NLD	Overijssel	149544
16	Haarlem	NLD	Noord-Holland	148772
17	Almere	NLD	Flevoland	142465
18	Arnhem	NLD	Gelderland	138020
19	Zaanstad	NLD	Noord-Holland	135621
20	s-Hertogenbosch	NLD	Noord-Brabant	129170
21	Amersfoort	NLD	Utrecht	126270
22	Maastricht	NLD	Limburg	122087
23	Dordrecht	NLD	Zuid-Holland	119811
24	Leiden	NLD	Zuid-Holland	117196
25	Haarlemmermeer	NLD	Noord-Holland	110722
26	Zoetermeer	NLD	Zuid-Holland	110214
27	Emmen	NLD	Drenthe	105853
28	Zwolle	NLD	Overijssel	105819
29	Ede	NLD	Gelderland	101574
30	Delft	NLD	Zuid-Holland	95268
31	Heerlen	NLD	Limburg	95052
32	Alkmaar	NLD	Noord-Holland	92713
33	Willemstad	ANT	Curacao	2345
34	Tirana	ALB	Tirana	270000
35	Alger	DZA	Alger	2168000
36	Oran	DZA	Oran	609823
37	Constantine	DZA	Constantine	443727
38	Annaba	DZA	Annaba	222518
39	Batna	DZA	Batna	183377
40	Setif	DZA	Setif	179055
41	Sidi Bel Abbes	DZA	Sidi Bel Abbes	153106
42	Skikda	DZA	Skikda	128747
43	Biskra	DZA	Biskra	128281
44	Blida (el-Boulaida)	DZA	Blida	127284
45	Bejaia	DZA	Bejaia	117162
46	Mostaganem	DZA	Mostaganem	115212
47	Tebessa	DZA	Tebessa	112007
48	Tlemcen (Tilimsen)	DZA	Tlemcen	110242
49	Bechar	DZA	Bechar	107311
50	Tiaret	DZA	Tiaret	100118
51	Ech-Chleff (el-Asnam)	DZA	Chlef	96794
52	Ghardaia	DZA	Ghardaia	89415
53	Tafuna	ASM	Tutuila	5200
54	Fagatogo	ASM	Tutuila	2323
55	Andorra la Vella	AND	Andorra la Vella	21189
56	Luanda	AGO	Luanda	2022000
57	Huambo	AGO	Huambo	163100
58	Lobito	AGO	Benguela	130000
59	Benguela	AGO	Benguela	128300
60	Namibe	AGO	Namibe	118200
61	South Hill	AIA	A	961
62	The Valley	AIA	A	595
63	Saint Johns	ATG	St John	24000
64	Dubai	ARE	Dubai	669181
65	Abu Dhabi	ARE	Abu Dhabi	398695
66	Sharja	ARE	Sharja	320095
67	al-Ayn	ARE	Abu Dhabi	225970
68	Ajman	ARE	Ajman	114395
69	Buenos Aires	ARG	Distrito Federal	2982146
70	La Matanza	ARG	Buenos Aires	1266461
71	Cordoba	ARG	Cordoba	1157507
72	Rosario	ARG	Santa Fe	907718
73	Lomas de Zamora	ARG	Buenos Aires	622013
74	Quilmes	ARG	Buenos Aires	559249
75	Almirante Brown	ARG	Buenos Aires	538918
76	La Plata	ARG	Buenos Aires	521936
77	Mar del Plata	ARG	Buenos Aires	512880
78	San Miguel de Tucuman	ARG	Tucuman	470809
79	Lanus	ARG	Buenos Aires	469735
80	Merlo	ARG	Buenos Aires	463846
81	General San Martin	ARG	Buenos Aires	422542
82	Salta	ARG	Salta	367550
83	Moreno	ARG	Buenos Aires	356993
84	Santa Fe	ARG	Santa Fe	353063
85	Avellaneda	ARG	Buenos Aires	353046
86	Tres de Febrero	ARG	Buenos Aires	352311
87	Moron	ARG	Buenos Aires	349246
88	Florencio Varela	ARG	Buenos Aires	315432
89	San Isidro	ARG	Buenos Aires	306341
90	Tigre	ARG	Buenos Aires	296226
91	Malvinas Argentinas	ARG	Buenos Aires	290335
92	Vicente Lopez	ARG	Buenos Aires	288341
93	Berazategui	ARG	Buenos Aires	276916
94	Corrientes	ARG	Corrientes	258103
95	San Miguel	ARG	Buenos Aires	248700
96	Bahia Blanca	ARG	Buenos Aires	239810
97	Esteban Echeverria	ARG	Buenos Aires	235760
98	Resistencia	ARG	Chaco	229212
99	Jose C. Paz	ARG	Buenos Aires	221754
\.


--
-- Data for Name: country; Type: TABLE DATA; Schema: public; 
--

COPY country_uao_using (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM stdin WITH NULL AS '';
AFG	Afghanistan	Asia	Southern and Central Asia	652090	1919	22720000	45.900002	5976.00	1	Afganistan/Afqanestan	Islamic Emirate	Mohammad Omar	1	AF
NLD	Netherlands	Europe	Western Europe	41526	1581	15864000	78.300003	371362.00	360478.00	Nederland	Constitutional Monarchy	Beatrix	5	NL
ANT	Netherlands Antilles	North America	Caribbean	800		217000	74.699997	1941.00		Nederlandse Antillen	Nonmetropolitan Territory of The Netherlands	Beatrix	33	AN
ALB	Albania	Europe	Southern Europe	28748	1912	3401200	71.599998	3205.00	2500.00	Shqiperia	Republic	Rexhep Mejdani	34	AL
DZA	Algeria	Africa	Northern Africa	2381741	1962	31471000	69.699997	49982.00	46966.00	Al-Jazaeir/Algerie	Republic	Abdelaziz Bouteflika	35	DZ
ASM	American Samoa	Oceania	Polynesia	199		68000	75.099998	334.00		Amerika Samoa	US Territory	George W. Bush	54	AS
AND	Andorra	Europe	Southern Europe	468	1278	78000	83.5	1630.00		Andorra	Parliamentary Coprincipality		55	AD
AGO	Angola	Africa	Central Africa	1246700	1975	12878000	38.299999	6648.00	7984.00	Angola	Republic	Jose Eduardo dos Santos	56	AO
AIA	Anguilla	North America	Caribbean	96		8000	76.099998	63.20		Anguilla	Dependent Territory of the UK	Elisabeth II	62	AI
ATG	Antigua and Barbuda	North America	Caribbean	442	1981	68000	70.5	612.00	584.00	Antigua and Barbuda	Constitutional Monarchy	Elisabeth II	63	AG
ARE	United Arab Emirates	Asia	Middle East	83600	1971	2441000	74.099998	37966.00	36846.00	Al-Imarat al-Arabiya al-Muttahida	Emirate Federation	Zayid bin Sultan al-Nahayan	65	AE
ARG	Argentina	South America	South America	2780400	1816	37032000	75.099998	340238.00	323310.00	Argentina	Federal Republic	Fernando de la Rua	69	AR
ARM	Armenia	Asia	Middle East	29800	1991	3520000	66.400002	1813.00	1627.00	Hajastan	Republic	Robert Kotdarjan	126	AM
ABW	Aruba	North America	Caribbean	193		103000	78.400002	828.00	793.00	Aruba	Nonmetropolitan Territory of The Netherlands	Beatrix	129	AW
AUS	Australia	Oceania	Australia and New Zealand	7741220	1901	18886000	79.800003	351182.00	392911.00	Australia	Constitutional Monarchy, Federation	Elisabeth II	135	AU
AZE	Azerbaijan	Asia	Middle East	86600	1991	7734000	62.900002	4127.00	4100.00	Azarbaycan	Federal Republic	Heydar Aliyev	144	AZ
BHS	Bahamas	North America	Caribbean	13878	1973	307000	71.099998	3527.00	3347.00	The Bahamas	Constitutional Monarchy	Elisabeth II	148	BS
BHR	Bahrain	Asia	Middle East	694	1971	617000	73	6366.00	6097.00	Al-Bahrayn	Monarchy (Emirate)	Hamad ibn Isa al-Khalifa	149	BH
BGD	Bangladesh	Asia	Southern and Central Asia	143998	1971	129155000	60.200001	32852.00	31966.00	Bangladesh	Republic	Shahabuddin Ahmad	150	BD
BRB	Barbados	North America	Caribbean	430	1966	270000	73	2223.00	2186.00	Barbados	Constitutional Monarchy	Elisabeth II	174	BB
BEL	Belgium	Europe	Western Europe	30518	1830	10239000	77.800003	249704.00	243948.00	Belgie/Belgique	Constitutional Monarchy, Federation	Albert II	179	BE
BLZ	Belize	North America	Central America	22696	1981	241000	70.900002	630.00	616.00	Belize	Constitutional Monarchy	Elisabeth II	185	BZ
BEN	Benin	Africa	Western Africa	112622	1960	6097000	50.200001	2357.00	2141.00	Benin	Republic	Mathieu Kerekou	187	BJ
BMU	Bermuda	North America	North America	53		65000	76.900002	2328.00	2190.00	Bermuda	Dependent Territory of the UK	Elisabeth II	191	BM
BTN	Bhutan	Asia	Southern and Central Asia	47000	1910	2124000	52.400002	372.00	383.00	Druk-Yul	Monarchy	Jigme Singye Wangchuk	192	BT
BOL	Bolivia	South America	South America	1098581	1825	8329000	63.700001	8571.00	7967.00	Bolivia	Republic	Hugo Banzer Suarez	194	BO
BIH	Bosnia and Herzegovina	Europe	Southern Europe	51197	1992	3972000	71.5	2841.00		Bosna i Hercegovina	Federal Republic	Ante Jelavic	201	BA
BWA	Botswana	Africa	Southern Africa	581730	1966	1622000	39.299999	4834.00	4935.00	Botswana	Republic	Festus G. Mogae	204	BW
BRA	Brazil	South America	South America	8547403	1822	170115000	62.900002	776739.00	804108.00	Brasil	Federal Republic	Fernando Henrique Cardoso	211	BR
GBR	United Kingdom	Europe	British Islands	242900	1066	59623400	77.699997	1378330.00	1296830.00	United Kingdom	Constitutional Monarchy	Elisabeth II	456	GB
VGB	Virgin Islands, British	North America	Caribbean	151		21000	75.400002	612.00	573.00	British Virgin Islands	Dependent Territory of the UK	Elisabeth II	537	VG
BRN	Brunei	Asia	Southeast Asia	5765	1984	328000	73.599998	11705.00	12460.00	Brunei Darussalam	Monarchy (Sultanate)	Haji Hassan al-Bolkiah	538	BN
BGR	Bulgaria	Europe	Eastern Europe	110994	1908	8190900	70.900002	12178.00	10169.00	Balgarija	Republic	Petar Stojanov	539	BG
BFA	Burkina Faso	Africa	Western Africa	274000	1960	11937000	46.700001	2425.00	2201.00	Burkina Faso	Republic	Blaise Compaore	549	BF
BDI	Burundi	Africa	Eastern Africa	27834	1962	6695000	46.200001	903.00	982.00	Burundi/Uburundi	Republic	Pierre Buyoya	552	BI
CYM	Cayman Islands	North America	Caribbean	264		38000	78.900002	1263.00	1186.00	Cayman Islands	Dependent Territory of the UK	Elisabeth II	553	KY
CHL	Chile	South America	South America	756626	1810	15211000	75.699997	72949.00	75780.00	Chile	Republic	Ricardo Lagos Escobar	554	CL
COK	Cook Islands	Oceania	Polynesia	236		20000	71.099998	100.00		The Cook Islands	Nonmetropolitan Territory of New Zealand	Elisabeth II	583	CK
CRI	Costa Rica	North America	Central America	51100	1821	4023000	75.800003	10226.00	9757.00	Costa Rica	Republic	Miguel Angel Rodriguez Echeverria	584	CR
DJI	Djibouti	Africa	Eastern Africa	23200	1977	638000	50.799999	382.00	373.00	Djibouti/Jibuti	Republic	Ismail Omar Guelleh	585	DJ
DMA	Dominica	North America	Caribbean	751	1978	71000	73.400002	256.00	243.00	Dominica	Republic	Vernon Shaw	586	DM
DOM	Dominican Republic	North America	Caribbean	48511	1844	8495000	73.199997	15846.00	15076.00	Republica Dominicana	Republic	Hipolito Mejia Dominguez	587	DO
ECU	Ecuador	South America	South America	283561	1822	12646000	71.099998	19770.00	19769.00	Ecuador	Republic	Gustavo Noboa Bejarano	594	EC
EGY	Egypt	Africa	Northern Africa	1001449	1922	68470000	63.299999	82710.00	75617.00	Misr	Republic	Hosni Mubarak	608	EG
SLV	El Salvador	North America	Central America	21041	1841	6276000	69.699997	11863.00	11203.00	El Salvador	Republic	Francisco Guillermo Flores Perez	645	SV
ERI	Eritrea	Africa	Eastern Africa	117600	1993	3850000	55.799999	650.00	755.00	Ertra	Republic	Isayas Afewerki [Isaias Afwerki]	652	ER
ESP	Spain	Europe	Southern Europe	505992	1492	39441700	78.800003	553233.00	532031.00	Espaaa	Constitutional Monarchy	Juan Carlos I	653	ES
ZAF	South Africa	Africa	Southern Africa	1221037	1910	40377000	51.099998	116729.00	129092.00	South Africa	Republic	Thabo Mbeki	716	ZA
ETH	Ethiopia	Africa	Eastern Africa	1104300	-1000	62565000	45.200001	6353.00	6180.00	YeItyopiya	Republic	Negasso Gidada	756	ET
FLK	Falkland Islands	South America	South America	12173		2000		0.00		Falkland Islands	Dependent Territory of the UK	Elisabeth II	763	FK
FJI	Fiji Islands	Oceania	Melanesia	18274	1970	817000	67.900002	1536.00	2149.00	Fiji Islands	Republic	Josefa Iloilo	764	FJ
PHL	Philippines	Asia	Southeast Asia	300000	1946	75967000	67.5	65107.00	82239.00	Pilipinas	Republic	Gloria Macapagal-Arroyo	766	PH
FRO	Faroe Islands	Europe	Nordic Countries	1399		43000	78.400002	0.00		Foroyar	Part of Denmark	Margrethe II	901	FO
GAB	Gabon	Africa	Central Africa	267668	1960	1226000	50.099998	5493.00	5279.00	Le Gabon	Republic	Omar Bongo	902	GA
GMB	Gambia	Africa	Western Africa	11295	1965	1305000	53.200001	320.00	325.00	The Gambia	Republic	Yahya Jammeh	904	GM
GEO	Georgia	Asia	Middle East	69700	1991	4968000	64.5	6064.00	5924.00	Sakartvelo	Republic	Eduard Gevardnadze	905	GE
GHA	Ghana	Africa	Western Africa	238533	1957	20212000	57.400002	7137.00	6884.00	Ghana	Republic	John Kufuor	910	GH
GIB	Gibraltar	Europe	Southern Europe	6		25000	79	258.00		Gibraltar	Dependent Territory of the UK	Elisabeth II	915	GI
GRD	Grenada	North America	Caribbean	344	1974	94000	64.5	318.00		Grenada	Constitutional Monarchy	Elisabeth II	916	GD
GRL	Greenland	North America	North America	2166090		56000	68.099998	0.00		Kalaallit Nunaat/Gronland	Part of Denmark	Margrethe II	917	GL
GLP	Guadeloupe	North America	Caribbean	1705		456000	77	3501.00		Guadeloupe	Overseas Department of France	Jacques Chirac	919	GP
GUM	Guam	Oceania	Micronesia	549		168000	77.800003	1197.00	1136.00	Guam	US Territory	George W. Bush	921	GU
GTM	Guatemala	North America	Central America	108889	1821	11385000	66.199997	19008.00	17797.00	Guatemala	Republic	Alfonso Portillo Cabrera	922	GT
GIN	Guinea	Africa	Western Africa	245857	1958	7430000	45.599998	2352.00	2383.00	Guinee	Republic	Lansana Conte	926	GN
GNB	Guinea-Bissau	Africa	Western Africa	36125	1974	1213000	49	293.00	272.00	Guine-Bissau	Republic	Kumba Iala	927	GW
GUY	Guyana	South America	South America	214969	1966	861000	64	722.00	743.00	Guyana	Republic	Bharrat Jagdeo	928	GY
HTI	Haiti	North America	Caribbean	27750	1804	8222000	49.200001	3459.00	3107.00	Haiti/Dayti	Republic	Jean-Bertrand Aristide	929	HT
HND	Honduras	North America	Central America	112088	1838	6485000	69.900002	5333.00	4697.00	Honduras	Republic	Carlos Roberto Flores Facusse	933	HN
HKG	Hong Kong	Asia	Eastern Asia	1075		6782000	79.5	166448.00	173610.00	Xianggang/Hong Kong	Special Administrative Region of China	Jiang Zemin	937	HK
SJM	Svalbard and Jan Mayen	Europe	Nordic Countries	62422		3200		0.00		Svalbard og Jan Mayen	Dependent Territory of Norway	Harald V	938	SJ
IDN	Indonesia	Asia	Southeast Asia	1904569	1945	212107000	68	84982.00	215002.00	Indonesia	Republic	Abdurrahman Wahid	939	ID
IND	India	Asia	Southern and Central Asia	3287263	1947	1013662000	62.5	447114.00	430572.00	Bharat/India	Federal Republic	Kocheril Raman Narayanan	1109	IN
IRQ	Iraq	Asia	Middle East	438317	1932	23115000	66.5	11500.00		Al-Iraq	Republic	Saddam Hussein al-Takriti	1365	IQ
IRN	Iran	Asia	Southern and Central Asia	1648195	1906	67702000	69.699997	195746.00	160151.00	Iran	Islamic Republic	Ali Mohammad Khatami-Ardakani	1380	IR
IRL	Ireland	Europe	British Islands	70273	1921	3775100	76.800003	75921.00	73132.00	Ireland/Eire	Republic	Mary McAleese	1447	IE
ISL	Iceland	Europe	Nordic Countries	103000	1944	279000	79.400002	8255.00	7474.00	Island	Republic	Olafur Ragnar Grimsson	1449	IS
ISR	Israel	Asia	Middle East	21056	1948	6217000	78.599998	97477.00	98577.00	Yisraeel/Israeil	Republic	Moshe Katzav	1450	IL
ITA	Italy	Europe	Southern Europe	301316	1861	57680000	79	1161755.00	1145372.00	Italia	Republic	Carlo Azeglio Ciampi	1464	IT
TMP	East Timor	Asia	Southeast Asia	14874		885000	46	0.00		Timor Timur	Administrated by the UN	Jose Alexandre Gusmao	1522	TP
AUT	Austria	Europe	Western Europe	83859	1918	8091800	77.699997	211860.00	206025.00	Osterreich	Federal Republic	Thomas Klestil	1523	AT
JAM	Jamaica	North America	Caribbean	10990	1962	2583000	75.199997	6871.00	6722.00	Jamaica	Constitutional Monarchy	Elisabeth II	1530	JM
JPN	Japan	Asia	Eastern Asia	377829	-660	126714000	80.699997	3787042.00	4192638.00	Nihon/Nippon	Constitutional Monarchy	Akihito	1532	JP
YEM	Yemen	Asia	Middle East	527968	1918	18112000	59.799999	6041.00	5729.00	Al-Yaman	Republic	Ali Abdallah Salih	1780	YE
JOR	Jordan	Asia	Middle East	88946	1946	5083000	77.400002	7526.00	7051.00	Al-Urdunn	Constitutional Monarchy	Abdullah II	1786	JO
CXR	Christmas Island	Oceania	Australia and New Zealand	135		2500		0.00		Christmas Island	Territory of Australia	Elisabeth II	1791	CX
YUG	Yugoslavia	Europe	Southern Europe	102173	1918	10640000	72.400002	17000.00		Jugoslavija	Federal Republic	Vojislav Kodtunica	1792	YU
KHM	Cambodia	Asia	Southeast Asia	181035	1953	11168000	56.5	5121.00	5670.00	Kampuchea	Constitutional Monarchy	Norodom Sihanouk	1800	KH
CMR	Cameroon	Africa	Central Africa	475442	1960	15085000	54.799999	9174.00	8596.00	Cameroun/Cameroon	Republic	Paul Biya	1804	CM
CAN	Canada	North America	North America	9970610	1867	31147000	79.400002	598862.00	625626.00	Canada	Constitutional Monarchy, Federation	Elisabeth II	1822	CA
CPV	Cape Verde	Africa	Western Africa	4033	1975	428000	68.900002	435.00	420.00	Cabo Verde	Republic	Antonio Mascarenhas Monteiro	1859	CV
KAZ	Kazakstan	Asia	Southern and Central Asia	2724900	1991	16223000	63.200001	24375.00	23383.00	Qazaqstan	Republic	Nursultan Nazarbajev	1864	KZ
KEN	Kenya	Africa	Eastern Africa	580367	1963	30080000	48	9217.00	10241.00	Kenya	Republic	Daniel arap Moi	1881	KE
CAF	Central African Republic	Africa	Central Africa	622984	1960	3615000	44	1054.00	993.00	Centrafrique/Be-Afrika	Republic	Ange-Felix Patasse	1889	CF
CHN	China	Asia	Eastern Asia	9572900	-1523	1277558000	71.400002	982268.00	917719.00	Zhongquo	People'sRepublic	Jiang Zemin	1891	CN
KGZ	Kyrgyzstan	Asia	Southern and Central Asia	199900	1991	4699000	63.400002	1626.00	1767.00	Kyrgyzstan	Republic	Askar Akajev	2253	KG
KIR	Kiribati	Oceania	Micronesia	726	1979	83000	59.799999	40.70		Kiribati	Republic	Teburoro Tito	2256	KI
COL	Colombia	South America	South America	1138914	1810	42321000	70.300003	102896.00	105116.00	Colombia	Republic	Andres Pastrana Arango	2257	CO
COM	Comoros	Africa	Eastern Africa	1862	1975	578000	60	4401.00	4361.00	Komori/Comores	Republic	Azali Assoumani	2295	KM
COG	Congo	Africa	Central Africa	342000	1960	2943000	47.400002	2108.00	2287.00	Congo	Republic	Denis Sassou-Nguesso	2296	CG
COD	Congo, The Democratic Republic of the	Africa	Central Africa	2344858	1960	51654000	48.799999	6964.00	2474.00	Republique Democratique du Congo	Republic	Joseph Kabila	2298	CD
CCK	Cocos (Keeling) Islands	Oceania	Australia and New Zealand	14		600		0.00		Cocos (Keeling) Islands	Territory of Australia	Elisabeth II	2317	CC
PRK	North Korea	Asia	Eastern Asia	120538	1948	24039000	70.699997	5332.00		Choson Minjujuui Inmin Konghwaguk (Bukhan)	Socialistic Republic	Kim Jong-il	2318	KP
KOR	South Korea	Asia	Eastern Asia	99434	1948	46844000	74.400002	320749.00	442544.00	Taehan Mineguk (Namhan)	Republic	Kim Dae-jung	2331	KR
GRC	Greece	Europe	Southern Europe	131626	1830	10545700	78.400002	120724.00	119946.00	Ellada	Republic	Kostis Stefanopoulos	2401	GR
HRV	Croatia	Europe	Southern Europe	56538	1991	4473000	73.699997	20208.00	19300.00	Hrvatska	Republic	Gtipe Mesic	2409	HR
CUB	Cuba	North America	Caribbean	110861	1902	11201000	76.199997	17843.00	18862.00	Cuba	Socialistic Republic	Fidel Castro Ruz	2413	CU
KWT	Kuwait	Asia	Middle East	17818	1961	1972000	76.099998	27037.00	30373.00	Al-Kuwayt	Constitutional Monarchy (Emirate)	Jabir al-Ahmad al-Jabir al-Sabah	2429	KW
CYP	Cyprus	Asia	Middle East	9251	1960	754700	76.699997	9333.00	8246.00	Kypros/Kibris	Republic	Glafkos Klerides	2430	CY
LAO	Laos	Asia	Southeast Asia	236800	1953	5433000	53.099998	1292.00	1746.00	Lao	Republic	Khamtay Siphandone	2432	LA
LVA	Latvia	Europe	Baltic Countries	64589	1991	2424200	68.400002	6398.00	5639.00	Latvija	Republic	Vaira Vike-Freiberga	2434	LV
LSO	Lesotho	Africa	Southern Africa	30355	1966	2153000	50.799999	1061.00	1161.00	Lesotho	Constitutional Monarchy	Letsie III	2437	LS
LBN	Lebanon	Asia	Middle East	10400	1941	3282000	71.300003	17121.00	15129.00	Lubnan	Republic	Emile Lahoud	2438	LB
LBR	Liberia	Africa	Western Africa	111369	1847	3154000	51	2012.00		Liberia	Republic	Charles Taylor	2440	LR
LBY	Libyan Arab Jamahiriya	Africa	Northern Africa	1759540	1951	5605000	75.5	44806.00	40562.00	Libiya	Socialistic State	Muammar al-Qadhafi	2441	LY
LIE	Liechtenstein	Europe	Western Europe	160	1806	32300	78.800003	1119.00	1084.00	Liechtenstein	Constitutional Monarchy	Hans-Adam II	2446	LI
LTU	Lithuania	Europe	Baltic Countries	65301	1991	3698500	69.099998	10692.00	9585.00	Lietuva	Republic	Valdas Adamkus	2447	LT
LUX	Luxembourg	Europe	Western Europe	2586	1867	435700	77.099998	16321.00	15519.00	Luxembourg/Letzebuerg	Constitutional Monarchy	Henri	2452	LU
ESH	Western Sahara	Africa	Northern Africa	266000		293000	49.799999	60.00		As-Sahrawiya	Occupied by Marocco	Mohammed Abdel Aziz	2453	EH
MAC	Macao	Asia	Eastern Asia	18		473000	81.599998	5749.00	5940.00	Macau/Aomen	Special Administrative Region of China	Jiang Zemin	2454	MO
MDG	Madagascar	Africa	Eastern Africa	587041	1960	15942000	55	3750.00	3545.00	Madagasikara/Madagascar	Federal Republic	Didier Ratsiraka	2455	MG
MKD	Macedonia	Europe	Southern Europe	25713	1991	2024000	73.800003	1694.00	1915.00	Makedonija	Republic	Boris Trajkovski	2460	MK
MWI	Malawi	Africa	Eastern Africa	118484	1964	10925000	37.599998	1687.00	2527.00	Malawi	Republic	Bakili Muluzi	2462	MW
MDV	Maldives	Asia	Southern and Central Asia	298	1965	286000	62.200001	199.00		Dhivehi Raajje/Maldives	Republic	Maumoon Abdul Gayoom	2463	MV
MYS	Malaysia	Asia	Southeast Asia	329758	1957	22244000	70.800003	69213.00	97884.00	Malaysia	Constitutional Monarchy, Federation	Salahuddin Abdul Aziz Shah Alhaj	2464	MY
MLI	Mali	Africa	Western Africa	1240192	1960	11234000	46.700001	2642.00	2453.00	Mali	Republic	Alpha Oumar Konare	2482	ML
MLT	Malta	Europe	Southern Europe	316	1964	380200	77.900002	3512.00	3338.00	Malta	Republic	Guido de Marco	2484	MT
MAR	Morocco	Africa	Northern Africa	446550	1956	28351000	69.099998	36124.00	33514.00	Al-Maghrib	Constitutional Monarchy	Mohammed VI	2486	MA
MHL	Marshall Islands	Oceania	Micronesia	181	1990	64000	65.5	97.00		Marshall Islands/Majol	Republic	Kessai Note	2507	MH
MTQ	Martinique	North America	Caribbean	1102		395000	78.300003	2731.00	2559.00	Martinique	Overseas Department of France	Jacques Chirac	2508	MQ
MRT	Mauritania	Africa	Western Africa	1025520	1960	2670000	50.799999	998.00	1081.00	Muritaniya/Mauritanie	Republic	Maaouiya Ould SidAhmad Taya	2509	MR
MUS	Mauritius	Africa	Eastern Africa	2040	1968	1158000	71	4251.00	4186.00	Mauritius	Republic	Cassam Uteem	2511	MU
MYT	Mayotte	Africa	Eastern Africa	373		149000	59.5	0.00		Mayotte	Territorial Collectivity of France	Jacques Chirac	2514	YT
MEX	Mexico	North America	Central America	1958201	1810	98881000	71.5	414972.00	401461.00	Mexico	Federal Republic	Vicente Fox Quesada	2515	MX
FSM	Micronesia, Federated States of	Oceania	Micronesia	702	1990	119000	68.599998	212.00		Micronesia	Federal Republic	Leo A. Falcam	2689	FM
MDA	Moldova	Europe	Eastern Europe	33851	1991	4380000	64.5	1579.00	1872.00	Moldova	Republic	Vladimir Voronin	2690	MD
MCO	Monaco	Europe	Western Europe	1.5	1861	34000	78.800003	776.00		Monaco	Constitutional Monarchy	Rainier III	2695	MC
MNG	Mongolia	Asia	Eastern Asia	1566500	1921	2662000	67.300003	1043.00	933.00	Mongol Uls	Republic	Natsagiin Bagabandi	2696	MN
MSR	Montserrat	North America	Caribbean	102		11000	78	109.00		Montserrat	Dependent Territory of the UK	Elisabeth II	2697	MS
MOZ	Mozambique	Africa	Eastern Africa	801590	1975	19680000	37.5	2891.00	2711.00	Mocambique	Republic	Joaquim A. Chissano	2698	MZ
MMR	Myanmar	Asia	Southeast Asia	676578	1948	45611000	54.900002	180375.00	171028.00	Myanma Pye	Republic	kenraali Than Shwe	2710	MM
NAM	Namibia	Africa	Southern Africa	824292	1990	1726000	42.5	3101.00	3384.00	Namibia	Republic	Sam Nujoma	2726	NA
NRU	Nauru	Oceania	Micronesia	21	1968	12000	60.799999	197.00		Naoero/Nauru	Republic	Bernard Dowiyogo	2728	NR
NPL	Nepal	Asia	Southern and Central Asia	147181	1769	23930000	57.799999	4768.00	4837.00	Nepal	Constitutional Monarchy	Gyanendra Bir Bikram	2729	NP
NIC	Nicaragua	North America	Central America	130000	1838	5074000	68.699997	1988.00	2023.00	Nicaragua	Republic	Arnoldo Aleman Lacayo	2734	NI
NER	Niger	Africa	Western Africa	1267000	1960	10730000	41.299999	1706.00	1580.00	Niger	Republic	Mamadou Tandja	2738	NE
NGA	Nigeria	Africa	Western Africa	923768	1960	111506000	51.599998	65707.00	58623.00	Nigeria	Federal Republic	Olusegun Obasanjo	2754	NG
NIU	Niue	Oceania	Polynesia	260		2000		0.00		Niue	Nonmetropolitan Territory of New Zealand	Elisabeth II	2805	NU
NFK	Norfolk Island	Oceania	Australia and New Zealand	36		2000		0.00		Norfolk Island	Territory of Australia	Elisabeth II	2806	NF
NOR	Norway	Europe	Nordic Countries	323877	1905	4478500	78.699997	145895.00	153370.00	Norge	Constitutional Monarchy	Harald V	2807	NO
CIV	Cote deIvoire	Africa	Western Africa	322463	1960	14786000	45.200001	11345.00	10285.00	Cote deIvoire	Republic	Laurent Gbagbo	2814	CI
OMN	Oman	Asia	Middle East	309500	1951	2542000	71.800003	16904.00	16153.00	Uman	Monarchy (Sultanate)	Qabus ibn Said	2821	OM
PAK	Pakistan	Asia	Southern and Central Asia	796095	1947	156483000	61.099998	61289.00	58549.00	Pakistan	Republic	Mohammad Rafiq Tarar	2831	PK
PLW	Palau	Oceania	Micronesia	459	1994	19000	68.599998	105.00		Belau/Palau	Republic	Kuniwo Nakamura	2881	PW
PAN	Panama	North America	Central America	75517	1903	2856000	75.5	9131.00	8700.00	Panama	Republic	Mireya Elisa Moscoso Rodriguez	2882	PA
PNG	Papua New Guinea	Oceania	Melanesia	462840	1975	4807000	63.099998	4988.00	6328.00	Papua New Guinea/Papua Niugini	Constitutional Monarchy	Elisabeth II	2884	PG
PRY	Paraguay	South America	South America	406752	1811	5496000	73.699997	8444.00	9555.00	Paraguay	Republic	Luis Angel Gonzalez Macchi	2885	PY
PER	Peru	South America	South America	1285216	1821	25662000	70	64140.00	65186.00	Peru/Piruw	Republic	Valentin Paniagua Corazao	2890	PE
PCN	Pitcairn	Oceania	Polynesia	49		50		0.00		Pitcairn	Dependent Territory of the UK	Elisabeth II	2912	PN
MNP	Northern Mariana Islands	Oceania	Micronesia	464		78000	75.5	0.00		Northern Mariana Islands	Commonwealth of the US	George W. Bush	2913	MP
PRT	Portugal	Europe	Southern Europe	91982	1143	9997600	75.800003	105954.00	102133.00	Portugal	Republic	Jorge Sampaio	2914	PT
PRI	Puerto Rico	North America	Caribbean	8875		3869000	75.599998	34100.00	32100.00	Puerto Rico	Commonwealth of the US	George W. Bush	2919	PR
POL	Poland	Europe	Eastern Europe	323250	1918	38653600	73.199997	151697.00	135636.00	Polska	Republic	Aleksander Kwasniewski	2928	PL
GNQ	Equatorial Guinea	Africa	Central Africa	28051	1968	453000	53.599998	283.00	542.00	Guinea Ecuatorial	Republic	Teodoro Obiang Nguema Mbasogo	2972	GQ
QAT	Qatar	Asia	Middle East	11000	1971	599000	72.400002	9472.00	8920.00	Qatar	Monarchy	Hamad ibn Khalifa al-Thani	2973	QA
FRA	France	Europe	Western Europe	551500	843	59225700	78.800003	1424285.00	1392448.00	France	Republic	Jacques Chirac	2974	FR
GUF	French Guiana	South America	South America	90000		181000	76.099998	681.00		Guyane francaise	Overseas Department of France	Jacques Chirac	3014	GF
PYF	French Polynesia	Oceania	Polynesia	4000		235000	74.800003	818.00	781.00	Polynesie francaise	Nonmetropolitan Territory of France	Jacques Chirac	3016	PF
REU	Reunion	Africa	Eastern Africa	2510		699000	72.699997	8287.00	7988.00	Reunion	Overseas Department of France	Jacques Chirac	3017	RE
ROM	Romania	Europe	Eastern Europe	238391	1878	22455500	69.900002	38158.00	34843.00	Romania	Republic	Ion Iliescu	3018	RO
RWA	Rwanda	Africa	Eastern Africa	26338	1962	7733000	39.299999	2036.00	1863.00	Rwanda/Urwanda	Republic	Paul Kagame	3047	RW
SWE	Sweden	Europe	Nordic Countries	449964	836	8861400	79.599998	226492.00	227757.00	Sverige	Constitutional Monarchy	Carl XVI Gustaf	3048	SE
SHN	Saint Helena	Africa	Western Africa	314		6000	76.800003	0.00		Saint Helena	Dependent Territory of the UK	Elisabeth II	3063	SH
KNA	Saint Kitts and Nevis	North America	Caribbean	261	1983	38000	70.699997	299.00		Saint Kitts and Nevis	Constitutional Monarchy	Elisabeth II	3064	KN
LCA	Saint Lucia	North America	Caribbean	622	1979	154000	72.300003	571.00		Saint Lucia	Constitutional Monarchy	Elisabeth II	3065	LC
VCT	Saint Vincent and the Grenadines	North America	Caribbean	388	1979	114000	72.300003	285.00		Saint Vincent and the Grenadines	Constitutional Monarchy	Elisabeth II	3066	VC
SPM	Saint Pierre and Miquelon	North America	North America	242		7000	77.599998	0.00		Saint-Pierre-et-Miquelon	Territorial Collectivity of France	Jacques Chirac	3067	PM
DEU	Germany	Europe	Western Europe	357022	1955	82164700	77.400002	2133367.00	2102826.00	Deutschland	Federal Republic	Johannes Rau	3068	DE
SLB	Solomon Islands	Oceania	Melanesia	28896	1978	444000	71.300003	182.00	220.00	Solomon Islands	Constitutional Monarchy	Elisabeth II	3161	SB
ZMB	Zambia	Africa	Eastern Africa	752618	1964	9169000	37.200001	3377.00	3922.00	Zambia	Republic	Frederick Chiluba	3162	ZM
WSM	Samoa	Oceania	Polynesia	2831	1962	180000	69.199997	141.00	157.00	Samoa	Parlementary Monarchy	Malietoa Tanumafili II	3169	WS
SMR	San Marino	Europe	Southern Europe	61	885	27000	81.099998	510.00		San Marino	Republic		3171	SM
STP	Sao Tome and Principe	Africa	Central Africa	964	1975	147000	65.300003	6.00		Sao Tome e Principe	Republic	Miguel Trovoada	3172	ST
SAU	Saudi Arabia	Asia	Middle East	2149690	1932	21607000	67.800003	137635.00	146171.00	Al-Arabiya as-Saudiya	Monarchy	Fahd ibn Abdul-Aziz al-Saud	3173	SA
SEN	Senegal	Africa	Western Africa	196722	1960	9481000	62.200001	4787.00	4542.00	Senegal/Sounougal	Republic	Abdoulaye Wade	3198	SN
SYC	Seychelles	Africa	Eastern Africa	455	1976	77000	70.400002	536.00	539.00	Sesel/Seychelles	Republic	France-Albert Rene	3206	SC
SLE	Sierra Leone	Africa	Western Africa	71740	1961	4854000	45.299999	746.00	858.00	Sierra Leone	Republic	Ahmed Tejan Kabbah	3207	SL
SGP	Singapore	Asia	Southeast Asia	618	1965	3567000	80.099998	86503.00	96318.00	Singapore/Singapura/Xinjiapo/Singapur	Republic	Sellapan Rama Nathan	3208	SG
SVK	Slovakia	Europe	Eastern Europe	49012	1993	5398700	73.699997	20594.00	19452.00	Slovensko	Republic	Rudolf Schuster	3209	SK
SVN	Slovenia	Europe	Southern Europe	20256	1991	1987800	74.900002	19756.00	18202.00	Slovenija	Republic	Milan Kucan	3212	SI
SOM	Somalia	Africa	Eastern Africa	637657	1960	10097000	46.200001	935.00		Soomaaliya	Republic	Abdiqassim Salad Hassan	3214	SO
LKA	Sri Lanka	Asia	Southern and Central Asia	65610	1948	18827000	71.800003	15706.00	15091.00	Sri Lanka/Ilankai	Republic	Chandrika Kumaratunga	3217	LK
SDN	Sudan	Africa	Northern Africa	2505813	1956	29490000	56.599998	10162.00		As-Sudan	Islamic Republic	Omar Hassan Ahmad al-Bashir	3225	SD
FIN	Finland	Europe	Nordic Countries	338145	1917	5171300	77.400002	121914.00	119833.00	Suomi	Republic	Tarja Halonen	3236	FI
SUR	Suriname	South America	South America	163265	1975	417000	71.400002	870.00	706.00	Suriname	Republic	Ronald Venetiaan	3243	SR
SWZ	Swaziland	Africa	Southern Africa	17364	1968	1008000	40.400002	1206.00	1312.00	kaNgwane	Monarchy	Mswati III	3244	SZ
CHE	Switzerland	Europe	Western Europe	41284	1499	7160400	79.599998	264478.00	256092.00	Schweiz/Suisse/Svizzera/Svizra	Federation	Adolf Ogi	3248	CH
SYR	Syria	Asia	Middle East	185180	1941	16125000	68.5	65984.00	64926.00	Suriya	Republic	Bashar al-Assad	3250	SY
TJK	Tajikistan	Asia	Southern and Central Asia	143100	1991	6188000	64.099998	1990.00	1056.00	Tocikiston	Republic	Emomali Rahmonov	3261	TJ
TWN	Taiwan	Asia	Eastern Asia	36188	1945	22256000	76.400002	256254.00	263451.00	Teai-wan	Republic	Chen Shui-bian	3263	TW
TZA	Tanzania	Africa	Eastern Africa	883749	1961	33517000	52.299999	8005.00	7388.00	Tanzania	Republic	Benjamin William Mkapa	3306	TZ
DNK	Denmark	Europe	Nordic Countries	43094	800	5330000	76.5	174099.00	169264.00	Danmark	Constitutional Monarchy	Margrethe II	3315	DK
THA	Thailand	Asia	Southeast Asia	513115	1350	61399000	68.599998	116416.00	153907.00	Prathet Thai	Constitutional Monarchy	Bhumibol Adulyadej	3320	TH
TGO	Togo	Africa	Western Africa	56785	1960	4629000	54.700001	1449.00	1400.00	Togo	Republic	Gnassingbe Eyadema	3332	TG
TKL	Tokelau	Oceania	Polynesia	12		2000		0.00		Tokelau	Nonmetropolitan Territory of New Zealand	Elisabeth II	3333	TK
TON	Tonga	Oceania	Polynesia	650	1970	99000	67.900002	146.00	170.00	Tonga	Monarchy	Taufa'ahau Tupou IV	3334	TO
TTO	Trinidad and Tobago	North America	Caribbean	5130	1962	1295000	68	6232.00	5867.00	Trinidad and Tobago	Republic	Arthur N. R. Robinson	3336	TT
TCD	Chad	Africa	Central Africa	1284000	1960	7651000	50.5	1208.00	1102.00	Tchad/Tshad	Republic	Idriss Deby	3337	TD
CZE	Czech Republic	Europe	Eastern Europe	78866	1993	10278100	74.5	55017.00	52037.00	esko	Republic	Vaclav Havel	3339	CZ
TUN	Tunisia	Africa	Northern Africa	163610	1956	9586000	73.699997	20026.00	18898.00	Tunis/Tunisie	Republic	Zine al-Abidine Ben Ali	3349	TN
TUR	Turkey	Asia	Middle East	774815	1923	66591000	71	210721.00	189122.00	Turkiye	Republic	Ahmet Necdet Sezer	3358	TR
TKM	Turkmenistan	Asia	Southern and Central Asia	488100	1991	4459000	60.900002	4397.00	2000.00	Turkmenostan	Republic	Saparmurad Nijazov	3419	TM
TCA	Turks and Caicos Islands	North America	Caribbean	430		17000	73.300003	96.00		The Turks and Caicos Islands	Dependent Territory of the UK	Elisabeth II	3423	TC
TUV	Tuvalu	Oceania	Polynesia	26	1978	12000	66.300003	6.00		Tuvalu	Constitutional Monarchy	Elisabeth II	3424	TV
UGA	Uganda	Africa	Eastern Africa	241038	1962	21778000	42.900002	6313.00	6887.00	Uganda	Republic	Yoweri Museveni	3425	UG
UKR	Ukraine	Europe	Eastern Europe	603700	1991	50456000	66	42168.00	49677.00	Ukrajina	Republic	Leonid Kutdma	3426	UA
HUN	Hungary	Europe	Eastern Europe	93030	1918	10043200	71.400002	48267.00	45914.00	Magyarorszag	Republic	Ferenc Madl	3483	HU
URY	Uruguay	South America	South America	175016	1828	3337000	75.199997	20831.00	19967.00	Uruguay	Republic	Jorge Batlle Ibaaez	3492	UY
NCL	New Caledonia	Oceania	Melanesia	18575		214000	72.800003	3563.00		Nouvelle-Caledonie	Nonmetropolitan Territory of France	Jacques Chirac	3493	NC
NZL	New Zealand	Oceania	Australia and New Zealand	270534	1907	3862000	77.800003	54669.00	64960.00	New Zealand/Aotearoa	Constitutional Monarchy	Elisabeth II	3499	NZ
UZB	Uzbekistan	Asia	Southern and Central Asia	447400	1991	24318000	63.700001	14194.00	21300.00	Uzbekiston	Republic	Islam Karimov	3503	UZ
BLR	Belarus	Europe	Eastern Europe	207600	1991	10236000	68	13714.00		Belarus	Republic	Aljaksandr Lukadenka	3520	BY
WLF	Wallis and Futuna	Oceania	Polynesia	200		15000		0.00		Wallis-et-Futuna	Nonmetropolitan Territory of France	Jacques Chirac	3536	WF
VUT	Vanuatu	Oceania	Melanesia	12189	1980	190000	60.599998	261.00	246.00	Vanuatu	Republic	John Bani	3537	VU
VAT	Holy See (Vatican City State)	Europe	Southern Europe	0.40000001	1929	1000		9.00		Santa Sede/Citta del Vaticano	Independent Church State	Johannes Paavali II	3538	VA
VEN	Venezuela	South America	South America	912050	1811	24170000	73.099998	95023.00	88434.00	Venezuela	Federal Republic	Hugo Chavez Frias	3539	VE
RUS	Russian Federation	Europe	Eastern Europe	17075400	1991	146934000	67.199997	276608.00	442989.00	Rossija	Federal Republic	Vladimir Putin	3580	RU
VNM	Vietnam	Asia	Southeast Asia	331689	1945	79832000	69.300003	21929.00	22834.00	Viet Nam	Socialistic Republic	Tran Duc Luong	3770	VN
EST	Estonia	Europe	Baltic Countries	45227	1991	1439200	69.5	5328.00	3371.00	Eesti	Republic	Lennart Meri	3791	EE
USA	United States	North America	North America	9363520	1776	278357000	77.099998	8510700.00	8110900.00	United States	Federal Republic	George W. Bush	3813	US
VIR	Virgin Islands, U.S.	North America	Caribbean	347		93000	78.099998	0.00		Virgin Islands of the United States	US Territory	George W. Bush	4067	VI
ZWE	Zimbabwe	Africa	Eastern Africa	390757	1980	11669000	37.799999	5951.00	8670.00	Zimbabwe	Republic	Robert G. Mugabe	4068	ZW
PSE	Palestine	Asia	Middle East	6257		3101000	71.400002	4173.00		Filastin	Autonomous Area	Yasser (Yasir) Arafat	4074	PS
ATA	Antarctica	Antarctica	Antarctica	13120000		0		0.00		A	Co-administrated			AQ
BVT	Bouvet Island	Antarctica	Antarctica	59		0		0.00		Bouvetoya	Dependent Territory of Norway	Harald V		BV
IOT	British Indian Ocean Territory	Africa	Eastern Africa	78		0		0.00		British Indian Ocean Territory	Dependent Territory of the UK	Elisabeth II		IO
SGS	South Georgia and the South Sandwich Islands	Antarctica	Antarctica	3903		0		0.00		South Georgia and the South Sandwich Islands	Dependent Territory of the UK	Elisabeth II		GS
HMD	Heard Island and McDonald Islands	Antarctica	Antarctica	359		0		0.00		Heard and McDonald Islands	Territory of Australia	Elisabeth II		HM
ATF	French Southern territories	Antarctica	Antarctica	7780		0		0.00		Terres australes francaises	Nonmetropolitan Territory of France	Jacques Chirac		TF
UMI	United States Minor Outlying Islands	Oceania	Micronesia/Caribbean	16		0		0.00		United States Minor Outlying Islands	Dependent Territory of the US	George W. Bush		UM
\.


--
-- Data for Name: countrylanguage; Type: TABLE DATA; Schema: public; 
--

COPY countrylanguage_uao_using (countrycode, "language", isofficial, percentage) FROM stdin;
AFG	Pashto	t	52.400002
NLD	Dutch	t	95.599998
ANT	Papiamento	t	86.199997
ALB	Albaniana	t	97.900002
DZA	Arabic	t	86
ASM	Samoan	t	90.599998
AND	Spanish	f	44.599998
AGO	Ovimbundu	f	37.200001
AIA	English	t	0
ATG	Creole English	f	95.699997
ARE	Arabic	t	42
ARG	Spanish	t	96.800003
ARM	Armenian	t	93.400002
ABW	Papiamento	f	76.699997
AUS	English	t	81.199997
AZE	Azerbaijani	t	89
BHS	Creole English	f	89.699997
BHR	Arabic	t	67.699997
BGD	Bengali	t	97.699997
BRB	Bajan	f	95.099998
BEL	Dutch	t	59.200001
BLZ	English	t	50.799999
BEN	Fon	f	39.799999
BMU	English	t	100
BTN	Dzongkha	t	50
BOL	Spanish	t	87.699997
BIH	Serbo-Croatian	t	99.199997
BWA	Tswana	f	75.5
BRA	Portuguese	t	97.5
GBR	English	t	97.300003
VGB	English	t	0
BRN	Malay	t	45.5
BGR	Bulgariana	t	83.199997
BFA	Mossi	f	50.200001
BDI	Kirundi	t	98.099998
CYM	English	t	0
CHL	Spanish	t	89.699997
COK	Maori	t	0
CRI	Spanish	t	97.5
DJI	Somali	f	43.900002
DMA	Creole English	f	100
DOM	Spanish	t	98
ECU	Spanish	t	93
EGY	Arabic	t	98.800003
SLV	Spanish	t	100
ERI	Tigrinja	t	49.099998
ESP	Spanish	t	74.400002
ZAF	Zulu	t	22.700001
ETH	Oromo	f	31
FLK	English	t	0
FJI	Fijian	t	50.799999
PHL	Pilipino	t	29.299999
FRO	Faroese	t	100
GAB	Fang	f	35.799999
GMB	Malinke	f	34.099998
GEO	Georgiana	t	71.699997
GHA	Akan	f	52.400002
GIB	English	t	88.900002
GRD	Creole English	f	100
GRL	Greenlandic	t	87.5
GLP	Creole French	f	95
GUM	English	t	37.5
GTM	Spanish	t	64.699997
GIN	Ful	f	38.599998
GNB	Crioulo	f	36.400002
GUY	Creole English	f	96.400002
HTI	Haiti Creole	f	100
HND	Spanish	t	97.199997
HKG	Canton Chinese	f	88.699997
SJM	Norwegian	t	0
IDN	Javanese	f	39.400002
IND	Hindi	t	39.900002
IRQ	Arabic	t	77.199997
IRN	Persian	t	45.700001
IRL	English	t	98.400002
ISL	Icelandic	t	95.699997
ISR	Hebrew	t	63.099998
ITA	Italian	t	94.099998
TMP	Sunda	f	0
AUT	German	t	92
JAM	Creole English	f	94.199997
JPN	Japanese	t	99.099998
YEM	Arabic	t	99.599998
JOR	Arabic	t	97.900002
CXR	Chinese	f	0
YUG	Serbo-Croatian	t	75.199997
KHM	Khmer	t	88.599998
CMR	Fang	f	19.700001
CAN	English	t	60.400002
CPV	Crioulo	f	100
KAZ	Kazakh	t	46
KEN	Kikuyu	f	20.9
CAF	Gbaya	f	23.799999
CHN	Chinese	t	92
KGZ	Kirgiz	t	59.700001
KIR	Kiribati	t	98.900002
COL	Spanish	t	99
COM	Comorian	t	75
COG	Kongo	f	51.5
COD	Luba	f	18
CCK	Malay	f	0
PRK	Korean	t	99.900002
KOR	Korean	t	99.900002
GRC	Greek	t	98.5
HRV	Serbo-Croatian	t	95.900002
CUB	Spanish	t	100
KWT	Arabic	t	78.099998
CYP	Greek	t	74.099998
LAO	Lao	t	67.199997
LVA	Latvian	t	55.099998
LSO	Sotho	t	85
LBN	Arabic	t	93
LBR	Kpelle	f	19.5
LBY	Arabic	t	96
LIE	German	t	89
LTU	Lithuanian	t	81.599998
LUX	Luxembourgish	t	64.400002
ESH	Arabic	t	100
MAC	Canton Chinese	f	85.599998
MDG	Malagasy	t	98.900002
MKD	Macedonian	t	66.5
MWI	Chichewa	t	58.299999
MDV	Dhivehi	t	100
MYS	Malay	t	58.400002
MLI	Bambara	f	31.799999
MLT	Maltese	t	95.800003
MAR	Arabic	t	65
MHL	Marshallese	t	96.800003
MTQ	Creole French	f	96.599998
MRT	Hassaniya	f	81.699997
MUS	Creole French	f	70.599998
MYT	Mahore	f	41.900002
MEX	Spanish	t	92.099998
FSM	Trukese	f	41.599998
MDA	Romanian	t	61.900002
MCO	French	t	41.900002
MNG	Mongolian	t	78.800003
MSR	English	t	0
MOZ	Makua	f	27.799999
MMR	Burmese	t	69
NAM	Ovambo	f	50.700001
NRU	Nauru	t	57.5
NPL	Nepali	t	50.400002
NIC	Spanish	t	97.599998
NER	Hausa	f	53.099998
NGA	Joruba	f	21.4
NIU	Niue	f	0
NFK	English	t	0
NOR	Norwegian	t	96.599998
CIV	Akan	f	30
OMN	Arabic	t	76.699997
PAK	Punjabi	f	48.200001
PLW	Palau	t	82.199997
PAN	Spanish	t	76.800003
PNG	Papuan Languages	f	78.099998
PRY	Spanish	t	55.099998
PER	Spanish	t	79.800003
PCN	Pitcairnese	f	0
MNP	Philippene Languages	f	34.099998
PRT	Portuguese	t	99
PRI	Spanish	t	51.299999
POL	Polish	t	97.599998
GNQ	Fang	f	84.800003
QAT	Arabic	t	40.700001
FRA	French	t	93.599998
GUF	Creole French	f	94.300003
PYF	Tahitian	f	46.400002
REU	Creole French	f	91.5
ROM	Romanian	t	90.699997
RWA	Rwanda	t	100
SWE	Swedish	t	89.5
SHN	English	t	0
KNA	Creole English	f	100
LCA	Creole French	f	80
VCT	Creole English	f	99.099998
SPM	French	t	0
DEU	German	t	91.300003
SLB	Malenasian Languages	f	85.599998
ZMB	Bemba	f	29.700001
WSM	Samoan-English	f	52
SMR	Italian	t	100
STP	Crioulo	f	86.300003
SAU	Arabic	t	95
SEN	Wolof	t	48.099998
SYC	Seselwa	f	91.300003
SLE	Mende	f	34.799999
SGP	Chinese	t	77.099998
SVK	Slovak	t	85.599998
SVN	Slovene	t	87.900002
SOM	Somali	t	98.300003
LKA	Singali	t	60.299999
SDN	Arabic	t	49.400002
FIN	Finnish	t	92.699997
SUR	Sranantonga	f	81
SWZ	Swazi	t	89.900002
CHE	German	t	63.599998
SYR	Arabic	t	90
TJK	Tadzhik	t	62.200001
TWN	Min	f	66.699997
TZA	Nyamwesi	f	21.1
DNK	Danish	t	93.5
THA	Thai	t	52.599998
TGO	Ewe	t	23.200001
TKL	Tokelau	f	0
TON	Tongan	t	98.300003
TTO	English	f	93.5
TCD	Sara	f	27.700001
CZE	Czech	t	81.199997
TUN	Arabic	t	69.900002
TUR	Turkish	t	87.599998
TKM	Turkmenian	t	76.699997
TCA	English	t	0
TUV	Tuvalu	t	92.5
UGA	Ganda	f	18.1
UKR	Ukrainian	t	64.699997
HUN	Hungarian	t	98.5
URY	Spanish	t	95.699997
NCL	Malenasian Languages	f	45.400002
NZL	English	t	87
UZB	Uzbek	t	72.599998
BLR	Belorussian	t	65.599998
WLF	Wallis	f	0
VUT	Bislama	t	56.599998
VAT	Italian	t	0
VEN	Spanish	t	96.900002
RUS	Russian	t	86.599998
VNM	Vietnamese	t	86.800003
EST	Estonian	t	65.300003
USA	English	t	86.199997
VIR	English	t	81.699997
UMI	English	t	0
ZWE	Shona	f	72.099998
PSE	Arabic	f	95.900002
AFG	Dari	t	32.099998
NLD	Fries	f	3.7
ANT	English	f	7.8000002
ALB	Greek	f	1.8
DZA	Berberi	f	14
ASM	English	t	3.0999999
AND	Catalan	t	32.299999
AGO	Mbundu	f	21.6
ATG	English	t	0
ARE	Hindi	f	0
ARG	Italian	f	1.7
ARM	Azerbaijani	f	2.5999999
ABW	English	f	9.5
AUS	Italian	f	2.2
AZE	Russian	f	3
BHS	Creole French	f	10.3
BHR	English	f	0
BGD	Chakma	f	0.40000001
BRB	English	t	0
BEL	French	t	32.599998
BLZ	Spanish	f	31.6
BEN	Joruba	f	12.2
BTN	Nepali	f	34.799999
BOL	Ketdua	t	8.1000004
BWA	Shona	f	12.3
BRA	German	f	0.5
GBR	Kymri	f	0.89999998
BRN	Malay-English	f	28.799999
BGR	Turkish	f	9.3999996
BFA	Ful	f	9.6999998
BDI	French	t	0
CHL	Araucan	f	9.6000004
COK	English	f	0
CRI	Creole English	f	2
DJI	Afar	f	34.799999
DMA	Creole French	f	0
DOM	Creole French	f	2
ECU	Ketdua	f	7
EGY	Sinaberberi	f	0
SLV	Nahua	f	0
ERI	Tigre	f	31.700001
ESP	Catalan	f	16.9
ZAF	Xhosa	t	17.700001
ETH	Amhara	f	30
FJI	Hindi	f	43.700001
PHL	Cebuano	f	23.299999
FRO	Danish	t	0
GAB	Punu-sira-nzebi	f	17.1
GMB	Ful	f	16.200001
GEO	Russian	f	8.8000002
GHA	Mossi	f	15.8
GIB	Arabic	f	7.4000001
GRL	Danish	t	12.5
GLP	French	t	0
GUM	Chamorro	t	29.6
GTM	Quiche	f	10.1
GIN	Malinke	f	23.200001
GNB	Ful	f	16.6
GUY	Caribbean	f	2.2
HTI	French	t	0
HND	Garifuna	f	1.3
HKG	English	t	2.2
SJM	Russian	f	0
IDN	Sunda	f	15.8
IND	Bengali	f	8.1999998
IRQ	Kurdish	f	19
IRN	Azerbaijani	f	16.799999
IRL	Irish	t	1.6
ISL	English	f	0
ISR	Arabic	t	18
ITA	Sardinian	f	2.7
TMP	Portuguese	t	0
AUT	Serbo-Croatian	f	2.2
JAM	Hindi	f	1.9
JPN	Korean	f	0.5
YEM	Soqutri	f	0
JOR	Circassian	f	1
CXR	English	t	0
YUG	Albaniana	f	16.5
KHM	Vietnamese	f	5.5
CMR	Bamileke-bamum	f	18.6
CAN	French	t	23.4
CPV	Portuguese	t	0
KAZ	Russian	f	34.700001
KEN	Luhya	f	13.8
CAF	Banda	f	23.5
CHN	Zhuang	f	1.4
KGZ	Russian	t	16.200001
KIR	Tuvalu	f	0.5
COL	Chibcha	f	0.40000001
COM	Comorian-French	f	12.9
COG	Teke	f	17.299999
COD	Kongo	f	16
CCK	English	t	0
PRK	Chinese	f	0.1
KOR	Chinese	f	0.1
GRC	Turkish	f	0.89999998
HRV	Slovene	f	0
KWT	English	f	0
CYP	Turkish	t	22.4
LAO	Mon-khmer	f	16.5
LVA	Russian	f	32.5
LSO	Zulu	f	15
LBN	Armenian	f	5.9000001
LBR	Bassa	f	13.7
LBY	Berberi	f	1
LIE	Italian	f	2.5
LTU	Russian	f	8.1000004
LUX	Portuguese	f	13
MAC	Portuguese	t	2.3
MDG	French	t	0
MKD	Albaniana	f	22.9
MWI	Lomwe	f	18.4
MDV	English	f	0
MYS	Chinese	f	9
MLI	Ful	f	13.9
MLT	English	t	2.0999999
MAR	Berberi	f	33
MHL	English	t	0
MTQ	French	t	0
MRT	Wolof	f	6.5999999
MUS	Bhojpuri	f	21.1
MYT	French	t	20.299999
MEX	Nahuatl	f	1.8
FSM	Pohnpei	f	23.799999
MDA	Russian	f	23.200001
MCO	Monegasque	f	16.1
MNG	Kazakh	f	5.9000001
MOZ	Tsonga	f	12.4
MMR	Shan	f	8.5
NAM	Nama	f	12.4
NRU	Kiribati	f	17.9
NPL	Maithili	f	11.9
NIC	Miskito	f	1.6
NER	Songhai-zerma	f	21.200001
NGA	Hausa	f	21.1
NIU	English	t	0
NOR	English	f	0.5
CIV	Gur	f	11.7
OMN	Balochi	f	0
PAK	Pashto	f	13.1
PLW	Philippene Languages	f	9.1999998
PAN	Creole English	f	14
PNG	Malenasian Languages	f	20
PRY	Guarani	t	40.099998
PER	Ketdua	t	16.4
MNP	Chamorro	f	30
PRI	English	f	47.400002
POL	German	f	1.3
GNQ	Bubi	f	8.6999998
QAT	Urdu	f	0
FRA	Arabic	f	2.5
GUF	Indian Languages	f	1.9
PYF	French	t	40.799999
REU	Chinese	f	2.8
ROM	Hungarian	f	7.1999998
RWA	French	t	0
SWE	Finnish	f	2.4000001
KNA	English	t	0
LCA	English	t	20
VCT	English	t	0
DEU	Turkish	f	2.5999999
SLB	Papuan Languages	f	8.6000004
ZMB	Tongan	f	11
WSM	Samoan	t	47.5
STP	French	f	0.69999999
SEN	Ful	f	21.700001
SYC	English	t	3.8
SLE	Temne	f	31.799999
SGP	Malay	t	14.1
SVK	Hungarian	f	10.5
SVN	Serbo-Croatian	f	7.9000001
SOM	Arabic	t	0
LKA	Tamil	t	19.6
SDN	Dinka	f	11.5
FIN	Swedish	t	5.6999998
SUR	Hindi	f	0
SWZ	Zulu	f	2
CHE	French	t	19.200001
SYR	Kurdish	f	9
TJK	Uzbek	f	23.200001
TWN	Mandarin Chinese	t	20.1
TZA	Swahili	t	8.8000002
DNK	Turkish	f	0.80000001
THA	Lao	f	26.9
TGO	Kabye	t	13.8
TKL	English	t	0
TON	English	t	0
TTO	Hindi	f	3.4000001
TCD	Arabic	t	12.3
CZE	Moravian	f	12.9
TUN	Arabic-French	f	26.299999
TUR	Kurdish	f	10.6
TKM	Uzbek	f	9.1999998
TUV	Kiribati	f	7.5
UGA	Nkole	f	10.7
UKR	Russian	f	32.900002
HUN	Romani	f	0.5
NCL	French	t	34.299999
NZL	Maori	f	4.3000002
UZB	Russian	f	10.9
BLR	Russian	t	32
WLF	Futuna	f	0
VUT	English	t	28.299999
VEN	Goajiro	f	0.40000001
RUS	Tatar	f	3.2
VNM	Tho	f	1.8
EST	Russian	f	27.799999
USA	Spanish	f	7.5
VIR	Spanish	f	13.3
ZWE	Ndebele	f	16.200001
PSE	Hebrew	f	4.0999999
AFG	Uzbek	f	8.8000002
NLD	Arabic	f	0.89999998
ANT	Dutch	t	0
ALB	Macedonian	f	0.1
ASM	Tongan	f	3.0999999
AND	Portuguese	f	10.8
AGO	Kongo	f	13.2
ARG	Indian Languages	f	0.30000001
ABW	Spanish	f	7.4000001
AUS	Greek	f	1.6
AZE	Lezgian	f	2.3
BGD	Marma	f	0.2
BEL	Italian	f	2.4000001
BLZ	Maya Languages	f	9.6000004
BEN	Adja	f	11.1
BTN	Asami	f	15.2
BOL	Aimara	t	3.2
BWA	San	f	3.5
BRA	Italian	f	0.40000001
GBR	Gaeli	f	0.1
BRN	Chinese	f	9.3000002
BGR	Romani	f	3.7
BFA	Gurma	f	5.6999998
BDI	Swahili	f	0
CHL	Aimara	f	0.5
CRI	Chibcha	f	0.30000001
DJI	Arabic	t	10.6
ERI	Afar	f	4.3000002
ESP	Galecian	f	6.4000001
ZAF	Afrikaans	t	14.3
ETH	Tigrinja	f	7.1999998
PHL	Ilocano	f	9.3000002
GAB	Mpongwe	f	14.6
GMB	Wolof	f	12.6
GEO	Armenian	f	6.8000002
GHA	Ewe	f	11.9
GUM	Philippene Languages	f	19.700001
GTM	Cakchiquel	f	8.8999996
GIN	Susu	f	11
GNB	Balante	f	14.6
GUY	Arawakan	f	1.4
HND	Creole English	f	0.2
HKG	Fukien	f	1.9
IDN	Malay	t	12.1
IND	Telugu	f	7.8000002
IRQ	Azerbaijani	f	1.7
IRN	Kurdish	f	9.1000004
ISR	Russian	f	8.8999996
ITA	Friuli	f	1.2
AUT	Turkish	f	1.5
JPN	Chinese	f	0.2
JOR	Armenian	f	1
YUG	Hungarian	f	3.4000001
KHM	Chinese	f	3.0999999
CMR	Duala	f	10.9
CAN	Chinese	f	2.5
KAZ	Ukrainian	f	5
KEN	Luo	f	12.8
CAF	Mandjia	f	14.8
CHN	Mantdu	f	0.89999998
KGZ	Uzbek	f	14.1
COL	Creole English	f	0.1
COM	Comorian-madagassi	f	5.5
COG	Mboshi	f	11.4
COD	Mongo	f	13.5
LAO	Thai	f	7.8000002
LVA	Belorussian	f	4.0999999
LSO	English	t	0
LBN	French	f	0
LBR	Grebo	f	8.8999996
LIE	Turkish	f	2.5
LTU	Polish	f	7
LUX	Italian	f	4.5999999
MAC	Mandarin Chinese	f	1.2
MKD	Turkish	f	4
MWI	Yao	f	13.2
MYS	Tamil	f	3.9000001
MLI	Senufo and Minianka	f	12
MRT	Tukulor	f	5.4000001
MUS	French	f	3.4000001
MYT	Malagasy	f	16.1
MEX	Yucatec	f	1.1
FSM	Mortlock	f	7.5999999
MDA	Ukrainian	f	8.6000004
MCO	Italian	f	16.1
MNG	Dorbet	f	2.7
MOZ	Sena	f	9.3999996
MMR	Karen	f	6.1999998
NAM	Kavango	f	9.6999998
NRU	Chinese	f	8.5
NPL	Bhojpuri	f	7.5
NIC	Creole English	f	0.5
NER	Tamashek	f	10.4
NGA	Ibo	f	18.1
NOR	Danish	f	0.40000001
CIV	Malinke	f	11.4
PAK	Sindhi	f	11.8
PLW	English	t	3.2
PAN	Guaymi	f	5.3000002
PRY	Portuguese	f	3.2
PER	Aimara	t	2.3
MNP	Chinese	f	7.0999999
POL	Ukrainian	f	0.60000002
FRA	Portuguese	f	1.2
PYF	Chinese	f	2.9000001
REU	Comorian	f	2.8
ROM	Romani	t	0.69999999
SWE	Southern Slavic Languages	f	1.3
DEU	Southern Slavic Languages	f	1.4
SLB	Polynesian Languages	f	3.8
ZMB	Nyanja	f	7.8000002
WSM	English	t	0.60000002
SEN	Serer	f	12.5
SYC	French	t	1.3
SLE	Limba	f	8.3000002
SGP	Tamil	t	7.4000001
SVK	Romani	f	1.7
SVN	Hungarian	f	0.5
LKA	Mixed Languages	f	19.6
SDN	Nubian Languages	f	8.1000004
FIN	Russian	f	0.40000001
CHE	Italian	t	7.6999998
TJK	Russian	f	9.6999998
TWN	Hakka	f	11
TZA	Hehet	f	6.9000001
DNK	Arabic	f	0.69999999
THA	Chinese	f	12.1
TGO	Watyi	f	10.3
TTO	Creole English	f	2.9000001
TCD	Mayo-kebbi	f	11.5
CZE	Slovak	f	3.0999999
TUN	Arabic-French-English	f	3.2
TUR	Arabic	f	1.4
TKM	Russian	f	6.6999998
TUV	English	t	0
UGA	Kiga	f	8.3000002
UKR	Romanian	f	0.69999999
HUN	German	f	0.40000001
NCL	Polynesian Languages	f	11.6
UZB	Tadzhik	f	4.4000001
BLR	Ukrainian	f	1.3
VUT	French	t	14.2
VEN	Warrau	f	0.1
RUS	Ukrainian	f	1.3
VNM	Thai	f	1.6
EST	Ukrainian	f	2.8
USA	French	f	0.69999999
VIR	French	f	2.5
ZWE	English	t	2.2
AFG	Turkmenian	f	1.9
NLD	Turkish	f	0.80000001
AND	French	f	6.1999998
AGO	Luimbe-nganguela	f	5.4000001
ABW	Dutch	t	5.3000002
AUS	Canton Chinese	f	1.1
AZE	Armenian	f	2
BGD	Garo	f	0.1
BEL	Arabic	f	1.6
BLZ	Garifuna	f	6.8000002
BEN	Aizo	f	8.6999998
BOL	Guarani	f	0.1
BWA	Khoekhoe	f	2.5
BRA	Japanese	f	0.40000001
BRN	English	f	3.0999999
BGR	Macedonian	f	2.5999999
BFA	Busansi	f	3.5
CHL	Rapa nui	f	0.2
CRI	Chinese	f	0.2
ERI	Hadareb	f	3.8
ESP	Basque	f	1.6
ZAF	Northsotho	f	9.1000004
ETH	Gurage	f	4.6999998
PHL	Hiligaynon	f	9.1000004
GAB	Mbete	f	13.8
GMB	Diola	f	9.1999998
GEO	Azerbaijani	f	5.5
GHA	Ga-adangme	f	7.8000002
GUM	Korean	f	3.3
GTM	Kekchi	f	4.9000001
GIN	Kissi	f	6
GNB	Portuguese	t	8.1000004
HND	Miskito	f	0.2
HKG	Hakka	f	1.6
IDN	Madura	f	4.3000002
IND	Marathi	f	7.4000001
IRQ	Assyrian	f	0.80000001
IRN	Gilaki	f	5.3000002
ITA	French	f	0.5
AUT	Hungarian	f	0.40000001
JPN	English	f	0.1
YUG	Romani	f	1.4
KHM	Tdam	f	2.4000001
CMR	Ful	f	9.6000004
CAN	Italian	f	1.7
KAZ	German	f	3.0999999
KEN	Kamba	f	11.2
CAF	Ngbaka	f	7.5
CHN	Hui	f	0.80000001
KGZ	Ukrainian	f	1.7
COL	Arawakan	f	0.1
COM	Comorian-Arabic	f	1.6
COG	Mbete	f	4.8000002
COD	Rwanda	f	10.3
LAO	Lao-Soung	f	5.1999998
LVA	Ukrainian	f	2.9000001
LBR	Gio	f	7.9000001
LTU	Belorussian	f	1.4
LUX	French	t	4.1999998
MAC	English	f	0.5
MKD	Romani	f	2.3
MWI	Ngoni	f	6.6999998
MYS	Iban	f	2.8
MLI	Soninke	f	8.6999998
MRT	Soninke	f	2.7
MUS	Hindi	f	1.2
MEX	Zapotec	f	0.60000002
FSM	Kosrean	f	7.3000002
MDA	Gagauzi	f	3.2
MCO	English	f	6.5
MNG	Bajad	f	1.9
MOZ	Lomwe	f	7.8000002
MMR	Rakhine	f	4.5
NAM	Afrikaans	f	9.5
NRU	Tuvalu	f	8.5
NPL	Tharu	f	5.4000001
NIC	Sumo	f	0.2
NER	Ful	f	9.6999998
NGA	Ful	f	11.3
NOR	Swedish	f	0.30000001
CIV	Kru	f	10.5
PAK	Saraiki	f	9.8000002
PLW	Chinese	f	1.6
PAN	Cuna	f	2
PRY	German	f	0.89999998
MNP	Korean	f	6.5
POL	Belorussian	f	0.5
FRA	Italian	f	0.40000001
REU	Malagasy	f	1.4
ROM	German	f	0.40000001
SWE	Arabic	f	0.80000001
DEU	Italian	f	0.69999999
ZMB	Lozi	f	6.4000001
SEN	Diola	f	5
SLE	Kono-vai	f	5.0999999
SVK	Czech and Moravian	f	1.1
SDN	Beja	f	6.4000001
FIN	Estonian	f	0.2
CHE	Romansh	t	0.60000002
TWN	Ami	f	0.60000002
TZA	Haya	f	5.9000001
DNK	German	f	0.5
THA	Malay	f	3.5999999
TGO	Kotokoli	f	5.6999998
TCD	Kanem-bornu	f	9
CZE	Polish	f	0.60000002
TKM	Kazakh	f	2
UGA	Soga	f	8.1999998
UKR	Bulgariana	f	0.30000001
HUN	Serbo-Croatian	f	0.2
UZB	Kazakh	f	3.8
BLR	Polish	f	0.60000002
RUS	Chuvash	f	0.89999998
VNM	Muong	f	1.5
EST	Belorussian	f	1.4
USA	German	f	0.69999999
ZWE	Nyanja	f	2.2
AFG	Balochi	f	0.89999998
AGO	Nyaneka-nkhumbi	f	5.4000001
AUS	Arabic	f	1
BGD	Khasi	f	0.1
BEL	German	t	1
BEN	Bariba	f	8.6999998
BWA	Ndebele	f	1.3
BRA	Indian Languages	f	0.2
BFA	Dagara	f	3.0999999
ERI	Bilin	f	3
ZAF	English	t	8.5
ETH	Somali	f	4.0999999
PHL	Bicol	f	5.6999998
GMB	Soninke	f	7.5999999
GEO	Osseetti	f	2.4000001
GHA	Gurma	f	3.3
GUM	Japanese	f	2
GTM	Mam	f	2.7
GIN	Kpelle	f	4.5999999
GNB	Malinke	f	6.9000001
HKG	Chiu chau	f	1.4
IDN	Minangkabau	f	2.4000001
IND	Tamil	f	6.3000002
IRQ	Persian	f	0.80000001
IRN	Luri	f	4.3000002
ITA	German	f	0.5
AUT	Slovene	f	0.40000001
JPN	Philippene Languages	f	0.1
YUG	Slovak	f	0.69999999
CMR	Tikar	f	7.4000001
CAN	German	f	1.6
KAZ	Uzbek	f	2.3
KEN	Kalenjin	f	10.8
CAF	Sara	f	6.4000001
CHN	Miao	f	0.69999999
KGZ	Tatar	f	1.3
COL	Caribbean	f	0.1
COM	Comorian-Swahili	f	0.5
COG	Punu	f	2.9000001
COD	Zande	f	6.0999999
LVA	Polish	f	2.0999999
LBR	Kru	f	7.1999998
LTU	Ukrainian	f	1.1
LUX	German	t	2.3
MKD	Serbo-Croatian	f	2
MYS	English	f	1.6
MLI	Tamashek	f	7.3000002
MRT	Ful	f	1.2
MUS	Tamil	f	0.80000001
MEX	Mixtec	f	0.60000002
FSM	Yap	f	5.8000002
MDA	Bulgariana	f	1.6
MNG	Buryat	f	1.7
MOZ	Shona	f	6.5
MMR	Mon	f	2.4000001
NAM	Herero	f	8
NRU	English	t	7.5
NPL	Tamang	f	4.9000001
NER	Kanuri	f	4.4000001
NGA	Ibibio	f	5.5999999
NOR	Saame	f	0
CIV	[South]Mande	f	7.6999998
PAK	Urdu	t	7.5999999
PAN	Embera	f	0.60000002
MNP	English	t	4.8000002
FRA	Spanish	f	0.40000001
REU	Tamil	f	0
ROM	Ukrainian	f	0.30000001
SWE	Spanish	f	0.60000002
DEU	Greek	f	0.40000001
ZMB	Chewa	f	5.6999998
SEN	Malinke	f	3.8
SLE	Bullom-sherbro	f	3.8
SVK	Ukrainian and Russian	f	0.60000002
SDN	Nuer	f	4.9000001
FIN	Saame	f	0
TWN	Atayal	f	0.40000001
TZA	Makonde	f	5.9000001
DNK	English	f	0.30000001
THA	Khmer	f	1.3
TGO	Ane	f	5.6999998
TCD	Ouaddai	f	8.6999998
CZE	German	f	0.5
UGA	Teso	f	6
UKR	Hungarian	f	0.30000001
HUN	Romanian	f	0.1
UZB	Karakalpak	f	2
RUS	Bashkir	f	0.69999999
VNM	Chinese	f	1.4
EST	Finnish	f	0.69999999
USA	Italian	f	0.60000002
AGO	Chokwe	f	4.1999998
AUS	Vietnamese	f	0.80000001
BGD	Santhali	f	0.1
BEL	Turkish	f	0.89999998
BEN	Somba	f	6.6999998
BFA	Dyula	f	2.5999999
ERI	Saho	f	3
ZAF	Tswana	f	8.1000004
ETH	Sidamo	f	3.2
PHL	Waray-waray	f	3.8
GEO	Abhyasi	f	1.7
GHA	Joruba	f	1.3
GIN	Yalunka	f	2.9000001
GNB	Mandyako	f	4.9000001
IDN	Batakki	f	2.2
IND	Urdu	f	5.0999999
IRN	Mazandarani	f	3.5999999
ITA	Albaniana	f	0.2
AUT	Polish	f	0.2
JPN	Ainu	f	0
YUG	Macedonian	f	0.5
CMR	Mandara	f	5.6999998
CAN	Polish	f	0.69999999
KAZ	Tatar	f	2
KEN	Gusii	f	6.0999999
CAF	Mbum	f	6.4000001
CHN	Uighur	f	0.60000002
KGZ	Kazakh	f	0.80000001
COG	Sango	f	2.5999999
COD	Ngala and Bangi	f	5.8000002
LVA	Lithuanian	f	1.2
LBR	Mano	f	7.1999998
MYS	Dusun	f	1.1
MLI	Songhai	f	6.9000001
MRT	Zenaga	f	1.2
MUS	Marathi	f	0.69999999
MEX	Otomi	f	0.40000001
FSM	Wolea	f	3.7
MNG	Dariganga	f	1.4
MOZ	Tswa	f	6
MMR	Chin	f	2.2
NAM	Caprivi	f	4.6999998
NPL	Newari	f	3.7
NGA	Kanuri	f	4.0999999
PAK	Balochi	f	3
PAN	Arabic	f	0.60000002
MNP	Carolinian	f	4.8000002
FRA	Turkish	f	0.40000001
ROM	Serbo-Croatian	f	0.1
SWE	Norwegian	f	0.5
DEU	Polish	f	0.30000001
ZMB	Nsenga	f	4.3000002
SEN	Soninke	f	1.3
SLE	Ful	f	3.8
SDN	Zande	f	2.7
TWN	Paiwan	f	0.30000001
TZA	Nyakusa	f	5.4000001
DNK	Swedish	f	0.30000001
THA	Kuy	f	1.1
TGO	Moba	f	5.4000001
TCD	Hadjarai	f	6.6999998
CZE	Silesiana	f	0.40000001
UGA	Lango	f	5.9000001
UKR	Belorussian	f	0.30000001
HUN	Slovak	f	0.1
UZB	Tatar	f	1.8
RUS	Chechen	f	0.60000002
VNM	Khmer	f	1.4
USA	Chinese	f	0.60000002
AGO	Luvale	f	3.5999999
AUS	Serbo-Croatian	f	0.60000002
BGD	Tripuri	f	0.1
BEN	Ful	f	5.5999999
ZAF	Southsotho	f	7.5999999
ETH	Walaita	f	2.8
PHL	Pampango	f	3
GIN	Loma	f	2.3
IDN	Bugi	f	2.2
IND	Gujarati	f	4.8000002
IRN	Balochi	f	2.3
ITA	Slovene	f	0.2
AUT	Czech	f	0.2
CMR	Maka	f	4.9000001
CAN	Spanish	f	0.69999999
KEN	Meru	f	5.5
CHN	Yi	f	0.60000002
KGZ	Tadzhik	f	0.80000001
COD	Rundi	f	3.8
LBR	Loma	f	5.8000002
MOZ	Chuabo	f	5.6999998
MMR	Kachin	f	1.4
NAM	San	f	1.9
NPL	Hindi	f	3
NGA	Edo	f	3.3
PAK	Hindko	f	2.4000001
SLE	Kuranko	f	3.4000001
SDN	Bari	f	2.5
TZA	Chaga and Pare	f	4.9000001
DNK	Norwegian	f	0.30000001
TGO	Naudemba	f	4.0999999
TCD	Tandjile	f	6.5
CZE	Romani	f	0.30000001
UGA	Lugbara	f	4.6999998
UKR	Polish	f	0.1
RUS	Mordva	f	0.5
VNM	Nung	f	1.1
USA	Tagalog	f	0.40000001
AGO	Ambo	f	2.4000001
AUS	German	f	0.60000002
ZAF	Tsonga	f	4.3000002
PHL	Pangasinan	f	1.8
IDN	Banja	f	1.8
IND	Kannada	f	3.9000001
IRN	Arabic	f	2.2
ITA	Romani	f	0.2
AUT	Romanian	f	0.2
CMR	Masana	f	3.9000001
CAN	Portuguese	f	0.69999999
KEN	Nyika	f	4.8000002
CHN	Tujia	f	0.5
COD	Teke	f	2.7
LBR	Malinke	f	5.0999999
MOZ	Ronga	f	3.7
MMR	Kayah	f	0.40000001
NAM	German	f	0.89999998
NGA	Tiv	f	2.3
PAK	Brahui	f	1.2
SLE	Yalunka	f	3.4000001
SDN	Fur	f	2.0999999
TZA	Luguru	f	4.9000001
TGO	Gurma	f	3.4000001
TCD	Gorane	f	6.1999998
CZE	Hungarian	f	0.2
UGA	Gisu	f	4.5
RUS	Kazakh	f	0.40000001
VNM	Miao	f	0.89999998
USA	Polish	f	0.30000001
AGO	Luchazi	f	2.4000001
ZAF	Swazi	f	2.5
PHL	Maguindanao	f	1.4
IDN	Bali	f	1.7
IND	Malajalam	f	3.5999999
IRN	Bakhtyari	f	1.7
CAN	Punjabi	f	0.69999999
KEN	Masai	f	1.6
CHN	Mongolian	f	0.40000001
COD	Boa	f	2.3
MOZ	Marendje	f	3.5
NGA	Ijo	f	1.8
SDN	Chilluk	f	1.7
TZA	Shambala	f	4.3000002
UGA	Acholi	f	4.4000001
RUS	Avarian	f	0.40000001
VNM	Man	f	0.69999999
USA	Korean	f	0.30000001
ZAF	Venda	f	2.2
PHL	Maranao	f	1.3
IND	Orija	f	3.3
IRN	Turkmenian	f	1.6
CAN	Ukrainian	f	0.60000002
KEN	Turkana	f	1.4
CHN	Tibetan	f	0.40000001
COD	Chokwe	f	1.8
MOZ	Nyanja	f	3.3
NGA	Bura	f	1.6
SDN	Lotuko	f	1.5
TZA	Gogo	f	3.9000001
UGA	Rwanda	f	3.2
RUS	Mari	f	0.40000001
USA	Vietnamese	f	0.2
ZAF	Ndebele	f	1.5
IND	Punjabi	f	2.8
CAN	Dutch	f	0.5
CHN	Puyi	f	0.2
TZA	Ha	f	3.5
RUS	Udmur	f	0.30000001
USA	Japanese	f	0.2
IND	Asami	f	1.5
CAN	Eskimo Languages	f	0.1
CHN	Dong	f	0.2
RUS	Belorussian	f	0.30000001
USA	Portuguese	f	0.2
\.



COMMIT;

ANALYZE city_uao_using;
ANALYZE country_uao_using;
ANALYZE countrylanguage_uao_using;


--query1
with capitals_uao as 
(select country_uao_using.code,id,city_uao_using.name from city_uao_using,country_uao_using 
 where city_uao_using.countrycode = country_uao_using.code AND city_uao_using.id = country_uao_using.capital) 
select capitals_uao.code,language from 
capitals_uao,countrylanguage_uao_using
where capitals_uao.code = countrylanguage_uao_using.countrycode and isofficial='true'
order by capitals_uao.code asc,countrylanguage_uao_using.language asc LIMIT 10;

with capitals_uao as
(select country_uao_using.code,id,city_uao_using.name from city_uao_using,country_uao_using
 where city_uao_using.countrycode = country_uao_using.code AND city_uao_using.id = country_uao_using.capital)
select capitals_uao.code,language from
capitals_uao,countrylanguage_uao_using
where capitals_uao.code = countrylanguage_uao_using.countrycode and isofficial='true'
order by capitals_uao.code desc, 2 desc LIMIT 10;

with capitals_uao as
(select country_uao_using.code,id,city_uao_using.name from city_uao_using,country_uao_using
 where city_uao_using.countrycode = country_uao_using.code AND city_uao_using.id = country_uao_using.capital)
select capitals_uao.code,language from
capitals_uao,countrylanguage_uao_using
where capitals_uao.code = countrylanguage_uao_using.countrycode and isofficial='true'
order by capitals_uao.code asc,countrylanguage_uao_using.language desc LIMIT 10;

with capitals_uao as
(select country_uao_using.code,id,city_uao_using.name from city_uao_using,country_uao_using
 where city_uao_using.countrycode = country_uao_using.code AND city_uao_using.id = country_uao_using.capital)
select capitals_uao.code,language from
capitals_uao,countrylanguage_uao_using
where capitals_uao.code = countrylanguage_uao_using.countrycode and isofficial='true'
order by capitals_uao.code using > , 2  using > LIMIT 10;

with capitals_uao as
(select country_uao_using.code,id,city_uao_using.name from city_uao_using,country_uao_using
 where city_uao_using.countrycode = country_uao_using.code AND city_uao_using.id = country_uao_using.capital)
select capitals_uao.code,language from
capitals_uao,countrylanguage_uao_using
where capitals_uao.code = countrylanguage_uao_using.countrycode and isofficial='true'
order by capitals_uao.code USING < , 2 using < LIMIT 10;
