/*-------------------------------------------------------------------------
 *
 * pg_window.h
 *	  definition of the system "window" relation (pg_window)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 2007-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_window.h
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PG_WINDOW_H
#define _PG_WINDOW_H

#include "nodes/pg_list.h"

#include "catalog/genbki.h"

/* ----------------------------------------------------------------
 *		pg_window definition.
 * ----------------------------------------------------------------
 */

/* ----------------
 *		pg_window definition.  cpp turns this into
 *		typedef struct FormData_pg_window
 * ----------------
 */
#define WindowRelationId	5004

CATALOG(pg_window,5004) BKI_WITHOUT_OIDS
{
	regproc	winfnoid;			/* pg_proc OID of the window function itself */
	bool	winrequireorder;	/* does wf require order by? */
	bool	winallowframe;		/* does wf allow framing? */
	bool	winpeercount;		/* does wf definition need peer row count? */
	bool	wincount;			/* does wf definition need partition row count? */
	regproc	winfunc;			/* immediate function (0 if none) */
	regproc	winprefunc;			/* preliminary function (0 if none) */
	Oid		winpretype;			/* type of preliminary function result */
	regproc	winfinfunc;			/* final function (0 if none) */
	char	winkind;			
	regproc	winframemakerfunc;	/* pg_proc OID of frame maker function */
} FormData_pg_window;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(winfnoid REFERENCES pg_proc(oid));
FOREIGN_KEY(winfunc REFERENCES pg_proc(oid));
FOREIGN_KEY(winprefunc REFERENCES pg_proc(oid));
FOREIGN_KEY(winfinfunc REFERENCES pg_proc(oid));

/* ----------------
 *		Form_pg_window corresponds to a pointer to a tuple with
 *		the format of pg_window relation.
 * ----------------
 */
typedef FormData_pg_window *Form_pg_window;


/* ----------------
 *		compiler constants for pg_window
 * ----------------
 */
#define Natts_pg_window						11
#define Anum_pg_window_winfnoid				1
#define Anum_pg_window_winrequireorder		2
#define Anum_pg_window_winallowframe		3
#define Anum_pg_window_winpeercount			4
#define Anum_pg_window_wincount				5
#define Anum_pg_window_winfunc				6
#define Anum_pg_window_winprefunc			7
#define Anum_pg_window_winpretype			8
#define Anum_pg_window_winfinfunc			9
#define Anum_pg_window_winkind				10
#define Anum_pg_window_winframemakerfunc	11

/* ----------------
 * Constants for winkind attribute
 * ---------------
 */
#define WINKIND_WINDOW		'w'		/* ordinary window function, except for NTILE */
#define WINKIND_NTILE       'n'     /* NTILE function */
#define WINKIND_FIRST_VALUE	'f'		/* Oracle FIRST_VALUE function */
#define WINKIND_LAST_VALUE	'l'		/* Oracle LAST_VALUE function */
#define WINKIND_LEAD		'd'		/* Oracle LEAD function */
#define WINKIND_LAG			'g'		/* Oracle LAG function */

/* This constant for use in code, never in catalog */
#define WINKIND_AGGREGATE	'a'		/* aggregate-based window function */

/* Convenience macros */
#define IS_FIRST_VALUE(winkind) ((winkind)==WINKIND_FIRST_VALUE)
#define IS_LAST_VALUE(winkind) ((winkind)==WINKIND_LAST_VALUE)
#define IS_FIRST_LAST(winkind) (IS_FIRST_VALUE(winkind) || IS_LAST_VALUE(winkind))
#define IS_LEAD(winkind) ((winkind)==WINKIND_LEAD)
#define IS_LAG(winkind) ((winkind)==WINKIND_LAG)
#define IS_LEAD_LAG(winkind) (IS_LEAD(winkind) || IS_LAG(winkind)) 
#define IS_NTILE(winkind) ((winkind)==WINKIND_NTILE)


/* ----------------
 * initial contents of pg_window
 * ---------------
 */
/*							winfunc		winprefunc	winpretype	winfinfunc */

/* row_number */
DATA(insert ( 7000	f f f f	7100		0			0			0		w	0));

