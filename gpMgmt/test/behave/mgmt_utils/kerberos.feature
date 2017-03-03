@kerberos
Feature: Kerberos testing

    @kerberos
    Scenario: valid until timestamp unset
        Given the kerberos is setup on the cluster
        Then a connection to database is succesfully established as a kerberos user

    @kerberos
    Scenario: valid until timestamp expired
        Given the kerberos is setup on the cluster
        When the valid until timestamp is expired for the kerberos user
        Then a kerberos user connection to the database will print valid until timestamp expired to stdout
        And the valid until timestamp is not expired for the kerberos user

    @kerberos
    Scenario: valid until timestamp not expired
        Given the kerberos is setup on the cluster
        When the valid until timestamp is not expired for the kerberos user
        Then a connection to database is succesfully established as a kerberos user

