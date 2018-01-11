/*-------------------------------------------------------------------------
 *
 * pg_aggregate.h
 *	  definition of the system "aggregate" relation (pg_aggregate)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_aggregate.h,v 1.68 2009/01/01 17:23:56 momjian Exp $
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_AGGREGATE_H
#define PG_AGGREGATE_H

#include "catalog/genbki.h"
#include "nodes/pg_list.h"

/* ----------------------------------------------------------------
 *		pg_aggregate definition.
 *
 *		cpp turns this into typedef struct FormData_pg_aggregate
 *
 *	aggfnoid			pg_proc OID of the aggregate itself
 *	aggkind				aggregate kind, see AGGKIND_ categories below
 *	aggnumdirectargs	number of arguments that are "direct" arguments
 *	aggtransfn			transition function
 *	agginvtransfn		inverse transition function
 *  aggprelimfn			preliminary aggregation function
 *  agginvprelimfn		inverse preliminary aggregation function
 *	aggfinalfn			final function (0 if none)
 *	aggfinalextra		true to pass extra dummy arguments to aggfinalfn
 *	aggsortop			associated sort operator (0 if none)
 *	aggtranstype		type of aggregate's transition (state) data
 *	agginitval			initial value for transition state (can be NULL)
 *  aggorder            array of ordering columns (can be NULL)
 * ----------------------------------------------------------------
 */
#define AggregateRelationId  2600

CATALOG(pg_aggregate,2600) BKI_WITHOUT_OIDS
{
	regproc		aggfnoid;
	char		aggkind;
	int2		aggnumdirectargs;
	regproc		aggtransfn;
	regproc		agginvtransfn;	/* MPP windowing */
	regproc		aggprelimfn;	/* MPP 2-phase agg */
	regproc		agginvprelimfn;	/* MPP windowing */
	regproc		aggfinalfn;
	bool		aggfinalextra;
	Oid			aggsortop;
	Oid			aggtranstype;
	text		agginitval;		/* VARIABLE LENGTH FIELD */
} FormData_pg_aggregate;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(aggfnoid REFERENCES pg_proc(oid));
FOREIGN_KEY(aggtransfn REFERENCES pg_proc(oid));
FOREIGN_KEY(agginvtransfn REFERENCES pg_proc(oid));
FOREIGN_KEY(aggprelimfn REFERENCES pg_proc(oid));
FOREIGN_KEY(agginvprelimfn REFERENCES pg_proc(oid));
FOREIGN_KEY(aggfinalfn REFERENCES pg_proc(oid));
FOREIGN_KEY(aggsortop REFERENCES pg_operator(oid));
FOREIGN_KEY(aggtranstype REFERENCES pg_type(oid));

/* ----------------
 *		Form_pg_aggregate corresponds to a pointer to a tuple with
 *		the format of pg_aggregate relation.
 * ----------------
 */
typedef FormData_pg_aggregate *Form_pg_aggregate;

/* ----------------
 *		compiler constants for pg_aggregate
 * ----------------
 */
#define Natts_pg_aggregate					12
#define Anum_pg_aggregate_aggfnoid			1
#define Anum_pg_aggregate_aggkind			2
#define Anum_pg_aggregate_aggnumdirectargs	3
#define Anum_pg_aggregate_aggtransfn		4
#define Anum_pg_aggregate_agginvtransfn		5
#define Anum_pg_aggregate_aggprelimfn		6
#define Anum_pg_aggregate_agginvprelimfn	7
#define Anum_pg_aggregate_aggfinalfn		8
#define Anum_pg_aggregate_aggfinalextra		9
#define Anum_pg_aggregate_aggsortop			10
#define Anum_pg_aggregate_aggtranstype		11
#define Anum_pg_aggregate_agginitval		12

/*
 * Symbolic values for aggkind column.	We distinguish normal aggregates
 * from ordered-set aggregates (which have two sets of arguments, namely
 * direct and aggregated arguments) and from hypothetical-set aggregates
 * (which are a subclass of ordered-set aggregates in which the last
 * direct arguments have to match up in number and datatypes with the
 * aggregated arguments).
 */
#define AGGKIND_NORMAL			'n'
#define AGGKIND_ORDERED_SET		'o'
#define AGGKIND_HYPOTHETICAL	'h'

/* Use this macro to test for "ordered-set agg including hypothetical case" */
#define AGGKIND_IS_ORDERED_SET(kind)  ((kind) != AGGKIND_NORMAL)

