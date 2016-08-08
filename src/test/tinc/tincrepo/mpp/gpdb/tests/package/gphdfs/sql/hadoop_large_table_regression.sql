\echo '-- start_ignore'
DROP EXTERNAL TABLE IF EXISTS large_table;
\echo '-- end_ignore'



CREATE READABLE EXTERNAL TABLE large_table (
    id BIGINT,
    hex VARCHAR,
    num_recipes BIGINT,
    borland BIGINT,
    glibc BIGINT,
    apple_carbon BIGINT,
    vax BIGINT,
    java BIGINT
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/random_with_seed_1.largetxt')
FORMAT 'TEXT';

\! echo $(date +%s) > %MYD%/gp_large_file_log

SELECT count(*) FROM large_table;

-- Calculate and save elapsed time
\! echo $(( $(date +%s) - $(cat %MYD%/gp_large_file_log) )) > %MYD%/gp_large_file_log
