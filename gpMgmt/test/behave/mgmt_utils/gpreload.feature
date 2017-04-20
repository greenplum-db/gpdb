@gpreload
Feature: gpreload feature to reload data based on columns to sort

    Scenario: gpreload with quicklz compression
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "ao" partition table "ao_part_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "co" table "co_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "co" partition table "co_part_table_comp" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And all the compression data from "testdb" is saved for verification
        When the user runs "gpreload -t test/behave/mgmt_utils/steps/data/gpreload_table_file1 -d testdb"
        Then gpreload should return a return code of 0
        And verify that the compression ratio of "public.ao_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.ao_part_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.co_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.co_part_table_comp" in "testdb" is good
        And verify that the data of tables in "testdb" is validated after reload

    Scenario: gpreload with zlib compression
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_table_comp" with compression "zlib" in "testdb" with data
        And there is a "ao" partition table "ao_part_table_comp" with compression "zlib" in "testdb" with data
        And there is a "co" table "co_table_comp" with compression "zlib" in "testdb" with data
        And there is a "co" partition table "co_part_table_comp" with compression "zlib" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And all the compression data from "testdb" is saved for verification
        When the user runs "gpreload -t test/behave/mgmt_utils/steps/data/gpreload_table_file1 -d testdb"
        Then gpreload should return a return code of 0
        And verify that the compression ratio of "public.ao_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.ao_part_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.co_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.co_part_table_comp" in "testdb" is good
        And verify that the data of tables in "testdb" is validated after reload

    Scenario: gpreload with no compression
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_table" with compression "None" in "testdb" with data
        And there is a "ao" partition table "ao_part_table" with compression "None" in "testdb" with data
        And there is a "co" table "co_table" with compression "None" in "testdb" with data
        And there is a "co" partition table "co_part_table" with compression "None" in "testdb" with data
        And there is a "heap" table "heap_table" with compression "None" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And all the compression data from "testdb" is saved for verification
        When the user runs "gpreload -t test/behave/mgmt_utils/steps/data/gpreload_table_file2 -d testdb"
        Then gpreload should return a return code of 0
        And verify that the compression ratio of "public.ao_table" in "testdb" is good
        And verify that the compression ratio of "public.ao_part_table" in "testdb" is good
        And verify that the compression ratio of "public.co_table" in "testdb" is good
        And verify that the compression ratio of "public.co_part_table" in "testdb" is good
        And verify that the data of tables in "testdb" is validated after reload

    Scenario: gpreload with quicklz compression with indexes
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_index_table" with index "ao_index" compression "quicklz" in "testdb" with data
        And there is a "co" table "co_index_table" with index "co_index" compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And all the compression data from "testdb" is saved for verification
        When the user runs "gpreload -t test/behave/mgmt_utils/steps/data/gpreload_table_file_index -d testdb -a"
        Then gpreload should return a return code of 0
        And gpreload should not print "Table public.ao_table_index has indexes. This might slow down table reload. Do you still want to continue ?" to stdout
        And gpreload should not print "Table public.co_table_index has indexes. This might slow down table reload. Do you still want to continue ?" to stdout
        And verify that the compression ratio of "public.ao_index_table" in "testdb" is good
        And verify that the compression ratio of "public.co_index_table" in "testdb" is good
        And verify that the data of tables in "testdb" is validated after reload

    Scenario: gpreload with invalid table files
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "ao" partition table "ao_part_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "co" table "co_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "co" partition table "co_part_table_comp" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And all the compression data from "testdb" is saved for verification
        When the user runs "gpreload -t test/behave/mgmt_utils/steps/data/gpreload_table_file_index -d testdb"
        Then gpreload should return a return code of 2
        And gpreload should print "Table public.ao_index_table does not exist" to stdout
        When the user runs "gpreload -t test/behave/mgmt_utils/steps/data/gpreload_table_file_invalid_columns -d testdb"
        Then gpreload should return a return code of 2
        And gpreload should print "Table public.ao_table_comp does not have column column10" to stdout
        When the user runs "gpreload -t test/behave/mgmt_utils/steps/data/gpreload_table_file_invalid_sort_order -d testdb"
        Then gpreload should return a return code of 2
        And gpreload should print "Line .* is not formatted correctly: Invalid sort order foo" to stdout
        When the user runs "gpreload -t test/behave/mgmt_utils/steps/data/gpreload_table_file_invalid -d testdb"
        Then gpreload should return a return code of 2
        And gpreload should print "Line .* is not formatted correctly: Empty column" to stdout
        And verify that the compression ratio of "public.ao_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.ao_part_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.co_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.co_part_table_comp" in "testdb" is good
        And verify that the data of tables in "testdb" is validated after reload

    Scenario: gpreload with quicklz compression on leaf partitions
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "ao" partition table "ao_part_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "co" table "co_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "co" partition table "co_part_table_comp" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And all the compression data from "testdb" is saved for verification
        When the user runs "gpreload -t test/behave/mgmt_utils/steps/data/gpreload_table_file_leaf_partitions -d testdb"
        Then gpreload should return a return code of 0
        And verify that the compression ratio of "public.ao_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.ao_part_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.co_table_comp" in "testdb" is good
        And verify that the compression ratio of "public.co_part_table_comp" in "testdb" is good
        And verify that the data of tables in "testdb" is validated after reload

    Scenario: gpreload with quicklz compression on mid level partitions
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "ao" table "ao_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "ao" partition table "ao_part_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "co" table "co_table_comp" with compression "quicklz" in "testdb" with data
        And there is a "co" partition table "co_part_table_comp" with compression "quicklz" in "testdb" with data
        And all the data from "testdb" is saved for verification
        And all the compression data from "testdb" is saved for verification
        When the user runs "gpreload -t test/behave/mgmt_utils/steps/data/gpreload_table_file_mid_partitions -d testdb"
        Then gpreload should return a return code of 2
        And gpreload should print "Please specify only leaf partitions or parent table name" to stdout