/* rank */
DATA(insert ( 7001	t f f f	7101		0			0			0		w	0));

/* dense_rank */
DATA(insert ( 7002	t f f f	7102		0			0			0		w	0));

/* percent_rank */
DATA(insert ( 7003	t f f t	0			7101		20			7303	w	0));

/* cume_dist */
DATA(insert ( 7004	t f t t	0			7204		20			7304	w	0));

/* ntile(int) */
DATA(insert ( 7005	t f f t	0			7205		1016		7305	n	0));

/* ntile(bigint) */
DATA(insert ( 7006	t f f t	0			7206		1016		7305	n	0));

/* ntile(numeric) */
DATA(insert ( 7007	t f f t	0			7207		1016		7305	n	0));

/* ratio_to_report */
/* XXX omitted for now; no way to represent auxiliary aggregate SUM(x). */

/* first_value */
DATA(insert ( 7017  t t f f 7111        0       0       0	f	0 ));
DATA(insert ( 7018  t t f f 7112		0		0		0	f	0 ));
DATA(insert ( 7019  t t f f 7113		0		0		0	f	0 ));
DATA(insert ( 7020  t t f f 7114		0		0		0	f	0 ));
DATA(insert ( 7021  t t f f 7115		0		0		0	f	0 ));
DATA(insert ( 7022  t t f f 7116		0		0		0	f	0 ));
DATA(insert ( 7023  t t f f 7117		0		0		0	f	0 ));
DATA(insert ( 7024  t t f f 7118		0		0		0	f	0 ));
DATA(insert ( 7025  t t f f 7119		0		0		0	f	0 ));
DATA(insert ( 7026  t t f f 7120		0		0		0	f	0 ));
DATA(insert ( 7027  t t f f 7121		0		0		0	f	0 ));
DATA(insert ( 7028  t t f f 7122		0		0		0	f	0 ));
DATA(insert ( 7029  t t f f 7123		0		0		0	f	0 ));
DATA(insert ( 7030  t t f f 7124		0		0		0	f	0 ));
DATA(insert ( 7031  t t f f 7125		0		0		0	f	0 ));
DATA(insert ( 7032  t t f f 7126		0		0		0	f	0 ));
DATA(insert ( 7033  t t f f 7127		0		0		0	f	0 ));
DATA(insert ( 7034  t t f f 7128		0		0		0	f	0 ));
DATA(insert ( 7035  t t f f 7129		0		0		0	f	0 ));
DATA(insert ( 7036  t t f f 7130		0		0		0	f	0 ));
DATA(insert ( 7037  t t f f 7131		0		0		0	f	0 ));
DATA(insert ( 7038  t t f f 7132		0		0		0	f	0 ));
DATA(insert ( 7039  t t f f 7133		0		0		0	f	0 ));
DATA(insert ( 7040  t t f f 7134		0		0		0	f	0 ));
DATA(insert ( 7041  t t f f 7135		0		0		0	f	0 ));
DATA(insert ( 7042  t t f f 7136		0		0		0	f	0 ));
DATA(insert ( 7043  t t f f 7137		0		0		0	f	0 ));
DATA(insert ( 7044  t t f f 7138		0		0		0	f	0 ));
DATA(insert ( 7045  t t f f 7139		0		0		0	f	0 ));
DATA(insert ( 7046  t t f f 7140		0		0		0	f	0 ));
DATA(insert ( 7047  t t f f 7141		0		0		0	f	0 ));
DATA(insert ( 7232  t t f f 7233        0       0		0   f	0 ));
DATA(insert ( 7256  t t f f 7257		0		0		0	f	0 ));
DATA(insert ( 7272  t t f f 7273		0		0		0	f	0 ));
DATA(insert ( 7288  t t f f 7289		0		0		0	f	0 ));


