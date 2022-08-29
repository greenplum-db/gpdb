---
title: gpfdists:// Protocol 
---

The `gpfdists://` protocol is a secure version of the `gpfdist:// protocol`.

To use it, you run the [gpfdist](../../utility_guide/ref/gpfdist.html) utility with the `--ssl` option. When specified in a URI, the `gpfdists://` protocol enables encrypted communication and secure identification of the file server and the Greenplum Database to protect against attacks such as eavesdropping and man-in-the-middle attacks.

`gpfdists` implements SSL security in a client/server scheme with the following attributes and limitations:

-   Client certificates are required.
-   Multilingual certificates are not supported.
-   A Certificate Revocation List \(CRL\) is not supported.
-   The `TLSv1` protocol is used with the `TLS_RSA_WITH_AES_128_CBC_SHA` encryption algorithm.
-   SSL parameters cannot be changed.
-   SSL renegotiation is supported.
-   The SSL ignore host mismatch parameter is set to `false`.
-   Private keys containing a passphrase are not supported for the `gpfdist` file server \(server.key\) and for the Greenplum Database \(client.key\).
-   Issuing certificates that are appropriate for the operating system in use is the user's responsibility. Generally, converting certificates as shown in [https://www.sslshopper.com/ssl-converter.html](https://www.sslshopper.com/ssl-converter.html) is supported.

    **Note:** A server started with the `gpfdist --ssl` option can only communicate with the `gpfdists` protocol. A server that was started with `gpfdist` without the `--ssl` option can only communicate with the `gpfdist` protocol.

-   The client certificate file, client.crt
-   The client private key file, client.key

Use one of the following methods to invoke the `gpfdists` protocol.

-   Run `gpfdist` with the `--ssl` option and then use the `gpfdists` protocol in the `LOCATION` clause of a `CREATE EXTERNAL TABLE` statement.
-   Use a `gpload` YAML control file with the `SSL` option set to true. Running `gpload` starts the `gpfdist` server with the `--ssl` option, then uses the `gpfdists` protocol.

Using `gpfdists` requires that the following client certificates reside in the `$PGDATA/gpfdists` directory on each segment.

-   The client certificate file, `client.crt`
-   The client private key file, `client.key`
-   The trusted certificate authorities, `root.crt`

For an example of loading data into an external table security, see [Example 3—Multiple gpfdists instances](g-example-3-multiple-gpfdists-instances.html).

The server configuration parameter [verify\_gpfdists\_cert](../../ref_guide/config_params/guc-list.html) controls whether SSL certificate authentication is enabled when Greenplum Database communicates with the `gpfdist` utility to either read data from or write data to an external data source. You can set the parameter value to `false` to deactivate authentication when testing the communication between the Greenplum Database external table and the `gpfdist` utility that is serving the external data. If the value is `false`, these SSL exceptions are ignored:

-   The self-signed SSL certificate that is used by `gpfdist` is not trusted by Greenplum Database.
-   The host name contained in the SSL certificate does not match the host name that is running `gpfdist`.

**Warning:** Deactivating SSL certificate authentication exposes a security risk by not validating the `gpfdists` SSL certificate.

**Parent topic:** [Defining External Tables](../external/g-external-tables.html)

