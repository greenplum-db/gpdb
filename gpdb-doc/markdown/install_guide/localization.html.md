---
title: Configuring Timezone and Localization Settings 
---

Describes the available timezone and localization features of Greenplum Database.

**Parent topic:**[Installing and Upgrading Greenplum](install_guide.html)

## <a id="topic_fvc_zh1_b2b"></a>Configuring the Timezone 

Greenplum Database selects a timezone to use from a set of internally stored PostgreSQL timezones. The available PostgreSQL timezones are taken from the Internet Assigned Numbers Authority \(IANA\) Time Zone Database, and Greenplum Database updates its list of available timezones as necessary when the IANA database changes for PostgreSQL.

Greenplum Database selects the timezone by matching a PostgreSQL timezone with the value of the `TimeZone` server configuration parameter, or the host system time zone if `TimeZone` is not set. For example, when selecting a default timezone from the host system time zone, Greenplum Database uses an algorithm to select a PostgreSQL timezone based on the host system timezone files. If the system timezone includes leap second information, Greenplum Database cannot match the system timezone with a PostgreSQL timezone. In this case, Greenplum Database calculates a "best match" with a PostgreSQL timezone based on information from the host system.

As a best practice, configure Greenplum Database and the host systems to use a known, supported timezone. This sets the timezone for the Greenplum Database coordinator and segment instances, and prevents Greenplum Database from selecting a best match timezone each time the cluster is restarted, using the current system timezone and Greenplum Database timezone files \(which may have been updated from the IANA database since the last restart\). Use the `gpconfig` utility to show and set the Greenplum Database timezone. For example, these commands show the Greenplum Database timezone and set the timezone to `US/Pacific`.

```
# gpconfig -s TimeZone
# gpconfig -c TimeZone -v 'US/Pacific'
```

You must restart Greenplum Database after changing the timezone. The command `gpstop -ra` restarts Greenplum Database. The catalog view `pg_timezone_names` provides Greenplum Database timezone information.

## <a id="topic2"></a>About Locale Support in Greenplum Database 

Greenplum Database supports localization with two approaches:

-   Using the locale features of the operating system to provide locale-specific collation order, number formatting, and so on.
-   Providing a number of different character sets defined in the Greenplum Database server, including multiple-byte character sets, to support storing text in all kinds of languages, and providing character set translation between client and server.

Locale support refers to an application respecting cultural preferences regarding alphabets, sorting, number formatting, etc. Greenplum Database uses the standard ISO C and POSIX locale facilities provided by the server operating system. For additional information refer to the documentation of your operating system.

Locale support is automatically initialized when a Greenplum Database system is initialized. The initialization utility, [gpinitsystem](../utility_guide/ref/gpinitsystem.html), will initialize the Greenplum array with the locale setting of its execution environment by default, so if your system is already set to use the locale that you want in your Greenplum Database system then there is nothing else you need to do.

When you are ready to initiate Greenplum Database and you want to use a different locale \(or you are not sure which locale your system is set to\), you can instruct `gpinitsystem` exactly which locale to use by specifying the -n locale option. For example:

```
$ gpinitsystem -c gp_init_config -n sv_SE
```

See [Initializing a Greenplum Database System](init_gpdb.html) for information about the database initialization process.

The example above sets the locale to Swedish \(sv\) as spoken in Sweden \(SE\). Other possibilities might be `en_US` \(U.S. English\) and `fr_CA` \(French Canadian\). If more than one character set can be useful for a locale then the specifications look like this: `cs_CZ.ISO8859-2`. What locales are available under what names on your system depends on what was provided by the operating system vendor and what was installed. On most systems, the command `locale -a` will provide a list of available locales.

Occasionally it is useful to mix rules from several locales, for example use English collation rules but Spanish messages. To support that, a set of locale subcategories exist that control only a certain aspect of the localization rules:

-   `LC_COLLATE` — String sort order
-   `LC_CTYPE` — Character classification \(What is a letter? Its upper-case equivalent?\)
-   `LC_MESSAGES` — Language of messages
-   `LC_MONETARY` — Formatting of currency amounts
-   `LC_NUMERIC` — Formatting of numbers
-   `LC_TIME` — Formatting of dates and times

If you want the system to behave as if it had no locale support, use the special locale `C` or `POSIX`.

The nature of some locale categories is that their value has to be fixed for the lifetime of a Greenplum Database system. That is, once `gpinitsystem` has run, you cannot change them anymore. `LC_COLLATE` and `LC_CTYPE` are those categories. They affect the sort order of indexes, so they must be kept fixed, or indexes on text columns will become corrupt. Greenplum Database enforces this by recording the values of `LC_COLLATE` and `LC_CTYPE` that are seen by gpinitsystem. The server automatically adopts those two values based on the locale that was chosen at initialization time.

The other locale categories can be changed as desired whenever the server is running by setting the server configuration parameters that have the same name as the locale categories \(see the *Greenplum Database Reference Guide* for more information on setting server configuration parameters\). The defaults that are chosen by gpinitsystem are written into the coordinator and segment `postgresql.conf` configuration files to serve as defaults when the Greenplum Database system is started. If you delete these assignments from the coordinator and each segment `postgresql.conf` files then the server will inherit the settings from its execution environment.

