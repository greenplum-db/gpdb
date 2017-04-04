@gpsegment
Feature: behave test for gpdb segment

    Scenario: Validate gpdb segment should timeout instead of hang forever if gpfdist doesn't response
        Given the database is running
        Given the database "testdb" does not exist
        Given the "gpfdist" process is killed
        And database "testdb" exists
        Given the "-f python.*fake_gpfdist.py.*-p.*8088" process is killed
        Given the client program "fake_gpfdist.py" is present under CWD in "test/behave/mgmt_utils/steps/data"
        When the user runs client program "fake_gpfdist.py -p 8088 -m NoResponse &" from "test/behave/mgmt_utils/steps/data" under CWD
        And a "writable" external table "wrt" is created on file "test" in "testdb"
        And the user runs command "psql -d testdb -c 'insert into wrt values(1)'"
        Then the client program should print "failed to send request" error message
        And the "-f python.*fake_gpfdist.py.*-p.*8088" process is killed

    @gpsegment_consective_sequence_number
    Scenario: Validate gpdb segment should send request by consective sequence number.
        Given the database is running
        Given the database "testdb" does not exist
        Given the "gpfdist" process is killed
        And database "testdb" exists
        Given the "-f python.*fake_gpfdist.py.*-p.*8088" process is killed
        Given the directory "extdata" exists in current working directory
        Given the client program "fake_gpfdist.py" is present under CWD in "test/behave/mgmt_utils/steps/data"
        When the user runs client program "fake_gpfdist.py -p 8088 -m TestSequenceNumber 1> extdata/fake_gpfdist.log 2>&1 &" from "test/behave/mgmt_utils/steps/data" under CWD
        And a "writable" external table "wrt" is created on file "test" in "testdb"
        And the user runs command "psql -d testdb -c 'insert into wrt values(1)'"
        And the user runs command "cat ./extdata/fake_gpfdist.log"
        Then the client program should print "TestSequence succeeded" to stdout
     
    @gpsegment_resend_sequence_number
    Scenario: Validate gpdb segment should resend with same sequence number if 408 
        Given the database is running
        Given the database "testdb" does not exist
        Given the "gpfdist" process is killed
        And database "testdb" exists
        Given the "-f python.*fake_gpfdist.py.*-p.*8088" process is killed
        Given the directory "extdata" exists in current working directory
        Given the client program "fake_gpfdist.py" is present under CWD in "test/behave/mgmt_utils/steps/data"
        When the user runs client program "fake_gpfdist.py -p 8088 -m TestBadResponse 1> extdata/fake_gpfdist.log 2>&1 &" from "test/behave/mgmt_utils/steps/data" under CWD
        And a "writable" external table "wrt" is created on file "test" in "testdb"
        And the user runs command "psql -d testdb -c 'insert into wrt values(1)'"
        And the user runs command "cat ./extdata/fake_gpfdist.log"
        Then the client program should print "TestBadResponse succeeded" to stdout
    
    Scenario: Validate gpdb segment should timeout when set readable_external_table_timeout if gpfdist doesn't response
        Given the database is running
        Given the user runs "gpconfig -c readable_external_table_timeout -v 5"
        Given the user runs "gpstop -u"
        Given the database "testdb" does not exist
        Given database "testdb" exists
        Given the directory "extdata" exists in current working directory
        Given the "gpfdist" process is killed
        Given the "-f python.*fake_gpfdist.py.*-p.*8088" process is killed
        Given the client program "fake_gpfdist.py" is present under CWD in "test/behave/mgmt_utils/steps/data"
        When the user runs client program "fake_gpfdist.py -p 8088 -m NoResponse &" from "test/behave/mgmt_utils/steps/data" under CWD
        And a "readable" external table "ret" is created on file "test" in "testdb"
        And the user runs command "psql -d testdb -c 'select * from ret' & echo $! > extdata/psql.pid"
        And the user runs command "sleep 70"
        And the user runs command "ps -ef|grep psql|grep -v grep|awk '{print $2}'|grep `cat extdata/psql.pid`|wc -l"
        Then the client program should print "0" to stdout
        And the "-f python.*fake_gpfdist.py.*-p.*8088" process is killed

    Scenario: Validate gpdb segment should not timeout when not set readable_external_table_timeout if gpfdist doesn't response
        Given the database is running
        Given the user runs "gpconfig -c readable_external_table_timeout -v 0"
        Given the user runs "gpstop -u"
        Given the database "testdb" does not exist
        Given database "testdb" exists
        Given the directory "extdata" exists in current working directory
        Given the "gpfdist" process is killed
        Given the "-f python.*fake_gpfdist.py.*-p.*8088" process is killed
        Given the client program "fake_gpfdist.py" is present under CWD in "test/behave/mgmt_utils/steps/data"
        When the user runs client program "fake_gpfdist.py -p 8088 -m NoResponse &" from "test/behave/mgmt_utils/steps/data" under CWD
        And a "readable" external table "ret" is created on file "test" in "testdb"
        And the user runs command "psql -d testdb -c 'select * from ret' & echo $! > extdata/psql.pid"
        And the user runs command "sleep 70"
        And the user runs command "ps -ef|grep psql|grep -v grep|awk '{print $2}'|grep `cat extdata/psql.pid`|wc -l"
        Then the client program should print "1" to stdout
        And the "-f python.*fake_gpfdist.py.*-p.*8088" process is killed
        And the "-f psql.*-d.*testdb.*-c" process is killed