/* last_value */
DATA(insert ( 7012	t t f f	7103		0		0		0	l	0 ));
DATA(insert ( 7013	t t f f	7107		0		0		0	l	0 ));
DATA(insert ( 7014	t t f f	7108		0		0		0	l	0 ));
DATA(insert ( 7015	t t f f	7109		0		0		0	l	0 ));
DATA(insert ( 7016	t t f f	7110		0		0		0	l	0 ));
DATA(insert ( 7063  t t f f 7165		0		0		0	l	0 ));
DATA(insert ( 7072  t t f f 7166		0		0		0	l	0 ));
DATA(insert ( 7073  t t f f 7167		0		0		0	l	0 ));
DATA(insert ( 7048  t t f f 7168		0		0		0	l	0 ));
DATA(insert ( 7049  t t f f 7142		0		0		0	l	0 ));
DATA(insert ( 7050  t t f f 7143		0		0		0	l	0 ));
DATA(insert ( 7051  t t f f 7144		0		0		0	l	0 ));
DATA(insert ( 7052  t t f f 7145		0		0		0	l	0 ));
DATA(insert ( 7053  t t f f 7146		0		0		0	l	0 ));
DATA(insert ( 7054  t t f f 7147		0		0		0	l	0 ));
DATA(insert ( 7055  t t f f 7148		0		0		0	l	0 ));
DATA(insert ( 7056  t t f f 7149		0		0		0	l	0 ));
DATA(insert ( 7057  t t f f 7150		0		0		0	l	0 ));
DATA(insert ( 7058  t t f f 7151		0		0		0	l	0 ));
DATA(insert ( 7059  t t f f 7152		0		0		0	l	0 ));
DATA(insert ( 7060  t t f f 7153		0		0		0	l	0 ));
DATA(insert ( 7061  t t f f 7154		0		0		0	l	0 ));
DATA(insert ( 7062  t t f f 7155		0		0		0	l	0 ));
DATA(insert ( 7064  t t f f 7157		0		0		0	l	0 ));
DATA(insert ( 7065  t t f f 7158		0		0		0	l	0 ));
DATA(insert ( 7066  t t f f 7159		0		0		0	l	0 ));
DATA(insert ( 7067  t t f f 7160		0		0		0	l	0 ));
DATA(insert ( 7068  t t f f 7161		0		0		0	l	0 ));
DATA(insert ( 7069  t t f f 7162		0		0		0	l	0 ));
DATA(insert ( 7070  t t f f 7163		0		0		0	l	0 ));
DATA(insert ( 7071  t t f f 7164		0		0		0	l	0 ));
DATA(insert ( 7238  t t f f 7239        0       0		0   l	0 ));
DATA(insert ( 7258  t t f f 7259		0		0		0	l	0 ));
DATA(insert ( 7274  t t f f 7275		0		0		0	l	0 ));
DATA(insert ( 7290  t t f f 7291		0		0		0	l	0 ));

