--
-- All derived from MPP-3613: use of incorrect parameters in queries
-- which intermix initPlans with "internal" parameters.
--
--
-- Name: in_crm; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace: 
--

CREATE TABLE in_crm (
    alias_id integer,
    subs_type character(8),
    pay_type character(7),
    switch_on_date date,
    switch_off_date date,
    rem_mnths integer,
    gender character(1),
    age integer,
    handset_age integer,
    value real,
    tariff_plan character(9),
    handset_type character(6),
    segment character(2),
    flg integer,
    zip integer,
    inact integer,
    name_origin character(9),
    churned boolean,
    churn_score real,
    churn_date date,
    product_x1_propensity_score real,
    product_x1_taken boolean,
    product_x1_taken_date date,
    product_x1_possible boolean,
    date_inserted date,
    last_call_date timestamp without time zone
);


--
-- Name: in_product; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace: 
--

CREATE TABLE in_product (
    alias_id integer NOT NULL,
    product_id smallint NOT NULL,
    product_name character varying(100),
    product_taken smallint,
    product_score double precision,
    date_inserted date NOT NULL
) distributed by (alias_id);


--
-- Name: module_job_parameters; Type: TABLE; Schema: public; Owner: xsl; Tablespace: 
--

CREATE TABLE module_job_parameters (
    mod_job_id integer NOT NULL,
    key character varying(50) NOT NULL,
    value character varying(50)
) distributed by (mod_job_id);

--
-- Name: module_targets; Type: TABLE; Schema: public; Owner: gpadmin; Tablespace: 
--

DROP TABLE IF EXISTS module_targets;

CREATE TABLE module_targets (
    mod_job_id integer NOT NULL,
    alias_id integer NOT NULL,
    target smallint
);


--
-- Data for Name: in_crm; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY in_crm (alias_id, subs_type, pay_type, switch_on_date, switch_off_date, rem_mnths, gender, age, handset_age, value, tariff_plan, handset_type, segment, flg, zip, inact, name_origin, churned, churn_score, churn_date, product_x1_propensity_score, product_x1_taken, product_x1_taken_date, product_x1_possible, date_inserted, last_call_date) FROM stdin;
59942104	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
58872517	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
67081692	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56424928	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
54245766	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
75447684	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
53965598	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55523751	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
55034097	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
76167338	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
55340005	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
63452371	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
55666691	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
58701524	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55783105	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
56608677	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
65991732	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56941761	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
70177746	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
56494032	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
74681978	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
57160769	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
53965598	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
57401289	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
56796459	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
58222785	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55340005	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
63452371	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
55666691	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
56609739	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56417504	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
56608677	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
57249357	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
62792843	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
70177746	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56494032	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
70144535	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
57137290	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
54626999	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
57909555	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55034097	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
75532384	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
73572029	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
63450299	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
57539619	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
58701524	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
56417504	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56179571	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
54494731	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56941761	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
57098291	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2007-11-22	\N	\N	\N	\N	2008-02-01	\N
61292015	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
60863168	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
57137290	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
53965598	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
56694316	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55896429	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
59809969	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
65738506	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
53889342	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2007-10-16	\N	\N	\N	\N	2008-01-01	\N
68560154	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54519364	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
54391913	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56788627	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
57249357	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
63303003	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
70177746	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
56494032	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
70144535	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
58328677	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
75751554	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
62409967	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
55604306	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
62385445	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
68677503	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
63450299	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
57539619	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
56609739	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54393600	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
80613616	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54494731	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
60207817	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
57098291	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
56494032	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
57108978	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
53867740	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
73802931	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56694316	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
55604306	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
59809969	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
58309737	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
53889342	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
58170561	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54519364	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
54391913	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
56179571	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
56702003	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55409609	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
70177746	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
56424928	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
57108978	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
67306587	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
75751554	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
53770921	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
55604306	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
63888848	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
53952715	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
53889342	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
58170561	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
54519364	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54391913	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
77810777	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
78610516	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
60207817	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
52887252	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
61292015	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
76411616	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
53867740	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
72847713	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
55283856	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54564659	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
55740398	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
58309737	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
60879814	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
56292221	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
53959165	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
66031871	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
80613616	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
60440604	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
63303003	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
54606371	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
70427469	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
76961560	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
54644900	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
72847713	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
55283856	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
77582221	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
57766355	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56414216	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
64195417	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
57596271	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
62239988	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55783105	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
56179571	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
63352159	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
55409609	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
54606371	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
58105403	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
57691310	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
61241334	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
72379397	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
52377660	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
54158137	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
63888848	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
73346572	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55760049	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
57596271	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
53908948	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
54391913	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
57004417	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55571420	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55101593	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
52530980	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
58105403	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
55855142	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
64253515	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
72847713	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
62409967	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
55092768	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
54584778	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
70624189	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
56464873	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
64231534	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
79822780	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
57626522	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
55260623	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
66315273	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
64882815	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
55078048	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
54216804	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
58225443	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
56786336	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
58005087	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
58329414	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
57465723	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
62923062	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
54367100	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
53834416	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56769588	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
53959165	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
57626522	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
60897733	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56643590	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55101593	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
55078048	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
62295386	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
76961560	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
75755053	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
58005087	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
55365365	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
63881989	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
59263153	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
66054512	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
53927250	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
56769588	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
53959165	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
54608631	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
61084952	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
56120856	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
57349959	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
55078048	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54216804	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
54307412	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
54644900	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
53863481	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
62409967	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
54158137	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
62878599	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
70624189	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
79283636	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
69882274	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
53908948	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
76077591	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
55260623	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
66315273	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
82104607	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
55078048	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
62295386	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
55855142	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
70048019	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
53863481	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
64867410	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
54405144	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
72652964	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
55646939	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
54261725	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
69882274	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
69862351	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
67554222	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54151518	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
56120856	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
82104607	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
55972966	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
62295386	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
54307412	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
75755053	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
60355437	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
55365365	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
65941401	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
62432413	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
61561577	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
79283636	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
60939010	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
57477682	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
53943704	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54151518	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
56770341	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
58794666	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
67639666	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54080319	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
54307412	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54171450	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
60355437	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
78466183	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
54584287	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
75398461	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
52192420	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
56909782	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
69882274	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
57263049	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
55037794	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
61537124	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
54087426	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
52184097	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
67639666	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
54080319	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
72724204	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
75755053	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-05-01	\N
56464368	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-01-01	\N
59659566	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-04-01	\N
56826889	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
63382244	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-03-01	\N
52192420	Consumer	PostPay	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	\N	2008-02-01	\N
\.


