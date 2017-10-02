@gptransfer
Feature: gptransfer tests

    @gptransfer_setup
    Scenario: gptransfer setup
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer_setup.sql -d template1"
        Then psql should return a return code of 0

    @T339833
    Scenario: gptransfer full, source cluster -> source cluster
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_SOURCE_USER --dest-port $GPTRANSFER_SOURCE_PORT --dest-host $GPTRANSFER_SOURCE_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2

    @T339831
    Scenario: gptransfer full no validator
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Using batch size of 10" to stdout
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows
        And verify that table "empty_table" in "gptransfer_testdb4" has "0" rows
        And verify that table "one_row_table" in "gptransfer_testdb4" has "1" rows
        And verify that table "wide_rows" in "gptransfer_testdb5" has "10" rows

    @T339889
    @T339913
    Scenario: gptransfer full md5 validator in TEXT format with '\010' delimiter
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --delimiter '\010' --format text --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate md5 --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows
        And verify that table "empty_table" in "gptransfer_testdb4" has "0" rows
        And verify that table "one_row_table" in "gptransfer_testdb4" has "1" rows
        And verify that table "wide_rows" in "gptransfer_testdb5" has "10" rows

    @skip_source_5
    Scenario: gptransfer sha256 validator should fail when the source cluster does not have pgcrypto installed
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --delimiter '\010' --format text --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate sha256 --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Source system must have pgcrypto installed when using sha256 validator" to stdout

    @skip_source_43
    Scenario: gptransfer full sha256 validator in TEXT format with '\010' delimiter
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --delimiter '\010' --format text --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate sha256 --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows
        And verify that table "empty_table" in "gptransfer_testdb4" has "0" rows
        And verify that table "one_row_table" in "gptransfer_testdb4" has "1" rows
        And verify that table "wide_rows" in "gptransfer_testdb5" has "10" rows

    @T339888
    @T339914
    Scenario: gptransfer full count validator in TEXT format with '\010' delimiter
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --delimiter '\010' --format text --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate count --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows
        And verify that table "empty_table" in "gptransfer_testdb4" has "0" rows
        And verify that table "one_row_table" in "gptransfer_testdb4" has "1" rows
        And verify that table "wide_rows" in "gptransfer_testdb5" has "10" rows

    @T439914
    Scenario: gptransfer full md5 validator in CSV format
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate md5 --format=csv --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows
        And verify that table "empty_table" in "gptransfer_testdb4" has "0" rows
        And verify that table "one_row_table" in "gptransfer_testdb4" has "1" rows
        And verify that table "wide_rows" in "gptransfer_testdb5" has "10" rows

    @skip_source_43
    Scenario: gptransfer full sha256 validator in CSV format
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate sha256 --format=csv --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows
        And verify that table "empty_table" in "gptransfer_testdb4" has "0" rows
        And verify that table "one_row_table" in "gptransfer_testdb4" has "1" rows
        And verify that table "wide_rows" in "gptransfer_testdb5" has "10" rows

    @T439915
    Scenario: gptransfer full count validator in CSV format
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate count --format=csv --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows
        And verify that table "empty_table" in "gptransfer_testdb4" has "0" rows
        And verify that table "one_row_table" in "gptransfer_testdb4" has "1" rows
        And verify that table "wide_rows" in "gptransfer_testdb5" has "10" rows

    @T339915
    Scenario: gptransfer full with drop
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer_setup.sql -d template1"
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --drop --batch-size=10"
        Then gptransfer should return a return code of 2

    @T339916
    Scenario: gptransfer full with skip
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer_setup.sql -d template1"
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --skip-existing --batch-size=10"
        Then gptransfer should return a return code of 2

    @T339917
    Scenario: gptransfer full with truncate
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer_setup.sql -d template1"
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --truncate --batch-size=10"
        Then gptransfer should return a return code of 2

    @T339947
    Scenario: gptransfer single table with drop
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer_setup.sql -d template1"
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --drop -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows

    @T506755
    Scenario: gptransfer invisible delimiter
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer_setup.sql -d template1"
        And the user runs "gptransfer --delimiter '\010' --format text -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --drop -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows

    @T886744
    Scenario: gptransfer -T exclude with --full
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full -T 'non_existdb.public./.?/1' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Cannot specify -T and --full options together" to stdout

    @T886745
    Scenario: gptransfer -F exclude with --full
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full -F test/behave/mgmt_utils/steps/data/gptransfer_wildcard_infile --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Cannot specify -F and --full options together" to stdout

    @T886746
    Scenario: gptransfer -T exclude non exist objects
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 -T 'non_existdb.public./.?/1' -T 'gptransfer_testdb1.public.non_exist_table' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Found no tables to exclude from transfer table list" to stdout

    @T886747
    Scenario: gptransfer -T table level wildcard
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 -T 'gptransfer_testdb1.public./.?/1' -T 'gptransfer_testdb1.public./.*/0' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that there is no table "t0" in "gptransfer_testdb1"
        And verify that there is no table "t1" in "gptransfer_testdb1"
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows

    @T886749
    Scenario: gptransfer -T schema level wildcard
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 -T 'gptransfer_testdb1.pu/.?/lic.t1' -T 'gptransfer_testdb1.pub/.*/.t0' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -d gptransfer_testdb3 -T 'gptransfer_testdb3.s/\w?/.t/\d+/' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a"
        Then gptransfer should return a return code of 0
        And verify that there is no table "t0" in "gptransfer_testdb1"
        And verify that there is no table "t1" in "gptransfer_testdb1"
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that there is no table "s1.t0" in "gptransfer_testdb3"
        And verify that there is no table "s2.t0" in "gptransfer_testdb3"

    @T886750
    Scenario: gptransfer -T database level wildcard
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 -T 'gptransfer_testdb1/|/gptransfer_testdb3.public.t0' -T '/.?/ptransfer_testdb1.public.t1' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that there is no table "t0" in "gptransfer_testdb1"
        And verify that there is no table "t1" in "gptransfer_testdb1"
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows

    @T506747
    Scenario: gptransfer -t table level wildcard
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t 'gptransfer_testdb1.public./.*/0' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t 'gptransfer_testdb1.public./.?/1' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t 'gptransfer_testdb1.public./.+/2' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t 'gptransfer_testdb3.public./.{1,2}/' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows

    @T506749
    Scenario: gptransfer -t schema level wildcard
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t 'gptransfer_testdb1.pub/.*/.t0' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t 'gptransfer_testdb1.pu/.?/lic.t1' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t 'gptransfer_testdb1.pu/.+/lic.t2' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t 'gptransfer_testdb3.s/\w?/.t/\d+/' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "s1.t0" in "gptransfer_testdb3" has "800" rows
        And verify that table "s2.t0" in "gptransfer_testdb3" has "900" rows

    @T506750
    Scenario: gptransfer -t database level wildcard
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t 'gptransfer_testdb1/|/gptransfer_testdb3.public.t0' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t '/.?/ptransfer_testdb1.public.t1' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t '/.+/ptransfer_testdb1.public.t2' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t 'gptransfer_testdb/\d+/.s1.t0' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t 'gptransfer_t/.*3/.s2.t0' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        And the user runs "gptransfer -t 'gptransfer_testdb/[0-9]?/.public.t4' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "s1.t0" in "gptransfer_testdb3" has "800" rows
        And verify that table "s2.t0" in "gptransfer_testdb3" has "900" rows

    @T339948
    Scenario: gptransfer single table with skip
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer_setup.sql -d template1"
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --skip-existing --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Found no tables to transfer" to stdout

    @T339949
    Scenario: gptransfer single table with truncate
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer_setup.sql -d template1"
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --truncate -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows

    @T339842
    @T339950
    Scenario: gptransfer single database with md5 validation
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -c "CREATE TABLE my_random_dist_table(i int) DISTRIBUTED RANDOMLY;" -d gptransfer_testdb1"
        And the user runs "gptransfer -d gptransfer_testdb1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate md5 -v --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And the user runs "psql gptransfer_testdb1 -c '\d+ my_random_dist_table'"
        Then psql should print "Distributed randomly" to stdout 1 times
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -c "DROP TABLE my_random_dist_table;" -d gptransfer_testdb1"

    @T339951
    Scenario: gptransfer single table with md5 validation
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate md5 --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows

    @T339952
    Scenario: gptransfer input file with md5 validation
        Given the gptransfer test is initialized
        And the user runs "gptransfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate md5 -f test/behave/mgmt_utils/steps/data/gptransfer_infile --batch-size=1"
        Then gptransfer should return a return code of 0
        And verify that gptransfer is in order of "test/behave/mgmt_utils/steps/data/gptransfer_infile" when partition transfer is "None"
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows

    @skip_source_43
    Scenario: gptransfer single database with sha256 validation
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -c "CREATE TABLE my_random_dist_table(i int) DISTRIBUTED RANDOMLY;" -d gptransfer_testdb1"
        And the user runs "gptransfer -d gptransfer_testdb1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate sha256 -v --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And the user runs "psql gptransfer_testdb1 -c '\d+ my_random_dist_table'"
        Then psql should print "Distributed randomly" to stdout 1 times
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -c "DROP TABLE my_random_dist_table;" -d gptransfer_testdb1"

    @T339951
    @skip_source_43
    Scenario: gptransfer single table with sha256 validation
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate sha256 --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows

    @T339952
    @skip_source_43
    Scenario: gptransfer input file with sha256 validation
        Given the gptransfer test is initialized
        And the user runs "gptransfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate sha256 -f test/behave/mgmt_utils/steps/data/gptransfer_infile --batch-size=1"
        Then gptransfer should return a return code of 0
        And verify that gptransfer is in order of "test/behave/mgmt_utils/steps/data/gptransfer_infile" when partition transfer is "None"
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows

    @T886748
    Scenario: gptransfer -F exclude input file
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 -d gptransfer_testdb3 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate md5 -F test/behave/mgmt_utils/steps/data/gptransfer_wildcard_infile --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that database "gptransfer_testdb1" does not exist
        And verify that database "gptransfer_testdb3" does not exist

    @T339953
    Scenario: gptransfer input file to destination database with conflict
        Given the gptransfer test is initialized
        And the user runs "gptransfer --dest-database gptransfer_destdb --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate md5 -f test/behave/mgmt_utils/steps/data/gptransfer_infile --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Multiple tables map to gptransfer_destdb.public.t0.  Remove one of the tables from the list or do not use the --dest-database option." to stdout

    @T339897
    @T339918
    Scenario: gptransfer multiple databases to single destination database
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 -d gptransfer_testdb4 --dest-database gptransfer_destdb --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate md5 --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_destdb" has "100" rows
        And verify that table "t1" in "gptransfer_destdb" has "200" rows
        And verify that table "t2" in "gptransfer_destdb" has "300" rows
        And verify that table "empty_table" in "gptransfer_destdb" has "0" rows
        And verify that table "one_row_table" in "gptransfer_destdb" has "1" rows

    @T339919
    Scenario: gptransfer multiple databases to single destination database with conflict
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 -d gptransfer_testdb3 --dest-database gptransfer_destdb --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate md5 --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Multiple tables map to gptransfer_destdb.public.t0.  Remove one of the tables from the list or do not use the --dest-database option." to stdout

    @T339920
    Scenario: gptransfer multiple tables to single destination database with conflict
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 -t gptransfer_testdb3.public.t0 --dest-database gptransfer_destdb --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate md5 --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Multiple tables map to gptransfer_destdb.public.t0.  Remove one of the tables from the list or do not use the --dest-database option." to stdout

    @T339955
    Scenario: gptransfer database where source database does not exist
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d bad_db --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Find no user databases matching "bad_db" in source system" to stdout
        And gptransfer should print "Found no tables to transfer" to stdout

    @T339956
    Scenario: gptransfer database where source table does not exist
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.bad_table --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Found no tables to transfer" to stdout

    @T339851
    @T339957
    Scenario: gptransfer single table with analyze
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --analyze --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Analyzing destination table" to stdout
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows

    @T339958
    Scenario: gptransfer single table with exclusive lock
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -x --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Exclusive locks will be used during table transfers" to stdout
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows

    @T339895
    @T339959
    @T339921
    Scenario: gptransfer single table to different database on same system
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer_setup.sql -d template1"
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --dest-database gptransfer_destdb --source-port $GPTRANSFER_DEST_PORT --source-host $GPTRANSFER_DEST_HOST --source-user $GPTRANSFER_DEST_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_destdb" has "100" rows

    @T339960
    Scenario: gptransfer single table with bad fqn
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Invalid fully qualified table name" to stdout

    # this test creates an incomplete map file by *removing* the first line from the source map file.
    # If you have only 1 segment, you will be left with an empty hosts map, and this test will fail
    @T339837
    Scenario: gptransfer incomplete map file
        Given the gptransfer test is initialized
        And an incomplete map file is created
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file /tmp/incomplete_map_file --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "missing from map file" to stdout

    @T339840
    Scenario: gptransfer invalid work-base-dir
        Given the gptransfer test is initialized
        And an incomplete map file is created
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --work-base-dir /root --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Permission denied" to stdout

    @T339841
    Scenario: gptransfer with dependent database object
        Given the gptransfer test is initialized
        And there is a table "dependent_table" dependent on function "test_function" in database "gptransfer_testdb1" on the source system
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that function "test_function" exists in database "gptransfer_testdb1"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -c 'drop table dependent_table; drop function test_function(int);' -d gptransfer_testdb1"

    @T339838
    @T339961
    Scenario: gptransfer bad map file
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file test/behave/mgmt_utils/steps/data/gptransfer_bad_map_file --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "missing from map file" to stdout

    @unsupported_identifiers
    Scenario: gptransfer unsupported identifiers in table name
        Given the gptransfer test is initialized
        And the user runs "psql -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -c 'create table "tt*$#@"(i int)' gptransfer_testdb1"
        And the user runs "gptransfer -d gptransfer_testdb1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 1
        And gptransfer should print "Found unsupported identifiers" to stdout
        And the user runs "psql -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -c 'drop table "tt*$#@"' gptransfer_testdb1"

    @unsupported_identifiers
    Scenario: gptransfer unsupported identifiers in schema name
        Given the gptransfer test is initialized
        And the user runs "psql -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -c 'create schema "Bad*$#@Schema"' gptransfer_testdb1"
        And the user runs "psql -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -c 'create table "Bad*$#@Schema".test(i int)' gptransfer_testdb1"
        And the user runs "gptransfer -d gptransfer_testdb1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 1
        And gptransfer should print "Found unsupported identifiers" to stdout
        And the user runs "psql -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -c 'drop schema "Bad*$#@Schema" cascade' gptransfer_testdb1"

    @unsupported_identifiers
    Scenario: gptransfer unsupported identifiers in database name
        Given the gptransfer test is initialized
        And the user runs "createdb -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST Bad*\$#@DB"
        And the user runs "psql -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -c 'create table test(i int)' Bad*\$#@DB"
        And the user runs "gptransfer -d 'Bad/[*][$]/#@DB' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 1
        And gptransfer should print "Found unsupported identifiers" to stdout
        And the user runs "dropdb -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST Bad*\$#@DB"

    @T339962
    Scenario: gptransfer should not allow --full and --dest-database option
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --dest-database gptransferdestdb --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "--dest-database option cannot be used with the --full option" to stdout

    @T339857
    @T339963
    Scenario: gptransfer should not allow --batch-size > 10
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --batch-size 11 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Invalid value for --batch-size.  --batch-size must be greater than 0 and less than 10" to stdout

    @T339856
    Scenario: gptransfer should not allow --batch-size < 1
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --batch-size 0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Invalid value for --batch-size.  --batch-size must be greater than 0 and less than 10" to stdout

    @T339967
    Scenario: gptransfer should not allow --truncate and --skip-existing
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --truncate --skip-existing --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Cannot specify --truncate and --skip-existing together" to stdout

    @T339968
    Scenario: gptransfer should not allow --truncate and --drop
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --truncate --drop --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Cannot specify --truncate and --drop together" to stdout

    @T339969
    Scenario: gptransfer should not allow --drop and --skip-existing
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --drop --skip-existing --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Cannot specify --drop and --skip-existing together" to stdout

    @T99999999
    Scenario: gptransfer should not allow --full, -f, -t, or -d to be called together
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 -t gptransfer_testdb1.public.temp_table --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate=md5 --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Only one of --full, -f, -t, or -d can be specified" to stdout
        And the user runs "gptransfer -d gptransfer_testdb1 -f test/behave/mgmt_utils/steps/data/gptransfer_infile --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate=md5 --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Only one of --full, -f, -t, or -d can be specified" to stdout
        And the user runs "gptransfer -t gptransfer_testdb1.public.temp_table -f test/behave/mgmt_utils/steps/data/gptransfer_infile --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate=md5 --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Only one of --full, -f, -t, or -d can be specified" to stdout
        And the user runs "gptransfer --full -d gptransfer_testdb1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate=md5 --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Only one of --full, -f, -t, or -d can be specified" to stdout
        And the user runs "gptransfer --full -d gptransfer_testdb1 -t gptransfer_testdb1.public.temp_table -f test/behave/mgmt_utils/steps/data/gptransfer_infile --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --validate=md5 --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Only one of --full, -f, -t, or -d can be specified" to stdout

    @T339970
    Scenario: gptransfer missing one of --full, -f, -t or -d
        Given the gptransfer test is initialized
        And the user runs "gptransfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "One of --full, -f, -t, or -d must be specified" to stdout

    Scenario: gptransfer invalid set of options, pass in unkown of unsupported options, eg: -o
        Given the gptransfer test is initialized

        And the user runs "gptransfer -a -t gptransfer_testdb1.public.t0 -o --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2

    Scenario: gptransfer invalid set of options, pass in option without value
        Given the gptransfer test is initialized

        And the user runs "gptransfer -a -t gptransfer_testdb1.public.t0 --source-port --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2

    @T339844
    @T339896
    Scenario:   gptransfer with -f options without --skip-existing, --truncate, or --drop option when dest system already has some of the tables
        Given the gptransfer test is initialized
        And the user runs "createdb -U $GPTRANSFER_DEST_USER -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST gptransfer_testdb1"
        And the user runs "psql -U $GPTRANSFER_DEST_USER -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -c 'create table t0(i int)' gptransfer_testdb1"
        And the user runs "gptransfer -f test/behave/mgmt_utils/steps/data/gptransfer_infile --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Table gptransfer_testdb1.public.t0 exists in database gptransfer_testdb1" to stdout

    @T339845
    Scenario: gptransfer with -t without --skip-existing, --truncate, or --drop option when dest system already has the table
        Given the gptransfer test is initialized
        And the user runs "createdb -U $GPTRANSFER_DEST_USER -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST gptransfer_testdb1"
        And the user runs "psql -U $GPTRANSFER_DEST_USER -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -c 'create table t0(i int)' gptransfer_testdb1"
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Table gptransfer_testdb1.public.t0 exists in database gptransfer_testdb1" to stdout

    @T339849
    Scenario: gptransfer with -a
        Given the gptransfer test is initialized
        And the user runs "createdb -U $GPTRANSFER_DEST_USER -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST gptransfer_testdb1"
        And the user runs "psql -U $GPTRANSFER_DEST_USER -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -c 'create table t0(i int)' gptransfer_testdb1"
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --drop -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should not print "Do you want to continue? Yy|Nn" to stdout

    @T339852
    Scenario: gptransfer using default port 8000
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -v --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "gptransfer_testdb1.public.t0 -p 8000" to stdout

    @T339853
    Scenario: gptransfer using already-occupied port
        Given the gptransfer test is initialized
        And the gpfdists occupying port 21100 on host "GPTRANSFER_MAP_FILE"
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -v --base-port=21100 --batch-size=10"
        Then gptransfer should return a return code of 0
        And the gpfdists running on port 21100 get cleaned up from host "GPTRANSFER_MAP_FILE"

    @T339854
    Scenario: gptransfer with batch-size of 2
        Given the gptransfer test is initialized
        When the user runs the command "test/behave/mgmt_utils/steps/scripts/test_gptransfer_batch_size.sh /tmp/batch_size" in the background
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 -t gptransfer_testdb1.public.t1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --base-port=$PGPORT"
        Then gptransfer should return a return code of 0
        And verify that the file "/tmp/batch_size" contains the string "3"
        And the temporary file "/tmp/batch_size" is removed
        And gptransfer should print "Using batch size of 2" to stdout
        And the user runs "kill -9 `ps aux | grep test_gptransfer_batch_size.sh | awk '{print $2}'`"

    @T339903
    Scenario:  gptransfer with --dry-run
        Given the gptransfer test is initialized
        And the user runs "createdb -U $GPTRANSFER_DEST_USER -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST gptransfer_testdb1"
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --dry-run --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "The following tables will be transfered:" to stdout
        And verify that there is no table "public.t0" in "gptransfer_testdb1"

    @T339904
    Scenario:   gptransfer with -f options, source system does not have tables, should error out for them
        Given the gptransfer test is initialized
        And the user runs "gptransfer -f test/behave/mgmt_utils/steps/data/gptransfer_bad_infile --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Found no tables to transfer" to stdout

    @T339850
    Scenario:   gptransfer --analyze option, do not specify as by default
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And gptransfer should not print "Analyzing destination table gptransfer_testdb1.public.t0" to stdout

    @T339859
    Scenario:  gptransfer -d, dest system already has the database
        Given the gptransfer test is initialized
        And the user runs "createdb -U $GPTRANSFER_DEST_USER -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST gptransfer_testdb1"
        And the user runs "gptransfer -d gptransfer_testdb1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows

    @T339899
    Scenario:   gptransfer --dest-database option not specified within the same system, should error out
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_SOURCE_USER --dest-port $GPTRANSFER_SOURCE_PORT --dest-host $GPTRANSFER_SOURCE_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Source and destination systems cannot be the same unless --dest-database or --partition-transfer option is set" to stdout

    @T339900
    Scenario:  gptransfer --dest-database option specified to a different name within the same system, should succeed
        Given the gptransfer test is initialized
        And the user runs "gptransfer -d gptransfer_testdb1 --dest-database gptransfer_destdb --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_SOURCE_USER --dest-port $GPTRANSFER_SOURCE_PORT --dest-host $GPTRANSFER_SOURCE_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        And the user runs "psql -t -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -c 'select array_agg(foo) from (select count(*) foo from t0 union select count(*) foo from t1 union select count(*) foo from t2) q' gptransfer_destdb && dropdb -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST gptransfer_destdb"
        Then gptransfer should return a return code of 0
        And psql should print "{100,200,300}" to stdout

    @T339886
    Scenario:   gptransfer -x, test the lock from source system
        Given the database is running
        And the database "gptransfer_destdb" does not exist
        And the database "gptransfer_testdb1" does not exist
        And the database "gptransfer_testdb3" does not exist
        And the database "gptransfer_testdb4" does not exist
        And the database "gptransfer_testdb5" does not exist
        And the user runs "psql -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -c 'CREATE TABLE tlock_test(i int)' gptransfer_testdb1"
        And the user runs "psql -U $GPTRANSFER_SOURCE_USER -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -c 'INSERT INTO tlock_test SELECT generate_series FROM generate_series(1,100), pg_sleep(120)' gptransfer_testdb1 & gptransfer -x -t gptransfer_testdb1.public.tlock_test --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE"
        Then verify that table "tlock_test" in "gptransfer_testdb1" has "100" rows

    @T339902
    Scenario: gptransfer within the same system with --dest-host not specified
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0

    @T339861
    Scenario: gptransfer with --dest-port not specified
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0

    @T339863
    Scenario: gptransfer with --dest-user not specified to use default system user
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0

    @T432109
    Scenario: gptransfer --full with user database exist in destination system
        Given the gptransfer test is initialized
        And database "gptransfer_destdb" exists
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "--full option specified but databases exist in destination system" to stdout

    @T339864
    Scenario: gptransfer with --dest-user root
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user root --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2

    @T339865
    Scenario: gptransfer with log in ~/gpAdminLogs
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that a log was created by gptransfer in the user's "gpAdminLogs" directory

    @T339866
    Scenario: gptransfer with log in /tmp
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -l /tmp --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that a log was created by gptransfer in the "/tmp" directory

    @T339867
    Scenario: gptransfer with log in nonexistent directory
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -l ~/gpAdminLogs/gpAdminLogs --batch-size=10"
        Then gptransfer should return a return code of 0

    @T339872
    Scenario: gptransfer --schema-only
        Given the gptransfer test is initialized
        And database "gptransfer_testdb1" exists
        And schema "test_schema" exists in "gptransfer_testdb1"
        And the user runs "gptransfer --schema-only -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that the schema "test_schema" exists in "gptransfer_testdb1"

    @T339908
    Scenario: gptransfer --schema-only conflicts with --truncate
        Given the gptransfer test is initialized
        And the user runs "gptransfer --schema-only -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --truncate --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Cannot specify --schema-only and --truncate together" to stdout

    @T339907
    Scenario: gptransfer -q
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 -q --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should not print "Transfering" to stdout
        And verify that a log was created by gptransfer in the user's "gpAdminLogs" directory

    @T339873
    Scenario: gptransfer --skip-existing conflicts with --full
        Given the gptransfer test is initialized
        And the user runs "gptransfer --skip-existing --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Cannot specify --drop, --truncate or --skip-existing with --full option" to stdout

    @T339877
    Scenario: gptransfer with default source user
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that there is a "heap" table "public.t0" in "gptransfer_testdb1"

    @T339876
    Scenario: gptransfer with invalid dest user
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user non_existent_user --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "FATAL:  role "non_existent_user" does not exist" to stdout

    @T339878
    Scenario: gptransfer with parallelism 25
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --enable-test "
        Then gptransfer should return a return code of 0
        And gptransfer should print "Using sub-batch size of 25" to stdout

    @T339879
    Scenario: gptransfer with parallelism 10
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -v --sub-batch-size 10 --enable-test --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Using sub-batch size of 10" to stdout

    @T339880
    Scenario: gptransfer with sub batch size too large
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --sub-batch-size 1000"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Invalid value for --sub-batch-size.  Must be greater than 0 and less than 50" to stdout

    @T339881
    Scenario: gptransfer with sub batch size too small
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --sub-batch-size 0 --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Invalid value for --sub-batch-size.  Must be greater than 0 and less than 50" to stdout

    @T339882
    Scenario: gptransfer -t db.public.table where table does not exist in destination system
        Given the gptransfer test is initialized
        And database "gptransfer_testdb1" exists
        And schema "public" exists in "gptransfer_testdb1"
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that there is a "heap" table "public.t0" in "gptransfer_testdb1"

    @T339883
    Scenario: gptransfer -t db.schema.table where db does not exist in destination system
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that there is a "heap" table "public.t0" in "gptransfer_testdb1"

    @T339838
    Scenario: gptransfer invalid map file
        Given the gptransfer test is initialized
        And the user runs "gptransfer -t gptransfer_testdb1.public.t0 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that there is a "heap" table "public.t0" in "gptransfer_testdb1"

    @T339911
    Scenario: gptransfer --verbose
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --verbose --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "\[DEBUG\]" to stdout

    @T439911
    Scenario: gptransfer full in CSV format with final count validation
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --format=csv > /tmp/gptransfer_stdout.txt --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that the file "/tmp/gptransfer_stdout.txt" contains "Running final table row count validation on destination tables"
        And verify that table "t0" in "gptransfer_testdb1" has "100" rows
        And verify that table "t1" in "gptransfer_testdb1" has "200" rows
        And verify that table "t2" in "gptransfer_testdb1" has "300" rows
        And verify that table "t0" in "gptransfer_testdb3" has "700" rows
        And verify that table "empty_table" in "gptransfer_testdb4" has "0" rows
        And verify that table "one_row_table" in "gptransfer_testdb4" has "1" rows
        And verify that table "wide_rows" in "gptransfer_testdb5" has "10" rows

    @T439912
    Scenario: gptransfer full with --no-final-count option
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --no-final-count > /tmp/gptransfer_stdout.txt --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that the file "/tmp/gptransfer_stdout.txt" does not contain "Running final table row count validation on destination tables"

    @T339830
    Scenario: gptransfer full with all types of database objects created
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/teardown.sql -d template1"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/teardown.sql -d template1"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/setup.sql -d template1"
        And the user "GPTRANSFER_SOURCE_USER" creates filespace_config file for "fs" on host "GPTRANSFER_SOURCE_HOST" with gpdb port "GPTRANSFER_SOURCE_PORT" and config "gpfilespace_config_src" in "HOME" directory
        And the user "GPTRANSFER_DEST_USER" creates filespace_config file for "fs" on host "GPTRANSFER_DEST_HOST" with gpdb port "GPTRANSFER_DEST_PORT" and config "gpfilespace_config_dest" in "HOME" directory
        And the user modifies the external_table.sql file "test/behave/mgmt_utils/steps/data/gptransfer_setup/external_table.sql" with host "GPTRANSFER_SOURCE_HOST" and port "2345"
        And the user starts the gpfdist on host "GPTRANSFER_SOURCE_HOST" and port "2345" in work directory "HOME" from remote "2"
        And the user "GPTRANSFER_SOURCE_USER" creates filespace on host "GPTRANSFER_SOURCE_HOST" with gpdb port "GPTRANSFER_SOURCE_PORT" and config "gpfilespace_config_src" in "HOME" directory
        And the user "GPTRANSFER_DEST_USER" creates filespace on host "GPTRANSFER_DEST_HOST" with gpdb port "GPTRANSFER_DEST_PORT" and config "gpfilespace_config_dest" in "HOME" directory
        And the user runs workload under "test/behave/mgmt_utils/steps/data/gptransfer_setup" with connection "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -d gptest"
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And psql should return a return code of 0
        Then Verify data integrity of database "gptest" between source and destination system, work-dir "test/behave/mgmt_utils/steps/data/gptransfer_verify"
        And the user stops the gpfdist on host "GPTRANSFER_SOURCE_HOST" and port "2345" in work directory "HOME" from remote "2"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/teardown.sql -d template1"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/teardown.sql -d template1"

    @T339824
    Scenario: gptransfer full with all ddl, dml and filespace
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/teardown.sql -d template1"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/teardown.sql -d template1"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/setup.sql -d template1"
        And the user "GPTRANSFER_SOURCE_USER" creates filespace_config file for "fs" on host "GPTRANSFER_SOURCE_HOST" with gpdb port "GPTRANSFER_SOURCE_PORT" and config "gpfilespace_config_src" in "HOME" directory
        And the user "GPTRANSFER_DEST_USER" creates filespace_config file for "fs" on host "GPTRANSFER_DEST_HOST" with gpdb port "GPTRANSFER_DEST_PORT" and config "gpfilespace_config_dest" in "HOME" directory
        And the user "GPTRANSFER_SOURCE_USER" creates filespace on host "GPTRANSFER_SOURCE_HOST" with gpdb port "GPTRANSFER_SOURCE_PORT" and config "gpfilespace_config_src" in "HOME" directory
        And the user "GPTRANSFER_DEST_USER" creates filespace on host "GPTRANSFER_DEST_HOST" with gpdb port "GPTRANSFER_DEST_PORT" and config "gpfilespace_config_dest" in "HOME" directory
        And the user runs workload under "test/behave/mgmt_utils/steps/data/gptransfer/pre/ddl" with connection "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -d template1"
        And the user runs workload under "test/behave/mgmt_utils/steps/data/gptransfer/pre/dml" with connection "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -d template1"
        And the user runs workload under "test/behave/mgmt_utils/steps/data/gptransfer/pre/filespace" with connection "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -d template1"
        When the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And run post verifying workload under "test/behave/mgmt_utils/steps/data/gptransfer/post/ddl"
        And run post verifying workload under "test/behave/mgmt_utils/steps/data/gptransfer/post/dml"
        And the user runs "psql -d template1 -f test/behave/mgmt_utils/steps/data/gptransfer/pull_pg_stats.sql > test/behave/mgmt_utils/steps/data/gptransfer/pull_pg_stats.out"
        And run post verifying workload under "test/behave/mgmt_utils/steps/data/gptransfer/post/filespace"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/teardown.sql -d template1"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/teardown.sql -d template1"

    @T339821
    Scenario: gptransfer full in change tracking
        Given the gptransfer test is initialized
        And the user puts cluster on "GPTRANSFER_SOURCE_HOST" "GPTRANSFER_SOURCE_PORT" "GPTRANSFER_SOURCE_USER" in "ct"
        When the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And the user puts cluster on "GPTRANSFER_SOURCE_HOST" "GPTRANSFER_SOURCE_PORT" "GPTRANSFER_SOURCE_USER" in "sync"

    @T339821
    Scenario: gptransfer in resync
        Given the gptransfer test is initialized
        And the user puts cluster on "GPTRANSFER_SOURCE_HOST" "GPTRANSFER_SOURCE_PORT" "GPTRANSFER_SOURCE_USER" in "resync"
        When the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And the user puts cluster on "GPTRANSFER_SOURCE_HOST" "GPTRANSFER_SOURCE_PORT" "GPTRANSFER_SOURCE_USER" in "sync"

    @T439821
    Scenario: gptransfer filespace exists test with --full -t and -d options
        Given the gptransfer test is initialized
        And the user "GPTRANSFER_SOURCE_USER" creates filespace_config file for "fs" on host "GPTRANSFER_SOURCE_HOST" with gpdb port "GPTRANSFER_SOURCE_PORT" and config "gpfilespace_config_src" in "HOME" directory
        And the user "GPTRANSFER_SOURCE_USER" creates filespace on host "GPTRANSFER_SOURCE_HOST" with gpdb port "GPTRANSFER_SOURCE_PORT" and config "gpfilespace_config_src" in "HOME" directory
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/filespace_exists_test.sql -d template1"
        And psql should return a return code of 0
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE > /tmp/gptransfer_stdout.txt --batch-size=10"
        Then gptransfer should return a return code of 2
        And verify that the file "/tmp/gptransfer_stdout.txt" contains "Filespace 'fs' does not exist on destination. Please create the filespace to run gptransfer"
        And the user runs "gptransfer -d gptransfer_filespace_test --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE > /tmp/gptransfer_stdout.txt --batch-size=10"
        Then gptransfer should return a return code of 2
        And verify that the file "/tmp/gptransfer_stdout.txt" contains "Filespace 'fs' does not exist on destination. Please create the filespace to run gptransfer"
        And the user runs "gptransfer -t gptransfer_filespace_test.public.t2 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE > /tmp/gptransfer_stdout.txt --batch-size=10"
        Then gptransfer should return a return code of 2
        And verify that the file "/tmp/gptransfer_stdout.txt" contains "Filespace 'fs' does not exist on destination. Please create the filespace to run gptransfer"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/filespace_exists_test_teardown.sql -d template1"
        And psql should return a return code of 0

    @T339870
    Scenario: gptransfer with max-line-length of 16KB
        Given the gptransfer test is initialized
        And a table is created containing rows of length "16384" with connection "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -d gptransfer_testdb5"
        And the user runs "gptransfer -t gptransfer_testdb5.public.wide_row_16384 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "wide_row_16384" in "gptransfer_testdb5" has "10" rows
        And the temporary table file "wide_row_16384.sql" is removed
        And drop the table "wide_row_16384" with connection "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -d gptransfer_testdb5"

    Scenario: Negative test for gptransfer full with invalid delimiter and format option
        Given the gptransfer test is initialized
        And the user runs "gptransfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_SOURCE_USER --dest-port $GPTRANSFER_SOURCE_PORT --dest-host $GPTRANSFER_SOURCE_HOST --source-map-file $GPTRANSFER_MAP_FILE --delimiter ':' --format=csv --batch-size=10"
        Then gptransfer should return a return code of 2

    Scenario: test for new line character, double quote, single quote, comma, etc. for gptransfer in CSV format
        Given the gptransfer test is initialized
        And the database "gptransfer_test_db" does not exist
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/gptransfer_test_db_data.sql -d template1"
        Then psql should return a return code of 0
        And the user runs "gptransfer -t 'gptransfer_test_db.public.t1' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "t1" in database "gptransfer_test_db" of source system has same data with table "t1" in database "gptransfer_test_db" of destination system with order by "id"

    Scenario: Empty spaces in NOT NULL columns and NULL values are getting transferred correctly in CSV format
        Given the gptransfer test is initialized
        And the database "gptransfer_test_db_one" does not exist
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/gptransfer_test_db_one_data.sql -d template1"
        Then psql should return a return code of 0
        And the user runs "gptransfer -t 'gptransfer_test_db_one.public.table1' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "table1" in database "gptransfer_test_db_one" of source system has same data with table "table1" in database "gptransfer_test_db_one" of destination system with order by "id"
        And verify that the query "select count(*) from table1 where address is NULL" in database "gptransfer_test_db_one" returns "1"
        And verify that the query "select count(*) from table1 where address=''" in database "gptransfer_test_db_one" returns "1"

    Scenario: Empty spaces and NULL values are getting transferred correctly in TEXT format
        Given the gptransfer test is initialized
        And the database "gptransfer_test_db_one" does not exist
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/gptransfer_test_db_one_data.sql -d template1"
        Then psql should return a return code of 0
        And the user runs "gptransfer -t 'gptransfer_test_db_one.public.table1' --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --delimiter '\001' --format=text --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "table1" in database "gptransfer_test_db_one" of source system has same data with table "tabl1" in database "gptransfer_test_db_one" of destination system with order by "id"
        And verify that the query "select count(*) from table1 where address is NULL" in database "gptransfer_test_db_one" returns "1"
        And verify that the query "select count(*) from table1 where address=''" in database "gptransfer_test_db_one" returns "1"

    Scenario: Table count with tablefile -f option
       Given the gptransfer test is initialized
       And the database "gptransfer_testdb_table_count_one" does not exist
       And the database "gptransfer_testdb_table_count_two" does not exist
       And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/gptransfer_test_table_count_data.sql -d template1"
       Then psql should return a return code of 0
       And the user runs "gptransfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -f test/behave/mgmt_utils/steps/data/gptransfer/tablefiles.txt> /tmp/table_count_stdout.txt --batch-size=10"
       Then gptransfer should return a return code of 0
       And verify that the file "/tmp/table_count_stdout.txt" contains "Number of tables to transfer: 3"
       And verify that the file "/tmp/table_count_stdout.txt" contains "remaining 2 of 3 tables"
       And verify that the file "/tmp/table_count_stdout.txt" contains "remaining 1 of 3 tables"
       And verify that the file "/tmp/table_count_stdout.txt" contains "remaining 0 of 3 tables"

    Scenario: Gptransfer should not fail when schema already exists due to a failed run
       Given the gptransfer test is initialized
       And the database "gptransfer_test_db_failed_schema" does not exist
       And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/gptransfer_test_failed_schema.sql -d template1"
       Then psql should return a return code of 0
       And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/gptransfer_test_failed_schema_dest.sql -d template1"
       Then psql should return a return code of 0
       And the user runs "gptransfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -t gptransfer_test_db_failed_schema.public.t1> /tmp/gptransfer_stdout.txt --batch-size=10"
       Then gptransfer should return a return code of 0
       And verify that the file "/tmp/gptransfer_stdout.txt" contains "Removing existing gptransfer schema on source system"
       And verify that the file "/tmp/gptransfer_stdout.txt" contains "Removing existing gptransfer schema on destination system"

    @partition_transfer
    @prt_transfer_1
    Scenario: gptransfer leaf partition -> leaf partition basic transfer
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_1, gptest.public.employee_1_prt_1"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "public.employee_1_prt_1" in database "gptest" of source system has same data with table "public.employee_1_prt_1" in database "gptest" of destination system with options "-q"

    @partition_transfer
    @prt_transfer_2
    Scenario: gptransfer leaf partition -> root partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_other, gptest.public.employee"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Destination table gptest.public.employee is not a leaf partition table" to stdout

    @partition_transfer
    @prt_transfer_3
    Scenario: gptransfer leaf partition -> sub partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_prt_2.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_prt_2.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2_2_prt_2, gptest.public.sales_1_prt_2"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Destination table gptest.public.sales_1_prt_2 is not a leaf partition table" to stdout

    @partition_transfer
    @prt_transfer_4
    Scenario: gptransfer sub partition -> sub partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_prt_2.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_prt_2.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2, gptest.public.sales_1_prt_2"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Source table gptest.public.sales_1_prt_2 is not a leaf partition table" to stdout

    @partition_transfer
    @prt_transfer_5
    Scenario: gptransfer root partition -> leaf partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee, gptest.public.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Source table gptest.public.employee is not a leaf partition table" to stdout

    @partition_transfer
    @prt_transfer_6
    Scenario: gptransfer sub partition -> leaf partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_prt_2.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_prt_2.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2, gptest.public.sales_1_prt_2_2_prt_2"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Source table gptest.public.sales_1_prt_2 is not a leaf partition table" to stdout

    @partition_transfer
    @prt_transfer_7
    Scenario: gptransfer leaf partition -> non exist db
        Given the gptransfer test is initialized
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_prt_2.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2_2_prt_2, gptest.public.sales_1_prt_p1_2_prt_1"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Table gptest.public.sales_1_prt_p1_2_prt_1 does not exist in destination database when transferring from partition tables" to stdout

    @partition_transfer
    @prt_transfer_8
    Scenario: gptransfer leaf partition -> non exist schema
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_prt_2.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2_2_prt_2, gptest.nonexist_schema.sales_1_prt_p1_2_prt_1"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Table gptest.nonexist_schema.sales_1_prt_p1_2_prt_1 does not exist in destination database when transferring from partition tables" to stdout

    @partition_transfer
    @prt_transfer_9
    Scenario: gptransfer leaf partition -> non exist table
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_prt_2.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2_2_prt_2, gptest.public.nonexist_table"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Table gptest.public.nonexist_table does not exist in destination database when transferring from partition tables" to stdout

    @partition_transfer
    @prt_transfer_10
    Scenario: gptransfer non exist db -> leaf partition
        Given the gptransfer test is initialized
        And the database "nongptest" does not exist on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And database "gptest" exists
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "nongptest.public.sales_1_prt_p1_2_prt_1, gptest.public.employee_1_prt_other"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "FATAL:  database "nongptest" does not exist" to stdout

    @partition_transfer
    @prt_transfer_11
    Scenario: gptransfer non exist schema -> leaf partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.nonexist_schema.sales_1_prt_p1_2_prt_1, gptest.public.employee_1_prt_other"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Found no tables to transfer" to stdout

    @partition_transfer
    @prt_transfer_12
    Scenario: gptransfer non exist table -> leaf partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.nonexist_table, gptest.public.employee_1_prt_other"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Found no tables to transfer" to stdout

    @partition_transfer
    @prt_transfer_13
    Scenario: gptransfer list partition -> range partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_range_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_boys, gptest.public.employee_1_prt_1"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Partition type is different" to stdout

    @partition_transfer
    @prt_transfer_14
    Scenario: gptransfer one level leaf partition -> zero level leaf partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_prt_2.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_range_prt_2.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2_2_prt_2, gptest.public.sales_1_prt_2"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Max level of partition is not same" to stdout

    @partition_transfer
    @prt_transfer_15
    Scenario: gptransfer leaf partition, more columns table -> less columns table
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_3.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_boys, gptest.public.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "has different column layout or types" to stdout

    @partition_transfer
    @prt_transfer_16
    Scenario: gptransfer leaf partition, same partition type using different column as partition key
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_2.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_4.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_main, gptest.public.employee_1_prt_main"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Partition column attributes are different" to stdout

    @partition_transfer
    @prt_transfer_17
    Scenario: gptransfer same leaf partition with different parent partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_list_prt_6.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2_2_prt_asia, gptest.public.sales_1_prt_2_2_prt_asia"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Partition column attributes are different" to stdout

    @partition_transfer
    @prt_transfer_18
    Scenario: gptransfer same leaf partition column key, with different partition value
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_5.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_girls, gptest.public.employee_1_prt_girls"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "partition value is different" to stdout

    @partition_transfer
    @prt_transfer_19
    Scenario: gptransfer same leaf partition column key, with different parent partition value
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_list_prt_6.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2_2_prt_asia, gptest.public.sales_1_prt_2_2_prt_asia"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Partition column attributes are different" to stdout

    @partition_transfer
    @prt_transfer_20
    Scenario: gptransfer leaf partition, with different table and column name
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/create_one_level_range_prt_3.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_e_employee.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/create_one_level_range_prt_3.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_1, gptest.public.e_employee_1_prt_1 | gptest.public.e_employee_1_prt_1, gptest.public.employee_1_prt_1"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "public.employee_1_prt_1" in database "gptest" of source system has same data with table "public.e_employee_1_prt_1" in database "gptest" of destination system with options "-t"
        And verify that table "public.e_employee_1_prt_1" in database "gptest" of source system has same data with table "public.employee_1_prt_1" in database "gptest" of destination system with options "-t"

    @partition_transfer
    @prt_transfer_21
    Scenario: gptransfer normal heap table to a leaf partition
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/create_heap_table.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.heap_employee, gptest.public.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Source table gptest.public.heap_employee is not a leaf partition table" to stdout

    @partition_transfer
    @prt_transfer_22
    Scenario: gptransfer ao partition table to co partition table
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/create_one_level_row_oriented.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/create_one_level_column_oriented.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_other, gptest.public.employee_1_prt_other"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "public.employee_1_prt_other" in database "gptest" of source system has same data with table "public.employee_1_prt_other" in database "gptest" of destination system with options "-t"

    @partition_transfer
    @prt_transfer_23
    Scenario: gptransfer with --truncate option
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_boys, gptest.public.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --truncate --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "public.employee_1_prt_boys" in database "gptest" of source system has same data with table "public.employee_1_prt_boys" in database "gptest" of destination system with options "-t"

    @partition_transfer
    @prt_transfer_24
    Scenario: gptransfer with --skip-existing option
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_boys, gptest.public.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --skip-existing --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "public.employee_1_prt_boys" in "gptest" has "1" rows

    @partition_transfer
    @prt_transfer_25
    Scenario: gptransfer, multi columns partition key to single column partition key
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/create_multi_column_partition_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_boys, gptest.public.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Number of partition columns is different" to stdout

    @partition_transfer
    @prt_transfer_26
    Scenario: gptransfer, with multi columns partition key reversed between source and destination
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/create_multi_column_partition_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/create_multi_column_partition_2.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_boys, gptest.public.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0

    @partition_transfer
    @prt_transfer_27
    Scenario: gptransfer, --partition-transfer can not be used with --full option
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl, gptest.public.tbl"
        When the user runs "gptransfer -f input_file --partition-transfer --full --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "--partition-transfer option cannot be used with --full option" to stdout

    @partition_transfer
    @prt_transfer_28
    Scenario: gptransfer, --partition-transfer can not be used with -d option
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl, gptest.public.tbl"
        When the user runs "gptransfer -f input_file --partition-transfer -d gptest --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "--partition-transfer option cannot be used with -d option" to stdout

    @partition_transfer
    @prt_transfer_29
    Scenario: gptransfer, --partition-transfer can not be used with --drop option
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl, gptest.public.tbl"
        When the user runs "gptransfer -f input_file --partition-transfer --drop --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "--partition-transfer option cannot be used with --drop option" to stdout

    @partition_transfer
    @prt_transfer_30
    Scenario: gptransfer, --partition-transfer can not be used with -t option
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl, gptest.public.tbl"
        When the user runs "gptransfer -f input_file --partition-transfer -t gptest.public.tbl --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "--partition-transfer option cannot be used with -t option" to stdout

    @partition_transfer
    @prt_transfer_31
    Scenario: gptransfer, --partition-transfer can not be used with --schema-only option
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl, gptest.public.tbl"
        When the user runs "gptransfer -f input_file --partition-transfer --schema-only --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "--partition-transfer option cannot be used with --schema-only option" to stdout

    @partition_transfer
    @prt_transfer_32
    Scenario: gptransfer, --partition-transfer can not be used with -T option
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl, gptest.public.tbl"
        When the user runs "gptransfer -f input_file --partition-transfer -T gptest.public.tbl --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "--partition-transfer option cannot be used with any exclude table option" to stdout

    @partition_transfer
    @prt_transfer_33
    Scenario: gptransfer, --partition-transfer can not be used with -F option
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl, gptest.public.tbl"
        When the user runs "gptransfer -f input_file --partition-transfer -F input_file --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "--partition-transfer option cannot be used with any exclude table option" to stdout

    @partition_transfer
    @prt_transfer_34
    Scenario: gptransfer with public schema to non public schema
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1_with_namespace.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_boys, gptest.schema1.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "schema1.employee_1_prt_boys" in "gptest" has "1" rows

    @partition_transfer
    @prt_transfer_35
    Scenario: gptransfer with non public schema to public schema
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1_with_namespace.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee_with_namespace.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.schema1.employee_1_prt_boys, gptest.public.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "public.employee_1_prt_boys" in "gptest" has "1" rows

    @partition_transfer
    @prt_transfer_36
    Scenario: gptransfer gives warnings on duplicated entries
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1_with_namespace.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee_with_namespace.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.schema1.employee_1_prt_boys, gptest.public.employee_1_prt_boys|gptest.schema1.employee_1_prt_boys, gptest.public.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Duplicate entries found" to stdout
        And verify that table "public.employee_1_prt_boys" in "gptest" has "1" rows

    @partition_transfer
    @prt_transfer_37
    Scenario: gptransfer won't allow transferring between same partition table
        Given the gptransfer test is initialized
        And database "gptest" exists
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_boys, gptest.public.employee_1_prt_boys"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_DEST_PORT --source-host $GPTRANSFER_DEST_HOST --source-user $GPTRANSFER_DEST_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Cannot transfer between same partition table" to stdout

    @partition_transfer
    @prt_transfer_38
    Scenario: gptransfer won't restrict if one pair is having any extra partition tables
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_list_prt_3.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_sales.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_list_prt_7.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_asia_2_prt_2, gptest.public.sales_1_prt_asia_2_prt_2"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "public.sales_1_prt_asia_2_prt_2" in "gptest" has "1" rows

    @partition_transfer
    @prt_transfer_39
    Scenario: gptransfer, --partition-transfer can not be used with --dest-database option
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl, gptest.public.tbl1"
        When the user runs "gptransfer -f input_file --partition-transfer --dest-database gptest --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "--partition-transfer option cannot be used with --dest-database option" to stdout

    @partition_transfer
    @prt_transfer_40
    Scenario: gptransfer, --partition-transfer ingores blank lines in input file
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables " "
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --verbose --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Skipping blank lines" to stdout

    @partition_transfer
    @prt_transfer_41
    Scenario: gptransfer, --partition-transfer pre-checks source and destination pair format
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl ,, gptest.public.tbl1"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Wrong format found for table transfer pair" to stdout

    @partition_transfer
    @prt_transfer_42
    Scenario: gptransfer, --partition-transfer pre-checks empty source table name
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables ", gptest.public.tbl1"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Cannot specify empty source table name" to stdout

    @partition_transfer
    @prt_transfer_43
    Scenario: gptransfer, --partition-transfer pre-checks empty destination table name
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl,"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Cannot specify empty destination table name" to stdout

    @partition_transfer
    @prt_transfer_44
    Scenario: gptransfer, --partition-transfer pre-checks full qualified format of source table name
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl.extra_tablename, gptest.public.tbl"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Source table name "gptest.public.tbl.extra_tablename" isn't fully qualified format" to stdout

    @partition_transfer
    @prt_transfer_45
    Scenario: gptransfer, --partition-transfer pre-checks full qualified format of destination table name
        Given the gptransfer test is initialized
        And there is a file "input_file" with tables "gptest.public.tbl, gptest.public.tbl.extra_tablename"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE -a --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Destination table name "gptest.public.tbl.extra_tablename" isn't fully qualified format" to stdout

    @partition_transfer
    @prt_transfer_46
    Scenario: gptransfer partition with batch size 1, tables transferred in sequential order
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_list_prt_1.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2_2_prt_asia, gptest.public.sales_1_prt_2_2_prt_asia|gptest.public.sales_1_prt_3_2_prt_asia, gptest.public.sales_1_prt_3_2_prt_asia|gptest.public.sales_1_prt_2_2_prt_other_regions, gptest.public.sales_1_prt_2_2_prt_other_regions"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=1"
        Then gptransfer should return a return code of 0
        And verify that gptransfer is in order of "input_file" when partition transfer is "True"

    @partition_transfer
    @prt_transfer_47
    Scenario: gptransfer leaf partition -> leaf partition drop and readd column before transfer
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/two_level_range_list_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/drop_and_readd_column.sql -d gptest"
		And the user runs "pg_dump -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -t public.sales -s gptest  > /tmp/mytab.sql"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -c "DROP TABLE IF EXISTS sales" -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f /tmp/mytab.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_sales_columns_reordered.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.sales_1_prt_2_2_prt_asia, gptest.public.sales_1_prt_2_2_prt_asia"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And verify that table "public.sales_1_prt_2_2_prt_asia" in database "gptest" of source system has same data with table "public.sales_1_prt_2_2_prt_asia" in database "gptest" of destination system with options "-q"

    @partition_transfer
    @prt_transfer_48
    Scenario: gptransfer leaf partition -> leaf partition different column order
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_range_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_range_prt_1_different_col_order.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_1, gptest.public.employee_1_prt_1"
        When the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Source partition table gptest.public.employee_1_prt_1 has different column layout or types from destination table gptest.public.employee_1_prt_1" to stdout
        Then the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_range_prt_1_different_prt_column.sql -d gptest"
        And the user runs "gptransfer -f input_file --partition-transfer --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 2
        And gptransfer should print "Partition column attributes are different at level" to stdout

    @distribution_key
    Scenario: gptransfer is run with distribution key that has upper case characters
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/distribution_test.sql -d gptest"
        And the user runs "gptransfer -t gptest.public.caps -t gptest.public.caps_with_dquote -t gptest.public.caps_with_dquote2 -t gptest.public.caps_with_dquote3 -t gptest.public.caps_with_dquote4 -t gptest.public.caps_with_squote -t gptest.public.randomkey --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --truncate -a --batch-size=10"
        Then gptransfer should return a return code of 0

    @distribution_key
    Scenario: gptransfer is run with multiple distribution keys
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/distribution_multiple_keys.sql -d gptest"
        And the user runs "gptransfer -t gptest.public.table_distribution -t gptest.public.reverse_table_distribution --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --truncate -a --batch-size=10"
        Then gptransfer should return a return code of 0
        And the user runs "psql gptest -c '\d+ table_distribution'"
        Then psql should print "Distributed by: (id1," , id2 crazy"')" to stdout 1 times
        And the user runs "psql gptest -c '\d+ reverse_table_distribution'"
        Then psql should print "Distributed by: (id2 crazy"', id1," )" to stdout 1 times

    @gptransfer_help
    Scenario: use gptransfer --help with another gptransfer process already running.
        Given the gptransfer test is initialized
        And a table is created containing rows of length "10" with connection "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -d gptransfer_testdb5"
        When the user runs the command "sleep 9; gptransfer -t gptransfer_testdb5.public.wide_row_10 --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --truncate -a" in the background
        And the user runs "gptransfer --help && [ $(ps ux | grep `which gptransfer` | grep -v grep | wc -l) != 0 ]"
        Then gptransfer should return a return code of 0
        And gptransfer should not print "An instance of gptransfer is already running" to stdout
        And the user waits for "gptransfer" to finish running

    @partition-transfer-non-partition-target
    @partition_transfer
    @prt_transfer_49
    Scenario: transfer multiple partition leaves to same non-partition table
        Given the gptransfer test is initialized
        And database "gptest" exists
        And database "gptest" is created if not exists on host "GPTRANSFER_SOURCE_HOST" with port "GPTRANSFER_SOURCE_PORT" with user "GPTRANSFER_SOURCE_USER"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/one_level_range_prt_1.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer/insert_into_employee.sql -d gptest"
        And the user runs "psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -f test/behave/mgmt_utils/steps/data/gptransfer/create_heap_table.sql -d gptest"
        And there is a file "input_file" with tables "gptest.public.employee_1_prt_1, gptest.public.heap_employee | gptest.public.employee_1_prt_2, gptest.public.heap_employee"
        When the user runs "gptransfer -f input_file --partition-transfer-non-partition-target --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Validation of gptest.public.heap_employee successful" to stdout
        # running gptransfer again with pre-existing data in the destination table to make sure validation is successful
        When the user runs "gptransfer -f input_file --partition-transfer-non-partition-target --source-port $GPTRANSFER_SOURCE_PORT --source-host $GPTRANSFER_SOURCE_HOST --source-user $GPTRANSFER_SOURCE_USER --dest-user $GPTRANSFER_DEST_USER --dest-port $GPTRANSFER_DEST_PORT --dest-host $GPTRANSFER_DEST_HOST --source-map-file $GPTRANSFER_MAP_FILE --batch-size=10"
        Then gptransfer should return a return code of 0
        And gptransfer should print "Validation of gptest.public.heap_employee successful" to stdout

    Scenario: gptransfer cleanup
        Given the gptransfer test is initialized
        And the user runs "psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -f test/behave/mgmt_utils/steps/data/gptransfer_cleanup.sql -d template1"
        Then psql should return a return code of 0
