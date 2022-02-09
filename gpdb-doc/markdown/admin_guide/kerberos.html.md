---
title: Using Kerberos Authentication 
---

You can control access to Greenplum Database with a Kerberos authentication server.

Greenplum Database supports the Generic Security Service Application Program Interface \(GSSAPI\) with Kerberos authentication. GSSAPI provides automatic authentication \(single sign-on\) for systems that support it. You specify the Greenplum Database users \(roles\) that require Kerberos authentication in the Greenplum Database configuration file pg\_hba.conf. The login fails if Kerberos authentication is not available when a role attempts to log in to Greenplum Database.

Kerberos provides a secure, encrypted authentication service. It does not encrypt data exchanged between the client and database and provides no authorization services. To encrypt data exchanged over the network, you must use an SSL connection. To manage authorization for access to Greenplum databases and objects such as schemas and tables, you use settings in the pg\_hba.conf file and privileges given to Greenplum Database users and roles within the database. For information about managing authorization privileges, see [Managing Roles and Privileges](roles_privs.html).

For more information about Kerberos, see [http://web.mit.edu/kerberos/](http://web.mit.edu/kerberos/).

## <a id="kerberos_prereq"></a>Prerequisites 

Before configuring Kerberos authentication for Greenplum Database, ensure that:

-   You can identify the KDC server you use for Kerberos authentication and the Kerberos realm for your Greenplum Database system. If you have not yet configured your MIT Kerberos KDC server, see [Installing and Configuring a Kerberos KDC Server](#task_setup_kdc) for example instructions.
-   System time on the Kerberos Key Distribution Center \(KDC\) server and Greenplum Database master is synchronized. \(For example, install the `ntp` package on both servers.\)
-   Network connectivity exists between the KDC server and the Greenplum Database master host.
-   Java 1.7.0\_17 or later is installed on all Greenplum Database hosts. Java 1.7.0\_17 is required to use Kerberos-authenticated JDBC on Red Hat Enterprise Linux 6.x or 7.x.

## <a id="nr166539"></a>Procedure 

Following are the tasks to complete to set up Kerberos authentication for Greenplum Database.

-   [Creating Greenplum Database Principals in the KDC Database](#task_m43_vwl_2p)
-   [Installing the Kerberos Client on the Master Host](#topic6)
-   [Configuring Greenplum Database to use Kerberos Authentication](#topic7)
-   [Mapping Kerberos Principals to Greenplum Database Roles](#topic_kmr_gws_d2b)
-   [Configuring JDBC Kerberos Authentication for Greenplum Database](#topic9)
-   [Configuring Kerberos for Linux Clients](kerberos-lin-client.html)
-   [Configuring Kerberos For Windows Clients](kerberos-win-client.html)

**Parent topic:**[Configuring Client Authentication](client_auth.html)

## <a id="task_m43_vwl_2p"></a>Creating Greenplum Database Principals in the KDC Database 

Create a service principal for the Greenplum Database service and a Kerberos admin principal that allows managing the KDC database as the gpadmin user.

1.  Log in to the Kerberos KDC server as the root user.

    ```
    $ ssh root@<kdc-server>
    ```

2.  Create a principal for the Greenplum Database service.

    ```
    # kadmin.local -q "addprinc -randkey postgres/mdw@GPDB.KRB"
    ```

    The `-randkey` option prevents the command from prompting for a password.

    The `postgres` part of the principal names matches the value of the Greenplum Database `krb_srvname` server configuration parameter, which is `postgres` by default.

    The host name part of the principal name must match the output of the `hostname` command on the Greenplum Database master host. If the `hostname` command shows the fully qualified domain name \(FQDN\), use it in the principal name, for example `postgres/mdw.example.com@GPDB.KRB`.

    The `GPDB.KRB` part of the principal name is the Kerberos realm name.

3.  Create a principal for the gpadmin/admin role.

    ```
    # kadmin.local -q "addprinc gpadmin/admin@GPDB.KRB"
    ```

    This principal allows you to manage the KDC database when you are logged in as gpadmin. Make sure that the Kerberos `kadm.acl` configuration file contains an ACL to grant permissions to this principal. For example, this ACL grants all permissions to any admin user in the GPDB.KRB realm.

    ```
    */admin@GPDB.KRB *
    ```

4.  Create a keytab file with `kadmin.local`. The following example creates a keytab file `gpdb-kerberos.keytab` in the current directory with authentication information for the Greenplum Database service principal and the gpadmin/admin principal.

    ```
    # kadmin.local -q "ktadd -k gpdb-kerberos.keytab postgres/mdw@GPDB.KRB gpadmin/admin@GPDB.KRB"
    ```

5.  Copy the keytab file to the master host.

    ```
    # scp gpdb-kerberos.keytab gpadmin@mdw:~
    ```


## <a id="topic6"></a>Installing the Kerberos Client on the Master Host 

Install the Kerberos client utilities and libraries on the Greenplum Database master.

1.  Install the Kerberos packages on the Greenplum Database master.

    ```
    $ sudo yum install krb5-libs krb5-workstation
    ```

2.  Copy the `/etc/krb5.conf` file from the KDC server to `/etc/krb5.conf` on the Greenplum Master host.

## <a id="topic7"></a>Configuring Greenplum Database to use Kerberos Authentication 

Configure Greenplum Database to use Kerberos.

1.  Log in to the Greenplum Database master host as the gpadmin user.

    ```
    $ ssh gpadmin@<master>
    $ source /usr/local/greenplum-db/greenplum_path.sh
    ```

2.  Set the ownership and permissions of the keytab file you copied from the KDC server.

    ```
    $ chown gpadmin:gpadmin /home/gpadmin/gpdb-kerberos.keytab
    $ chmod 400 /home/gpadmin/gpdb-kerberos.keytab
    ```

3.  Configure the location of the keytab file by setting the Greenplum Database `krb_server_keyfile` server configuration parameter. This `gpconfig` command specifies the folder /home/gpadmin as the location of the keytab file gpdb-kerberos.keytab.

    ```
    $ gpconfig -c krb_server_keyfile -v  '/home/gpadmin/gpdb-kerberos.keytab'
    ```

4.  Modify the Greenplum Database file `pg_hba.conf` to enable Kerberos support. For example, adding the following line to `pg_hba.conf` adds GSSAPI and Kerberos authentication support for connection requests from all users and hosts on the same network to all Greenplum Database databases.

    ```
    host all all 0.0.0.0/0 gss include_realm=0 krb_realm=GPDB.KRB
    ```

    Setting the `krb_realm` option to a realm name ensures that only users from that realm can successfully authenticate with Kerberos. Setting the `include_realm` option to `0` excludes the realm name from the authenticated user name. For information about the `pg_hba.conf` file, see [The pg\_hba.conf file](https://www.postgresql.org/docs/9.4/auth-pg-hba-conf.html) in the PostgreSQL documentation.

5.  Restart Greenplum Database after updating the `krb_server_keyfile` parameter and the `pg_hba.conf` file.

    ```
    $ gpstop -ar
    ```

6.  Create the gpadmin/admin Greenplum Database superuser role.

    ```
    $ createuser gpadmin/admin --superuser
    ```

    The Kerberos keys for this database role are in the keyfile you copied from the KDC server.

7.  Create a ticket using `kinit` and show the tickets in the Kerberos ticket cache with `klist`.

    ```
    $ LD_LIBRARY_PATH= kinit -k -t /home/gpadmin/gpdb-kerberos.keytab gpadmin/admin@GPDB.KRB
    $ LD_LIBRARY_PATH= klist
    Ticket cache: FILE:/tmp/krb5cc_1000
    Default principal: gpadmin/admin@GPDB.KRB
    
    Valid starting       Expires              Service principal
    06/13/2018 17:37:35  06/14/2018 17:37:35  krbtgt/GPDB.KRB@GPDB.KRB
    ```

    **Note:** When you set up the Greenplum Database environment by sourcing the `greenplum-db_path.sh` script, the `LD_LIBRARY_PATH` environment variable is set to include the Greenplum Database `lib` directory, which includes Kerberos libraries. This may cause Kerberos utility commands such as `kinit` and `klist` to fail due to version conflicts. The solution is to run Kerberos utilities before you source the `greenplum-db_path.sh` file or temporarily unset the `LD_LIBRARY_PATH` variable when you run Kerberos utilities, as shown in the example.

8.  As a test, log in to the postgres database with the `gpadmin/admin` role:

    ```
    $ psql -U "gpadmin/admin" -h mdw postgres
    psql (9.4.20)
    Type "help" for help.
    
    postgres=# select current_user;
     current_user
    ---------------
     gpadmin/admin
    (1 row)
    ```

    **Note:** When you start `psql` on the master host, you must include the `-h <master-hostname>` option to force a TCP connection because Kerberos authentication does not work with local connections.


If a Kerberos principal is not a Greenplum Database user, a message similar to the following is displayed from the `psql` command line when the user attempts to log in to the database:

```
psql: krb5_sendauth: Bad response
```

The principal must be added as a Greenplum Database user.

## <a id="topic_kmr_gws_d2b"></a>Mapping Kerberos Principals to Greenplum Database Roles 

To connect to a Greenplum Database system with Kerberos authentication enabled, a user first requests a ticket-granting ticket from the KDC server using the `kinit` utility with a password or a keytab file provided by the Kerberos admin. When the user then connects to the Kerberos-enabled Greenplum Database system, the user's Kerberos principle name will be the Greenplum Database role name, subject to transformations specified in the options field of the `gss` entry in the Greenplum Database `pg_hba.conf` file:

-   If the `krb_realm=<realm>` option is present, Greenplum Database only accepts Kerberos principals who are members pf the specified realm.
-   If the `include_realm=0` option is specified, the Greenplum Database role name is the Kerberos principal name without the Kerberos realm. If the `include_realm=1` option is instead specified, the Kerberos realm is not stripped from the Greenplum Database rolename. The role must have been created with the Greenplum Database `CREATE ROLE` command.
-   If the `map=<map-name>` option is specified, the Kerberos principal name is compared to entries labeled with the specified `<map-name>` in the `$COORDINATOR_DATA_DIRECTORY/pg_ident.conf` file and replaced with the Greenplum Database role name specified in the first matching entry.

A user name map is defined in the `$COORDINATOR_DATA_DIRECTORY/pg_ident.conf` configuration file. This example defines a map named `mymap` with two entries.

```

# MAPNAME   SYSTEM-USERNAME        GP-USERNAME
mymap       /^admin@GPDB.KRB$      gpadmin
mymap       /^(.*)_gp)@GPDB.KRB$   \1
```

The map name is specified in the `pg_hba.conf` Kerberos entry in the options field:

```
host all all 0.0.0.0/0 gss include_realm=0 krb_realm=GPDB.KRB map=mymap
```

The first map entry matches the Kerberos principal admin@GPDB.KRB and replaces it with the Greenplum Database gpadmin role name. The second entry uses a wildcard to match any Kerberos principal in the GPDB-KRB realm with a name ending with the characters `_gp` and replaces it with the initial portion of the principal name. Greenplum Database applies the first matching map entry in the `pg_ident.conf` file, so the order of entries is significant.

For more information about using username maps see [Username maps](https://www.postgresql.org/docs/9.4/auth-username-maps.html) in the PostgreSQL documentation.

## <a id="topic9"></a>Configuring JDBC Kerberos Authentication for Greenplum Database 

Enable Kerberos-authenticated JDBC access to Greenplum Database.

You can configure Greenplum Database to use Kerberos to run user-defined Java functions.

1.  Ensure that Kerberos is installed and configured on the Greenplum Database master. See [Installing the Kerberos Client on the Master Host](#topic6).

2.  Create the file .java.login.config in the folder `/home/gpadmin` and add the following text to the file:

    ```
    pgjdbc {
      com.sun.security.auth.module.Krb5LoginModule required
      doNotPrompt=true
      useTicketCache=true
      debug=true
      client=true;
    };
    ```

3.  Create a Java application that connects to Greenplum Database using Kerberos authentication. The following example database connection URL uses a PostgreSQL JDBC driver and specifies parameters for Kerberos authentication:

    ```
    jdbc:postgresql://mdw:5432/mytest?kerberosServerName=postgres
    &jaasApplicationName=pgjdbc&user=gpadmin/gpdb-kdc
    ```

    The parameter names and values specified depend on how the Java application performs Kerberos authentication.

4.  Test the Kerberos login by running a sample Java application from Greenplum Database.


## <a id="task_setup_kdc"></a>Installing and Configuring a Kerberos KDC Server 

Steps to set up a Kerberos Key Distribution Center \(KDC\) server on a Red Hat Enterprise Linux host for use with Greenplum Database.

If you do not already have a KDC, follow these steps to install and configure a KDC server on a Red Hat Enterprise Linux host with a `GPDB.KRB` realm. The host name of the KDC server in this example is gpdb-kdc.

1.  Install the Kerberos server and client packages:

    ```
    $ sudo yum install krb5-libs krb5-server krb5-workstation
    ```

2.  Edit the /etc/krb5.conf configuration file. The following example shows a Kerberos server configured with a default `GPDB.KRB` realm.

    ```
    [logging]
     default = FILE:/var/log/krb5libs.log
     kdc = FILE:/var/log/krb5kdc.log
     admin_server = FILE:/var/log/kadmind.log
    
    [libdefaults]
     default_realm = GPDB.KRB
     dns_lookup_realm = false
     dns_lookup_kdc = false
     ticket_lifetime = 24h
     renew_lifetime = 7d
     forwardable = true
     default_tgs_enctypes = aes128-cts des3-hmac-sha1 des-cbc-crc des-cbc-md5
     default_tkt_enctypes = aes128-cts des3-hmac-sha1 des-cbc-crc des-cbc-md5
     permitted_enctypes = aes128-cts des3-hmac-sha1 des-cbc-crc des-cbc-md5
    
    [realms]
     GPDB.KRB = {
      kdc = gpdb-kdc:88
      admin_server = gpdb-kdc:749
      default_domain = gpdb.krb
     }
    
    [domain_realm]
     .gpdb.krb = GPDB.KRB
     gpdb.krb = GPDB.KRB
    
    [appdefaults]
     pam = {
        debug = false
        ticket_lifetime = 36000
        renew_lifetime = 36000
        forwardable = true
        krb4_convert = false
     }
    
    ```

    The `kdc` and `admin_server` keys in the `[realms]` section specify the host \(`gpdb-kdc`\) and port where the Kerberos server is running. IP numbers can be used in place of host names.

    If your Kerberos server manages authentication for other realms, you would instead add the `GPDB.KRB` realm in the `[realms]` and `[domain_realm]` section of the `kdc.conf` file. See the [Kerberos documentation](http://web.mit.edu/kerberos/krb5-latest/doc/) for information about the `kdc.conf` file.

3.  To create the Kerberos database, run the `kdb5_util`.

    ```
    # kdb5_util create -s
    ```

    The `kdb5_util` create command creates the database to store keys for the Kerberos realms that are managed by this KDC server. The -s option creates a stash file. Without the stash file, every time the KDC server starts it requests a password.

4.  Add an administrative user to the KDC database with the `kadmin.local` utility. Because it does not itself depend on Kerberos authentication, the `kadmin.local` utility allows you to add an initial administrative user to the local Kerberos server. To add the user `gpadmin` as an administrative user to the KDC database, run the following command:

    ```
    # kadmin.local -q "addprinc gpadmin/admin"
    ```

    Most users do not need administrative access to the Kerberos server. They can use `kadmin` to manage their own principals \(for example, to change their own password\). For information about `kadmin`, see the [Kerberos documentation](http://web.mit.edu/kerberos/krb5-latest/doc/).

5.  If needed, edit the /var/kerberos/krb5kdc/kadm5.acl file to grant the appropriate permissions to `gpadmin`.
6.  Start the Kerberos daemons:

    ```
    # /sbin/service krb5kdc start#
    /sbin/service kadmin start
    ```

7.  To start Kerberos automatically upon restart:

    ```
    # /sbin/chkconfig krb5kdc on
    # /sbin/chkconfig kadmin on
    ```


