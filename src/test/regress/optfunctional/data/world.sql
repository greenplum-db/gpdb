--
-- PostgreSQL port of the MySQL "World" database.
--
-- The sample data used in the world database is Copyright Statistics 
-- Finland, http://www.stat.fi/worldinfigures.
--

-- Modified to use it with GPDB

DROP TABLE IF EXISTS city cascade;

DROP TABLE IF EXISTS country cascade;

DROP TABLE IF EXISTS countrylanguage cascade;

--SET client_encoding = 'LATIN1';


CREATE TABLE city (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) distributed by(id);



CREATE TABLE country (
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
) distributed by (code);



CREATE TABLE countrylanguage (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
)distributed by (countrycode,language);

COPY city (id, name, countrycode, district, population) FROM stdin;
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
100	Parana	ARG	Entre Rios	207041
101	Godoy Cruz	ARG	Mendoza	206998
102	Posadas	ARG	Misiones	201273
103	Guaymallen	ARG	Mendoza	200595
104	Santiago del Estero	ARG	Santiago del Estero	189947
105	San Salvador de Jujuy	ARG	Jujuy	178748
106	Hurlingham	ARG	Buenos Aires	170028
107	Neuquen	ARG	Neuquen	167296
108	Ituzaingo	ARG	Buenos Aires	158197
109	San Fernando	ARG	Buenos Aires	153036
110	Formosa	ARG	Formosa	147636
111	Las Heras	ARG	Mendoza	145823
112	La Rioja	ARG	La Rioja	138117
113	San Fernando del Valle de Cata	ARG	Catamarca	134935
114	Rio Cuarto	ARG	Cordoba	134355
115	Comodoro Rivadavia	ARG	Chubut	124104
116	Mendoza	ARG	Mendoza	123027
117	San Nicolas de los Arroyos	ARG	Buenos Aires	119302
118	San Juan	ARG	San Juan	119152
119	Escobar	ARG	Buenos Aires	116675
120	Concordia	ARG	Entre Rios	116485
121	Pilar	ARG	Buenos Aires	113428
122	San Luis	ARG	San Luis	110136
123	Ezeiza	ARG	Buenos Aires	99578
124	San Rafael	ARG	Mendoza	94651
125	Tandil	ARG	Buenos Aires	91101
126	Yerevan	ARM	Yerevan	1248700
127	Gjumri	ARM	Girak	211700
128	Vanadzor	ARM	Lori	172700
129	Oranjestad	ABW	A	29034
130	Sydney	AUS	New South Wales	3276207
131	Melbourne	AUS	Victoria	2865329
132	Brisbane	AUS	Queensland	1291117
133	Perth	AUS	West Australia	1096829
134	Adelaide	AUS	South Australia	978100
135	Canberra	AUS	Capital Region	322723
136	Gold Coast	AUS	Queensland	311932
137	Newcastle	AUS	New South Wales	270324
138	Central Coast	AUS	New South Wales	227657
139	Wollongong	AUS	New South Wales	219761
140	Hobart	AUS	Tasmania	126118
141	Geelong	AUS	Victoria	125382
142	Townsville	AUS	Queensland	109914
143	Cairns	AUS	Queensland	92273
144	Baku	AZE	Baki	1787800
145	Ganca	AZE	Ganca	299300
146	Sumqayit	AZE	Sumqayit	283000
147	Mingacevir	AZE	Mingacevir	93900
148	Nassau	BHS	New Providence	172000
149	al-Manama	BHR	al-Manama	148000
150	Dhaka	BGD	Dhaka	3612850
151	Chittagong	BGD	Chittagong	1392860
152	Khulna	BGD	Khulna	663340
153	Rajshahi	BGD	Rajshahi	294056
154	Narayanganj	BGD	Dhaka	202134
155	Rangpur	BGD	Rajshahi	191398
156	Mymensingh	BGD	Dhaka	188713
157	Barisal	BGD	Barisal	170232
158	Tungi	BGD	Dhaka	168702
159	Jessore	BGD	Khulna	139710
160	Comilla	BGD	Chittagong	135313
161	Nawabganj	BGD	Rajshahi	130577
162	Dinajpur	BGD	Rajshahi	127815
163	Bogra	BGD	Rajshahi	120170
164	Sylhet	BGD	Sylhet	117396
165	Brahmanbaria	BGD	Chittagong	109032
166	Tangail	BGD	Dhaka	106004
167	Jamalpur	BGD	Dhaka	103556
168	Pabna	BGD	Rajshahi	103277
169	Naogaon	BGD	Rajshahi	101266
170	Sirajganj	BGD	Rajshahi	99669
171	Narsinghdi	BGD	Dhaka	98342
172	Saidpur	BGD	Rajshahi	96777
173	Gazipur	BGD	Dhaka	96717
174	Bridgetown	BRB	St Michael	6070
175	Antwerpen	BEL	Antwerpen	446525
176	Gent	BEL	East Flanderi	224180
177	Charleroi	BEL	Hainaut	200827
178	Liege	BEL	Liege	185639
179	Bruxelles [Brussel]	BEL	Bryssel	133859
180	Brugge	BEL	West Flanderi	116246
181	Schaerbeek	BEL	Bryssel	105692
182	Namur	BEL	Namur	105419
183	Mons	BEL	Hainaut	90935
184	Belize City	BLZ	Belize City	55810
185	Belmopan	BLZ	Cayo	7105
186	Cotonou	BEN	Atlantique	536827
187	Porto-Novo	BEN	Oueme	194000
188	Djougou	BEN	Atacora	134099
189	Parakou	BEN	Borgou	103577
190	Saint George	BMU	Saint Georges	1800
191	Hamilton	BMU	Hamilton	1200
192	Thimphu	BTN	Thimphu	22000
193	Santa Cruz de la Sierra	BOL	Santa Cruz	935361
194	La Paz	BOL	La Paz	758141
195	El Alto	BOL	La Paz	534466
196	Cochabamba	BOL	Cochabamba	482800
197	Oruro	BOL	Oruro	223553
198	Sucre	BOL	Chuquisaca	178426
199	Potosi	BOL	Potosi	140642
200	Tarija	BOL	Tarija	125255
201	Sarajevo	BIH	Federaatio	360000
202	Banja Luka	BIH	Republika Srpska	143079
203	Zenica	BIH	Federaatio	96027
204	Gaborone	BWA	Gaborone	213017
205	Francistown	BWA	Francistown	101805
206	Sao Paulo	BRA	Sao Paulo	9968485
207	Rio de Janeiro	BRA	Rio de Janeiro	5598953
208	Salvador	BRA	Bahia	2302832
209	Belo Horizonte	BRA	Minas Gerais	2139125
210	Fortaleza	BRA	Ceara	2097757
211	Brasilia	BRA	Distrito Federal	1969868
212	Curitiba	BRA	Parana	1584232
213	Recife	BRA	Pernambuco	1378087
214	Porto Alegre	BRA	Rio Grande do Sul	1314032
215	Manaus	BRA	Amazonas	1255049
216	Belem	BRA	Para	1186926
217	Guarulhos	BRA	Sao Paulo	1095874
218	Goiania	BRA	Goias	1056330
219	Campinas	BRA	Sao Paulo	950043
220	Sao Goncalo	BRA	Rio de Janeiro	869254
221	Nova Iguacu	BRA	Rio de Janeiro	862225
222	Sao Luis	BRA	Maranhao	837588
223	Maceio	BRA	Alagoas	786288
224	Duque de Caxias	BRA	Rio de Janeiro	746758
225	Sao Bernardo do Campo	BRA	Sao Paulo	723132
226	Teresina	BRA	Piaui	691942
227	Natal	BRA	Rio Grande do Norte	688955
228	Osasco	BRA	Sao Paulo	659604
229	Campo Grande	BRA	Mato Grosso do Sul	649593
230	Santo Andre	BRA	Sao Paulo	630073
231	Joao Pessoa	BRA	Paraiba	584029
232	Jaboatao dos Guararapes	BRA	Pernambuco	558680
233	Contagem	BRA	Minas Gerais	520801
234	Sao Jose dos Campos	BRA	Sao Paulo	515553
235	Uberlandia	BRA	Minas Gerais	487222
236	Feira de Santana	BRA	Bahia	479992
237	Ribeirao Preto	BRA	Sao Paulo	473276
238	Sorocaba	BRA	Sao Paulo	466823
239	Niteroi	BRA	Rio de Janeiro	459884
240	Cuiaba	BRA	Mato Grosso	453813
241	Juiz de Fora	BRA	Minas Gerais	450288
242	Aracaju	BRA	Sergipe	445555
243	Sao Joao de Meriti	BRA	Rio de Janeiro	440052
244	Londrina	BRA	Parana	432257
245	Joinville	BRA	Santa Catarina	428011
246	Belford Roxo	BRA	Rio de Janeiro	425194
247	Santos	BRA	Sao Paulo	408748
248	Ananindeua	BRA	Para	400940
249	Campos dos Goytacazes	BRA	Rio de Janeiro	398418
250	Maua	BRA	Sao Paulo	375055
251	Carapicuiba	BRA	Sao Paulo	357552
252	Olinda	BRA	Pernambuco	354732
253	Campina Grande	BRA	Paraiba	352497
254	Sao Jose do Rio Preto	BRA	Sao Paulo	351944
255	Caxias do Sul	BRA	Rio Grande do Sul	349581
256	Moji das Cruzes	BRA	Sao Paulo	339194
257	Diadema	BRA	Sao Paulo	335078
258	Aparecida de Goiania	BRA	Goias	324662
259	Piracicaba	BRA	Sao Paulo	319104
260	Cariacica	BRA	Espirito Santo	319033
261	Vila Velha	BRA	Espirito Santo	318758
262	Pelotas	BRA	Rio Grande do Sul	315415
263	Bauru	BRA	Sao Paulo	313670
264	Porto Velho	BRA	Rondonia	309750
265	Serra	BRA	Espirito Santo	302666
266	Betim	BRA	Minas Gerais	302108
267	Jundiai	BRA	Sao Paulo	296127
268	Canoas	BRA	Rio Grande do Sul	294125
269	Franca	BRA	Sao Paulo	290139
270	Sao Vicente	BRA	Sao Paulo	286848
271	Maringa	BRA	Parana	286461
272	Montes Claros	BRA	Minas Gerais	286058
273	Anapolis	BRA	Goias	282197
274	Florianopolis	BRA	Santa Catarina	281928
275	Petropolis	BRA	Rio de Janeiro	279183
276	Itaquaquecetuba	BRA	Sao Paulo	270874
277	Vitoria	BRA	Espirito Santo	270626
278	Ponta Grossa	BRA	Parana	268013
279	Rio Branco	BRA	Acre	259537
280	Foz do Iguacu	BRA	Parana	259425
281	Macapa	BRA	Amapa	256033
282	Ilheus	BRA	Bahia	254970
283	Vitoria da Conquista	BRA	Bahia	253587
284	Uberaba	BRA	Minas Gerais	249225
285	Paulista	BRA	Pernambuco	248473
286	Limeira	BRA	Sao Paulo	245497
287	Blumenau	BRA	Santa Catarina	244379
288	Caruaru	BRA	Pernambuco	244247
289	Santarem	BRA	Para	241771
290	Volta Redonda	BRA	Rio de Janeiro	240315
291	Novo Hamburgo	BRA	Rio Grande do Sul	239940
292	Caucaia	BRA	Ceara	238738
293	Santa Maria	BRA	Rio Grande do Sul	238473
294	Cascavel	BRA	Parana	237510
295	Guaruja	BRA	Sao Paulo	237206
296	Ribeirao das Neves	BRA	Minas Gerais	232685
297	Governador Valadares	BRA	Minas Gerais	231724
298	Taubate	BRA	Sao Paulo	229130
299	Imperatriz	BRA	Maranhao	224564
300	Gravatai	BRA	Rio Grande do Sul	223011
301	Embu	BRA	Sao Paulo	222223
302	Mossoro	BRA	Rio Grande do Norte	214901
303	Varzea Grande	BRA	Mato Grosso	214435
304	Petrolina	BRA	Pernambuco	210540
305	Barueri	BRA	Sao Paulo	208426
306	Viamao	BRA	Rio Grande do Sul	207557
307	Ipatinga	BRA	Minas Gerais	206338
308	Juazeiro	BRA	Bahia	201073
309	Juazeiro do Norte	BRA	Ceara	199636
310	Taboao da Serra	BRA	Sao Paulo	197550
311	Sao Jose dos Pinhais	BRA	Parana	196884
312	Mage	BRA	Rio de Janeiro	196147
313	Suzano	BRA	Sao Paulo	195434
314	Sao Leopoldo	BRA	Rio Grande do Sul	189258
315	Marilia	BRA	Sao Paulo	188691
316	Sao Carlos	BRA	Sao Paulo	187122
317	Sumare	BRA	Sao Paulo	186205
318	Presidente Prudente	BRA	Sao Paulo	185340
319	Divinopolis	BRA	Minas Gerais	185047
320	Sete Lagoas	BRA	Minas Gerais	182984
321	Rio Grande	BRA	Rio Grande do Sul	182222
322	Itabuna	BRA	Bahia	182148
323	Jequie	BRA	Bahia	179128
324	Arapiraca	BRA	Alagoas	178988
325	Colombo	BRA	Parana	177764
326	Americana	BRA	Sao Paulo	177409
327	Alvorada	BRA	Rio Grande do Sul	175574
328	Araraquara	BRA	Sao Paulo	174381
329	Itaborai	BRA	Rio de Janeiro	173977
330	Santa Barbara dOeste	BRA	Sao Paulo	171657
331	Nova Friburgo	BRA	Rio de Janeiro	170697
332	Jacarei	BRA	Sao Paulo	170356
333	Aracatuba	BRA	Sao Paulo	169303
334	Barra Mansa	BRA	Rio de Janeiro	168953
335	Praia Grande	BRA	Sao Paulo	168434
336	Maraba	BRA	Para	167795
337	Criciuma	BRA	Santa Catarina	167661
338	Boa Vista	BRA	Roraima	167185
339	Passo Fundo	BRA	Rio Grande do Sul	166343
340	Dourados	BRA	Mato Grosso do Sul	164716
341	Santa Luzia	BRA	Minas Gerais	164704
342	Rio Claro	BRA	Sao Paulo	163551
343	Maracanau	BRA	Ceara	162022
344	Guarapuava	BRA	Parana	160510
345	Rondonopolis	BRA	Mato Grosso	155115
346	Sao Jose	BRA	Santa Catarina	155105
347	Cachoeiro de Itapemirim	BRA	Espirito Santo	155024
348	Nilopolis	BRA	Rio de Janeiro	153383
349	Itapevi	BRA	Sao Paulo	150664
350	Cabo de Santo Agostinho	BRA	Pernambuco	149964
351	Camacari	BRA	Bahia	149146
352	Sobral	BRA	Ceara	146005
353	Itajai	BRA	Santa Catarina	145197
354	Chapeco	BRA	Santa Catarina	144158
355	Cotia	BRA	Sao Paulo	140042
356	Lages	BRA	Santa Catarina	139570
357	Ferraz de Vasconcelos	BRA	Sao Paulo	139283
358	Indaiatuba	BRA	Sao Paulo	135968
359	Hortolandia	BRA	Sao Paulo	135755
360	Caxias	BRA	Maranhao	133980
361	Sao Caetano do Sul	BRA	Sao Paulo	133321
362	Itu	BRA	Sao Paulo	132736
363	Nossa Senhora do Socorro	BRA	Sergipe	131351
364	Parnaiba	BRA	Piaui	129756
365	Pocos de Caldas	BRA	Minas Gerais	129683
366	Teresopolis	BRA	Rio de Janeiro	128079
367	Barreiras	BRA	Bahia	127801
368	Castanhal	BRA	Para	127634
369	Alagoinhas	BRA	Bahia	126820
370	Itapecerica da Serra	BRA	Sao Paulo	126672
371	Uruguaiana	BRA	Rio Grande do Sul	126305
372	Paranagua	BRA	Parana	126076
373	Ibirite	BRA	Minas Gerais	125982
374	Timon	BRA	Maranhao	125812
375	Luziania	BRA	Goias	125597
376	Macae	BRA	Rio de Janeiro	125597
377	Teofilo Otoni	BRA	Minas Gerais	124489
378	Moji-Guacu	BRA	Sao Paulo	123782
379	Palmas	BRA	Tocantins	121919
380	Pindamonhangaba	BRA	Sao Paulo	121904
381	Francisco Morato	BRA	Sao Paulo	121197
382	Bage	BRA	Rio Grande do Sul	120793
383	Sapucaia do Sul	BRA	Rio Grande do Sul	120217
384	Cabo Frio	BRA	Rio de Janeiro	119503
385	Itapetininga	BRA	Sao Paulo	119391
386	Patos de Minas	BRA	Minas Gerais	119262
387	Camaragibe	BRA	Pernambuco	118968
388	Braganca Paulista	BRA	Sao Paulo	116929
389	Queimados	BRA	Rio de Janeiro	115020
390	Araguaina	BRA	Tocantins	114948
391	Garanhuns	BRA	Pernambuco	114603
392	Vitoria de Santo Antao	BRA	Pernambuco	113595
393	Santa Rita	BRA	Paraiba	113135
394	Barbacena	BRA	Minas Gerais	113079
395	Abaetetuba	BRA	Para	111258
396	Jau	BRA	Sao Paulo	109965
397	Lauro de Freitas	BRA	Bahia	109236
398	Franco da Rocha	BRA	Sao Paulo	108964
399	Teixeira de Freitas	BRA	Bahia	108441
400	Varginha	BRA	Minas Gerais	108314
401	Ribeirao Pires	BRA	Sao Paulo	108121
402	Sabara	BRA	Minas Gerais	107781
403	Catanduva	BRA	Sao Paulo	107761
404	Rio Verde	BRA	Goias	107755
405	Botucatu	BRA	Sao Paulo	107663
406	Colatina	BRA	Espirito Santo	107354
407	Santa Cruz do Sul	BRA	Rio Grande do Sul	106734
408	Linhares	BRA	Espirito Santo	106278
409	Apucarana	BRA	Parana	105114
410	Barretos	BRA	Sao Paulo	104156
411	Guaratingueta	BRA	Sao Paulo	103433
412	Cachoeirinha	BRA	Rio Grande do Sul	103240
413	Codo	BRA	Maranhao	103153
414	Jaragua do Sul	BRA	Santa Catarina	102580
415	Cubatao	BRA	Sao Paulo	102372
416	Itabira	BRA	Minas Gerais	102217
417	Itaituba	BRA	Para	101320
418	Araras	BRA	Sao Paulo	101046
419	Resende	BRA	Rio de Janeiro	100627
420	Atibaia	BRA	Sao Paulo	100356
421	Pouso Alegre	BRA	Minas Gerais	100028
422	Toledo	BRA	Parana	99387
423	Crato	BRA	Ceara	98965
424	Passos	BRA	Minas Gerais	98570
425	Araguari	BRA	Minas Gerais	98399
426	Sao Jose de Ribamar	BRA	Maranhao	98318
427	Pinhais	BRA	Parana	98198
428	Sertaozinho	BRA	Sao Paulo	98140
429	Conselheiro Lafaiete	BRA	Minas Gerais	97507
430	Paulo Afonso	BRA	Bahia	97291
431	Angra dos Reis	BRA	Rio de Janeiro	96864
432	Eunapolis	BRA	Bahia	96610
433	Salto	BRA	Sao Paulo	96348
434	Ourinhos	BRA	Sao Paulo	96291
435	Parnamirim	BRA	Rio Grande do Norte	96210
436	Jacobina	BRA	Bahia	96131
437	Coronel Fabriciano	BRA	Minas Gerais	95933
438	Birigui	BRA	Sao Paulo	94685
439	Tatui	BRA	Sao Paulo	93897
440	Ji-Parana	BRA	Rondonia	93346
441	Bacabal	BRA	Maranhao	93121
442	Cameta	BRA	Para	92779
443	Guaiba	BRA	Rio Grande do Sul	92224
444	Sao Lourenco da Mata	BRA	Pernambuco	91999
445	Santana do Livramento	BRA	Rio Grande do Sul	91779
446	Votorantim	BRA	Sao Paulo	91777
447	Campo Largo	BRA	Parana	91203
448	Patos	BRA	Paraiba	90519
449	Ituiutaba	BRA	Minas Gerais	90507
450	Corumba	BRA	Mato Grosso do Sul	90111
451	Palhoca	BRA	Santa Catarina	89465
452	Barra do Pirai	BRA	Rio de Janeiro	89388
453	Bento Goncalves	BRA	Rio Grande do Sul	89254
454	Poa	BRA	Sao Paulo	89236
455	Aguas Lindas de Goias	BRA	Goias	89200
456	London	GBR	England	7285000
457	Birmingham	GBR	England	1013000
458	Glasgow	GBR	Scotland	619680
459	Liverpool	GBR	England	461000
460	Edinburgh	GBR	Scotland	450180
461	Sheffield	GBR	England	431607
462	Manchester	GBR	England	430000
463	Leeds	GBR	England	424194
464	Bristol	GBR	England	402000
465	Cardiff	GBR	Wales	321000
466	Coventry	GBR	England	304000
467	Leicester	GBR	England	294000
468	Bradford	GBR	England	289376
469	Belfast	GBR	North Ireland	287500
470	Nottingham	GBR	England	287000
471	Kingston upon Hull	GBR	England	262000
472	Plymouth	GBR	England	253000
473	Stoke-on-Trent	GBR	England	252000
474	Wolverhampton	GBR	England	242000
475	Derby	GBR	England	236000
476	Swansea	GBR	Wales	230000
477	Southampton	GBR	England	216000
478	Aberdeen	GBR	Scotland	213070
479	Northampton	GBR	England	196000
480	Dudley	GBR	England	192171
481	Portsmouth	GBR	England	190000
482	Newcastle upon Tyne	GBR	England	189150
483	Sunderland	GBR	England	183310
484	Luton	GBR	England	183000
485	Swindon	GBR	England	180000
486	Southend-on-Sea	GBR	England	176000
487	Walsall	GBR	England	174739
488	Bournemouth	GBR	England	162000
489	Peterborough	GBR	England	156000
490	Brighton	GBR	England	156124
491	Blackpool	GBR	England	151000
492	Dundee	GBR	Scotland	146690
493	West Bromwich	GBR	England	146386
494	Reading	GBR	England	148000
495	Oldbury/Smethwick (Warley)	GBR	England	145542
496	Middlesbrough	GBR	England	145000
497	Huddersfield	GBR	England	143726
498	Oxford	GBR	England	144000
499	Poole	GBR	England	141000
500	Bolton	GBR	England	139020
501	Blackburn	GBR	England	140000
502	Newport	GBR	Wales	139000
503	Preston	GBR	England	135000
504	Stockport	GBR	England	132813
505	Norwich	GBR	England	124000
506	Rotherham	GBR	England	121380
507	Cambridge	GBR	England	121000
508	Watford	GBR	England	113080
509	Ipswich	GBR	England	114000
510	Slough	GBR	England	112000
511	Exeter	GBR	England	111000
512	Cheltenham	GBR	England	106000
513	Gloucester	GBR	England	107000
514	Saint Helens	GBR	England	106293
515	Sutton Coldfield	GBR	England	106001
516	York	GBR	England	104425
517	Oldham	GBR	England	103931
518	Basildon	GBR	England	100924
519	Worthing	GBR	England	100000
520	Chelmsford	GBR	England	97451
521	Colchester	GBR	England	96063
522	Crawley	GBR	England	97000
523	Gillingham	GBR	England	92000
524	Solihull	GBR	England	94531
525	Rochdale	GBR	England	94313
526	Birkenhead	GBR	England	93087
527	Worcester	GBR	England	95000
528	Hartlepool	GBR	England	92000
529	Halifax	GBR	England	91069
530	Woking/Byfleet	GBR	England	92000
531	Southport	GBR	England	90959
532	Maidstone	GBR	England	90878
533	Eastbourne	GBR	England	90000
534	Grimsby	GBR	England	89000
535	Saint Helier	GBR	Jersey	27523
536	Douglas	GBR	A	23487
537	Road Town	VGB	Tortola	8000
538	Bandar Seri Begawan	BRN	Brunei and Muara	21484
539	Sofija	BGR	Grad Sofija	1122302
540	Plovdiv	BGR	Plovdiv	342584
541	Varna	BGR	Varna	299801
542	Burgas	BGR	Burgas	195255
543	Ruse	BGR	Ruse	166467
544	Stara Zagora	BGR	Haskovo	147939
545	Pleven	BGR	Lovec	121952
546	Sliven	BGR	Burgas	105530
547	Dobric	BGR	Varna	100399
548	Gumen	BGR	Varna	94686
549	Ouagadougou	BFA	Kadiogo	824000
550	Bobo-Dioulasso	BFA	Houet	300000
551	Koudougou	BFA	Boulkiemde	105000
552	Bujumbura	BDI	Bujumbura	300000
553	George Town	CYM	Grand Cayman	19600
554	Santiago de Chile	CHL	Santiago	4703954
555	Puente Alto	CHL	Santiago	386236
556	Viaa del Mar	CHL	Valparaiso	312493
557	Valparaiso	CHL	Valparaiso	293800
558	Talcahuano	CHL	Biobio	277752
559	Antofagasta	CHL	Antofagasta	251429
560	San Bernardo	CHL	Santiago	241910
561	Temuco	CHL	La Araucania	233041
562	Concepcion	CHL	Biobio	217664
563	Rancagua	CHL	OHiggins	212977
564	Arica	CHL	Tarapaca	189036
565	Talca	CHL	Maule	187557
566	Chillan	CHL	Biobio	178182
567	Iquique	CHL	Tarapaca	177892
568	Los Angeles	CHL	Biobio	158215
569	Puerto Montt	CHL	Los Lagos	152194
570	Coquimbo	CHL	Coquimbo	143353
571	Osorno	CHL	Los Lagos	141468
572	La Serena	CHL	Coquimbo	137409
573	Calama	CHL	Antofagasta	137265
574	Valdivia	CHL	Los Lagos	133106
575	Punta Arenas	CHL	Magallanes	125631
576	Copiapo	CHL	Atacama	120128
577	Quilpue	CHL	Valparaiso	118857
578	Curico	CHL	Maule	115766
579	Ovalle	CHL	Coquimbo	94854
580	Coronel	CHL	Biobio	93061
581	San Pedro de la Paz	CHL	Biobio	91684
582	Melipilla	CHL	Santiago	91056
583	Avarua	COK	Rarotonga	11900
584	San Jose	CRI	San Jose	339131
585	Djibouti	DJI	Djibouti	383000
586	Roseau	DMA	St George	16243
587	Santo Domingo de Guzman	DOM	Distrito Nacional	1609966
588	Santiago de los Caballeros	DOM	Santiago	365463
589	La Romana	DOM	La Romana	140204
590	San Pedro de Macoris	DOM	San Pedro de Macoris	124735
591	San Francisco de Macoris	DOM	Duarte	108485
592	San Felipe de Puerto Plata	DOM	Puerto Plata	89423
593	Guayaquil	ECU	Guayas	2070040
594	Quito	ECU	Pichincha	1573458
595	Cuenca	ECU	Azuay	270353
596	Machala	ECU	El Oro	210368
597	Santo Domingo de los Colorados	ECU	Pichincha	202111
598	Portoviejo	ECU	Manabi	176413
599	Ambato	ECU	Tungurahua	169612
600	Manta	ECU	Manabi	164739
601	Duran [Eloy Alfaro]	ECU	Guayas	152514
602	Ibarra	ECU	Imbabura	130643
603	Quevedo	ECU	Los Rios	129631
604	Milagro	ECU	Guayas	124177
605	Loja	ECU	Loja	123875
606	Riobamba	ECU	Chimborazo	123163
607	Esmeraldas	ECU	Esmeraldas	123045
608	Cairo	EGY	Kairo	6789479
609	Alexandria	EGY	Aleksandria	3328196
610	Giza	EGY	Giza	2221868
611	Shubra al-Khayma	EGY	al-Qalyubiya	870716
612	Port Said	EGY	Port Said	469533
613	Suez	EGY	Suez	417610
614	al-Mahallat al-Kubra	EGY	al-Gharbiya	395402
615	Tanta	EGY	al-Gharbiya	371010
616	al-Mansura	EGY	al-Daqahliya	369621
617	Luxor	EGY	Luxor	360503
618	Asyut	EGY	Asyut	343498
619	Bahtim	EGY	al-Qalyubiya	275807
620	Zagazig	EGY	al-Sharqiya	267351
621	al-Faiyum	EGY	al-Faiyum	260964
622	Ismailia	EGY	Ismailia	254477
623	Kafr al-Dawwar	EGY	al-Buhayra	231978
624	Assuan	EGY	Assuan	219017
625	Damanhur	EGY	al-Buhayra	212203
626	al-Minya	EGY	al-Minya	201360
627	Bani Suwayf	EGY	Bani Suwayf	172032
628	Qina	EGY	Qina	171275
629	Sawhaj	EGY	Sawhaj	170125
630	Shibin al-Kawm	EGY	al-Minufiya	159909
631	Bulaq al-Dakrur	EGY	Giza	148787
632	Banha	EGY	al-Qalyubiya	145792
633	Warraq al-Arab	EGY	Giza	127108
634	Kafr al-Shaykh	EGY	Kafr al-Shaykh	124819
635	Mallawi	EGY	al-Minya	119283
636	Bilbays	EGY	al-Sharqiya	113608
637	Mit Ghamr	EGY	al-Daqahliya	101801
638	al-Arish	EGY	Shamal Sina	100447
639	Talkha	EGY	al-Daqahliya	97700
640	Qalyub	EGY	al-Qalyubiya	97200
641	Jirja	EGY	Sawhaj	95400
642	Idfu	EGY	Qina	94200
643	al-Hawamidiya	EGY	Giza	91700
644	Disuq	EGY	Kafr al-Shaykh	91300
645	San Salvador	SLV	San Salvador	415346
646	Santa Ana	SLV	Santa Ana	139389
647	Mejicanos	SLV	San Salvador	138800
648	Soyapango	SLV	San Salvador	129800
649	San Miguel	SLV	San Miguel	127696
650	Nueva San Salvador	SLV	La Libertad	98400
651	Apopa	SLV	San Salvador	88800
652	Asmara	ERI	Maekel	431000
653	Madrid	ESP	Madrid	2879052
654	Barcelona	ESP	Katalonia	1503451
655	Valencia	ESP	Valencia	739412
656	Sevilla	ESP	Andalusia	701927
657	Zaragoza	ESP	Aragonia	603367
658	Malaga	ESP	Andalusia	530553
659	Bilbao	ESP	Baskimaa	357589
660	Las Palmas de Gran Canaria	ESP	Canary Islands	354757
661	Murcia	ESP	Murcia	353504
662	Palma de Mallorca	ESP	Balears	326993
663	Valladolid	ESP	Castilla and Leon	319998
664	Cordoba	ESP	Andalusia	311708
665	Vigo	ESP	Galicia	283670
666	Alicante [Alacant]	ESP	Valencia	272432
667	Gijon	ESP	Asturia	267980
668	LHospitalet de Llobregat	ESP	Katalonia	247986
669	Granada	ESP	Andalusia	244767
670	A Coruaa (La Coruaa)	ESP	Galicia	243402
671	Vitoria-Gasteiz	ESP	Baskimaa	217154
672	Santa Cruz de Tenerife	ESP	Canary Islands	213050
673	Badalona	ESP	Katalonia	209635
674	Oviedo	ESP	Asturia	200453
675	Mostoles	ESP	Madrid	195351
676	Elche [Elx]	ESP	Valencia	193174
677	Sabadell	ESP	Katalonia	184859
678	Santander	ESP	Cantabria	184165
679	Jerez de la Frontera	ESP	Andalusia	182660
680	Pamplona [Iruaa]	ESP	Navarra	180483
681	Donostia-San Sebastian	ESP	Baskimaa	179208
682	Cartagena	ESP	Murcia	177709
683	Leganes	ESP	Madrid	173163
684	Fuenlabrada	ESP	Madrid	171173
685	Almeria	ESP	Andalusia	169027
686	Terrassa	ESP	Katalonia	168695
687	Alcala de Henares	ESP	Madrid	164463
688	Burgos	ESP	Castilla and Leon	162802
689	Salamanca	ESP	Castilla and Leon	158720
690	Albacete	ESP	Kastilia-La Mancha	147527
691	Getafe	ESP	Madrid	145371
692	Cadiz	ESP	Andalusia	142449
693	Alcorcon	ESP	Madrid	142048
694	Huelva	ESP	Andalusia	140583
695	Leon	ESP	Castilla and Leon	139809
696	Castellon de la Plana [Castell	ESP	Valencia	139712
697	Badajoz	ESP	Extremadura	136613
698	[San Cristobal de] la Laguna	ESP	Canary Islands	127945
699	Logroao	ESP	La Rioja	127093
700	Santa Coloma de Gramenet	ESP	Katalonia	120802
701	Tarragona	ESP	Katalonia	113016
702	Lleida (Lerida)	ESP	Katalonia	112207
703	Jaen	ESP	Andalusia	109247
704	Ourense (Orense)	ESP	Galicia	109120
705	Mataro	ESP	Katalonia	104095
706	Algeciras	ESP	Andalusia	103106
707	Marbella	ESP	Andalusia	101144
708	Barakaldo	ESP	Baskimaa	98212
709	Dos Hermanas	ESP	Andalusia	94591
710	Santiago de Compostela	ESP	Galicia	93745
711	Torrejon de Ardoz	ESP	Madrid	92262
712	Cape Town	ZAF	Western Cape	2352121
713	Soweto	ZAF	Gauteng	904165
714	Johannesburg	ZAF	Gauteng	756653
715	Port Elizabeth	ZAF	Eastern Cape	752319
716	Pretoria	ZAF	Gauteng	658630
717	Inanda	ZAF	KwaZulu-Natal	634065
718	Durban	ZAF	KwaZulu-Natal	566120
719	Vanderbijlpark	ZAF	Gauteng	468931
720	Kempton Park	ZAF	Gauteng	442633
721	Alberton	ZAF	Gauteng	410102
722	Pinetown	ZAF	KwaZulu-Natal	378810
723	Pietermaritzburg	ZAF	KwaZulu-Natal	370190
724	Benoni	ZAF	Gauteng	365467
725	Randburg	ZAF	Gauteng	341288
726	Umlazi	ZAF	KwaZulu-Natal	339233
727	Bloemfontein	ZAF	Free State	334341
728	Vereeniging	ZAF	Gauteng	328535
729	Wonderboom	ZAF	Gauteng	283289
730	Roodepoort	ZAF	Gauteng	279340
731	Boksburg	ZAF	Gauteng	262648
732	Klerksdorp	ZAF	North West	261911
733	Soshanguve	ZAF	Gauteng	242727
734	Newcastle	ZAF	KwaZulu-Natal	222993
735	East London	ZAF	Eastern Cape	221047
736	Welkom	ZAF	Free State	203296
737	Kimberley	ZAF	Northern Cape	197254
738	Uitenhage	ZAF	Eastern Cape	192120
739	Chatsworth	ZAF	KwaZulu-Natal	189885
740	Mdantsane	ZAF	Eastern Cape	182639
741	Krugersdorp	ZAF	Gauteng	181503
742	Botshabelo	ZAF	Free State	177971
743	Brakpan	ZAF	Gauteng	171363
744	Witbank	ZAF	Mpumalanga	167183
745	Oberholzer	ZAF	Gauteng	164367
746	Germiston	ZAF	Gauteng	164252
747	Springs	ZAF	Gauteng	162072
748	Westonaria	ZAF	Gauteng	159632
749	Randfontein	ZAF	Gauteng	120838
750	Paarl	ZAF	Western Cape	105768
751	Potchefstroom	ZAF	North West	101817
752	Rustenburg	ZAF	North West	97008
753	Nigel	ZAF	Gauteng	96734
754	George	ZAF	Western Cape	93818
755	Ladysmith	ZAF	KwaZulu-Natal	89292
756	Addis Abeba	ETH	Addis Abeba	2495000
757	Dire Dawa	ETH	Dire Dawa	164851
758	Nazret	ETH	Oromia	127842
759	Gonder	ETH	Amhara	112249
760	Dese	ETH	Amhara	97314
761	Mekele	ETH	Tigray	96938
762	Bahir Dar	ETH	Amhara	96140
763	Stanley	FLK	East Falkland	1636
764	Suva	FJI	Central	77366
765	Quezon	PHL	National Capital Reg	2173831
766	Manila	PHL	National Capital Reg	1581082
767	Kalookan	PHL	National Capital Reg	1177604
768	Davao	PHL	Southern Mindanao	1147116
769	Cebu	PHL	Central Visayas	718821
770	Zamboanga	PHL	Western Mindanao	601794
771	Pasig	PHL	National Capital Reg	505058
772	Valenzuela	PHL	National Capital Reg	485433
773	Las Piaas	PHL	National Capital Reg	472780
774	Antipolo	PHL	Southern Tagalog	470866
775	Taguig	PHL	National Capital Reg	467375
776	Cagayan de Oro	PHL	Northern Mindanao	461877
777	Paraaaque	PHL	National Capital Reg	449811
778	Makati	PHL	National Capital Reg	444867
779	Bacolod	PHL	Western Visayas	429076
780	General Santos	PHL	Southern Mindanao	411822
781	Marikina	PHL	National Capital Reg	391170
782	Dasmariaas	PHL	Southern Tagalog	379520
783	Muntinlupa	PHL	National Capital Reg	379310
784	Iloilo	PHL	Western Visayas	365820
785	Pasay	PHL	National Capital Reg	354908
786	Malabon	PHL	National Capital Reg	338855
787	San Jose del Monte	PHL	Central Luzon	315807
788	Bacoor	PHL	Southern Tagalog	305699
789	Iligan	PHL	Central Mindanao	285061
790	Calamba	PHL	Southern Tagalog	281146
791	Mandaluyong	PHL	National Capital Reg	278474
792	Butuan	PHL	Caraga	267279
793	Angeles	PHL	Central Luzon	263971
794	Tarlac	PHL	Central Luzon	262481
795	Mandaue	PHL	Central Visayas	259728
796	Baguio	PHL	CAR	252386
797	Batangas	PHL	Southern Tagalog	247588
798	Cainta	PHL	Southern Tagalog	242511
799	San Pedro	PHL	Southern Tagalog	231403
800	Navotas	PHL	National Capital Reg	230403
801	Cabanatuan	PHL	Central Luzon	222859
802	San Fernando	PHL	Central Luzon	221857
803	Lipa	PHL	Southern Tagalog	218447
804	Lapu-Lapu	PHL	Central Visayas	217019
805	San Pablo	PHL	Southern Tagalog	207927
806	Biaan	PHL	Southern Tagalog	201186
807	Taytay	PHL	Southern Tagalog	198183
808	Lucena	PHL	Southern Tagalog	196075
809	Imus	PHL	Southern Tagalog	195482
810	Olongapo	PHL	Central Luzon	194260
811	Binangonan	PHL	Southern Tagalog	187691
812	Santa Rosa	PHL	Southern Tagalog	185633
813	Tagum	PHL	Southern Mindanao	179531
814	Tacloban	PHL	Eastern Visayas	178639
815	Malolos	PHL	Central Luzon	175291
816	Mabalacat	PHL	Central Luzon	171045
817	Cotabato	PHL	Central Mindanao	163849
818	Meycauayan	PHL	Central Luzon	163037
819	Puerto Princesa	PHL	Southern Tagalog	161912
820	Legazpi	PHL	Bicol	157010
821	Silang	PHL	Southern Tagalog	156137
822	Ormoc	PHL	Eastern Visayas	154297
823	San Carlos	PHL	Ilocos	154264
824	Kabankalan	PHL	Western Visayas	149769
825	Talisay	PHL	Central Visayas	148110
826	Valencia	PHL	Northern Mindanao	147924
827	Calbayog	PHL	Eastern Visayas	147187
828	Santa Maria	PHL	Central Luzon	144282
829	Pagadian	PHL	Western Mindanao	142515
830	Cadiz	PHL	Western Visayas	141954
831	Bago	PHL	Western Visayas	141721
832	Toledo	PHL	Central Visayas	141174
833	Naga	PHL	Bicol	137810
834	San Mateo	PHL	Southern Tagalog	135603
835	Panabo	PHL	Southern Mindanao	133950
836	Koronadal	PHL	Southern Mindanao	133786
837	Marawi	PHL	Central Mindanao	131090
838	Dagupan	PHL	Ilocos	130328
839	Sagay	PHL	Western Visayas	129765
840	Roxas	PHL	Western Visayas	126352
841	Lubao	PHL	Central Luzon	125699
842	Digos	PHL	Southern Mindanao	125171
843	San Miguel	PHL	Central Luzon	123824
844	Malaybalay	PHL	Northern Mindanao	123672
845	Tuguegarao	PHL	Cagayan Valley	120645
846	Ilagan	PHL	Cagayan Valley	119990
847	Baliuag	PHL	Central Luzon	119675
848	Surigao	PHL	Caraga	118534
849	San Carlos	PHL	Western Visayas	118259
850	San Juan del Monte	PHL	National Capital Reg	117680
851	Tanauan	PHL	Southern Tagalog	117539
852	Concepcion	PHL	Central Luzon	115171
853	Rodriguez (Montalban)	PHL	Southern Tagalog	115167
854	Sariaya	PHL	Southern Tagalog	114568
855	Malasiqui	PHL	Ilocos	113190
856	General Mariano Alvarez	PHL	Southern Tagalog	112446
857	Urdaneta	PHL	Ilocos	111582
858	Hagonoy	PHL	Central Luzon	111425
859	San Jose	PHL	Southern Tagalog	111009
860	Polomolok	PHL	Southern Mindanao	110709
861	Santiago	PHL	Cagayan Valley	110531
862	Tanza	PHL	Southern Tagalog	110517
863	Ozamis	PHL	Northern Mindanao	110420
864	Mexico	PHL	Central Luzon	109481
865	San Jose	PHL	Central Luzon	108254
866	Silay	PHL	Western Visayas	107722
867	General Trias	PHL	Southern Tagalog	107691
868	Tabaco	PHL	Bicol	107166
869	Cabuyao	PHL	Southern Tagalog	106630
870	Calapan	PHL	Southern Tagalog	105910
871	Mati	PHL	Southern Mindanao	105908
872	Midsayap	PHL	Central Mindanao	105760
873	Cauayan	PHL	Cagayan Valley	103952
874	Gingoog	PHL	Northern Mindanao	102379
875	Dumaguete	PHL	Central Visayas	102265
876	San Fernando	PHL	Ilocos	102082
877	Arayat	PHL	Central Luzon	101792
878	Bayawan (Tulong)	PHL	Central Visayas	101391
879	Kidapawan	PHL	Central Mindanao	101205
880	Daraga (Locsin)	PHL	Bicol	101031
881	Marilao	PHL	Central Luzon	101017
882	Malita	PHL	Southern Mindanao	100000
883	Dipolog	PHL	Western Mindanao	99862
884	Cavite	PHL	Southern Tagalog	99367
885	Danao	PHL	Central Visayas	98781
886	Bislig	PHL	Caraga	97860
887	Talavera	PHL	Central Luzon	97329
888	Guagua	PHL	Central Luzon	96858
889	Bayambang	PHL	Ilocos	96609
890	Nasugbu	PHL	Southern Tagalog	96113
891	Baybay	PHL	Eastern Visayas	95630
892	Capas	PHL	Central Luzon	95219
893	Sultan Kudarat	PHL	ARMM	94861
894	Laoag	PHL	Ilocos	94466
895	Bayugan	PHL	Caraga	93623
896	Malungon	PHL	Southern Mindanao	93232
897	Santa Cruz	PHL	Southern Tagalog	92694
898	Sorsogon	PHL	Bicol	92512
899	Candelaria	PHL	Southern Tagalog	92429
900	Ligao	PHL	Bicol	90603
901	Torshavn	FRO	Streymoyar	14542
902	Libreville	GAB	Estuaire	419000
903	Serekunda	GMB	Kombo St Mary	102600
904	Banjul	GMB	Banjul	42326
905	Tbilisi	GEO	Tbilisi	1235200
906	Kutaisi	GEO	Imereti	240900
907	Rustavi	GEO	Kvemo Kartli	155400
908	Batumi	GEO	Adzaria [Atdara]	137700
909	Sohumi	GEO	Abhasia [Aphazeti]	111700
910	Accra	GHA	Greater Accra	1070000
911	Kumasi	GHA	Ashanti	385192
912	Tamale	GHA	Northern	151069
913	Tema	GHA	Greater Accra	109975
914	Sekondi-Takoradi	GHA	Western	103653
915	Gibraltar	GIB	A	27025
916	Saint Georges	GRD	St George	4621
917	Nuuk	GRL	Kitaa	13445
918	Les Abymes	GLP	Grande-Terre	62947
919	Basse-Terre	GLP	Basse-Terre	12433
920	Tamuning	GUM	A	9500
921	Agaaa	GUM	A	1139
922	Ciudad de Guatemala	GTM	Guatemala	823301
923	Mixco	GTM	Guatemala	209791
924	Villa Nueva	GTM	Guatemala	101295
925	Quetzaltenango	GTM	Quetzaltenango	90801
926	Conakry	GIN	Conakry	1090610
927	Bissau	GNB	Bissau	241000
928	Georgetown	GUY	Georgetown	254000
929	Port-au-Prince	HTI	Ouest	884472
930	Carrefour	HTI	Ouest	290204
931	Delmas	HTI	Ouest	240429
932	Le-Cap-Haitien	HTI	Nord	102233
933	Tegucigalpa	HND	Distrito Central	813900
934	San Pedro Sula	HND	Cortes	383900
935	La Ceiba	HND	Atlantida	89200
936	Kowloon and New Kowloon	HKG	Kowloon and New Kowl	1987996
937	Victoria	HKG	Hongkong	1312637
938	Longyearbyen	SJM	Lansimaa	1438
939	Jakarta	IDN	Jakarta Raya	9604900
940	Surabaya	IDN	East Java	2663820
941	Bandung	IDN	West Java	2429000
942	Medan	IDN	Sumatera Utara	1843919
943	Palembang	IDN	Sumatera Selatan	1222764
944	Tangerang	IDN	West Java	1198300
945	Semarang	IDN	Central Java	1104405
946	Ujung Pandang	IDN	Sulawesi Selatan	1060257
947	Malang	IDN	East Java	716862
948	Bandar Lampung	IDN	Lampung	680332
949	Bekasi	IDN	West Java	644300
950	Padang	IDN	Sumatera Barat	534474
951	Surakarta	IDN	Central Java	518600
952	Banjarmasin	IDN	Kalimantan Selatan	482931
953	Pekan Baru	IDN	Riau	438638
954	Denpasar	IDN	Bali	435000
955	Yogyakarta	IDN	Yogyakarta	418944
956	Pontianak	IDN	Kalimantan Barat	409632
957	Samarinda	IDN	Kalimantan Timur	399175
958	Jambi	IDN	Jambi	385201
959	Depok	IDN	West Java	365200
960	Cimahi	IDN	West Java	344600
961	Balikpapan	IDN	Kalimantan Timur	338752
962	Manado	IDN	Sulawesi Utara	332288
963	Mataram	IDN	Nusa Tenggara Barat	306600
964	Pekalongan	IDN	Central Java	301504
965	Tegal	IDN	Central Java	289744
966	Bogor	IDN	West Java	285114
967	Ciputat	IDN	West Java	270800
968	Pondokgede	IDN	West Java	263200
969	Cirebon	IDN	West Java	254406
970	Kediri	IDN	East Java	253760
971	Ambon	IDN	Molukit	249312
972	Jember	IDN	East Java	218500
973	Cilacap	IDN	Central Java	206900
974	Cimanggis	IDN	West Java	205100
975	Pematang Siantar	IDN	Sumatera Utara	203056
976	Purwokerto	IDN	Central Java	202500
977	Ciomas	IDN	West Java	187400
978	Tasikmalaya	IDN	West Java	179800
979	Madiun	IDN	East Java	171532
980	Bengkulu	IDN	Bengkulu	146439
981	Karawang	IDN	West Java	145000
982	Banda Aceh	IDN	Aceh	143409
983	Palu	IDN	Sulawesi Tengah	142800
984	Pasuruan	IDN	East Java	134019
985	Kupang	IDN	Nusa Tenggara Timur	129300
986	Tebing Tinggi	IDN	Sumatera Utara	129300
987	Percut Sei Tuan	IDN	Sumatera Utara	129000
988	Binjai	IDN	Sumatera Utara	127222
989	Sukabumi	IDN	West Java	125766
990	Waru	IDN	East Java	124300
991	Pangkal Pinang	IDN	Sumatera Selatan	124000
992	Magelang	IDN	Central Java	123800
993	Blitar	IDN	East Java	122600
994	Serang	IDN	West Java	122400
995	Probolinggo	IDN	East Java	120770
996	Cilegon	IDN	West Java	117000
997	Cianjur	IDN	West Java	114300
998	Ciparay	IDN	West Java	111500
999	Lhokseumawe	IDN	Aceh	109600
1000	Taman	IDN	East Java	107000
1001	Depok	IDN	Yogyakarta	106800
1002	Citeureup	IDN	West Java	105100
1003	Pemalang	IDN	Central Java	103500
1004	Klaten	IDN	Central Java	103300
1005	Salatiga	IDN	Central Java	103000
1006	Cibinong	IDN	West Java	101300
1007	Palangka Raya	IDN	Kalimantan Tengah	99693
1008	Mojokerto	IDN	East Java	96626
1009	Purwakarta	IDN	West Java	95900
1010	Garut	IDN	West Java	95800
1011	Kudus	IDN	Central Java	95300
1012	Kendari	IDN	Sulawesi Tenggara	94800
1013	Jaya Pura	IDN	West Irian	94700
1014	Gorontalo	IDN	Sulawesi Utara	94058
1015	Majalaya	IDN	West Java	93200
1016	Pondok Aren	IDN	West Java	92700
1017	Jombang	IDN	East Java	92600
1018	Sunggal	IDN	Sumatera Utara	92300
1019	Batam	IDN	Riau	91871
1020	Padang Sidempuan	IDN	Sumatera Utara	91200
1021	Sawangan	IDN	West Java	91100
1022	Banyuwangi	IDN	East Java	89900
1023	Tanjung Pinang	IDN	Riau	89900
1024	Mumbai (Bombay)	IND	Maharashtra	10500000
1025	Delhi	IND	Delhi	7206704
1026	Calcutta [Kolkata]	IND	West Bengali	4399819
1027	Chennai (Madras)	IND	Tamil Nadu	3841396
1028	Hyderabad	IND	Andhra Pradesh	2964638
1029	Ahmedabad	IND	Gujarat	2876710
1030	Bangalore	IND	Karnataka	2660088
1031	Kanpur	IND	Uttar Pradesh	1874409
1032	Nagpur	IND	Maharashtra	1624752
1033	Lucknow	IND	Uttar Pradesh	1619115
1034	Pune	IND	Maharashtra	1566651
1035	Surat	IND	Gujarat	1498817
1036	Jaipur	IND	Rajasthan	1458483
1037	Indore	IND	Madhya Pradesh	1091674
1038	Bhopal	IND	Madhya Pradesh	1062771
1039	Ludhiana	IND	Punjab	1042740
1040	Vadodara (Baroda)	IND	Gujarat	1031346
1041	Kalyan	IND	Maharashtra	1014557
1042	Madurai	IND	Tamil Nadu	977856
1043	Haora (Howrah)	IND	West Bengali	950435
1044	Varanasi (Benares)	IND	Uttar Pradesh	929270
1045	Patna	IND	Bihar	917243
1046	Srinagar	IND	Jammu and Kashmir	892506
1047	Agra	IND	Uttar Pradesh	891790
1048	Coimbatore	IND	Tamil Nadu	816321
1049	Thane (Thana)	IND	Maharashtra	803389
1050	Allahabad	IND	Uttar Pradesh	792858
1051	Meerut	IND	Uttar Pradesh	753778
1052	Vishakhapatnam	IND	Andhra Pradesh	752037
1053	Jabalpur	IND	Madhya Pradesh	741927
1054	Amritsar	IND	Punjab	708835
1055	Faridabad	IND	Haryana	703592
1056	Vijayawada	IND	Andhra Pradesh	701827
1057	Gwalior	IND	Madhya Pradesh	690765
1058	Jodhpur	IND	Rajasthan	666279
1059	Nashik (Nasik)	IND	Maharashtra	656925
1060	Hubli-Dharwad	IND	Karnataka	648298
1061	Solapur (Sholapur)	IND	Maharashtra	604215
1062	Ranchi	IND	Jharkhand	599306
1063	Bareilly	IND	Uttar Pradesh	587211
1064	Guwahati (Gauhati)	IND	Assam	584342
1065	Shambajinagar (Aurangabad)	IND	Maharashtra	573272
1066	Cochin (Kochi)	IND	Kerala	564589
1067	Rajkot	IND	Gujarat	559407
1068	Kota	IND	Rajasthan	537371
1069	Thiruvananthapuram (Trivandrum	IND	Kerala	524006
1070	Pimpri-Chinchwad	IND	Maharashtra	517083
1071	Jalandhar (Jullundur)	IND	Punjab	509510
1072	Gorakhpur	IND	Uttar Pradesh	505566
1073	Chandigarh	IND	Chandigarh	504094
1074	Mysore	IND	Karnataka	480692
1075	Aligarh	IND	Uttar Pradesh	480520
1076	Guntur	IND	Andhra Pradesh	471051
1077	Jamshedpur	IND	Jharkhand	460577
1078	Ghaziabad	IND	Uttar Pradesh	454156
1079	Warangal	IND	Andhra Pradesh	447657
1080	Raipur	IND	Chhatisgarh	438639
1081	Moradabad	IND	Uttar Pradesh	429214
1082	Durgapur	IND	West Bengali	425836
1083	Amravati	IND	Maharashtra	421576
1084	Calicut (Kozhikode)	IND	Kerala	419831
1085	Bikaner	IND	Rajasthan	416289
1086	Bhubaneswar	IND	Orissa	411542
1087	Kolhapur	IND	Maharashtra	406370
1088	Kataka (Cuttack)	IND	Orissa	403418
1089	Ajmer	IND	Rajasthan	402700
1090	Bhavnagar	IND	Gujarat	402338
1091	Tiruchirapalli	IND	Tamil Nadu	387223
1092	Bhilai	IND	Chhatisgarh	386159
1093	Bhiwandi	IND	Maharashtra	379070
1094	Saharanpur	IND	Uttar Pradesh	374945
1095	Ulhasnagar	IND	Maharashtra	369077
1096	Salem	IND	Tamil Nadu	366712
1097	Ujjain	IND	Madhya Pradesh	362266
1098	Malegaon	IND	Maharashtra	342595
1099	Jamnagar	IND	Gujarat	341637
1100	Bokaro Steel City	IND	Jharkhand	333683
1101	Akola	IND	Maharashtra	328034
1102	Belgaum	IND	Karnataka	326399
1103	Rajahmundry	IND	Andhra Pradesh	324851
1104	Nellore	IND	Andhra Pradesh	316606
1105	Udaipur	IND	Rajasthan	308571
1106	New Bombay	IND	Maharashtra	307297
1107	Bhatpara	IND	West Bengali	304952
1108	Gulbarga	IND	Karnataka	304099
1109	New Delhi	IND	Delhi	301297
1110	Jhansi	IND	Uttar Pradesh	300850
1111	Gaya	IND	Bihar	291675
1112	Kakinada	IND	Andhra Pradesh	279980
1113	Dhule (Dhulia)	IND	Maharashtra	278317
1114	Panihati	IND	West Bengali	275990
1115	Nanded (Nander)	IND	Maharashtra	275083
1116	Mangalore	IND	Karnataka	273304
1117	Dehra Dun	IND	Uttaranchal	270159
1118	Kamarhati	IND	West Bengali	266889
1119	Davangere	IND	Karnataka	266082
1120	Asansol	IND	West Bengali	262188
1121	Bhagalpur	IND	Bihar	253225
1122	Bellary	IND	Karnataka	245391
1123	Barddhaman (Burdwan)	IND	West Bengali	245079
1124	Rampur	IND	Uttar Pradesh	243742
1125	Jalgaon	IND	Maharashtra	242193
1126	Muzaffarpur	IND	Bihar	241107
1127	Nizamabad	IND	Andhra Pradesh	241034
1128	Muzaffarnagar	IND	Uttar Pradesh	240609
1129	Patiala	IND	Punjab	238368
1130	Shahjahanpur	IND	Uttar Pradesh	237713
1131	Kurnool	IND	Andhra Pradesh	236800
1132	Tiruppur (Tirupper)	IND	Tamil Nadu	235661
1133	Rohtak	IND	Haryana	233400
1134	South Dum Dum	IND	West Bengali	232811
1135	Mathura	IND	Uttar Pradesh	226691
1136	Chandrapur	IND	Maharashtra	226105
1137	Barahanagar (Baranagar)	IND	West Bengali	224821
1138	Darbhanga	IND	Bihar	218391
1139	Siliguri (Shiliguri)	IND	West Bengali	216950
1140	Raurkela	IND	Orissa	215489
1141	Ambattur	IND	Tamil Nadu	215424
1142	Panipat	IND	Haryana	215218
1143	Firozabad	IND	Uttar Pradesh	215128
1144	Ichalkaranji	IND	Maharashtra	214950
1145	Jammu	IND	Jammu and Kashmir	214737
1146	Ramagundam	IND	Andhra Pradesh	214384
1147	Eluru	IND	Andhra Pradesh	212866
1148	Brahmapur	IND	Orissa	210418
1149	Alwar	IND	Rajasthan	205086
1150	Pondicherry	IND	Pondicherry	203065
1151	Thanjavur	IND	Tamil Nadu	202013
1152	Bihar Sharif	IND	Bihar	201323
1153	Tuticorin	IND	Tamil Nadu	199854
1154	Imphal	IND	Manipur	198535
1155	Latur	IND	Maharashtra	197408
1156	Sagar	IND	Madhya Pradesh	195346
1157	Farrukhabad-cum-Fatehgarh	IND	Uttar Pradesh	194567
1158	Sangli	IND	Maharashtra	193197
1159	Parbhani	IND	Maharashtra	190255
1160	Nagar Coil	IND	Tamil Nadu	190084
1161	Bijapur	IND	Karnataka	186939
1162	Kukatpalle	IND	Andhra Pradesh	185378
1163	Bally	IND	West Bengali	184474
1164	Bhilwara	IND	Rajasthan	183965
1165	Ratlam	IND	Madhya Pradesh	183375
1166	Avadi	IND	Tamil Nadu	183215
1167	Dindigul	IND	Tamil Nadu	182477
1168	Ahmadnagar	IND	Maharashtra	181339
1169	Bilaspur	IND	Chhatisgarh	179833
1170	Shimoga	IND	Karnataka	179258
1171	Kharagpur	IND	West Bengali	177989
1172	Mira Bhayandar	IND	Maharashtra	175372
1173	Vellore	IND	Tamil Nadu	175061
1174	Jalna	IND	Maharashtra	174985
1175	Burnpur	IND	West Bengali	174933
1176	Anantapur	IND	Andhra Pradesh	174924
1177	Allappuzha (Alleppey)	IND	Kerala	174666
1178	Tirupati	IND	Andhra Pradesh	174369
1179	Karnal	IND	Haryana	173751
1180	Burhanpur	IND	Madhya Pradesh	172710
1181	Hisar (Hissar)	IND	Haryana	172677
1182	Tiruvottiyur	IND	Tamil Nadu	172562
1183	Mirzapur-cum-Vindhyachal	IND	Uttar Pradesh	169336
1184	Secunderabad	IND	Andhra Pradesh	167461
1185	Nadiad	IND	Gujarat	167051
1186	Dewas	IND	Madhya Pradesh	164364
1187	Murwara (Katni)	IND	Madhya Pradesh	163431
1188	Ganganagar	IND	Rajasthan	161482
1189	Vizianagaram	IND	Andhra Pradesh	160359
1190	Erode	IND	Tamil Nadu	159232
1191	Machilipatnam (Masulipatam)	IND	Andhra Pradesh	159110
1192	Bhatinda (Bathinda)	IND	Punjab	159042
1193	Raichur	IND	Karnataka	157551
1194	Agartala	IND	Tripura	157358
1195	Arrah (Ara)	IND	Bihar	157082
1196	Satna	IND	Madhya Pradesh	156630
1197	Lalbahadur Nagar	IND	Andhra Pradesh	155500
1198	Aizawl	IND	Mizoram	155240
1199	Uluberia	IND	West Bengali	155172
1200	Katihar	IND	Bihar	154367
1201	Cuddalore	IND	Tamil Nadu	153086
1202	Hugli-Chinsurah	IND	West Bengali	151806
1203	Dhanbad	IND	Jharkhand	151789
1204	Raiganj	IND	West Bengali	151045
1205	Sambhal	IND	Uttar Pradesh	150869
1206	Durg	IND	Chhatisgarh	150645
1207	Munger (Monghyr)	IND	Bihar	150112
1208	Kanchipuram	IND	Tamil Nadu	150100
1209	North Dum Dum	IND	West Bengali	149965
1210	Karimnagar	IND	Andhra Pradesh	148583
1211	Bharatpur	IND	Rajasthan	148519
1212	Sikar	IND	Rajasthan	148272
1213	Hardwar (Haridwar)	IND	Uttaranchal	147305
1214	Dabgram	IND	West Bengali	147217
1215	Morena	IND	Madhya Pradesh	147124
1216	Noida	IND	Uttar Pradesh	146514
1217	Hapur	IND	Uttar Pradesh	146262
1218	Bhusawal	IND	Maharashtra	145143
1219	Khandwa	IND	Madhya Pradesh	145133
1220	Yamuna Nagar	IND	Haryana	144346
1221	Sonipat (Sonepat)	IND	Haryana	143922
1222	Tenali	IND	Andhra Pradesh	143726
1223	Raurkela Civil Township	IND	Orissa	140408
1224	Kollam (Quilon)	IND	Kerala	139852
1225	Kumbakonam	IND	Tamil Nadu	139483
1226	Ingraj Bazar (English Bazar)	IND	West Bengali	139204
1227	Timkur	IND	Karnataka	138903
1228	Amroha	IND	Uttar Pradesh	137061
1229	Serampore	IND	West Bengali	137028
1230	Chapra	IND	Bihar	136877
1231	Pali	IND	Rajasthan	136842
1232	Maunath Bhanjan	IND	Uttar Pradesh	136697
1233	Adoni	IND	Andhra Pradesh	136182
1234	Jaunpur	IND	Uttar Pradesh	136062
1235	Tirunelveli	IND	Tamil Nadu	135825
1236	Bahraich	IND	Uttar Pradesh	135400
1237	Gadag Betigeri	IND	Karnataka	134051
1238	Proddatur	IND	Andhra Pradesh	133914
1239	Chittoor	IND	Andhra Pradesh	133462
1240	Barrackpur	IND	West Bengali	133265
1241	Bharuch (Broach)	IND	Gujarat	133102
1242	Naihati	IND	West Bengali	132701
1243	Shillong	IND	Meghalaya	131719
1244	Sambalpur	IND	Orissa	131138
1245	Junagadh	IND	Gujarat	130484
1246	Rae Bareli	IND	Uttar Pradesh	129904
1247	Rewa	IND	Madhya Pradesh	128981
1248	Gurgaon	IND	Haryana	128608
1249	Khammam	IND	Andhra Pradesh	127992
1250	Bulandshahr	IND	Uttar Pradesh	127201
1251	Navsari	IND	Gujarat	126089
1252	Malkajgiri	IND	Andhra Pradesh	126066
1253	Midnapore (Medinipur)	IND	West Bengali	125498
1254	Miraj	IND	Maharashtra	125407
1255	Raj Nandgaon	IND	Chhatisgarh	125371
1256	Alandur	IND	Tamil Nadu	125244
1257	Puri	IND	Orissa	125199
1258	Navadwip	IND	West Bengali	125037
1259	Sirsa	IND	Haryana	125000
1260	Korba	IND	Chhatisgarh	124501
1261	Faizabad	IND	Uttar Pradesh	124437
1262	Etawah	IND	Uttar Pradesh	124072
1263	Pathankot	IND	Punjab	123930
1264	Gandhinagar	IND	Gujarat	123359
1265	Palghat (Palakkad)	IND	Kerala	123289
1266	Veraval	IND	Gujarat	123000
1267	Hoshiarpur	IND	Punjab	122705
1268	Ambala	IND	Haryana	122596
1269	Sitapur	IND	Uttar Pradesh	121842
1270	Bhiwani	IND	Haryana	121629
1271	Cuddapah	IND	Andhra Pradesh	121463
1272	Bhimavaram	IND	Andhra Pradesh	121314
1273	Krishnanagar	IND	West Bengali	121110
1274	Chandannagar	IND	West Bengali	120378
1275	Mandya	IND	Karnataka	120265
1276	Dibrugarh	IND	Assam	120127
1277	Nandyal	IND	Andhra Pradesh	119813
1278	Balurghat	IND	West Bengali	119796
1279	Neyveli	IND	Tamil Nadu	118080
1280	Fatehpur	IND	Uttar Pradesh	117675
1281	Mahbubnagar	IND	Andhra Pradesh	116833
1282	Budaun	IND	Uttar Pradesh	116695
1283	Porbandar	IND	Gujarat	116671
1284	Silchar	IND	Assam	115483
1285	Berhampore (Baharampur)	IND	West Bengali	115144
1286	Purnea (Purnia)	IND	Jharkhand	114912
1287	Bankura	IND	West Bengali	114876
1288	Rajapalaiyam	IND	Tamil Nadu	114202
1289	Titagarh	IND	West Bengali	114085
1290	Halisahar	IND	West Bengali	114028
1291	Hathras	IND	Uttar Pradesh	113285
1292	Bhir (Bid)	IND	Maharashtra	112434
1293	Pallavaram	IND	Tamil Nadu	111866
1294	Anand	IND	Gujarat	110266
1295	Mango	IND	Jharkhand	110024
1296	Santipur	IND	West Bengali	109956
1297	Bhind	IND	Madhya Pradesh	109755
1298	Gondiya	IND	Maharashtra	109470
1299	Tiruvannamalai	IND	Tamil Nadu	109196
1300	Yeotmal (Yavatmal)	IND	Maharashtra	108578
1301	Kulti-Barakar	IND	West Bengali	108518
1302	Moga	IND	Punjab	108304
1303	Shivapuri	IND	Madhya Pradesh	108277
1304	Bidar	IND	Karnataka	108016
1305	Guntakal	IND	Andhra Pradesh	107592
1306	Unnao	IND	Uttar Pradesh	107425
1307	Barasat	IND	West Bengali	107365
1308	Tambaram	IND	Tamil Nadu	107187
1309	Abohar	IND	Punjab	107163
1310	Pilibhit	IND	Uttar Pradesh	106605
1311	Valparai	IND	Tamil Nadu	106523
1312	Gonda	IND	Uttar Pradesh	106078
1313	Surendranagar	IND	Gujarat	105973
1314	Qutubullapur	IND	Andhra Pradesh	105380
1315	Beawar	IND	Rajasthan	105363
1316	Hindupur	IND	Andhra Pradesh	104651
1317	Gandhidham	IND	Gujarat	104585
1318	Haldwani-cum-Kathgodam	IND	Uttaranchal	104195
1319	Tellicherry (Thalassery)	IND	Kerala	103579
1320	Wardha	IND	Maharashtra	102985
1321	Rishra	IND	West Bengali	102649
1322	Bhuj	IND	Gujarat	102176
1323	Modinagar	IND	Uttar Pradesh	101660
1324	Gudivada	IND	Andhra Pradesh	101656
1325	Basirhat	IND	West Bengali	101409
1326	Uttarpara-Kotrung	IND	West Bengali	100867
1327	Ongole	IND	Andhra Pradesh	100836
1328	North Barrackpur	IND	West Bengali	100513
1329	Guna	IND	Madhya Pradesh	100490
1330	Haldia	IND	West Bengali	100347
1331	Habra	IND	West Bengali	100223
1332	Kanchrapara	IND	West Bengali	100194
1333	Tonk	IND	Rajasthan	100079
1334	Champdani	IND	West Bengali	98818
1335	Orai	IND	Uttar Pradesh	98640
1336	Pudukkottai	IND	Tamil Nadu	98619
1337	Sasaram	IND	Bihar	98220
1338	Hazaribag	IND	Jharkhand	97712
1339	Palayankottai	IND	Tamil Nadu	97662
1340	Banda	IND	Uttar Pradesh	97227
1341	Godhra	IND	Gujarat	96813
1342	Hospet	IND	Karnataka	96322
1343	Ashoknagar-Kalyangarh	IND	West Bengali	96315
1344	Achalpur	IND	Maharashtra	96216
1345	Patan	IND	Gujarat	96109
1346	Mandasor	IND	Madhya Pradesh	95758
1347	Damoh	IND	Madhya Pradesh	95661
1348	Satara	IND	Maharashtra	95133
1349	Meerut Cantonment	IND	Uttar Pradesh	94876
1350	Dehri	IND	Bihar	94526
1351	Delhi Cantonment	IND	Delhi	94326
1352	Chhindwara	IND	Madhya Pradesh	93731
1353	Bansberia	IND	West Bengali	93447
1354	Nagaon	IND	Assam	93350
1355	Kanpur Cantonment	IND	Uttar Pradesh	93109
1356	Vidisha	IND	Madhya Pradesh	92917
1357	Bettiah	IND	Bihar	92583
1358	Purulia	IND	Jharkhand	92574
1359	Hassan	IND	Karnataka	90803
1360	Ambala Sadar	IND	Haryana	90712
1361	Baidyabati	IND	West Bengali	90601
1362	Morvi	IND	Gujarat	90357
1363	Raigarh	IND	Chhatisgarh	89166
1364	Vejalpur	IND	Gujarat	89053
1365	Baghdad	IRQ	Baghdad	4336000
1366	Mosul	IRQ	Ninawa	879000
1367	Irbil	IRQ	Irbil	485968
1368	Kirkuk	IRQ	al-Tamim	418624
1369	Basra	IRQ	Basra	406296
1370	al-Sulaymaniya	IRQ	al-Sulaymaniya	364096
1371	al-Najaf	IRQ	al-Najaf	309010
1372	Karbala	IRQ	Karbala	296705
1373	al-Hilla	IRQ	Babil	268834
1374	al-Nasiriya	IRQ	DhiQar	265937
1375	al-Amara	IRQ	Maysan	208797
1376	al-Diwaniya	IRQ	al-Qadisiya	196519
1377	al-Ramadi	IRQ	al-Anbar	192556
1378	al-Kut	IRQ	Wasit	183183
1379	Baquba	IRQ	Diyala	114516
1380	Teheran	IRN	Teheran	6758845
1381	Mashhad	IRN	Khorasan	1887405
1382	Esfahan	IRN	Esfahan	1266072
1383	Tabriz	IRN	East Azerbaidzan	1191043
1384	Shiraz	IRN	Fars	1053025
1385	Karaj	IRN	Teheran	940968
1386	Ahvaz	IRN	Khuzestan	804980
1387	Qom	IRN	Qom	777677
1388	Kermanshah	IRN	Kermanshah	692986
1389	Urmia	IRN	West Azerbaidzan	435200
1390	Zahedan	IRN	Sistan va Baluchesta	419518
1391	Rasht	IRN	Gilan	417748
1392	Hamadan	IRN	Hamadan	401281
1393	Kerman	IRN	Kerman	384991
1394	Arak	IRN	Markazi	380755
1395	Ardebil	IRN	Ardebil	340386
1396	Yazd	IRN	Yazd	326776
1397	Qazvin	IRN	Qazvin	291117
1398	Zanjan	IRN	Zanjan	286295
1399	Sanandaj	IRN	Kordestan	277808
1400	Bandar-e-Abbas	IRN	Hormozgan	273578
1401	Khorramabad	IRN	Lorestan	272815
1402	Eslamshahr	IRN	Teheran	265450
1403	Borujerd	IRN	Lorestan	217804
1404	Abadan	IRN	Khuzestan	206073
1405	Dezful	IRN	Khuzestan	202639
1406	Kashan	IRN	Esfahan	201372
1407	Sari	IRN	Mazandaran	195882
1408	Gorgan	IRN	Golestan	188710
1409	Najafabad	IRN	Esfahan	178498
1410	Sabzevar	IRN	Khorasan	170738
1411	Khomeynishahr	IRN	Esfahan	165888
1412	Amol	IRN	Mazandaran	159092
1413	Neyshabur	IRN	Khorasan	158847
1414	Babol	IRN	Mazandaran	158346
1415	Khoy	IRN	West Azerbaidzan	148944
1416	Malayer	IRN	Hamadan	144373
1417	Bushehr	IRN	Bushehr	143641
1418	Qaemshahr	IRN	Mazandaran	143286
1419	Qarchak	IRN	Teheran	142690
1420	Qods	IRN	Teheran	138278
1421	Sirjan	IRN	Kerman	135024
1422	Bojnurd	IRN	Khorasan	134835
1423	Maragheh	IRN	East Azerbaidzan	132318
1424	Birjand	IRN	Khorasan	127608
1425	Ilam	IRN	Ilam	126346
1426	Bukan	IRN	West Azerbaidzan	120020
1427	Masjed-e-Soleyman	IRN	Khuzestan	116883
1428	Saqqez	IRN	Kordestan	115394
1429	Gonbad-e Qabus	IRN	Mazandaran	111253
1430	Saveh	IRN	Qom	111245
1431	Mahabad	IRN	West Azerbaidzan	107799
1432	Varamin	IRN	Teheran	107233
1433	Andimeshk	IRN	Khuzestan	106923
1434	Khorramshahr	IRN	Khuzestan	105636
1435	Shahrud	IRN	Semnan	104765
1436	Marv Dasht	IRN	Fars	103579
1437	Zabol	IRN	Sistan va Baluchesta	100887
1438	Shahr-e Kord	IRN	Chaharmahal va Bakht	100477
1439	Bandar-e Anzali	IRN	Gilan	98500
1440	Rafsanjan	IRN	Kerman	98300
1441	Marand	IRN	East Azerbaidzan	96400
1442	Torbat-e Heydariyeh	IRN	Khorasan	94600
1443	Jahrom	IRN	Fars	94200
1444	Semnan	IRN	Semnan	91045
1445	Miandoab	IRN	West Azerbaidzan	90100
1446	Qomsheh	IRN	Esfahan	89800
1447	Dublin	IRL	Leinster	481854
1448	Cork	IRL	Munster	127187
1449	Reykjavik	ISL	Hofuoborgarsvaoi	109184
1450	Jerusalem	ISR	Jerusalem	633700
1451	Tel Aviv-Jaffa	ISR	Tel Aviv	348100
1452	Haifa	ISR	Haifa	265700
1453	Rishon Le Ziyyon	ISR	Ha Merkaz	188200
1454	Beerseba	ISR	Ha Darom	163700
1455	Holon	ISR	Tel Aviv	163100
1456	Petah Tiqwa	ISR	Ha Merkaz	159400
1457	Ashdod	ISR	Ha Darom	155800
1458	Netanya	ISR	Ha Merkaz	154900
1459	Bat Yam	ISR	Tel Aviv	137000
1460	Bene Beraq	ISR	Tel Aviv	133900
1461	Ramat Gan	ISR	Tel Aviv	126900
1462	Ashqelon	ISR	Ha Darom	92300
1463	Rehovot	ISR	Ha Merkaz	90300
1464	Roma	ITA	Latium	2643581
1465	Milano	ITA	Lombardia	1300977
1466	Napoli	ITA	Campania	1002619
1467	Torino	ITA	Piemonte	903705
1468	Palermo	ITA	Sisilia	683794
1469	Genova	ITA	Liguria	636104
1470	Bologna	ITA	Emilia-Romagna	381161
1471	Firenze	ITA	Toscana	376662
1472	Catania	ITA	Sisilia	337862
1473	Bari	ITA	Apulia	331848
1474	Venezia	ITA	Veneto	277305
1475	Messina	ITA	Sisilia	259156
1476	Verona	ITA	Veneto	255268
1477	Trieste	ITA	Friuli-Venezia Giuli	216459
1478	Padova	ITA	Veneto	211391
1479	Taranto	ITA	Apulia	208214
1480	Brescia	ITA	Lombardia	191317
1481	Reggio di Calabria	ITA	Calabria	179617
1482	Modena	ITA	Emilia-Romagna	176022
1483	Prato	ITA	Toscana	172473
1484	Parma	ITA	Emilia-Romagna	168717
1485	Cagliari	ITA	Sardinia	165926
1486	Livorno	ITA	Toscana	161673
1487	Perugia	ITA	Umbria	156673
1488	Foggia	ITA	Apulia	154891
1489	Reggio nell Emilia	ITA	Emilia-Romagna	143664
1490	Salerno	ITA	Campania	142055
1491	Ravenna	ITA	Emilia-Romagna	138418
1492	Ferrara	ITA	Emilia-Romagna	132127
1493	Rimini	ITA	Emilia-Romagna	131062
1494	Syrakusa	ITA	Sisilia	126282
1495	Sassari	ITA	Sardinia	120803
1496	Monza	ITA	Lombardia	119516
1497	Bergamo	ITA	Lombardia	117837
1498	Pescara	ITA	Abruzzit	115698
1499	Latina	ITA	Latium	114099
1500	Vicenza	ITA	Veneto	109738
1501	Terni	ITA	Umbria	107770
1502	Forli	ITA	Emilia-Romagna	107475
1503	Trento	ITA	Trentino-Alto Adige	104906
1504	Novara	ITA	Piemonte	102037
1505	Piacenza	ITA	Emilia-Romagna	98384
1506	Ancona	ITA	Marche	98329
1507	Lecce	ITA	Apulia	98208
1508	Bolzano	ITA	Trentino-Alto Adige	97232
1509	Catanzaro	ITA	Calabria	96700
1510	La Spezia	ITA	Liguria	95504
1511	Udine	ITA	Friuli-Venezia Giuli	94932
1512	Torre del Greco	ITA	Campania	94505
1513	Andria	ITA	Apulia	94443
1514	Brindisi	ITA	Apulia	93454
1515	Giugliano in Campania	ITA	Campania	93286
1516	Pisa	ITA	Toscana	92379
1517	Barletta	ITA	Apulia	91904
1518	Arezzo	ITA	Toscana	91729
1519	Alessandria	ITA	Piemonte	90289
1520	Cesena	ITA	Emilia-Romagna	89852
1521	Pesaro	ITA	Marche	88987
1522	Dili	TMP	Dili	47900
1523	Wien	AUT	Wien	1608144
1524	Graz	AUT	Steiermark	240967
1525	Linz	AUT	North Austria	188022
1526	Salzburg	AUT	Salzburg	144247
1527	Innsbruck	AUT	Tiroli	111752
1528	Klagenfurt	AUT	Karnten	91141
1529	Spanish Town	JAM	St. Catherine	110379
1530	Kingston	JAM	St. Andrew	103962
1531	Portmore	JAM	St. Andrew	99799
1532	Tokyo	JPN	Tokyo-to	7980230
1533	Jokohama [Yokohama]	JPN	Kanagawa	3339594
1534	Osaka	JPN	Osaka	2595674
1535	Nagoya	JPN	Aichi	2154376
1536	Sapporo	JPN	Hokkaido	1790886
1537	Kioto	JPN	Kyoto	1461974
1538	Kobe	JPN	Hyogo	1425139
1539	Fukuoka	JPN	Fukuoka	1308379
1540	Kawasaki	JPN	Kanagawa	1217359
1541	Hiroshima	JPN	Hiroshima	1119117
1542	Kitakyushu	JPN	Fukuoka	1016264
1543	Sendai	JPN	Miyagi	989975
1544	Chiba	JPN	Chiba	863930
1545	Sakai	JPN	Osaka	797735
1546	Kumamoto	JPN	Kumamoto	656734
1547	Okayama	JPN	Okayama	624269
1548	Sagamihara	JPN	Kanagawa	586300
1549	Hamamatsu	JPN	Shizuoka	568796
1550	Kagoshima	JPN	Kagoshima	549977
1551	Funabashi	JPN	Chiba	545299
1552	Higashiosaka	JPN	Osaka	517785
1553	Hachioji	JPN	Tokyo-to	513451
1554	Niigata	JPN	Niigata	497464
1555	Amagasaki	JPN	Hyogo	481434
1556	Himeji	JPN	Hyogo	475167
1557	Shizuoka	JPN	Shizuoka	473854
1558	Urawa	JPN	Saitama	469675
1559	Matsuyama	JPN	Ehime	466133
1560	Matsudo	JPN	Chiba	461126
1561	Kanazawa	JPN	Ishikawa	455386
1562	Kawaguchi	JPN	Saitama	452155
1563	Ichikawa	JPN	Chiba	441893
1564	Omiya	JPN	Saitama	441649
1565	Utsunomiya	JPN	Tochigi	440353
1566	Oita	JPN	Oita	433401
1567	Nagasaki	JPN	Nagasaki	432759
1568	Yokosuka	JPN	Kanagawa	430200
1569	Kurashiki	JPN	Okayama	425103
1570	Gifu	JPN	Gifu	408007
1571	Hirakata	JPN	Osaka	403151
1572	Nishinomiya	JPN	Hyogo	397618
1573	Toyonaka	JPN	Osaka	396689
1574	Wakayama	JPN	Wakayama	391233
1575	Fukuyama	JPN	Hiroshima	376921
1576	Fujisawa	JPN	Kanagawa	372840
1577	Asahikawa	JPN	Hokkaido	364813
1578	Machida	JPN	Tokyo-to	364197
1579	Nara	JPN	Nara	362812
1580	Takatsuki	JPN	Osaka	361747
1581	Iwaki	JPN	Fukushima	361737
1582	Nagano	JPN	Nagano	361391
1583	Toyohashi	JPN	Aichi	360066
1584	Toyota	JPN	Aichi	346090
1585	Suita	JPN	Osaka	345750
1586	Takamatsu	JPN	Kagawa	332471
1587	Koriyama	JPN	Fukushima	330335
1588	Okazaki	JPN	Aichi	328711
1589	Kawagoe	JPN	Saitama	327211
1590	Tokorozawa	JPN	Saitama	325809
1591	Toyama	JPN	Toyama	325790
1592	Kochi	JPN	Kochi	324710
1593	Kashiwa	JPN	Chiba	320296
1594	Akita	JPN	Akita	314440
1595	Miyazaki	JPN	Miyazaki	303784
1596	Koshigaya	JPN	Saitama	301446
1597	Naha	JPN	Okinawa	299851
1598	Aomori	JPN	Aomori	295969
1599	Hakodate	JPN	Hokkaido	294788
1600	Akashi	JPN	Hyogo	292253
1601	Yokkaichi	JPN	Mie	288173
1602	Fukushima	JPN	Fukushima	287525
1603	Morioka	JPN	Iwate	287353
1604	Maebashi	JPN	Gumma	284473
1605	Kasugai	JPN	Aichi	282348
1606	Otsu	JPN	Shiga	282070
1607	Ichihara	JPN	Chiba	279280
1608	Yao	JPN	Osaka	276421
1609	Ichinomiya	JPN	Aichi	270828
1610	Tokushima	JPN	Tokushima	269649
1611	Kakogawa	JPN	Hyogo	266281
1612	Ibaraki	JPN	Osaka	261020
1613	Neyagawa	JPN	Osaka	257315
1614	Shimonoseki	JPN	Yamaguchi	257263
1615	Yamagata	JPN	Yamagata	255617
1616	Fukui	JPN	Fukui	254818
1617	Hiratsuka	JPN	Kanagawa	254207
1618	Mito	JPN	Ibaragi	246559
1619	Sasebo	JPN	Nagasaki	244240
1620	Hachinohe	JPN	Aomori	242979
1621	Takasaki	JPN	Gumma	239124
1622	Shimizu	JPN	Shizuoka	239123
1623	Kurume	JPN	Fukuoka	235611
1624	Fuji	JPN	Shizuoka	231527
1625	Soka	JPN	Saitama	222768
1626	Fuchu	JPN	Tokyo-to	220576
1627	Chigasaki	JPN	Kanagawa	216015
1628	Atsugi	JPN	Kanagawa	212407
1629	Numazu	JPN	Shizuoka	211382
1630	Ageo	JPN	Saitama	209442
1631	Yamato	JPN	Kanagawa	208234
1632	Matsumoto	JPN	Nagano	206801
1633	Kure	JPN	Hiroshima	206504
1634	Takarazuka	JPN	Hyogo	205993
1635	Kasukabe	JPN	Saitama	201838
1636	Chofu	JPN	Tokyo-to	201585
1637	Odawara	JPN	Kanagawa	200171
1638	Kofu	JPN	Yamanashi	199753
1639	Kushiro	JPN	Hokkaido	197608
1640	Kishiwada	JPN	Osaka	197276
1641	Hitachi	JPN	Ibaragi	196622
1642	Nagaoka	JPN	Niigata	192407
1643	Itami	JPN	Hyogo	190886
1644	Uji	JPN	Kyoto	188735
1645	Suzuka	JPN	Mie	184061
1646	Hirosaki	JPN	Aomori	177522
1647	Ube	JPN	Yamaguchi	175206
1648	Kodaira	JPN	Tokyo-to	174984
1649	Takaoka	JPN	Toyama	174380
1650	Obihiro	JPN	Hokkaido	173685
1651	Tomakomai	JPN	Hokkaido	171958
1652	Saga	JPN	Saga	170034
1653	Sakura	JPN	Chiba	168072
1654	Kamakura	JPN	Kanagawa	167661
1655	Mitaka	JPN	Tokyo-to	167268
1656	Izumi	JPN	Osaka	166979
1657	Hino	JPN	Tokyo-to	166770
1658	Hadano	JPN	Kanagawa	166512
1659	Ashikaga	JPN	Tochigi	165243
1660	Tsu	JPN	Mie	164543
1661	Sayama	JPN	Saitama	162472
1662	Yachiyo	JPN	Chiba	161222
1663	Tsukuba	JPN	Ibaragi	160768
1664	Tachikawa	JPN	Tokyo-to	159430
1665	Kumagaya	JPN	Saitama	157171
1666	Moriguchi	JPN	Osaka	155941
1667	Otaru	JPN	Hokkaido	155784
1668	Anjo	JPN	Aichi	153823
1669	Narashino	JPN	Chiba	152849
1670	Oyama	JPN	Tochigi	152820
1671	Ogaki	JPN	Gifu	151758
1672	Matsue	JPN	Shimane	149821
1673	Kawanishi	JPN	Hyogo	149794
1674	Hitachinaka	JPN	Tokyo-to	148006
1675	Niiza	JPN	Saitama	147744
1676	Nagareyama	JPN	Chiba	147738
1677	Tottori	JPN	Tottori	147523
1678	Tama	JPN	Ibaragi	146712
1679	Iruma	JPN	Saitama	145922
1680	Ota	JPN	Gumma	145317
1681	Omuta	JPN	Fukuoka	142889
1682	Komaki	JPN	Aichi	139827
1683	Ome	JPN	Tokyo-to	139216
1684	Kadoma	JPN	Osaka	138953
1685	Yamaguchi	JPN	Yamaguchi	138210
1686	Higashimurayama	JPN	Tokyo-to	136970
1687	Yonago	JPN	Tottori	136461
1688	Matsubara	JPN	Osaka	135010
1689	Musashino	JPN	Tokyo-to	134426
1690	Tsuchiura	JPN	Ibaragi	134072
1691	Joetsu	JPN	Niigata	133505
1692	Miyakonojo	JPN	Miyazaki	133183
1693	Misato	JPN	Saitama	132957
1694	Kakamigahara	JPN	Gifu	131831
1695	Daito	JPN	Osaka	130594
1696	Seto	JPN	Aichi	130470
1697	Kariya	JPN	Aichi	127969
1698	Urayasu	JPN	Chiba	127550
1699	Beppu	JPN	Oita	127486
1700	Niihama	JPN	Ehime	127207
1701	Minoo	JPN	Osaka	127026
1702	Fujieda	JPN	Shizuoka	126897
1703	Abiko	JPN	Chiba	126670
1704	Nobeoka	JPN	Miyazaki	125547
1705	Tondabayashi	JPN	Osaka	125094
1706	Ueda	JPN	Nagano	124217
1707	Kashihara	JPN	Nara	124013
1708	Matsusaka	JPN	Mie	123582
1709	Isesaki	JPN	Gumma	123285
1710	Zama	JPN	Kanagawa	122046
1711	Kisarazu	JPN	Chiba	121967
1712	Noda	JPN	Chiba	121030
1713	Ishinomaki	JPN	Miyagi	120963
1714	Fujinomiya	JPN	Shizuoka	119714
1715	Kawachinagano	JPN	Osaka	119666
1716	Imabari	JPN	Ehime	119357
1717	Aizuwakamatsu	JPN	Fukushima	119287
1718	Higashihiroshima	JPN	Hiroshima	119166
1719	Habikino	JPN	Osaka	118968
1720	Ebetsu	JPN	Hokkaido	118805
1721	Hofu	JPN	Yamaguchi	118751
1722	Kiryu	JPN	Gumma	118326
1723	Okinawa	JPN	Okinawa	117748
1724	Yaizu	JPN	Shizuoka	117258
1725	Toyokawa	JPN	Aichi	115781
1726	Ebina	JPN	Kanagawa	115571
1727	Asaka	JPN	Saitama	114815
1728	Higashikurume	JPN	Tokyo-to	111666
1729	Ikoma	JPN	Nara	111645
1730	Kitami	JPN	Hokkaido	111295
1731	Koganei	JPN	Tokyo-to	110969
1732	Iwatsuki	JPN	Saitama	110034
1733	Mishima	JPN	Shizuoka	109699
1734	Handa	JPN	Aichi	108600
1735	Muroran	JPN	Hokkaido	108275
1736	Komatsu	JPN	Ishikawa	107937
1737	Yatsushiro	JPN	Kumamoto	107661
1738	Iida	JPN	Nagano	107583
1739	Tokuyama	JPN	Yamaguchi	107078
1740	Kokubunji	JPN	Tokyo-to	106996
1741	Akishima	JPN	Tokyo-to	106914
1742	Iwakuni	JPN	Yamaguchi	106647
1743	Kusatsu	JPN	Shiga	106232
1744	Kuwana	JPN	Mie	106121
1745	Sanda	JPN	Hyogo	105643
1746	Hikone	JPN	Shiga	105508
1747	Toda	JPN	Saitama	103969
1748	Tajimi	JPN	Gifu	103171
1749	Ikeda	JPN	Osaka	102710
1750	Fukaya	JPN	Saitama	102156
1751	Ise	JPN	Mie	101732
1752	Sakata	JPN	Yamagata	101651
1753	Kasuga	JPN	Fukuoka	101344
1754	Kamagaya	JPN	Chiba	100821
1755	Tsuruoka	JPN	Yamagata	100713
1756	Hoya	JPN	Tokyo-to	100313
1757	Nishio	JPN	Chiba	100032
1758	Tokai	JPN	Aichi	99738
1759	Inazawa	JPN	Aichi	98746
1760	Sakado	JPN	Saitama	98221
1761	Isehara	JPN	Kanagawa	98123
1762	Takasago	JPN	Hyogo	97632
1763	Fujimi	JPN	Saitama	96972
1764	Urasoe	JPN	Okinawa	96002
1765	Yonezawa	JPN	Yamagata	95592
1766	Konan	JPN	Aichi	95521
1767	Yamatokoriyama	JPN	Nara	95165
1768	Maizuru	JPN	Kyoto	94784
1769	Onomichi	JPN	Hiroshima	93756
1770	Higashimatsuyama	JPN	Saitama	93342
1771	Kimitsu	JPN	Chiba	93216
1772	Isahaya	JPN	Nagasaki	93058
1773	Kanuma	JPN	Tochigi	93053
1774	Izumisano	JPN	Osaka	92583
1775	Kameoka	JPN	Kyoto	92398
1776	Mobara	JPN	Chiba	91664
1777	Narita	JPN	Chiba	91470
1778	Kashiwazaki	JPN	Niigata	91229
1779	Tsuyama	JPN	Okayama	91170
1780	Sanaa	YEM	Sanaa	503600
1781	Aden	YEM	Aden	398300
1782	Taizz	YEM	Taizz	317600
1783	Hodeida	YEM	Hodeida	298500
1784	al-Mukalla	YEM	Hadramawt	122400
1785	Ibb	YEM	Ibb	103300
1786	Amman	JOR	Amman	1000000
1787	al-Zarqa	JOR	al-Zarqa	389815
1788	Irbid	JOR	Irbid	231511
1789	al-Rusayfa	JOR	al-Zarqa	137247
1790	Wadi al-Sir	JOR	Amman	89104
1791	Flying Fish Cove	CXR	A	700
1792	Beograd	YUG	Central Serbia	1204000
1793	Novi Sad	YUG	Vojvodina	179626
1794	Nid	YUG	Central Serbia	175391
1795	Pridtina	YUG	Kosovo and Metohija	155496
1796	Kragujevac	YUG	Central Serbia	147305
1797	Podgorica	YUG	Montenegro	135000
1798	Subotica	YUG	Vojvodina	100386
1799	Prizren	YUG	Kosovo and Metohija	92303
1800	Phnom Penh	KHM	Phnom Penh	570155
1801	Battambang	KHM	Battambang	129800
1802	Siem Reap	KHM	Siem Reap	105100
1803	Douala	CMR	Littoral	1448300
1804	Yaounde	CMR	Centre	1372800
1805	Garoua	CMR	Nord	177000
1806	Maroua	CMR	Extreme-Nord	143000
1807	Bamenda	CMR	Nord-Ouest	138000
1808	Bafoussam	CMR	Ouest	131000
1809	Nkongsamba	CMR	Littoral	112454
1810	Montreal	CAN	Quebec	1016376
1811	Calgary	CAN	Alberta	768082
1812	Toronto	CAN	Ontario	688275
1813	North York	CAN	Ontario	622632
1814	Winnipeg	CAN	Manitoba	618477
1815	Edmonton	CAN	Alberta	616306
1816	Mississauga	CAN	Ontario	608072
1817	Scarborough	CAN	Ontario	594501
1818	Vancouver	CAN	British Colombia	514008
1819	Etobicoke	CAN	Ontario	348845
1820	London	CAN	Ontario	339917
1821	Hamilton	CAN	Ontario	335614
1822	Ottawa	CAN	Ontario	335277
1823	Laval	CAN	Quebec	330393
1824	Surrey	CAN	British Colombia	304477
1825	Brampton	CAN	Ontario	296711
1826	Windsor	CAN	Ontario	207588
1827	Saskatoon	CAN	Saskatchewan	193647
1828	Kitchener	CAN	Ontario	189959
1829	Markham	CAN	Ontario	189098
1830	Regina	CAN	Saskatchewan	180400
1831	Burnaby	CAN	British Colombia	179209
1832	Quebec	CAN	Quebec	167264
1833	York	CAN	Ontario	154980
1834	Richmond	CAN	British Colombia	148867
1835	Vaughan	CAN	Ontario	147889
1836	Burlington	CAN	Ontario	145150
1837	Oshawa	CAN	Ontario	140173
1838	Oakville	CAN	Ontario	139192
1839	Saint Catharines	CAN	Ontario	136216
1840	Longueuil	CAN	Quebec	127977
1841	Richmond Hill	CAN	Ontario	116428
1842	Thunder Bay	CAN	Ontario	115913
1843	Nepean	CAN	Ontario	115100
1844	Cape Breton	CAN	Nova Scotia	114733
1845	East York	CAN	Ontario	114034
1846	Halifax	CAN	Nova Scotia	113910
1847	Cambridge	CAN	Ontario	109186
1848	Gloucester	CAN	Ontario	107314
1849	Abbotsford	CAN	British Colombia	105403
1850	Guelph	CAN	Ontario	103593
1851	Saint Johns	CAN	Newfoundland	101936
1852	Coquitlam	CAN	British Colombia	101820
1853	Saanich	CAN	British Colombia	101388
1854	Gatineau	CAN	Quebec	100702
1855	Delta	CAN	British Colombia	95411
1856	Sudbury	CAN	Ontario	92686
1857	Kelowna	CAN	British Colombia	89442
1858	Barrie	CAN	Ontario	89269
1859	Praia	CPV	Sao Tiago	94800
1860	Almaty	KAZ	Almaty Qalasy	1129400
1861	Qaraghandy	KAZ	Qaraghandy	436900
1862	Shymkent	KAZ	South Kazakstan	360100
1863	Taraz	KAZ	Taraz	330100
1864	Astana	KAZ	Astana	311200
1865	Oskemen	KAZ	East Kazakstan	311000
1866	Pavlodar	KAZ	Pavlodar	300500
1867	Semey	KAZ	East Kazakstan	269600
1868	Aqtobe	KAZ	Aqtobe	253100
1869	Qostanay	KAZ	Qostanay	221400
1870	Petropavl	KAZ	North Kazakstan	203500
1871	Oral	KAZ	West Kazakstan	195500
1872	Temirtau	KAZ	Qaraghandy	170500
1873	Qyzylorda	KAZ	Qyzylorda	157400
1874	Aqtau	KAZ	Mangghystau	143400
1875	Atyrau	KAZ	Atyrau	142500
1876	Ekibastuz	KAZ	Pavlodar	127200
1877	Kokshetau	KAZ	North Kazakstan	123400
1878	Rudnyy	KAZ	Qostanay	109500
1879	Taldyqorghan	KAZ	Almaty	98000
1880	Zhezqazghan	KAZ	Qaraghandy	90000
1881	Nairobi	KEN	Nairobi	2290000
1882	Mombasa	KEN	Coast	461753
1883	Kisumu	KEN	Nyanza	192733
1884	Nakuru	KEN	Rift Valley	163927
1885	Machakos	KEN	Eastern	116293
1886	Eldoret	KEN	Rift Valley	111882
1887	Meru	KEN	Eastern	94947
1888	Nyeri	KEN	Central	91258
1889	Bangui	CAF	Bangui	524000
1890	Shanghai	CHN	Shanghai	9696300
1891	Peking	CHN	Peking	7472000
1892	Chongqing	CHN	Chongqing	6351600
1893	Tianjin	CHN	Tianjin	5286800
1894	Wuhan	CHN	Hubei	4344600
1895	Harbin	CHN	Heilongjiang	4289800
1896	Shenyang	CHN	Liaoning	4265200
1897	Kanton [Guangzhou]	CHN	Guangdong	4256300
1898	Chengdu	CHN	Sichuan	3361500
1899	Nanking [Nanjing]	CHN	Jiangsu	2870300
1900	Changchun	CHN	Jilin	2812000
1901	Xian	CHN	Shaanxi	2761400
1902	Dalian	CHN	Liaoning	2697000
1903	Qingdao	CHN	Shandong	2596000
1904	Jinan	CHN	Shandong	2278100
1905	Hangzhou	CHN	Zhejiang	2190500
1906	Zhengzhou	CHN	Henan	2107200
1907	Shijiazhuang	CHN	Hebei	2041500
1908	Taiyuan	CHN	Shanxi	1968400
1909	Kunming	CHN	Yunnan	1829500
1910	Changsha	CHN	Hunan	1809800
1911	Nanchang	CHN	Jiangxi	1691600
1912	Fuzhou	CHN	Fujian	1593800
1913	Lanzhou	CHN	Gansu	1565800
1914	Guiyang	CHN	Guizhou	1465200
1915	Ningbo	CHN	Zhejiang	1371200
1916	Hefei	CHN	Anhui	1369100
1917	Urumtdi [Urumqi]	CHN	Xinxiang	1310100
1918	Anshan	CHN	Liaoning	1200000
1919	Fushun	CHN	Liaoning	1200000
1920	Nanning	CHN	Guangxi	1161800
1921	Zibo	CHN	Shandong	1140000
1922	Qiqihar	CHN	Heilongjiang	1070000
1923	Jilin	CHN	Jilin	1040000
1924	Tangshan	CHN	Hebei	1040000
1925	Baotou	CHN	Inner Mongolia	980000
1926	Shenzhen	CHN	Guangdong	950500
1927	Hohhot	CHN	Inner Mongolia	916700
1928	Handan	CHN	Hebei	840000
1929	Wuxi	CHN	Jiangsu	830000
1930	Xuzhou	CHN	Jiangsu	810000
1931	Datong	CHN	Shanxi	800000
1932	Yichun	CHN	Heilongjiang	800000
1933	Benxi	CHN	Liaoning	770000
1934	Luoyang	CHN	Henan	760000
1935	Suzhou	CHN	Jiangsu	710000
1936	Xining	CHN	Qinghai	700200
1937	Huainan	CHN	Anhui	700000
1938	Jixi	CHN	Heilongjiang	683885
1939	Daqing	CHN	Heilongjiang	660000
1940	Fuxin	CHN	Liaoning	640000
1941	Amoy [Xiamen]	CHN	Fujian	627500
1942	Liuzhou	CHN	Guangxi	610000
1943	Shantou	CHN	Guangdong	580000
1944	Jinzhou	CHN	Liaoning	570000
1945	Mudanjiang	CHN	Heilongjiang	570000
1946	Yinchuan	CHN	Ningxia	544500
1947	Changzhou	CHN	Jiangsu	530000
1948	Zhangjiakou	CHN	Hebei	530000
1949	Dandong	CHN	Liaoning	520000
1950	Hegang	CHN	Heilongjiang	520000
1951	Kaifeng	CHN	Henan	510000
1952	Jiamusi	CHN	Heilongjiang	493409
1953	Liaoyang	CHN	Liaoning	492559
1954	Hengyang	CHN	Hunan	487148
1955	Baoding	CHN	Hebei	483155
1956	Hunjiang	CHN	Jilin	482043
1957	Xinxiang	CHN	Henan	473762
1958	Huangshi	CHN	Hubei	457601
1959	Haikou	CHN	Hainan	454300
1960	Yantai	CHN	Shandong	452127
1961	Bengbu	CHN	Anhui	449245
1962	Xiangtan	CHN	Hunan	441968
1963	Weifang	CHN	Shandong	428522
1964	Wuhu	CHN	Anhui	425740
1965	Pingxiang	CHN	Jiangxi	425579
1966	Yingkou	CHN	Liaoning	421589
1967	Anyang	CHN	Henan	420332
1968	Panzhihua	CHN	Sichuan	415466
1969	Pingdingshan	CHN	Henan	410775
1970	Xiangfan	CHN	Hubei	410407
1971	Zhuzhou	CHN	Hunan	409924
1972	Jiaozuo	CHN	Henan	409100
1973	Wenzhou	CHN	Zhejiang	401871
1974	Zhangjiang	CHN	Guangdong	400997
1975	Zigong	CHN	Sichuan	393184
1976	Shuangyashan	CHN	Heilongjiang	386081
1977	Zaozhuang	CHN	Shandong	380846
1978	Yakeshi	CHN	Inner Mongolia	377869
1979	Yichang	CHN	Hubei	371601
1980	Zhenjiang	CHN	Jiangsu	368316
1981	Huaibei	CHN	Anhui	366549
1982	Qinhuangdao	CHN	Hebei	364972
1983	Guilin	CHN	Guangxi	364130
1984	Liupanshui	CHN	Guizhou	363954
1985	Panjin	CHN	Liaoning	362773
1986	Yangquan	CHN	Shanxi	362268
1987	Jinxi	CHN	Liaoning	357052
1988	Liaoyuan	CHN	Jilin	354141
1989	Lianyungang	CHN	Jiangsu	354139
1990	Xianyang	CHN	Shaanxi	352125
1991	Taian	CHN	Shandong	350696
1992	Chifeng	CHN	Inner Mongolia	350077
1993	Shaoguan	CHN	Guangdong	350043
1994	Nantong	CHN	Jiangsu	343341
1995	Leshan	CHN	Sichuan	341128
1996	Baoji	CHN	Shaanxi	337765
1997	Linyi	CHN	Shandong	324720
1998	Tonghua	CHN	Jilin	324600
1999	Siping	CHN	Jilin	317223
2000	Changzhi	CHN	Shanxi	317144
2001	Tengzhou	CHN	Shandong	315083
2002	Chaozhou	CHN	Guangdong	313469
2003	Yangzhou	CHN	Jiangsu	312892
2004	Dongwan	CHN	Guangdong	308669
2005	Maanshan	CHN	Anhui	305421
2006	Foshan	CHN	Guangdong	303160
2007	Yueyang	CHN	Hunan	302800
2008	Xingtai	CHN	Hebei	302789
2009	Changde	CHN	Hunan	301276
2010	Shihezi	CHN	Xinxiang	299676
2011	Yancheng	CHN	Jiangsu	296831
2012	Jiujiang	CHN	Jiangxi	291187
2013	Dongying	CHN	Shandong	281728
2014	Shashi	CHN	Hubei	281352
2015	Xintai	CHN	Shandong	281248
2016	Jingdezhen	CHN	Jiangxi	281183
2017	Tongchuan	CHN	Shaanxi	280657
2018	Zhongshan	CHN	Guangdong	278829
2019	Shiyan	CHN	Hubei	273786
2020	Tieli	CHN	Heilongjiang	265683
2021	Jining	CHN	Shandong	265248
2022	Wuhai	CHN	Inner Mongolia	264081
2023	Mianyang	CHN	Sichuan	262947
2024	Luzhou	CHN	Sichuan	262892
2025	Zunyi	CHN	Guizhou	261862
2026	Shizuishan	CHN	Ningxia	257862
2027	Neijiang	CHN	Sichuan	256012
2028	Tongliao	CHN	Inner Mongolia	255129
2029	Tieling	CHN	Liaoning	254842
2030	Wafangdian	CHN	Liaoning	251733
2031	Anqing	CHN	Anhui	250718
2032	Shaoyang	CHN	Hunan	247227
2033	Laiwu	CHN	Shandong	246833
2034	Chengde	CHN	Hebei	246799
2035	Tianshui	CHN	Gansu	244974
2036	Nanyang	CHN	Henan	243303
2037	Cangzhou	CHN	Hebei	242708
2038	Yibin	CHN	Sichuan	241019
2039	Huaiyin	CHN	Jiangsu	239675
2040	Dunhua	CHN	Jilin	235100
2041	Yanji	CHN	Jilin	230892
2042	Jiangmen	CHN	Guangdong	230587
2043	Tongling	CHN	Anhui	228017
2044	Suihua	CHN	Heilongjiang	227881
2045	Gongziling	CHN	Jilin	226569
2046	Xiantao	CHN	Hubei	222884
2047	Chaoyang	CHN	Liaoning	222394
2048	Ganzhou	CHN	Jiangxi	220129
2049	Huzhou	CHN	Zhejiang	218071
2050	Baicheng	CHN	Jilin	217987
2051	Shangzi	CHN	Heilongjiang	215373
2052	Yangjiang	CHN	Guangdong	215196
2053	Qitaihe	CHN	Heilongjiang	214957
2054	Gejiu	CHN	Yunnan	214294
2055	Jiangyin	CHN	Jiangsu	213659
2056	Hebi	CHN	Henan	212976
2057	Jiaxing	CHN	Zhejiang	211526
2058	Wuzhou	CHN	Guangxi	210452
2059	Meihekou	CHN	Jilin	209038
2060	Xuchang	CHN	Henan	208815
2061	Liaocheng	CHN	Shandong	207844
2062	Haicheng	CHN	Liaoning	205560
2063	Qianjiang	CHN	Hubei	205504
2064	Baiyin	CHN	Gansu	204970
2065	Beian	CHN	Heilongjiang	204899
2066	Yixing	CHN	Jiangsu	200824
2067	Laizhou	CHN	Shandong	198664
2068	Qaramay	CHN	Xinxiang	197602
2069	Acheng	CHN	Heilongjiang	197595
2070	Dezhou	CHN	Shandong	195485
2071	Nanping	CHN	Fujian	195064
2072	Zhaoqing	CHN	Guangdong	194784
2073	Beipiao	CHN	Liaoning	194301
2074	Fengcheng	CHN	Jiangxi	193784
2075	Fuyu	CHN	Jilin	192981
2076	Xinyang	CHN	Henan	192509
2077	Dongtai	CHN	Jiangsu	192247
2078	Yuci	CHN	Shanxi	191356
2079	Honghu	CHN	Hubei	190772
2080	Ezhou	CHN	Hubei	190123
2081	Heze	CHN	Shandong	189293
2082	Daxian	CHN	Sichuan	188101
2083	Linfen	CHN	Shanxi	187309
2084	Tianmen	CHN	Hubei	186332
2085	Yiyang	CHN	Hunan	185818
2086	Quanzhou	CHN	Fujian	185154
2087	Rizhao	CHN	Shandong	185048
2088	Deyang	CHN	Sichuan	182488
2089	Guangyuan	CHN	Sichuan	182241
2090	Changshu	CHN	Jiangsu	181805
2091	Zhangzhou	CHN	Fujian	181424
2092	Hailar	CHN	Inner Mongolia	180650
2093	Nanchong	CHN	Sichuan	180273
2094	Jiutai	CHN	Jilin	180130
2095	Zhaodong	CHN	Heilongjiang	179976
2096	Shaoxing	CHN	Zhejiang	179818
2097	Fuyang	CHN	Anhui	179572
2098	Maoming	CHN	Guangdong	178683
2099	Qujing	CHN	Yunnan	178669
2100	Ghulja	CHN	Xinxiang	177193
2101	Jiaohe	CHN	Jilin	176367
2102	Puyang	CHN	Henan	175988
2103	Huadian	CHN	Jilin	175873
2104	Jiangyou	CHN	Sichuan	175753
2105	Qashqar	CHN	Xinxiang	174570
2106	Anshun	CHN	Guizhou	174142
2107	Fuling	CHN	Sichuan	173878
2108	Xinyu	CHN	Jiangxi	173524
2109	Hanzhong	CHN	Shaanxi	169930
2110	Danyang	CHN	Jiangsu	169603
2111	Chenzhou	CHN	Hunan	169400
2112	Xiaogan	CHN	Hubei	166280
2113	Shangqiu	CHN	Henan	164880
2114	Zhuhai	CHN	Guangdong	164747
2115	Qingyuan	CHN	Guangdong	164641
2116	Aqsu	CHN	Xinxiang	164092
2117	Jining	CHN	Inner Mongolia	163552
2118	Xiaoshan	CHN	Zhejiang	162930
2119	Zaoyang	CHN	Hubei	162198
2120	Xinghua	CHN	Jiangsu	161910
2121	Hami	CHN	Xinxiang	161315
2122	Huizhou	CHN	Guangdong	161023
2123	Jinmen	CHN	Hubei	160794
2124	Sanming	CHN	Fujian	160691
2125	Ulanhot	CHN	Inner Mongolia	159538
2126	Korla	CHN	Xinxiang	159344
2127	Wanxian	CHN	Sichuan	156823
2128	Ruian	CHN	Zhejiang	156468
2129	Zhoushan	CHN	Zhejiang	156317
2130	Liangcheng	CHN	Shandong	156307
2131	Jiaozhou	CHN	Shandong	153364
2132	Taizhou	CHN	Jiangsu	152442
2133	Suzhou	CHN	Anhui	151862
2134	Yichun	CHN	Jiangxi	151585
2135	Taonan	CHN	Jilin	150168
2136	Pingdu	CHN	Shandong	150123
2137	Jian	CHN	Jiangxi	148583
2138	Longkou	CHN	Shandong	148362
2139	Langfang	CHN	Hebei	148105
2140	Zhoukou	CHN	Henan	146288
2141	Suining	CHN	Sichuan	146086
2142	Yulin	CHN	Guangxi	144467
2143	Jinhua	CHN	Zhejiang	144280
2144	Liuan	CHN	Anhui	144248
2145	Shuangcheng	CHN	Heilongjiang	142659
2146	Suizhou	CHN	Hubei	142302
2147	Ankang	CHN	Shaanxi	142170
2148	Weinan	CHN	Shaanxi	140169
2149	Longjing	CHN	Jilin	139417
2150	Daan	CHN	Jilin	138963
2151	Lengshuijiang	CHN	Hunan	137994
2152	Laiyang	CHN	Shandong	137080
2153	Xianning	CHN	Hubei	136811
2154	Dali	CHN	Yunnan	136554
2155	Anda	CHN	Heilongjiang	136446
2156	Jincheng	CHN	Shanxi	136396
2157	Longyan	CHN	Fujian	134481
2158	Xichang	CHN	Sichuan	134419
2159	Wendeng	CHN	Shandong	133910
2160	Hailun	CHN	Heilongjiang	133565
2161	Binzhou	CHN	Shandong	133555
2162	Linhe	CHN	Inner Mongolia	133183
2163	Wuwei	CHN	Gansu	133101
2164	Duyun	CHN	Guizhou	132971
2165	Mishan	CHN	Heilongjiang	132744
2166	Shangrao	CHN	Jiangxi	132455
2167	Changji	CHN	Xinxiang	132260
2168	Meixian	CHN	Guangdong	132156
2169	Yushu	CHN	Jilin	131861
2170	Tiefa	CHN	Liaoning	131807
2171	Huaian	CHN	Jiangsu	131149
2172	Leiyang	CHN	Hunan	130115
2173	Zalantun	CHN	Inner Mongolia	130031
2174	Weihai	CHN	Shandong	128888
2175	Loudi	CHN	Hunan	128418
2176	Qingzhou	CHN	Shandong	128258
2177	Qidong	CHN	Jiangsu	126872
2178	Huaihua	CHN	Hunan	126785
2179	Luohe	CHN	Henan	126438
2180	Chuzhou	CHN	Anhui	125341
2181	Kaiyuan	CHN	Liaoning	124219
2182	Linqing	CHN	Shandong	123958
2183	Chaohu	CHN	Anhui	123676
2184	Laohekou	CHN	Hubei	123366
2185	Dujiangyan	CHN	Sichuan	123357
2186	Zhumadian	CHN	Henan	123232
2187	Linchuan	CHN	Jiangxi	121949
2188	Jiaonan	CHN	Shandong	121397
2189	Sanmenxia	CHN	Henan	120523
2190	Heyuan	CHN	Guangdong	120101
2191	Manzhouli	CHN	Inner Mongolia	120023
2192	Lhasa	CHN	Tibet	120000
2193	Lianyuan	CHN	Hunan	118858
2194	Kuytun	CHN	Xinxiang	118553
2195	Puqi	CHN	Hubei	117264
2196	Hongjiang	CHN	Hunan	116188
2197	Qinzhou	CHN	Guangxi	114586
2198	Renqiu	CHN	Hebei	114256
2199	Yuyao	CHN	Zhejiang	114065
2200	Guigang	CHN	Guangxi	114025
2201	Kaili	CHN	Guizhou	113958
2202	Yanan	CHN	Shaanxi	113277
2203	Beihai	CHN	Guangxi	112673
2204	Xuangzhou	CHN	Anhui	112673
2205	Quzhou	CHN	Zhejiang	112373
2206	Yongan	CHN	Fujian	111762
2207	Zixing	CHN	Hunan	110048
2208	Liyang	CHN	Jiangsu	109520
2209	Yizheng	CHN	Jiangsu	109268
2210	Yumen	CHN	Gansu	109234
2211	Liling	CHN	Hunan	108504
2212	Yuncheng	CHN	Shanxi	108359
2213	Shanwei	CHN	Guangdong	107847
2214	Cixi	CHN	Zhejiang	107329
2215	Yuanjiang	CHN	Hunan	107004
2216	Bozhou	CHN	Anhui	106346
2217	Jinchang	CHN	Gansu	105287
2218	Fuan	CHN	Fujian	105265
2219	Suqian	CHN	Jiangsu	105021
2220	Shishou	CHN	Hubei	104571
2221	Hengshui	CHN	Hebei	104269
2222	Danjiangkou	CHN	Hubei	103211
2223	Fujin	CHN	Heilongjiang	103104
2224	Sanya	CHN	Hainan	102820
2225	Guangshui	CHN	Hubei	102770
2226	Huangshan	CHN	Anhui	102628
2227	Xingcheng	CHN	Liaoning	102384
2228	Zhucheng	CHN	Shandong	102134
2229	Kunshan	CHN	Jiangsu	102052
2230	Haining	CHN	Zhejiang	100478
2231	Pingliang	CHN	Gansu	99265
2232	Fuqing	CHN	Fujian	99193
2233	Xinzhou	CHN	Shanxi	98667
2234	Jieyang	CHN	Guangdong	98531
2235	Zhangjiagang	CHN	Jiangsu	97994
2236	Tong Xian	CHN	Peking	97168
2237	Yaan	CHN	Sichuan	95900
2238	Jinzhou	CHN	Liaoning	95761
2239	Emeishan	CHN	Sichuan	94000
2240	Enshi	CHN	Hubei	93056
2241	Bose	CHN	Guangxi	93009
2242	Yuzhou	CHN	Henan	92889
2243	Kaiyuan	CHN	Yunnan	91999
2244	Tumen	CHN	Jilin	91471
2245	Putian	CHN	Fujian	91030
2246	Linhai	CHN	Zhejiang	90870
2247	Xilin Hot	CHN	Inner Mongolia	90646
2248	Shaowu	CHN	Fujian	90286
2249	Junan	CHN	Shandong	90222
2250	Huaying	CHN	Sichuan	89400
2251	Pingyi	CHN	Shandong	89373
2252	Huangyan	CHN	Zhejiang	89288
2253	Bishkek	KGZ	Bishkek shaary	589400
2254	Osh	KGZ	Osh	222700
2255	Bikenibeu	KIR	South Tarawa	5055
2256	Bairiki	KIR	South Tarawa	2226
2257	Santafe de Bogota	COL	Santafe de Bogota	6260862
2258	Cali	COL	Valle	2077386
2259	Medellin	COL	Antioquia	1861265
2260	Barranquilla	COL	Atlantico	1223260
2261	Cartagena	COL	Bolivar	805757
2262	Cucuta	COL	Norte de Santander	606932
2263	Bucaramanga	COL	Santander	515555
2264	Ibague	COL	Tolima	393664
2265	Pereira	COL	Risaralda	381725
2266	Santa Marta	COL	Magdalena	359147
2267	Manizales	COL	Caldas	337580
2268	Bello	COL	Antioquia	333470
2269	Pasto	COL	Nariao	332396
2270	Neiva	COL	Huila	300052
2271	Soledad	COL	Atlantico	295058
2272	Armenia	COL	Quindio	288977
2273	Villavicencio	COL	Meta	273140
2274	Soacha	COL	Cundinamarca	272058
2275	Valledupar	COL	Cesar	263247
2276	Monteria	COL	Cordoba	248245
2277	Itagui	COL	Antioquia	228985
2278	Palmira	COL	Valle	226509
2279	Buenaventura	COL	Valle	224336
2280	Floridablanca	COL	Santander	221913
2281	Sincelejo	COL	Sucre	220704
2282	Popayan	COL	Cauca	200719
2283	Barrancabermeja	COL	Santander	178020
2284	Dos Quebradas	COL	Risaralda	159363
2285	Tulua	COL	Valle	152488
2286	Envigado	COL	Antioquia	135848
2287	Cartago	COL	Valle	125884
2288	Girardot	COL	Cundinamarca	110963
2289	Buga	COL	Valle	110699
2290	Tunja	COL	Boyaca	109740
2291	Florencia	COL	Caqueta	108574
2292	Maicao	COL	La Guajira	108053
2293	Sogamoso	COL	Boyaca	107728
2294	Giron	COL	Santander	90688
2295	Moroni	COM	Njazidja	36000
2296	Brazzaville	COG	Brazzaville	950000
2297	Pointe-Noire	COG	Kouilou	500000
2298	Kinshasa	COD	Kinshasa	5064000
2299	Lubumbashi	COD	Shaba	851381
2300	Mbuji-Mayi	COD	East Kasai	806475
2301	Kolwezi	COD	Shaba	417810
2302	Kisangani	COD	Haute-Zaire	417517
2303	Kananga	COD	West Kasai	393030
2304	Likasi	COD	Shaba	299118
2305	Bukavu	COD	South Kivu	201569
2306	Kikwit	COD	Bandundu	182142
2307	Tshikapa	COD	West Kasai	180860
2308	Matadi	COD	Bas-Zaire	172730
2309	Mbandaka	COD	Equateur	169841
2310	Mwene-Ditu	COD	East Kasai	137459
2311	Boma	COD	Bas-Zaire	135284
2312	Uvira	COD	South Kivu	115590
2313	Butembo	COD	North Kivu	109406
2314	Goma	COD	North Kivu	109094
2315	Kalemie	COD	Shaba	101309
2316	Bantam	CCK	Home Island	503
2317	West Island	CCK	West Island	167
2318	Pyongyang	PRK	Pyongyang-si	2484000
2319	Hamhung	PRK	Hamgyong N	709730
2320	Chongjin	PRK	Hamgyong P	582480
2321	Nampo	PRK	Nampo-si	566200
2322	Sinuiju	PRK	Pyongan P	326011
2323	Wonsan	PRK	Kangwon	300148
2324	Phyongsong	PRK	Pyongan N	272934
2325	Sariwon	PRK	Hwanghae P	254146
2326	Haeju	PRK	Hwanghae N	229172
2327	Kanggye	PRK	Chagang	223410
2328	Kimchaek	PRK	Hamgyong P	179000
2329	Hyesan	PRK	Yanggang	178020
2330	Kaesong	PRK	Kaesong-si	171500
2331	Seoul	KOR	Seoul	9981619
2332	Pusan	KOR	Pusan	3804522
2333	Inchon	KOR	Inchon	2559424
2334	Taegu	KOR	Taegu	2548568
2335	Taejon	KOR	Taejon	1425835
2336	Kwangju	KOR	Kwangju	1368341
2337	Ulsan	KOR	Kyongsangnam	1084891
2338	Songnam	KOR	Kyonggi	869094
2339	Puchon	KOR	Kyonggi	779412
2340	Suwon	KOR	Kyonggi	755550
2341	Anyang	KOR	Kyonggi	591106
2342	Chonju	KOR	Chollabuk	563153
2343	Chongju	KOR	Chungchongbuk	531376
2344	Koyang	KOR	Kyonggi	518282
2345	Ansan	KOR	Kyonggi	510314
2346	Pohang	KOR	Kyongsangbuk	508899
2347	Chang-won	KOR	Kyongsangnam	481694
2348	Masan	KOR	Kyongsangnam	441242
2349	Kwangmyong	KOR	Kyonggi	350914
2350	Chonan	KOR	Chungchongnam	330259
2351	Chinju	KOR	Kyongsangnam	329886
2352	Iksan	KOR	Chollabuk	322685
2353	Pyongtaek	KOR	Kyonggi	312927
2354	Kumi	KOR	Kyongsangbuk	311431
2355	Uijongbu	KOR	Kyonggi	276111
2356	Kyongju	KOR	Kyongsangbuk	272968
2357	Kunsan	KOR	Chollabuk	266569
2358	Cheju	KOR	Cheju	258511
2359	Kimhae	KOR	Kyongsangnam	256370
2360	Sunchon	KOR	Chollanam	249263
2361	Mokpo	KOR	Chollanam	247452
2362	Yong-in	KOR	Kyonggi	242643
2363	Wonju	KOR	Kang-won	237460
2364	Kunpo	KOR	Kyonggi	235233
2365	Chunchon	KOR	Kang-won	234528
2366	Namyangju	KOR	Kyonggi	229060
2367	Kangnung	KOR	Kang-won	220403
2368	Chungju	KOR	Chungchongbuk	205206
2369	Andong	KOR	Kyongsangbuk	188443
2370	Yosu	KOR	Chollanam	183596
2371	Kyongsan	KOR	Kyongsangbuk	173746
2372	Paju	KOR	Kyonggi	163379
2373	Yangsan	KOR	Kyongsangnam	163351
2374	Ichon	KOR	Kyonggi	155332
2375	Asan	KOR	Chungchongnam	154663
2376	Koje	KOR	Kyongsangnam	147562
2377	Kimchon	KOR	Kyongsangbuk	147027
2378	Nonsan	KOR	Chungchongnam	146619
2379	Kuri	KOR	Kyonggi	142173
2380	Chong-up	KOR	Chollabuk	139111
2381	Chechon	KOR	Chungchongbuk	137070
2382	Sosan	KOR	Chungchongnam	134746
2383	Shihung	KOR	Kyonggi	133443
2384	Tong-yong	KOR	Kyongsangnam	131717
2385	Kongju	KOR	Chungchongnam	131229
2386	Yongju	KOR	Kyongsangbuk	131097
2387	Chinhae	KOR	Kyongsangnam	125997
2388	Sangju	KOR	Kyongsangbuk	124116
2389	Poryong	KOR	Chungchongnam	122604
2390	Kwang-yang	KOR	Chollanam	122052
2391	Miryang	KOR	Kyongsangnam	121501
2392	Hanam	KOR	Kyonggi	115812
2393	Kimje	KOR	Chollabuk	115427
2394	Yongchon	KOR	Kyongsangbuk	113511
2395	Sachon	KOR	Kyongsangnam	113494
2396	Uiwang	KOR	Kyonggi	108788
2397	Naju	KOR	Chollanam	107831
2398	Namwon	KOR	Chollabuk	103544
2399	Tonghae	KOR	Kang-won	95472
2400	Mun-gyong	KOR	Kyongsangbuk	92239
2401	Athenai	GRC	Attika	772072
2402	Thessaloniki	GRC	Central Macedonia	383967
2403	Pireus	GRC	Attika	182671
2404	Patras	GRC	West Greece	153344
2405	Peristerion	GRC	Attika	137288
2406	Herakleion	GRC	Crete	116178
2407	Kallithea	GRC	Attika	114233
2408	Larisa	GRC	Thessalia	113090
2409	Zagreb	HRV	Grad Zagreb	706770
2410	Split	HRV	Split-Dalmatia	189388
2411	Rijeka	HRV	Primorje-Gorski Kota	167964
2412	Osijek	HRV	Osijek-Baranja	104761
2413	La Habana	CUB	La Habana	2256000
2414	Santiago de Cuba	CUB	Santiago de Cuba	433180
2415	Camaguey	CUB	Camaguey	298726
2416	Holguin	CUB	Holguin	249492
2417	Santa Clara	CUB	Villa Clara	207350
2418	Guantanamo	CUB	Guantanamo	205078
2419	Pinar del Rio	CUB	Pinar del Rio	142100
2420	Bayamo	CUB	Granma	141000
2421	Cienfuegos	CUB	Cienfuegos	132770
2422	Victoria de las Tunas	CUB	Las Tunas	132350
2423	Matanzas	CUB	Matanzas	123273
2424	Manzanillo	CUB	Granma	109350
2425	Sancti-Spiritus	CUB	Sancti-Spiritus	100751
2426	Ciego de Avila	CUB	Ciego de Avila	98505
2427	al-Salimiya	KWT	Hawalli	130215
2428	Jalib al-Shuyukh	KWT	Hawalli	102178
2429	Kuwait	KWT	al-Asima	28859
2430	Nicosia	CYP	Nicosia	195000
2431	Limassol	CYP	Limassol	154400
2432	Vientiane	LAO	Viangchan	531800
2433	Savannakhet	LAO	Savannakhet	96652
2434	Riga	LVA	Riika	764328
2435	Daugavpils	LVA	Daugavpils	114829
2436	Liepaja	LVA	Liepaja	89439
2437	Maseru	LSO	Maseru	297000
2438	Beirut	LBN	Beirut	1100000
2439	Tripoli	LBN	al-Shamal	240000
2440	Monrovia	LBR	Montserrado	850000
2441	Tripoli	LBY	Tripoli	1682000
2442	Bengasi	LBY	Bengasi	804000
2443	Misrata	LBY	Misrata	121669
2444	al-Zawiya	LBY	al-Zawiya	89338
2445	Schaan	LIE	Schaan	5346
2446	Vaduz	LIE	Vaduz	5043
2447	Vilnius	LTU	Vilna	577969
2448	Kaunas	LTU	Kaunas	412639
2449	Klaipeda	LTU	Klaipeda	202451
2450	Giauliai	LTU	Giauliai	146563
2451	Panevezys	LTU	Panevezys	133695
2452	Luxembourg [Luxemburg/Letzebuerg]	LUX	Luxembourg	80700
2453	El-Aaiun	ESH	El-Aaiun	169000
2454	Macao	MAC	Macau	437500
2455	Antananarivo	MDG	Antananarivo	675669
2456	Toamasina	MDG	Toamasina	127441
2457	Antsirabe	MDG	Antananarivo	120239
2458	Mahajanga	MDG	Mahajanga	100807
2459	Fianarantsoa	MDG	Fianarantsoa	99005
2460	Skopje	MKD	Skopje	444299
2461	Blantyre	MWI	Blantyre	478155
2462	Lilongwe	MWI	Lilongwe	435964
2463	Male	MDV	Maale	71000
2464	Kuala Lumpur	MYS	Wilayah Persekutuan	1297526
2465	Ipoh	MYS	Perak	382853
2466	Johor Baharu	MYS	Johor	328436
2467	Petaling Jaya	MYS	Selangor	254350
2468	Kelang	MYS	Selangor	243355
2469	Kuala Terengganu	MYS	Terengganu	228119
2470	Pinang	MYS	Pulau Pinang	219603
2471	Kota Bharu	MYS	Kelantan	219582
2472	Kuantan	MYS	Pahang	199484
2473	Taiping	MYS	Perak	183261
2474	Seremban	MYS	Negeri Sembilan	182869
2475	Kuching	MYS	Sarawak	148059
2476	Sibu	MYS	Sarawak	126381
2477	Sandakan	MYS	Sabah	125841
2478	Alor Setar	MYS	Kedah	124412
2479	Selayang Baru	MYS	Selangor	124228
2480	Sungai Petani	MYS	Kedah	114763
2481	Shah Alam	MYS	Selangor	102019
2482	Bamako	MLI	Bamako	809552
2483	Birkirkara	MLT	Outer Harbour	21445
2484	Valletta	MLT	Inner Harbour	7073
2485	Casablanca	MAR	Casablanca	2940623
2486	Rabat	MAR	Rabat-Sale-Zammour-Z	623457
2487	Marrakech	MAR	Marrakech-Tensift-Al	621914
2488	Fes	MAR	Fes-Boulemane	541162
2489	Tanger	MAR	Tanger-Tetouan	521735
2490	Sale	MAR	Rabat-Sale-Zammour-Z	504420
2491	Meknes	MAR	Meknes-Tafilalet	460000
2492	Oujda	MAR	Oriental	365382
2493	Kenitra	MAR	Gharb-Chrarda-Beni H	292600
2494	Tetouan	MAR	Tanger-Tetouan	277516
2495	Safi	MAR	Doukkala-Abda	262300
2496	Agadir	MAR	Souss Massa-Draa	155244
2497	Mohammedia	MAR	Casablanca	154706
2498	Khouribga	MAR	Chaouia-Ouardigha	152090
2499	Beni-Mellal	MAR	Tadla-Azilal	140212
2500	Temara	MAR	Rabat-Sale-Zammour-Z	126303
2501	El Jadida	MAR	Doukkala-Abda	119083
2502	Nador	MAR	Oriental	112450
2503	Ksar el Kebir	MAR	Tanger-Tetouan	107065
2504	Settat	MAR	Chaouia-Ouardigha	96200
2505	Taza	MAR	Taza-Al Hoceima-Taou	92700
2506	El Araich	MAR	Tanger-Tetouan	90400
2507	Dalap-Uliga-Darrit	MHL	Majuro	28000
2508	Fort-de-France	MTQ	Fort-de-France	94050
2509	Nouakchott	MRT	Nouakchott	667300
2510	Nouadhibou	MRT	Dakhlet Nouadhibou	97600
2511	Port-Louis	MUS	Port-Louis	138200
2512	Beau Bassin-Rose Hill	MUS	Plaines Wilhelms	100616
2513	Vacoas-Phoenix	MUS	Plaines Wilhelms	98464
2514	Mamoutzou	MYT	Mamoutzou	12000
2515	Ciudad de Mexico	MEX	Distrito Federal	8591309
2516	Guadalajara	MEX	Jalisco	1647720
2517	Ecatepec de Morelos	MEX	Mexico	1620303
2518	Puebla	MEX	Puebla	1346176
2519	Nezahualcoyotl	MEX	Mexico	1224924
2520	Juarez	MEX	Chihuahua	1217818
2521	Tijuana	MEX	Baja California	1212232
2522	Leon	MEX	Guanajuato	1133576
2523	Monterrey	MEX	Nuevo Leon	1108499
2524	Zapopan	MEX	Jalisco	1002239
2525	Naucalpan de Juarez	MEX	Mexico	857511
2526	Mexicali	MEX	Baja California	764902
2527	Culiacan	MEX	Sinaloa	744859
2528	Acapulco de Juarez	MEX	Guerrero	721011
2529	Tlalnepantla de Baz	MEX	Mexico	720755
2530	Merida	MEX	Yucatan	703324
2531	Chihuahua	MEX	Chihuahua	670208
2532	San Luis Potosi	MEX	San Luis Potosi	669353
2533	Guadalupe	MEX	Nuevo Leon	668780
2534	Toluca	MEX	Mexico	665617
2535	Aguascalientes	MEX	Aguascalientes	643360
2536	Queretaro	MEX	Queretaro de Arteaga	639839
2537	Morelia	MEX	Michoacan de Ocampo	619958
2538	Hermosillo	MEX	Sonora	608697
2539	Saltillo	MEX	Coahuila de Zaragoza	577352
2540	Torreon	MEX	Coahuila de Zaragoza	529093
2541	Centro (Villahermosa)	MEX	Tabasco	519873
2542	San Nicolas de los Garza	MEX	Nuevo Leon	495540
2543	Durango	MEX	Durango	490524
2544	Chimalhuacan	MEX	Mexico	490245
2545	Tlaquepaque	MEX	Jalisco	475472
2546	Atizapan de Zaragoza	MEX	Mexico	467262
2547	Veracruz	MEX	Veracruz	457119
2548	Cuautitlan Izcalli	MEX	Mexico	452976
2549	Irapuato	MEX	Guanajuato	440039
2550	Tuxtla Gutierrez	MEX	Chiapas	433544
2551	Tultitlan	MEX	Mexico	432411
2552	Reynosa	MEX	Tamaulipas	419776
2553	Benito Juarez	MEX	Quintana Roo	419276
2554	Matamoros	MEX	Tamaulipas	416428
2555	Xalapa	MEX	Veracruz	390058
2556	Celaya	MEX	Guanajuato	382140
2557	Mazatlan	MEX	Sinaloa	380265
2558	Ensenada	MEX	Baja California	369573
2559	Ahome	MEX	Sinaloa	358663
2560	Cajeme	MEX	Sonora	355679
2561	Cuernavaca	MEX	Morelos	337966
2562	Tonala	MEX	Jalisco	336109
2563	Valle de Chalco Solidaridad	MEX	Mexico	323113
2564	Nuevo Laredo	MEX	Tamaulipas	310277
2565	Tepic	MEX	Nayarit	305025
2566	Tampico	MEX	Tamaulipas	294789
2567	Ixtapaluca	MEX	Mexico	293160
2568	Apodaca	MEX	Nuevo Leon	282941
2569	Guasave	MEX	Sinaloa	277201
2570	Gomez Palacio	MEX	Durango	272806
2571	Tapachula	MEX	Chiapas	271141
2572	Nicolas Romero	MEX	Mexico	269393
2573	Coatzacoalcos	MEX	Veracruz	267037
2574	Uruapan	MEX	Michoacan de Ocampo	265211
2575	Victoria	MEX	Tamaulipas	262686
2576	Oaxaca de Juarez	MEX	Oaxaca	256848
2577	Coacalco de Berriozabal	MEX	Mexico	252270
2578	Pachuca de Soto	MEX	Hidalgo	244688
2579	General Escobedo	MEX	Nuevo Leon	232961
2580	Salamanca	MEX	Guanajuato	226864
2581	Santa Catarina	MEX	Nuevo Leon	226573
2582	Tehuacan	MEX	Puebla	225943
2583	Chalco	MEX	Mexico	222201
2584	Cardenas	MEX	Tabasco	216903
2585	Campeche	MEX	Campeche	216735
2586	La Paz	MEX	Mexico	213045
2587	Othon P. Blanco (Chetumal)	MEX	Quintana Roo	208014
2588	Texcoco	MEX	Mexico	203681
2589	La Paz	MEX	Baja California Sur	196708
2590	Metepec	MEX	Mexico	194265
2591	Monclova	MEX	Coahuila de Zaragoza	193657
2592	Huixquilucan	MEX	Mexico	193156
2593	Chilpancingo de los Bravo	MEX	Guerrero	192509
2594	Puerto Vallarta	MEX	Jalisco	183741
2595	Fresnillo	MEX	Zacatecas	182744
2596	Ciudad Madero	MEX	Tamaulipas	182012
2597	Soledad de Graciano Sanchez	MEX	San Luis Potosi	179956
2598	San Juan del Rio	MEX	Queretaro	179300
2599	San Felipe del Progreso	MEX	Mexico	177330
2600	Cordoba	MEX	Veracruz	176952
2601	Tecamac	MEX	Mexico	172410
2602	Ocosingo	MEX	Chiapas	171495
2603	Carmen	MEX	Campeche	171367
2604	Lazaro Cardenas	MEX	Michoacan de Ocampo	170878
2605	Jiutepec	MEX	Morelos	170428
2606	Papantla	MEX	Veracruz	170123
2607	Comalcalco	MEX	Tabasco	164640
2608	Zamora	MEX	Michoacan de Ocampo	161191
2609	Nogales	MEX	Sonora	159103
2610	Huimanguillo	MEX	Tabasco	158335
2611	Cuautla	MEX	Morelos	153132
2612	Minatitlan	MEX	Veracruz	152983
2613	Poza Rica de Hidalgo	MEX	Veracruz	152678
2614	Ciudad Valles	MEX	San Luis Potosi	146411
2615	Navolato	MEX	Sinaloa	145396
2616	San Luis Rio Colorado	MEX	Sonora	145276
2617	Penjamo	MEX	Guanajuato	143927
2618	San Andres Tuxtla	MEX	Veracruz	142251
2619	Guanajuato	MEX	Guanajuato	141215
2620	Navojoa	MEX	Sonora	140495
2621	Zitacuaro	MEX	Michoacan de Ocampo	137970
2622	Boca del Rio	MEX	Veracruz-Llave	135721
2623	Allende	MEX	Guanajuato	134645
2624	Silao	MEX	Guanajuato	134037
2625	Macuspana	MEX	Tabasco	133795
2626	San Juan Bautista Tuxtepec	MEX	Oaxaca	133675
2627	San Cristobal de las Casas	MEX	Chiapas	132317
2628	Valle de Santiago	MEX	Guanajuato	130557
2629	Guaymas	MEX	Sonora	130108
2630	Colima	MEX	Colima	129454
2631	Dolores Hidalgo	MEX	Guanajuato	128675
2632	Lagos de Moreno	MEX	Jalisco	127949
2633	Piedras Negras	MEX	Coahuila de Zaragoza	127898
2634	Altamira	MEX	Tamaulipas	127490
2635	Tuxpam	MEX	Veracruz	126475
2636	San Pedro Garza Garcia	MEX	Nuevo Leon	126147
2637	Cuauhtemoc	MEX	Chihuahua	124279
2638	Manzanillo	MEX	Colima	124014
2639	Iguala de la Independencia	MEX	Guerrero	123883
2640	Zacatecas	MEX	Zacatecas	123700
2641	Tlajomulco de Zuaiga	MEX	Jalisco	123220
2642	Tulancingo de Bravo	MEX	Hidalgo	121946
2643	Zinacantepec	MEX	Mexico	121715
2644	San Martin Texmelucan	MEX	Puebla	121093
2645	Tepatitlan de Morelos	MEX	Jalisco	118948
2646	Martinez de la Torre	MEX	Veracruz	118815
2647	Orizaba	MEX	Veracruz	118488
2648	Apatzingan	MEX	Michoacan de Ocampo	117849
2649	Atlixco	MEX	Puebla	117019
2650	Delicias	MEX	Chihuahua	116132
2651	Ixtlahuaca	MEX	Mexico	115548
2652	El Mante	MEX	Tamaulipas	112453
2653	Lerdo	MEX	Durango	112272
2654	Almoloya de Juarez	MEX	Mexico	110550
2655	Acambaro	MEX	Guanajuato	110487
2656	Acuaa	MEX	Coahuila de Zaragoza	110388
2657	Guadalupe	MEX	Zacatecas	108881
2658	Huejutla de Reyes	MEX	Hidalgo	108017
2659	Hidalgo	MEX	Michoacan de Ocampo	106198
2660	Los Cabos	MEX	Baja California Sur	105199
2661	Comitan de Dominguez	MEX	Chiapas	104986
2662	Cunduacan	MEX	Tabasco	104164
2663	Rio Bravo	MEX	Tamaulipas	103901
2664	Temapache	MEX	Veracruz	102824
2665	Chilapa de Alvarez	MEX	Guerrero	102716
2666	Hidalgo del Parral	MEX	Chihuahua	100881
2667	San Francisco del Rincon	MEX	Guanajuato	100149
2668	Taxco de Alarcon	MEX	Guerrero	99907
2669	Zumpango	MEX	Mexico	99781
2670	San Pedro Cholula	MEX	Puebla	99734
2671	Lerma	MEX	Mexico	99714
2672	Tecoman	MEX	Colima	99296
2673	Las Margaritas	MEX	Chiapas	97389
2674	Cosoleacaque	MEX	Veracruz	97199
2675	San Luis de la Paz	MEX	Guanajuato	96763
2676	Jose Azueta	MEX	Guerrero	95448
2677	Santiago Ixcuintla	MEX	Nayarit	95311
2678	San Felipe	MEX	Guanajuato	95305
2679	Tejupilco	MEX	Mexico	94934
2680	Tantoyuca	MEX	Veracruz	94709
2681	Salvatierra	MEX	Guanajuato	94322
2682	Tultepec	MEX	Mexico	93364
2683	Temixco	MEX	Morelos	92686
2684	Matamoros	MEX	Coahuila de Zaragoza	91858
2685	Panuco	MEX	Veracruz	90551
2686	El Fuerte	MEX	Sinaloa	89556
2687	Tierra Blanca	MEX	Veracruz	89143
2688	Weno	FSM	Chuuk	22000
2689	Palikir	FSM	Pohnpei	8600
2690	Chisinau	MDA	Chisinau	719900
2691	Tiraspol	MDA	Dnjestria	194300
2692	Balti	MDA	Balti	153400
2693	Bender (Tighina)	MDA	Bender (Tighina)	125700
2694	Monte-Carlo	MCO	A	13154
2695	Monaco-Ville	MCO	A	1234
2696	Ulan Bator	MNG	Ulaanbaatar	773700
2697	Plymouth	MSR	Plymouth	2000
2698	Maputo	MOZ	Maputo	1018938
2699	Matola	MOZ	Maputo	424662
2700	Beira	MOZ	Sofala	397368
2701	Nampula	MOZ	Nampula	303346
2702	Chimoio	MOZ	Manica	171056
2703	Nacala-Porto	MOZ	Nampula	158248
2704	Quelimane	MOZ	Zambezia	150116
2705	Mocuba	MOZ	Zambezia	124700
2706	Tete	MOZ	Tete	101984
2707	Xai-Xai	MOZ	Gaza	99442
2708	Gurue	MOZ	Zambezia	99300
2709	Maxixe	MOZ	Inhambane	93985
2710	Rangoon (Yangon)	MMR	Rangoon [Yangon]	3361700
2711	Mandalay	MMR	Mandalay	885300
2712	Moulmein (Mawlamyine)	MMR	Mon	307900
2713	Pegu (Bago)	MMR	Pegu [Bago]	190900
2714	Bassein (Pathein)	MMR	Irrawaddy [Ayeyarwad	183900
2715	Monywa	MMR	Sagaing	138600
2716	Sittwe (Akyab)	MMR	Rakhine	137600
2717	Taunggyi (Taunggye)	MMR	Shan	131500
2718	Meikhtila	MMR	Mandalay	129700
2719	Mergui (Myeik)	MMR	Tenasserim [Tanintha	122700
2720	Lashio (Lasho)	MMR	Shan	107600
2721	Prome (Pyay)	MMR	Pegu [Bago]	105700
2722	Henzada (Hinthada)	MMR	Irrawaddy [Ayeyarwad	104700
2723	Myingyan	MMR	Mandalay	103600
2724	Tavoy (Dawei)	MMR	Tenasserim [Tanintha	96800
2725	Pagakku (Pakokku)	MMR	Magwe [Magway]	94800
2726	Windhoek	NAM	Khomas	169000
2727	Yangor	NRU	A	4050
2728	Yaren	NRU	A	559
2729	Kathmandu	NPL	Central	591835
2730	Biratnagar	NPL	Eastern	157764
2731	Pokhara	NPL	Western	146318
2732	Lalitapur	NPL	Central	145847
2733	Birgunj	NPL	Central	90639
2734	Managua	NIC	Managua	959000
2735	Leon	NIC	Leon	123865
2736	Chinandega	NIC	Chinandega	97387
2737	Masaya	NIC	Masaya	88971
2738	Niamey	NER	Niamey	420000
2739	Zinder	NER	Zinder	120892
2740	Maradi	NER	Maradi	112965
2741	Lagos	NGA	Lagos	1518000
2742	Ibadan	NGA	Oyo & Osun	1432000
2743	Ogbomosho	NGA	Oyo & Osun	730000
2744	Kano	NGA	Kano & Jigawa	674100
2745	Oshogbo	NGA	Oyo & Osun	476800
2746	Ilorin	NGA	Kwara & Kogi	475800
2747	Abeokuta	NGA	Ogun	427400
2748	Port Harcourt	NGA	Rivers & Bayelsa	410000
2749	Zaria	NGA	Kaduna	379200
2750	Ilesha	NGA	Oyo & Osun	378400
2751	Onitsha	NGA	Anambra & Enugu & Eb	371900
2752	Iwo	NGA	Oyo & Osun	362000
2753	Ado-Ekiti	NGA	Ondo & Ekiti	359400
2754	Abuja	NGA	Federal Capital Dist	350100
2755	Kaduna	NGA	Kaduna	342200
2756	Mushin	NGA	Lagos	333200
2757	Maiduguri	NGA	Borno & Yobe	320000
2758	Enugu	NGA	Anambra & Enugu & Eb	316100
2759	Ede	NGA	Oyo & Osun	307100
2760	Aba	NGA	Imo & Abia	298900
2761	Ife	NGA	Oyo & Osun	296800
2762	Ila	NGA	Oyo & Osun	264000
2763	Oyo	NGA	Oyo & Osun	256400
2764	Ikerre	NGA	Ondo & Ekiti	244600
2765	Benin City	NGA	Edo & Delta	229400
2766	Iseyin	NGA	Oyo & Osun	217300
2767	Katsina	NGA	Katsina	206500
2768	Jos	NGA	Plateau & Nassarawa	206300
2769	Sokoto	NGA	Sokoto & Kebbi & Zam	204900
2770	Ilobu	NGA	Oyo & Osun	199000
2771	Offa	NGA	Kwara & Kogi	197200
2772	Ikorodu	NGA	Lagos	184900
2773	Ilawe-Ekiti	NGA	Ondo & Ekiti	184500
2774	Owo	NGA	Ondo & Ekiti	183500
2775	Ikirun	NGA	Oyo & Osun	181400
2776	Shaki	NGA	Oyo & Osun	174500
2777	Calabar	NGA	Cross River	174400
2778	Ondo	NGA	Ondo & Ekiti	173600
2779	Akure	NGA	Ondo & Ekiti	162300
2780	Gusau	NGA	Sokoto & Kebbi & Zam	158000
2781	Ijebu-Ode	NGA	Ogun	156400
2782	Effon-Alaiye	NGA	Oyo & Osun	153100
2783	Kumo	NGA	Bauchi & Gombe	148000
2784	Shomolu	NGA	Lagos	147700
2785	Oka-Akoko	NGA	Ondo & Ekiti	142900
2786	Ikare	NGA	Ondo & Ekiti	140800
2787	Sapele	NGA	Edo & Delta	139200
2788	Deba Habe	NGA	Bauchi & Gombe	138600
2789	Minna	NGA	Niger	136900
2790	Warri	NGA	Edo & Delta	126100
2791	Bida	NGA	Niger	125500
2792	Ikire	NGA	Oyo & Osun	123300
2793	Makurdi	NGA	Benue	123100
2794	Lafia	NGA	Plateau & Nassarawa	122500
2795	Inisa	NGA	Oyo & Osun	119800
2796	Shagamu	NGA	Ogun	117200
2797	Awka	NGA	Anambra & Enugu & Eb	111200
2798	Gombe	NGA	Bauchi & Gombe	107800
2799	Igboho	NGA	Oyo & Osun	106800
2800	Ejigbo	NGA	Oyo & Osun	105900
2801	Agege	NGA	Lagos	105000
2802	Ise-Ekiti	NGA	Ondo & Ekiti	103400
2803	Ugep	NGA	Cross River	102600
2804	Epe	NGA	Lagos	101000
2805	Alofi	NIU	A	682
2806	Kingston	NFK	A	800
2807	Oslo	NOR	Oslo	508726
2808	Bergen	NOR	Hordaland	230948
2809	Trondheim	NOR	Sor-Trondelag	150166
2810	Stavanger	NOR	Rogaland	108848
2811	Barum	NOR	Akershus	101340
2812	Abidjan	CIV	Abidjan	2500000
2813	Bouake	CIV	Bouake	329850
2814	Yamoussoukro	CIV	Yamoussoukro	130000
2815	Daloa	CIV	Daloa	121842
2816	Korhogo	CIV	Korhogo	109445
2817	al-Sib	OMN	Masqat	155000
2818	Salala	OMN	Zufar	131813
2819	Bawshar	OMN	Masqat	107500
2820	Suhar	OMN	al-Batina	90814
2821	Masqat	OMN	Masqat	51969
2822	Karachi	PAK	Sindh	9269265
2823	Lahore	PAK	Punjab	5063499
2824	Faisalabad	PAK	Punjab	1977246
2825	Rawalpindi	PAK	Punjab	1406214
2826	Multan	PAK	Punjab	1182441
2827	Hyderabad	PAK	Sindh	1151274
2828	Gujranwala	PAK	Punjab	1124749
2829	Peshawar	PAK	Nothwest Border Prov	988005
2830	Quetta	PAK	Baluchistan	560307
2831	Islamabad	PAK	Islamabad	524500
2832	Sargodha	PAK	Punjab	455360
2833	Sialkot	PAK	Punjab	417597
2834	Bahawalpur	PAK	Punjab	403408
2835	Sukkur	PAK	Sindh	329176
2836	Jhang	PAK	Punjab	292214
2837	Sheikhupura	PAK	Punjab	271875
2838	Larkana	PAK	Sindh	270366
2839	Gujrat	PAK	Punjab	250121
2840	Mardan	PAK	Nothwest Border Prov	244511
2841	Kasur	PAK	Punjab	241649
2842	Rahim Yar Khan	PAK	Punjab	228479
2843	Sahiwal	PAK	Punjab	207388
2844	Okara	PAK	Punjab	200901
2845	Wah	PAK	Punjab	198400
2846	Dera Ghazi Khan	PAK	Punjab	188100
2847	Mirpur Khas	PAK	Sind	184500
2848	Nawabshah	PAK	Sind	183100
2849	Mingora	PAK	Nothwest Border Prov	174500
2850	Chiniot	PAK	Punjab	169300
2851	Kamoke	PAK	Punjab	151000
2852	Mandi Burewala	PAK	Punjab	149900
2853	Jhelum	PAK	Punjab	145800
2854	Sadiqabad	PAK	Punjab	141500
2855	Jacobabad	PAK	Sind	137700
2856	Shikarpur	PAK	Sind	133300
2857	Khanewal	PAK	Punjab	133000
2858	Hafizabad	PAK	Punjab	130200
2859	Kohat	PAK	Nothwest Border Prov	125300
2860	Muzaffargarh	PAK	Punjab	121600
2861	Khanpur	PAK	Punjab	117800
2862	Gojra	PAK	Punjab	115000
2863	Bahawalnagar	PAK	Punjab	109600
2864	Muridke	PAK	Punjab	108600
2865	Pak Pattan	PAK	Punjab	107800
2866	Abottabad	PAK	Nothwest Border Prov	106000
2867	Tando Adam	PAK	Sind	103400
2868	Jaranwala	PAK	Punjab	103300
2869	Khairpur	PAK	Sind	102200
2870	Chishtian Mandi	PAK	Punjab	101700
2871	Daska	PAK	Punjab	101500
2872	Dadu	PAK	Sind	98600
2873	Mandi Bahauddin	PAK	Punjab	97300
2874	Ahmadpur East	PAK	Punjab	96000
2875	Kamalia	PAK	Punjab	95300
2876	Khuzdar	PAK	Baluchistan	93100
2877	Vihari	PAK	Punjab	92300
2878	Dera Ismail Khan	PAK	Nothwest Border Prov	90400
2879	Wazirabad	PAK	Punjab	89700
2880	Nowshera	PAK	Nothwest Border Prov	89400
2881	Koror	PLW	Koror	12000
2882	Ciudad de Panama	PAN	Panama	471373
2883	San Miguelito	PAN	San Miguelito	315382
2884	Port Moresby	PNG	National Capital Dis	247000
2885	Asuncion	PRY	Asuncion	557776
2886	Ciudad del Este	PRY	Alto Parana	133881
2887	San Lorenzo	PRY	Central	133395
2888	Lambare	PRY	Central	99681
2889	Fernando de la Mora	PRY	Central	95287
2890	Lima	PER	Lima	6464693
2891	Arequipa	PER	Arequipa	762000
2892	Trujillo	PER	La Libertad	652000
2893	Chiclayo	PER	Lambayeque	517000
2894	Callao	PER	Callao	424294
2895	Iquitos	PER	Loreto	367000
2896	Chimbote	PER	Ancash	336000
2897	Huancayo	PER	Junin	327000
2898	Piura	PER	Piura	325000
2899	Cusco	PER	Cusco	291000
2900	Pucallpa	PER	Ucayali	220866
2901	Tacna	PER	Tacna	215683
2902	Ica	PER	Ica	194820
2903	Sullana	PER	Piura	147361
2904	Juliaca	PER	Puno	142576
2905	Huanuco	PER	Huanuco	129688
2906	Ayacucho	PER	Ayacucho	118960
2907	Chincha Alta	PER	Ica	110016
2908	Cajamarca	PER	Cajamarca	108009
2909	Puno	PER	Puno	101578
2910	Ventanilla	PER	Callao	101056
2911	Castilla	PER	Piura	90642
2912	Adamstown	PCN	A	42
2913	Garapan	MNP	Saipan	9200
2914	Lisboa	PRT	Lisboa	563210
2915	Porto	PRT	Porto	273060
2916	Amadora	PRT	Lisboa	122106
2917	Coimbra	PRT	Coimbra	96100
2918	Braga	PRT	Braga	90535
2919	San Juan	PRI	San Juan	434374
2920	Bayamon	PRI	Bayamon	224044
2921	Ponce	PRI	Ponce	186475
2922	Carolina	PRI	Carolina	186076
2923	Caguas	PRI	Caguas	140502
2924	Arecibo	PRI	Arecibo	100131
2925	Guaynabo	PRI	Guaynabo	100053
2926	Mayaguez	PRI	Mayaguez	98434
2927	Toa Baja	PRI	Toa Baja	94085
2928	Warszawa	POL	Mazowieckie	1615369
2929	Lodz	POL	Lodzkie	800110
2930	Krakow	POL	Malopolskie	738150
2931	Wroclaw	POL	Dolnoslaskie	636765
2932	Poznan	POL	Wielkopolskie	576899
2933	Gdansk	POL	Pomorskie	458988
2934	Szczecin	POL	Zachodnio-Pomorskie	416988
2935	Bydgoszcz	POL	Kujawsko-Pomorskie	386855
2936	Lublin	POL	Lubelskie	356251
2937	Katowice	POL	Slaskie	345934
2938	Bialystok	POL	Podlaskie	283937
2939	Czestochowa	POL	Slaskie	257812
2940	Gdynia	POL	Pomorskie	253521
2941	Sosnowiec	POL	Slaskie	244102
2942	Radom	POL	Mazowieckie	232262
2943	Kielce	POL	Swietokrzyskie	212383
2944	Gliwice	POL	Slaskie	212164
2945	Torun	POL	Kujawsko-Pomorskie	206158
2946	Bytom	POL	Slaskie	205560
2947	Zabrze	POL	Slaskie	200177
2948	Bielsko-Biala	POL	Slaskie	180307
2949	Olsztyn	POL	Warminsko-Mazurskie	170904
2950	Rzeszow	POL	Podkarpackie	162049
2951	Ruda Slaska	POL	Slaskie	159665
2952	Rybnik	POL	Slaskie	144582
2953	Walbrzych	POL	Dolnoslaskie	136923
2954	Tychy	POL	Slaskie	133178
2955	Dabrowa Gornicza	POL	Slaskie	131037
2956	Plock	POL	Mazowieckie	131011
2957	Elblag	POL	Warminsko-Mazurskie	129782
2958	Opole	POL	Opolskie	129553
2959	Gorzow Wielkopolski	POL	Lubuskie	126019
2960	Wloclawek	POL	Kujawsko-Pomorskie	123373
2961	Chorzow	POL	Slaskie	121708
2962	Tarnow	POL	Malopolskie	121494
2963	Zielona Gora	POL	Lubuskie	118182
2964	Koszalin	POL	Zachodnio-Pomorskie	112375
2965	Legnica	POL	Dolnoslaskie	109335
2966	Kalisz	POL	Wielkopolskie	106641
2967	Grudziadz	POL	Kujawsko-Pomorskie	102434
2968	Slupsk	POL	Pomorskie	102370
2969	Jastrzebie-Zdroj	POL	Slaskie	102294
2970	Jaworzno	POL	Slaskie	97929
2971	Jelenia Gora	POL	Dolnoslaskie	93901
2972	Malabo	GNQ	Bioko	40000
2973	Doha	QAT	Doha	355000
2974	Paris	FRA	Ile-de-France	2125246
2975	Marseille	FRA	Provence-Alpes-Cote	798430
2976	Lyon	FRA	Rhone-Alpes	445452
2977	Toulouse	FRA	Midi-Pyrenees	390350
2978	Nice	FRA	Provence-Alpes-Cote	342738
2979	Nantes	FRA	Pays de la Loire	270251
2980	Strasbourg	FRA	Alsace	264115
2981	Montpellier	FRA	Languedoc-Roussillon	225392
2982	Bordeaux	FRA	Aquitaine	215363
2983	Rennes	FRA	Haute-Normandie	206229
2984	Le Havre	FRA	Champagne-Ardenne	190905
2985	Reims	FRA	Nord-Pas-de-Calais	187206
2986	Lille	FRA	Rhone-Alpes	184657
2987	St-Etienne	FRA	Bretagne	180210
2988	Toulon	FRA	Provence-Alpes-Cote	160639
2989	Grenoble	FRA	Rhone-Alpes	153317
2990	Angers	FRA	Pays de la Loire	151279
2991	Dijon	FRA	Bourgogne	149867
2992	Brest	FRA	Bretagne	149634
2993	Le Mans	FRA	Pays de la Loire	146105
2994	Clermont-Ferrand	FRA	Auvergne	137140
2995	Amiens	FRA	Picardie	135501
2996	Aix-en-Provence	FRA	Provence-Alpes-Cote	134222
2997	Limoges	FRA	Limousin	133968
2998	Nimes	FRA	Languedoc-Roussillon	133424
2999	Tours	FRA	Centre	132820
3000	Villeurbanne	FRA	Rhone-Alpes	124215
3001	Metz	FRA	Lorraine	123776
3002	Besancon	FRA	Franche-Comte	117733
3003	Caen	FRA	Basse-Normandie	113987
3004	Orleans	FRA	Centre	113126
3005	Mulhouse	FRA	Alsace	110359
3006	Rouen	FRA	Haute-Normandie	106592
3007	Boulogne-Billancourt	FRA	Ile-de-France	106367
3008	Perpignan	FRA	Languedoc-Roussillon	105115
3009	Nancy	FRA	Lorraine	103605
3010	Roubaix	FRA	Nord-Pas-de-Calais	96984
3011	Argenteuil	FRA	Ile-de-France	93961
3012	Tourcoing	FRA	Nord-Pas-de-Calais	93540
3013	Montreuil	FRA	Ile-de-France	90674
3014	Cayenne	GUF	Cayenne	50699
3015	Faaa	PYF	Tahiti	25888
3016	Papeete	PYF	Tahiti	25553
3017	Saint-Denis	REU	Saint-Denis	131480
3018	Bucuresti	ROM	Bukarest	2016131
3019	Iasi	ROM	Iasi	348070
3020	Constanta	ROM	Constanta	342264
3021	Cluj-Napoca	ROM	Cluj	332498
3022	Galati	ROM	Galati	330276
3023	Timisoara	ROM	Timis	324304
3024	Brasov	ROM	Brasov	314225
3025	Craiova	ROM	Dolj	313530
3026	Ploiesti	ROM	Prahova	251348
3027	Braila	ROM	Braila	233756
3028	Oradea	ROM	Bihor	222239
3029	Bacau	ROM	Bacau	209235
3030	Pitesti	ROM	Arges	187170
3031	Arad	ROM	Arad	184408
3032	Sibiu	ROM	Sibiu	169611
3033	Targu Mures	ROM	Mures	165153
3034	Baia Mare	ROM	Maramures	149665
3035	Buzau	ROM	Buzau	148372
3036	Satu Mare	ROM	Satu Mare	130059
3037	Botosani	ROM	Botosani	128730
3038	Piatra Neamt	ROM	Neamt	125070
3039	Ramnicu Valcea	ROM	Valcea	119741
3040	Suceava	ROM	Suceava	118549
3041	Drobeta-Turnu Severin	ROM	Mehedinti	117865
3042	Targoviste	ROM	Dambovita	98980
3043	Focsani	ROM	Vrancea	98979
3044	Targu Jiu	ROM	Gorj	98524
3045	Tulcea	ROM	Tulcea	96278
3046	Resita	ROM	Caras-Severin	93976
3047	Kigali	RWA	Kigali	286000
3048	Stockholm	SWE	Lisboa	750348
3049	Gothenburg [Goteborg]	SWE	West Gotanmaan lan	466990
3050	Malmo	SWE	Skane lan	259579
3051	Uppsala	SWE	Uppsala lan	189569
3052	Linkoping	SWE	East Gotanmaan lan	133168
3053	Vasteras	SWE	Vastmanlands lan	126328
3054	Orebro	SWE	Orebros lan	124207
3055	Norrkoping	SWE	East Gotanmaan lan	122199
3056	Helsingborg	SWE	Skane lan	117737
3057	Jonkoping	SWE	Jonkopings lan	117095
3058	Umea	SWE	Vasterbottens lan	104512
3059	Lund	SWE	Skane lan	98948
3060	Boras	SWE	West Gotanmaan lan	96883
3061	Sundsvall	SWE	Vasternorrlands lan	93126
3062	Gavle	SWE	Gavleborgs lan	90742
3063	Jamestown	SHN	Saint Helena	1500
3064	Basseterre	KNA	St George Basseterre	11600
3065	Castries	LCA	Castries	2301
3066	Kingstown	VCT	St George	17100
3067	Saint-Pierre	SPM	Saint-Pierre	5808
3068	Berlin	DEU	Berliini	3386667
3069	Hamburg	DEU	Hamburg	1704735
3070	Munich [Munchen]	DEU	Baijeri	1194560
3071	Koln	DEU	Nordrhein-Westfalen	962507
3072	Frankfurt am Main	DEU	Hessen	643821
3073	Essen	DEU	Nordrhein-Westfalen	599515
3074	Dortmund	DEU	Nordrhein-Westfalen	590213
3075	Stuttgart	DEU	Baden-Wurttemberg	582443
3076	Dusseldorf	DEU	Nordrhein-Westfalen	568855
3077	Bremen	DEU	Bremen	540330
3078	Duisburg	DEU	Nordrhein-Westfalen	519793
3079	Hannover	DEU	Niedersachsen	514718
3080	Leipzig	DEU	Saksi	489532
3081	Nurnberg	DEU	Baijeri	486628
3082	Dresden	DEU	Saksi	476668
3083	Bochum	DEU	Nordrhein-Westfalen	392830
3084	Wuppertal	DEU	Nordrhein-Westfalen	368993
3085	Bielefeld	DEU	Nordrhein-Westfalen	321125
3086	Mannheim	DEU	Baden-Wurttemberg	307730
3087	Bonn	DEU	Nordrhein-Westfalen	301048
3088	Gelsenkirchen	DEU	Nordrhein-Westfalen	281979
3089	Karlsruhe	DEU	Baden-Wurttemberg	277204
3090	Wiesbaden	DEU	Hessen	268716
3091	Munster	DEU	Nordrhein-Westfalen	264670
3092	Monchengladbach	DEU	Nordrhein-Westfalen	263697
3093	Chemnitz	DEU	Saksi	263222
3094	Augsburg	DEU	Baijeri	254867
3095	Halle/Saale	DEU	Anhalt Sachsen	254360
3096	Braunschweig	DEU	Niedersachsen	246322
3097	Aachen	DEU	Nordrhein-Westfalen	243825
3098	Krefeld	DEU	Nordrhein-Westfalen	241769
3099	Magdeburg	DEU	Anhalt Sachsen	235073
3100	Kiel	DEU	Schleswig-Holstein	233795
3101	Oberhausen	DEU	Nordrhein-Westfalen	222349
3102	Lubeck	DEU	Schleswig-Holstein	213326
3103	Hagen	DEU	Nordrhein-Westfalen	205201
3104	Rostock	DEU	Mecklenburg-Vorpomme	203279
3105	Freiburg im Breisgau	DEU	Baden-Wurttemberg	202455
3106	Erfurt	DEU	Thuringen	201267
3107	Kassel	DEU	Hessen	196211
3108	Saarbrucken	DEU	Saarland	183836
3109	Mainz	DEU	Rheinland-Pfalz	183134
3110	Hamm	DEU	Nordrhein-Westfalen	181804
3111	Herne	DEU	Nordrhein-Westfalen	175661
3112	Mulheim an der Ruhr	DEU	Nordrhein-Westfalen	173895
3113	Solingen	DEU	Nordrhein-Westfalen	165583
3114	Osnabruck	DEU	Niedersachsen	164539
3115	Ludwigshafen am Rhein	DEU	Rheinland-Pfalz	163771
3116	Leverkusen	DEU	Nordrhein-Westfalen	160841
3117	Oldenburg	DEU	Niedersachsen	154125
3118	Neuss	DEU	Nordrhein-Westfalen	149702
3119	Heidelberg	DEU	Baden-Wurttemberg	139672
3120	Darmstadt	DEU	Hessen	137776
3121	Paderborn	DEU	Nordrhein-Westfalen	137647
3122	Potsdam	DEU	Brandenburg	128983
3123	Wurzburg	DEU	Baijeri	127350
3124	Regensburg	DEU	Baijeri	125236
3125	Recklinghausen	DEU	Nordrhein-Westfalen	125022
3126	Gottingen	DEU	Niedersachsen	124775
3127	Bremerhaven	DEU	Bremen	122735
3128	Wolfsburg	DEU	Niedersachsen	121954
3129	Bottrop	DEU	Nordrhein-Westfalen	121097
3130	Remscheid	DEU	Nordrhein-Westfalen	120125
3131	Heilbronn	DEU	Baden-Wurttemberg	119526
3132	Pforzheim	DEU	Baden-Wurttemberg	117227
3133	Offenbach am Main	DEU	Hessen	116627
3134	Ulm	DEU	Baden-Wurttemberg	116103
3135	Ingolstadt	DEU	Baijeri	114826
3136	Gera	DEU	Thuringen	114718
3137	Salzgitter	DEU	Niedersachsen	112934
3138	Cottbus	DEU	Brandenburg	110894
3139	Reutlingen	DEU	Baden-Wurttemberg	110343
3140	Furth	DEU	Baijeri	109771
3141	Siegen	DEU	Nordrhein-Westfalen	109225
3142	Koblenz	DEU	Rheinland-Pfalz	108003
3143	Moers	DEU	Nordrhein-Westfalen	106837
3144	Bergisch Gladbach	DEU	Nordrhein-Westfalen	106150
3145	Zwickau	DEU	Saksi	104146
3146	Hildesheim	DEU	Niedersachsen	104013
3147	Witten	DEU	Nordrhein-Westfalen	103384
3148	Schwerin	DEU	Mecklenburg-Vorpomme	102878
3149	Erlangen	DEU	Baijeri	100750
3150	Kaiserslautern	DEU	Rheinland-Pfalz	100025
3151	Trier	DEU	Rheinland-Pfalz	99891
3152	Jena	DEU	Thuringen	99779
3153	Iserlohn	DEU	Nordrhein-Westfalen	99474
3154	Gutersloh	DEU	Nordrhein-Westfalen	95028
3155	Marl	DEU	Nordrhein-Westfalen	93735
3156	Lunen	DEU	Nordrhein-Westfalen	92044
3157	Duren	DEU	Nordrhein-Westfalen	91092
3158	Ratingen	DEU	Nordrhein-Westfalen	90951
3159	Velbert	DEU	Nordrhein-Westfalen	89881
3160	Esslingen am Neckar	DEU	Baden-Wurttemberg	89667
3161	Honiara	SLB	Honiara	50100
3162	Lusaka	ZMB	Lusaka	1317000
3163	Ndola	ZMB	Copperbelt	329200
3164	Kitwe	ZMB	Copperbelt	288600
3165	Kabwe	ZMB	Central	154300
3166	Chingola	ZMB	Copperbelt	142400
3167	Mufulira	ZMB	Copperbelt	123900
3168	Luanshya	ZMB	Copperbelt	118100
3169	Apia	WSM	Upolu	35900
3170	Serravalle	SMR	Serravalle/Dogano	4802
3171	San Marino	SMR	San Marino	2294
3172	Sao Tome	STP	Aqua Grande	49541
3173	Riyadh	SAU	Riyadh	3324000
3174	Jedda	SAU	Mekka	2046300
3175	Mekka	SAU	Mekka	965700
3176	Medina	SAU	Medina	608300
3177	al-Dammam	SAU	al-Sharqiya	482300
3178	al-Taif	SAU	Mekka	416100
3179	Tabuk	SAU	Tabuk	292600
3180	Burayda	SAU	al-Qasim	248600
3181	al-Hufuf	SAU	al-Sharqiya	225800
3182	al-Mubarraz	SAU	al-Sharqiya	219100
3183	Khamis Mushayt	SAU	Asir	217900
3184	Hail	SAU	Hail	176800
3185	al-Kharj	SAU	Riad	152100
3186	al-Khubar	SAU	al-Sharqiya	141700
3187	Jubayl	SAU	al-Sharqiya	140800
3188	Hafar al-Batin	SAU	al-Sharqiya	137800
3189	al-Tuqba	SAU	al-Sharqiya	125700
3190	Yanbu	SAU	Medina	119800
3191	Abha	SAU	Asir	112300
3192	Araar	SAU	al-Khudud al-Samaliy	108100
3193	al-Qatif	SAU	al-Sharqiya	98900
3194	al-Hawiya	SAU	Mekka	93900
3195	Unayza	SAU	Qasim	91100
3196	Najran	SAU	Najran	91000
3197	Pikine	SEN	Cap-Vert	855287
3198	Dakar	SEN	Cap-Vert	785071
3199	Thies	SEN	Thies	248000
3200	Kaolack	SEN	Kaolack	199000
3201	Ziguinchor	SEN	Ziguinchor	192000
3202	Rufisque	SEN	Cap-Vert	150000
3203	Saint-Louis	SEN	Saint-Louis	132400
3204	Mbour	SEN	Thies	109300
3205	Diourbel	SEN	Diourbel	99400
3206	Victoria	SYC	Mahe	41000
3207	Freetown	SLE	Western	850000
3208	Singapore	SGP	A	4017733
3209	Bratislava	SVK	Bratislava	448292
3210	Kodice	SVK	Vychodne Slovensko	241874
3211	Predov	SVK	Vychodne Slovensko	93977
3212	Ljubljana	SVN	Osrednjeslovenska	270986
3213	Maribor	SVN	Podravska	115532
3214	Mogadishu	SOM	Banaadir	997000
3215	Hargeysa	SOM	Woqooyi Galbeed	90000
3216	Kismaayo	SOM	Jubbada Hoose	90000
3217	Colombo	LKA	Western	645000
3218	Dehiwala	LKA	Western	203000
3219	Moratuwa	LKA	Western	190000
3220	Jaffna	LKA	Northern	149000
3221	Kandy	LKA	Central	140000
3222	Sri Jayawardenepura Kotte	LKA	Western	118000
3223	Negombo	LKA	Western	100000
3224	Omdurman	SDN	Khartum	1271403
3225	Khartum	SDN	Khartum	947483
3226	Sharq al-Nil	SDN	Khartum	700887
3227	Port Sudan	SDN	al-Bahr al-Ahmar	308195
3228	Kassala	SDN	Kassala	234622
3229	Obeid	SDN	Kurdufan al-Shamaliy	229425
3230	Nyala	SDN	Darfur al-Janubiya	227183
3231	Wad Madani	SDN	al-Jazira	211362
3232	al-Qadarif	SDN	al-Qadarif	191164
3233	Kusti	SDN	al-Bahr al-Abyad	173599
3234	al-Fashir	SDN	Darfur al-Shamaliya	141884
3235	Juba	SDN	Bahr al-Jabal	114980
3236	Helsinki [Helsingfors]	FIN	Newmaa	555474
3237	Espoo	FIN	Newmaa	213271
3238	Tampere	FIN	Pirkanmaa	195468
3239	Vantaa	FIN	Newmaa	178471
3240	Turku [Abo]	FIN	Varsinais-Suomi	172561
3241	Oulu	FIN	Pohjois-Pohjanmaa	120753
3242	Lahti	FIN	Paijat-Hame	96921
3243	Paramaribo	SUR	Paramaribo	112000
3244	Mbabane	SWZ	Hhohho	61000
3245	Zurich	CHE	Zurich	336800
3246	Geneve	CHE	Geneve	173500
3247	Basel	CHE	Basel-Stadt	166700
3248	Bern	CHE	Bern	122700
3249	Lausanne	CHE	Vaud	114500
3250	Damascus	SYR	Damascus	1347000
3251	Aleppo	SYR	Aleppo	1261983
3252	Hims	SYR	Hims	507404
3253	Hama	SYR	Hama	343361
3254	Latakia	SYR	Latakia	264563
3255	al-Qamishliya	SYR	al-Hasaka	144286
3256	Dayr al-Zawr	SYR	Dayr al-Zawr	140459
3257	Jaramana	SYR	Damaskos	138469
3258	Duma	SYR	Damaskos	131158
3259	al-Raqqa	SYR	al-Raqqa	108020
3260	Idlib	SYR	Idlib	91081
3261	Dushanbe	TJK	Karotegin	524000
3262	Khujand	TJK	Khujand	161500
3263	Taipei	TWN	Taipei	2641312
3264	Kaohsiung	TWN	Kaohsiung	1475505
3265	Taichung	TWN	Taichung	940589
3266	Tainan	TWN	Tainan	728060
3267	Panchiao	TWN	Taipei	523850
3268	Chungho	TWN	Taipei	392176
3269	Keelung (Chilung)	TWN	Keelung	385201
3270	Sanchung	TWN	Taipei	380084
3271	Hsinchuang	TWN	Taipei	365048
3272	Hsinchu	TWN	Hsinchu	361958
3273	Chungli	TWN	Taoyuan	318649
3274	Fengshan	TWN	Kaohsiung	318562
3275	Taoyuan	TWN	Taoyuan	316438
3276	Chiayi	TWN	Chiayi	265109
3277	Hsintien	TWN	Taipei	263603
3278	Changhwa	TWN	Changhwa	227715
3279	Yungho	TWN	Taipei	227700
3280	Tucheng	TWN	Taipei	224897
3281	Pingtung	TWN	Pingtung	214727
3282	Yungkang	TWN	Tainan	193005
3283	Pingchen	TWN	Taoyuan	188344
3284	Tali	TWN	Taichung	171940
3285	Taiping	TWN		165524
3286	Pate	TWN	Taoyuan	161700
3287	Fengyuan	TWN	Taichung	161032
3288	Luchou	TWN	Taipei	160516
3289	Hsichuh	TWN	Taipei	154976
3290	Shulin	TWN	Taipei	151260
3291	Yuanlin	TWN	Changhwa	126402
3292	Yangmei	TWN	Taoyuan	126323
3293	Taliao	TWN		115897
3294	Kueishan	TWN		112195
3295	Tanshui	TWN	Taipei	111882
3296	Taitung	TWN	Taitung	111039
3297	Hualien	TWN	Hualien	108407
3298	Nantou	TWN	Nantou	104723
3299	Lungtan	TWN	Taipei	103088
3300	Touliu	TWN	Yunlin	98900
3301	Tsaotun	TWN	Nantou	96800
3302	Kangshan	TWN	Kaohsiung	92200
3303	Ilan	TWN	Ilan	92000
3304	Miaoli	TWN	Miaoli	90000
3305	Dar es Salaam	TZA	Dar es Salaam	1747000
3306	Dodoma	TZA	Dodoma	189000
3307	Mwanza	TZA	Mwanza	172300
3308	Zanzibar	TZA	Zanzibar West	157634
3309	Tanga	TZA	Tanga	137400
3310	Mbeya	TZA	Mbeya	130800
3311	Morogoro	TZA	Morogoro	117800
3312	Arusha	TZA	Arusha	102500
3313	Moshi	TZA	Kilimanjaro	96800
3314	Tabora	TZA	Tabora	92800
3315	Kobenhavn	DNK	Kobenhavn	495699
3316	Arhus	DNK	Arhus	284846
3317	Odense	DNK	Fyn	183912
3318	Aalborg	DNK	Nordjylland	161161
3319	Frederiksberg	DNK	Frederiksberg	90327
3320	Bangkok	THA	Bangkok	6320174
3321	Nonthaburi	THA	Nonthaburi	292100
3322	Nakhon Ratchasima	THA	Nakhon Ratchasima	181400
3323	Chiang Mai	THA	Chiang Mai	171100
3324	Udon Thani	THA	Udon Thani	158100
3325	Hat Yai	THA	Songkhla	148632
3326	Khon Kaen	THA	Khon Kaen	126500
3327	Pak Kret	THA	Nonthaburi	126055
3328	Nakhon Sawan	THA	Nakhon Sawan	123800
3329	Ubon Ratchathani	THA	Ubon Ratchathani	116300
3330	Songkhla	THA	Songkhla	94900
3331	Nakhon Pathom	THA	Nakhon Pathom	94100
3332	Lome	TGO	Maritime	375000
3333	Fakaofo	TKL	Fakaofo	300
3334	Nukualofa	TON	Tongatapu	22400
3335	Chaguanas	TTO	Caroni	56601
3336	Port-of-Spain	TTO	Port-of-Spain	43396
3337	NDjamena	TCD	Chari-Baguirmi	530965
3338	Moundou	TCD	Logone Occidental	99500
3339	Praha	CZE	Hlavni mesto Praha	1181126
3340	Brno	CZE	Jizni Morava	381862
3341	Ostrava	CZE	Severni Morava	320041
3342	Plzen	CZE	Zapadni Cechy	166759
3343	Olomouc	CZE	Severni Morava	102702
3344	Liberec	CZE	Severni Cechy	99155
3345	Ceske Budejovice	CZE	Jizni Cechy	98186
3346	Hradec Kralove	CZE	Vychodni Cechy	98080
3347	Usti nad Labem	CZE	Severni Cechy	95491
3348	Pardubice	CZE	Vychodni Cechy	91309
3349	Tunis	TUN	Tunis	690600
3350	Sfax	TUN	Sfax	257800
3351	Ariana	TUN	Ariana	197000
3352	Ettadhamen	TUN	Ariana	178600
3353	Sousse	TUN	Sousse	145900
3354	Kairouan	TUN	Kairouan	113100
3355	Biserta	TUN	Biserta	108900
3356	Gabes	TUN	Gabes	106600
3357	Istanbul	TUR	Istanbul	8787958
3358	Ankara	TUR	Ankara	3038159
3359	Izmir	TUR	Izmir	2130359
3360	Adana	TUR	Adana	1131198
3361	Bursa	TUR	Bursa	1095842
3362	Gaziantep	TUR	Gaziantep	789056
3363	Konya	TUR	Konya	628364
3364	Mersin (Icel)	TUR	Icel	587212
3365	Antalya	TUR	Antalya	564914
3366	Diyarbakir	TUR	Diyarbakir	479884
3367	Kayseri	TUR	Kayseri	475657
3368	Eskisehir	TUR	Eskisehir	470781
3369	Sanliurfa	TUR	Sanliurfa	405905
3370	Samsun	TUR	Samsun	339871
3371	Malatya	TUR	Malatya	330312
3372	Gebze	TUR	Kocaeli	264170
3373	Denizli	TUR	Denizli	253848
3374	Sivas	TUR	Sivas	246642
3375	Erzurum	TUR	Erzurum	246535
3376	Tarsus	TUR	Adana	246206
3377	Kahramanmaras	TUR	Kahramanmaras	245772
3378	Elazig	TUR	Elazig	228815
3379	Van	TUR	Van	219319
3380	Sultanbeyli	TUR	Istanbul	211068
3381	Izmit (Kocaeli)	TUR	Kocaeli	210068
3382	Manisa	TUR	Manisa	207148
3383	Batman	TUR	Batman	203793
3384	Balikesir	TUR	Balikesir	196382
3385	Sakarya (Adapazari)	TUR	Sakarya	190641
3386	Iskenderun	TUR	Hatay	153022
3387	Osmaniye	TUR	Osmaniye	146003
3388	Corum	TUR	Corum	145495
3389	Kutahya	TUR	Kutahya	144761
3390	Hatay (Antakya)	TUR	Hatay	143982
3391	Kirikkale	TUR	Kirikkale	142044
3392	Adiyaman	TUR	Adiyaman	141529
3393	Trabzon	TUR	Trabzon	138234
3394	Ordu	TUR	Ordu	133642
3395	Aydin	TUR	Aydin	128651
3396	Usak	TUR	Usak	128162
3397	Edirne	TUR	Edirne	123383
3398	Corlu	TUR	Tekirdag	123300
3399	Isparta	TUR	Isparta	121911
3400	Karabuk	TUR	Karabuk	118285
3401	Kilis	TUR	Kilis	118245
3402	Alanya	TUR	Antalya	117300
3403	Kiziltepe	TUR	Mardin	112000
3404	Zonguldak	TUR	Zonguldak	111542
3405	Siirt	TUR	Siirt	107100
3406	Viransehir	TUR	Sanliurfa	106400
3407	Tekirdag	TUR	Tekirdag	106077
3408	Karaman	TUR	Karaman	104200
3409	Afyon	TUR	Afyon	103984
3410	Aksaray	TUR	Aksaray	102681
3411	Ceyhan	TUR	Adana	102412
3412	Erzincan	TUR	Erzincan	102304
3413	Bismil	TUR	Diyarbakir	101400
3414	Nazilli	TUR	Aydin	99900
3415	Tokat	TUR	Tokat	99500
3416	Kars	TUR	Kars	93000
3417	Inegol	TUR	Bursa	90500
3418	Bandirma	TUR	Balikesir	90200
3419	Ashgabat	TKM	Ahal	540600
3420	Charjew	TKM	Lebap	189200
3421	Dashhowuz	TKM	Dashhowuz	141800
3422	Mary	TKM	Mary	101000
3423	Cockburn Town	TCA	Grand Turk	4800
3424	Funafuti	TUV	Funafuti	4600
3425	Kampala	UGA	Central	890800
3426	Kyiv	UKR	Kiova	2624000
3427	Harkova [Harkiv]	UKR	Harkova	1500000
3428	Dnipropetrovsk	UKR	Dnipropetrovsk	1103000
3429	Donetsk	UKR	Donetsk	1050000
3430	Odesa	UKR	Odesa	1011000
3431	Zaporizzja	UKR	Zaporizzja	848000
3432	Lviv	UKR	Lviv	788000
3433	Kryvyi Rig	UKR	Dnipropetrovsk	703000
3434	Mykolajiv	UKR	Mykolajiv	508000
3435	Mariupol	UKR	Donetsk	490000
3436	Lugansk	UKR	Lugansk	469000
3437	Vinnytsja	UKR	Vinnytsja	391000
3438	Makijivka	UKR	Donetsk	384000
3439	Herson	UKR	Herson	353000
3440	Sevastopol	UKR	Krim	348000
3441	Simferopol	UKR	Krim	339000
3442	Pultava [Poltava]	UKR	Pultava	313000
3443	Tdernigiv	UKR	Tdernigiv	313000
3444	Tderkasy	UKR	Tderkasy	309000
3445	Gorlivka	UKR	Donetsk	299000
3446	Zytomyr	UKR	Zytomyr	297000
3447	Sumy	UKR	Sumy	294000
3448	Dniprodzerzynsk	UKR	Dnipropetrovsk	270000
3449	Kirovograd	UKR	Kirovograd	265000
3450	Hmelnytskyi	UKR	Hmelnytskyi	262000
3451	Tdernivtsi	UKR	Tdernivtsi	259000
3452	Rivne	UKR	Rivne	245000
3453	Krementduk	UKR	Pultava	239000
3454	Ivano-Frankivsk	UKR	Ivano-Frankivsk	237000
3455	Ternopil	UKR	Ternopil	236000
3456	Lutsk	UKR	Volynia	217000
3457	Bila Tserkva	UKR	Kiova	215000
3458	Kramatorsk	UKR	Donetsk	186000
3459	Melitopol	UKR	Zaporizzja	169000
3460	Kertd	UKR	Krim	162000
3461	Nikopol	UKR	Dnipropetrovsk	149000
3462	Berdjansk	UKR	Zaporizzja	130000
3463	Pavlograd	UKR	Dnipropetrovsk	127000
3464	Sjeverodonetsk	UKR	Lugansk	127000
3465	Slovjansk	UKR	Donetsk	127000
3466	Uzgorod	UKR	Taka-Karpatia	127000
3467	Altdevsk	UKR	Lugansk	119000
3468	Lysytdansk	UKR	Lugansk	116000
3469	Jevpatorija	UKR	Krim	112000
3470	Kamjanets-Podilskyi	UKR	Hmelnytskyi	109000
3471	Jenakijeve	UKR	Donetsk	105000
3472	Krasnyi Lutd	UKR	Lugansk	101000
3473	Stahanov	UKR	Lugansk	101000
3474	Oleksandrija	UKR	Kirovograd	99000
3475	Konotop	UKR	Sumy	96000
3476	Kostjantynivka	UKR	Donetsk	95000
3477	Berdytdiv	UKR	Zytomyr	90000
3478	Izmajil	UKR	Odesa	90000
3479	Gostka	UKR	Sumy	90000
3480	Uman	UKR	Tderkasy	90000
3481	Brovary	UKR	Kiova	89000
3482	Mukatdeve	UKR	Taka-Karpatia	89000
3483	Budapest	HUN	Budapest	1811552
3484	Debrecen	HUN	Hajdu-Bihar	203648
3485	Miskolc	HUN	Borsod-Abauj-Zemplen	172357
3486	Szeged	HUN	Csongrad	158158
3487	Pecs	HUN	Baranya	157332
3488	Gyor	HUN	Gyor-Moson-Sopron	127119
3489	Nyiregyhaza	HUN	Szabolcs-Szatmar-Ber	112419
3490	Kecskemet	HUN	Bacs-Kiskun	105606
3491	Szekesfehervar	HUN	Fejer	105119
3492	Montevideo	URY	Montevideo	1236000
3493	Noumea	NCL	A	76293
3494	Auckland	NZL	Auckland	381800
3495	Christchurch	NZL	Canterbury	324200
3496	Manukau	NZL	Auckland	281800
3497	North Shore	NZL	Auckland	187700
3498	Waitakere	NZL	Auckland	170600
3499	Wellington	NZL	Wellington	166700
3500	Dunedin	NZL	Dunedin	119600
3501	Hamilton	NZL	Hamilton	117100
3502	Lower Hutt	NZL	Wellington	98100
3503	Toskent	UZB	Toskent Shahri	2117500
3504	Namangan	UZB	Namangan	370500
3505	Samarkand	UZB	Samarkand	361800
3506	Andijon	UZB	Andijon	318600
3507	Buhoro	UZB	Buhoro	237100
3508	Karsi	UZB	Qashqadaryo	194100
3509	Nukus	UZB	Karakalpakistan	194100
3510	KuKon	UZB	Fargona	190100
3511	Fargona	UZB	Fargona	180500
3512	Circik	UZB	Toskent	146400
3513	Margilon	UZB	Fargona	140800
3514	Urgenc	UZB	Khorazm	138900
3515	Angren	UZB	Toskent	128000
3516	Cizah	UZB	Cizah	124800
3517	Navoi	UZB	Navoi	116300
3518	Olmalik	UZB	Toskent	114900
3519	Termiz	UZB	Surkhondaryo	109500
3520	Minsk	BLR	Horad Minsk	1674000
3521	Gomel	BLR	Gomel	475000
3522	Mogiljov	BLR	Mogiljov	356000
3523	Vitebsk	BLR	Vitebsk	340000
3524	Grodno	BLR	Grodno	302000
3525	Brest	BLR	Brest	286000
3526	Bobruisk	BLR	Mogiljov	221000
3527	Baranovitdi	BLR	Brest	167000
3528	Borisov	BLR	Minsk	151000
3529	Pinsk	BLR	Brest	130000
3530	Orda	BLR	Vitebsk	124000
3531	Mozyr	BLR	Gomel	110000
3532	Novopolotsk	BLR	Vitebsk	106000
3533	Lida	BLR	Grodno	101000
3534	Soligorsk	BLR	Minsk	101000
3535	Molodetdno	BLR	Minsk	97000
3536	Mata-Utu	WLF	Wallis	1137
3537	Port-Vila	VUT	Shefa	33700
3538	Citta del Vaticano	VAT	A	455
3539	Caracas	VEN	Distrito Federal	1975294
3540	Maracaibo	VEN	Zulia	1304776
3541	Barquisimeto	VEN	Lara	877239
3542	Valencia	VEN	Carabobo	794246
3543	Ciudad Guayana	VEN	Bolivar	663713
3544	Petare	VEN	Miranda	488868
3545	Maracay	VEN	Aragua	444443
3546	Barcelona	VEN	Anzoategui	322267
3547	Maturin	VEN	Monagas	319726
3548	San Cristobal	VEN	Tachira	319373
3549	Ciudad Bolivar	VEN	Bolivar	301107
3550	Cumana	VEN	Sucre	293105
3551	Merida	VEN	Merida	224887
3552	Cabimas	VEN	Zulia	221329
3553	Barinas	VEN	Barinas	217831
3554	Turmero	VEN	Aragua	217499
3555	Baruta	VEN	Miranda	207290
3556	Puerto Cabello	VEN	Carabobo	187722
3557	Santa Ana de Coro	VEN	Falcon	185766
3558	Los Teques	VEN	Miranda	178784
3559	Punto Fijo	VEN	Falcon	167215
3560	Guarenas	VEN	Miranda	165889
3561	Acarigua	VEN	Portuguesa	158954
3562	Puerto La Cruz	VEN	Anzoategui	155700
3563	Ciudad Losada	VEN		134501
3564	Guacara	VEN	Carabobo	131334
3565	Valera	VEN	Trujillo	130281
3566	Guanare	VEN	Portuguesa	125621
3567	Carupano	VEN	Sucre	119639
3568	Catia La Mar	VEN	Distrito Federal	117012
3569	El Tigre	VEN	Anzoategui	116256
3570	Guatire	VEN	Miranda	109121
3571	Calabozo	VEN	Guarico	107146
3572	Pozuelos	VEN	Anzoategui	105690
3573	Ciudad Ojeda	VEN	Zulia	99354
3574	Ocumare del Tuy	VEN	Miranda	97168
3575	Valle de la Pascua	VEN	Guarico	95927
3576	Araure	VEN	Portuguesa	94269
3577	San Fernando de Apure	VEN	Apure	93809
3578	San Felipe	VEN	Yaracuy	90940
3579	El Limon	VEN	Aragua	90000
3580	Moscow	RUS	Moscow (City)	8389200
3581	St Petersburg	RUS	Pietari	4694000
3582	Novosibirsk	RUS	Novosibirsk	1398800
3583	Nizni Novgorod	RUS	Nizni Novgorod	1357000
3584	Jekaterinburg	RUS	Sverdlovsk	1266300
3585	Samara	RUS	Samara	1156100
3586	Omsk	RUS	Omsk	1148900
3587	Kazan	RUS	Tatarstan	1101000
3588	Ufa	RUS	Badkortostan	1091200
3589	Tdeljabinsk	RUS	Tdeljabinsk	1083200
3590	Rostov-na-Donu	RUS	Rostov-na-Donu	1012700
3591	Perm	RUS	Perm	1009700
3592	Volgograd	RUS	Volgograd	993400
3593	Voronez	RUS	Voronez	907700
3594	Krasnojarsk	RUS	Krasnojarsk	875500
3595	Saratov	RUS	Saratov	874000
3596	Toljatti	RUS	Samara	722900
3597	Uljanovsk	RUS	Uljanovsk	667400
3598	Izevsk	RUS	Udmurtia	652800
3599	Krasnodar	RUS	Krasnodar	639000
3600	Jaroslavl	RUS	Jaroslavl	616700
3601	Habarovsk	RUS	Habarovsk	609400
3602	Vladivostok	RUS	Primorje	606200
3603	Irkutsk	RUS	Irkutsk	593700
3604	Barnaul	RUS	Altai	580100
3605	Novokuznetsk	RUS	Kemerovo	561600
3606	Penza	RUS	Penza	532200
3607	Rjazan	RUS	Rjazan	529900
3608	Orenburg	RUS	Orenburg	523600
3609	Lipetsk	RUS	Lipetsk	521000
3610	Nabereznyje Tdelny	RUS	Tatarstan	514700
3611	Tula	RUS	Tula	506100
3612	Tjumen	RUS	Tjumen	503400
3613	Kemerovo	RUS	Kemerovo	492700
3614	Astrahan	RUS	Astrahan	486100
3615	Tomsk	RUS	Tomsk	482100
3616	Kirov	RUS	Kirov	466200
3617	Ivanovo	RUS	Ivanovo	459200
3618	Tdeboksary	RUS	Tduvassia	459200
3619	Brjansk	RUS	Brjansk	457400
3620	Tver	RUS	Tver	454900
3621	Kursk	RUS	Kursk	443500
3622	Magnitogorsk	RUS	Tdeljabinsk	427900
3623	Kaliningrad	RUS	Kaliningrad	424400
3624	Nizni Tagil	RUS	Sverdlovsk	390900
3625	Murmansk	RUS	Murmansk	376300
3626	Ulan-Ude	RUS	Burjatia	370400
3627	Kurgan	RUS	Kurgan	364700
3628	Arkangeli	RUS	Arkangeli	361800
3629	Sotdi	RUS	Krasnodar	358600
3630	Smolensk	RUS	Smolensk	353400
3631	Orjol	RUS	Orjol	344500
3632	Stavropol	RUS	Stavropol	343300
3633	Belgorod	RUS	Belgorod	342000
3634	Kaluga	RUS	Kaluga	339300
3635	Vladimir	RUS	Vladimir	337100
3636	Mahatdkala	RUS	Dagestan	332800
3637	Tderepovets	RUS	Vologda	324400
3638	Saransk	RUS	Mordva	314800
3639	Tambov	RUS	Tambov	312000
3640	Vladikavkaz	RUS	North Ossetia-Alania	310100
3641	Tdita	RUS	Tdita	309900
3642	Vologda	RUS	Vologda	302500
3643	Veliki Novgorod	RUS	Novgorod	299500
3644	Komsomolsk-na-Amure	RUS	Habarovsk	291600
3645	Kostroma	RUS	Kostroma	288100
3646	Volzski	RUS	Volgograd	286900
3647	Taganrog	RUS	Rostov-na-Donu	284400
3648	Petroskoi	RUS	Karjala	282100
3649	Bratsk	RUS	Irkutsk	277600
3650	Dzerzinsk	RUS	Nizni Novgorod	277100
3651	Surgut	RUS	Hanti-Mansia	274900
3652	Orsk	RUS	Orenburg	273900
3653	Sterlitamak	RUS	Badkortostan	265200
3654	Angarsk	RUS	Irkutsk	264700
3655	Jodkar-Ola	RUS	Marinmaa	249200
3656	Rybinsk	RUS	Jaroslavl	239600
3657	Prokopjevsk	RUS	Kemerovo	237300
3658	Niznevartovsk	RUS	Hanti-Mansia	233900
3659	Naltdik	RUS	Kabardi-Balkaria	233400
3660	Syktyvkar	RUS	Komi	229700
3661	Severodvinsk	RUS	Arkangeli	229300
3662	Bijsk	RUS	Altai	225000
3663	Niznekamsk	RUS	Tatarstan	223400
3664	Blagovedtdensk	RUS	Amur	222000
3665	Gahty	RUS	Rostov-na-Donu	221800
3666	Staryi Oskol	RUS	Belgorod	213800
3667	Zelenograd	RUS	Moscow (City)	207100
3668	Balakovo	RUS	Saratov	206000
3669	Novorossijsk	RUS	Krasnodar	203300
3670	Pihkova	RUS	Pihkova	201500
3671	Zlatoust	RUS	Tdeljabinsk	196900
3672	Jakutsk	RUS	Saha (Jakutia)	195400
3673	Podolsk	RUS	Moskova	194300
3674	Petropavlovsk-Kamtdatski	RUS	Kamtdatka	194100
3675	Kamensk-Uralski	RUS	Sverdlovsk	190600
3676	Engels	RUS	Saratov	189000
3677	Syzran	RUS	Samara	186900
3678	Grozny	RUS	Tdetdenia	186000
3679	Novotderkassk	RUS	Rostov-na-Donu	184400
3680	Berezniki	RUS	Perm	181900
3681	Juzno-Sahalinsk	RUS	Sahalin	179200
3682	Volgodonsk	RUS	Rostov-na-Donu	178200
3683	Abakan	RUS	Hakassia	169200
3684	Maikop	RUS	Adygea	167300
3685	Miass	RUS	Tdeljabinsk	166200
3686	Armavir	RUS	Krasnodar	164900
3687	Ljubertsy	RUS	Moskova	163900
3688	Rubtsovsk	RUS	Altai	162600
3689	Kovrov	RUS	Vladimir	159900
3690	Nahodka	RUS	Primorje	157700
3691	Ussurijsk	RUS	Primorje	157300
3692	Salavat	RUS	Badkortostan	156800
3693	Mytidtdi	RUS	Moskova	155700
3694	Kolomna	RUS	Moskova	150700
3695	Elektrostal	RUS	Moskova	147000
3696	Murom	RUS	Vladimir	142400
3697	Kolpino	RUS	Pietari	141200
3698	Norilsk	RUS	Krasnojarsk	140800
3699	Almetjevsk	RUS	Tatarstan	140700
3700	Novomoskovsk	RUS	Tula	138100
3701	Dimitrovgrad	RUS	Uljanovsk	137000
3702	Pervouralsk	RUS	Sverdlovsk	136100
3703	Himki	RUS	Moskova	133700
3704	Baladiha	RUS	Moskova	132900
3705	Nevinnomyssk	RUS	Stavropol	132600
3706	Pjatigorsk	RUS	Stavropol	132500
3707	Korolev	RUS	Moskova	132400
3708	Serpuhov	RUS	Moskova	132000
3709	Odintsovo	RUS	Moskova	127400
3710	Orehovo-Zujevo	RUS	Moskova	124900
3711	Kamydin	RUS	Volgograd	124600
3712	Novotdeboksarsk	RUS	Tduvassia	123400
3713	Tderkessk	RUS	Karatdai-Tderkessia	121700
3714	Atdinsk	RUS	Krasnojarsk	121600
3715	Magadan	RUS	Magadan	121000
3716	Mitdurinsk	RUS	Tambov	120700
3717	Kislovodsk	RUS	Stavropol	120400
3718	Jelets	RUS	Lipetsk	119400
3719	Seversk	RUS	Tomsk	118600
3720	Noginsk	RUS	Moskova	117200
3721	Velikije Luki	RUS	Pihkova	116300
3722	Novokuibydevsk	RUS	Samara	116200
3723	Neftekamsk	RUS	Badkortostan	115700
3724	Leninsk-Kuznetski	RUS	Kemerovo	113800
3725	Oktjabrski	RUS	Badkortostan	111500
3726	Sergijev Posad	RUS	Moskova	111100
3727	Arzamas	RUS	Nizni Novgorod	110700
3728	Kiseljovsk	RUS	Kemerovo	110000
3729	Novotroitsk	RUS	Orenburg	109600
3730	Obninsk	RUS	Kaluga	108300
3731	Kansk	RUS	Krasnojarsk	107400
3732	Glazov	RUS	Udmurtia	106300
3733	Solikamsk	RUS	Perm	106000
3734	Sarapul	RUS	Udmurtia	105700
3735	Ust-Ilimsk	RUS	Irkutsk	105200
3736	Gtdolkovo	RUS	Moskova	104900
3737	Mezduretdensk	RUS	Kemerovo	104400
3738	Usolje-Sibirskoje	RUS	Irkutsk	103500
3739	Elista	RUS	Kalmykia	103300
3740	Novodahtinsk	RUS	Rostov-na-Donu	101900
3741	Votkinsk	RUS	Udmurtia	101700
3742	Kyzyl	RUS	Tyva	101100
3743	Serov	RUS	Sverdlovsk	100400
3744	Zelenodolsk	RUS	Tatarstan	100200
3745	Zeleznodoroznyi	RUS	Moskova	100100
3746	Kinedma	RUS	Ivanovo	100000
3747	Kuznetsk	RUS	Penza	98200
3748	Uhta	RUS	Komi	98000
3749	Jessentuki	RUS	Stavropol	97900
3750	Tobolsk	RUS	Tjumen	97600
3751	Neftejugansk	RUS	Hanti-Mansia	97400
3752	Bataisk	RUS	Rostov-na-Donu	97300
3753	Nojabrsk	RUS	Yamalin Nenetsia	97300
3754	Baladov	RUS	Saratov	97100
3755	Zeleznogorsk	RUS	Kursk	96900
3756	Zukovski	RUS	Moskova	96500
3757	Anzero-Sudzensk	RUS	Kemerovo	96100
3758	Bugulma	RUS	Tatarstan	94100
3759	Zeleznogorsk	RUS	Krasnojarsk	94000
3760	Novouralsk	RUS	Sverdlovsk	93300
3761	Pudkin	RUS	Pietari	92900
3762	Vorkuta	RUS	Komi	92600
3763	Derbent	RUS	Dagestan	92300
3764	Kirovo-Tdepetsk	RUS	Kirov	91600
3765	Krasnogorsk	RUS	Moskova	91000
3766	Klin	RUS	Moskova	90000
3767	Tdaikovski	RUS	Perm	90000
3768	Novyi Urengoi	RUS	Yamalin Nenetsia	89800
3769	Ho Chi Minh City	VNM	Ho Chi Minh City	3980000
3770	Hanoi	VNM	Hanoi	1410000
3771	Haiphong	VNM	Haiphong	783133
3772	Da Nang	VNM	Quang Nam-Da Nang	382674
3773	Bien Hoa	VNM	Dong Nai	282095
3774	Nha Trang	VNM	Khanh Hoa	221331
3775	Hue	VNM	Thua Thien-Hue	219149
3776	Can Tho	VNM	Can Tho	215587
3777	Cam Pha	VNM	Quang Binh	209086
3778	Nam Dinh	VNM	Nam Ha	171699
3779	Quy Nhon	VNM	Binh Dinh	163385
3780	Vung Tau	VNM	Ba Ria-Vung Tau	145145
3781	Rach Gia	VNM	Kien Giang	141132
3782	Long Xuyen	VNM	An Giang	132681
3783	Thai Nguyen	VNM	Bac Thai	127643
3784	Hong Gai	VNM	Quang Ninh	127484
3785	Phan Thiet	VNM	Binh Thuan	114236
3786	Cam Ranh	VNM	Khanh Hoa	114041
3787	Vinh	VNM	Nghe An	112455
3788	My Tho	VNM	Tien Giang	108404
3789	Da Lat	VNM	Lam Dong	106409
3790	Buon Ma Thuot	VNM	Dac Lac	97044
3791	Tallinn	EST	Harjumaa	403981
3792	Tartu	EST	Tartumaa	101246
3793	New York	USA	New York	8008278
3794	Los Angeles	USA	California	3694820
3795	Chicago	USA	Illinois	2896016
3796	Houston	USA	Texas	1953631
3797	Philadelphia	USA	Pennsylvania	1517550
3798	Phoenix	USA	Arizona	1321045
3799	San Diego	USA	California	1223400
3800	Dallas	USA	Texas	1188580
3801	San Antonio	USA	Texas	1144646
3802	Detroit	USA	Michigan	951270
3803	San Jose	USA	California	894943
3804	Indianapolis	USA	Indiana	791926
3805	San Francisco	USA	California	776733
3806	Jacksonville	USA	Florida	735167
3807	Columbus	USA	Ohio	711470
3808	Austin	USA	Texas	656562
3809	Baltimore	USA	Maryland	651154
3810	Memphis	USA	Tennessee	650100
3811	Milwaukee	USA	Wisconsin	596974
3812	Boston	USA	Massachusetts	589141
3813	Washington	USA	District of Columbia	572059
3814	Nashville-Davidson	USA	Tennessee	569891
3815	El Paso	USA	Texas	563662
3816	Seattle	USA	Washington	563374
3817	Denver	USA	Colorado	554636
3818	Charlotte	USA	North Carolina	540828
3819	Fort Worth	USA	Texas	534694
3820	Portland	USA	Oregon	529121
3821	Oklahoma City	USA	Oklahoma	506132
3822	Tucson	USA	Arizona	486699
3823	New Orleans	USA	Louisiana	484674
3824	Las Vegas	USA	Nevada	478434
3825	Cleveland	USA	Ohio	478403
3826	Long Beach	USA	California	461522
3827	Albuquerque	USA	New Mexico	448607
3828	Kansas City	USA	Missouri	441545
3829	Fresno	USA	California	427652
3830	Virginia Beach	USA	Virginia	425257
3831	Atlanta	USA	Georgia	416474
3832	Sacramento	USA	California	407018
3833	Oakland	USA	California	399484
3834	Mesa	USA	Arizona	396375
3835	Tulsa	USA	Oklahoma	393049
3836	Omaha	USA	Nebraska	390007
3837	Minneapolis	USA	Minnesota	382618
3838	Honolulu	USA	Hawaii	371657
3839	Miami	USA	Florida	362470
3840	Colorado Springs	USA	Colorado	360890
3841	Saint Louis	USA	Missouri	348189
3842	Wichita	USA	Kansas	344284
3843	Santa Ana	USA	California	337977
3844	Pittsburgh	USA	Pennsylvania	334563
3845	Arlington	USA	Texas	332969
3846	Cincinnati	USA	Ohio	331285
3847	Anaheim	USA	California	328014
3848	Toledo	USA	Ohio	313619
3849	Tampa	USA	Florida	303447
3850	Buffalo	USA	New York	292648
3851	Saint Paul	USA	Minnesota	287151
3852	Corpus Christi	USA	Texas	277454
3853	Aurora	USA	Colorado	276393
3854	Raleigh	USA	North Carolina	276093
3855	Newark	USA	New Jersey	273546
3856	Lexington-Fayette	USA	Kentucky	260512
3857	Anchorage	USA	Alaska	260283
3858	Louisville	USA	Kentucky	256231
3859	Riverside	USA	California	255166
3860	Saint Petersburg	USA	Florida	248232
3861	Bakersfield	USA	California	247057
3862	Stockton	USA	California	243771
3863	Birmingham	USA	Alabama	242820
3864	Jersey City	USA	New Jersey	240055
3865	Norfolk	USA	Virginia	234403
3866	Baton Rouge	USA	Louisiana	227818
3867	Hialeah	USA	Florida	226419
3868	Lincoln	USA	Nebraska	225581
3869	Greensboro	USA	North Carolina	223891
3870	Plano	USA	Texas	222030
3871	Rochester	USA	New York	219773
3872	Glendale	USA	Arizona	218812
3873	Akron	USA	Ohio	217074
3874	Garland	USA	Texas	215768
3875	Madison	USA	Wisconsin	208054
3876	Fort Wayne	USA	Indiana	205727
3877	Fremont	USA	California	203413
3878	Scottsdale	USA	Arizona	202705
3879	Montgomery	USA	Alabama	201568
3880	Shreveport	USA	Louisiana	200145
3881	Augusta-Richmond County	USA	Georgia	199775
3882	Lubbock	USA	Texas	199564
3883	Chesapeake	USA	Virginia	199184
3884	Mobile	USA	Alabama	198915
3885	Des Moines	USA	Iowa	198682
3886	Grand Rapids	USA	Michigan	197800
3887	Richmond	USA	Virginia	197790
3888	Yonkers	USA	New York	196086
3889	Spokane	USA	Washington	195629
3890	Glendale	USA	California	194973
3891	Tacoma	USA	Washington	193556
3892	Irving	USA	Texas	191615
3893	Huntington Beach	USA	California	189594
3894	Modesto	USA	California	188856
3895	Durham	USA	North Carolina	187035
3896	Columbus	USA	Georgia	186291
3897	Orlando	USA	Florida	185951
3898	Boise City	USA	Idaho	185787
3899	Winston-Salem	USA	North Carolina	185776
3900	San Bernardino	USA	California	185401
3901	Jackson	USA	Mississippi	184256
3902	Little Rock	USA	Arkansas	183133
3903	Salt Lake City	USA	Utah	181743
3904	Reno	USA	Nevada	180480
3905	Newport News	USA	Virginia	180150
3906	Chandler	USA	Arizona	176581
3907	Laredo	USA	Texas	176576
3908	Henderson	USA	Nevada	175381
3909	Arlington	USA	Virginia	174838
3910	Knoxville	USA	Tennessee	173890
3911	Amarillo	USA	Texas	173627
3912	Providence	USA	Rhode Island	173618
3913	Chula Vista	USA	California	173556
3914	Worcester	USA	Massachusetts	172648
3915	Oxnard	USA	California	170358
3916	Dayton	USA	Ohio	166179
3917	Garden Grove	USA	California	165196
3918	Oceanside	USA	California	161029
3919	Tempe	USA	Arizona	158625
3920	Huntsville	USA	Alabama	158216
3921	Ontario	USA	California	158007
3922	Chattanooga	USA	Tennessee	155554
3923	Fort Lauderdale	USA	Florida	152397
3924	Springfield	USA	Massachusetts	152082
3925	Springfield	USA	Missouri	151580
3926	Santa Clarita	USA	California	151088
3927	Salinas	USA	California	151060
3928	Tallahassee	USA	Florida	150624
3929	Rockford	USA	Illinois	150115
3930	Pomona	USA	California	149473
3931	Metairie	USA	Louisiana	149428
3932	Paterson	USA	New Jersey	149222
3933	Overland Park	USA	Kansas	149080
3934	Santa Rosa	USA	California	147595
3935	Syracuse	USA	New York	147306
3936	Kansas City	USA	Kansas	146866
3937	Hampton	USA	Virginia	146437
3938	Lakewood	USA	Colorado	144126
3939	Vancouver	USA	Washington	143560
3940	Irvine	USA	California	143072
3941	Aurora	USA	Illinois	142990
3942	Moreno Valley	USA	California	142381
3943	Pasadena	USA	California	141674
3944	Hayward	USA	California	140030
3945	Brownsville	USA	Texas	139722
3946	Bridgeport	USA	Connecticut	139529
3947	Hollywood	USA	Florida	139357
3948	Warren	USA	Michigan	138247
3949	Torrance	USA	California	137946
3950	Eugene	USA	Oregon	137893
3951	Pembroke Pines	USA	Florida	137427
3952	Salem	USA	Oregon	136924
3953	Pasadena	USA	Texas	133936
3954	Escondido	USA	California	133559
3955	Sunnyvale	USA	California	131760
3956	Savannah	USA	Georgia	131510
3957	Fontana	USA	California	128929
3958	Orange	USA	California	128821
3959	Naperville	USA	Illinois	128358
3960	Alexandria	USA	Virginia	128283
3961	Rancho Cucamonga	USA	California	127743
3962	Grand Prairie	USA	Texas	127427
3963	East Los Angeles	USA	California	126379
3964	Fullerton	USA	California	126003
3965	Corona	USA	California	124966
3966	Flint	USA	Michigan	124943
3967	Paradise	USA	Nevada	124682
3968	Mesquite	USA	Texas	124523
3969	Sterling Heights	USA	Michigan	124471
3970	Sioux Falls	USA	South Dakota	123975
3971	New Haven	USA	Connecticut	123626
3972	Topeka	USA	Kansas	122377
3973	Concord	USA	California	121780
3974	Evansville	USA	Indiana	121582
3975	Hartford	USA	Connecticut	121578
3976	Fayetteville	USA	North Carolina	121015
3977	Cedar Rapids	USA	Iowa	120758
3978	Elizabeth	USA	New Jersey	120568
3979	Lansing	USA	Michigan	119128
3980	Lancaster	USA	California	118718
3981	Fort Collins	USA	Colorado	118652
3982	Coral Springs	USA	Florida	117549
3983	Stamford	USA	Connecticut	117083
3984	Thousand Oaks	USA	California	117005
3985	Vallejo	USA	California	116760
3986	Palmdale	USA	California	116670
3987	Columbia	USA	South Carolina	116278
3988	El Monte	USA	California	115965
3989	Abilene	USA	Texas	115930
3990	North Las Vegas	USA	Nevada	115488
3991	Ann Arbor	USA	Michigan	114024
3992	Beaumont	USA	Texas	113866
3993	Waco	USA	Texas	113726
3994	Macon	USA	Georgia	113336
3995	Independence	USA	Missouri	113288
3996	Peoria	USA	Illinois	112936
3997	Inglewood	USA	California	112580
3998	Springfield	USA	Illinois	111454
3999	Simi Valley	USA	California	111351
4000	Lafayette	USA	Louisiana	110257
4001	Gilbert	USA	Arizona	109697
4002	Carrollton	USA	Texas	109576
4003	Bellevue	USA	Washington	109569
4004	West Valley City	USA	Utah	108896
4005	Clarksville	USA	Tennessee	108787
4006	Costa Mesa	USA	California	108724
4007	Peoria	USA	Arizona	108364
4008	South Bend	USA	Indiana	107789
4009	Downey	USA	California	107323
4010	Waterbury	USA	Connecticut	107271
4011	Manchester	USA	New Hampshire	107006
4012	Allentown	USA	Pennsylvania	106632
4013	McAllen	USA	Texas	106414
4014	Joliet	USA	Illinois	106221
4015	Lowell	USA	Massachusetts	105167
4016	Provo	USA	Utah	105166
4017	West Covina	USA	California	105080
4018	Wichita Falls	USA	Texas	104197
4019	Erie	USA	Pennsylvania	103717
4020	Daly City	USA	California	103621
4021	Citrus Heights	USA	California	103455
4022	Norwalk	USA	California	103298
4023	Gary	USA	Indiana	102746
4024	Berkeley	USA	California	102743
4025	Santa Clara	USA	California	102361
4026	Green Bay	USA	Wisconsin	102313
4027	Cape Coral	USA	Florida	102286
4028	Arvada	USA	Colorado	102153
4029	Pueblo	USA	Colorado	102121
4030	Sandy	USA	Utah	101853
4031	Athens-Clarke County	USA	Georgia	101489
4032	Cambridge	USA	Massachusetts	101355
4033	Westminster	USA	Colorado	100940
4034	San Buenaventura	USA	California	100916
4035	Portsmouth	USA	Virginia	100565
4036	Livonia	USA	Michigan	100545
4037	Burbank	USA	California	100316
4038	Clearwater	USA	Florida	99936
4039	Midland	USA	Texas	98293
4040	Davenport	USA	Iowa	98256
4041	Mission Viejo	USA	California	98049
4042	Miami Beach	USA	Florida	97855
4043	Sunrise Manor	USA	Nevada	95362
4044	New Bedford	USA	Massachusetts	94780
4045	El Cajon	USA	California	94578
4046	Norman	USA	Oklahoma	94193
4047	Richmond	USA	California	94100
4048	Albany	USA	New York	93994
4049	Brockton	USA	Massachusetts	93653
4050	Roanoke	USA	Virginia	93357
4051	Billings	USA	Montana	92988
4052	Compton	USA	California	92864
4053	Gainesville	USA	Florida	92291
4054	Fairfield	USA	California	92256
4055	Arden-Arcade	USA	California	92040
4056	San Mateo	USA	California	91799
4057	Visalia	USA	California	91762
4058	Boulder	USA	Colorado	91238
4059	Cary	USA	North Carolina	91213
4060	Santa Monica	USA	California	91084
4061	Fall River	USA	Massachusetts	90555
4062	Kenosha	USA	Wisconsin	89447
4063	Elgin	USA	Illinois	89408
4064	Odessa	USA	Texas	89293
4065	Carson	USA	California	89089
4066	Charleston	USA	South Carolina	89063
4067	Charlotte Amalie	VIR	St Thomas	13000
4068	Harare	ZWE	Harare	1410000
4069	Bulawayo	ZWE	Bulawayo	621742
4070	Chitungwiza	ZWE	Harare	274912
4071	Mount Darwin	ZWE	Harare	164362
4072	Mutare	ZWE	Manicaland	131367
4073	Gweru	ZWE	Midlands	128037
4074	Gaza	PSE	Gaza	353632
4075	Khan Yunis	PSE	Khan Yunis	123175
4076	Hebron	PSE	Hebron	119401
4077	Jabaliya	PSE	North Gaza	113901
4078	Nablus	PSE	Nablus	100231
4079	Rafah	PSE	Rafah	92020
\.


--
-- Data for Name: country; Type: TABLE DATA; Schema: public; Owner: chriskl
--

COPY country (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM stdin WITH NULL AS '';
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
-- Data for Name: countrylanguage; Type: TABLE DATA; Schema: public; Owner: chriskl
--

COPY countrylanguage (countrycode, "language", isofficial, percentage) FROM stdin;
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


ALTER TABLE ONLY city
    ADD CONSTRAINT city_pkey PRIMARY KEY (id);

ALTER TABLE ONLY country
    ADD CONSTRAINT country_pkey PRIMARY KEY (code);

ALTER TABLE ONLY countrylanguage
    ADD CONSTRAINT countrylanguage_pkey PRIMARY KEY (countrycode, "language");


ANALYZE city;
ANALYZE country;
ANALYZE countrylanguage;

