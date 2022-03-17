---
title: Using LDAP Authentication with TLS/SSL 
---

You can control access to Greenplum Database with an LDAP server and, optionally, secure the connection with encryption by adding parameters to pg\_hba.conf file entries.

Greenplum Database supports LDAP authentication with the TLS/SSL protocol to encrypt communication with an LDAP server:

-   LDAP authentication with STARTTLS and TLS protocol – STARTTLS starts with a clear text connection \(no encryption\) and upgrades it to a secure connection \(with encryption\).
-   LDAP authentication with a secure connection and TLS/SSL \(LDAPS\) – Greenplum Database uses the TLS or SSL protocol based on the protocol that is used by the LDAP server.

If no protocol is specified, Greenplum Database communicates with the LDAP server with a clear text connection.

To use LDAP authentication, the Greenplum Database master host must be configured as an LDAP client. See your LDAP documentation for information about configuring LDAP clients.

## <a id="enldap"></a>Enabling LDAP Authentication with STARTTLS and TLS 

To enable STARTTLS with the TLS protocol, in the pg\_hba.conf file, add an `ldap` line and specify the `ldaptls` parameter with the value 1. The default port is 389. In this example, the authentication method parameters include the `ldaptls` parameter.

```
ldap ldapserver=myldap.com ldaptls=1 ldapprefix="uid=" ldapsuffix=",ou=People,dc=example,dc=com"
```

Specify a non-default port with the `ldapport` parameter. In this example, the authentication method includes the `ldaptls` parameter and the `ldapport` parameter to specify the port 550.

```
ldap ldapserver=myldap.com ldaptls=1 ldapport=500 ldapprefix="uid=" ldapsuffix=",ou=People,dc=example,dc=com"
```

## <a id="enldapauth"></a>Enabling LDAP Authentication with a Secure Connection and TLS/SSL 

To enable a secure connection with TLS/SSL, add `ldaps://` as the prefix to the LDAP server name specified in the `ldapserver` parameter. The default port is 636.

This example `ldapserver` parameter specifies a secure connection and the TLS/SSL protocol for the LDAP server `myldap.com`.

```
ldapserver=ldaps://myldap.com
```

To specify a non-default port, add a colon \(:\) and the port number after the LDAP server name. This example `ldapserver` parameter includes the `ldaps://` prefix and the non-default port 550.

```
ldapserver=ldaps://myldap.com:550
```

## <a id="conauth"></a>Configuring Authentication with a System-wide OpenLDAP System 

If you have a system-wide OpenLDAP system and logins are configured to use LDAP with TLS or SSL in the pg\_hba.conf file, logins may fail with the following message:

```
could not start LDAP TLS session: error code '-11'
```

To use an existing OpenLDAP system for authentication, Greenplum Database must be set up to use the LDAP server's CA certificate to validate user certificates. Follow these steps on both the master and standby hosts to configure Greenplum Database:

1.  Copy the base64-encoded root CA chain file from the Active Directory or LDAP server to the Greenplum Database master and standby master hosts. This example uses the directory `/etc/pki/tls/certs`.
2.  Change to the directory where you copied the CA certificate file and, as the root user, generate the hash for OpenLDAP:

    ```
    # cd /etc/pki/tls/certs  
    # openssl x509 -noout -hash -in <ca-certificate-file>  
    # ln -s <ca-certificate-file> <ca-certificate-file>.0
    ```

3.  Configure an OpenLDAP configuration file for Greenplum Database with the CA certificate directory and certificate file specified.

    As the root user, edit the OpenLDAP configuration file `/etc/openldap/ldap.conf`:

    ```
    SASL_NOCANON on
     URI ldaps://ldapA.example.priv ldaps://ldapB.example.priv ldaps://ldapC.example.priv
     BASE dc=example,dc=priv
     TLS_CACERTDIR /etc/pki/tls/certs
     TLS_CACERT /etc/pki/tls/certs/<ca-certificate-file>
    ```

    **Note:** For certificate validation to succeed, the hostname in the certificate must match a hostname in the URI property. Otherwise, you must also add `TLS_REQCERT allow` to the file.

4.  As the gpadmin user, edit `/usr/local/greenplum-db/greenplum_path.sh` and add the following line.

    ```
    export LDAPCONF=/etc/openldap/ldap.conf
    ```


## <a id="notes2"></a>Notes 

Greenplum Database logs an error if the following are specified in an pg\_hba.conf file entry:

-   If both the `ldaps://` prefix and the `ldaptls=1` parameter are specified.
-   If both the `ldaps://` prefix and the `ldapport` parameter are specified.

Enabling encrypted communication for LDAP authentication only encrypts the communication between Greenplum Database and the LDAP server.

See [Encrypting Client/Server Connections](client_auth.html) for information about encrypting client connections.

## <a id="exams"></a>Examples 

These are example entries from an pg\_hba.conf file.

This example specifies LDAP authentication with no encryption between Greenplum Database and the LDAP server.

```
host all plainuser 0.0.0.0/0 ldap ldapserver=myldap.com ldapprefix="uid=" ldapsuffix=",ou=People,dc=example,dc=com"
```

This example specifies LDAP authentication with the STARTTLS and TLS protocol between Greenplum Database and the LDAP server.

```
host all tlsuser 0.0.0.0/0 ldap ldapserver=myldap.com ldaptls=1 ldapprefix="uid=" ldapsuffix=",ou=People,dc=example,dc=com" 
```

This example specifies LDAP authentication with a secure connection and TLS/SSL protocol between Greenplum Database and the LDAP server.

```
host all ldapsuser 0.0.0.0/0 ldap ldapserver=ldaps://myldap.com ldapprefix="uid=" ldapsuffix=",ou=People,dc=example,dc=com"
```

**Parent topic:**[Configuring Client Authentication](client_auth.html)

