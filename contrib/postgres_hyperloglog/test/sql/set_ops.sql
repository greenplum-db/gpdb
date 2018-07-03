SET search_path = public, pg_catalog;

BEGIN;

    SELECT hyperloglog_equal(hyperloglog_accum(i),hyperloglog_accum(i)) sparse_equality from generate_series(1,100) s(i);
  
    SELECT hyperloglog_equal(hyperloglog_accum(i),hyperloglog_accum(i)) sparse_equality2 from generate_series(1,1000) s(i);
  
    SELECT hyperloglog_equal(hyperloglog_accum(i),hyperloglog_accum(i)) dense_equality from generate_series(1,10000) s(i);
  
    SELECT hyperloglog_not_equal(hyperloglog_accum(i),hyperloglog_accum(i*-1)) sparse_inequality from generate_series(1,100) s(i);
  
    SELECT hyperloglog_not_equal(hyperloglog_accum(i),hyperloglog_accum(i*-1)) sparse_inequality2 from generate_series(1,1000) s(i);
  
    SELECT hyperloglog_not_equal(hyperloglog_accum(i),hyperloglog_accum(i*-1)) dense_inequality from generate_series(1,10000) s(i);
  
    SELECT hyperloglog_intersection(hyperloglog_accum(i),hyperloglog_accum(i*-1)) no_intersection_sparse from generate_series(1,100) s(i);
  
    SELECT hyperloglog_intersection(hyperloglog_accum(i),hyperloglog_accum(i*-1)) no_intersection_dense from generate_series(1,10000) s(i);
  
    SELECT hyperloglog_intersection(hyperloglog_accum(i),hyperloglog_accum(i*-1)) intersection_sparse from generate_series(-10,100) s(i);
  
    SELECT hyperloglog_intersection(hyperloglog_accum(i),hyperloglog_accum(i*-1)) intersection_dense from generate_series(-1000,10000) s(i);
  
    SELECT hyperloglog_intersection(hyperloglog_accum(i),hyperloglog_accum(i%100)) intersection_mixed from generate_series(1,10000) s(i);
  
    SELECT hyperloglog_union(hyperloglog_accum(i),hyperloglog_accum(i)) union_sparse from generate_series(1,100) s(i);
  
    SELECT hyperloglog_union(hyperloglog_accum(i),hyperloglog_accum(i)) union_dense from generate_series(1,10000) s(i);
  
    SELECT hyperloglog_union(hyperloglog_accum(i%5),hyperloglog_accum(i)) union_mixed1 from generate_series(1,10000) s(i);
  
    SELECT hyperloglog_union(hyperloglog_accum(i),hyperloglog_accum(i%5)) union_mixed2 from generate_series(1,10000) s(i);
  
    SELECT hyperloglog_compliment(hyperloglog_accum(i),hyperloglog_accum(i%100)) compliment from generate_series(1,10000) s(i);
  
    SELECT hyperloglog_compliment(hyperloglog_accum(i%100),hyperloglog_accum(i)) = 0 compliment from generate_series(1,10000) s(i);
  
    SELECT hyperloglog_symmetric_diff(hyperloglog_accum(i),hyperloglog_accum(i*-1))  symmetric_diff from generate_series(-1000,10000) s(i);

ROLLBACK;