/* lag */
DATA(insert ( 7675  t f f f 7490		0		0		0	g	5081 ));
DATA(insert ( 7491  t f f f 7492		0		0		0	g	5081 ));
DATA(insert ( 7493  t f f f 7494		0		0		0	g	5081 ));
DATA(insert ( 7495  t f f f 7496		0		0		0	g	5081 ));
DATA(insert ( 7497  t f f f 7498		0		0		0	g	5081 ));
DATA(insert ( 7499  t f f f 7500		0		0		0	g	5081 ));
DATA(insert ( 7501  t f f f 7502		0		0		0	g	5081 ));
DATA(insert ( 7503  t f f f 7504		0		0		0	g	5081 ));
DATA(insert ( 7505  t f f f 7506		0		0		0	g	5081 ));
DATA(insert ( 7507  t f f f 7508		0		0		0	g	5081 ));
DATA(insert ( 7509  t f f f 7510		0		0		0	g	5081 ));
DATA(insert ( 7511  t f f f 7512		0		0		0	g	5081 ));
DATA(insert ( 7513  t f f f 7514		0		0		0	g	5081 ));
DATA(insert ( 7515  t f f f 7516		0		0		0	g	5081 ));
DATA(insert ( 7517  t f f f 7518		0		0		0	g	5081 ));
DATA(insert ( 7519  t f f f 7520		0		0		0	g	5081 ));
DATA(insert ( 7521  t f f f 7522		0		0		0	g	5081 ));
DATA(insert ( 7523  t f f f 7524		0		0		0	g	5081 ));
DATA(insert ( 7525  t f f f 7526		0		0		0	g	5081 ));
DATA(insert ( 7527  t f f f 7528		0		0		0	g	5081 ));
DATA(insert ( 7529  t f f f 7530		0		0		0	g	5081 ));
DATA(insert ( 7531  t f f f 7532		0		0		0	g	5081 ));
DATA(insert ( 7533  t f f f 7534		0		0		0	g	5081 ));
DATA(insert ( 7535  t f f f 7536		0		0		0	g	5081 ));
DATA(insert ( 7537  t f f f 7538		0		0		0	g	5081 ));
DATA(insert ( 7539  t f f f 7540		0		0		0	g	5081 ));
DATA(insert ( 7541  t f f f 7542		0		0		0	g	5081 ));
DATA(insert ( 7543  t f f f 7544		0		0		0	g	5081 ));
DATA(insert ( 7545  t f f f 7546		0		0		0	g	5081 ));
DATA(insert ( 7547  t f f f 7548		0		0		0	g	5081 ));
DATA(insert ( 7549  t f f f 7550		0		0		0	g	5081 ));
DATA(insert ( 7551  t f f f 7552		0		0		0	g	5081 ));
DATA(insert ( 7553  t f f f 7554		0		0		0	g	5081 ));
DATA(insert ( 7555  t f f f 7556		0		0		0	g	5081 ));
DATA(insert ( 7557  t f f f 7558		0		0		0	g	5081 ));
DATA(insert ( 7559  t f f f 7560		0		0		0	g	5081 ));
DATA(insert ( 7561  t f f f 7562		0		0		0	g	5081 ));
DATA(insert ( 7563  t f f f 7564		0		0		0	g	5081 ));
DATA(insert ( 7565  t f f f 7566		0		0		0	g	5081 ));
DATA(insert ( 7567  t f f f 7568		0		0		0	g	5081 ));
DATA(insert ( 7569  t f f f 7570		0		0		0	g	5081 ));
DATA(insert ( 7571  t f f f 7572		0		0		0	g	5081 ));
DATA(insert ( 7573  t f f f 7574		0		0		0	g	5081 ));
DATA(insert ( 7575  t f f f 7576		0		0		0	g	5081 ));
DATA(insert ( 7577  t f f f 7578		0		0		0	g	5081 ));
DATA(insert ( 7579  t f f f 7580		0		0		0	g	5081 ));
DATA(insert ( 7581  t f f f 7582		0		0		0	g	5081 ));
DATA(insert ( 7583  t f f f 7584		0		0		0	g	5081 ));
DATA(insert ( 7585  t f f f 7586		0		0		0	g	5081 ));
DATA(insert ( 7587  t f f f 7588		0		0		0	g	5081 ));
DATA(insert ( 7589  t f f f 7590		0		0		0	g	5081 ));
DATA(insert ( 7591  t f f f 7592		0		0		0	g	5081 ));
DATA(insert ( 7593  t f f f 7594		0		0		0	g	5081 ));
DATA(insert ( 7595  t f f f 7596		0		0		0	g	5081 ));
DATA(insert ( 7597  t f f f 7598		0		0		0	g	5081 ));
DATA(insert ( 7599  t f f f 7600		0		0		0	g	5081 ));
DATA(insert ( 7601  t f f f 7602		0		0		0	g	5081 ));
DATA(insert ( 7603  t f f f 7604		0		0		0	g	5081 ));
DATA(insert ( 7605  t f f f 7606		0		0		0	g	5081 ));
DATA(insert ( 7607  t f f f 7608		0		0		0	g	5081 ));
DATA(insert ( 7609  t f f f 7610		0		0		0	g	5081 ));
DATA(insert ( 7611  t f f f 7612		0		0		0	g	5081 ));
DATA(insert ( 7613  t f f f 7614		0		0		0	g	5081 ));
DATA(insert ( 7615  t f f f 7616		0		0		0	g	5081 ));
DATA(insert ( 7617  t f f f 7618		0		0		0	g	5081 ));
DATA(insert ( 7619  t f f f 7620		0		0		0	g	5081 ));
DATA(insert ( 7621  t f f f 7622		0		0		0	g	5081 ));
DATA(insert ( 7623  t f f f 7624		0		0		0	g	5081 ));
DATA(insert ( 7625  t f f f 7626		0		0		0	g	5081 ));
DATA(insert ( 7627  t f f f 7628		0		0		0	g	5081 ));
DATA(insert ( 7629  t f f f 7630		0		0		0	g	5081 ));
DATA(insert ( 7631  t f f f 7632		0		0		0	g	5081 ));
DATA(insert ( 7633  t f f f 7634		0		0		0	g	5081 ));
DATA(insert ( 7635  t f f f 7636		0		0		0	g	5081 ));
DATA(insert ( 7637  t f f f 7638		0		0		0	g	5081 ));
DATA(insert ( 7639  t f f f 7640		0		0		0	g	5081 ));
DATA(insert ( 7641  t f f f 7642		0		0		0	g	5081 ));
DATA(insert ( 7643  t f f f 7644		0		0		0	g	5081 ));
DATA(insert ( 7645  t f f f 7646		0		0		0	g	5081 ));
DATA(insert ( 7647  t f f f 7648		0		0		0	g	5081 ));
DATA(insert ( 7649  t f f f 7650		0		0		0	g	5081 ));
DATA(insert ( 7651  t f f f 7652		0		0		0	g	5081 ));
DATA(insert ( 7653  t f f f 7654		0		0		0	g	5081 ));
DATA(insert ( 7655  t f f f 7656		0		0		0	g	5081 ));
DATA(insert ( 7657  t f f f 7658		0		0		0	g	5081 ));
DATA(insert ( 7659  t f f f 7660		0		0		0	g	5081 ));
DATA(insert ( 7661  t f f f 7662		0		0		0	g	5081 ));
DATA(insert ( 7663  t f f f 7664		0		0		0	g	5081 ));
DATA(insert ( 7665  t f f f 7666		0		0		0	g	5081 ));
DATA(insert ( 7667  t f f f 7668		0		0		0	g	5081 ));
DATA(insert ( 7669  t f f f 7670		0		0		0	g	5081 ));
DATA(insert ( 7671  t f f f 7672		0		0		0	g	5081 ));
DATA(insert ( 7673  t f f f 7674		0		0		0	g	5081 ));
DATA(insert ( 7211  t f f f 7208		0		0		0	g	5081 ));
DATA(insert ( 7212  t f f f 7209		0		0		0	g	5081 ));
DATA(insert ( 7213  t f f f 7210		0		0		0	g	5081 ));
DATA(insert ( 7226  t f f f 7227        0       0       0   g	5081 ));
DATA(insert ( 7228  t f f f 7229        0       0       0   g	5081 ));
DATA(insert ( 7230  t f f f 7231        0       0       0   g	5081 ));
DATA(insert ( 7250  t f f f 7251		0		0		0	g	5081 ));
DATA(insert ( 7252  t f f f 7253		0		0		0	g	5081 ));
DATA(insert ( 7254  t f f f 7255		0		0		0	g	5081 ));
DATA(insert ( 7266  t f f f 7267		0		0		0	g	5081 ));
DATA(insert ( 7268  t f f f 7269		0		0		0	g	5081 ));
DATA(insert ( 7270  t f f f 7271		0		0		0	g	5081 ));

