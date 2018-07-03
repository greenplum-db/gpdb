SET search_path = public, pg_catalog;

BEGIN;
    SELECT length(hyperloglog_accum(i)) sparse_length from generate_series(1,1) s(i);

    SELECT length(hyperloglog_accum(i)) sparse_length1 from generate_series(1,100) s(i);

    SELECT length(hyperloglog_accum(i)) sparse_length2 from generate_series(1,500) s(i);

    SELECT length(hyperloglog_accum(i)) sparse_length3 from generate_series(1,1000) s(i);

    SELECT length(hyperloglog_accum(i)) sparse_length4 from generate_series(1,1020) s(i);

    SELECT length(hyperloglog_accum(i)) dense_length from generate_series(1,1021) s(i);

    SELECT length(hyperloglog_accum(i)) dense_length1 from generate_series(1,10000) s(i);

    SELECT length(hyperloglog_accum(i)) dense_length2 from generate_series(1,100000) s(i);

    SELECT length(hyperloglog_decomp(hyperloglog_accum(i))) sparse_decomp_length from generate_series(1,1) s(i);

    SELECT length(hyperloglog_decomp(hyperloglog_accum(i))) sparse_decomp_length from generate_series(1,1020) s(i);

    SELECT length(hyperloglog_decomp(hyperloglog_accum(i))) dense_decomp_length from generate_series(1,10000) s(i);

    SELECT length(hyperloglog_decomp(hyperloglog_comp(hyperloglog_decomp(hyperloglog_accum(i))))) sparse_chain_length from generate_series(1,100) s(i);

    SELECT length(hyperloglog_decomp(hyperloglog_comp(hyperloglog_decomp(hyperloglog_accum(i))))) dense_chain_length from generate_series(1,10000) s(i);

    SELECT #(hyperloglog_decomp(hyperloglog_comp(hyperloglog_decomp(hyperloglog_accum(i))))) sparse_chain_val from generate_series(1,100) s(i);

    SELECT #(hyperloglog_decomp(hyperloglog_comp(hyperloglog_decomp(hyperloglog_accum(i))))) dense_chain_val from generate_series(1,10000) s(i);

    SELECT hyperloglog_get_estimate(hyperloglog_merge(hyperloglog_decomp(hyperloglog_accum(i)),hyperloglog_comp(hyperloglog_accum(i)))) merge_decomp_comp from generate_series(1,100) s(i);

    SELECT hyperloglog_get_estimate(hyperloglog_merge(hyperloglog_comp(hyperloglog_accum(i)),hyperloglog_decomp(hyperloglog_accum(i)))) merge_comp_decomp from generate_series(1,100) s(i);

    SELECT hyperloglog_get_estimate(hyperloglog_merge(hyperloglog_decomp(hyperloglog_accum(i)),hyperloglog_decomp(hyperloglog_accum(i)))) merge_decomp_decomp from generate_series(1,100) s(i);
ROLLBACK;
