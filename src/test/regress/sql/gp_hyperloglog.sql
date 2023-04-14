-- This file contains test cases from gp_hyperloglog.

-- 1. Test estimating the cardinality of an given stream.
SELECT gp_hyperloglog_get_estimate(gp_hyperloglog_accum(i))
  FROM generate_series(1, 10000)i;

-- 2. Test merging two hloglog counter.
-- a) A ∩ B = {}, A ∪ B = {1, 2, ..., 20}
SELECT gp_hyperloglog_get_estimate(
    gp_hyperloglog_merge(gp_hyperloglog_accum(x), gp_hyperloglog_accum(y)))
  FROM generate_series(1, 10)x, generate_series(11, 20)y;

-- b) A ∩ B = {5, 6, 7, 8, 9, 10}, A ∪ B = {1, 2, ..., 20}
SELECT gp_hyperloglog_get_estimate(
    gp_hyperloglog_merge(gp_hyperloglog_accum(x), gp_hyperloglog_accum(y)))
  FROM generate_series(1, 10)x, generate_series(5, 20)y;

-- c) A ∩ B = A, A ∪ B = {1, 2, ..., 20}
SELECT gp_hyperloglog_get_estimate(
    gp_hyperloglog_merge(gp_hyperloglog_accum(x), gp_hyperloglog_accum(y)))
  FROM generate_series(1, 10)x, generate_series(1, 20)y;

-- 3. Test the gp_hyperloglog_add_item_agg_default() UDF.
-- a) The newly added item is out of the range of the original set.
SELECT gp_hyperloglog_get_estimate(
    gp_hyperloglog_add_item_agg_default(gp_hyperloglog_accum(i), 101))
  FROM generate_series(1, 100)i;

-- b) The newly added item is within the range of the original set.
SELECT gp_hyperloglog_get_estimate(
    gp_hyperloglog_add_item_agg_default(gp_hyperloglog_accum(i), 50))
  FROM generate_series(1, 100)i;

-- c) When the first argument of gp_hyperloglog_add_item_agg_default() is null,
-- it will create a new hloglog counter for us. The following test will create
-- 5 hloglog counters.
SELECT gp_hyperloglog_get_estimate(
    gp_hyperloglog_add_item_agg_default(null, 50))
  FROM generate_series(1, 5)i;

-- d) When the second argument of gp_hyperloglog_add_item_agg_default() is null,
-- hloglog counter will skip that value.
SELECT gp_hyperloglog_get_estimate(
    gp_hyperloglog_add_item_agg_default(gp_hyperloglog_accum(i), null::int))
  FROM generate_series(1, 100)i;

-- 3. Test printing out hloglog counter in base64.
SELECT gp_hyperloglog_accum(i) FROM generate_series(1, 100)i;

-- 4. Test convert a base64 string to hloglog counter.
-- a) Test convert a valid hloglog counter.
SELECT gp_hyperloglog_get_estimate('8gYCAP////8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAegAPAWQGDyANDwH/DwH/DwECAdsPIA0PAQgDDyANDwEUAg8gDQ8BNe8P5ygPAXofLVUPARgEDyANDwH4H7t/9w8BFhjjBcQFDy4IDwH/DwEET48F/0+mWQ9rNG8ENA8Bxh/PWQ8BDx+qRQ/jev8PARpvuf8PAVUvMBJ/K3wv4mEfATNfm3T3H8xGLyRmD3hlBw/xZw8B/w8BLk9QKf8/03QPAUFfZGEGAc/NEw+hFj+mYR88H/+P+8g6IC9Uag8BEQ+fjQ8BFg/HtQ8B/98PAVJvz8cPAT0/ZH8fuYgHDyANDwH//yliL2g0L65/DwEdD8BSrydDH3mbD62b/w8Bas+fAR88Rx+VyA/ayA8B/w8B/w8Bif+vSTp/VEMPAf8fc0vPWIh/kg0PARltin8P9JQPATx/jicfLeIPASeP9P8PAR0E/w8gDQ8BQx+1/w8Bti9OHS8IHi84/w8BzP8f74MPlYMPAXFvSSCP6NbftPE/Nf8PAaX/P7N/L1lCL61gD3IFAhcPjjsP22BcBr9vJgAvU38PAf8PAbcv/gc/FxIDf8gA/z9OaQ8BsR8+FB9kdq/fIA8B/w8BB2MDHx/qHC8Ydg8BVUJ8fytbAAAAAA==');

-- b) Test convert an invalid hloglog counter.
SELECT gp_hyperloglog_get_estimate('blah');
