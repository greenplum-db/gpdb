SET search_path = public, pg_catalog;

BEGIN;

    SELECT #(hyperloglog_accum(i)) hashtag_operator from generate_series(1,100) s(i);

    SELECT #(hyperloglog_accum(i) || hyperloglog_accum(i)) concat_operator from generate_series(1,100) s(i);

    SELECT hyperloglog_accum(i) = hyperloglog_accum(i) equal_operator from generate_series(1,100) s(i);

    SELECT hyperloglog_accum(i) <> hyperloglog_accum(i::text) notequal_operator from generate_series(1,100) s(i);

    SELECT hyperloglog_accum(i) > hyperloglog_accum(i%5) greater_than_operator from generate_series(1,100) s(i);

    SELECT hyperloglog_accum(i%5) < hyperloglog_accum(i) less_than_operator from generate_series(1,100) s(i);

    SELECT hyperloglog_accum(i) >= hyperloglog_accum(i) greater_than_equal_operator_equal from generate_series(1,100) s(i);

    SELECT hyperloglog_accum(i) >= hyperloglog_accum(i%5) greater_than_equal_operator_greater from generate_series(1,100) s(i);

    SELECT hyperloglog_accum(i) <= hyperloglog_accum(i) less_than_operator_equal from generate_series(1,100) s(i);

    SELECT hyperloglog_accum(i%5) <= hyperloglog_accum(i) less_than_equal_operator_less from generate_series(1,100) s(i);

ROLLBACK;
