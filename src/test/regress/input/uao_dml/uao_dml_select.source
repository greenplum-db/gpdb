create schema uao_dml_select_@amname@;
set search_path=uao_dml_select_@amname@;
SET default_table_access_method=@amname@;

-- @Description select with aggregate function
--

DROP TABLE IF EXISTS city_uao cascade;
DROP TABLE IF EXISTS country_uao cascade;
DROP TABLE IF EXISTS countrylanguage_uao cascade;

BEGIN;

CREATE TABLE city_uao (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) distributed by(id);

CREATE TABLE country_uao (
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

CREATE TABLE countrylanguage_uao (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
) distributed by (countrycode,language);

COPY city_uao (id, name, countrycode, district, population) FROM '@abs_srcdir@/data/city.data';
COPY country_uao (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM '@abs_srcdir@/data/country.data' WITH NULL AS '';
COPY countrylanguage_uao (countrycode, "language", isofficial, percentage) FROM '@abs_srcdir@/data/countrylanguage.data';

COMMIT;

ANALYZE city_uao;
ANALYZE country_uao;
ANALYZE countrylanguage_uao;

--query with aggregate functions

select
(select max(city_uao.population) from city_uao ) as WORLD_MAX_POP,
(select avg(city_uao.population) from city_uao) AS WORLD_AVG_POP,
 city_uao.name as populationwise_top_five_cities,city_uao.population
 from
 city_uao order by city_uao.population desc LIMIT 5;

select
(select min(city_uao.population) from city_uao ) as WORLD_MIN_POP,
(select SUM(city_uao.population) from city_uao) AS WORLD_SUM_POP,
 city_uao.name as populationwise_top_five_cities,city_uao.population
 from
 city_uao order by city_uao.population desc LIMIT 5;


-- @Description select with between
--

-- Reuse the country_uao table from previous test.

-- Using  the BETWEEN clause

--query
select *
from
country_uao
where indepyear between 1800 and 1900
and lifeexpectancy between 70 and 80
and gnp between 80000 and 100000;


-- @Description select with CASE WHEN.. ELSE
--
DROP TABLE IF EXISTS sto_uao_emp_limit_offset;
CREATE TABLE sto_uao_emp_limit_offset (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_limit_offset values 
 (1,'JOHNSON','ADMIN',6,'12-17-1990',18000,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',52000,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',25000,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',27000,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',22500,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',54000,20,4)
,(7,'POLK','TECH',6,'09-22-1997',25000,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',32000,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',75000,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',56000,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',34000,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18000,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,2)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18000,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',24000,30,1);


select ename , hiredate,
CASE when hiredate < '01-01-1991' then 'Founders'
when hiredate < '01-01-1996' then 'old timer'
else 'new employee' 
end as tag
from sto_uao_emp_limit_offset  order by 1; 


-- @Description select inner/right join
--
DROP TABLE IF EXISTS sto_uao_emp_crossjoin cascade;
CREATE TABLE sto_uao_emp_crossjoin (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_crossjoin values
(1,'JOHNSON','ADMIN',6,'12-17-1990',18000,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',52000,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',25000,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',27000,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',22500,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',54000,20,4)
,(7,'POLK','TECH',6,'09-22-1997',25000,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',32000,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',75000,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',56000,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',34000,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18000,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,7)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18000,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',24000,30,1);


DROP TABLE IF EXISTS sto_uao_dept_crossjoin cascade;
CREATE TABLE sto_uao_dept_crossjoin (
   deptno INT NOT NULL,
   dname  VARCHAR(14),
   loc    VARCHAR(13)) distributed by (deptno);

insert into sto_uao_dept_crossjoin values
 (1,'ACCOUNTING','ST LOUIS')
,(2,'RESEARCH','NEW YORK')
,(3,'SALES','ATLANTA')
,(5,'LOGISTICS','BOSTON')
,(4, 'OPERATIONS','SEATTLE');

select ename,loc from sto_uao_emp_crossjoin cross join sto_uao_dept_crossjoin
order by 1 asc, 2 asc LIMIT 10;


-- @Description select distinct
--
DROP TABLE IF EXISTS sto_uao_emp_seldistinct cascade;
CREATE TABLE sto_uao_emp_seldistinct (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_seldistinct values
(1,'JOHNSON','ADMIN',6,'12-17-1990',18000,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',52000,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',25000,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',27000,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',22500,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',54000,20,4)
,(7,'POLK','TECH',6,'09-22-1997',25000,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',32000,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',75000,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',56000,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',34000,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18000,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,7)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18000,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',24000,30,1);


SELECT distinct(dept) FROM sto_uao_emp_seldistinct order by dept;


-- @description : Create Data and execute select statements on UAO tables EXCEPT
--
DROP TABLE IF EXISTS city_uao cascade;
DROP TABLE IF EXISTS country_uao cascade;
DROP TABLE IF EXISTS countrylanguage_uao cascade;

BEGIN;

CREATE TABLE city_uao (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) distributed by(id);

CREATE TABLE country_uao (
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

CREATE TABLE countrylanguage_uao (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
) distributed by (countrycode,language);

COPY city_uao (id, name, countrycode, district, population) FROM stdin;
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
922	Ciudad de Guatemala	GTM	Guatemala	823301
923	Mixco	GTM	Guatemala	209791
924	Villa Nueva	GTM	Guatemala	101295
925	Quetzaltenango	GTM	Quetzaltenango	90801
2882	Ciudad de Panama	PAN	Panama	471373
2883	San Miguelito	PAN	San Miguelito	315382
190	Saint George	BMU	Saint Georges	1800
191	Hamilton	BMU	Hamilton	1200
184	Belize City	BLZ	Belize City	55810
185	Belmopan	BLZ	Cayo	7105
\.

COPY country_uao (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM stdin WITH NULL AS '';
CAN	Canada	North America	North America	9970610	1867	31147000	79.400002	598862.00	625626.00	Canada	Constitutional Monarchy, Federation	Elisabeth II	1822	CA
MEX	Mexico	North America	Central America	1958201	1810	98881000	71.5	414972.00	401461.00	Mexico	Federal Republic	Vicente Fox Quesada	2515	MX
CUB	Cuba	North America	Caribbean	110861	1902	11201000	76.199997	17843.00	18862.00	Cuba	Socialistic Republic	Fidel Castro Ruz	2413	CU
GTM	Guatemala	North America	Central America	108889	1821	11385000	66.199997	19008.00	17797.00	Guatemala	Republic	Alfonso Portillo Cabrera	922	GT
PAN	Panama	North America	Central America	75517	1903	2856000	75.5	9131.00	8700.00	Panama	Republic	Mireya Elisa Moscoso Rodriguez	2882	PA
USA	United States	North America	North America	9363520	1776	278357000	77.099998	8510700.00	8110900.00	United States	Federal Republic	George W. Bush	3813	US
BMU	Bermuda	North America	North America	53		65000	76.900002	2328.00	2190.00	Bermuda	Dependent Territory of the UK	Elisabeth II	191	BM
BLZ	Belize	North America	Central America	22696	1981	241000	70.900002	630.00	616.00	Belize	Constitutional Monarchy	Elisabeth II	185	BZ
\.

COPY countrylanguage_uao (countrycode, "language", isofficial, percentage) FROM stdin;
CAN	English	t	60.400002
CAN	French	t	23.4
CAN	Chinese	f	2.5
CAN	Italian	f	1.7
CAN	German	f	1.6
CAN	Polish	f	0.69999999
CAN	Spanish	f	0.69999999
CAN	Portuguese	f	0.69999999
CAN	Punjabi	f	0.69999999
CAN	Ukrainian	f	0.60000002
CAN	Dutch	f	0.5
CAN	Eskimo Languages	f	0.1
USA	English	t	86.199997
USA	Spanish	f	7.5
USA	French	f	0.69999999
USA	German	f	0.69999999
USA	Italian	f	0.60000002
USA	Chinese	f	0.60000002
USA	Tagalog	f	0.40000001
USA	Polish	f	0.30000001
USA	Korean	f	0.30000001
USA	Vietnamese	f	0.2
USA	Japanese	f	0.2
USA	Portuguese	f	0.2
MEX	Spanish	t	92.099998
MEX	Nahuatl	f	1.8
MEX	Yucatec	f	1.1
MEX	Zapotec	f	0.60000002
MEX	Mixtec	f	0.60000002
MEX	Otomi	f	0.40000001
CUB	Spanish	t	100
GTM	Spanish	t	64.699997
GTM	Quiche	f	10.1
GTM	Cakchiquel	f	8.8999996
GTM	Kekchi	f	4.9000001
GTM	Mam	f	2.7
PAN	Spanish	t	76.800003
PAN	Creole English	f	14
PAN	Guaymi	f	5.3000002
PAN	Cuna	f	2
PAN	Embera	f	0.60000002
PAN	Arabic	f	0.60000002
BMU	English	t	100
BLZ	English	t	50.799999
BLZ	Spanish	f	31.6
BLZ	Maya Languages	f	9.6000004
BLZ	Garifuna	f	6.8000002
\.

COMMIT;

ANALYZE city_uao;
ANALYZE country_uao;
ANALYZE countrylanguage_uao;

-- Using  EXCEPT clause

select * from city_uao where population > 770000
UNION
select * from city_uao where population < 100000
EXCEPT
select * from city_uao where countrycode in ('MEX','USA');


-- @description : Create Data and execute select statements on UAO tables GROUP BY
--
DROP TABLE IF EXISTS sto_uao_emp_groupby;
CREATE TABLE sto_uao_emp_groupby (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_groupby values 
 (1,'JOHNSON','ADMIN',6,'12-17-1990',18000,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',52000,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',25000,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',27000,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',22500,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',54000,20,4)
,(7,'POLK','TECH',6,'09-22-1997',25000,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',32000,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',75000,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',56000,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',34000,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18000,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,2)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18000,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',24000,30,1);
DROP TABLE IF EXISTS sto_uao_dept_groupby;
CREATE TABLE sto_uao_dept_groupby (
   deptno INT NOT NULL,
   dname  VARCHAR(14),
   loc    VARCHAR(13)) distributed by (deptno);

insert into sto_uao_dept_groupby values 
 (1,'ACCOUNTING','ST LOUIS')
,(2,'RESEARCH','NEW YORK')
,(3,'SALES','ATLANTA')
,(4, 'OPERATIONS','SEATTLE');


-- Simple Group by
select count(*) as num_of_emp, loc from sto_uao_dept_groupby, sto_uao_emp_groupby where sto_uao_dept_groupby.deptno = sto_uao_emp_groupby.dept group by loc order by 1 asc;
select  loc ,job, sum(sal) as sum_of_salary, count(*) as num_emp  from sto_uao_emp_groupby,sto_uao_dept_groupby where sto_uao_emp_groupby.dept=sto_uao_dept_groupby.deptno
group by loc,job order by loc,job;

-- Rollup

select  loc ,job,comm,sum(sal),  count(*) as cnt from sto_uao_emp_groupby,sto_uao_dept_groupby where sto_uao_emp_groupby.dept=sto_uao_dept_groupby.deptno
and loc in ('NEW YORK', 'ATLANTA')
group by rollup (loc,job,comm) order by loc,job,comm;

-- Cube
select  loc ,job,comm,sum(sal),  count(*) as cnt from sto_uao_emp_groupby,sto_uao_dept_groupby where sto_uao_emp_groupby.dept=sto_uao_dept_groupby.deptno
and loc != 'SEATTLE'
group by cube (loc,job,comm) order by loc,job,comm;

-- grouping sets
select  loc ,job,comm,sum(sal) as sum from sto_uao_emp_groupby,sto_uao_dept_groupby where sto_uao_emp_groupby.dept=sto_uao_dept_groupby.deptno
group by grouping sets ((loc,job),  (loc,comm)) 
order by loc,job,comm;

-- grouping set , rollup
select  loc ,job, sum(sal) from sto_uao_emp_groupby,sto_uao_dept_groupby where sto_uao_emp_groupby.dept=sto_uao_dept_groupby.deptno
group by grouping sets (loc, rollup (loc,job) ) 
order by loc,job ;

-- @description : Create Data and execute select statements on UAO tables  HAVING
--
DROP TABLE IF EXISTS city_uao cascade;
DROP TABLE IF EXISTS country_uao cascade;
DROP TABLE IF EXISTS countrylanguage_uao cascade;

BEGIN;

CREATE TABLE city_uao (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) distributed by(id);

CREATE TABLE country_uao (
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

CREATE TABLE countrylanguage_uao (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
) distributed by (countrycode,language);

COPY city_uao (id, name, countrycode, district, population) FROM '@abs_srcdir@/data/city.data';
COPY country_uao (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM '@abs_srcdir@/data/country.data' WITH NULL AS '';
COPY countrylanguage_uao (countrycode, "language", isofficial, percentage) FROM '@abs_srcdir@/data/countrylanguage.data';

COMMIT;

ANALYZE city_uao;
ANALYZE country_uao;
ANALYZE countrylanguage_uao;

--queries with HAVING clause

with notdiversecountries_uao as
(select country_uao.code,country_uao.name,country_uao.capital,d.CNT
 from country_uao,
 (select countrylanguage_uao.countrycode,count(*) as CNT from countrylanguage_uao group by countrycode
  HAVING count(*) < 3) d
 where d.countrycode = country_uao.code and country_uao.gnp > 100000)

select country_uao.name COUNTRY,city_uao.name CAPITAL,count(*) LANGCNT from
country_uao,city_uao,countrylanguage_uao
where country_uao.code = countrylanguage_uao.countrycode and country_uao.capital = city_uao.id
group by country_uao.name,city_uao.name
HAVING count(*) NOT IN (select CNT from notdiversecountries_uao where notdiversecountries_uao.name = country_uao.name)
order by country_uao.name
LIMIT 10;


-- @Description select inner/right join
--
DROP TABLE IF EXISTS sto_uao_emp_rightjoin cascade;
CREATE TABLE sto_uao_emp_rightjoin (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_rightjoin values
(1,'JOHNSON','ADMIN',6,'12-17-1990',18000,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',52000,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',25000,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',27000,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',22500,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',54000,20,4)
,(7,'POLK','TECH',6,'09-22-1997',25000,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',32000,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',75000,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',56000,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',34000,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18000,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,7)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18000,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',24000,30,1);

DROP TABLE IF EXISTS sto_uao_dept_rightjoin cascade;
CREATE TABLE sto_uao_dept_rightjoin (
   deptno INT NOT NULL,
   dname  VARCHAR(14),
   loc    VARCHAR(13)) distributed by (deptno);

insert into sto_uao_dept_rightjoin values
 (1,'ACCOUNTING','ST LOUIS')
,(2,'RESEARCH','NEW YORK')
,(3,'SALES','ATLANTA')
,(5,'LOGISTICS','BOSTON')
,(4, 'OPERATIONS','SEATTLE');

select ename, loc from sto_uao_emp_rightjoin left inner join sto_uao_dept_rightjoin on  sto_uao_dept_rightjoin.deptno = sto_uao_emp_rightjoin.dept order by 1 asc, 2 asc; 

-- @description : Create Data and execute select statements on UAO tables INTERSECT
--
DROP TABLE IF EXISTS city_uao cascade;
DROP TABLE IF EXISTS country_uao cascade;
DROP TABLE IF EXISTS countrylanguage_uao cascade;

BEGIN;

CREATE TABLE city_uao (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) distributed by(id);

CREATE TABLE country_uao (
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

CREATE TABLE countrylanguage_uao (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
) distributed by (countrycode,language);

COPY city_uao (id, name, countrycode, district, population) FROM stdin;
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
922	Ciudad de Guatemala	GTM	Guatemala	823301
923	Mixco	GTM	Guatemala	209791
924	Villa Nueva	GTM	Guatemala	101295
925	Quetzaltenango	GTM	Quetzaltenango	90801
2882	Ciudad de Panama	PAN	Panama	471373
2883	San Miguelito	PAN	San Miguelito	315382
190	Saint George	BMU	Saint Georges	1800
191	Hamilton	BMU	Hamilton	1200
184	Belize City	BLZ	Belize City	55810
185	Belmopan	BLZ	Cayo	7105
\.

COPY country_uao (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM stdin WITH NULL AS '';
CAN	Canada	North America	North America	9970610	1867	31147000	79.400002	598862.00	625626.00	Canada	Constitutional Monarchy, Federation	Elisabeth II	1822	CA
MEX	Mexico	North America	Central America	1958201	1810	98881000	71.5	414972.00	401461.00	Mexico	Federal Republic	Vicente Fox Quesada	2515	MX
CUB	Cuba	North America	Caribbean	110861	1902	11201000	76.199997	17843.00	18862.00	Cuba	Socialistic Republic	Fidel Castro Ruz	2413	CU
GTM	Guatemala	North America	Central America	108889	1821	11385000	66.199997	19008.00	17797.00	Guatemala	Republic	Alfonso Portillo Cabrera	922	GT
PAN	Panama	North America	Central America	75517	1903	2856000	75.5	9131.00	8700.00	Panama	Republic	Mireya Elisa Moscoso Rodriguez	2882	PA
USA	United States	North America	North America	9363520	1776	278357000	77.099998	8510700.00	8110900.00	United States	Federal Republic	George W. Bush	3813	US
BMU	Bermuda	North America	North America	53		65000	76.900002	2328.00	2190.00	Bermuda	Dependent Territory of the UK	Elisabeth II	191	BM
BLZ	Belize	North America	Central America	22696	1981	241000	70.900002	630.00	616.00	Belize	Constitutional Monarchy	Elisabeth II	185	BZ
\.

COPY countrylanguage_uao (countrycode, "language", isofficial, percentage) FROM stdin;
CAN	English	t	60.400002
CAN	French	t	23.4
CAN	Chinese	f	2.5
CAN	Italian	f	1.7
CAN	German	f	1.6
CAN	Polish	f	0.69999999
CAN	Spanish	f	0.69999999
CAN	Portuguese	f	0.69999999
CAN	Punjabi	f	0.69999999
CAN	Ukrainian	f	0.60000002
CAN	Dutch	f	0.5
CAN	Eskimo Languages	f	0.1
USA	English	t	86.199997
USA	Spanish	f	7.5
USA	French	f	0.69999999
USA	German	f	0.69999999
USA	Italian	f	0.60000002
USA	Chinese	f	0.60000002
USA	Tagalog	f	0.40000001
USA	Polish	f	0.30000001
USA	Korean	f	0.30000001
USA	Vietnamese	f	0.2
USA	Japanese	f	0.2
USA	Portuguese	f	0.2
MEX	Spanish	t	92.099998
MEX	Nahuatl	f	1.8
MEX	Yucatec	f	1.1
MEX	Zapotec	f	0.60000002
MEX	Mixtec	f	0.60000002
MEX	Otomi	f	0.40000001
CUB	Spanish	t	100
GTM	Spanish	t	64.699997
GTM	Quiche	f	10.1
GTM	Cakchiquel	f	8.8999996
GTM	Kekchi	f	4.9000001
GTM	Mam	f	2.7
PAN	Spanish	t	76.800003
PAN	Creole English	f	14
PAN	Guaymi	f	5.3000002
PAN	Cuna	f	2
PAN	Embera	f	0.60000002
PAN	Arabic	f	0.60000002
BMU	English	t	100
BLZ	English	t	50.799999
BLZ	Spanish	f	31.6
BLZ	Maya Languages	f	9.6000004
BLZ	Garifuna	f	6.8000002
\.

COMMIT;

ANALYZE city_uao;
ANALYZE country_uao;
ANALYZE countrylanguage_uao;

-- Using  EXCEPT clause

select population from city_uao where countrycode = 'CAN'
INTERSECT
select population from city_uao where countrycode = 'MEX';


-- @description : Create Data and execute select statements on UAO tables LIMIT OFFSET 
--
DROP TABLE IF EXISTS sto_uao_emp_limit_offset;
CREATE TABLE sto_uao_emp_limit_offset (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_limit_offset values 
 (1,'JOHNSON','ADMIN',6,'12-17-1990',18000,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',52000,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',25000,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',27000,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',22500,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',54000,20,4)
,(7,'POLK','TECH',6,'09-22-1997',25000,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',32000,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',75000,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',56000,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',34000,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18000,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,2)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18000,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',24000,30,1);

select * from sto_uao_emp_limit_offset  order by empno LIMIT 7 ;
select * from sto_uao_emp_limit_offset  order by empno LIMIT 7 OFFSET 5;


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
) distributed by(id);

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
) distributed by (code);

CREATE TABLE countrylanguage_uao_using (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
) distributed by (countrycode,language);

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

COPY country_uao_using (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM '@abs_srcdir@/data/country.data' WITH NULL AS '';

COPY countrylanguage_uao_using (countrycode, "language", isofficial, percentage) FROM '@abs_srcdir@/data/countrylanguage.data';

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


-- @Description select outer/left join
--

DROP TABLE IF EXISTS sto_uao_emp_leftjoin cascade;
CREATE TABLE sto_uao_emp_leftjoin (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_leftjoin values
(1,'JOHNSON','ADMIN',6,'12-17-1990',18000,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',52000,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',25000,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',27000,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',22500,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',54000,20,4)
,(7,'POLK','TECH',6,'09-22-1997',25000,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',32000,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',75000,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',56000,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',34000,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18000,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,7)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18000,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',24000,30,1);

DROP TABLE IF EXISTS sto_uao_dept_leftjoin cascade;
CREATE TABLE sto_uao_dept_leftjoin (
   deptno INT NOT NULL,
   dname  VARCHAR(14),
   loc    VARCHAR(13)) distributed by (deptno);

insert into sto_uao_dept_leftjoin values
 (1,'ACCOUNTING','ST LOUIS')
,(2,'RESEARCH','NEW YORK')
,(3,'SALES','ATLANTA')
,(5,'LOGISTICS','BOSTON')
,(4, 'OPERATIONS','SEATTLE');

select ename, loc from sto_uao_emp_leftjoin left join sto_uao_dept_leftjoin on  sto_uao_dept_leftjoin.deptno = sto_uao_emp_leftjoin.dept order by 1 asc, 2 asc; 


-- @Description select with subquery
--
DROP TABLE IF EXISTS country_uao_subq cascade;

BEGIN;

CREATE TABLE country_uao_subq (
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

COPY country_uao_subq (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM '@abs_srcdir@/data/country.data' WITH NULL AS '';

COMMIT;

ANALYZE country_uao_subq;

-- Using subquery

select *
from
country_uao_subq
where code in (select code from country_uao_subq where gnp between 80000 and 100000)
and code in (select code from country_uao_subq where lifeexpectancy between 70 and 80)
and code in (select code from country_uao_subq where  indepyear between 1800 and 1900);


-- @Description select with with UNION , UNION ALL 
--
DROP TABLE IF EXISTS city_uao_union cascade;
DROP TABLE IF EXISTS country_uao_union cascade;
DROP TABLE IF EXISTS countrylanguage_uao_union cascade;

BEGIN;

CREATE TABLE city_uao_union (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) distributed by(id);

CREATE TABLE country_uao_union (
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

CREATE TABLE countrylanguage_uao_union (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
) distributed by (countrycode,language);

COPY city_uao_union (id, name, countrycode, district, population) FROM '@abs_srcdir@/data/city.data';

COPY country_uao_union (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM '@abs_srcdir@/data/country.data' WITH NULL AS '';

COPY countrylanguage_uao_union (countrycode, "language", isofficial, percentage) FROM '@abs_srcdir@/data/countrylanguage.data';

COMMIT;

ANALYZE city_uao_union;
ANALYZE country_uao_union;
ANALYZE countrylanguage_uao_union;


--queries Using Union All and except

with somecheapasiandiversecountries_uao as
(
 select FOO_uao.code,FOO_uao.COUNTRY,FOO_uao.CAPITAL,FOO_uao.headofstate,count(*) ASIAN_COUNT from
 (
 select country_uao_union.code,country_uao_union.name COUNTRY,city_uao_union.name CAPITAL,country_uao_union.headofstate
 from country_uao_union,city_uao_union
 where country_uao_union.capital = city_uao_union.id 
 and country_uao_union.gnp < 10000
 and country_uao_union.region = 'Southeast Asia'
 and country_uao_union.continent = 'Asia'
 
 UNION ALL

 select country_uao_union.code,country_uao_union.name COUNTRY,city_uao_union.name CAPITAL,country_uao_union.headofstate
 from country_uao_union,city_uao_union
 where country_uao_union.capital = city_uao_union.id 
 and country_uao_union.gnp < 10000
 and country_uao_union.region = 'Eastern Asia'
 and country_uao_union.continent = 'Asia'

 UNION ALL

 select country_uao_union.code,country_uao_union.name COUNTRY,city_uao_union.name CAPITAL,country_uao_union.headofstate
 from country_uao_union,city_uao_union
 where country_uao_union.capital = city_uao_union.id 
 and country_uao_union.gnp < 10000
 and country_uao_union.region = 'Middle East'
 and country_uao_union.continent = 'Asia'
 ) FOO_uao, countrylanguage_uao_union
 where FOO_uao.code = countrylanguage_uao_union.countrycode
 group by FOO_uao.code,FOO_uao.COUNTRY,FOO_uao.CAPITAL,FOO_uao.headofstate,countrylanguage_uao_union.countrycode
 HAVING count(*) >=
 (select min(CNT) FROM
   (select count(*) CNT,country_uao_union.code from countrylanguage_uao_union,country_uao_union
    where countrylanguage_uao_union.countrycode=country_uao_union.code
    and country_uao_union.continent = 'Asia'
    and country_uao_union.region = 'Southern and Central Asia'
    group by country_uao_union.code
   ) FOO1_uao
 )
)

select FOO_uao.code,FOO_uao.COUNTRY,FOO_uao.CAPITAL,FOO_uao.headofstate,count(*) compared_with_cheap_asian_cnt
from
(
select country_uao_union.code,country_uao_union.name COUNTRY,city_uao_union.name CAPITAL,country_uao_union.headofstate
from country_uao_union,city_uao_union
where country_uao_union.capital = city_uao_union.id 
and country_uao_union.continent = 'North America'


UNION ALL

select country_uao_union.code,country_uao_union.name COUNTRY,city_uao_union.name CAPITAL,country_uao_union.headofstate
from country_uao_union,city_uao_union
where country_uao_union.capital =	city_uao_union.id	
and country_uao_union.continent =	'South America'

EXCEPT
select country_uao_union.code,country_uao_union.name COUNTRY,city_uao_union.name CAPITAL,country_uao_union.headofstate
from country_uao_union,city_uao_union
where 
country_uao_union.capital =     city_uao_union.id
and country_uao_union.code='GTM'
) FOO_uao,countrylanguage_uao_union

where FOO_uao.code = countrylanguage_uao_union.countrycode
group by FOO_uao.code,FOO_uao.COUNTRY,FOO_uao.CAPITAL,FOO_uao.headofstate

HAVING count(*)  >=
 (select min(ASIAN_COUNT) FROM
   (select ASIAN_COUNT FROM somecheapasiandiversecountries_uao,country_uao_union
    where somecheapasiandiversecountries_uao.code = country_uao_union.code
    and country_uao_union.gnp >= country_uao_union.gnpold
   ) ASIANCOUNT
 )
order by compared_with_cheap_asian_cnt desc, 1 asc
LIMIT 10;


-- @Description PostgreSQL port of the MySQL "World" database.
--
-- The sample data used in the world database is Copyright Statistics 
-- Finland, http://www.stat.fi/worldinfigures.
--


-- Modified to use it with GPDB

DROP TABLE IF EXISTS city_uao cascade;
DROP TABLE IF EXISTS country_uao cascade;
DROP TABLE IF EXISTS countrylanguage_uao cascade;

BEGIN;

CREATE TABLE city_uao (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) distributed by(id);

CREATE TABLE country_uao (
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

CREATE TABLE countrylanguage_uao (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
) distributed by (countrycode,language);

COPY city_uao (id, name, countrycode, district, population) FROM '@abs_srcdir@/data/city.data';

COPY country_uao (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM '@abs_srcdir@/data/country.data' WITH NULL AS '';

COPY countrylanguage_uao (countrycode, "language", isofficial, percentage) FROM '@abs_srcdir@/data/countrylanguage.data';

COMMIT;

ANALYZE city_uao;
ANALYZE country_uao;
ANALYZE countrylanguage_uao;

-- queries with one CTE that is referenced once

-- Using CTE in the FROM clause


--queries Using a CTE in the HAVING clause and Union All


with somecheapasiandiversecountries_uao as
(
 select FOO_uao.code,FOO_uao.COUNTRY,FOO_uao.CAPITAL,FOO_uao.headofstate,count(*) ASIAN_COUNT from
 (
 select country_uao.code,country_uao.name COUNTRY,city_uao.name CAPITAL,country_uao.headofstate
 from country_uao,city_uao
 where country_uao.capital = city_uao.id 
 and country_uao.gnp < 10000
 and country_uao.region = 'Southeast Asia'
 and country_uao.continent = 'Asia'
 UNION ALL
 select country_uao.code,country_uao.name COUNTRY,city_uao.name CAPITAL,country_uao.headofstate
 from country_uao,city_uao
 where country_uao.capital = city_uao.id 
 and country_uao.gnp < 10000
 and country_uao.region = 'Eastern Asia'
 and country_uao.continent = 'Asia'
 UNION ALL
 select country_uao.code,country_uao.name COUNTRY,city_uao.name CAPITAL,country_uao.headofstate
 from country_uao,city_uao
 where country_uao.capital = city_uao.id 
 and country_uao.gnp < 10000
 and country_uao.region = 'Middle East'
 and country_uao.continent = 'Asia'
 ) FOO_uao, countrylanguage_uao
 where FOO_uao.code = countrylanguage_uao.countrycode
 group by FOO_uao.code,FOO_uao.COUNTRY,FOO_uao.CAPITAL,FOO_uao.headofstate,countrylanguage_uao.countrycode
 HAVING count(*) >=
 (select min(CNT) FROM
   (select count(*) CNT,country_uao.code from countrylanguage_uao,country_uao
    where countrylanguage_uao.countrycode=country_uao.code
    and country_uao.continent = 'Asia'
    and country_uao.region = 'Southern and Central Asia'
    group by country_uao.code
   ) FOO1_uao
 )
)
select FOO_uao.code,FOO_uao.COUNTRY,FOO_uao.CAPITAL,FOO_uao.headofstate,count(*) compared_with_cheap_asian_cnt
from
(
select country_uao.code,country_uao.name COUNTRY,city_uao.name CAPITAL,country_uao.headofstate
from country_uao,city_uao
where country_uao.capital = city_uao.id 
and country_uao.continent = 'North America'


UNION ALL

select country_uao.code,country_uao.name COUNTRY,city_uao.name CAPITAL,country_uao.headofstate
from country_uao,city_uao
where country_uao.capital =	city_uao.id	
and country_uao.continent =	'South America'
) FOO_uao,countrylanguage_uao

where FOO_uao.code = countrylanguage_uao.countrycode
group by FOO_uao.code,FOO_uao.COUNTRY,FOO_uao.CAPITAL,FOO_uao.headofstate

HAVING count(*)  >=
 (select min(ASIAN_COUNT) FROM
   (select ASIAN_COUNT FROM somecheapasiandiversecountries_uao,country_uao
    where somecheapasiandiversecountries_uao.code = country_uao.code
    and country_uao.gnp >= country_uao.gnpold
   ) ASIANCOUNT
 )
order by compared_with_cheap_asian_cnt desc , 1 desc
LIMIT 10;


-- @description : Create view and execute select from view
--
DROP TABLE IF EXISTS sto_uao_emp_forview cascade;
CREATE TABLE sto_uao_emp_forview (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_forview values 
 (1,'JOHNSON','ADMIN',6,'12-17-1990',18000,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',52000,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',25000,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',27000,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',22500,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',54000,20,4)
,(7,'POLK','TECH',6,'09-22-1997',25000,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',32000,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',75000,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',56000,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',34000,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18000,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,2)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18000,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',24000,30,1);
DROP TABLE IF EXISTS sto_uao_dept_forview cascade;
CREATE TABLE sto_uao_dept_forview (
   deptno INT NOT NULL,
   dname  VARCHAR(14),
   loc    VARCHAR(13)) distributed by (deptno);

insert into sto_uao_dept_forview values 
 (1,'ACCOUNTING','ST LOUIS')
,(2,'RESEARCH','NEW YORK')
,(3,'SALES','ATLANTA')
,(4, 'OPERATIONS','SEATTLE');


-- Create view
create or replace view sto_uao_emp_view as
select empno,ename,job,mgr,hiredate,sal,comm,dname,loc from sto_uao_dept_forview, sto_uao_emp_forview where sto_uao_dept_forview.deptno = sto_uao_emp_forview.dept; 

select * from sto_uao_emp_view  where mgr is not NULL order by 1;

-- @Description select with where clause
--
DROP TABLE IF EXISTS city_uao_where cascade;
DROP TABLE IF EXISTS country_uao_where cascade;
DROP TABLE IF EXISTS countrylanguage_uao_where cascade;

BEGIN;

CREATE TABLE city_uao_where (
    id integer NOT NULL,
    name text NOT NULL,
    countrycode character(3) NOT NULL,
    district text NOT NULL,
    population integer NOT NULL
) distributed by(id);

CREATE TABLE country_uao_where (
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

CREATE TABLE countrylanguage_uao_where (
    countrycode character(3) NOT NULL,
    "language" text NOT NULL,
    isofficial boolean NOT NULL,
    percentage real NOT NULL
) distributed by (countrycode,language);

COPY city_uao_where (id, name, countrycode, district, population) FROM '@abs_srcdir@/data/city.data';
COPY country_uao_where (code, name, continent, region, surfacearea, indepyear, population, lifeexpectancy, gnp, gnpold, localname, governmentform, headofstate, capital, code2) FROM '@abs_srcdir@/data/country.data' WITH NULL AS '';
COPY countrylanguage_uao_where (countrycode, "language", isofficial, percentage) FROM '@abs_srcdir@/data/countrylanguage.data';

COMMIT;

ANALYZE city_uao_where;
ANALYZE country_uao_where;
ANALYZE countrylanguage_uao_where;


-- Using  WHERE clause

with lang_total_uao as
( select lang_count,countrycode from
  (
  (
   select count(*) as lang_count,country_uao_where.code,countrylanguage_uao_where.countrycode
  from country_uao_where join countrylanguage_uao_where on (country_uao_where.code=countrylanguage_uao_where.countrycode and governmentform='Federal Republic')
  group by country_uao_where.code,countrylanguage_uao_where.countrycode order by country_uao_where.code)
 
   UNION ALL
 
  (
   select count(*) as lang_count,country_uao_where.code,countrylanguage_uao_where.countrycode
  from country_uao_where join countrylanguage_uao_where on (country_uao_where.code=countrylanguage_uao_where.countrycode and governmentform='Monarchy')
  group by country_uao_where.code,countrylanguage_uao_where.countrycode order by country_uao_where.code)
 
 ) FOO1_uao
)
select * from
(
select count(*) as cnt,country_uao_where.code,country_uao_where.name 
from
country_uao_where,countrylanguage_uao_where
where country_uao_where.code=countrylanguage_uao_where.countrycode group by country_uao_where.code,country_uao_where.name) AS FOO_uao
where foo_uao.cnt = (select max(lang_count) from lang_total_uao) order by foo_uao.code;

-- @Description select wildcard 
--

DROP TABLE IF EXISTS sto_uao_emp_wildcard cascade;
CREATE TABLE sto_uao_emp_wildcard (
   empno    INT  ,
   ename    VARCHAR(10),
   job      VARCHAR(9),
   mgr      INT NULL,
   hiredate DATE,
   sal      NUMERIC(7,2),
   comm     NUMERIC(7,2) NULL,
   dept     INT) distributed by (empno);

insert into sto_uao_emp_wildcard values
(1,'JOHNSON','ADMIN',6,'12-17-1990',18000,10,4)
,(2,'HARDING','MANAGER',9,'02-02-1998',52000,15,3)
,(3,'TAFT','SALES I',2,'01-02-1996',25000,20,3)
,(4,'HOOVER','SALES I',2,'04-02-1990',27000,15,3)
,(5,'LINCOLN','TECH',6,'06-23-1994',22500,15,4)
,(6,'GARFIELD','MANAGER',9,'05-01-1993',54000,20,4)
,(7,'POLK','TECH',6,'09-22-1997',25000,15,4)
,(8,'GRANT','ENGINEER',10,'03-30-1997',32000,20,2)
,(9,'JACKSON','CEO',NULL,'01-01-1990',75000,30,4)
,(10,'FILLMORE','MANAGER',9,'08-09-1994',56000,20,2)
,(11,'ADAMS','ENGINEER',10,'03-15-1996',34000,20,2)
,(12,'WASHINGTON','ADMIN',6,'04-16-1998',18000,15,4)
,(13,'MONROE','ENGINEER',10,'12-03-2000',30000,20,7)
,(14,'ROOSEVELT','CPA',9,'10-12-1995',35000,30,1)
,(15,'More','ENGINEER',9,'10-12-1994',25000,20,2)
,(16,'ROSE','SALES I',10,'10-12-1999',18000,15,3)
,(17,'CLINT','CPA',2,'10-12-2001',24000,30,1);


DROP TABLE IF EXISTS sto_uao_dept_wildcard cascade;
CREATE TABLE sto_uao_dept_wildcard (
   deptno INT NOT NULL,
   dname  VARCHAR(14),
   loc    VARCHAR(13)) distributed by (deptno);

insert into sto_uao_dept_wildcard values
 (1,'ACCOUNTING','ST LOUIS')
,(2,'RESEARCH','NEW YORK')
,(3,'SALES','ATLANTA')
,(5,'LOGISTICS','BOSTON')
,(4, 'OPERATIONS','SEATTLE');

select ename from sto_uao_emp_wildcard where dept in ( select deptno from  sto_uao_dept_wildcard  where dname like '%S')
AND ename like 'H%' order by 1 asc LIMIT 15; 

-- @Description select with window 
--

DROP TABLE IF EXISTS uao_orders;
CREATE TABLE uao_orders(order_id serial , customer_id integer, 
      order_datetime timestamp, order_total numeric(10,2));

INSERT INTO uao_orders(customer_id, order_datetime, order_total)
VALUES (1,'2009-05-01 10:00 AM', 500),
    (1,'2009-05-15 11:00 AM', 650),
    (2,'2009-05-11 11:00 PM', 100),
    (2,'2009-05-12 11:00 PM', 5),
       (3,'2009-04-11 11:00 PM', 100),
          (1,'2009-05-20 11:00 AM', 3);

select * from uao_orders order by order_id;
SELECT row_number()  OVER(window_custtime) As rtime_d, 
n.customer_id, lead(order_id) OVER(window_custtime) As cr_num, n.order_id, n.order_total
FROM uao_orders AS n 
WINDOW window_custtime AS (PARTITION BY n.customer_id 
                               ORDER BY n.customer_id, n.order_id, n.order_total)
ORDER BY  1, 2;


