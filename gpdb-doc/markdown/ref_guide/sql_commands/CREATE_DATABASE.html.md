# CREATE DATABASE 

Creates a new database.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE DATABASE name
    [ [WITH] [OWNER [=] <user_name>]
          [TEMPLATE [=] <template>]
          [ENCODING [=] <encoding>]
          [LC_COLLATE [=] <lc_collate>]
          [LC_CTYPE [=] <lc_ctype>]
          [TABLESPACE [=] <tablespace_name>]
          [ALLOW_CONNECTIONS [=] <allowconn>]
          [CONNECTION LIMIT [=] <connlimit> ]
          [IS_TEMPLATE [=] <istemplate> ] ]
```

## <a id="section3"></a>Description 

`CREATE DATABASE` creates a new database.

To create a database, you must be a superuser or have the special `CREATEDB` privilege. See [CREATE ROLE](CREATE_ROLE.html).

The creator becomes the owner of the new database by default. Superusers can create databases owned by other users by using the `OWNER` clause. They can even create databases owned by users with no special privileges. Non-superusers with `CREATEDB` privilege can only create databases owned by themselves.

By default, the new database will be created by cloning the standard system database `template1`. A different template can be specified by writing `TEMPLATE name`. In particular, by writing `TEMPLATE template0`, you can create a clean database containing only the standard objects predefined by Greenplum Database. This is useful if you wish to avoid copying any installation-local objects that may have been added to `template1`.

## <a id="section4"></a>Parameters 

name
:   The name of a database to create.

user\_name
:   The name of the database user who will own the new database, or `DEFAULT` to use the default owner \(the user running the command\). To create a database owned by another role, you must be a direct or indirect member of that role, or be a superuser.

template
:   The name of the template from which to create the new database, or `DEFAULT` to use the default template \(template1\).

encoding
:   Character set encoding to use in the new database. Specify a string constant \(such as `'SQL_ASCII'`\), an integer encoding number, or `DEFAULT` to use the default encoding  \(namely, the encoding of the template database\). For more information, see [Character Set Support](../character_sets.html). Refer to the [Notes](#section5) for additional restrictions.

lc\_collate
:   The collation order \(`LC_COLLATE`\) to use in the new database. This affects the sort order applied to strings, e.g. in queries with `ORDER BY`, as well as the order used in indexes on text columns. The default is to use the collation order of the template database. See the [Notes](#section5) for additional restrictions.

lc\_ctype
:   The character classification \(`LC_CTYPE`\) to use in the new database. This affects the categorization of characters, e.g. lower, upper, and digit. The default is to use the character classification of the template database. See the [Notes](#section5) for additional restrictions.

tablespace\_name
:   The name of the tablespace that will be associated with the new database, or `DEFAULT` to use the template database's tablespace. This tablespace will be the default tablespace used for objects created in this database. See [CREATE TABLESPACE](CREATE_TABLESPACE.html) for more information.

allowconn
:   If `false`, then no one can connect to this database. The default is `true`, allowing connections \(except as restricted by other mechanisms, such as `GRANT/REVOKE CONNECT`\).

connlimit
:   The maximum number of concurrent connections allowed to this database on the coordinator. The default is `-1`, no limit. Greenplum Database superusers are exempt from this limit.

istemplate
:   If `true`, then this database can be cloned by any user with `CREATEDB` privileges; if `false` \(the default\), then only superusers or the owner of the database can clone it. Note that template databases cannot be dropped.

Optional parameters can be written in any order, not only the order illustrated above.

## <a id="section5"></a>Notes 

`CREATE DATABASE` cannot be run inside a transaction block.

Errors along the line of “could not initialize database directory” are most likely related to insufficient permissions on the data directory, a full disk, or other file system problems.

Use [DROP DATABASE](DROP_DATABASE.HTML) to remove a database.

The program [createdb](../../utility_guide/ref/createdb.html) is a wrapper program around this command, provided for convenience.

Database-level configuration parameters \(set via [ALTER DATABASE](ALTER_DATABASE.html) and database-level permissions \(set via [GRANT](GRANT.html)\) are not copied from the template database.

Although it is possible to copy a database other than `template1` by specifying its name as the template, this is not \(yet\) intended as a general-purpose `“COPY DATABASE”` facility. The principal limitation is that no other sessions can be connected to the template database while it is being copied. `CREATE DATABASE` will fail if any other connection exists when it starts; otherwise, new connections to the template database are locked out until `CREATE DATABASE` completes.

The character set encoding specified for the new database must be compatible with the chosen locale settings \(`LC_COLLATE` and `LC_CTYPE`\). If the locale is `C` \(or equivalently `POSIX`\), then all encodings are allowed, but for other locale settings there is only one encoding that will work properly. `CREATE DATABASE` will allow superusers to specify `SQL_ASCII` encoding regardless of the locale settings, but this choice is deprecated and may result in misbehavior of character-string functions if data that is not encoding-compatible with the locale is stored in the database.

The encoding and locale settings must match those of the template database, except when `template0` is used as template. This is because other databases might contain data that does not match the specified encoding, or might contain indexes whose ordering is affected by `LC_COLLATE` and `LC_CTYPE`. Copying such data would result in a database that is corrupt according to the new settings. `template0`, however, is known to not contain any data or indexes that would be affected.

The `CONNECTION LIMIT` option is enforced approximately; if two new sessions start at about the same time when just one connection “slot” remains for the database, it is possible that both will fail. Also, the limit is not enforced against superusers or background worker processes.


## <a id="section6"></a>Examples 

To create a new database:

```
CREATE DATABASE gpdb;
```

To create a database `sales` owned by user `salesapp` with a default tablespace of `salesspace`:

```
CREATE DATABASE sales OWNER salesapp TABLESPACE salesspace;
```

To create a database `music` with a different locale:

```
CREATE DATABASE music
    LC_COLLATE 'sv_SE.utf8' LC_CTYPE 'sv_SE.utf8'
    TEMPLATE template0;
```

In this example, the `TEMPLATE template0` clause is required if the specialized locale is different from the one in `template1`. \(If it is not, then specifying the locale explicitly is redundant.\)

To create a database `music2` with a different locale and a different character set encoding:

```
CREATE DATABASE music2
    LC_COLLATE 'sv_SE.iso885915' LC_CTYPE 'sv_SE.iso885915'
    ENCODING LATIN9
    TEMPLATE template0;
```

The specified locale and encoding settings must match, or an error will be reported.

Note that locale names are specific to the operating system, so that the above commands might not work in the same way everywhere.

## <a id="section7"></a>Compatibility 

There is no `CREATE DATABASE` statement in the SQL standard. Databases are equivalent to catalogs, whose creation is implementation-defined.

## <a id="section8"></a>See Also 

[ALTER DATABASE](ALTER_DATABASE.html), [DROP DATABASE](DROP_DATABASE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

