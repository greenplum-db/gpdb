# DROP EXTENSION 

Removes an extension from a Greenplum database.

## Synopsis 

``` {#sql_command_synopsis}
DROP EXTENSION [ IF EXISTS ] <name> [, ...] [ CASCADE | RESTRICT ]
```

## Description 

`DROP EXTENSION` removes extensions from the database. Dropping an extension causes its component objects to be dropped as well.

**Note:** The required supporting extension files what were installed to create the extension are not deleted. The files must be manually removed from the Greenplum Database hosts.

You must own the extension to use `DROP EXTENSION`.

This command fails if any of the extension objects are in use in the database. For example, if a table is defined with columns of the extension type. Add the `CASCADE` option to forcibly remove those dependent objects.

**Important:** Before issuing a `DROP EXTENSION` with the `CASCADE` keyword, you should be aware of all object that depend on the extension to avoid unintended consequences.

## Parameters 

IF EXISTS
:   Do not throw an error if the extension does not exist. A notice is issued.

name
:   The name of an installed extension.

CASCADE
:   Automatically drop objects that depend on the extension, and in turn all objects that depend on those objects. See the PostgreSQL information about [Dependency Tracking](https://www.postgresql.org/docs/9.6/static/ddl-depend.html).

RESTRICT
:   Refuse to drop an extension if any objects depend on it, other than the extension member objects. This is the default.

## Compatibility 

`DROP EXTENSION` is a Greenplum Database extension.

## See Also 

[CREATE EXTENSION](CREATE_EXTENSION.html), [ALTER EXTENSION](ALTER_EXTENSION.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