/*
 * Constant definitions of pg_proc.oid for aggregate functions.
 */
#define AGGFNOID_COUNT_ANY 2147 /* returns INT8OID */
#define AGGFNOID_SUM_BIGINT 2107 /* returns NUMERICOID */

/* ----------------
 * initial contents of pg_aggregate
 * ---------------
 */

/* avg */
DATA(insert ( 2100	n 0 int8_avg_accum	int8_avg_decum int8_avg_amalg int8_avg_demalg int8_avg	f 0 17 ""));
DATA(insert ( 2101	n 0 int4_avg_accum	int4_avg_decum int8_avg_amalg int8_avg_demalg int8_avg	f 0 17 ""));
DATA(insert ( 2102	n 0 int2_avg_accum	int2_avg_decum int8_avg_amalg int8_avg_demalg int8_avg	f 0 17 ""));
DATA(insert ( 2103	n 0 numeric_avg_accum numeric_avg_decum	 numeric_avg_amalg numeric_avg_demalg numeric_avg f 0	17 ""));
DATA(insert ( 2104	n 0 float4_avg_accum float4_avg_decum float8_avg_amalg float8_avg_demalg float8_avg	f 0 17 ""));
DATA(insert ( 2105	n 0 float8_avg_accum float8_avg_decum float8_avg_amalg float8_avg_demalg float8_avg	f 0 17 ""));
DATA(insert ( 2106	n 0 interval_accum	interval_decum interval_amalg	interval_demalg interval_avg	f 0	1187	"{0 second,0 second}"));

/* sum */
DATA(insert ( 2107	n 0 int8_sum		int8_invsum 	numeric_add		numeric_sub -				f 0	1700	_null_));
DATA(insert ( 2108	n 0 int4_sum		int4_invsum 	int8pl			int8mi -				f 0	20		_null_));
DATA(insert ( 2109	n 0 int2_sum		int2_invsum		int8pl			int8mi -				f 0	20		_null_));
DATA(insert ( 2110	n 0 float4pl		float4mi float4pl			float4mi -				f 0	700		_null_));
DATA(insert ( 2111	n 0 float8pl		float8mi float8pl			float8mi -				f 0	701		_null_));
DATA(insert ( 2112	n 0 cash_pl			cash_mi cash_pl			cash_mi -				f 0	790		_null_ ));
DATA(insert ( 2113	n 0 interval_pl		interval_mi interval_pl		interval_mi -				f 0	1186	_null_));
DATA(insert ( 2114	n 0 numeric_add		numeric_sub numeric_add		numeric_sub -				f 0	1700	_null_));

/* max */
DATA(insert ( 2115	n 0 int8larger		- int8larger		- -				f 413		20		_null_));
DATA(insert ( 2116	n 0 int4larger		- int4larger		- -				f 521		23		_null_));
DATA(insert ( 2117	n 0 int2larger		- int2larger		- -				f 520		21		_null_));
DATA(insert ( 2118	n 0 oidlarger		- oidlarger			- -				f 610		26		_null_));
DATA(insert ( 2119	n 0 float4larger	- float4larger		- -				f 623		700		_null_));
DATA(insert ( 2120	n 0 float8larger	- float8larger		- -				f 674		701		_null_));
DATA(insert ( 2121	n 0 int4larger		- int4larger		- -				f 563		702		_null_));
DATA(insert ( 2122	n 0 date_larger		- date_larger		- -				f 1097	1082	_null_));
DATA(insert ( 2123	n 0 time_larger		- time_larger		- -				f 1112	1083	_null_));
DATA(insert ( 2124	n 0 timetz_larger	- timetz_larger		- -				f 1554	1266	_null_));
DATA(insert ( 2125	n 0 cashlarger		- cashlarger		- -				f 903		790		_null_));
DATA(insert ( 2126	n 0 timestamp_larger - timestamp_larger - -				f 2064	1114	_null_));
DATA(insert ( 2127	n 0 timestamptz_larger - timestamptz_larger - -			f 1324	1184	_null_));
DATA(insert ( 2128	n 0 interval_larger - interval_larger	- -				f 1334	1186	_null_));
DATA(insert ( 2129	n 0 text_larger		- text_larger		- -				f 666		25		_null_));
DATA(insert ( 2130	n 0 numeric_larger	- numeric_larger	- -				f 1756	1700	_null_));
DATA(insert ( 2050	n 0 array_larger	- array_larger		- -				f 1073	2277	_null_));
DATA(insert ( 2244	n 0 bpchar_larger	- bpchar_larger		- -				f 1060	1042	_null_));
DATA(insert ( 2797	n 0 tidlarger		- tidlarger			- -				f 2800	27		_null_));
DATA(insert ( 3526	n 0 enum_larger		- enum_larger		- -				f 3519	3500	_null_));
DATA(insert ( 3332	n 0 gpxlogloclarger	- gpxlogloclarger	- -				f 3328	3310	_null_));

