# CREATE DATABASE 

Creates a new database.

## Synopsis 

``` {#sql_command_synopsis}
CREATE DATABASE name [ [WITH] [OWNER [=] <dbowner>]
                     [TEMPLATE [=] <template>]
                     [ENCODING [=] <encoding>]
                     [TABLESPACE [=] <tablespace>]
                     [CONNECTION LIMIT [=] connlimit ] ]
```

## Description 

`CREATE DATABASE` creates a new database. To create a database, you must be a superuser or have the special `CREATEDB` privilege.

The creator becomes the owner of the new database by default. Superusers can create databases owned by other users by using the `OWNER` clause. They can even create databases owned by users with no special privileges. Non-superusers with `CREATEDB` privilege can only create databases owned by themselves.

By default, the new database will be created by cloning the standard system database `template1`. A different template can be specified by writing `TEMPLATE name`. In particular, by writing `TEMPLATE template0`, you can create a clean database containing only the standard objects predefined by Greenplum Database. This is useful if you wish to avoid copying any installation-local objects that may have been added to `template1`.

## Parameters 

name
:   The name of a database to create.

dbowner
:   The name of the database user who will own the new database, or `DEFAULT` to use the default owner \(the user executing the command\).

template
:   The name of the template from which to create the new database, or `DEFAULT` to use the default template \(template1\).

encoding
:   Character set encoding to use in the new database. Specify a string constant \(such as `'SQL_ASCII'`\), an integer encoding number, or `DEFAULT` to use the default encoding. For more information, see [Character Set Support](../character_sets.html).

tablespace
:   The name of the tablespace that will be associated with the new database, or `DEFAULT` to use the template database's tablespace. This tablespace will be the default tablespace used for objects created in this database.

connlimit
:   The maximum number of concurrent connections posible. The default of -1 means there is no limitation.

## Notes 

`CREATE DATABASE` cannot be executed inside a transaction block.

When you copy a database by specifying its name as the template, no other sessions can be connected to the template database while it is being copied. New connections to the template database are locked out until `CREATE DATABASE` completes.

The `CONNECTION LIMIT` is not enforced against superusers.

## Examples 

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
CREATE DATABASE music ENCODING 'LATIN1';
```

## Compatibility 

There is no `CREATE DATABASE` statement in the SQL standard. Databases are equivalent to catalogs, whose creation is implementation-defined.

## See Also 

[ALTER DATABASE](ALTER_DATABASE.html), [DROP DATABASE](DROP_DATABASE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

