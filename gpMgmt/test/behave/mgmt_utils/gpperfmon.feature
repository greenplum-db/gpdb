@gpperfmon
Feature: gpperfmon
    """
    Quantum controls how frequently text files get their data put in external tables.
    Note that only certain values are possible: 10, 15, 20, 30, 60.
    In this test setup, it's within the second quantum that the files are processed
    perhaps because of some initialization.
    So for quantum = 10, an initial query-log-to-external-table phase will take up to 20 seconds

    Harvest_interval controls how frequently the perfmon external tables are brought into the heap tables

    Important: for this behave to succeed on macOS, see steps in gpdb/gpAux/gpperfmon/README
    """
    @gpperfmon_install
    Scenario: install gpperfmon
        Given the environment variable "PGDATABASE" is set to "template1"
        Given the database "gpperfmon" does not exist
        When the user runs "gpperfmon_install --port 15432 --enable --password foo"
        Then gpperfmon_install should return a return code of 0
        Then verify that the last line of the file "postgresql.conf" in the master data directory contains the string "gpperfmon_log_alert_level='warning'"
        Then verify that the last line of the file "pg_hba.conf" in the master data directory contains the string "host     all         gpmon         ::1/128    md5"
        And verify that there is a "heap" table "database_history" in "gpperfmon"

    @gpperfmon_run
    Scenario: run gpperfmon
        Given the environment variable "PGDATABASE" is set to "template1"
        Given the database "gpperfmon" does not exist
        Then gpperfmon is configured and running in qamode

    @gpperfmon_database_history
    Scenario: gpperfmon adds to database_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "database_history" tables in "gpperfmon"
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM database_history" is "true"

    @gpperfmon_query_history
    Scenario: gpperfmon adds to query_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "queries_history" tables in "gpperfmon"
        And the user runs "psql -c 'select 1;' template1"
        Then psql should return a return code of 0
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM queries_history" is "true"

    @gpperfmon_query_history
    Scenario: gpperfmon adds to query_history table with carriage returns
        Given gpperfmon is configured and running in qamode
        When the user truncates "queries_history" tables in "gpperfmon"
        And execute following sql in db "gpperfmon" and store result in the context
            """
            --some comment
            SELECT 1
            ;
            """
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM queries_history where query_text like '--some%'" is "true"
        When execute following sql in db "gpperfmon" and store result in the context
            """
            SELECT query_text FROM queries_history where query_text like '--some%'
            """
        Then validate that first column of first stored row has "3" lines of raw output

    @gpperfmon_system_history
    Scenario: gpperfmon adds to system_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "system_history" tables in "gpperfmon"
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM system_history" is "true"

    @gpperfmon_log_alert_history
    Scenario: gpperfmon adds to log_alert_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "log_alert_history" tables in "gpperfmon"
        And the user runs "psql non_existing_database"
        Then psql should return a return code of 2
        When the latest gpperfmon gpdb-alert log is copied to a file with a fake (earlier) timestamp
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM log_alert_history" is "true"
        And the file with the fake timestamp no longer exists

    @gpperfmon_segment_history
    Scenario: gpperfmon adds to segment_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "segment_history" tables in "gpperfmon"
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM segment_history" is "true"

    @gpperfmon_diskspace_history
    Scenario: gpperfmon adds to diskspace_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "diskspace_history" tables in "gpperfmon"
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM diskspace_history" is "true"

    """
    The gpperfmon_skew_cpu_and_cpu_elapsed does not work on MacOS because of Sigar lib limitations.
    To run all the other scenarios and omit this test on MacOS, use:
    $ behave test/behave/mgmt_utils/gpperfmon.feature --tags @gpperfmon --tags ~@gpperfmon_skew_cpu_and_cpu_elapsed
    """
    @gpperfmon_queries_history_metrics
    Scenario: gpperfmon records cpu_elapsed, skew_cpu, skew_rows and rows_out
        Given gpperfmon is configured and running in qamode
        Given the user truncates "queries_history" tables in "gpperfmon"
        Given database "gptest" is dropped and recreated
        When below sql is executed in "gptest" db
        """
        CREATE TABLE sales (id int, date date, amt decimal(10,2))
        DISTRIBUTED BY (id);
        """
        When below sql is executed in "gptest" db
        """
        insert into sales values(generate_series(2,10), '2016-12-01', generate_series(1,20));
        insert into sales values(1, '2016-12-01', generate_series(1,10000000));
        """
        When below sql is executed in "gptest" db
        """
        select gp_segment_id, count(*),sum(pow(amt,2)) from sales group by gp_segment_id;
        """
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM queries_history where cpu_elapsed > 1 and query_text like 'select gp_segment_id, count(*)%'" is "true"
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM queries_history where skew_cpu > 0.05 and db = 'gptest'" is "true"
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM queries_history where skew_rows > 0 and db = 'gptest'" is "true"
        Then wait until the results from boolean sql "SELECT rows_out = count(distinct content) from queries_history, gp_segment_configuration where query_text like 'select gp_segment_id, count(*)%' and db = 'gptest' and content != -1 group by rows_out" is "true"

    @gpperfmon_partition
    Scenario: gpperfmon keeps all partitions upon restart if partition_age not set, drops excess partitions otherwise
        Given gpperfmon is configured and running in qamode
        Given the setting "partition_age" is NOT set in the configuration file "gpperfmon/conf/gpperfmon.conf"
        # default history table is created with three partitions
        Then wait until the results from boolean sql "SELECT count(*) = 3 FROM pg_partitions WHERE tablename = 'diskspace_history'" is "true"
        Then wait until the results from boolean sql "SELECT count(*) = 1 from pg_partitions where tablename = 'diskspace_history' and partitionrangestart like '%' || date_part('year', CURRENT_DATE) || '-' || to_char(CURRENT_DATE, 'MM') || '%';" is "true"
        Then wait until the results from boolean sql "SELECT count(*) = 1 from pg_partitions where tablename = 'diskspace_history' and partitionrangestart like '%' || date_part('year', CURRENT_DATE + interval '1 month') || '-' || to_char(CURRENT_DATE  + interval '1 month' * 1, 'MM') || '%';" is "true"
        When below sql is executed in "gpperfmon" db
            """
            ALTER table diskspace_history add partition
            start ('2-01-17 00:00:00'::timestamp without time zone) inclusive
            end ('3-01-17 00:00:00'::timestamp without time zone) exclusive;
            ALTER table diskspace_history add partition
            start ('3-01-17 00:00:00'::timestamp without time zone) inclusive
            end ('4-01-17 00:00:00'::timestamp without time zone) exclusive;
            ALTER table diskspace_history add partition
            start ('4-01-17 00:00:00'::timestamp without time zone) inclusive
            end ('5-01-17 00:00:00'::timestamp without time zone) exclusive;
            ALTER table diskspace_history add partition
            start ('5-01-17 00:00:00'::timestamp without time zone) inclusive
            end ('6-01-17 00:00:00'::timestamp without time zone) exclusive;
            """
        Then wait until the results from boolean sql "SELECT count(*) = 7 FROM pg_partitions WHERE tablename = 'diskspace_history'" is "true"
        When the user runs command "pkill gpmmon"
        Then wait until the process "gpmmon" is up
        And wait until the process "gpsmon" is up
        # to make sure that no partition reaping is going to happen, we could add a step like this, but it is implementation specific:
        # wait until the latest gpperfmon log file contains the line "partition_age turned off"
        Then wait until the results from boolean sql "SELECT count(*) = 7 FROM pg_partitions WHERE tablename = 'diskspace_history'" is "true"
        When the setting "partition_age = 4" is placed in the configuration file "gpperfmon/conf/gpperfmon.conf"
        Then verify that the last line of the file "gpperfmon/conf/gpperfmon.conf" in the master data directory contains the string "partition_age = 4"
        When the user runs command "pkill gpmmon"
        Then wait until the process "gpmmon" is up
        And wait until the process "gpsmon" is up
        # Note that the code considers partition_age + 1 as the number of partitions to keep
        Then wait until the results from boolean sql "SELECT count(*) = 5 FROM pg_partitions WHERE tablename = 'diskspace_history'" is "true"
