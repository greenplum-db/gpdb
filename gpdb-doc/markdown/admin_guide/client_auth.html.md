---
title: Configuring Client Authentication 
---

This topic explains how to configure client connections and authentication for Greenplum Database.

When a Greenplum Database system is first initialized, the system contains one predefined *superuser* role. This role will have the same name as the operating system user who initialized the Greenplum Database system. This role is referred to as `gpadmin`. By default, the system is configured to only allow local connections to the database from the `gpadmin` role. If you want to allow any other roles to connect, or if you want to allow connections from remote hosts, you have to configure Greenplum Database to allow such connections. This section explains how to configure client connections and authentication to Greenplum Database.

-   **[Using LDAP Authentication with TLS/SSL](ldap.html)**  
You can control access to Greenplum Database with an LDAP server and, optionally, secure the connection with encryption by adding parameters to pg\_hba.conf file entries.
-   **[Using Kerberos Authentication](kerberos.html)**  
You can control access to Greenplum Database with a Kerberos authentication server.
-   **[Configuring Kerberos for Linux Clients](kerberos-lin-client.html)**  
You can configure Linux client applications to connect to a Greenplum Database system that is configured to authenticate with Kerberos.
-   **[Configuring Kerberos For Windows Clients](kerberos-win-client.html)**  
You can configure Microsoft Windows client applications to connect to a Greenplum Database system that is configured to authenticate with Kerberos.

**Parent topic:** [Managing Greenplum Database Access](partIII.html)

## <a id="topic2"></a>Allowing Connections to Greenplum Database 

