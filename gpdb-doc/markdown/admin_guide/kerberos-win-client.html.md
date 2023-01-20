---
title: Configuring Kerberos For Windows Clients 
---

You can configure Microsoft Windows client applications to connect to a Greenplum Database system that is configured to authenticate with Kerberos.

When a Greenplum Database system is configured to authenticate with Kerberos, you can configure Kerberos authentication for the Greenplum Database client utilities `gpload` and `psql` on a Microsoft Windows system. The Greenplum Database clients authenticate with Kerberos directly.

This section contains the following information.

-   [Installing and Configuring Kerberos on a Windows System](#topic_win_kerberos_install)
-   [Running the psql Utility](#topic_win_psql_kerb)
-   [Example gpload YAML File](#topic_win_gpload_kerb)
-   [Creating a Kerberos Keytab File](#topic_win_keytab)
-   [Issues and Possible Solutions](#topic_win_kerberos_issues)

These topics assume that the Greenplum Database system is configured to authenticate with Kerberos. For information about configuring Greenplum Database with Kerberos authentication, refer to [Using Kerberos Authentication](kerberos.html).

**Parent topic:** [Configuring Client Authentication](client_auth.html)

## <a id="topic_win_kerberos_install"></a>Installing and Configuring Kerberos on a Windows System 

The `kinit`, `kdestroy`, and `klist` MIT Kerberos Windows client programs and supporting libraries are installed on your system when you install the Greenplum Database Client and Load Tools package:

-   `kinit` - generate a Kerberos ticket
-   `kdestroy` - destroy active Kerberos tickets
-   `klist` - list Kerberos tickets

You must configure Kerberos on the Windows client to authenticate with Greenplum Database:

1.  Copy the Kerberos configuration file `/etc/krb5.conf` from the Greenplum Database coordinator to the Windows system, rename it to `krb5.ini`, and place it in the default Kerberos location on the Windows system, `C:\ProgramData\MIT\Kerberos5\krb5.ini`. This directory may be hidden. This step requires administrative privileges on the Windows client system. You may also choose to place the `/etc/krb5.ini` file in a custom location. If you choose to do this, you must configure and set a system environment variable named `KRB5_CONFIG` to the custom location.
2.  Locate the `[libdefaults]` section of the `krb5.ini` file, and remove the entry identifying the location of the Kerberos credentials cache file, `default_ccache_name`. This step requires administrative privileges on the Windows client system.

    This is an example configuration file with `default_ccache_name` removed. The `[logging]` section is also removed.

    ```
    [libdefaults]
     debug = true
     default_etypes = aes256-cts-hmac-sha1-96
     default_realm = EXAMPLE.LOCAL
     dns_lookup_realm = false
     dns_lookup_kdc = false
     ticket_lifetime = 24h
     renew_lifetime = 7d
     forwardable = true
    
    [realms]
     EXAMPLE.LOCAL = {
      kdc =bocdc.example.local
      admin_server = bocdc.example.local
     }
    
    [domain_realm]
     .example.local = EXAMPLE.LOCAL
     example.local = EXAMPLE.LOCAL
    ```

3.  Set up the Kerberos credential cache file. On the Windows system, set the environment variable `KRB5CCNAME` to specify the file system location of the cache file. The file must be named `krb5cache`. This location identifies a file, not a directory, and should be unique to each login on the server. When you set `KRB5CCNAME`, you can specify the value in either a local user environment or within a session. For example, the following command sets `KRB5CCNAME` in the session:

    ```
    set KRB5CCNAME=%USERPROFILE%\krb5cache
    ```

4.  Obtain your Kerberos principal and password or keytab file from your system administrator.
5.  Generate a Kerberos ticket using a password or a keytab. For example, to generate a ticket using a password:

    ```
    kinit [<principal>]
    ```

    To generate a ticket using a keytab \(as described in [Creating a Kerberos Keytab File](#topic_win_keytab)\):

    ```
    kinit -k -t <keytab_filepath> [<principal>]
    ```

6.  Set up the Greenplum clients environment:

    ```
    set PGGSSLIB=gssapi
    "c:\Program Files\Greenplum\greenplum-clients\greenplum_clients_path.bat"
    ```


## <a id="topic_win_psql_kerb"></a>Running the psql Utility 

After you configure Kerberos and generate the Kerberos ticket on a Windows system, you can run the Greenplum Database command line client `psql`.

If you get warnings indicating that the Console code page differs from Windows code page, you can run the Windows utility `chcp` to change the code page. This is an example of the warning and fix:

```
psql -h prod1.example.local warehouse
psql (9.4.20)
WARNING: Console code page (850) differs from Windows code page (1252)
 8-bit characters might not work correctly. See psql reference
 page "Notes for Windows users" for details.
Type "help" for help.

warehouse=# \q

chcp 1252
Active code page: 1252

psql -h prod1.example.local warehouse
psql (9.4.20)
Type "help" for help.
```

## <a id="topic_win_keytab"></a>Creating a Kerberos Keytab File 

You can create and use a Kerberos `keytab` file to avoid entering a password at the command line or listing a password in a script file when you connect to a Greenplum Database system, perhaps when automating a scheduled Greenplum task such as `gpload`. You can create a keytab file with the Java JRE keytab utility `ktab`. If you use AES256-CTS-HMAC-SHA1-96 encryption, you need to download and install the Java extension *Java Cryptography Extension \(JCE\) Unlimited Strength Jurisdiction Policy Files for JDK/JRE* from Oracle.

> **Note** You must enter the password to create a keytab file. The password is visible onscreen as you enter it.

This example runs the Java `ktab.exe` program to create a keytab file \(`-a` option\) and list the keytab name and entries \(`-l` `-e` `-t` options\).

```
C:\Users\dev1>"\Program Files\Java\jre1.8.0_77\bin"\ktab -a dev1
Password for dev1@EXAMPLE.LOCAL:<your_password>
Done!
Service key for dev1 is saved in C:\Users\dev1\krb5.keytab

C:\Users\dev1>"\Program Files\Java\jre1.8.0_77\bin"\ktab -l -e -t
Keytab name: C:\Users\dev1\krb5.keytab
KVNO Timestamp Principal
---- -------------- ------------------------------------------------------
 4 13/04/16 19:14 dev1@EXAMPLE.LOCAL (18:AES256 CTS mode with HMAC SHA1-96)
 4 13/04/16 19:14 dev1@EXAMPLE.LOCAL (17:AES128 CTS mode with HMAC SHA1-96)
 4 13/04/16 19:14 dev1@EXAMPLE.LOCAL (16:DES3 CBC mode with SHA1-KD)
 4 13/04/16 19:14 dev1@EXAMPLE.LOCAL (23:RC4 with HMAC)
```

You can then generate a Kerberos ticket using a keytab with the following command:

```
kinit -kt dev1.keytab dev1
```

or

```
kinit -kt %USERPROFILE%\krb5.keytab dev1
```

## <a id="topic_win_gpload_kerb"></a>Example gpload YAML File 

When you initiate a `gpload` job to a Greenplum Database system using Kerberos authentication, you omit the `USER:` property and value from the YAML control file.

This example `gpload` YAML control file named `test.yaml` does not include a `USER:` entry:

```
---
VERSION: 1.0.0.1
DATABASE: warehouse
HOST: prod1.example.local
PORT: 5432

GPLOAD:
   INPUT:
    - SOURCE:
         PORT_RANGE: [18080,18080]
         FILE:
           - /Users/dev1/Downloads/test.csv
    - FORMAT: text
    - DELIMITER: ','
    - QUOTE: '"'
    - ERROR_LIMIT: 25
    - LOG_ERRORS: true
   OUTPUT:
    - TABLE: public.test
    - MODE: INSERT
   PRELOAD:
    - REUSE_TABLES: true
```

These commands run `kinit` using a keytab file, run `gpload.bat` with the `test.yaml` file, and then display successful `gpload` output.

```
kinit -kt %USERPROFILE%\krb5.keytab dev1

gpload.bat -f test.yaml
2016-04-10 16:54:12|INFO|gpload session started 2016-04-10 16:54:12
2016-04-10 16:54:12|INFO|started gpfdist -p 18080 -P 18080 -f "/Users/dev1/Downloads/test.csv" -t 30
2016-04-10 16:54:13|INFO|running time: 0.23 seconds
2016-04-10 16:54:13|INFO|rows Inserted = 3
2016-04-10 16:54:13|INFO|rows Updated = 0
2016-04-10 16:54:13|INFO|data formatting errors = 0
2016-04-10 16:54:13|INFO|gpload succeeded
```

## <a id="topic_win_kerberos_issues"></a>Issues and Possible Solutions 

-   This message indicates that Kerberos cannot find your Kerberos credentials cache file:

    ```
    Credentials cache I/O operation failed XXX
    (Kerberos error 193)
    krb5_cc_default() failed
    ```

    To ensure that Kerberos can find the file, set the environment variable `KRB5CCNAME` and run `kinit`.

    ```
    set KRB5CCNAME=%USERPROFILE%\krb5cache
    kinit
    ```

-   This `kinit` message indicates that the `kinit -k -t` command could not find the keytab.

    ```
    kinit: Generic preauthentication failure while getting initial credentials
    ```

    Confirm that the full path and filename for the Kerberos keytab file is correct.