/* min */
DATA(insert ( 2131	n 0 int8smaller		- int8smaller		- -				f 412		20		_null_));
DATA(insert ( 2132	n 0 int4smaller		- int4smaller		- -				f 97		23		_null_));
DATA(insert ( 2133	n 0 int2smaller		- int2smaller		- -				f 95		21		_null_));
DATA(insert ( 2134	n 0 oidsmaller		- oidsmaller		- -				f 609		26		_null_));
DATA(insert ( 2135	n 0 float4smaller	- float4smaller		- -				f 622		700		_null_));
DATA(insert ( 2136	n 0 float8smaller	- float8smaller		- -				f 672		701		_null_));
DATA(insert ( 2137	n 0 int4smaller		- int4smaller		- -				f 562		702		_null_));
DATA(insert ( 2138	n 0 date_smaller	- date_smaller		- -				f 1095	1082	_null_));
DATA(insert ( 2139	n 0 time_smaller	- time_smaller		- -				f 1110	1083	_null_));
DATA(insert ( 2140	n 0 timetz_smaller	- timetz_smaller	- -				f 1552	1266	_null_));
DATA(insert ( 2141	n 0 cashsmaller		- cashsmaller		- -				f 902		790		_null_));
DATA(insert ( 2142	n 0 timestamp_smaller	- timestamp_smaller	- -			f 2062	1114	_null_));
DATA(insert ( 2143	n 0 timestamptz_smaller - timestamptz_smaller - -		f 1322	1184	_null_));
DATA(insert ( 2144	n 0 interval_smaller	- interval_smaller	- -			f 1332	1186	_null_));
DATA(insert ( 2145	n 0 text_smaller	- text_smaller		- -				f 664		25		_null_));
DATA(insert ( 2146	n 0 numeric_smaller - numeric_smaller	- -				f 1754	1700	_null_));
DATA(insert ( 2051	n 0 array_smaller	- array_smaller		- -				f 1072	2277	_null_));
DATA(insert ( 2245	n 0 bpchar_smaller	- bpchar_smaller	- -				f 1058	1042	_null_));
DATA(insert ( 2798	n 0 tidsmaller		- tidsmaller		- -				f 2799	27		_null_));
DATA(insert ( 3527	n 0 enum_smaller	- enum_smaller		- -				f 3518	3500	_null_));
DATA(insert ( 3333	n 0 gpxloglocsmaller - gpxloglocsmaller		- -			f 3327	3310	_null_));

/* count */
DATA(insert ( 2147	n 0 int8inc_any		- int8pl	int8mi -				f 0		20		"0"));
DATA(insert ( 2803	n 0 int8inc			int8dec	 int8pl	int8mi -				f 0		20		"0"));

/* var_pop */
DATA(insert ( 2718	n 0 int8_accum		int8_decum	 numeric_amalg	numeric_demalg numeric_var_pop	f 0	1231	"{0,0,0}"));
DATA(insert ( 2719	n 0 int4_accum		int4_decum	 numeric_amalg	numeric_demalg numeric_var_pop	f 0	1231	"{0,0,0}"));
DATA(insert ( 2720	n 0 int2_accum		int2_decum	 numeric_amalg	numeric_demalg numeric_var_pop	f 0	1231	"{0,0,0}"));
DATA(insert ( 2721	n 0 float4_accum	float4_decum float8_amalg	float8_demalg float8_var_pop	f 0	1022	"{0,0,0}"));
DATA(insert ( 2722	n 0 float8_accum	float8_decum float8_amalg	float8_demalg float8_var_pop	f 0	1022	"{0,0,0}"));
DATA(insert ( 2723	n 0 numeric_accum	numeric_decum	 numeric_amalg	numeric_demalg numeric_var_pop	f 0	1231	"{0,0,0}"));

