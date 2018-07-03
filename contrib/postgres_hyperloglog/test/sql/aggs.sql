SET search_path = public, pg_catalog;

BEGIN;

    SELECT hyperloglog_distinct(id) distinct_estimate_int_sparse FROM generate_series(1,100) s(id);

    SELECT hyperloglog_distinct(id::text) distinct_estimate_text_sparse FROM generate_series(1,100) s(id);

    SELECT hyperloglog_distinct(id) distinct_estimate_int_dense FROM generate_series(1,100000) s(id);

    SELECT hyperloglog_distinct(id::text) distinct_estimate_text_dense FROM generate_series(1,100000) s(id);

    SELECT hyperloglog_get_estimate(hyperloglog_accum(i)) accum_sparse1 FROM generate_series(1,1) s(i);

    SELECT hyperloglog_get_estimate(hyperloglog_accum(i)) accum_sparse2 FROM generate_series(1,100) s(i);

    SELECT hyperloglog_get_estimate(hyperloglog_accum(i)) accum_sparse3 FROM generate_series(1,1020) s(i);

    SELECT hyperloglog_get_estimate(hyperloglog_accum(i)) accum_dense1 FROM generate_series(1,10000) s(i);

    SELECT hyperloglog_get_estimate(hyperloglog_accum(i)) accum_dense2 FROM generate_series(1,100000) s(i);

    SELECT hyperloglog_get_estimate(hyperloglog_accum(i::text)) accum_dense3 FROM generate_series(1,10000) s(i);

    SELECT hyperloglog_get_estimate(hyperloglog_accum(i::text)) accum_dense4 FROM generate_series(1,100000) s(i);

    SELECT hyperloglog_accum(i,'u') accum_unpacked_sparse FROM generate_series(1,1000) s(i);

    SELECT hyperloglog_accum(i,'U') accum_unpacked_sparse FROM generate_series(1,1000) s(i);

    SELECT hyperloglog_accum(i,'u') accum_unpacked_dense1 FROM generate_series(1,10000) s(i);

    SELECT hyperloglog_accum(i,'U') accum_unpacked_dense2 FROM generate_series(1,10000) s(i);

    SELECT hyperloglog_accum(i,'U') accum_unpacked_dense3 FROM generate_series(1,100000) s(i);

    SELECT hyperloglog_accum(i,'p') accum_packed_sparse FROM generate_series(1,1000) s(i);

    SELECT hyperloglog_accum(i,'P') accum_packed_sparse FROM generate_series(1,1000) s(i);

    SELECT hyperloglog_accum(i,'p') accum_packed_dense1 FROM generate_series(1,10000) s(i);

    SELECT hyperloglog_accum(i,'P') accum_packed_dense2 FROM generate_series(1,10000) s(i);

    SELECT hyperloglog_accum(i,'P') accum_packed_dense3 FROM generate_series(1,100000) s(i);

    CREATE TEMP TABLE TEST_ACCUM AS
    SELECT
	i
    FROM
	generate_series(1,100000) s(i)
    ;

    SELECT hyperloglog_accum(i,'u') accum_unpacked_two_level_agg FROM TEST_ACCUM;

    SELECT hyperloglog_accum(i,'p') accum_packed_two_level_Agg FROM TEST_ACCUM;

    SELECT 
        hyperloglog_get_estimate(hyperloglog_merge(j.counters)) merge_agg_sparse 
    FROM 
        (SELECT 
            i, 
            hyperloglog_accum(i*400 + m.m) counters 
        FROM 
            generate_series(1,100) s(i) 
        cross join 
            (SELECT 
                m 
            from 
                generate_series(1,400) s(m)) m 
            group by i) j;

    SELECT 
        hyperloglog_get_estimate(hyperloglog_merge(j.counters)) merge_agg_dense 
    FROM 
        (SELECT 
            i , 
            hyperloglog_accum(i*10000 + m.m) counters 
        FROM 
            generate_series(1,100) s(i) 
        cross join 
            (SELECT 
                m 
            from 
                generate_series(1,10000) s(m)) m 
            group by i) j;

    SELECT 
        sum(j.counters) sum_sparse
    FROM
        (SELECT
            i,
            hyperloglog_accum(i*400 + m.m) counters
        FROM
            generate_series(1,100) s(i)
        cross join
            (SELECT
                m
            from
                generate_series(1,400) s(m)) m
            group by i) j;
    
    SELECT
        sum(j.counters) sum_dense
    FROM
        (SELECT
            i ,
            hyperloglog_accum(i*10000 + m.m) counters
        FROM
            generate_series(1,100) s(i)
        cross join
            (SELECT
                m
            from
                generate_series(1,10000) s(m)) m
            group by i) j;

    CREATE TEMP TABLE test as select 
        hyperloglog_accum(i) a 
    FROM 
        generate_series(1,1000) s(i);

    INSERT INTO test SELECT 
        hyperloglog_accum(i) 
    FROM 
        generate_series(1,2000) s(i);

    SELECT 
        1, 
        #(hyperloglog_merge(a)) 
    FROM 
        test 
    UNION ALL 
    SELECT 
        2, 
        #(hyperloglog_merge(a)) 
    FROM 
        test;
 
ROLLBACK;
