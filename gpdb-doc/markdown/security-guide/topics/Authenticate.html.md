# Configuring Client Authentication 

Describes the available methods for authenticating Greenplum Database clients.

When a Greenplum Database system is first initialized, the system contains one predefined superuser role. This role will have the same name as the operating system user who initialized the Greenplum Database system. This role is referred to as gpadmin. By default, the system is configured to only allow local connections to the database from the gpadmin role. If you want to allow any other roles to connect, or if you want to allow connections from remote hosts, you have to configure Greenplum Database to allow such connections. This section explains how to configure client connections and authentication to Greenplum Database.

-   [Allowing Connections to Greenplum Database](#topic_ln1_ptd_jr)
-   [Editing the pg\_hba.conf File](#topic_xwr_rvd_jr)
-   [Authentication Methods](#topic_nyh_gwd_jr)
-   [Limiting Concurrent Connections](#topic_hwn_bk2_jr)
-   [Encrypting Client/Server Connections](#topic_ibc_nl2_jr)

**Parent topic:** [Greenplum Database Security Configuration Guide](../topics/preface.html)

## <a id="topic_ln1_ptd_jr"></a>Allowing Connections to Greenplum Database 

Client access and authentication is controlled by a configuration file named `pg_hba.conf` \(the standard PostgreSQL host-based authentication file\). For detailed information about this file, see [The pg\_hba.conf File](https://www.postgresql.org/docs/12/auth-pg-hba-conf.html) in the PostgreSQL documentation.

In Greenplum Database, the `pg_hba.conf` file of the coordinator instance controls client access and authentication to your Greenplum system. The segments also have `pg_hba.conf` files, but these are already correctly configured to only allow client connections from the coordinator host. The segments never accept outside client connections, so there is no need to alter the `pg_hba.conf` file on segments.

The general format of the `pg_hba.conf` file is a set of records, one per line. Blank lines are ignored, as is any text after a \# comment character. A record is made up of a number of fields which are separated by spaces and/or tabs. Fields can contain white space if the field value is quoted. Records cannot be continued across lines.

A record can have one of seven formats:

```
local      <database>  <user>  <auth-method>  [<auth-options>]
host       <database>  <user>  <address>  <auth-method>  [<auth-options>]
hostssl    <database>  <user>  <address>  <auth-method>  [<auth-options>]
hostnossl  <database>  <user>  <address>  <auth-method>  [<auth-options>]
host       <database>  <user>  <IP-address>  <IP-mask>  <auth-method>  [<auth-options>]
hostssl    <database>  <user>  <IP-address>  <IP-mask>  <auth-method>  [<auth-options>]
hostnossl  <database>  <user>  <IP-address>  <IP-mask>  <auth-method>  [<auth-options>]
```

The meaning of the `pg_hba.conf` fields is as follows:

`local`
:   Matches connection attempts using UNIX-domain sockets. Without a record of this type, UNIX-domain socket connections are disallowed.

`host`
:   Matches connection attempts made using TCP/IP. Remote TCP/IP connections will not be possible unless the server is started with an appropriate value for the `listen_addresses` server configuration parameter. Greenplum Database by default allows connections from all hosts \(`'*'`\).

`hostssl`
:   Matches connection attempts made using TCP/IP, but only when the connection is made with SSL encryption. SSL must be enabled at server start time by setting the `ssl` configuration parameter to on. Requires SSL authentication be configured in `postgresql.conf`. See [Configuring postgresql.conf for SSL Authentication](#ssl_postgresql).

`hostnossl`
:   Matches connection attempts made over TCP/IP that do not use SSL.

`database`
:   Specifies which database names this record matches. The value `all` specifies that it matches all databases. Multiple database names can be supplied by separating them with commas. A separate file containing database names can be specified by preceding the file name with `@`.

`user`
:   Specifies which database role names this record matches. The value `all` specifies that it matches all roles. If the specified role is a group and you want all members of that group to be included, precede the role name with a `+`. Multiple role names can be supplied by separating them with commas. A separate file containing role names can be specified by preceding the file name with `@`.

`address`
:   Specifies the client machine addresses that this record matches. This field can contain either a host name, an IP address range, or one of the special key words mentioned below.

:   An IP address range is specified using standard numeric notation for the range's starting address, then a slash \(`/`\) and a CIDR mask length. The mask length indicates the number of high-order bits of the client IP address that must match. Bits to the right of this should be zero in the given IP address. There must not be any white space between the IP address, the `/`, and the CIDR mask length.

:   Typical examples of an IPv4 address range specified this way are `172.20.143.89/32` for a single host, or `172.20.143.0/24` for a small network, or `10.6.0.0/16` for a larger one. An IPv6 address range might look like `::1/128` for a single host \(in this case the IPv6 loopback address\) or `fe80::7a31:c1ff:0000:0000/96` for a small network. `0.0.0.0/0` represents all IPv4 addresses, and `::0/0` represents all IPv6 addresses. To specify a single host, use a mask length of 32 for IPv4 or 128 for IPv6. In a network address, do not omit trailing zeroes.

:   An entry given in IPv4 format will match only IPv4 connections, and an entry given in IPv6 format will match only IPv6 connections, even if the represented address is in the IPv4-in-IPv6 range.

:   > **Note** Entries in IPv6 format will be rejected if the host system C library does not have support for IPv6 addresses.

:   You can also write `all` to match any IP address, `samehost` to match any of the server's own IP addresses, or `samenet` to match any address in any subnet to which the server is directly connected.

:   If a host name is specified \(an address that is not an IP address, IP range, or special key word is treated as a host name\), that name is compared with the result of a reverse name resolution of the client IP address \(for example, reverse DNS lookup, if DNS is used\). Host name comparisons are case insensitive. If there is a match, then a forward name resolution \(for example, forward DNS lookup\) is performed on the host name to check whether any of the addresses it resolves to are equal to the client IP address. If both directions match, then the entry is considered to match.

:   The host name that is used in `pg_hba.conf` should be the one that address-to-name resolution of the client's IP address returns, otherwise the line won't be matched. Some host name databases allow associating an IP address with multiple host names, but the operating system will only return one host name when asked to resolve an IP address.

:   A host name specification that starts with a dot \(.\) matches a suffix of the actual host name. So `.example.com` would match `foo.example.com` \(but not just `example.com`\).

:   When host names are specified in `pg_hba.conf`, you should ensure that name resolution is reasonably fast. It can be advantageous to set up a local name resolution cache such as `nscd`. Also, you can enable the server configuration parameter `log_hostname` to see the client host name instead of the IP address in the log.

`IP-address`
`IP-mask`
:   These two fields can be used as an alternative to the CIDR address notation. Instead of specifying the mask length, the actual mask is specified in a separate column. For example, `255.0.0.0` represents an IPv4 CIDR mask length of 8, and `255.255.255.255` represents a CIDR mask length of 32.

`auth-method`
:   Specifies the authentication method to use when a connection matches this record. See [Authentication Methods](#topic_nyh_gwd_jr) for options.

`auth-options`
:   After the `auth-method` field, there can be field\(s\) of the form `name=value` that specify options for the authentication method. Details about which options are available for which authentication methods are described in [Authentication Methods](#topic_nyh_gwd_jr).

Files included by @ constructs are read as lists of names, which can be separated by either whitespace or commas. Comments are introduced by \#, just as in `pg_hba.conf`, and nested @ constructs are allowed. Unless the file name following @ is an absolute path, it is taken to be relative to the directory containing the referencing file.

The `pg_hba.conf` records are examined sequentially for each connection attempt, so the order of the records is significant. Typically, earlier records will have tight connection match parameters and weaker authentication methods, while later records will have looser match parameters and stronger authentication methods. For example, you might wish to use `trust` authentication for local TCP/IP connections but require a password for remote TCP/IP connections. In this case a record specifying `trust` authentication for connections from 127.0.0.1 would appear before a record specifying `password` authentication for a wider range of allowed client IP addresses.

The `pg_hba.conf` file is read on start-up and when the main server process receives a SIGHUP signal. If you edit the file on an active system, you must reload the file using this command:

```
$ gpstop -u
```

**CAUTION:**  For a more secure system, remove records for remote connections that use `trust` authentication from the `pg_hba.conf` file. `trust` authentication grants any user who can connect to the server access to the database using any role they specify. You can safely replace `trust` authentication with `ident` authentication for local UNIX-socket connections. You can also use `ident` authentication for local and remote TCP clients, but the client host must be running an ident service and you must `trust` the integrity of that machine.

## <a id="topic_xwr_rvd_jr"></a>Editing the pg\_hba.conf File 

Initially, the `pg_hba.conf` file is set up with generous permissions for the gpadmin user and no database access for other Greenplum Database roles. You will need to edit the `pg_hba.conf` file to enable users' access to databases and to secure the gpadmin user. Consider removing entries that have `trust` authentication, since they allow anyone with access to the server to connect with any role they choose. For local \(UNIX socket\) connections, use `ident` authentication, which requires the operating system user to match the role specified. For local and remote TCP connections, `ident` authentication requires the client's host to run an indent service. You could install an ident service on the coordinator host and then use `ident` authentication for local TCP connections, for example `127.0.0.1/28`. Using `ident` authentication for remote TCP connections is less secure because it requires you to trust the integrity of the ident service on the client's host.

> **Note** Greenplum Command Center provides an interface for editing the `pg_hba.conf` file. It verifies entries before you save them, keeps a version history of the file so that you can reload a previous version of the file, and reloads the file into Greenplum Database.

This example shows how to edit the `pg_hba.conf` file on the coordinator host to allow remote client access to all databases from all roles using encrypted password authentication.

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


## <a id="topic_nyh_gwd_jr"></a>Authentication Methods 

-   [Basic Authentication](#basic_auth)
-   [GSSAPI Authentication](#kerberos_auth)
-   [LDAP Authentication](#ldap_auth)
-   [SSL Client Authentication](#topic_fzv_wb2_jr)
-   [PAM-Based Authentication](#topic_yxp_5h2_jr)
-   [Radius Authentication](#topic_ed4_d32_jr)

### <a id="basic_auth"></a>Basic Authentication 

Trust
:   Allows the connection unconditionally, without the need for a password or any other authentication. This entry is required for the `gpadmin` role, and for Greenplum utilities \(for example `gpinitsystem`, `gpstop`, or `gpstart` amongst others\) that need to connect between nodes without prompting for input or a password.

:   > **Important** For a more secure system, remove records for remote connections that use `trust` authentication from the `pg_hba.conf` file. `trust` authentication grants any user who can connect to the server access to the database using any role they specify. You can safely replace `trust` authentication with `ident` authentication for local UNIX-socket connections. You can also use `ident` authentication for local and remote TCP clients, but the client host must be running an ident service and you must `trust` the integrity of that machine.

Reject
:   Reject the connections with the matching parameters. You should typically use this to restrict access from specific hosts or insecure connections.

Ident
:   Authenticates based on the client's operating system user name. This is secure for local socket connections. Using `ident` for TCP connections from remote hosts requires that the client's host is running an ident service. The `ident` authentication method should only be used with remote hosts on a trusted, closed network.

md5
:   Require the client to supply a double-MD5-hashed password for authentication.

password
:   Require the client to supply an unencrypted password for authentication. Since the password is sent in clear text over the network, this should not be used on untrusted networks.

The password-based authentication methods are `md5` and `password`. These methods operate similarly except for the way that the password is sent across the connection: MD5-hashed and clear-text respectively.

If you are at all concerned about password "sniffing" attacks then `md5` is preferred. Plain `password` should always be avoided if possible. If the connection is protected by SSL encryption then `password` can be used safely \(although SSL certificate authentication might be a better choice if you are depending on using SSL\).

Following are some sample `pg_hba.conf` basic authentication entries:

```
hostnossl    all   all        0.0.0.0   reject
hostssl      all   testuser   0.0.0.0/0 md5
local        all   gpuser               ident
```

Or:

```

local    all           gpadmin         ident 
host     all           gpadmin         localhost      trust 
host     all           gpadmin         cdw            trust 
local    replication   gpadmin         ident 
host     replication   gpadmin         samenet       trust 
host     all           all             0.0.0.0/0     md5
```

### <a id="kerberos_auth"></a>GSSAPI Authentication 

GSSAPI is an industry-standard protocol for secure authentication defined in RFC 2743. Greenplum Database supports GSSAPI with Kerberos authentication according to RFC 1964. GSSAPI provides automatic authentication \(single sign-on\) for systems that support it. The authentication itself is secure, but the data sent over the database connection will be sent unencrypted unless SSL is used.

The `gss` authentication method is only available for TCP/IP connections.

When GSSAPI uses Kerberos, it uses a standard principal in the format `servicename/hostname@realm`. The Greenplum Database server will accept any principal that is included in the keytab file used by the server, but care needs to be taken to specify the correct principal details when making the connection from the client using the `krbsrvname` connection parameter. \(See [Connection Parameter Key Words](https://www.postgresql.org/docs/12/libpq-connect.html#LIBPQ-PARAMKEYWORDS) in the PostgreSQL documentation.\) In most environments, this parameter never needs to be changed. Some Kerberos implementations might require a different service name, such as Microsoft Active Directory, which requires the service name to be in upper case \(POSTGRES\).

`hostname` is the fully qualified host name of the server machine. The service principal's realm is the preferred realm of the server machine.

Client principals must have their Greenplum Database user name as their first component, for example `gpusername@realm`. Alternatively, you can use a user name mapping to map from the first component of the principal name to the database user name. By default, Greenplum Database does not check the realm of the client. If you have cross-realm authentication enabled and need to verify the realm, use the `krb_realm` parameter, or enable `include_realm` and use user name mapping to check the realm.

Make sure that your server keytab file is readable \(and preferably only readable\) by the `gpadmin` server account. The location of the key file is specified by the [krb\_server\_keyfile](../../ref_guide/config_params/guc-list.html) configuration parameter. For security reasons, it is recommended to use a separate keytab just for the Greenplum Database server rather than opening up permissions on the system keytab file.

The keytab file is generated by the Kerberos software; see the Kerberos documentation for details. The following example is for MIT-compatible Kerberos 5 implementations:

```
kadmin% **ank -randkey postgres/server.my.domain.org**
kadmin% **ktadd -k krb5.keytab postgres/server.my.domain.org**
```

When connecting to the database make sure you have a ticket for a principal matching the requested database user name. For example, for database user name `fred`, principal `fred@EXAMPLE.COM` would be able to connect. To also allow principal `fred/users.example.com@EXAMPLE.COM`, use a user name map, as described in [User Name Maps](https://www.postgresql.org/docs/12/auth-username-maps.html) in the PostgreSQL documentation.

The following configuration options are supported for GSSAPI:

`include_realm`
:   If set to 1, the realm name from the authenticated user principal is included in the system user name that is passed through user name mapping. This is the recommended configuration as, otherwise, it is impossible to differentiate users with the same username who are from different realms. The default for this parameter is 0 \(meaning to not include the realm in the system user name\) but may change to 1 in a future version of Greenplum Database. You can set it explicitly to avoid any issues when upgrading.

`map`
:   Allows for mapping between system and database user names. For a GSSAPI/Kerberos principal, such as `username@EXAMPLE.COM` \(or, less commonly, `username/hostbased@EXAMPLE.COM`\), the default user name used for mapping is `username` \(or `username/hostbased`, respectively\), unless `include_realm` has been set to 1 \(as recommended, see above\), in which case `username@EXAMPLE.COM` \(or `username/hostbased@EXAMPLE.COM`\) is what is seen as the system username when mapping.

`krb_realm`
:   Sets the realm to match user principal names against. If this parameter is set, only users of that realm will be accepted. If it is not set, users of any realm can connect, subject to whatever user name mapping is done.

### <a id="ldap_auth"></a>LDAP Authentication 

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

ldapsearchfilter
:   Beginning with Greenplum 6.22, this attribute enables you to provide a search filter to use when doing search+bind authentication. Occurrences of `$username` will be replaced with the user name. This allows for more flexible search filters than `ldapsearchattribute`. Note that you can specify _either_ `ldapsearchattribute` or `ldapsearchattribute`, but not both.

When using search+bind mode, the search can be performed using a single attribute specified with `ldapsearchattribute`, or using a custom search filter specified with `ldapsearchfilter`. Specifying `ldapsearchattribute=foo` is equivalent to specifying `ldapsearchfilter="(foo=$username)"`. If neither option is specified the default is `ldapsearchattribute=uid`.

Here is an example for a search+bind configuration that uses `ldapsearchfilter` instead of `ldapsearchattribute` to allow authentication by user ID or email address:

```
host ... ldap ldapserver=ldap.example.net ldapbasedn="dc=example, dc=net" ldapsearchfilter="(|(uid=$username)(mail=$username))"
```

Following are additional sample `pg_hba.conf` file entries for LDAP authentication:

```
host all testuser 0.0.0.0/0 ldap ldap
ldapserver=ldapserver.greenplum.com ldapport=389 ldapprefix="cn=" ldapsuffix=",ou=people,dc=greenplum,dc=com"
hostssl   all   ldaprole   0.0.0.0/0   ldap
ldapserver=ldapserver.greenplum.com ldaptls=1 ldapprefix="cn=" ldapsuffix=",ou=people,dc=greenplum,dc=com"
```

## <a id="topic_fzv_wb2_jr"></a>SSL Client Authentication 

SSL authentication compares the Common Name \(cn\) attribute of an SSL certificate provided by the connecting client during the SSL handshake to the requested database user name. The database user should exist in the database. A map file can be used for mapping between system and database user names.

### <a id="sslauth"></a>SSL Authentication Parameters 

Authentication method:

-   Cert

    Authentication options:

    Hostssl
    :   Connection type must be hostssl.

    map=mapping
    :   mapping.

    :   This is specified in the `pg_ident.conf`, or in the file specified in the `ident_file` server setting.

    Following are sample `pg_hba.conf` entries for SSL client authentication:

    ```
    Hostssl testdb certuser 192.168.0.0/16 cert
    Hostssl testdb all 192.168.0.0/16 cert map=gpuser
    
    ```


### <a id="openssl_config"></a>OpenSSL Configuration 

You can make changes to the OpenSSL configuration by updating the `openssl.cnf` file under your OpenSSL installation directory, or the file referenced by `$OPENSSL_CONF`, if present, and then restarting the Greenplum Database server.

### <a id="create_a_cert"></a>Creating a Self-Signed Certificate 

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

### <a id="ssl_postgresql"></a>Configuring postgresql.conf for SSL Authentication 

The following Server settings need to be specified in the `postgresql.conf` configuration file:

-   `ssl` *boolean*. Enables SSL connections.
-   `ssl_renegotiation_limit` *integer*. Specifies the data limit before key renegotiation.
-   `ssl_ciphers` *string*. Configures the list SSL ciphers that are allowed. `ssl_ciphers` *overrides* any ciphers string specified in `/etc/openssl.cnf`. The default value `ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH` enables all ciphers except for ADH, LOW, EXP, and MD5 ciphers, and prioritizes ciphers by their strength.

    <br/>> **Note** With TLS 1.2 some ciphers in MEDIUM and HIGH strength still use NULL encryption \(no encryption for transport\), which the default `ssl_ciphers` string allows. To bypass NULL ciphers with TLS 1.2 use a string such as `TLSv1.2:!eNULL:!aNULL`.

    It is possible to have authentication without encryption overhead by using `NULL-SHA` or `NULL-MD5` ciphers. However, a man-in-the-middle could read and pass communications between client and server. Also, encryption overhead is minimal compared to the overhead of authentication. For these reasons, NULL ciphers should not be used.


The default location for the following SSL server files is the Greenplum Database coordinator data directory \(`$MASTER_DATA_DIRECTORY`\):

-   `server.crt` - Server certificate.
-   `server.key` - Server private key.
-   `root.crt` - Trusted certificate authorities.
-   `root.crl` - Certificates revoked by certificate authorities.

If Greenplum Database coordinator mirroring is enabled with SSL client authentication, the SSL server files *should not be placed* in the default directory `$MASTER_DATA_DIRECTORY`. If a `gpinitstandby` operation is performed, the contents of `$MASTER_DATA_DIRECTORY` is copied from the coordinator to the standby coordinator and the incorrect SSL key, and cert files \(the coordinator files, and not the standby coordinator files\) will prevent standby coordinator start up.

You can specify a different directory for the location of the SSL server files with the `postgresql.conf` parameters `sslcert`, `sslkey`, `sslrootcert`, and `sslcrl`.

### <a id="config_ssl_client_conn"></a>Configuring the SSL Client Connection 

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
:   The file name of the client SSL certificate. The default is `$MASTER_DATA_DIRECTORY/postgresql.crt`.

sslkey
:   The secret key used for the client certificate. The default is `$MASTER_DATA_DIRECTORY/postgresql.key`.

sslrootcert
:   The name of a file containing SSL Certificate Authority certificate\(s\). The default is `$MASTER_DATA_DIRECTORY/root.crt`.

sslcrl
:   The name of the SSL certificate revocation list. The default is `$MASTER_DATA_DIRECTORY/root.crl`.

The client connection parameters can be set using the following environment variables:

-   `sslmode` – `PGSSLMODE`
-   `sslcert` – `PGSSLCERT`
-   `sslkey` – `PGSSLKEY`
-   `sslrootcert` – `PGSSLROOTCERT`
-   `sslcrl` – `PGSSLCRL` 

For example, run the following command to connect to the `postgres` database from `localhost` and verify the certificate present in the default location under `$MASTER_DATA_DIRECTORY`:

```
psql "sslmode=verify-ca host=localhost dbname=postgres"
```

## <a id="topic_yxp_5h2_jr"></a>PAM-Based Authentication 

The "PAM" \(Pluggable Authentication Modules\) authentication method validates username/password pairs, similar to basic authentication. To use PAM authentication, the user must already exist as a Greenplum Database role name.

Greenplum uses the `pamservice` authentication parameter to identify the service from which to obtain the PAM configuration.

> **Note** If PAM is set up to read `/etc/shadow`, authentication will fail because the PostgreSQL server is started by a non-root user. This is not an issue when PAM is configured to use LDAP or another authentication method.

Greenplum Database does not install a PAM configuration file. If you choose to use PAM authentication with Greenplum, you must identify the PAM service name for Greenplum and create the associated PAM service configuration file and configure Greenplum Database to use PAM authentication as described below:

1.  Log in to the Greenplum Database coordinator host and set up your environment. For example:

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


## <a id="topic_ed4_d32_jr"></a>Radius Authentication 

RADIUS \(Remote Authentication Dial In User Service\) authentication works by sending an Access Request message of type 'Authenticate Only' to a configured RADIUS server. It includes parameters for user name, password \(encrypted\), and the Network Access Server \(NAS\) Identifier. The request is encrypted using the shared secret specified in the `radiussecret` option. The RADIUS server responds with either `Access Accept` or `Access Reject`.

> **Note** RADIUS accounting is not supported.

RADIUS authentication only works if the users already exist in the database.

The RADIUS encryption vector requires SSL to be enabled in order to be cryptographically strong.

### <a id="radius"></a>RADIUS Authentication Options 

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
hostssl  all all 0.0.0.0/0 radius radiusserver=servername radiussecret=sharedsecret
```

## <a id="topic_hwn_bk2_jr"></a>Limiting Concurrent Connections 

To limit the number of active concurrent sessions to your Greenplum Database system, you can configure the `max_connections` server configuration parameter. This is a local parameter, meaning that you must set it in the `postgresql.conf` file of the coordinator, the standby coordinator, and each segment instance \(primary and mirror\). The value of `max_connections` on segments must be 5-10 times the value on the coordinator.

When you set `max_connections`, you must also set the dependent parameter `max_prepared_transactions`. This value must be at least as large as the value of `max_connections` on the coordinator, and segment instances should be set to the same value as the coordinator.

In `$MASTER_DATA_DIRECTORY/postgresql.conf` \(including standby coordinator\):

```
max_connections=100
max_prepared_transactions=100
```

In `SEGMENT_DATA_DIRECTORY/postgresql.conf` for all segment instances:

```
max_connections=500
max_prepared_transactions=100
```

> **Note** Raising the values of these parameters may cause Greenplum Database to request more shared memory. To mitigate this effect, consider decreasing other memory-related parameters such as `gp_cached_segworkers_threshold`.

To change the number of allowed connections:

1.  Stop your Greenplum Database system:

    ```
    $ gpstop
    ```

2.  On the coordinator host, edit `$MASTER_DATA_DIRECTORY/postgresql.conf` and change the following two parameters:
    -   `max_connections` – the number of active user sessions you want to allow plus the number of `superuser_reserved_connections`.
    -   `max_prepared_transactions` – must be greater than or equal to `max_connections`.
3.  On each segment instance, edit `SEGMENT_DATA_DIRECTORY/postgresql.conf` and change the following two parameters:
    -   `max_connections` – must be 5-10 times the value on the coordinator.
    -   `max_prepared_transactions` – must be equal to the value on the coordinator.
4.  Restart your Greenplum Database system:

    ```
    $ gpstart
    ```


## <a id="topic_ibc_nl2_jr"></a>Encrypting Client/Server Connections 

Greenplum Database has native support for SSL connections between the client and the coordinator server. SSL connections prevent third parties from snooping on the packets, and also prevent man-in-the-middle attacks. SSL should be used whenever the client connection goes through an insecure link, and must be used whenever client certificate authentication is used.

> **Note** For information about encrypting data between the `gpfdist` server and Greenplum Database segment hosts, see [Encrypting gpfdist Connections](Encryption.html).

To enable SSL requires that OpenSSL be installed on both the client and the coordinator server systems. Greenplum can be started with SSL enabled by setting the server configuration parameter `ssl=on` in the coordinator `postgresql.conf`. When starting in SSL mode, the server will look for the files `server.key` \(server private key\) and `server.crt` \(server certificate\) in the coordinator data directory. These files must be set up correctly before an SSL-enabled Greenplum system can start.

> **Important** Do not protect the private key with a passphrase. The server does not prompt for a passphrase for the private key, and the database startup fails with an error if one is required.

A self-signed certificate can be used for testing, but a certificate signed by a certificate authority \(CA\) should be used in production, so the client can verify the identity of the server. Either a global or local CA can be used. If all the clients are local to the organization, a local CA is recommended. See [Creating a Self-Signed Certificate](#create_a_cert) for steps to create a self-signed certificate.

