@gpcheckperf
Feature: Tests for gpcheckperf

  @concourse_cluster
  Scenario: gpcheckperf runs disk and memory tests
    Given the database is running
    When  the user runs "gpcheckperf -h cdw -h sdw1 -d /data/gpdata/ -r ds"
    Then  gpcheckperf should return a return code of 0
    And   gpcheckperf should print "disk write tot bytes" to stdout

  @concourse_cluster
  Scenario: gpcheckperf runs runs sequential network test
    Given the database is running
    When  the user runs "gpcheckperf -h cdw -h sdw1 -d /data/gpdata/ -r n"
    Then  gpcheckperf should return a return code of 0
    And   gpcheckperf should print "avg = " to stdout
    And   gpcheckperf should not print "NOTICE: -t is deprecated " to stdout

  Scenario: gpcheckperf runs with -S option and prints a warning message
    Given the database is running
    When  the user runs "gpcheckperf -h localhost -r d -d /tmp -S 1GB"
    Then  gpcheckperf should return a return code of 0
    And   gpcheckperf should print "\[Warning] Using 1073741824 bytes for disk performance test. This might take some time" to stdout

  Scenario: gpcheckperf errors out when invalid value is passed to the -S option
    Given the database is running
    When  the user runs "gpcheckperf -h localhost -r d -d /tmp -S abc"
    Then  gpcheckperf should return a return code of 1

  @concourse_cluster
  Scenario Outline: gpcheckperf run <test_type> test by passing hostfile in regular mode
    Given the database is running
    And create a gpcheckperf input host file
    When  the user runs "gpcheckperf -f /tmp/hostfile1 -r <cmd_param> -d /data/gpdata/ --duration=10s"
    Then  gpcheckperf should return a return code of 0
    And   gpcheckperf should print "--  NETPERF TEST" to stdout
    And   gpcheckperf should print "<print_message>" to stdout
    And   gpcheckperf should print "Summary:" to stdout
    And   gpcheckperf should print "sum =" to stdout
    And   gpcheckperf should print "min =" to stdout
    And   gpcheckperf should print "max =" to stdout
    And   gpcheckperf should print "avg =" to stdout
    And   gpcheckperf should print "median =" to stdout

  Examples:
    | test_type | cmd_param | print_message                      |
    | network   | N         | Netperf bisection bandwidth test   |
    | matrix    | M         | Full matrix netperf bandwidth test |

  @concourse_cluster
  Scenario Outline: gpcheckperf runs <test_type> test with hostfile in <verbosity> mode
     Given the database is running
     And create a gpcheckperf input host file
     When  the user runs "gpcheckperf -f /tmp/hostfile1 -r <cmd_param> -d /data/gpdata/ --duration=10s <verbose_flag>"
     Then  gpcheckperf should return a return code of 0
     And   gpcheckperf should print "--  NETPERF TEST" to stdout
     And   gpcheckperf should print "<print_message>" to stdout
     And   gpcheckperf should print "making gpcheckperf directory on all hosts ..." to stdout
     And   gpcheckperf should print "[Info].*gpssh <gpssh_param> .*hostfile1 .*gpnetbenchClient." to stdout
     And   gpcheckperf should print "[Info].*gpssh <gpssh_param> .*hostfile1 .*gpnetbenchServer." to stdout
     And   gpcheckperf should print "==  RESULT*" to stdout
     And   gpcheckperf should print "Summary:" to stdout
     And   gpcheckperf should print "TEARDOWN" to stdout

  Examples:
    | test_type | verbosity     | cmd_param  | verbose_flag | gpssh_param | print_message                      |
    | network   | verbose       | N          | -v           | -f          | Netperf bisection bandwidth test   |
    | network   | extra verbose | N          | -V           | -v -f       | Netperf bisection bandwidth test   |
    | matrix    | verbose       | M          | -v           | -f          | Full matrix netperf bandwidth test |
    | matrix    | extra verbose | M          | -V           | -v -f       | Full matrix netperf bandwidth test |

  @concourse_cluster
  Scenario Outline: running gpcheckperf single host <test_name> test case
     Given the database is running
     And create a gpcheckperf input host file
     When  the user runs "gpcheckperf -h cdw -r <cmd_param> -d /data/gpdata/ --duration=10s -v"
     Then  gpcheckperf should return a return code of 0
     And   gpcheckperf should print "--  NETPERF TEST" to stdout
     And   gpcheckperf should print "single host only - abandon netperf test" to stdout
     And   gpcheckperf should print "TEARDOWN" to stdout

  Examples:
    | test_name   | cmd_param|
    | matrix test | M        |
    | network test| N        |


  @concourse_cluster
  Scenario: gpcheckperf runs successfully when scp is not available or does not have execute permission on master host
    Given the database is running
    And   "/usr/bin/scp" has its permissions set to "664" on "cdw"
    When  the user runs "gpcheckperf -h sdw1 -r ds -d /tmp -S 10MB -v"
    Then  gpcheckperf should return a return code of 0
    And   gpcheckperf should print "\[Warning] Either scp is not available or does not have execute permission on host:cdw" to stdout
    And   gpcheckperf should print "rsync -P -a -c -e * .*multidd sdw1:*" to stdout
    And   rely on environment.py to restore path permissions

  @concourse_cluster
  Scenario: gpcheckperf runs successfully when scp is not available or does not have execute permission on segment host
    Given the database is running
    And   "/usr/bin/scp" has its permissions set to "664" on "sdw1"
    When  the user runs "gpcheckperf -h cdw -h sdw1 -r ds -d /data/gpdata/ -S 10MB -v"
    Then  gpcheckperf should return a return code of 0
    And   gpcheckperf should print "\[Warning] Either scp is not available or does not have execute permission on host:sdw1" to stdout
    And   gpcheckperf should print "rsync -P -a -c -e * .*multidd cdw:*" to stdout
    And   rely on environment.py to restore path permissions

  @concourse_cluster
  Scenario: gpcheckperf runs sequential network test with buffer size flag
    Given the database is running
    When  the user runs "gpcheckperf -h cdw -h sdw1 -d /data/gpdata/ -r n --buffer-size 8 -v"
    Then  gpcheckperf should return a return code of 0
    And   gpcheckperf should print "avg = " to stdout
    And   gpcheckperf should print "gpnetbenchClient -H cdw -p 23000 -l 15 -P 0 -b 8" to stdout

  @concourse_cluster
  Scenario: gpcheckperf runs sequential network test with buffer size flag and netperf option
    Given the database is running
    When  the user runs "gpcheckperf -h cdw -h sdw1 -d /data/gpdata/ -r n --buffer-size 8 --netperf"
    Then  gpcheckperf should print "--buffer-size option will be ignored when the --netperf option is enabled" to stdout

  @concourse_cluster
  Scenario: gpcheckperf runs sequential network test without buffer size flag
    Given the database is running
    When  the user runs "gpcheckperf -h cdw -h sdw1 -d /data/gpdata/ -r n"
    Then  gpcheckperf should return a return code of 0
    And   gpcheckperf should print "--buffer-size value is not specified or invalid. Using default \(32 kilobytes\)" to stdout
    And   gpcheckperf should print "avg = " to stdout
