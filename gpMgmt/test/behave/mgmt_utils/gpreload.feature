@gpreload
Feature: gpreload behave tests

    Scenario: Handle case where a table (public.t1) and view (gpreload_schema.t1) have the same name
        Given schema "gpreload_schema" exists in "gpreload_db"
        And there is a regular "heap" table "t1" with column name list "a,b" and column type list "int,int" in schema "public"
        And some data is inserted into table "t1" in schema "public" with column type list "int,int"
        And a view "gpreload_schema.t1" exists on table "t1"
        When the user runs command "echo 'public.t1: a, b' > gpreload_config_file"
        And the user runs "gpreload -d gpreload_db -t gpreload_config_file"
        Then gpreload should return a return code of 0
