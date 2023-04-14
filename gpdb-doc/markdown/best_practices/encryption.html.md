---
title: Encrypting Data and Database Connections 
---

Best practices for implementing encryption and managing keys.

Encryption can be used to protect data in a Greenplum Database system in the following ways:

-   Connections between clients and the coordinator database can be encrypted with SSL. This is enabled by setting the `ssl` server configuration parameter to `on` and editing the `pg_hba.conf` file. See "Encrypting Client/Server Connections" in the *Greenplum Database Administrator Guide* for information about enabling SSL in Greenplum Database.
-   Greenplum Database 4.2.1 and above allow SSL encryption of data in transit between the Greenplum parallel file distribution server, `gpfdist`, and segment hosts. See [Encrypting gpfdist Connections](#section_kjt_3kr_bs) for more information. 
-   Network communications between hosts in the Greenplum Database cluster can be encrypted using IPsec. An authenticated, encrypted VPN is established between every pair of hosts in the cluster. Check your operating system documentation for IPsec support, or consider a third-party solution such as that provided by [Zettaset](https://www.zettaset.com).
-   The `pgcrypto` module of encryption/decryption functions protects data at rest in the database. Encryption at the column level protects sensitive information, such as passwords, Social Security numbers, or credit card numbers. See [Encrypting Data in Tables using PGP](#section_emf_3kr_bs) for an example.

## <a id="bp1"></a>Best Practices 

-   Encryption ensures that data can be seen only by users who have the key required to decrypt the data.
-   Encrypting and decrypting data has a performance cost; only encrypt data that requires encryption.
-   Do performance testing before implementing any encryption solution in a production system.
-   Server certificates in a production Greenplum Database system should be signed by a certificate authority \(CA\) so that clients can authenticate the server. The CA may be local if all clients are local to the organization.
-   Client connections to Greenplum Database should use SSL encryption whenever the connection goes through an insecure link.
-   A symmetric encryption scheme, where the same key is used to both encrypt and decrypt, has better performance than an asymmetric scheme and should be used when the key can be shared safely.
-   Use functions from the pgcrypto module to encrypt data on disk. The data is encrypted and decrypted in the database process, so it is important to secure the client connection with SSL to avoid transmitting unencrypted data.
-   Use the gpfdists protocol to secure ETL data as it is loaded into or unloaded from the database. See [Encrypting gpfdist Connections](#section_kjt_3kr_bs).

## <a id="keyman"></a>Key Management 

Whether you are using symmetric \(single private key\) or asymmetric \(public and private key\) cryptography, it is important to store the coordinator or private key securely. There are many options for storing encryption keys, for example, on a file system, key vault, encrypted USB, trusted platform module \(TPM\), or hardware security module \(HSM\).

Consider the following questions when planning for key management:

-   Where will the keys be stored?
-   When should keys expire?
-   How are keys protected?
-   How are keys accessed?
-   How can keys be recovered and revoked?

The Open Web Application Security Project \(OWASP\) provides a very comprehensive [guide to securing encryption keys](https://www.owasp.org/index.php/Cryptographic_Storage_Cheat_Sheet).

## <a id="encdat"></a>Encrypting Data at Rest with pgcrypto 

The pgcrypto module for Greenplum Database provides functions for encrypting data at rest in the database. Administrators can encrypt columns with sensitive information, such as social security numbers or credit card numbers, to provide an extra layer of protection. Database data stored in encrypted form cannot be read by users who do not have the encryption key, and the data cannot be read directly from disk.

pgcrypto is installed by default when you install Greenplum Database. You must explicitly enable pgcrypto in each database in which you want to use the module.

pgcrypto allows PGP encryption using symmetric and asymmetric encryption. Symmetric encryption encrypts and decrypts data using the same key and is faster than asymmetric encryption. It is the preferred method in an environment where exchanging secret keys is not an issue. With asymmetric encryption, a public key is used to encrypt data and a private key is used to decrypt data. This is slower then symmetric encryption and it requires a stronger key.

Using pgcrypto always comes at the cost of performance and maintainability. It is important to use encryption only with the data that requires it. Also, keep in mind that you cannot search encrypted data by indexing the data.

Before you implement in-database encryption, consider the following PGP limitations.

-   No support for signing. That also means that it is not checked whether the encryption sub-key belongs to the coordinator key.
-   No support for encryption key as coordinator key. This practice is generally discouraged, so this limitation should not be a problem.
-   No support for several subkeys. This may seem like a problem, as this is common practice. On the other hand, you should not use your regular GPG/PGP keys with pgcrypto, but create new ones, as the usage scenario is rather different.

Greenplum Database is compiled with zlib by default; this allows PGP encryption functions to compress data before encrypting. When compiled with OpenSSL, more algorithms will be available.

Because pgcrypto functions run inside the database server, the data and passwords move between pgcrypto and the client application in clear-text. For optimal security, you should connect locally or use SSL connections and you should trust both the system and database administrators.

pgcrypto configures itself according to the findings of the main PostgreSQL configure script.

When compiled with `zlib`, pgcrypto encryption functions are able to compress data before encrypting.

Pgcrypto has various levels of encryption ranging from basic to advanced built-in functions. The following table shows the supported encryption algorithms.

|Value Functionality|Built-in|With OpenSSL|
|:------------------|:-------|:-----------|
|MD5|yes|yes|
|SHA1|yes|yes|
|SHA224/256/384/512|yes|yes [1](#fntarg_1).|
|Other digest algorithms|no|yes [2](#fntarg_2)|
|Blowfish|yes|yes|
|AES|yes|yes[3](#fntarg_3)|
|DES/3DES/CAST5|no|yes|
|Raw Encryption|yes|yes|
|PGP Symmetric-Key|yes|yes|
|PGP Public Key|yes|yes|

## <a id="creating_pgp_keys"></a>Creating PGP Keys 

To use PGP asymmetric encryption in Greenplum Database, you must first create public and private keys and install them.

This section assumes you are installing Greenplum Database on a Linux machine with the Gnu Privacy Guard \(`gpg`\) command line tool. VMware recommends using the latest version of GPG to create keys. Download and install Gnu Privacy Guard \(GPG\) for your operating system from [https://www.gnupg.org/download/](https://www.gnupg.org/download/). On the GnuPG website you will find installers for popular Linux distributions and links for Windows and Mac OS X installers.

1.  As root, run the following command and choose option 1 from the menu:

    ```
    # gpg --gen-key 
    gpg (GnuPG) 2.0.14; Copyright (C) 2009 Free Software Foundation, Inc.
    This is free software: you are free to change and redistribute it.
    There is NO WARRANTY, to the extent permitted by law.
     
    gpg: directory `/root/.gnupg' created
    gpg: new configuration file `/root/.gnupg/gpg.conf' created
    gpg: WARNING: options in `/root/.gnupg/gpg.conf' are not yet active during this run
    gpg: keyring `/root/.gnupg/secring.gpg' created
    gpg: keyring `/root/.gnupg/pubring.gpg' created
    Please select what kind of key you want:
     (1) RSA and RSA (default)
     (2) DSA and Elgamal
     (3) DSA (sign only)
     (4) RSA (sign only)
    Your selection? 1
    ```

2.  Respond to the prompts and follow the instructions, as shown in this example:

    ```
    RSA keys may be between 1024 and 4096 bits long.
    What keysize do you want? (2048) Press enter to accept default key size
    Requested keysize is 2048 bits
    Please specify how long the key should be valid.
     0 = key does not expire
     <n> = key expires in n days
     <n>w = key expires in n weeks
     <n>m = key expires in n months
     <n>y = key expires in n years
     Key is valid for? (0) 365
    Key expires at Wed 13 Jan 2016 10:35:39 AM PST
    Is this correct? (y/N) y
    
    GnuPG needs to construct a user ID to identify your key.
    
    Real name: John Doe
    Email address: jdoe@email.com
    Comment: 
    You selected this USER-ID:
     "John Doe <jdoe@email.com>"
    
    Change (N)ame, (C)omment, (E)mail or (O)kay/(Q)uit? O
    You need a Passphrase to protect your secret key.
    (For this demo the passphrase is blank.)
    can't connect to `/root/.gnupg/S.gpg-agent': No such file or directory
    You don't want a passphrase - this is probably a *bad* idea!
    I will do it anyway.  You can change your passphrase at any time,
    using this program with the option "--edit-key".
    
    We need to generate a lot of random bytes. It is a good idea to perform
    some other action (type on the keyboard, move the mouse, utilize the
    disks) during the prime generation; this gives the random number
    generator a better chance to gain enough entropy.
    We need to generate a lot of random bytes. It is a good idea to perform
    some other action (type on the keyboard, move the mouse, utilize the
    disks) during the prime generation; this gives the random number
    generator a better chance to gain enough entropy.
    gpg: /root/.gnupg/trustdb.gpg: trustdb created
    gpg: key 2027CC30 marked as ultimately trusted
    public and secret key created and signed.
    
    gpg:  checking the trustdbgpg: 
          3 marginal(s) needed, 1 complete(s) needed, PGP trust model
    gpg:  depth: 0  valid:   1  signed:   0  trust: 0-, 0q, 0n, 0m, 0f, 1u
    gpg:  next trustdb check due at 2016-01-13
    pub   2048R/2027CC30 2015-01-13 [expires: 2016-01-13]
          Key fingerprint = 7EDA 6AD0 F5E0 400F 4D45   3259 077D 725E 2027 CC30
    uid                  John Doe <jdoe@email.com>
    sub   2048R/4FD2EFBB 2015-01-13 [expires: 2016-01-13]
    
    ```

3.  List the PGP keys by entering the following command:

    ```
    gpg --list-secret-keys 
    /root/.gnupg/secring.gpg
    ------------------------
    sec   2048R/2027CC30 2015-01-13 [expires: 2016-01-13]
    uid                  John Doe <jdoe@email.com>
    ssb   2048R/4FD2EFBB 2015-01-13
    
    
    ```

    2027CC30 is the public key and will be used to *encrypt* data in the database. 4FD2EFBB is the private \(secret\) key and will be used to *decrypt* data.

4.  Export the keys using the following commands:

    ```
    # gpg -a --export 4FD2EFBB > public.key
    # gpg -a --export-secret-keys 2027CC30 > secret.key
    ```


See the [pgcrypto](https://www.postgresql.org/docs/12/pgcrypto.html) documentation for more information about PGP encryption functions.

## <a id="section_emf_3kr_bs"></a>Encrypting Data in Tables using PGP 

This section shows how to encrypt data inserted into a column using the PGP keys you generated.

1.  Dump the contents of the `public.key` file and then copy it to the clipboard:

    ```
    # cat public.key
    -----BEGIN PGP PUBLIC KEY BLOCK-----
    Version: GnuPG v2.0.14 (GNU/Linux)
                
    mQENBFS1Zf0BCADNw8Qvk1V1C36Kfcwd3Kpm/dijPfRyyEwB6PqKyA05jtWiXZTh
    2His1ojSP6LI0cSkIqMU9LAlncecZhRIhBhuVgKlGSgd9texg2nnSL9Admqik/yX
    R5syVKG+qcdWuvyZg9oOOmeyjhc3n+kkbRTEMuM3flbMs8shOwzMvstCUVmuHU/V
    . . .
    WH+N2lasoUaoJjb2kQGhLOnFbJuevkyBylRz+hI/+8rJKcZOjQkmmK8Hkk8qb5x/
    HMUc55H0g2qQAY0BpnJHgOOQ45Q6pk3G2/7Dbek5WJ6K1wUrFy51sNlGWE8pvgEx
    /UUZB+dYqCwtvX0nnBu1KNCmk2AkEcFK3YoliCxomdOxhFOv9AKjjojDyC65KJci
    Pv2MikPS2fKOAg1R3LpMa8zDEtl4w3vckPQNrQNnYuUtfj6ZoCxv
    =XZ8J
    -----END PGP PUBLIC KEY BLOCK-----
    
    ```

2.  Enable the `pgcrypto` extension:

    ```
    CREATE EXTENSION pgcrypto;
    ```

3.  Create a table called `userssn` and insert some sensitive data, social security numbers for Bob and Alice, in this example. Paste the public.key contents after "dearmor\(".

    ```
    CREATE TABLE userssn( ssn_id SERIAL PRIMARY KEY, 
        username varchar(100), ssn bytea); 
    
    INSERT INTO userssn(username, ssn)
    SELECT robotccs.username, pgp_pub_encrypt(robotccs.ssn, keys.pubkey) AS
    ssn
    FROM (
    VALUES ('Alice', '123-45-6788'), ('Bob', '123-45-6799'))
    AS robotccs(username, ssn)
    CROSS JOIN (SELECT dearmor('-----BEGIN PGP PUBLIC KEY BLOCK-----
    Version: GnuPG v2.0.22 (GNU/Linux)
    
    mQENBGCb7NQBCADfCoMFIbjb6dup8eJHgTpo8TILiIubqhqASHqUPe/v3eI+p9W8
    mZbTZo+EUFCJmFZx8RWw0s0t4DG3fzBQOv5y2oBEu9sg3ofgFkK6TaQV7ueZfifx
    S1DxQE8kWEFrGsB13VJlLMMLPr4tdjtaYOdn5b+3N4/8GOJALn2CeWrP8lIXaget
    . . .
    T9dl2HhMOatlVhBUOcYrqSBEWgwtQbX36hFzhp1tNCDOvtDpsfLNHJr8vIpXAeyz
    juW0/vEgrAtSK8P2/kmRsmNM/LJIbCBHD+tTSTHZ194+QYUc1KYXW4NV5LLW08MY
    skETyovyVDFYEpTMVrRKJYLROhEBv8cqYgKq1XtcIH8eiwJIZ0L1L/1Cw7Z/BpRT
    WbrwmhXTpqi+/Vdm7q9gPFoAfw/ur44hJGsc13bQxdmluTigSN2f+qf9RzA=
    =xdQf
    -----END PGP PUBLIC KEY BLOCK-----')  as pubkey) AS keys;
    
    ```

4.  Verify that the `ssn` column is encrypted.

    ```
    test_db=# select * from userssn;
    ssn_id   | 1
    username | Alice
    ssn      | \301\300L\003\235M%_O\322\357\273\001\010\000\272\227\010\341\216\360\217C\020\261)_\367
    [\227\034\313:C\354d<\337\006Q\351('\2330\031lX\263Qf\341\262\200\3015\235\036AK\242fL+\315g\322
    7u\270*\304\361\355\220\021\330"\200%\264\274}R\213\377\363\235\366\030\023)\364!\331\303\237t\277=
    f \015\004\242\231\263\225%\032\271a\001\035\277\021\375X\232\304\305/\340\334\0131\325\344[~\362\0
    37-\251\336\303\340\377_\011\275\301/MY\334\343\245\244\372y\257S\374\230\346\277\373W\346\230\276\
    017fi\226Q\307\012\326\3646\000\326\005:E\364W\252=zz\010(:\343Y\237\257iqU\0326\350=v0\362\327\350\
    315G^\027:K_9\254\362\354\215<\001\304\357\331\355\323,\302\213Fe\265\315\232\367\254\245%(\\\373
    4\254\230\331\356\006B\257\333\326H\022\013\353\216F?\023\220\370\035vH5/\227\344b\322\227\026\362=\
    42\033\322<\001}\243\224;)\030zqX\214\340\221\035\275U\345\327\214\032\351\223c\2442\345\304K\016\
    011\214\307\227\237\270\026`R\205\205a~1\263\236[\037C\260\031\205\374\245\317\033k|\366\253\037
    ---------+--------------------------------------------------------------------------------------------
    ------------------------------------------------------------------------------------------------------
    ------------------------------------------------------------------------------------------------------
    ------------------------------------------------------------------------------------------------------
    ------------------------------------------------------------------------------------------------------
    ------------------------------------------------------------------------------------------------------
    ------------------------------------------------------------------------------------------------------
    ------------------------------------------------------------------------------------------------------
    ------------------------------------------------------------------------------------------------------
    ------------------------------------------------------------------------------
    ssn_id   | 2
    username | Bob
    ssn      | \301\300L\003\235M%_O\322\357\273\001\007\377t>\345\343,\200\256\272\300\012\033M4\265\032L
    L[v\262k\244\2435\264\232B\357\370d9\375\011\002\327\235<\246\210b\030\012\337@\226Z\361\246\032\00
    7`\012c\353]\355d7\360T\335\314\367\370;X\371\350*\231\212\260B\010#RQ0\223\253c7\0132b\355\242\233\34
    1\000\370\370\366\013\022\357\005i\202~\005\\z\301o\012\230Z\014\362\244\324&\243g\351\362\325\375
    \213\032\226$\2751\256XR\346k\266\030\234\267\201vUh\004\250\337A\231\223u\247\366/i\022\275\276\350\2
    20\316\306|\203+\010\261;\232\254tp\255\243\261\373Rq;\316w\357\006\207\374U\333\365\365\245hg\031\005
    \322\347ea\220\015l\212g\337\264\336b\263\004\311\210.4\340G+\221\274D\035\375\2216\241`\346a0\273wE\2
    12\342y^\202\262|A7\202t\240\333p\345G\373\253\243oCO\011\360\247\211\014\024{\272\271\322<\001\267
    \347\240\005\213\0078\036\210\307$\317\322\311\222\035\354\006<\266\264\004\376\251q\256\220(+\030\
    3270\013c\327\272\212%\363\033\252\322\337\354\276\225\232\201\212^\304\210\2269@\3230\370{
    
    ```

5.  Extract the public.key ID from the database:

    ```
    SELECT pgp_key_id(dearmor('-----BEGIN PGP PUBLIC KEY BLOCK-----
    Version: GnuPG v2.0.14 (GNU/Linux)
    
    mQENBFS1Zf0BCADNw8Qvk1V1C36Kfcwd3Kpm/dijPfRyyEwB6PqKyA05jtWiXZTh
    2His1ojSP6LI0cSkIqMU9LAlncecZhRIhBhuVgKlGSgd9texg2nnSL9Admqik/yX
    R5syVKG+qcdWuvyZg9oOOmeyjhc3n+kkbRTEMuM3flbMs8shOwzMvstCUVmuHU/V
    . . .
    WH+N2lasoUaoJjb2kQGhLOnFbJuevkyBylRz+hI/+8rJKcZOjQkmmK8Hkk8qb5x/
    HMUc55H0g2qQAY0BpnJHgOOQ45Q6pk3G2/7Dbek5WJ6K1wUrFy51sNlGWE8pvgEx
    /UUZB+dYqCwtvX0nnBu1KNCmk2AkEcFK3YoliCxomdOxhFOv9AKjjojDyC65KJci
    Pv2MikPS2fKOAg1R3LpMa8zDEtl4w3vckPQNrQNnYuUtfj6ZoCxv
    =XZ8J
    -----END PGP PUBLIC KEY BLOCK-----'));
    
    pgp_key_id | 9D4D255F4FD2EFBB
    
    ```

    This shows that the PGP key ID used to encrypt the `ssn` column is 9D4D255F4FD2EFBB. It is recommended to perform this step whenever a new key is created and then store the ID for tracking.

    You can use this key to see which key pair was used to encrypt the data:

    ```
    SELECT username, pgp_key_id(ssn) As key_used                                                                                                                       FROM userssn;                                                                                                                                                                 username | Bob
    key_used | 9D4D255F4FD2EFBB
    ---------+-----------------
    username | Alice
    key_used | 9D4D255F4FD2EFBB
    
    ```

    > **Note** Different keys may have the same ID. This is rare, but is a normal event. The client application should try to decrypt with each one to see which fits — like handling `ANYKEY`. See [pgp\_key\_id\(\)](https://www.postgresql.org/docs/12/pgcrypto.html) in the pgcrypto documentation.

6.  Decrypt the data using the private key.

    ```
    SELECT username, pgp_pub_decrypt(ssn, keys.privkey) 
                     AS decrypted_ssn FROM userssn
                     CROSS JOIN
                     (SELECT dearmor('-----BEGIN PGP PRIVATE KEY BLOCK-----
    Version: GnuPG v2.0.14 (GNU/Linux)
    
    lQOYBFS1Zf0BCADNw8Qvk1V1C36Kfcwd3Kpm/dijPfRyyEwB6PqKyA05jtWiXZTh
    2His1ojSP6LI0cSkIqMU9LAlncecZhRIhBhuVgKlGSgd9texg2nnSL9Admqik/yX
    R5syVKG+qcdWuvyZg9oOOmeyjhc3n+kkbRTEMuM3flbMs8shOwzMvstCUVmuHU/V
    . . .
    QNPSvz62WH+N2lasoUaoJjb2kQGhLOnFbJuevkyBylRz+hI/+8rJKcZOjQkmmK8H
    kk8qb5x/HMUc55H0g2qQAY0BpnJHgOOQ45Q6pk3G2/7Dbek5WJ6K1wUrFy51sNlG
    WE8pvgEx/UUZB+dYqCwtvX0nnBu1KNCmk2AkEcFK3YoliCxomdOxhFOv9AKjjojD
    yC65KJciPv2MikPS2fKOAg1R3LpMa8zDEtl4w3vckPQNrQNnYuUtfj6ZoCxv
    =fa+6
    -----END PGP PRIVATE KEY BLOCK-----') AS privkey) AS keys;
    
    username | decrypted_ssn 
    ----------+---------------
     Alice    | 123-45-6788
     Bob      | 123-45-6799
    (2 rows)
    
    
    ```

    If you created a key with passphrase, you may have to enter it here. However for the purpose of this example, the passphrase is blank.


## <a id="section_kjt_3kr_bs"></a>Encrypting gpfdist Connections 

The `gpfdists` protocol is a secure version of the `gpfdist` protocol that securely identifies the file server and the Greenplum Database and encrypts the communications between them. Using `gpfdists` protects against eavesdropping and man-in-the-middle attacks.

The `gpfdists` protocol implements client/server SSL security with the following notable features:

-   Client certificates are required.
-   Multilingual certificates are not supported.
-   A Certificate Revocation List \(CRL\) is not supported.
-   The TLSv1 protocol is used with the `TLS_RSA_WITH_AES_128_CBC_SHA` encryption algorithm. These SSL parameters cannot be changed.
-   SSL renegotiation is supported.
-   The SSL ignore host mismatch parameter is set to false.
-   Private keys containing a passphrase are not supported for the `gpfdist` file server \(server.key\) or for the Greenplum Database \(client.key\).
-   It is the user's responsibility to issue certificates that are appropriate for the operating system in use. Generally, converting certificates to the required format is supported, for example using the SSL Converter at [https://www.sslshopper.com/ssl-converter.html](http://www.commoncriteriaportal.org/products/?expand#ALL).

A `gpfdist` server started with the `--ssl` option can only communicate with the `gpfdists` protocol. A `gpfdist` server started without the `--ssl` option can only communicate with the `gpfdist` protocol. For more detail about `gpfdist` refer to the *Greenplum Database Administrator Guide*.

There are two ways to enable the `gpfdists` protocol:

-   Run `gpfdist` with the `--ssl` option and then use the `gpfdists` protocol in the `LOCATION` clause of a `CREATE EXTERNAL TABLE` statement.
-   Use a YAML control file with the SSL option set to true and run `gpload`. Running `gpload` starts the `gpfdist` server with the `--ssl` option and then uses the `gpfdists` protocol.

When using gpfdists, the following client certificates must be located in the `$PGDATA/gpfdists` directory on each segment:

-   The client certificate file, `client.crt`
-   The client private key file, `client.key`
-   The trusted certificate authorities, `root.crt`

> **Important** Do not protect the private key with a passphrase. The server does not prompt for a passphrase for the private key, and loading data fails with an error if one is required.

When using `gpload` with SSL you specify the location of the server certificates in the YAML control file. When using `gpfdist` with SSL, you specify the location of the server certificates with the --ssl option.

The following example shows how to securely load data into an external table. The example creates a readable external table named `ext_expenses` from all files with the `txt` extension, using the `gpfdists` protocol. The files are formatted with a pipe \(`|`\) as the column delimiter and an empty space as null.

1.  Run `gpfdist` with the `--ssl` option on the segment hosts.
2.  Log into the database and run the following command:

    ```
    
    =# CREATE EXTERNAL TABLE ext_expenses 
       ( name text, date date, amount float4, category text, desc1 text )
    LOCATION ('gpfdists://etlhost-1:8081/*.txt', 'gpfdists://etlhost-2:8082/*.txt')
    FORMAT 'TEXT' ( DELIMITER '|' NULL ' ') ;
    
    ```


**Parent topic:** [Greenplum Database Best Practices](intro.html)

[1](#fnsrc_1) SHA2 algorithms were added to OpenSSL in version 0.9.8. For older versions, pgcrypto will use built-in code[2](#fnsrc_2) Any digest algorithm OpenSSL supports is automatically picked up. This is not possible with ciphers, which need to be supported explicitly.[3](#fnsrc_3) AES is included in OpenSSL since version 0.9.7. For older versions, pgcrypto will use built-in code.

