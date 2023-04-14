---
title: Configuring Kerberos for Linux Clients 
---

You can configure Linux client applications to connect to a Greenplum Database system that is configured to authenticate with Kerberos.

If your JDBC application on Red Hat Enterprise Linux uses Kerberos authentication when it connects to your Greenplum Database, your client system must be configured to use Kerberos authentication. If you are not using Kerberos authentication to connect to a Greenplum Database, Kerberos is not needed on your client system.

-   [Requirements](#topic13)
-   [Setting Up Client System with Kerberos Authentication](#topic17)
-   [Running a Java Application](#topic18)

For information about enabling Kerberos authentication with Greenplum Database, see the chapter "Setting Up Kerberos Authentication" in the *Greenplum Database Administrator Guide*.

**Parent topic:** [Configuring Client Authentication](client_auth.html)

## <a id="topic13"></a>Requirements 

The following are requirements to connect to a Greenplum Database that is enabled with Kerberos authentication from a client system with a JDBC application.

-   [Prerequisites](#topic14)
-   [Required Software on the Client Machine](#topic15)

### <a id="topic14"></a>Prerequisites 

-   Kerberos must be installed and configured on the Greenplum Database coordinator host.

    > **Important** Greenplum Database must be configured so that a remote user can connect to Greenplum Database with Kerberos authentication. Authorization to access Greenplum Database is controlled by the `pg_hba.conf` file. For details, see "Editing the pg\_hba.conf File" in the *Greenplum Database Administration Guide*, and also see the *Greenplum Database Security Configuration Guide*.

-   The client system requires the Kerberos configuration file `krb5.conf` from the Greenplum Database coordinator.
-   The client system requires a Kerberos keytab file that contains the authentication credentials for the Greenplum Database user that is used to log into the database.
-   The client machine must be able to connect to Greenplum Database coordinator host.

    If necessary, add the Greenplum Database coordinator host name and IP address to the system `hosts` file. On Linux systems, the `hosts` file is in `/etc`.


### <a id="topic15"></a>Required Software on the Client Machine 

-   The Kerberos `kinit` utility is required on the client machine. The `kinit` utility is available when you install the Kerberos packages:

    -   krb5-libs
    -   krb5-workstation<br/>
    > **Note** When you install the Kerberos packages, you can use other Kerberos utilities such as `klist` to display Kerberos ticket information.


Java applications require this additional software:

-   Java JDK

    Java JDK 1.7.0\_17 is supported on Red Hat Enterprise Linux 6.x.

-   Ensure that JAVA\_HOME is set to the installation directory of the supported Java JDK.

## <a id="topic17"></a>Setting Up Client System with Kerberos Authentication 

To connect to Greenplum Database with Kerberos authentication requires a Kerberos ticket. On client systems, tickets are generated from Kerberos keytab files with the `kinit` utility and are stored in a cache file.

1.  Install a copy of the Kerberos configuration file `krb5.conf` from the Greenplum Database coordinator. The file is used by the Greenplum Database client software and the Kerberos utilities.

    Install `krb5.conf` in the directory `/etc`.

    If needed, add the parameter `default_ccache_name` to the `[libdefaults]` section of the `krb5.ini` file and specify location of the Kerberos ticket cache file on the client system.

2.  Obtain a Kerberos keytab file that contains the authentication credentials for the Greenplum Database user.
3.  Run `kinit` specifying the keytab file to create a ticket on the client machine. For this example, the keytab file `gpdb-kerberos.keytab` is in the current directory. The ticket cache file is in the `gpadmin` user home directory.

    ```
    > kinit -k -t gpdb-kerberos.keytab -c /home/gpadmin/cache.txt 
       gpadmin/kerberos-gpdb@KRB.EXAMPLE.COM
    ```


## <a id="topic12"></a>Running psql 

From a remote system, you can access a Greenplum Database that has Kerberos authentication enabled.

### <a id="mh151095"></a>To connect to Greenplum Database with psql 

1.  As the gpadmin user, open a command window.
2.  Start `psql` from the command window and specify a connection to the Greenplum Database specifying the user that is configured with Kerberos authentication.

    The following example logs into the Greenplum Database on the machine `kerberos-gpdb` as the `gpadmin` user with the Kerberos credentials `gpadmin/kerberos-gpdb`:

    ```
    $ psql -U "gpadmin/kerberos-gpdb" -h kerberos-gpdb postgres
    ```


## <a id="topic18"></a>Running a Java Application 

Accessing Greenplum Database from a Java application with Kerberos authentication uses the Java Authentication and Authorization Service \(JAAS\)

1.  Create the file `.java.login.config` in the user home folder.

    For example, on a Linux system, the home folder is similar to `/home/gpadmin`.

    Add the following text to the file:

    ```
    pgjdbc {
      com.sun.security.auth.module.Krb5LoginModule required
      doNotPrompt=true
      useTicketCache=true
      ticketCache = "/home/gpadmin/cache.txt"
      debug=true
      client=true;
    };
    ```

2.  Create a Java application that connects to Greenplum Database using Kerberos authentication and run the application as the user.

This example database connection URL uses a PostgreSQL JDBC driver and specifies parameters for Kerberos authentication.

```
jdbc:postgresql://kerberos-gpdb:5432/mytest? 
  kerberosServerName=postgres&jaasApplicationName=pgjdbc& 
  user=gpadmin/kerberos-gpdb
```

The parameter names and values specified depend on how the Java application performs Kerberos authentication.

