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
-   Private keys containing a passphrase are not supported for the `gpfdist` file server \(`server.key`\) and for the Greenplum Database \(`client.key`\).
-   Issuing certificates that are appropriate for the operating system in use is the user's responsibility. Generally, converting certificates as shown in [https://www.sslshopper.com/ssl-converter.html](https://www.sslshopper.com/ssl-converter.html) is supported.

    > **Note** A server started with the `gpfdist --ssl` option can only communicate with the `gpfdists` protocol. A server that was started with `gpfdist` without the `--ssl` option can only communicate with the `gpfdist` protocol.

-   The client certificate file, `client.crt`
-   The client private key file, `client.key`

Use one of the following methods to invoke the `gpfdists` protocol.

-   Run `gpfdist` with the `--ssl` option and then use the `gpfdists` protocol in the `LOCATION` clause of a `CREATE EXTERNAL TABLE` statement.
-   Use a `gpload` YAML control file with the `SSL` option set to true. Running `gpload` starts the `gpfdist` server with the `--ssl` option, then uses the `gpfdists` protocol.

## <a id="about_cert_files"></a> About the Required Certificate Files

The settings of the [verify_gpfdists_cert](../../ref_guide/config_params/guc-list.html#verify_gpfdists_cert) server configuration parameter (default value `true`) and the [gpfdist](../../utility_guide/ref/gpfdist.html) `--ssl_verify_peer <boolean>` option (default value `on`) control whether SSL certificate authentication is enabled when Greenplum Database communicates with the `gpfdist` utility to either read data from or write data to an external data source. These settings also determine which of the following certificate files must reside in the `$PGDATA/gpfdists` directory on each Greenplum Database segment:

-   The client certificate file, `client.crt`
-   The client private key file, `client.key`
-   The trusted certificate authorities, `root.crt`

The certificate files required for each `verify_gpfdists_cert` and `--ssl_verify_peer` setting combination are identified in the table below:

| verify_gpfdists_cert | --ssl_verify_peer | Required Certificate Files |
|-----------|-------|-------------------|
| on | on | `client.key`</br>`client.crt`</br>`root.crt` |
| on | off | `root.crt` |
| off | on | `client.key`</br>`client.crt` |
| off | off | N/A |

