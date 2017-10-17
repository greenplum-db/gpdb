@analyzedb
Feature: Incrementally analyze the database

    @analyzedb_UI
    Scenario: Invalid arguments entered
        When the user runs "analyzedb -w"
        Then analyzedb should print "error: no such option" error message
        When the user runs "analyzedb -d incr_analyze -w"
        Then analyzedb should print "error: no such option" error message

    #    @analyzedb_UI
    #    Scenario: Duplicate options
    #      When the user runs "analyzedb -d incr_analyze -d incr_analyze_2"
    #      Then analyzedb should print "error: duplicate options" error message

    @analyzedb_UI
    Scenario: Missing required options
        When the user runs "analyzedb"
        Then analyzedb should print "option -d required" to stdout
        When the user runs "analyzedb -l -d incr_analyze -i x"
        Then analyzedb should print "option -i or -x can only be used together with -t" to stdout
        When the user runs "analyzedb -l -d incr_analyze -x x"
        Then analyzedb should print "option -i or -x can only be used together with -t" to stdout

    @analyzedb_UI
    Scenario: Missing parameters
        When the user runs "analyzedb -d"
        Then analyzedb should print "error: -d option requires an argument" error message
        When the user runs "analyzedb -d incr_analyze -t"
        Then analyzedb should print "error: -t option requires an argument" error message
        When the user runs "analyzedb -d incr_analyze -s"
        Then analyzedb should print "error: -s option requires an argument" error message
        When the user runs "analyzedb -d incr_analyze -t public.t1_ao -i"
        Then analyzedb should print "error: -i option requires an argument" error message
        When the user runs "analyzedb -d incr_analyze -t public.t1_ao -x"
        Then analyzedb should print "error: -x option requires an argument" error message

    @analyzedb_UI
    Scenario: Additional ignored arguments
        When the user runs "analyzedb -l -d incr_analyze xyz"
        Then analyzedb should print "\[WARNING]:-Please note that some of the arguments \(\['xyz']\) aren't valid and will be ignored" to stdout

    @analyzedb_UI
    Scenario: Mutually exclusive arguments
        When the user runs "analyzedb -l -d incr_analyze -t public.t1_ao -i x -x y"
        Then analyzedb should print "options -i and -x are mutually exclusive" to stdout

    @analyzedb_UI
    Scenario: Table name not qualified with schema name
        When the user runs "analyzedb -a -d incr_analyze -t t1_ao"
        Then analyzedb should print "No schema name supplied for table" to stdout
        When the user runs "analyzedb -l -d incr_analyze -t public"
        Then analyzedb should print "No schema name supplied for table" to stdout
        When the user runs command "printf 't1_ao' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then analyzedb should print "No schema name supplied for table" to stdout

    @analyzedb_UI
    Scenario: Input is a view rather than a table
        Given a view "v1" exists on table "t1_ao" in schema "public"
        When the user runs "analyzedb -l -d incr_analyze -t public.v1"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        When the user runs command "printf 'public.v1' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout

    @analyzedb_UI
    Scenario: Database object does not exist, command line
        When the user runs "analyzedb -l -d ghost_db"
        Then analyzedb should print "database "ghost_db" does not exist" to stdout
        When the user runs "analyzedb -l -d incr_analyze -s public.t1_ao"
        Then analyzedb should print "Schema public.t1_ao does not exist" to stdout
        When the user runs "analyzedb -l -d incr_analyze -t public.t1_xyz"
        Then analyzedb should print "relation "public.t1_xyz" does not exist" to stdout
        When the user runs "analyzedb -l -d incr_analyze -t public.t1_ao -i r"
        Then analyzedb should print "Invalid input columns for table public.t1_ao" to stdout
        When the user runs "analyzedb -l -d incr_analyze -t public.t1_ao -x r"
        Then analyzedb should print "Invalid input columns for table public.t1_ao" to stdout

    @analyzedb_UI
    Scenario: Database object does not exist, config file
        When the user runs command "printf 'public.t1_ao' > config_file"
        And the user runs "analyzedb -l -d ghost_db -f config_file"
        Then analyzedb should print "database "ghost_db" does not exist" to stdout
        When the user runs command "printf 'public.t1_xyz' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then analyzedb should print "relation "public.t1_xyz" does not exist" to stdout
        When the user runs command "printf 'public.t1_ao -i r' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then analyzedb should print "Invalid input columns for table public.t1_ao" to stdout
        When the user runs command "printf 'public.t1_ao -x r' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then analyzedb should print "Invalid input columns for table public.t1_ao" to stdout

    @analyzedb_UI
    Scenario: Missing or empty config file
        When the user runs "analyzedb -l -d incr_analyze -f ghost_config"
        Then analyzedb should print "No such file or directory: 'ghost_config'" to stdout
        When the user runs command "printf '' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout

    @analyzedb_UI
    Scenario: Duplicate/inconsistent lines in config file
        When the user runs command "printf 'public.t1_ao\npublic.t1_ao' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then analyzedb should print "analyzedb error: Duplicate table name" to stdout
        When the user runs command "printf 'public.t1_ao -i x\npublic.t1_ao -x x' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then analyzedb should print "analyzedb error: Duplicate table name" to stdout
        When the user runs command "printf 'public.t1_ao -i x\npublic.t1_ao -x y' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then analyzedb should print "analyzedb error: Duplicate table name" to stdout

    @analyzedb_UI
    Scenario: Show help
        Given no state files exist for database "incr_analyze"
        When the user runs "analyzedb -?"
        Then analyzedb should print "Analyze a database" to stdout
        And analyzedb should print "Options" to stdout
        When the user runs "analyzedb -h"
        Then analyzedb should print "Analyze a database" to stdout
        And analyzedb should print "Options" to stdout
        When the user runs "analyzedb --help"
        Then analyzedb should print "Analyze a database" to stdout
        And analyzedb should print "Options" to stdout

    @analyzedb_UI
    Scenario: Valid inputs
        Given no state files exist for database "incr_analyze"
        When the user runs "analyzedb -l -d incr_analyze"
        Then analyzedb should print "-public.t1_ao" to stdout
        And analyzedb should print "-public.t2_heap" to stdout
        And analyzedb should print "-public.t3_ao" to stdout
        When the user runs "analyzedb -l -d incr_analyze -s public"
        Then analyzedb should print "-public.t1_ao" to stdout
        And analyzedb should print "-public.t2_heap" to stdout
        And analyzedb should print "-public.t3_ao" to stdout
        When the user runs "analyzedb -l -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "-public.t1_ao" to stdout
        When the user runs "analyzedb -l -d incr_analyze -t public.t1_ao -i x"
        Then analyzedb should print "-public.t1_ao\(x\)" to stdout
        When the user runs "analyzedb -l -d incr_analyze -t public.t1_ao -x y"
        Then analyzedb should print "-public.t1_ao\(x,z\)" to stdout
        When the user runs command "printf 'public.t1_ao' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao" to stdout
        When the user runs command "printf 'public.t1_ao -x y,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(x\)" and "-public.t3_ao"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i b' > config_file"
        And the user runs "analyzedb -l -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(b\)"

    @analyzedb_UI
    Scenario: Mixed case inputs
        Given no state files exist for database "incr_analyze"
        And schema ""MySchema"" exists in "incr_analyze"
        And there is a regular "ao" table ""My_ao"" with column name list ""y","Y",z" and column type list "int,text,real" in schema ""MySchema""
        And there is a regular "heap" table ""T2_heap_UPPERCASE"" with column name list "x,y,z" and column type list "int,text,real" in schema "public"
        When the user runs "analyzedb -l -d incr_analyze -s MySchema"
        Then analyzedb should print "-"MySchema"."My_ao" to stdout
        When the user runs "analyzedb -l -d incr_analyze -t \"MySchema\".\"My_ao\""
        Then analyzedb should print "-"MySchema"."My_ao" to stdout
        When the user runs command "printf '\"MySchema\".\"My_ao\" -x Y,z\npublic.\"T2_heap_UPPERCASE\"' > config_file"
        And the user runs "analyzedb -d incr_analyze -f config_file"
        Then analyzedb should print "-public."T2_heap_UPPERCASE" to stdout
        And analyzedb should print "-"MySchema"."My_ao"\(y\)" to stdout
        When the user runs "analyzedb -l -d incr_analyze -s public"
        Then analyzedb should print "-public.\"T2_heap_UPPERCASE\"" to stdout

    @analyzedb_UI
    Scenario: Table and schema name with a space
        Given no state files exist for database "incr_analyze"
        And schema ""my schema"" exists in "incr_analyze"
        And there is a regular "ao" table ""my ao"" with column name list ""my col","My Col",z" and column type list "int,text,real" in schema ""my schema""
        And there is a regular "heap" table ""my heap"" with column name list ""my col","My Col",z" and column type list "int,text,real" in schema "public"
        When the user runs "analyzedb -l -d incr_analyze -s 'my schema'"
        Then analyzedb should print "-"my schema"."my ao" to stdout
        When the user runs "analyzedb -l -d incr_analyze -t '"my schema"."my ao"'"
        Then analyzedb should print "-"my schema"."my ao" to stdout


    @analyzedb_core @analyzedb_single_table
    Scenario: Incremental analyze, no dirty tables
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        When the user runs "analyzedb -a -l -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And "public.t1_ao" should appear in the latest state files

    @analyzedb_core @analyzedb_single_table
    Scenario: Incremental analyze, dirty table
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        When some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And the user runs "analyzedb -a -l -d incr_analyze -t public.t1_ao"
        # when running analyzedb, the analyze target will be printed with a prefix dash
        Then analyzedb should print "-public.t1_ao" to stdout
        And "public.t1_ao" should appear in the latest state files

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, dml, no entry in state file, whole table requested
        Given table "public.t1_ao" does not appear in the latest state files
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "-public.t1_ao" to stdout
        And "public.t1_ao" should appear in the latest state files

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, ddl, no entry in state file, whole table requested
        Given no state files exist for database "incr_analyze"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "-public.t1_ao" to stdout
        And "public.t1_ao" should appear in the latest state files

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, dml, ddl, no entry in state file, whole table requested
        Given no state files exist for database "incr_analyze"
        And some ddl is performed on table "t1_ao" in schema "public"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "-public.t1_ao" to stdout
        And "public.t1_ao" should appear in the latest state files

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, dml, no entry in state file, some columns requested
        Given no state files exist for database "incr_analyze"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x"
        Then analyzedb should print "-public.t1_ao\(x\)" to stdout
        And "public.t1_ao" should appear in the latest state files
        And columns "x" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, ddl, no entry in state file, some columns requested
        Given no state files exist for database "incr_analyze"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        Then output should contain either "-public.t1_ao\(x,y\)" or "-public.t1_ao\(y,x\)"
        And "public.t1_ao" should appear in the latest state files
        And columns "x,y" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, dml, ddl, no entry in state file, some columns requested
        Given no state files exist for database "incr_analyze"
        And some ddl is performed on table "t1_ao" in schema "public"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,z"
        Then output should contain either "-public.t1_ao\(z,x\)" or "-public.t1_ao\(x,z\)"
        And "public.t1_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for all columns, no change, some columns requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        And "public.t1_ao" appears in the latest state files
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for all columns, dml, some columns requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        And "public.t1_ao" appears in the latest state files
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        Then output should contain either "-public.t1_ao\(y,x\)" or "-public.t1_ao\(x,y\)"
        And columns "x,y" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for all columns, ddl, whole table requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "-public.t1_ao" to stdout
        And "public.t1_ao" should appear in the latest state files

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for all columns, ddl, some columns requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        And "public.t1_ao" appears in the latest state files
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,z"
        Then output should contain either "-public.t1_ao\(z,x\)" or "-public.t1_ao\(x,z\)"
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for all columns, ddl, dml, whole table requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        And some ddl is performed on table "t1_ao" in schema "public"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "-public.t1_ao" to stdout
        And "public.t1_ao" should appear in the latest state files

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for all columns, ddl, dml, some columns requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        And "public.t1_ao" appears in the latest state files
        And some ddl is performed on table "t1_ao" in schema "public"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,z"
        Then output should contain either "-public.t1_ao\(z,x\)" or "-public.t1_ao\(x,z\)"
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for some columns, no change, whole table requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "-public.t1_ao\(z\)" to stdout
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for some columns, no change, some columns requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,z"
        Then analyzedb should print "-public.t1_ao\(z\)" to stdout
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for some columns, dml, whole table requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "-public.t1_ao" to stdout
        And "public.t1_ao" should appear in the latest state files

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for some columns, dml, some columns requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,z"
        Then output should contain either "-public.t1_ao\(z,x\)" or "-public.t1_ao\(x,z\)"
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for some columns, ddl, whole table requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "-public.t1_ao" to stdout
        And "public.t1_ao" should appear in the latest state files

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for some columns, ddl, some columns requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,z"
        Then output should contain either "-public.t1_ao\(z,x\)" or "-public.t1_ao\(x,z\)"
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for some columns, ddl, dml, whole table requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        And some ddl is performed on table "t1_ao" in schema "public"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao"
        Then analyzedb should print "-public.t1_ao" to stdout
        And "public.t1_ao" should appear in the latest state files

    @analyzedb_core @analyzedb_single_table
    Scenario: Single table, entry exists for some columns, ddl, dml, some columns requested
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,y"
        And some ddl is performed on table "t1_ao" in schema "public"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        When the user runs "analyzedb -a -d incr_analyze -t public.t1_ao -i x,z"
        Then output should contain either "-public.t1_ao\(z,x\)" or "-public.t1_ao\(x,z\)"
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file


    ### (no entry, no entry)

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, no entry), (no change, no change), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i b,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(b,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "b,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, no entry), (no change, no change), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, no entry), (no change, no change), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i b,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(x,z\)" and "-public.t3_ao\(b,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "b,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, no entry), (no change, DML), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i b,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(b,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "b,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, no entry), (no change, DDL), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, no entry), (DML, DDL), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i b,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(x,z\)" and "-public.t3_ao\(b,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "b,c" of table "public.t3_ao" should appear in the latest column state file

    ### (no entry, some cols)

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (no change, no change), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i b,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(b\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "b,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (no change, no change), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(a,b\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,b,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (no change, no change), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i a,b,c"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i b,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao\(x,z\)" to stdout
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,b,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (no change, DML), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (no change, DML), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (no change, DML), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (no change, DML&DDL), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (no change, DML&DDL), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (no change, DML&DDL), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(x,z\)" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (DML, DDL), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (DML, DDL), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And output should not contain "-public.t3_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, some cols), (DML, DDL), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao -i c"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file

    ### (no entry, whole table)

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (no change, no change), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i b,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (no change, no change), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (no change, no change), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i b,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao\(x,z\)" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (no change, DML), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (no change, DML), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (no change, DML), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (DML&DDL, no change), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (DML&DDL, no change), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (DML&DDL, no change), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (DML, DDL), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (DML, DDL), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (no entry, whole table), (DML, DDL), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.t3_ao"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(x,z\)" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file


    ### (some cols, whole table)


    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (no change, no change), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i b,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao\(y\)" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (no change, no change), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao\(y\)" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (no change, no change), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        When the user runs command "printf 'public.t1_ao -i x,y\npublic.t3_ao -i b,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao\(y\)" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (no change, DML), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(y\)" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (no change, DML), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(y\)" and "-public.t3_ao"
        And output should not contain "-public.t3_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (no change, DML), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao -i y,z\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(y\)" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (DML&DDL, no change), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao" to stdout
        And output should not contain "public.t1_ao\("
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (DML&DDL, no change), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao" to stdout
        And output should not contain "public.t1_ao\("
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (DML&DDL, no change), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao\(x,z\)" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And column "y" of table "public.t1_ao" should not appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (DML, DDL), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(a,c\)"
        And output should not contain "public.t1_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (DML, DDL), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And output should not contain "public.t1_ao\("
        And output should not contain "public.t3_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, whole table), (DML, DDL), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(x,z\)" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    ### (whole table, whole table)

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (no change, no change), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i b,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (no change, no change), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (no change, no change), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        When the user runs command "printf 'public.t1_ao -i x,y\npublic.t3_ao -i b,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (no change, DML), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t3_ao\(a,c\)" to stdout
        And output should not contain "-public.t1_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (no change, DML), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t3_ao" to stdout
        And output should not contain "-public.t1_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (no change, DML), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao -i y,z\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t3_ao\(a,c\)" to stdout
        And output should not contain "-public.t1_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (DML&DDL, no change), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao" to stdout
        And output should not contain "public.t1_ao\("
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (DML&DDL, no change), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao" to stdout
        And output should not contain "public.t1_ao\("
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (DML&DDL, no change), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao\(x,z\)" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And column "y" of table "public.t1_ao" should not appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (DML, DDL), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(a,c\)"
        And output should not contain "public.t1_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (DML, DDL), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And output should not contain "public.t1_ao\("
        And output should not contain "public.t3_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (whole table, whole table), (DML, DDL), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao \npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(x,z\)" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    ### (some cols, some cols)

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (no change, no change), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao\(y\)" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,b" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (no change, no change), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(y\)" and "-public.t3_ao\(c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,b,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (no change, no change), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        When the user runs command "printf 'public.t1_ao -i x,y\npublic.t3_ao -i b,a' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "-public.t1_ao\(y\)" to stdout
        And output should not contain "public.t3_ao"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "b,a" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (no change, DML), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(y\)" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (no change, DML), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(y\)" and "-public.t3_ao"
        And output should not contain "-public.t3_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (no change, DML), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t3_ao" in schema "public" with column type list "int,text,real"
        When the user runs command "printf 'public.t1_ao -i y,z\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(y\)" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,y,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (DML&DDL, no change), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(c\)"
        And output should not contain "public.t1_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,b,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (DML&DDL, no change), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(c\)"
        And output should not contain "public.t1_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,b,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (DML&DDL, no change), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t1_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(x,z\)" and "-public.t3_ao\(c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And column "y" of table "public.t1_ao" should not appear in the latest column state file
        And columns "a,b,c" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (DML, DDL), (whole table, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao\(a,c\)"
        And output should not contain "public.t1_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (DML, DDL), (whole table, whole table)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao\npublic.t3_ao\n' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao" and "-public.t3_ao"
        And output should not contain "public.t1_ao\("
        And output should not contain "public.t3_ao\("
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "-1" of table "public.t1_ao" should appear in the latest column state file
        And columns "-1" of table "public.t3_ao" should appear in the latest column state file

    @analyzedb_core @analyzedb_multitables
    Scenario: Multiple tables, (some cols, some cols), (DML, DDL), (some cols, some cols)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,b' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And some data is inserted into table "t1_ao" in schema "public" with column type list "int,text,real"
        And some ddl is performed on table "t3_ao" in schema "public"
        When the user runs command "printf 'public.t1_ao -i x,z\npublic.t3_ao -i a,c' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should contain both "-public.t1_ao\(x,z\)" and "-public.t3_ao\(a,c\)"
        And "public.t1_ao" should appear in the latest state files
        And "public.t3_ao" should appear in the latest state files
        And columns "x,z" of table "public.t1_ao" should appear in the latest column state file
        And columns "a,c" of table "public.t3_ao" should appear in the latest column state file
        And column "b" of table "public.t3_ao" should not appear in the latest column state file

    # no entry in state files for parition tables

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (no entry, no change, root)
        Given no state files exist for database "incr_analyze"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then output should contain both "-public.sales_1_prt_default_dates" and "-public.sales_1_prt_2"
        And output should contain both "-public.sales_1_prt_3" and "-public.sales_1_prt_4"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_default_dates" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (no entry, no change, some parts)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_3' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_4"
        And output should contain both "-public.sales_1_prt_2" and "-public.sales_1_prt_3"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (no entry, dml on all parts, root)
        Given no state files exist for database "incr_analyze"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the row "3,'2008-01-03'" is inserted into "public.sales" in "incr_analyze"
        And the row "4,'2008-01-04'" is inserted into "public.sales" in "incr_analyze"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then output should contain both "-public.sales_1_prt_default_dates" and "-public.sales_1_prt_2"
        And output should contain both "-public.sales_1_prt_3" and "-public.sales_1_prt_4"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_default_dates" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (no entry, dml on all parts, some parts)
        Given no state files exist for database "incr_analyze"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the row "3,'2008-01-03'" is inserted into "public.sales" in "incr_analyze"
        And the row "4,'2008-01-04'" is inserted into "public.sales" in "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_3' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_4"
        And output should contain both "-public.sales_1_prt_2" and "-public.sales_1_prt_3"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (no entry, dml on some parts, root)
        Given no state files exist for database "incr_analyze"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then output should contain both "-public.sales_1_prt_default_dates" and "-public.sales_1_prt_2"
        And output should contain both "-public.sales_1_prt_3" and "-public.sales_1_prt_4"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_default_dates" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (no entry, dml on some parts, some parts)
        Given no state files exist for database "incr_analyze"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_3"
        And output should contain both "-public.sales_1_prt_2" and "-public.sales_1_prt_4"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files

    # entries exist for all parts in state files for parition tables

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for all parts, no change, root)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.sales"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_default_dates" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for all parts, no change, some parts)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.sales"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_3' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for all parts, dml on all parts, root)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.sales"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the row "3,'2008-01-03'" is inserted into "public.sales" in "incr_analyze"
        And the row "4,'2008-01-04'" is inserted into "public.sales" in "incr_analyze"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then output should contain both "-public.sales_1_prt_default_dates" and "-public.sales_1_prt_2"
        And output should contain both "-public.sales_1_prt_3" and "-public.sales_1_prt_4"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_default_dates" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for all parts, dml on all parts, some parts)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.sales"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the row "3,'2008-01-03'" is inserted into "public.sales" in "incr_analyze"
        And the row "4,'2008-01-04'" is inserted into "public.sales" in "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_3' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_4"
        And output should contain both "-public.sales_1_prt_2" and "-public.sales_1_prt_3"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for all parts, dml on some parts, root)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.sales"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then output should contain both "-public.sales_1_prt_2" and "-public.sales_1_prt_3"
        And output should not contain "-public.sales_1_prt_4"
        And output should not contain "-public.sales_1_prt_default_dates"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_default_dates" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for all parts, dml on some parts, some parts)
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.sales"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_3"
        And output should not contain "-public.sales_1_prt_4"
        And analyzedb should print "-public.sales_1_prt_2" to stdout
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files

    # entries exist for some parts in state files for parition tables

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for some parts, no change, root)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then output should contain both "-public.sales_1_prt_3" and "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_2"
        And output should not contain "-public.sales_1_prt_4"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_default_dates" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for some parts, no change, some parts)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_3' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_2"
        And output should not contain "-public.sales_1_prt_4"
        And analyzedb should print "-public.sales_1_prt_3" to stdout
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for some parts, dml on all parts, root)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the row "3,'2008-01-03'" is inserted into "public.sales" in "incr_analyze"
        And the row "4,'2008-01-04'" is inserted into "public.sales" in "incr_analyze"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then output should contain both "-public.sales_1_prt_default_dates" and "-public.sales_1_prt_2"
        And output should contain both "-public.sales_1_prt_3" and "-public.sales_1_prt_4"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_default_dates" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for some parts, dml on all parts, some parts)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the row "3,'2008-01-03'" is inserted into "public.sales" in "incr_analyze"
        And the row "4,'2008-01-04'" is inserted into "public.sales" in "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_3' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_4"
        And output should contain both "-public.sales_1_prt_2" and "-public.sales_1_prt_3"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for some parts, dml on some parts, root)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then output should contain both "-public.sales_1_prt_2" and "-public.sales_1_prt_3"
        And output should not contain "-public.sales_1_prt_4"
        And analyzedb should print "-public.sales_1_prt_default_dates" to stdout
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_default_dates" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for some parts, dml on some parts, some parts)
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_3 \npublic.sales_1_prt_4' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_2"
        And output should not contain "-public.sales_1_prt_4"
        And analyzedb should print "-public.sales_1_prt_3" to stdout
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files

    # refresh root stats

    @analyzedb_core @analyzedb_partition_tables @skip_root_stats
    Scenario: Partition tables, (entries for all parts, dml on some parts, some parts), request root stats
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.sales"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file --skip_root_stats"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_3"
        And output should not contain "-public.sales_1_prt_4"
        And analyzedb should print "-public.sales_1_prt_2" to stdout
        And output should not contain "analyze rootpartition public.sales"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables @refresh_root_stats
    Scenario: Partition tables, (no entry, dml on some parts, some parts), request root stats
        Given no state files exist for database "incr_analyze"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file --skip_root_stats"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_3"
        And output should contain both "-public.sales_1_prt_2" and "-public.sales_1_prt_4"
        And output should not contain "analyze rootpartition public.sales"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables @refresh_root_stats
    Scenario: Partition tables, (entries for some parts, dml on some parts, some parts), request root stats
        Given no state files exist for database "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_3 \npublic.sales_1_prt_4' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file --skip_root_stats"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_2"
        And output should not contain "-public.sales_1_prt_4"
        And analyzedb should print "-public.sales_1_prt_3" to stdout
        And output should not contain "analyze rootpartition public.sales"
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables @refresh_root_stats
    Scenario: Partition tables, (entries for all parts, no change, root), request root stats
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.sales --skip_root_stats"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_default_dates" should appear in the latest state files

    @analyzedb_core @analyzedb_partition_tables @refresh_root_stats
    Scenario: Partition tables, (entries for all parts, no change, some parts), request root stats
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.sales  --skip_root_stats"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_3' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files

    @analyzedb_core @analyzedb_root_and_partition_tables @refresh_root_stats
    Scenario: Partition tables, (entries for all parts, no change, some parts, root parts), request root stats
        Given no state files exist for database "incr_analyze"
        And the user runs "analyzedb -a -d incr_analyze -t public.sales"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files
        And "public.sales" should appear in the latest report file

    # request mid-level
    @analyzedb_core @analyzedb_partition_tables
    Scenario: Multi-level partition and request mid-level
        Given no state files exist for database "incr_analyze"
        And there is a hard coded multi-level ao partition table "sales_region" with 4 mid-level and 16 leaf-level partitions in schema "public"
        When the user runs "analyzedb -a -d incr_analyze -t public.sales_region_1_prt_2"
        Then analyzedb should print "There are no tables or partitions to be analyzed" to stdout
        And analyzedb should print "Skipping mid-level partition public.sales_region_1_prt_2" to stdout

    @analyzedb_core @analyzedb_partition_tables
    Scenario: Partition tables, (entries for some parts, dml on some parts, some parts), request root stats
        Given no state files exist for database "incr_analyze"
        And there is a hard coded multi-level ao partition table "sales_region" with 4 mid-level and 16 leaf-level partitions in schema "public"
        And the user runs command "printf 'public.sales_1_prt_2 \npublic.sales_1_prt_4' > config_file"
        And the user runs "analyzedb -a -d incr_analyze -f config_file"
        And the row "1,'2008-01-01'" is inserted into "public.sales" in "incr_analyze"
        And the row "2,'2008-01-02'" is inserted into "public.sales" in "incr_analyze"
        And the user runs command "printf 'public.sales_1_prt_3 \npublic.sales_1_prt_4\n public.sales_region_1_prt_3' > config_file"
        When the user runs "analyzedb -a -d incr_analyze -f config_file --skip_root_stats"
        Then output should not contain "-public.sales_1_prt_default_dates"
        And output should not contain "-public.sales_1_prt_2"
        And output should not contain "-public.sales_1_prt_4"
        And analyzedb should print "-public.sales_1_prt_3" to stdout
        And output should not contain "analyze rootpartition public.sales"
        And analyzedb should print "Skipping mid-level partition public.sales_region_1_prt_3" to stdout
        And "public.sales_1_prt_2" should appear in the latest state files
        And "public.sales_1_prt_4" should appear in the latest state files
        And "public.sales_1_prt_3" should appear in the latest state files

    @analyzedb_core @catalog_tables
    Scenario: Catalog tables
        Given no state files exist for database "incr_analyze"
        When the user runs "analyzedb -l -d incr_analyze -t pg_catalog.pg_class"
        Then analyzedb should print "-pg_catalog.pg_class" to stdout
        When the user runs "analyzedb -l -d incr_analyze -t pg_catalog.pg_attribute"
        Then analyzedb should print "-pg_catalog.pg_attribute" to stdout
        When the user runs "analyzedb -l -d incr_analyze -s pg_catalog"
        Then output should contain both "pg_catalog.pg_class" and "pg_catalog.pg_partition_rule"
        When the user runs "analyzedb -l -d incr_analyze"
        Then output should contain both "pg_catalog.pg_class" and "pg_catalog.pg_partition_rule"

    @analyzedb_concurrent
    Scenario: Concurrent analyzedb runs all capture the correct values in their output files
        Given no state files exist for database "incr_analyze"
        And the user runs "psql -d incr_analyze -c 'create schema incr_analyze_schema;'"
        And there is a regular "ao" table ""analyzedb_test"" with column name list "id,val" and column type list "int,text" in schema "incr_analyze_schema"
        And there is a regular "ao" table ""analyzedb_test_2"" with column name list "id,val" and column type list "int,text" in schema ""incr_analyze_schema""
        And some data is inserted into table "analyzedb_test" in schema "incr_analyze_schema" with column type list "int,text"
        And some data is inserted into table "analyzedb_test_2" in schema "incr_analyze_schema" with column type list "int,text"
        # modcount for both tables should be 1 at this point
        When the user runs "analyzedb -a -d incr_analyze -t incr_analyze_schema.analyzedb_test"
        Then analyzedb should return a return code of 0
        And "1" analyze directories exist for database "incr_analyze"
        When some data is inserted into table "analyzedb_test" in schema "incr_analyze_schema" with column type list "int,text"
        # modcount for analyzedb_test is now 2
        And the user starts a transaction and runs "update incr_analyze_schema.analyzedb_test SET id = 3  where id = 1;" on "incr_analyze"
        # the next analyze will have to wait on the previous transaction to finish
        And the user asynchronously runs "analyzedb -a -d incr_analyze -t incr_analyze_schema.analyzedb_test" and the process is saved
        # an analyze on second table will finish immediately
        And the user runs "analyzedb -a -d incr_analyze -t incr_analyze_schema.analyzedb_test_2"
        Then analyzedb should return a return code of 0
        And "2" analyze directories exist for database "incr_analyze"

        When the user rollsback the transaction
        # modcount for analyzedb_test is still 2
        And the async process finished with a return code of 0
        Then "3" analyze directories exist for database "incr_analyze"

        # we want any analyzedb run to watch out for concurrent runs and incorporate any new info in its output
        Then "incr_analyze_schema,analyzedb_test,2" should appear in the latest ao_state file in database "incr_analyze"
        And "incr_analyze_schema,analyzedb_test_2,1" should appear in the latest ao_state file in database "incr_analyze"
        # finally, another run should find nothing to do
        When the user runs "analyzedb -a -d incr_analyze -t incr_analyze_schema.analyzedb_test"
        Then analyzedb should return a return code of 0
        And analyzedb should print "There are no tables or partitions to be analyzed. Exiting" to stdout
        And "3" analyze directories exist for database "incr_analyze"

    @analyzedb_statement_mem
    Scenario: Run analyzedb with -m flag and confirm that statement_mem was set to custom value
        Given the count for "set statement_mem="1MB"; analyze pg_catalog.pg_class" in pglog is stored
        And the user runs "analyzedb -a -d incr_analyze -t pg_catalog.pg_class -m 1MB"
        Then the count for "set statement_mem="1MB"; analyze pg_catalog.pg_class" in pglog is incremented by "2"
