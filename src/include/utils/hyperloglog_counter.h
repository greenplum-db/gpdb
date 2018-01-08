#ifndef HYPERLOGLOG_COUNTER_H
#define HYPERLOGLOG_COUNTER_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/lsyscache.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"

#include "utils/hyperloglog.h"
#include "utils/encoding.h"

/* shoot for 2^64 distinct items and 0.8125% error rate by default */
#define DEFAULT_NDISTINCT   1ULL << 63 
#define DEFAULT_ERROR       0.008125


/* ------------- function declarations for local functions --------------- */
extern HLLCounter hyperloglog_add_item(HLLCounter hllcounter, Datum element, int16 typlen, bool typbyval, char typalign);

extern double hyperloglog_get_estimate(HLLCounter hyperloglog);
extern HLLCounter hyperloglog_merge(HLLCounter counter1, HLLCounter counter2);
 
extern HLLCounter hyperloglog_init_default(void);
extern int hyperloglog_size_default(void);
extern int hyperloglog_length(HLLCounter hyperloglog);
extern void hyperloglog_reset(HLLCounter hyperloglog);

extern HLLCounter hyperloglog_comp(HLLCounter hyperloglog);
extern HLLCounter hyperloglog_decomp(HLLCounter hyperloglog);

#endif