--
-- Data for Name: in_product; Type: TABLE DATA; Schema: public; Owner: gpadmin
--

COPY in_product (alias_id, product_id, product_name, product_taken, product_score, date_inserted) FROM stdin;
52832898	2	open_end	1	\N	2008-05-01
69528497	2	open_end	0	\N	2008-05-01
77591686	2	open_end	0	\N	2008-05-01
77681522	2	open_end	1	\N	2008-05-01
80103585	2	open_end	1	\N	2008-05-01
58086780	2	open_end	0	\N	2008-05-01
82894720	2	open_end	0	\N	2008-05-01
58488280	2	open_end	0	\N	2008-05-01
77379466	2	open_end	0	\N	2008-01-01
58934326	2	open_end	0	\N	2008-05-01
66913100	2	open_end	0	\N	2008-05-01
58311690	2	open_end	0	\N	2008-05-01
\.

COPY module_job_parameters (mod_job_id, key, value) FROM stdin;
30	time_inserted	2008-06-30 10:16:11
30	parameter_status	0
30	xsl_job_id	25
30	t4	2008-03-03
30	t2	2008-03-02
30	job_type	churn
30	model_id	683
30	t5	2008-05-04
30	usecase	export
30	tCRM	2008-03-01
30	t1	2007-12-03
\.

COPY module_targets (mod_job_id, alias_id, target) FROM stdin;
\.

ALTER TABLE ONLY in_product
    ADD CONSTRAINT in_product_pkey PRIMARY KEY (alias_id, product_id, date_inserted);

ALTER TABLE ONLY module_job_parameters
    ADD CONSTRAINT module_job_parameters_pkey PRIMARY KEY (mod_job_id, key);

--
-- Name: create_target_list(integer, integer); Type: FUNCTION; Schema: public; Owner: gpadmin
--

CREATE OR REPLACE FUNCTION create_target_list(integer, integer) RETURNS SETOF integer
    AS $_$