/* var_samp */
DATA(insert ( 2641	n 0 int8_accum		int8_decum	 numeric_amalg	numeric_demalg numeric_var_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2642	n 0 int4_accum		int4_decum	 numeric_amalg	numeric_demalg numeric_var_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2643	n 0 int2_accum		int2_decum	 numeric_amalg	numeric_demalg numeric_var_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2644	n 0 float4_accum	float4_decum float8_amalg	float8_demalg float8_var_samp	f 0	1022	"{0,0,0}"));
DATA(insert ( 2645	n 0 float8_accum	float8_decum float8_amalg	float8_demalg float8_var_samp	f 0	1022	"{0,0,0}"));
DATA(insert ( 2646	n 0 numeric_accum 	numeric_decum	 numeric_amalg	numeric_demalg numeric_var_samp	f 0	1231	"{0,0,0}"));

/* variance: historical Postgres syntax for var_samp */
DATA(insert ( 2148	n 0 int8_accum		int8_decum	 numeric_amalg	numeric_demalg numeric_var_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2149	n 0 int4_accum		int4_decum	 numeric_amalg	numeric_demalg numeric_var_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2150	n 0 int2_accum		int2_decum	 numeric_amalg	numeric_demalg numeric_var_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2151	n 0 float4_accum	float4_decum float8_amalg	float8_demalg float8_var_samp	f 0	1022	"{0,0,0}"));
DATA(insert ( 2152	n 0 float8_accum	float8_decum float8_amalg	float8_demalg float8_var_samp	f 0	1022	"{0,0,0}"));
DATA(insert ( 2153	n 0 numeric_accum	numeric_decum	 numeric_amalg	numeric_demalg numeric_var_samp	f 0	1231	"{0,0,0}"));

/* stddev_pop */
DATA(insert ( 2724	n 0 int8_accum		int8_decum	 numeric_amalg	numeric_demalg numeric_stddev_pop	f 0	1231	"{0,0,0}"));
DATA(insert ( 2725	n 0 int4_accum		int4_decum	 numeric_amalg	numeric_demalg numeric_stddev_pop	f 0	1231	"{0,0,0}"));
DATA(insert ( 2726	n 0 int2_accum		int2_decum	 numeric_amalg	numeric_demalg numeric_stddev_pop	f 0	1231	"{0,0,0}"));
DATA(insert ( 2727	n 0 float4_accum	float4_decum float8_amalg	float8_demalg float8_stddev_pop		f 0	1022	"{0,0,0}"));
DATA(insert ( 2728	n 0 float8_accum	float8_decum float8_amalg	float8_demalg float8_stddev_pop		f 0	1022	"{0,0,0}"));
DATA(insert ( 2729	n 0 numeric_accum	numeric_decum	 numeric_amalg	numeric_demalg numeric_stddev_pop	f 0	1231	"{0,0,0}"));

/* stddev_samp */
DATA(insert ( 2712	n 0 int8_accum		int8_decum	 numeric_amalg	numeric_demalg numeric_stddev_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2713	n 0 int4_accum		int4_decum	 numeric_amalg	numeric_demalg numeric_stddev_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2714	n 0 int2_accum		int2_decum	 numeric_amalg	numeric_demalg numeric_stddev_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2715	n 0 float4_accum	float4_decum float8_amalg	float8_demalg float8_stddev_samp	f 0	1022	"{0,0,0}"));
DATA(insert ( 2716	n 0 float8_accum	float8_decum float8_amalg	float8_demalg float8_stddev_samp	f 0	1022	"{0,0,0}"));
DATA(insert ( 2717	n 0 numeric_accum	numeric_decum	 numeric_amalg	numeric_demalg numeric_stddev_samp	f 0	1231	"{0,0,0}"));

/* stddev: historical Postgres syntax for stddev_samp */
DATA(insert ( 2154	n 0 int8_accum		int8_decum	 numeric_amalg	numeric_demalg numeric_stddev_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2155	n 0 int4_accum		int4_decum	 numeric_amalg	numeric_demalg numeric_stddev_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2156	n 0 int2_accum		int2_decum numeric_amalg	numeric_demalg numeric_stddev_samp	f 0	1231	"{0,0,0}"));
DATA(insert ( 2157	n 0 float4_accum	float4_decum float8_amalg	float8_demalg float8_stddev_samp	f 0	1022	"{0,0,0}"));
DATA(insert ( 2158	n 0 float8_accum	float8_decum float8_amalg	float8_demalg float8_stddev_samp	f 0	1022	"{0,0,0}"));
DATA(insert ( 2159	n 0 numeric_accum	numeric_decum numeric_amalg	numeric_demalg numeric_stddev_samp	f 0	1231	"{0,0,0}"));

