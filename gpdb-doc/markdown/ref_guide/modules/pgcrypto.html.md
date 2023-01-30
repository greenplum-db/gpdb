# pgcrypto 

Greenplum Database is installed with an optional module of encryption/decryption functions called `pgcrypto`. The `pgcrypto` functions allow database administrators to store certain columns of data in encrypted form. This adds an extra layer of protection for sensitive data, as data stored in Greenplum Database in encrypted form cannot be read by anyone who does not have the encryption key, nor can it be read directly from the disks.

> **Note** The `pgcrypto` functions run inside the database server, which means that all the data and passwords move between `pgcrypto` and the client application in clear-text. For optimal security, consider also using SSL connections between the client and the Greenplum coordinator server.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `pgcrypto` module is installed when you install Greenplum Database. Before you can use any of the functions defined in the module, you must register the `pgcrypto` extension in each database in which you want to use the functions. Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="configuring-fips-encryption"></a>Configuring FIPS Encryption 

Starting with Greenplum 6.22, the `pgcrypto` extension provides a module-specific configuration parameter, `pgcrypto.fips`. This parameter configures Greenplum Database support for a limited set of FIPS encryption functionality \(*Federal Information Processing Standard* \(FIPS\) 140-2\). For information about FIPS, see [https://www.nist.gov/itl/popular-links/federal-information-processing-standards-fips](https://www.nist.gov/itl/popular-links/federal-information-processing-standards-fips). The default setting is `off`, FIPS encryption is not enabled.

Before enabling this parameter, ensure that FIPS is enabled on all Greenplum Database system hosts.

When this parameter is enabled, these changes occur:

-   FIPS mode is initialized in the OpenSSL library
-   The functions `digest()` and `hmac()` allow only the SHA encryption algorithm \(MD5 is not allowed\)
-   The functions for the crypt and gen\_salt algorithms are disabled
-   PGP encryption and decryption functions support only AES and 3DES encryption algorithms \(other algorithms such as blowfish are not allowed\)
-   RAW encryption and decryption functions support only AES and 3DES \(other algorithms such as blowfish are not allowed\)

**To enable `pgcrypto.fips`**

1.  Enable the `pgcrypto` functions as an extension if it is not enabled. See [Installing Additional Supplied Modules](../../install_guide/install_modules.html). This example `psql` command creates the `pgcrypto` extension in the database `testdb`.

    ```
    psql -d testdb -c 'CREATE EXTENSION pgcrypto'
    ```

2.  Configure the Greenplum Database server configuration parameter `shared_preload_libraries` to load the `pgcrypto` library. This example uses the `gpconfig` utility to update the parameter in the Greenplum Database `postgresql.conf` files.

    ```
    gpconfig -c shared_preload_libraries -v '\$libdir/pgcrypto'
    ```

    This command displays the value of `shared_preload_libraries`.

    ```
    gpconfig -s shared_preload_libraries
    ```

3.  Restart the Greenplum Database system.

    ```
    gpstop -ra 
    ```

4.  Set the `pgcrypto.fips` server configuration parameter to `on` for each database that uses FIPS encryption. For example, these commands set the parameter to `on` for the database `testdb`.

    ```
    psql -d postgres
    ```
    ```
    ALTER DATABASE testdb SET pgcrypto.fips TO on;
    ```

    > **Important** You must use the `ALTER DATABASE` command to set the parameter. You cannot use the `SET` command that updates the parameter for a session, or use the `gpconfig` utility that updates `postgresql.conf` files.

5.  After setting the parameter, reconnect to the database to enable encryption support for a session. This example uses the `psql` meta command `\c` to connect to the `testdb` database.

    ```
    \c testdb
    ```


**To disable `pgcrypto.fips`**

1.  If the database does not use `pgcrypto` functions, disable the `pgcrypto` extension. This example `psql` command drops the `pgcrypto` extension in the database `testdb`.

    ```
    psql -d testdb -c 'DROP EXTENSION pgcrypto'
    ```

2.  Remove `\$libdir/pgcrypto` from the `shared_preload_libraries` parameter, and restart Greenplum Database. This `gpconfig` command displays the value of the parameter from the Greenplum Database `postgresql.conf` files.

    ```
    gpconfig -s shared_preload_libraries
    ```

    Use the `gpconfig` utility with the `-c` and `-v` options to change the value of the parameter. Use the `-r` option to remove the parameter.

3.  Restart the Greenplum Database system.

    ```
    gpstop -ra 
    ```

## <a id="topic_info"></a>Additional Module Documentation 

Refer to [pgcrypto](https://www.postgresql.org/docs/12/pgcrypto.html) in the PostgreSQL documentation for more information about the individual functions in this module.