DECLARE 
    retval integer;

    BEGIN

    RETURN NEXT $1;
    RETURN NEXT $2;

    IF $2=1::integer THEN

      INSERT INTO module_targets
      SELECT DISTINCT $1 as mod_job_id, a.alias_id,b.target 
      FROM in_crm a LEFT JOIN
        (SELECT DISTINCT alias_id, 
          CASE
            WHEN max(churn_date) IS NOT NULL 
              AND max(churn_date) <= 
                (SELECT to_date(pt.value::text, 'YYYY-MM-DD'::text)
                 FROM module_job_parameters pt
                 WHERE pt.mod_job_id = $1
                 AND pt."key"::text = 't5'::text
                )
              AND max(churn_date) >= 
                (SELECT to_date(pt.value::text, 'YYYY-MM-DD'::text)
                 FROM module_job_parameters pt
                 WHERE pt.mod_job_id = $1 
                 AND pt."key"::text = 't4'::text
                )
              THEN 1
              ELSE -1
           END AS target
           FROM in_crm
           GROUP BY alias_id) b on a.alias_id=b.alias_id
        WHERE a.date_inserted = 
           (SELECT to_date(pt.value::text, 'YYYY-MM-DD'::text)
            FROM module_job_parameters pt
            WHERE pt.mod_job_id = $1 
            AND pt."key"::text = 'tCRM'::text
           )
        AND a.subs_type='Consumer'::text
        AND a.pay_type='PostPay';

      RETURN NEXT 1;

    ELSIF $2=2 THEN 

      INSERT INTO module_targets
      SELECT DISTINCT $1 as mod_job_id, ip.alias_id, 
	  CASE
              WHEN ip.alias_id IS NOT NULL AND ip.product_taken = 1 THEN 1
              ELSE -1
          END AS target
      FROM in_product ip
      WHERE ip.product_id = 
            (SELECT pt.value::text::integer 
	     FROM module_job_parameters pt
      	     WHERE pt.mod_job_id = $1
               AND pt."key"::text = 'product_id'::text
	    )
        AND ip.date_inserted =
	    (SELECT to_date(pt.value::text, 'YYYY-MM-DD'::text)
	     FROM module_job_parameters pt
      	     WHERE pt.mod_job_id = $1 
               AND pt."key"::text = 'tCRM'::text);


      INSERT INTO module_modelling_exceptions
      SELECT cr.alias_id,
	CASE WHEN ip.product_taken=1 THEN 1 
	     WHEN ip.product_taken IS NULL THEN -1
	     ELSE 0 
	END AS prod_status
      FROM (
      	   select alias_id 
	   from in_crm 
	   where date_inserted =
	     (SELECT to_date(pt.value::text, 'YYYY-MM-DD'::text)
	      FROM module_job_parameters pt
      	      WHERE pt.mod_job_id = $1 
                AND pt."key"::text = 'tCRM'::text)
	   ) cr 
      LEFT JOIN in_product ip on ip.alias_id=cr.alias_id 
      WHERE ip.product_id = 
            (SELECT pt.value::text::integer 
	     FROM module_job_parameters pt
      	     WHERE pt.mod_job_id = $1
               AND pt."key"::text = 'product_id'::text
	    )
        AND ip.date_inserted =
	    (SELECT to_date(pt.value::text, 'YYYY-MM-DD'::text)
	     FROM module_job_parameters pt
      	     WHERE pt.mod_job_id = $1 
               AND pt."key"::text = 'tCRM'::text);


      RETURN NEXT 2;

    ELSE 

      SELECT 0 into retval;
      RETURN NEXT 3;
    END IF;

    SELECT count(*) FROM module_targets into retval;
 
RETURN NEXT retval;
END;
$_$
    LANGUAGE plpgsql MODIFIES SQL DATA;

CREATE OR REPLACE FUNCTION create_target_list_sql(integer) RETURNS int4
    AS $$
      INSERT INTO module_targets
      SELECT DISTINCT $1 as mod_job_id, a.alias_id,b.target 
      FROM in_crm a LEFT JOIN
        (SELECT DISTINCT alias_id, 
          CASE
            WHEN max(churn_date) IS NOT NULL 
              AND max(churn_date) <= 
                (SELECT to_date(pt.value::text, 'YYYY-MM-DD'::text)
                 FROM module_job_parameters pt
                 WHERE pt.mod_job_id = $1
                 AND pt."key"::text = 't5'::text
                )
              AND max(churn_date) >= 
                (SELECT to_date(pt.value::text, 'YYYY-MM-DD'::text)
                 FROM module_job_parameters pt
                 WHERE pt.mod_job_id = $1 
                 AND pt."key"::text = 't4'::text
                )
              THEN 1
              ELSE -1
           END AS target
           FROM in_crm
           GROUP BY alias_id) b on a.alias_id=b.alias_id
        WHERE a.date_inserted = 
           (SELECT to_date(pt.value::text, 'YYYY-MM-DD'::text)
            FROM module_job_parameters pt
            WHERE pt.mod_job_id = $1 
            AND pt."key"::text = 'tCRM'::text
           )
        AND a.subs_type='Consumer'::text
        AND a.pay_type='PostPay';

	select count(*)::int4 from module_targets;
$$
    LANGUAGE sql MODIFIES SQL DATA;

truncate module_targets;
select * from create_target_list(30, 1);
truncate module_targets;
select * from create_target_list_sql(30);

--DROP TABLE IF EXISTS module_targets;

--CREATE TABLE module_targets (
--    mod_job_id integer NOT NULL,
--    alias_id integer NOT NULL,
--    target integer
--);

--truncate module_targets;
--select * from create_target_list(30, 1);
--truncate module_targets;
--select * from create_target_list_sql(30);
