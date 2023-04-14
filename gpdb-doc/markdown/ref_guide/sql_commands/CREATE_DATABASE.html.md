# CREATE DATABASE 

Creates a new database.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE DATABASE name [ [WITH] [OWNER [=] <user_name>]
                     [TEMPLATE [=] <template>]
                     [ENCODING [=] <encoding>]
                     [LC_COLLATE [=] <lc_collate>]
                     [LC_CTYPE [=] <lc_ctype>]
                     [TABLESPACE [=] <tablespace>]
                     [CONNECTION LIMIT [=] connlimit ] ]
```

## <a id="section3"></a>Description 

`CREATE DATABASE` creates a new database. To create a database, you must be a superuser or have the special `CREATEDB` privilege.

The creator becomes the owner of the new database by default. Superusers can create databases owned by other users by using the `OWNER` clause. They can even create databases owned by users with no special privileges. Non-superusers with `CREATEDB` privilege can only create databases owned by themselves.

By default, the new database will be created by cloning the standard system database `template1`. A different template can be specified by writing `TEMPLATE name`. In particular, by writing `TEMPLATE template0`, you can create a clean database containing only the standard objects predefined by Greenplum Database. This is useful if you wish to avoid copying any installation-local objects that may have been added to `template1`.

## <a id="section4"></a>Parameters 

name
:   The name of a database to create.

user\_name
:   The name of the database user who will own the new database, or `DEFAULT` to use the default owner \(the user running the command\).

template
:   The name of the template from which to create the new database, or `DEFAULT` to use the default template \(template1\).

encoding
:   Character set encoding to use in the new database. Specify a string constant \(such as `'SQL_ASCII'`\), an integer encoding number, or `DEFAULT` to use the default encoding. For more information, see [Character Set Support](../character_sets.html).

lc\_collate
:   The collation order \(`LC_COLLATE`\) to use in the new database. This affects the sort order applied to strings, e.g. in queries with `ORDER BY`, as well as the order used in indexes on text columns. The default is to use the collation order of the template database. See the Notes section for additional restrictions.

lc\_ctype
:   The character classification \(`LC_CTYPE`\) to use in the new database. This affects the categorization of characters, e.g. lower, upper and digit. The default is to use the character classification of the template database. See below for additional restrictions.

tablespace
:   The name of the tablespace that will be associated with the new database, or `DEFAULT` to use the template database's tablespace. This tablespace will be the default tablespace used for objects created in this database.

connlimit
:   The maximum number of concurrent connections possible. The default of -1 means there is no limitation.

## <a id="section5"></a>Notes 

`CREATE DATABASE` cannot be run inside a transaction block.

When you copy a database by specifying its name as the template, no other sessions can be connected to the template database while it is being copied. New connections to the template database are locked out until `CREATE DATABASE` completes.

The `CONNECTION LIMIT` is not enforced against superusers.

The character set encoding specified for the new database must be compatible with the chosen locale settings \(`LC_COLLATE` and `LC_CTYPE`\). If the locale is `C` \(or equivalently `POSIX`\), then all encodings are allowed, but for other locale settings there is only one encoding that will work properly. `CREATE DATABASE` will allow superusers to specify `SQL_ASCII` encoding regardless of the locale settings, but this choice is deprecated and may result in misbehavior of character-string functions if data that is not encoding-compatible with the locale is stored in the database.

The encoding and locale settings must match those of the template database, except when `template0` is used as template. This is because `COLLATE` and `CTYPE` affect the ordering in indexes, so that any indexes copied from the template database would be invalid in the new database with different settings. `template0`, however, is known to not contain any data or indexes that would be affected.

## <a id="section6"></a>Examples 

To create a new database:

```
CREATE DATABASE gpdb;
```

To create a database `sales` owned by user `salesapp` with a default tablespace of `salesspace`:

```
CREATE DATABASE sales OWNER salesapp TABLESPACE salesspace;
```

To create a database `music` which supports the ISO-8859-1 character set:

```
CREATE DATABASE music ENCODING 'LATIN1' TEMPLATE template0;
```

In this example, the `TEMPLATE template0` clause would only be required if `template1`'s encoding is not ISO-8859-1. Note that changing encoding might require selecting new `LC_COLLATE` and `LC_CTYPE` settings as well.

## <a id="section7"></a>Compatibility 

There is no `CREATE DATABASE` statement in the SQL standard. Databases are equivalent to catalogs, whose creation is implementation-defined.

## <a id="section8"></a>See Also 

[ALTER DATABASE](ALTER_DATABASE.html), [DROP DATABASE](DROP_DATABASE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

