---
title: Character Set Support 
---

The character set support in Greenplum Database allows you to store text in a variety of character sets, including single-byte character sets such as the ISO 8859 series and multiple-byte character sets such as EUC \(Extended Unix Code\), UTF-8, and Mule internal code. All supported character sets can be used transparently by clients, but a few are not supported for use within the server \(that is, as a server-side encoding\)<sup>1</sup>. The default character set is selected while initializing your Greenplum Database array using `gpinitsystem`. It can be overridden when you create a database, so you can have multiple databases each with a different character set.

|Name|Description|Language|Server?|Bytes/Char|Aliases|
|----|-----------|--------|-------|----------|-------|
|BIG5|Big Five|Traditional Chinese|No|1-2|WIN950, Windows950|
|EUC\_CN|Extended UNIX Code-CN|Simplified Chinese|Yes|1-3| |
|EUC\_JP|Extended UNIX Code-JP|Japanese|Yes|1-3| |
|EUC\_KR|Extended UNIX Code-KR|Korean|Yes|1-3| |
|EUC\_TW|Extended UNIX Code-TW|Traditional Chinese, Taiwanese|Yes|1-3| |
|GB18030|National Standard|Chinese|No|1-2| |
|GBK|Extended National Standard|Simplified Chinese|No|1-2|WIN936, Windows936|
|ISO\_8859\_5|ISO 8859-5, ECMA 113|Latin/Cyrillic|Yes|1| |
|ISO\_8859\_6|ISO 8859-6, ECMA 114|Latin/Arabic|Yes|1| |
|ISO\_8859\_7|ISO 8859-7, ECMA 118|Latin/Greek|Yes|1| |
|ISO\_8859\_8|ISO 8859-8, ECMA 121|Latin/Hebrew|Yes|1| |
|JOHAB|JOHA|Korean \(Hangul\)|Yes|1-3| |
|KOI8|KOI8-R\(U\)|Cyrillic|Yes|1|KOI8R|
|LATIN1|ISO 8859-1, ECMA 94|Western European|Yes|1|ISO88591|
|LATIN2|ISO 8859-2, ECMA 94|Central European|Yes|1|ISO88592|
|LATIN3|ISO 8859-3, ECMA 94|South European|Yes|1|ISO88593|
|LATIN4|ISO 8859-4, ECMA 94|North European|Yes|1|ISO88594|
|LATIN5|ISO 8859-9, ECMA 128|Turkish|Yes|1|ISO88599|
|LATIN6|ISO 8859-10, ECMA 144|Nordic|Yes|1|ISO885910|
|LATIN7|ISO 8859-13|Baltic|Yes|1|ISO885913|
|LATIN8|ISO 8859-14|Celtic|Yes|1|ISO885914|
|LATIN9|ISO 8859-15|LATIN1 with Euro and accents|Yes|1|ISO885915|
|LATIN10|ISO 8859-16, ASRO SR 14111|Romanian|Yes|1|ISO885916|
|MULE\_INTERNAL|Mule internal code|Multilingual Emacs|Yes|1-4| |
|SJIS|Shift JIS|Japanese|No|1-2|Mskanji, ShiftJIS, WIN932, Windows932|
|SQL\_ASCII|unspecified<sup>2</sup>|any|No|1| |
|UHC|Unified Hangul Code|Korean|No|1-2|WIN949, Windows949|
|UTF8|Unicode, 8-bit|all|Yes|1-4|Unicode|
|WIN866|Windows CP866|Cyrillic|Yes|1|ALT|
|WIN874|Windows CP874|Thai|Yes|1| |
|WIN1250|Windows CP1250|Central European|Yes|1| |
|WIN1251|Windows CP1251|Cyrillic|Yes|1|WIN|
|WIN1252|Windows CP1252|Western European|Yes|1| |
|WIN1253|Windows CP1253|Greek|Yes|1| |
|WIN1254|Windows CP1254|Turkish|Yes|1| |
|WIN1255|Windows CP1255|Hebrew|Yes|1| |
|WIN1256|Windows CP1256|Arabic|Yes|1| |
|WIN1257|Windows CP1257|Baltic|Yes|1| |
|WIN1258|Windows CP1258|Vietnamese|Yes|1|ABC, TCVN, TCVN5712, VSCII|

**Parent topic:** [Greenplum Database Reference Guide](ref_guide.html)

## <a id="topic2"></a>Setting the Character Set 

`gpinitsystem` defines the default character set for a Greenplum Database system by reading the setting of the `ENCODING` parameter in the `gp_init_config` file at initialization time. The default character set is `UNICODE` or `UTF8`.

You can create a database with a different character set besides what is used as the system-wide default. For example:

```
=> CREATE DATABASE korean WITH ENCODING 'EUC_KR';
```

**Important:** Although you can specify any encoding you want for a database, it is unwise to choose an encoding that is not what is expected by the locale you have selected. The `LC_COLLATE` and `LC_CTYPE` settings imply a particular encoding, and locale-dependent operations \(such as sorting\) are likely to misinterpret data that is in an incompatible encoding.

Since these locale settings are frozen by `gpinitsystem`, the apparent flexibility to use different encodings in different databases is more theoretical than real.

One way to use multiple encodings safely is to set the locale to `C` or `POSIX` during initialization time, thus deactivating any real locale awareness.

