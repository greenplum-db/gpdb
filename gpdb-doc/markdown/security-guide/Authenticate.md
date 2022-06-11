# Configuring Client Authentication 

Describes the available methods for authenticating Greenplum Database clients.

When a Greenplum Database system is first initialized, the system contains one predefined superuser role. This role will have the same name as the operating system user who initialized the Greenplum Database system. This role is referred to as gpadmin. By default, the system is configured to only allow local connections to the database from the gpadmin role. If you want to allow any other roles to connect, or if you want to allow connections from remote hosts, you have to configure Greenplum Database to allow such connections. This section explains how to configure client connections and authentication to Greenplum Database.

-   [Allowing Connections to Greenplum Database](#topic_ln1_ptd_jr)
-   [Editing the pg\_hba.conf File](#topic_xwr_rvd_jr)
-   [Authentication Methods](#topic_nyh_gwd_jr)
-   [Limiting Concurrent Connections](#topic_hwn_bk2_jr)
-   [Encrypting Client/Server Connections](#topic_ibc_nl2_jr)

**Parent topic:** [Greenplum Database Security Configuration Guide](../topics/preface.html)

## Allowing Connections to Greenplum Database 

Client access and authentication is controlled by a configuration file named `pg_hba.conf` \(the standard PostgreSQL host-based authentication file\). For detailed information about this file, see [The pg\_hba.conf File](https://www.postgresql.org/docs/9.1/static/auth-pg-hba-conf.html) in the PostgreSQL documentation.

In Greenplum Database, the `pg_hba.conf` file of the master instance controls client access and authentication to your Greenplum system. The segments also have `pg_hba.conf` files, but these are already correctly configured to only allow client connections from the master host. The segments never accept outside client connections, so there is no need to alter the `pg_hba.conf` file on segments.

The general format of the `pg_hba.conf` file is a set of records, one per line. Blank lines are ignored, as is any text after a \# comment character. A record is made up of a number of fields which are separated by spaces and/or tabs. Fields can contain white space if the field value is quoted. Records cannot be continued across lines. Each remote client access record is in this format:

```
host   database   role   address   authentication-method

```

A UNIX-domain socket access record is in this format:

```
local   database   role   authentication-method

```

The meaning of the `pg_hba.conf` fields is as follows:

local
:   Matches connection attempts using UNIX-domain sockets. Without a record of this type, UNIX-domain socket connections are disallowed.

host
:   Matches connection attempts made using TCP/IP. Remote TCP/IP connections will not be possible unless the server is started with an appropriate value for the `listen_addresses` server configuration parameter.

hostssl
:   Matches connection attempts made using TCP/IP, but only when the connection is made with SSL encryption. SSL must be enabled at server start time by setting the `ssl` configuration parameter. Requires SSL authentication be configured in `postgresql.conf`. See [Configuring postgresql.conf for SSL Authentication](#ssl_postgresql).

hostnossl
:   Matches connection attempts made over TCP/IP that do not use SSL. Requires SSL authentication be configured in `postgresql.conf`. See [Configuring postgresql.conf for SSL Authentication](#ssl_postgresql).

database
:   Specifies which database names this record matches. The value `all` specifies that it matches all databases. Multiple database names can be supplied by separating them with commas. A separate file containing database names can be specified by preceding the file name with `@`.

role
:   Specifies which database role names this record matches. The value all specifies that it matches all roles. If the specified role is a group and you want all members of that group to be included, precede the role name with a `+`. Multiple role names can be supplied by separating them with commas. A separate file containing role names can be specified by preceding the file name with a `@`.

address
:   Specifies the client machine addresses that this record matches. This field can contain an IP address, an IP address range, or a host name.

:   An IP address range is specified using standard numeric notation for the range's starting address, then a slash \(`/`\) and a CIDR mask length. The mask length indicates the number of high-order bits of the client IP address that must match. Bits to the right of this should be zero in the given IP address. There must not be any white space between the IP address, the `/`, and the CIDR mask length.

:   Typical examples of an IPv4 address range specified this way are `172.20.143.89/32` for a single host, or `172.20.143.0/24` for a small network, or `10.6.0.0/16` for a larger one. An IPv6 address range might look like `::1/128` for a single host \(in this case the IPv6 loopback address\) or `fe80::7a31:c1ff:0000:0000/96` for a small network. `0.0.0.0/0` represents all IPv4 addresses, and `::0/0` represents all IPv6 addresses. To specify a single host, use a mask length of 32 for IPv4 or 128 for IPv6. In a network address, do not omit trailing zeroes.

:   An entry given in IPv4 format will match only IPv4 connections, and an entry given in IPv6 format will match only IPv6 connections, even if the represented address is in the IPv4-in-IPv6 range.

    **Note:** Entries in IPv6 format will be rejected if the host system C library does not have support for IPv6 addresses.

:   If a host name is specified \(an address that is not an IP address or IP range is treated as a host name\), that name is compared with the result of a reverse name resolution of the client IP address \(for example, reverse DNS lookup, if DNS is used\). Host name comparisons are case insensitive. If there is a match, then a forward name resolution \(for example, forward DNS lookup\) is performed on the host name to check whether any of the addresses it resolves to are equal to the client IP address. If both directions match, then the entry is considered to match.

:   Some host name databases allow associating an IP address with multiple host names, but the operating system only returns one host name when asked to resolve an IP address. The host name that is used in `pg_hba.conf` must be the one that the address-to-name resolution of the client IP address returns, otherwise the line will not be considered a match.

:   When host names are specified in `pg_hba.conf`, you should ensure that name resolution is reasonably fast. It can be of advantage to set up a local name resolution cache such as `nscd`. Also, you can enable the server configuration parameter `log_hostname` to see the client host name instead of the IP address in the log.

IP-address
IP-mask
:   These fields can be used as an alternative to the CIDR address notation. Instead of specifying the mask length, the actual mask is specified in a separate column. For example, `255.0.0.0` represents an IPv4 CIDR mask length of 8, and `255.255.255.255` represents a CIDR mask length of 32.

authentication-method
:   Specifies the authentication method to use when connecting. See [Authentication Methods](#topic_nyh_gwd_jr) for options.

CAUTION:

For a more secure system, consider removing records for remote connections that use trust authentication from the `pg_hba.conf` file. Trust authentication grants any user who can connect to the server access to the database using any role they specify. You can safely replace trust authentication with ident authentication for local UNIX-socket connections. You can also use ident authentication for local and remote TCP clients, but the client host must be running an ident service and you must trust the integrity of that machine.

## Editing the pg\_hba.conf File 

Initially, the `pg_hba.conf` file is set up with generous permissions for the gpadmin user and no database access for other Greenplum Database roles. You will need to edit the `pg_hba.conf` file to enable users' access to databases and to secure the gpadmin user. Consider removing entries that have trust authentication, since they allow anyone with access to the server to connect with any role they choose. For local \(UNIX socket\) connections, use ident authentication, which requires the operating system user to match the role specified. For local and remote TCP connections, ident authentication requires the client's host to run an indent service. You could install an ident service on the master host and then use ident authentication for local TCP connections, for example `127.0.0.1/28`. Using ident authentication for remote TCP connections is less secure because it requires you to trust the integrity of the ident service on the client's host.

This example shows how to edit the `pg_hba.conf` file of the master to allow remote client access to all databases from all roles using encrypted password authentication.

To edit `pg_hba.conf`:

1.  Open the file `$MASTER_DATA_DIRECTORY/pg_hba.conf` in a text editor.
2.  Add a line to the file for each type of connection you want to allow. Records are read sequentially, so the order of the records is significant. Typically, earlier records will have tight connection match parameters and weaker authentication methods, while later records will have looser match parameters and stronger authentication methods. For example:

    ```
    # allow the gpadmin user local access to all databases
    # using ident authentication
    local   all   gpadmin   ident         sameuser
    host    all   gpadmin   127.0.0.1/32  ident
    host    all   gpadmin   ::1/128       ident
    # allow the 'dba' role access to any database from any
    # host with IP address 192.168.x.x and use md5 encrypted
    # passwords to authenticate the user
    # Note that to use SHA-256 encryption, replace md5 with
    # password in the line below
    host    all   dba   192.168.0.0/32  md5
    
    ```


## Authentication Methods 

-   [Basic Authentication](#basic_auth)
-   [Kerberos Authentication](#kerberos_auth)
-   [LDAP Authentication](#ldap_auth)
-   [SSL Client Authentication](#topic_fzv_wb2_jr)
-   [PAM-Based Authentication](#topic_yxp_5h2_jr)
-   [Radius Authentication](#topic_ed4_d32_jr)

### Basic Authentication 

The following basic authentication methods are supported:

Password or MD5
:   Requires clients to provide a password, one of either:

    -   Md5 – password transmitted as an MD5 hash.
    -   Password – A password transmitted in clear text. Always use SSL connections to prevent password sniffing during transit. This is configurable, see "Encrypting Passwords" in the *Greenplum Database Administrator Guide* for more information.

Reject
:   Reject the connections with the matching parameters. You should typically use this to restrict access from specific hosts or insecure connections.

Ident
:   Authenticates based on the client's operating system user name. This is secure for local socket connections. Using ident for TCP connections from remote hosts requires that the client's host is running an ident service. The ident authentication method should only be used with remote hosts on a trusted, closed network.

Following are some sample `pg_hba.conf` basic authentication entries:

```
hostnossl    all   all        0.0.0.0   reject
hostssl      all   testuser   0.0.0.0/0 md5
local        all   gpuser               ident

```

### Kerberos Authentication 

You can authenticate against a Kerberos server \(RFC 2743, 1964\).

The format for Kerberos authentication in the `pg_hba.conf` file is:

```
servicename/hostname@realm
```

The following options may be added to the entry:

Map
:   Map system and database users.

Include\_realm
:   Option to specify realm name included in the system-user name in the ident map file.

Krb\_realm
:   Specify the realm name for matching the principals.

Krb\_server\_hostname
:   The hostname of the service principal.

Following is an example `pg_hba.conf` entry for Kerberos:

```
host    all all 0.0.0.0/0   krb5
hostssl all all 0.0.0.0/0   krb5 map=krbmap

```

The following Kerberos server settings are specified in `postgresql.conf`:

krb\_server\_key *file*
:   Sets the location of the Kerberos server key file.

krb\_srvname *string*
:   Kerberos service name.

krb\_caseins\_users *boolean*
:   Case-sensitivity. The default is off.

The following client setting is specified as a connection parameter:

`Krbsrvname`
:   The Kerberos service name to use for authentication.

### LDAP Authentication 

You can authenticate against an LDAP directory.

-   LDAPS and LDAP over TLS options encrypt the connection to the LDAP server.
-   The connection from the client to the server is not encrypted unless SSL is enabled. Configure client connections to use SSL to encrypt connections from the client.
-   To configure or customize LDAP settings, set the `LDAPCONF` environment variable with the path to the `ldap.conf` file and add this to the `greenplum_path.sh` script.

Following are the recommended steps for configuring your system for LDAP authentication:

1.   Set up the LDAP server with the database users/roles to be authenticated via LDAP.
2.  On the database:
    1.  Verify that the database users to be authenticated via LDAP exist on the database. LDAP is only used for verifying username/password pairs, so the roles should exist in the database.
    2.  Update the `pg_hba.conf` file in the `$MASTER_DATA_DIRECTORY` to use LDAP as the authentication method for the respective users. Note that the first entry to match the user/role in the `pg_hba.conf` file will be used as the authentication mechanism, so the position of the entry in the file is important.
    3.  Reload the server for the `pg_hba.conf` configuration settings to take effect \(`gpstop -u`\).

Specify the following parameter `auth-options`.

ldapserver
:   Names or IP addresses of LDAP servers to connect to. Multiple servers may be specified, separated by spaces.

ldapprefix
:   String to prepend to the user name when forming the DN to bind as, when doing simple bind authentication.

ldapsuffix
:   String to append to the user name when forming the DN to bind as, when doing simple bind authentication.

ldapport
:   Port number on LDAP server to connect to. If no port is specified, the LDAP library's default port setting will be used.

ldaptls
:   Set to 1 to make the connection between PostgreSQL and the LDAP server use TLS encryption. Note that this only encrypts the traffic to the LDAP server — the connection to the client will still be unencrypted unless SSL is used.

ldapbasedn
:   Root DN to begin the search for the user in, when doing search+bind authentication.

ldapbinddn
:   DN of user to bind to the directory with to perform the search when doing search+bind authentication.

ldapbindpasswd
:   Password for user to bind to the directory with to perform the search when doing search+bind authentication.

ldapsearchattribute
:   Attribute to match against the user name in the search when doing search+bind authentication.

Example:

```
ldapserver=ldap.greenplum.com prefix="cn=" suffix=", dc=greenplum, dc=com"
```

Following are sample `pg_hba.conf` file entries for LDAP authentication:

```
host all testuser 0.0.0.0/0 ldap ldap
ldapserver=ldapserver.greenplum.com ldapport=389 ldapprefix="cn=" ldapsuffix=",ou=people,dc=greenplum,dc=com"
hostssl   all   ldaprole   0.0.0.0/0   ldap
ldapserver=ldapserver.greenplum.com ldaptls=1 ldapprefix="cn=" ldapsuffix=",ou=people,dc=greenplum,dc=com"          
        
```

## SSL Client Authentication 

SSL authentication compares the Common Name \(cn\) attribute of an SSL certificate provided by the connecting client during the SSL handshake to the requested database user name. The database user should exist in the database. A map file can be used for mapping between system and database user names.

### SSL Authentication Parameters 

Authentication method:

Cert

Authentication options:

:    Hostssl - Connection type must be hostssl.

:    map=mapping - mapping.

    This is specified in the `pg_ident.conf`, or in the file specified in the `ident_file` server setting.

    Following are sample `pg_hba.conf` entries for SSL client authentication:

    ```
    Hostssl testdb certuser 192.168.0.0/16 cert
    Hostssl testdb all 192.168.0.0/16 cert map=gpuser
    
    ```


### OpenSSL Configuration 

Greenplum Database reads the OpenSSL configuration file specified in `$GP_HOME/etc/openssl.cnf` by default. You can make changes to the default configuration for OpenSSL by modifying or updating this file and restarting the server.

### Creating a Self-Signed Certificate 

A self-signed certificate can be used for testing, but a certificate signed by a certificate authority \(CA\) \(either one of the global CAs or a local one\) should be used in production so that clients can verify the server's identity. If all the clients are local to the organization, using a local CA is recommended.

To create a self-signed certificate for the server:

1.  Enter the following `openssl` command:

    ```
    openssl req -new -text -out server.req
    ```

2.  Enter the requested information at the prompts.

    Make sure you enter the local host name for the Common Name. The challenge password can be left blank.

3.  The program generates a key that is passphrase-protected; it does not accept a passphrase that is less than four characters long. To remove the passphrase \(and you must if you want automatic start-up of the server\), run the following command:

    ```
    openssl rsa -in privkey.pem -out server.key
    rm privkey.pem
    ```

4.  Enter the old passphrase to unlock the existing key. Then run the following command:

    ```
    openssl req -x509 -in server.req -text -key server.key -out server.crt
    ```

    This turns the certificate into a self-signed certificate and copies the key and certificate to where the server will look for them.

5.  Finally, run the following command:

    ```
    chmod og-rwx server.key
    ```


For more details on how to create your server private key and certificate, refer to the OpenSSL documentation.

### Configuring postgresql.conf for SSL Authentication 

The following Server settings need to be specified in the `postgresql.conf` configuration file:

-   `ssl` *boolean*. Enables SSL connections.
-   `ssl_renegotiation_limit` *integer*. Specifies the data limit before key renegotiation.
-   `ssl_ciphers` *string*. Lists SSL ciphers that are allowed.

The following SSL server files can be found in the Master Data Directory:

-   `server.crt`. Server certificate.
-   `server.key`. Server private key.
-   `root.crt`. Trusted certificate authorities.
-   `root.crl`. Certificates revoked by certificate authorities.

### Configuring the SSL Client Connection 

SSL options:

sslmode
:   Specifies the level of protection.

    `require`
    :   Only use an SSL connection. If a root CA file is present, verify the certificate in the same way as if `verify-ca` was specified.

    `verify-ca`
    :   Only use an SSL connection. Verify that the server certificate is issued by a trusted CA.

    `verify-full`
    :   Only use an SSL connection. Verify that the server certificate is issued by a trusted CA and that the server host name matches that in the certificate.

sslcert
:   The file name of the client SSL certificate. The default is `~/.postgresql/postgresql.crt`.

sslkey
:   The secret key used for the client certificate. The default is `~/.postgresql/postgresql.key`.

sslrootcert
:   The name of a file containing SSL Certificate Authority certificate\(s\). The default is `~/.postgresql/root.crt`.

sslcrl
:   The name of the SSL certificate revocation list. The default is `~/.postgresql/root.crl`.

The client connection parameters can be set using the following environment variables:

-   `sslmode` – `PGSSLMODE`
-   `sslkey` – `PGSSLKEY`
-   `sslrootcert` – `PGSSLROOTCERT`
-   `sslcert` – `PGSSLCERT`
-   `sslcrl` – `PGSSLCRL` 

## PAM-Based Authentication 

The "PAM" \(Pluggable Authentication Modules\) authentication method validates username/password pairs, similar to basic authentication. To use PAM authentication, the user must already exist as a Greenplum Database role name.

Greenplum uses the `pamservice` authentication parameter to identify the service from which to obtain the PAM configuration.

**Note:** If PAM is set up to read `/etc/shadow`, authentication will fail because the PostgreSQL server is started by a non-root user. This is not an issue when PAM is configured to use LDAP or another authentication method.

Greenplum Database does not install a PAM configuration file. If you choose to use PAM authentication with Greenplum, you must identify the PAM service name for Greenplum and create the associated PAM service configuration file and configure Greenplum Database to use PAM authentication as described below:

1.  Log in to the Greenplum Database master host and set up your environment. For example:

    ```
    $ ssh gpadmin@<gpmaster>
    gpadmin@gpmaster$ . /usr/local/greenplum-db/greenplum_path.sh
    ```

2.  Identify the `pamservice` name for Greenplum Database. In this procedure, we choose the name `greenplum`.
3.  Create the PAM service configuration file, `/etc/pam.d/greenplum`, and add the text below. You must have operating system superuser privileges to create the `/etc/pam.d` directory \(if necessary\) and the `greenplum` PAM configuration file.

    ```
    #%PAM-1.0
    auth		include		password-auth
    account		include		password-auth
    
    ```

    This configuration instructs PAM to authenticate the local operating system user.

4.  Ensure that the `/etc/pam.d/greenplum` file is readable by all users:

    ```
    sudo chmod 644 /etc/pam.d/greenplum
    ```

5.  Add one or more entries to the `pg_hba.conf` configuration file to enable PAM authentication in Greenplum Database. These entries must specify the `pam` *auth-method*. You must also specify the `pamservice=greenplum` *auth-option*. For example:

    ```
    
    host     <user-name>     <db-name>     <address>     pam     pamservice=greenplum
    
    ```

6.  Reload the Greenplum Database configuration:

    ```
    $ gpstop -u
    ```


## Radius Authentication 

RADIUS \(Remote Authentication Dial In User Service\) authentication works by sending an Access Request message of type 'Authenticate Only' to a configured RADIUS server. It includes parameters for user name, password \(encrypted\), and the Network Access Server \(NAS\) Identifier. The request is encrypted using the shared secret specified in the `radiussecret` option. The RADIUS server responds with either `Access Accept` or `Access Reject`.

**Note:** RADIUS accounting is not supported.

RADIUS authentication only works if the users already exist in the database.

The RADIUS encryption vector requires SSL to be enabled in order to be cryptographically strong.

### RADIUS Authentication Options 

radiusserver
:   The name of the RADIUS server.

radiussecret
:   The RADIUS shared secret.

radiusport
:   The port to connect to on the RADIUS server.

radiusidentifier
:   NAS identifier in RADIUS requests.

Following are sample `pg_hba.conf` entries for RADIUS client authentication:

```
hostssl  all all 0.0.0.0/0 radius radiusserver=servername radiussecret=<sharedsecret>
```

## Limiting Concurrent Connections 

To limit the number of active concurrent sessions to your Greenplum Database system, you can configure the `max_connections` server configuration parameter. This is a local parameter, meaning that you must set it in the `postgresql.conf` file of the master, the standby master, and each segment instance \(primary and mirror\). The value of `max_connections` on segments must be 5-10 times the value on the master.

When you set `max_connections`, you must also set the dependent parameter `max_prepared_transactions`. This value must be at least as large as the value of `max_connections` on the master, and segment instances should be set to the same value as the master.

In `$MASTER_DATA_DIRECTORY/postgresql.conf` \(including standby master\):

```
max_connections=100
max_prepared_transactions=100
```

In `SEGMENT_DATA_DIRECTORY/postgresql.conf` for all segment instances:

```
max_connections=500
max_prepared_transactions=100
```

**Note:** Raising the values of these parameters may cause Greenplum Database to request more shared memory. To mitigate this effect, consider decreasing other memory-related parameters such as `gp_cached_segworkers_threshold`.

To change the number of allowed connections:

1.  Stop your Greenplum Database system:

    ```
    $ gpstop
    ```

2.  On the master host, edit `$MASTER_DATA_DIRECTORY/postgresql.conf` and change the following two parameters:
    -   `max_connections` – the number of active user sessions you want to allow plus the number of `superuser_reserved_connections`.
    -   `max_prepared_transactions` – must be greater than or equal to `max_connections`.
3.  On each segment instance, edit `SEGMENT_DATA_DIRECTORY/postgresql.conf` and change the following two parameters:
    -   `max_connections` – must be 5-10 times the value on the master.
    -   `max_prepared_transactions` – must be equal to the value on the master.
4.  Restart your Greenplum Database system:

    ```
    $ gpstart
    ```


## Encrypting Client/Server Connections 

Greenplum Database has native support for SSL connections between the client and the master server. SSL connections prevent third parties from snooping on the packets, and also prevent man-in-the-middle attacks. SSL should be used whenever the client connection goes through an insecure link, and must be used whenever client certificate authentication is used.

**Note:** For information about encrypting data between the `gpfdist` server and Greenplum Database segment hosts, see [Encrypting gpfdist Connections](Encryption.html).

To enable SSL requires that OpenSSL be installed on both the client and the master server systems. Greenplum can be started with SSL enabled by setting the server configuration parameter `ssl=on` in the master `postgresql.conf`. When starting in SSL mode, the server will look for the files `server.key` \(server private key\) and `server.crt` \(server certificate\) in the master data directory. These files must be set up correctly before an SSL-enabled Greenplum system can start.

**Important:** Do not protect the private key with a passphrase. The server does not prompt for a passphrase for the private key, and the database startup fails with an error if one is required.

A self-signed certificate can be used for testing, but a certificate signed by a certificate authority \(CA\) should be used in production, so the client can verify the identity of the server. Either a global or local CA can be used. If all the clients are local to the organization, a local CA is recommended. See [Creating a Self-Signed Certificate](#create_a_cert) for steps to create a self-signed certificate.