Client access and authentication is controlled by the standard PostgreSQL host-based authentication file, pg\_hba.conf. For detailed information about this file, see [The pg\_hba.conf File](https://www.postgresql.org/docs/12/auth-pg-hba-conf.html) in the PostgreSQL documentation.

In Greenplum Database, the pg\_hba.conf file of the coordinator instance controls client access and authentication to your Greenplum Database system. The Greenplum Database segments also have pg\_hba.conf files, but these are already correctly configured to allow only client connections from the coordinator host. The segments never accept outside client connections, so there is no need to alter the `pg_hba.conf` file on segments.

The general format of the pg\_hba.conf file is a set of records, one per line. Greenplum Database ignores blank lines and any text after the `#` comment character. A record consists of a number of fields that are separated by spaces or tabs. Fields can contain white space if the field value is quoted. Records cannot be continued across lines. Each remote client access record has the following format:

```
host   database   role   address   authentication-method
```

Each UNIX-domain socket access record is in this format:

```
local   database   role   authentication-method
```

The following table describes meaning of each field.

<table class="table" id="topic2__ip141709"><caption><span class="table--title-label">Table 1. </span><span class="title">pg_hba.conf Fields</span></caption><colgroup><col style="width:25%"><col style="width:75%"></colgroup><thead class="thead">
            <tr class="row">
              <th class="entry" id="topic2__ip141709__entry__1">Field</th>
              <th class="entry" id="topic2__ip141709__entry__2">Description</th>
            </tr>
          </thead><tbody class="tbody">
            <tr class="row">
              <td class="entry" headers="topic2__ip141709__entry__1">local</td>
              <td class="entry" headers="topic2__ip141709__entry__2">Matches connection attempts using UNIX-domain sockets. Without a
                record of this type, UNIX-domain socket connections are disallowed.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic2__ip141709__entry__1">host</td>
              <td class="entry" headers="topic2__ip141709__entry__2">Matches connection attempts made using TCP/IP. Remote TCP/IP
                connections will not be possible unless the server is started with an appropriate
                value for the <code class="ph codeph">listen_addresses</code> server configuration
                parameter.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic2__ip141709__entry__1">hostssl</td>
              <td class="entry" headers="topic2__ip141709__entry__2">Matches connection attempts made using TCP/IP, but only when the
                connection is made with SSL encryption. SSL must be enabled at server start time by
                setting the <code class="ph codeph">ssl</code> server configuration parameter. </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic2__ip141709__entry__1">hostnossl</td>
              <td class="entry" headers="topic2__ip141709__entry__2">Matches connection attempts made over TCP/IP that do not use
                SSL.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic2__ip141709__entry__1">database</td>
              <td class="entry" headers="topic2__ip141709__entry__2">Specifies which database names this record matches. The value
                  <code class="ph codeph">all</code> specifies that it matches all databases. Multiple database
                names can be supplied by separating them with commas. A separate file containing
                database names can be specified by preceding the file name with a
                <code class="ph codeph">@</code>.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic2__ip141709__entry__1">role</td>
              <td class="entry" headers="topic2__ip141709__entry__2">Specifies which database role names this record matches. The
                value <code class="ph codeph">all</code> specifies that it matches all roles. If the specified
                role is a group and you want all members of that group to be included, precede the
                role name with a <code class="ph codeph">+</code>. Multiple role names can be supplied by
                separating them with commas. A separate file containing role names can be specified
                by preceding the file name with a <code class="ph codeph">@</code>.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic2__ip141709__entry__1">address</td>
              <td class="entry" headers="topic2__ip141709__entry__2">Specifies the client machine addresses that this record matches.
                This field can contain an IP address, an IP address range, or a host name. <p class="p">An IP
                  address range is specified using standard numeric notation for the range's
                  starting address, then a slash (<code class="ph codeph">/</code>) and a CIDR mask length. The
                  mask length indicates the number of high-order bits of the client IP address that
                  must match. Bits to the right of this should be zero in the given IP address.
                  There must not be any white space between the IP address, the <code class="ph codeph">/</code>,
                  and the CIDR mask length.</p><p class="p">Typical examples of an IPv4 address range
                  specified this way are <code class="ph codeph">172.20.143.89/32</code> for a single host, or
                    <code class="ph codeph">172.20.143.0/24</code> for a small network, or
                    <code class="ph codeph">10.6.0.0/16</code> for a larger one. An IPv6 address range might look
                  like <code class="ph codeph">::1/128</code> for a single host (in this case the IPv6 loopback
                  address) or <code class="ph codeph">fe80::7a31:c1ff:0000:0000/96</code> for a small network.
                    <code class="ph codeph">0.0.0.0/0</code> represents all IPv4 addresses, and
                    <code class="ph codeph">::0/0</code> represents all IPv6 addresses. To specify a single host,
                  use a mask length of 32 for IPv4 or 128 for IPv6. In a network address, do not
                  omit trailing zeroes.</p><div class="p">An entry given in IPv4 format will match only IPv4
                  connections, and an entry given in IPv6 format will match only IPv6 connections,
                  even if the represented address is in the IPv4-in-IPv6 range.
                  <div class="note note note_note"><span class="note__title">Note:</span> Entries in IPv6 format will be rejected if the host system C library does
                    not have support for IPv6 addresses.</div></div><p class="p">If a host name is specified
                  (an address that is not an IP address or IP range is treated as a host name), that
                  name is compared with the result of a reverse name resolution of the client IP
                  address (for example, reverse DNS lookup, if DNS is used). Host name comparisons
                  are case insensitive. If there is a match, then a forward name resolution (for
                  example, forward DNS lookup) is performed on the host name to check whether any of
                  the addresses it resolves to are equal to the client IP address. If both
                  directions match, then the entry is considered to match. </p><p class="p">Some host name
                  databases allow associating an IP address with multiple host names, but the
                  operating system only returns one host name when asked to resolve an IP address.
                  The host name that is used in <code class="ph codeph">pg_hba.conf</code> must be the one that
                  the address-to-name resolution of the client IP address returns, otherwise the
                  line will not be considered a match. </p><p class="p">When host names are specified in
                    <code class="ph codeph">pg_hba.conf</code>, you should ensure that name resolution is
                  reasonably fast. It can be of advantage to set up a local name resolution cache
                  such as <code class="ph codeph">nscd</code>. Also, you can enable the server configuration
                  parameter <code class="ph codeph">log_hostname</code> to see the client host name instead of the
                  IP address in the log. </p></td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic2__ip141709__entry__1">IP-address<p class="p">IP-mask</p></td>
              <td class="entry" headers="topic2__ip141709__entry__2">These fields can be used as an alternative to the CIDR address
                notation. Instead of specifying the mask length, the actual mask is specified in a
                separate column. For example, <code class="ph codeph">255.0.0.0</code> represents an IPv4 CIDR
                mask length of 8, and <code class="ph codeph">255.255.255.255</code> represents a CIDR mask length
                of 32.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic2__ip141709__entry__1">authentication-method</td>
              <td class="entry" headers="topic2__ip141709__entry__2">Specifies the authentication method to use when connecting.
                Greenplum supports the <a class="xref" href="https://www.postgresql.org/docs/12/auth-methods.html" target="_blank" rel="external noopener">authentication methods</a> supported by PostgreSQL 9.4.</td>
            </tr>
          </tbody></table>

**CAUTION:** For a more secure system, consider removing records for remote connections that use trust authentication from the `pg_hba.conf` file. Trust authentication grants any user who can connect to the server access to the database using any role they specify. You can safely replace trust authentication with ident authentication for local UNIX-socket connections. You can also use ident authentication for local and remote TCP clients, but the client host must be running an ident service and you must trust the integrity of that machine.

### <a id="topic3"></a>Editing the pg\_hba.conf File 

Initially, the `pg_hba.conf` file is set up with generous permissions for the gpadmin user and no database access for other Greenplum Database roles. You will need to edit the `pg_hba.conf` file to enable users' access to databases and to secure the gpadmin user. Consider removing entries that have trust authentication, since they allow anyone with access to the server to connect with any role they choose. For local \(UNIX socket\) connections, use ident authentication, which requires the operating system user to match the role specified. For local and remote TCP connections, ident authentication requires the client's host to run an indent service. You can install an ident service on the coordinator host and then use ident authentication for local TCP connections, for example `127.0.0.1/28`. Using ident authentication for remote TCP connections is less secure because it requires you to trust the integrity of the ident service on the client's host.

This example shows how to edit the pg\_hba.conf file of the coordinator to allow remote client access to all databases from all roles using encrypted password authentication.

#### <a id="ip144328"></a>Editing pg\_hba.conf 

1.  Open the file $COORDINATOR\_DATA\_DIRECTORY/pg\_hba.conf in a text editor.
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
    # allow all roles access to any database from any
    # host and use ldap to authenticate the user. Greenplum role
    # names must match the LDAP common name.
    host    all   all   192.168.0.0/32  ldap ldapserver=usldap1 ldapport=1389 ldapprefix="cn=" ldapsuffix=",ou=People,dc=company,dc=com"
    ```

3.  Save and close the file.
4.  Reload the pg\_hba.conf configuration file for your changes to take effect:

    ```
    $ gpstop -u
    ```


> **Note** Note that you can also control database access by setting object privileges as described in [Managing Object Privileges](roles_privs.html). The pg\_hba.conf file just controls who can initiate a database session and how those connections are authenticated.

## <a id="topic4"></a>Limiting Concurrent Connections 

Greenplum Database allocates some resources on a per-connection basis, so setting the maximum number of connections allowed is recommended.

To limit the number of active concurrent sessions to your Greenplum Database system, you can configure the `max_connections` server configuration parameter. This is a *local* parameter, meaning that you must set it in the `postgresql.conf` file of the coordinator, the standby coordinator, and each segment instance \(primary and mirror\). The recommended value of `max_connections` on segments is 5-10 times the value on the coordinator.

When you set `max_connections`, you must also set the dependent parameter `max_prepared_transactions`. This value must be at least as large as the value of `max_connections` on the coordinator, and segment instances should be set to the same value as the coordinator.

For example:

-   In `$COORDINATOR_DATA_DIRECTORY/postgresql.conf` \(including standby coordinator\):

    ```
    max_connections=100
    max_prepared_transactions=100
    
    ```

-   In `SEGMENT_DATA_DIRECTORY/postgresql.conf` for all segment instances:

    ```
    max_connections=500
    max_prepared_transactions=100
    
    ```


The following steps set the parameter values with the Greenplum Database utility `gpconfig`.

For information about `gpconfig`, see the *Greenplum Database Utility Guide*.

### <a id="ip142411"></a>To change the number of allowed connections 

1.  Log into the Greenplum Database coordinator host as the Greenplum Database administrator and source the file `$GPHOME/greenplum_path.sh`.
2.  Set the value of the `max_connections` parameter. This `gpconfig` command sets the value on the segments to 1000 and the value on the coordinator to 200.

    ```
    $ gpconfig -c max_connections -v 1000 -m 200
    
    ```

    The value on the segments must be greater than the value on the coordinator. The recommended value of `max_connections` on segments is 5-10 times the value on the coordinator.

3.  Set the value of the `max_prepared_transactions` parameter. This `gpconfig` command sets the value to 200 on the coordinator and all segments.

    ```
    $ gpconfig -c max_prepared_transactions -v 200
    
    ```

    The value of `max_prepared_transactions` must be greater than or equal to `max_connections` on the coordinator.

4.  Stop and restart your Greenplum Database system.

    ```
    $ gpstop -r
    
    ```

5.  You can check the value of parameters on the coordinator and segments with the `gpconfig` `-s` option. This `gpconfig` command displays the values of the `max_connections` parameter.

    ```
    $ gpconfig -s max_connections
    
    ```


> **Note** Raising the values of these parameters may cause Greenplum Database to request more shared memory. To mitigate this effect, consider decreasing other memory-related parameters such as `gp_cached_segworkers_threshold`.

## <a id="topic5"></a>Encrypting Client/Server Connections 

Enable SSL for client connections to Greenplum Database to encrypt the data passed over the network between the client and the database.

Greenplum Database has native support for SSL connections between the client and the coordinator server. SSL connections prevent third parties from snooping on the packets, and also prevent man-in-the-middle attacks. SSL should be used whenever the client connection goes through an insecure link, and must be used whenever client certificate authentication is used.

Enabling Greenplum Database in SSL mode requires the following items.

-   OpenSSL installed on both the client and the coordinator server hosts \(coordinator and standby coordinator\).
-   The SSL files server.key \(server private key\) and server.crt \(server certificate\) should be correctly generated for the coordinator host and standby coordinator host.

    -   The private key should not be protected with a passphrase. The server does not prompt for a passphrase for the private key, and Greenplum Database start up fails with an error if one is required.
    -   On a production system, there should be a key and certificate pair for the coordinator host and a pair for the standby coordinator host with a subject CN \(Common Name\) for the coordinator host and standby coordinator host.
    A self-signed certificate can be used for testing, but a certificate signed by a certificate authority \(CA\) should be used in production, so the client can verify the identity of the server. Either a global or local CA can be used. If all the clients are local to the organization, a local CA is recommended.

-   Ensure that Greenplum Database can access server.key and server.crt, and any additional authentication files such as `root.crt` \(for trusted certificate authorities\). When starting in SSL mode, the Greenplum Database coordinator looks for server.key and server.crt. As the default, Greenplum Database does not start if the files are not in the coordinator data directory \(`$COORDINATOR_DATA_DIRECTORY`\). Also, if you use other SSL authentication files such as `root.crt` \(trusted certificate authorities\), the files must be on the coordinator host.

    If Greenplum Database coordinator mirroring is enabled with SSL client authentication, SSL authentication files must be on both the coordinator host and standby coordinator host and *should not be placed* in the default directory `$COORDINATOR_DATA_DIRECTORY`. When coordinator mirroring is enabled, an `initstandby` operation copies the contents of the `$COORDINATOR_DATA_DIRECTORY` from the coordinator to the standby coordinator and the incorrect SSL key, and cert files \(the coordinator files, and not the standby coordinator files\) will prevent standby coordinator start up.

    You can specify a different directory for the location of the SSL server files with the `postgresql.conf` parameters `sslcert`, `sslkey`, `sslrootcert`, and `sslcrl`. For more information about the parameters, see [SSL Client Authentication](../security-guide/topics/Authenticate.html) in the *Security Configuration Guide*.


Greenplum Database can be started with SSL enabled by setting the server configuration parameter `ssl=on` in the `postgresql.conf` file on the coordinator and standby coordinator hosts. This `gpconfig` command sets the parameter:

```
gpconfig -c ssl -m on -v off
```

Setting the parameter requires a server restart. This command restarts the system: `gpstop -ra`.

### <a id="topic6"></a>Creating a Self-signed Certificate without a Passphrase for Testing Only 

To create a quick self-signed certificate for the server for testing, use the following OpenSSL command:

```
# openssl req -new -text -out server.req

```

Enter the information requested by the prompts. Be sure to enter the local host name as *Common Name*. The challenge password can be left blank.

The program will generate a key that is passphrase protected, and does not accept a passphrase that is less than four characters long.

To use this certificate with Greenplum Database, remove the passphrase with the following commands:

```
# openssl rsa -in privkey.pem -out server.key
# rm privkey.pem
```

Enter the old passphrase when prompted to unlock the existing key.

Then, enter the following command to turn the certificate into a self-signed certificate and to copy the key and certificate to a location where the server will look for them.

```
# openssl req -x509 -in server.req -text -key server.key -out server.crt
```

Finally, change the permissions on the key with the following command. The server will reject the file if the permissions are less restrictive than these.

```
# chmod og-rwx server.key
```

For more details on how to create your server private key and certificate, refer to the [OpenSSL documentation](https://www.openssl.org/docs/).