Note that the locale behavior of the server is determined by the environment variables seen by the server, not by the environment of any client. Therefore, be careful to configure the correct locale settings on each Greenplum Database host \(coordinator and segments\) before starting the system. A consequence of this is that if client and server are set up in different locales, messages may appear in different languages depending on where they originated.

Inheriting the locale from the execution environment means the following on most operating systems: For a given locale category, say the collation, the following environment variables are consulted in this order until one is found to be set: `LC_ALL`, `LC_COLLATE` \(the variable corresponding to the respective category\), `LANG`. If none of these environment variables are set then the locale defaults to `C`.

Some message localization libraries also look at the environment variable `LANGUAGE` which overrides all other locale settings for the purpose of setting the language of messages. If in doubt, please refer to the documentation for your operating system, in particular the documentation about `gettext`, for more information.

Native language support \(NLS\), which enables messages to be translated to the user's preferred language, is not enabled in Greenplum Database for languages other than English. This is independent of the other locale support.

### <a id="topic3"></a>Locale Behavior 

The locale settings influence the following SQL features:

-   Sort order in queries using `ORDER BY` on textual data
-   The ability to use indexes with `LIKE` clauses
-   The `upper`, `lower`, and `initcap` functions
-   The `to_char` family of functions

The drawback of using locales other than `C` or `POSIX` in Greenplum Database is its performance impact. It slows character handling and prevents ordinary indexes from being used by `LIKE`. For this reason use locales only if you actually need them.

### <a id="topic4"></a>Troubleshooting Locales 

If locale support does not work as expected, check that the locale support in your operating system is correctly configured. To check what locales are installed on your system, you may use the command `locale -a` if your operating system provides it.

Check that Greenplum Database is actually using the locale that you think it is. `LC_COLLATE` and `LC_CTYPE` settings are determined at initialization time and cannot be changed without redoing [gpinitsystem](../utility_guide/ref/gpinitsystem.html). Other locale settings including `LC_MESSAGES` and `LC_MONETARY` are initially determined by the operating system environment of the coordinator and/or segment host, but can be changed after initialization by editing the `postgresql.conf` file of each Greenplum coordinator and segment instance. You can check the active locale settings of the coordinator host using the `SHOW` command. Note that every host in your Greenplum Database array should be using identical locale settings.

## <a id="topic5"></a>Character Set Support 

The character set support in Greenplum Database allows you to store text in a variety of character sets, including single-byte character sets such as the ISO 8859 series and multiple-byte character sets such as EUC \(Extended Unix Code\), UTF-8, and Mule internal code. All supported character sets can be used transparently by clients, but a few are not supported for use within the server \(that is, as a server-side encoding\). The default character set is selected while initializing your Greenplum Database array using `gpinitsystem`. It can be overridden when you create a database, so you can have multiple databases each with a different character set.

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
|SQL\_ASCII|unspecified[2](#jk168303)|any|No|1| |
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

## <a id="topic6"></a>Setting the Character Set 

gpinitsystem defines the default character set for a Greenplum Database system by reading the setting of the `ENCODING` parameter in the `gp_init_config` file at initialization time. The default character set is `UNICODE` or `UTF8`.

You can create a database with a different character set besides what is used as the system-wide default. For example:

```
=> CREATE DATABASE korean WITH ENCODING 'EUC_KR';
```

**Important:** Although you can specify any encoding you want for a database, it is unwise to choose an encoding that is not what is expected by the locale you have selected. The `LC_COLLATE` and `LC_CTYPE` settings imply a particular encoding, and locale-dependent operations \(such as sorting\) are likely to misinterpret data that is in an incompatible encoding.

Since these locale settings are frozen by gpinitsystem, the apparent flexibility to use different encodings in different databases is more theoretical than real.

One way to use multiple encodings safely is to set the locale to `C` or `POSIX` during initialization time, thus disabling any real locale awareness.

## <a id="topic7"></a>Character Set Conversion Between Server and Client 

Greenplum Database supports automatic character set conversion between server and client for certain character set combinations. The conversion information is stored in the coordinator `pg_conversion` system catalog table. Greenplum Database comes with some predefined conversions or you can create a new conversion using the SQL command `CREATE CONVERSION`.

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
-   Using `SET client_encoding TO`. Setting the client encoding can be done with this SQL command:

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
-   Setting the configuration parameter `client_encoding`. If `client_encoding` is set in the coordinator `postgresql.conf` file, that client encoding is automatically selected when a connection to Greenplum Database is made. \(This can subsequently be overridden using any of the other methods mentioned above.\)

If the conversion of a particular character is not possible — suppose you chose `EUC_JP` for the server and `LATIN1` for the client, then some Japanese characters do not have a representation in `LATIN1` — then an error is reported.

If the client character set is defined as `SQL_ASCII`, encoding conversion is disabled, regardless of the server's character set. The use of `SQL_ASCII` is unwise unless you are working with all-ASCII data. `SQL_ASCII` is not supported as a server encoding.

1 Not all APIs support all the listed character sets. For example, the JDBC driver does not support MULE\_INTERNAL, LATIN6, LATIN8, and LATIN10. 
2 The SQL\_ASCII setting behaves considerably differently from the other settings. Byte values 0-127 are interpreted according to the ASCII standard, while byte values 128-255 are taken as uninterpreted characters. If you are working with any non-ASCII data, it is unwise to use the SQL\_ASCII setting as a client encoding. SQL\_ASCII is not supported as a server encoding.

