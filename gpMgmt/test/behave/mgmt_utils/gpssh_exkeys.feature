@gpssh_exkeys
Feature: gpssh behave tests

    # tests Step 2/3 of 5
    @skip
    Scenario: gpssh-exkeys on a single-node cluster can get transfer keys to enable scp between two hosts
        Given the gpssh_exkeys master host is set to "localhost"
        And the gpssh_exkeys segment host is set to "localhost3,localhost4,localhost5,localhost6"
        And the segment known_hosts mapping is removed on localhost
        And the segment hosts "cannot" reach each other automatically
        When gpssh-exkeys is run successfully
        Then the segment hosts "can" reach each other automatically

    # tests Step 2/3 of 5    
    @skip
    Scenario: gpssh-exkeys on a multi-node cluster can get transfer keys to enable scp between two hosts
        Given the gpssh_exkeys master host is set to "mdw"
        And the gpssh_exkeys segment host is set to "sdw1,sdw2,sdw3"
        And the segment known_hosts mapping is removed
        And the segment hosts "cannot" reach each other automatically
        When gpssh-exkeys is run successfully
        Then the segment hosts "can" reach each other automatically
        # make sure gpssh-exkeys can be run twice in a row
        When gpssh-exkeys is run successfully
        Then the segment hosts "can" reach each other automatically

    # tests Step 1 of 5
    @skip
    Scenario: gpssh-exkeys on a single-node cluster requires master to have a private key
        Given the gpssh_exkeys master host is set to "localhost"
        And the gpssh_exkeys segment host is set to "localhost3,localhost4,localhost5,localhost6"
        # And the segment known_hosts mapping is removed on localhost
        # And the segment hosts "cannot" reach each other automatically
        And the ssh file "id_rsa" is moved to a temporary directory
        When gpssh-exkeys is run eok         
        Then gpssh-exkeys should print "key file does not exist" error message
        And gpssh-exkeys should return a return code of 1

    # tests Step 1 of 5
    @skip
    Scenario: gpssh-exkeys on a single-node cluster can generate the public key
        Given the gpssh_exkeys master host is set to "localhost"
        And the gpssh_exkeys segment host is set to "localhost3,localhost4,localhost5,localhost6"
        # And the segment known_hosts mapping is removed on localhost
        # And the segment hosts "cannot" reach each other automatically
        And the ssh file "id_rsa.pub" is moved to a temporary directory
        When gpssh-exkeys is run successfully
        Then gpssh-exkeys should print "corresponding public key file not found...generating" to stdout

    @concourse_cluster
    Scenario: N-to-N exchange works
        Given the gpssh_exkeys master host is set to "mdw"
          And the gpssh_exkeys segment host is set to "sdw1,sdw2,sdw3"
          And all SSH configurations are backed up and removed
          And the segments can only be accessed using the master key
          And there is no duplication in the "authorized_keys" files
         Then all hosts "cannot" reach each other or themselves automatically
         When gpssh-exkeys is run successfully
         Then all hosts "can" reach each other or themselves automatically
         When gpssh-exkeys is run successfully
         Then all hosts "can" reach each other or themselves automatically
          And there is no duplication in the "known_hosts" files
          And there is no duplication in the "authorized_keys" files

    @concourse_cluster
    Scenario: additional hosts may be added after initial run
        Given the gpssh_exkeys master host is set to "mdw"
          And the gpssh_exkeys segment host is set to "sdw1,sdw2,sdw3"
          And all SSH configurations are backed up and removed
          And the segments can only be accessed using the master key
          And there is no duplication in the "authorized_keys" files
         Then all hosts "cannot" reach each other or themselves automatically
         When gpssh-exkeys is run successfully on hosts "sdw1, sdw2"
          And gpssh-exkeys is run successfully on additional hosts "sdw3"
         Then all hosts "can" reach each other or themselves automatically
          And there is no duplication in the "known_hosts" files
          And there is no duplication in the "authorized_keys" files

    @concourse_cluster
    Scenario: public keys are generated as needed
        Given the gpssh_exkeys master host is set to "mdw"
          And the gpssh_exkeys segment host is set to "sdw1,sdw2,sdw3"
          And all SSH configurations are backed up and removed
          And the segments can only be accessed using the master key
          And there is no duplication in the "authorized_keys" files
         Then all hosts "cannot" reach each other or themselves automatically

        Given the local public key is backed up and removed
         When gpssh-exkeys is run successfully
         Then all hosts "can" reach each other or themselves automatically

    @concourse_cluster
    Scenario: hostfiles are accepted as well
        Given the gpssh_exkeys master host is set to "mdw"
          And the gpssh_exkeys segment host is set to "sdw1,sdw2,sdw3"
          And all SSH configurations are backed up and removed
          And the segments can only be accessed using the master key
          And there is no duplication in the "authorized_keys" files
         Then all hosts "cannot" reach each other or themselves automatically

         When gpssh-exkeys is run successfully with a hostfile
         Then all hosts "can" reach each other or themselves automatically