/* SQL2003 binary regression aggregates */
DATA(insert ( 2818	n 0 int8inc_float8_float8	- int8pl					- -							f 0	20		"0"));
DATA(insert ( 2819	n 0 float8_regr_accum		- float8_regr_amalg	- float8_regr_sxx			f 0	1022	"{0,0,0,0,0,0}"));
DATA(insert ( 2820	n 0 float8_regr_accum		- float8_regr_amalg	- float8_regr_syy			f 0	1022	"{0,0,0,0,0,0}"));
DATA(insert ( 2821	n 0 float8_regr_accum		- float8_regr_amalg	- float8_regr_sxy			f 0	1022	"{0,0,0,0,0,0}"));
DATA(insert ( 2822	n 0 float8_regr_accum		- float8_regr_amalg	- float8_regr_avgx			f 0	1022	"{0,0,0,0,0,0}"));
DATA(insert ( 2823	n 0 float8_regr_accum		- float8_regr_amalg	- float8_regr_avgy			f 0	1022	"{0,0,0,0,0,0}"));
DATA(insert ( 2824	n 0 float8_regr_accum		- float8_regr_amalg	- float8_regr_r2			f 0	1022	"{0,0,0,0,0,0}"));
DATA(insert ( 2825	n 0 float8_regr_accum		- float8_regr_amalg	- float8_regr_slope			f 0	1022	"{0,0,0,0,0,0}"));
DATA(insert ( 2826	n 0 float8_regr_accum		- float8_regr_amalg	- float8_regr_intercept		f 0	1022	"{0,0,0,0,0,0}"));
DATA(insert ( 2827	n 0 float8_regr_accum		- float8_regr_amalg	- float8_covar_pop			f 0	1022	"{0,0,0,0,0,0}"));
DATA(insert ( 2828	n 0 float8_regr_accum		- float8_regr_amalg	- float8_covar_samp			f 0	1022	"{0,0,0,0,0,0}"));
DATA(insert ( 2829	n 0 float8_regr_accum		- float8_regr_amalg	- float8_corr				f 0	1022	"{0,0,0,0,0,0}"));

/* boolean-and and boolean-or */
DATA(insert ( 2517	n 0 booland_statefunc	- booland_statefunc	- -			f 0	16		_null_));
DATA(insert ( 2518	n 0 boolor_statefunc	- boolor_statefunc	- -			f 0	16		_null_));
DATA(insert ( 2519	n 0 booland_statefunc	- booland_statefunc	- -			f 0	16		_null_));

/* bitwise integer */
DATA(insert ( 2236 n 0 int2and		  - int2and		  - -					f 0	21		_null_));
DATA(insert ( 2237 n 0 int2or		  - int2or		  - -					f 0	21		_null_));
DATA(insert ( 2238 n 0 int4and		  - int4and		  - -					f 0	23		_null_));
DATA(insert ( 2239 n 0 int4or		  - int4or		  - -					f 0	23		_null_));
DATA(insert ( 2240 n 0 int8and		  - int8and		  - -					f 0	20		_null_));
DATA(insert ( 2241 n 0 int8or		  - int8or		  - -					f 0	20		_null_));
DATA(insert ( 2242 n 0 bitand		  - bitand		  - -					f 0	1560	_null_));
DATA(insert ( 2243 n 0 bitor		  - bitor		  - -					f 0	1560	_null_));

/* MPP Aggregate -- array_sum -- special for prospective customer. */
DATA(insert ( 6013	n 0 array_add     - array_add	  - -					f 0	1007	"{}"));

/* sum(array[]) */
DATA(insert ( 3216  n 0 int2_matrix_accum     - int8_matrix_accum     - -   f 0   1016    _null_));
DATA(insert ( 3217  n 0 int4_matrix_accum     - int8_matrix_accum     - -   f 0   1016    _null_));
DATA(insert ( 3218  n 0 int8_matrix_accum     - int8_matrix_accum     - -   f 0   1016    _null_));
DATA(insert ( 3219  n 0 float8_matrix_accum   - float8_matrix_accum   - -   f 0   1022    _null_));