/* lead */
DATA(insert ( 7011	t f f f	7106		0		0		0	d	5081 ));
DATA(insert ( 7074	t f f f	7105		0		0		0	d	5081 ));
DATA(insert ( 7075	t f f f	7104		0		0		0	d	5081 ));
DATA(insert ( 7310  t f f f 7311		0		0		0	d	5081 ));
DATA(insert ( 7312  t f f f 7313		0		0		0	d	5081 ));
DATA(insert ( 7314  t f f f 7315		0		0		0	d	5081 ));
DATA(insert ( 7316  t f f f 7317		0		0		0	d	5081 ));
DATA(insert ( 7318  t f f f 7319		0		0		0	d	5081 ));
DATA(insert ( 7320  t f f f 7321		0		0		0	d	5081 ));
DATA(insert ( 7322  t f f f 7323		0		0		0	d	5081 ));
DATA(insert ( 7324  t f f f 7325		0		0		0	d	5081 ));
DATA(insert ( 7326  t f f f 7327		0		0		0	d	5081 ));
DATA(insert ( 7328  t f f f 7329		0		0		0	d	5081 ));
DATA(insert ( 7330  t f f f 7331		0		0		0	d	5081 ));
DATA(insert ( 7332  t f f f 7333		0		0		0	d	5081 ));
DATA(insert ( 7334  t f f f 7335		0		0		0	d	5081 ));
DATA(insert ( 7336  t f f f 7337		0		0		0	d	5081 ));
DATA(insert ( 7338  t f f f 7339		0		0		0	d	5081 ));
DATA(insert ( 7340  t f f f 7341		0		0		0	d	5081 ));
DATA(insert ( 7342  t f f f 7343		0		0		0	d	5081 ));
DATA(insert ( 7344  t f f f 7345		0		0		0	d	5081 ));
DATA(insert ( 7346  t f f f 7347		0		0		0	d	5081 ));
DATA(insert ( 7348  t f f f 7349		0		0		0	d	5081 ));
DATA(insert ( 7350  t f f f 7351		0		0		0	d	5081 ));
DATA(insert ( 7352  t f f f 7353		0		0		0	d	5081 ));
DATA(insert ( 7354  t f f f 7355		0		0		0	d	5081 ));
DATA(insert ( 7356  t f f f 7357		0		0		0	d	5081 ));
DATA(insert ( 7358  t f f f 7359		0		0		0	d	5081 ));
DATA(insert ( 7360  t f f f 7361		0		0		0	d	5081 ));
DATA(insert ( 7362  t f f f 7363		0		0		0	d	5081 ));
DATA(insert ( 7364  t f f f 7365		0		0		0	d	5081 ));
DATA(insert ( 7366  t f f f 7367		0		0		0	d	5081 ));
DATA(insert ( 7368  t f f f 7369		0		0		0	d	5081 ));
DATA(insert ( 7370  t f f f 7371		0		0		0	d	5081 ));
DATA(insert ( 7372  t f f f 7373		0		0		0	d	5081 ));
DATA(insert ( 7374  t f f f 7375		0		0		0	d	5081 ));
DATA(insert ( 7376  t f f f 7377		0		0		0	d	5081 ));
DATA(insert ( 7378  t f f f 7379		0		0		0	d	5081 ));
DATA(insert ( 7380  t f f f 7381		0		0		0	d	5081 ));
DATA(insert ( 7382  t f f f 7383		0		0		0	d	5081 ));
DATA(insert ( 7384  t f f f 7385		0		0		0	d	5081 ));
DATA(insert ( 7386  t f f f 7387		0		0		0	d	5081 ));
DATA(insert ( 7388  t f f f 7389		0		0		0	d	5081 ));
DATA(insert ( 7390  t f f f 7391		0		0		0	d	5081 ));
DATA(insert ( 7392  t f f f 7393		0		0		0	d	5081 ));
DATA(insert ( 7394  t f f f 7395		0		0		0	d	5081 ));
DATA(insert ( 7396  t f f f 7397		0		0		0	d	5081 ));
DATA(insert ( 7398  t f f f 7399		0		0		0	d	5081 ));
DATA(insert ( 7400  t f f f 7401		0		0		0	d	5081 ));
DATA(insert ( 7402  t f f f 7403		0		0		0	d	5081 ));
DATA(insert ( 7404  t f f f 7405		0		0		0	d	5081 ));
DATA(insert ( 7406  t f f f 7407		0		0		0	d	5081 ));
DATA(insert ( 7408  t f f f 7409		0		0		0	d	5081 ));
DATA(insert ( 7410  t f f f 7411		0		0		0	d	5081 ));
DATA(insert ( 7412  t f f f 7413		0		0		0	d	5081 ));
DATA(insert ( 7414  t f f f 7415		0		0		0	d	5081 ));
DATA(insert ( 7416  t f f f 7417		0		0		0	d	5081 ));
DATA(insert ( 7418  t f f f 7419		0		0		0	d	5081 ));
DATA(insert ( 7420  t f f f 7421		0		0		0	d	5081 ));
DATA(insert ( 7422  t f f f 7423		0		0		0	d	5081 ));
DATA(insert ( 7424  t f f f 7425		0		0		0	d	5081 ));
DATA(insert ( 7426  t f f f 7427		0		0		0	d	5081 ));
DATA(insert ( 7428  t f f f 7429		0		0		0	d	5081 ));
DATA(insert ( 7430  t f f f 7431		0		0		0	d	5081 ));
DATA(insert ( 7432  t f f f 7433		0		0		0	d	5081 ));
DATA(insert ( 7434  t f f f 7435		0		0		0	d	5081 ));
DATA(insert ( 7436  t f f f 7437		0		0		0	d	5081 ));
DATA(insert ( 7438  t f f f 7439		0		0		0	d	5081 ));
DATA(insert ( 7440  t f f f 7441		0		0		0	d	5081 ));
DATA(insert ( 7442  t f f f 7443		0		0		0	d	5081 ));
DATA(insert ( 7444  t f f f 7445		0		0		0	d	5081 ));
DATA(insert ( 7446  t f f f 7447		0		0		0	d	5081 ));
DATA(insert ( 7448  t f f f 7449		0		0		0	d	5081 ));
DATA(insert ( 7450  t f f f 7451		0		0		0	d	5081 ));
DATA(insert ( 7452  t f f f 7453		0		0		0	d	5081 ));
DATA(insert ( 7454  t f f f 7455		0		0		0	d	5081 ));
DATA(insert ( 7456  t f f f 7457		0		0		0	d	5081 ));
DATA(insert ( 7458  t f f f 7459		0		0		0	d	5081 ));
DATA(insert ( 7460  t f f f 7461		0		0		0	d	5081 ));
DATA(insert ( 7462  t f f f 7463		0		0		0	d	5081 ));
DATA(insert ( 7464  t f f f 7465		0		0		0	d	5081 ));
DATA(insert ( 7466  t f f f 7467		0		0		0	d	5081 ));
DATA(insert ( 7468  t f f f 7469		0		0		0	d	5081 ));
DATA(insert ( 7470  t f f f 7471		0		0		0	d	5081 ));
DATA(insert ( 7472  t f f f 7473		0		0		0	d	5081 ));
DATA(insert ( 7474  t f f f 7475		0		0		0	d	5081 ));
DATA(insert ( 7476  t f f f 7477		0		0		0	d	5081 ));
DATA(insert ( 7478  t f f f 7479		0		0		0	d	5081 ));
DATA(insert ( 7480  t f f f 7481		0		0		0	d	5081 ));
DATA(insert ( 7482  t f f f 7483		0		0		0	d	5081 ));
DATA(insert ( 7484  t f f f 7485		0		0		0	d	5081 ));
DATA(insert ( 7486  t f f f 7487		0		0		0	d	5081 ));
DATA(insert ( 7488  t f f f 7489		0		0		0	d	5081 ));
DATA(insert ( 7214  t f f f 7217		0		0		0	d	5081 ));
DATA(insert ( 7215  t f f f 7218		0		0		0	d	5081 ));
DATA(insert ( 7216  t f f f 7219		0		0		0	d	5081 ));
DATA(insert ( 7220  t f f f 7221		0		0		0	d	5081 ));
DATA(insert ( 7222  t f f f 7223		0		0		0	d	5081 ));
DATA(insert ( 7224  t f f f 7225		0		0		0	d	5081 ));
DATA(insert ( 7244  t f f f 7245		0		0		0	d	5081 ));
DATA(insert ( 7246  t f f f 7247		0		0		0	d	5081 ));
DATA(insert ( 7248  t f f f 7249		0		0		0	d	5081 ));
DATA(insert ( 7260  t f f f 7261		0		0		0	d	5081 ));
DATA(insert ( 7262  t f f f 7263		0		0		0	d	5081 ));
DATA(insert ( 7264  t f f f 7265		0		0		0	d	5081 ));

#endif   /* _PG_WINDOW_H */