## <a id="topic3"></a>Character Set Conversion Between Server and Client 

Greenplum Database supports automatic character set conversion between server and client for certain character set combinations. The conversion information is stored in the master *pg\_conversion* system catalog table. Greenplum Database comes with some predefined conversions or you can create a new conversion using the SQL command `CREATE CONVERSION`.

|Server Character Set|Available Client Character Sets|
|--------------------|-------------------------------|
|BIG5|not supported as a server encoding|
|EUC\_CN|EUC\_CN, MULE\_INTERNAL, UTF8|
|EUC\_JP|EUC\_JP, MULE\_INTERNAL, SJIS, UTF8|
|EUC\_KR|EUC\_KR, MULE\_INTERNAL, UTF8|
|EUC\_TW|EUC\_TW, BIG5, MULE\_INTERNAL, UTF8|
|GB18030|not supported as a server encoding|
|GBK|not supported as a server encoding|
|ISO\_8859\_5|ISO\_8859\_5, KOI8, MULE\_INTERNAL, UTF8, WIN866, WIN1251|
|ISO\_8859\_6|ISO\_8859\_6, UTF8|
|ISO\_8859\_7|ISO\_8859\_7, UTF8|
|ISO\_8859\_8|ISO\_8859\_8, UTF8|
|JOHAB|JOHAB, UTF8|
|KOI8|KOI8, ISO\_8859\_5, MULE\_INTERNAL, UTF8, WIN866, WIN1251|
|LATIN1|LATIN1, MULE\_INTERNAL, UTF8|
|LATIN2|LATIN2, MULE\_INTERNAL, UTF8, WIN1250|
|LATIN3|LATIN3, MULE\_INTERNAL, UTF8|
|LATIN4|LATIN4, MULE\_INTERNAL, UTF8|
|LATIN5|LATIN5, UTF8|
|LATIN6|LATIN6, UTF8|
|LATIN7|LATIN7, UTF8|
|LATIN8|LATIN8, UTF8|
|LATIN9|LATIN9, UTF8|
|LATIN10|LATIN10, UTF8|
|MULE\_INTERNAL|MULE\_INTERNAL, BIG5, EUC\_CN, EUC\_JP, EUC\_KR, EUC\_TW, ISO\_8859\_5, KOI8, LATIN1 to LATIN4, SJIS, WIN866, WIN1250, WIN1251|
|SJIS|not supported as a server encoding|
|SQL\_ASCII|not supported as a server encoding|
|UHC|not supported as a server encoding|
|UTF8|all supported encodings|
|WIN866|WIN866|
|ISO\_8859\_5|KOI8, MULE\_INTERNAL, UTF8, WIN1251|
|WIN874|WIN874, UTF8|
|WIN1250|WIN1250, LATIN2, MULE\_INTERNAL, UTF8|
|WIN1251|WIN1251, ISO\_8859\_5, KOI8, MULE\_INTERNAL, UTF8, WIN866|
|WIN1252|WIN1252, UTF8|
|WIN1253|WIN1253, UTF8|
|WIN1254|WIN1254, UTF8|
|WIN1255|WIN1255, UTF8|
|WIN1256|WIN1256, UTF8|
|WIN1257|WIN1257, UTF8|
|WIN1258|WIN1258, UTF8|

To enable automatic character set conversion, you have to tell Greenplum Database the character set \(encoding\) you would like to use in the client. There are several ways to accomplish this:

-   Using the `\encoding` command in `psql`, which allows you to change client encoding on the fly.
-   Using `SET client_encoding TO`.

    To set the client encoding, use the following SQL command:

    ```
    => SET CLIENT_ENCODING TO '<value>';
    ```

    To query the current client encoding:

    ```
    => SHOW client_encoding;
    ```

    To return to the default encoding:

    ```
    => RESET client_encoding;
    ```

-   Using the `PGCLIENTENCODING` environment variable. When `PGCLIENTENCODING` is defined in the client's environment, that client encoding is automatically selected when a connection to the server is made. \(This can subsequently be overridden using any of the other methods mentioned above.\)
-   Setting the configuration parameter `client_encoding`. If `client_encoding` is set in the master `postgresql.conf`file, that client encoding is automatically selected when a connection to Greenplum Database is made. \(This can subsequently be overridden using any of the other methods mentioned above.\)

If the conversion of a particular character is not possible (suppose you chose `EUC_JP` for the server and `LATIN1` for the client, then some Japanese characters do not have a representation in `LATIN1`) then an error is reported.

If the client character set is defined as `SQL_ASCII`, encoding conversion is deactivated, regardless of the server's character set. The use of `SQL_ASCII` is unwise unless you are working with all-ASCII data. `SQL_ASCII` is not supported as a server encoding.

<sup>1</sup>Not all APIs support all the listed character sets. For example, the JDBC driver does not support MULE\_INTERNAL, LATIN6, LATIN8, and LATIN10.

<sup>2</sup>The SQL\_ASCII setting behaves considerably differently from the other settings. Byte values 0-127 are interpreted according to the ASCII standard, while byte values 128-255 are taken as uninterpreted characters. If you are working with any non-ASCII data, it is unwise to use the SQL\_ASCII setting as a client encoding. SQL\_ASCII is not supported as a server encoding.