/* pivot_sum(...) */
DATA(insert ( 3226  n 0 int4_pivot_accum      - int8_matrix_accum     - -   f 0   1007    _null_));
DATA(insert ( 3228  n 0 int8_pivot_accum      - int8_matrix_accum     - -   f 0   1016    _null_));
DATA(insert ( 3230  n 0 float8_pivot_accum    - float8_matrix_accum   - -   f 0   1022    _null_));

/* xml */
DATA(insert ( 2901  n 0 xmlconcat2	             - - - - 				  f 0	142  _null_));

/* array */
DATA(insert ( 2335	n 0 array_agg_transfn        - - - array_agg_finalfn  t 0 2281 _null_));

/* text */
DATA(insert ( 3537	n 0 string_agg_transfn       - - - string_agg_finalfn f 0 2281 _null_));
DATA(insert ( 3538	n 0 string_agg_delim_transfn - - - string_agg_finalfn f 0 2281 _null_));

/* ordered-set and hypothetical-set aggregates */
DATA(insert ( 3972	o 1 ordered_set_transition	- - - 		percentile_disc_final					t 0	2281	_null_ ));
DATA(insert ( 3974	o 1 ordered_set_transition	- - -		percentile_cont_float8_final			f 0	2281	_null_ ));
DATA(insert ( 3976	o 1 ordered_set_transition	- - -		percentile_cont_interval_final			f 0	2281	_null_ ));
DATA(insert ( 3978	o 1 ordered_set_transition	- - -		percentile_disc_multi_final				t 0	2281	_null_ ));
DATA(insert ( 3980	o 1 ordered_set_transition	- - -		percentile_cont_float8_multi_final		f 0	2281	_null_ ));
DATA(insert ( 3982	o 1 ordered_set_transition	- - -		percentile_cont_interval_multi_final	f 0	2281	_null_ ));
DATA(insert ( 3984	o 0 ordered_set_transition	- - -		mode_final								t 0	2281	_null_ ));
DATA(insert ( 3986	h 1 ordered_set_transition_multi - - -	rank_final								t 0	2281	_null_ ));
DATA(insert ( 3988	h 1 ordered_set_transition_multi - - -	percent_rank_final						t 0	2281	_null_ ));
DATA(insert ( 3990	h 1 ordered_set_transition_multi - - -	cume_dist_final							t 0	2281	_null_ ));
DATA(insert ( 3992	h 1 ordered_set_transition_multi - - -	dense_rank_final						t 0	2281	_null_ ));

/* GPDB: additional variants of percentile_cont, for timestamps */
DATA(insert ( 6119	o 1 ordered_set_transition	- - -		percentile_cont_timestamp_final			f 0	2281	_null_ ));
DATA(insert ( 6121	o 1 ordered_set_transition	- - -		percentile_cont_timestamp_multi_final	f 0	2281	_null_ ));
DATA(insert ( 6123	o 1 ordered_set_transition	- - -		percentile_cont_timestamptz_final		f 0	2281	_null_ ));
DATA(insert ( 6125	o 1 ordered_set_transition	- - -		percentile_cont_timestamptz_multi_final	f 0	2281	_null_ ));

/* median */
DATA(insert ( 6127	o 1 ordered_set_transition	- - -		percentile_cont_float8_final			f 0	2281	_null_ ));
DATA(insert ( 6128	o 1 ordered_set_transition	- - -		percentile_cont_interval_final			f 0	2281	_null_ ));
DATA(insert ( 6129	o 1 ordered_set_transition	- - -		percentile_cont_timestamp_final			f 0	2281	_null_ ));
DATA(insert ( 6130	o 1 ordered_set_transition	- - -		percentile_cont_timestamptz_final		f 0	2281	_null_ ));

/*
 * prototypes for functions in pg_aggregate.c
 */
extern void AggregateCreate(const char *aggName,
				Oid aggNamespace,
				char aggKind,
				int numArgs,
				int numDirectArgs,
				oidvector *parameterTypes,
				Datum allParameterTypes,
				Datum parameterModes,
				Datum parameterNames,
				List *parameterDefaults,
				Oid variadicArgType,
				List *aggtransfnName,
				List *aggprelimfnName,
				List *aggfinalfnName,
				bool finalfnExtraArgs,
				List *aggsortopName,
				Oid aggTransType,
				const char *agginitval);

#endif   /* PG_AGGREGATE_H */
