@fuzzystrmatch
Feature: fuzzystrmatch regression

    Scenario: check fuzzystrmatch module is installed
        Then verify that file "fuzzystrmatch.so" exists under "${GPHOME}/lib/postgresql"
        Then verify that file "fuzzystrmatch.sql" exists under "${GPHOME}/share/postgresql/contrib"
        Then verify that file "uninstall_fuzzystrmatch.sql" exists under "${GPHOME}/share/postgresql/contrib"

    Scenario: soundex(),test_soundex() and difference() should work
        And the database "testfuzzy" does not exist
        And database "testfuzzy" exists
        When the user runs command "psql -d testfuzzy -f $GPHOME/share/postgresql/contrib/uninstall_fuzzystrmatch.sql" 
        When the user runs command "psql -d testfuzzy -f $GPHOME/share/postgresql/contrib/fuzzystrmatch.sql" 
        When execute sql "SELECT soundex('Anne'), soundex('Andrew'), text_soundex('Andrew'), difference('Anne', 'Andrew')" in db "testfuzzy" and store result in the context
        Then validate that following rows are in the stored rows
          | soundex | soundex | text_soundex| difference |
          | A500    | A536    | A536        |          2 |

    Scenario: metaphone(),dmetaphone_alt() and dmetaphone() should work
        And the database "testfuzzy" does not exist
        And database "testfuzzy" exists
        When the user runs command "psql -d testfuzzy -f $GPHOME/share/postgresql/contrib/uninstall_fuzzystrmatch.sql" 
        When the user runs command "psql -d testfuzzy -f $GPHOME/share/postgresql/contrib/fuzzystrmatch.sql" 
        When execute sql "SELECT metaphone('nujabes',4), dmetaphone('nujabes'), dmetaphone_alt('nujabes')" in db "testfuzzy" and store result in the context
        Then validate that following rows are in the stored rows
          | metaphone | dmetaphone | dmetaphone_alt |
          | NJBS      | NJPS       | NHPS           |

    Scenario: two versions of levenshtein() and levenshtein_less_equal() should work
        And the database "testfuzzy" does not exist
        And database "testfuzzy" exists
        When the user runs command "psql -d testfuzzy -f $GPHOME/share/postgresql/contrib/uninstall_fuzzystrmatch.sql" 
        When the user runs command "psql -d testfuzzy -f $GPHOME/share/postgresql/contrib/fuzzystrmatch.sql" 
        When execute sql "SELECT levenshtein('GUMBO', 'GAMBOL'), levenshtein('GUMBO', 'GAMBOL', 2,1,1), levenshtein_less_equal('extensive', 'exhaustive',2), levenshtein_less_equal('extensive', 'exhaustive',4)" in db "testfuzzy" and store result in the context
        Then validate that following rows are in the stored rows
          | levenshtein | levenshtein | levenshtein_less_equal | levenshtein_less_equal |
          | 2           | 3           | 3                      | 4                      |
