# GRANT 

Defines access privileges.

## Synopsis 

``` {#sql_command_synopsis}
GRANT { {SELECT | INSERT | UPDATE | DELETE | REFERENCES | 
TRIGGER | TRUNCATE } [,...] | ALL [PRIVILEGES] }
    ON [TABLE] <tablename> [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT { {USAGE | SELECT | UPDATE} [,...] | ALL [PRIVILEGES] }
    ON SEQUENCE <sequencename> [, ...]
    TO { <rolename> | PUBLIC } [, ...] [WITH GRANT OPTION]

GRANT { {CREATE | CONNECT | TEMPORARY | TEMP} [,...] | ALL 
[PRIVILEGES] }
    ON DATABASE <dbname> [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON FOREIGN DATA WRAPPER <fdwname> [, ...]
    TO { [ GROUP ] <rolename> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON FOREIGN SERVER <servername> [, ...]
    TO { [ GROUP ] <rolename> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { EXECUTE | ALL [PRIVILEGES] }
    ON FUNCTION <funcname> ( [ [<argmode>] [<argname>] <argtype> [, ...] 
] ) [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT { USAGE | ALL [PRIVILEGES] }
    ON LANGUAGE <langname> [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT { {CREATE | USAGE} [,...] | ALL [PRIVILEGES] }
    ON SCHEMA <schemaname> [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT { CREATE | ALL [PRIVILEGES] }
    ON TABLESPACE <tablespacename> [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT <parent_role> [, ...] 
    TO <member_role> [, ...] [WITH ADMIN OPTION]

GRANT { SELECT | INSERT | ALL [PRIVILEGES] } 
    ON PROTOCOL <protocolname>
    TO <username>
```

## Description 

The `GRANT` command has two basic variants: one that grants privileges on a database object \(table, view, foreign table, sequence, database, foreign-data wrapper, foreign server, function, procedural language, schema, or tablespace\), and one that grants membership in a role.

**GRANT on Database Objects**

This variant of the `GRANT` command gives specific privileges on a database object to one or more roles. These privileges are added to those already granted, if any.

The key word `PUBLIC` indicates that the privileges are to be granted to all roles, including those that may be created later. `PUBLIC` may be thought of as an implicitly defined group-level role that always includes all roles. Any particular role will have the sum of privileges granted directly to it, privileges granted to any role it is presently a member of, and privileges granted to `PUBLIC`.

If `WITH GRANT OPTION` is specified, the recipient of the privilege may in turn grant it to others. Without a grant option, the recipient cannot do that. Grant options cannot be granted to `PUBLIC`.

There is no need to grant privileges to the owner of an object \(usually the role that created it\), as the owner has all privileges by default. The right to drop an object, or to alter its definition in any way is not described by a grantable privilege; it is inherent in the owner, and cannot be granted or revoked. The owner implicitly has all grant options for the object, too.

Depending on the type of object, the initial default privileges may include granting some privileges to `PUBLIC`. The default is no public access for tables, schemas, and tablespaces; `CONNECT` privilege and `TEMP` table creation privilege for databases; `EXECUTE` privilege for functions; and `USAGE` privilege for languages. The object owner may of course revoke these privileges.

**GRANT on Roles**

This variant of the `GRANT` command grants membership in a role to one or more other roles. Membership in a role is significant because it conveys the privileges granted to a role to each of its members.

If `WITH ADMIN OPTION` is specified, the member may in turn grant membership in the role to others, and revoke membership in the role as well. Database superusers can grant or revoke membership in any role to anyone. Roles having `CREATEROLE` privilege can grant or revoke membership in any role that is not a superuser.

Unlike the case with privileges, membership in a role cannot be granted to `PUBLIC`.

**GRANT on Protocols**

After creating a custom protocol, specify `CREATE TRUSTED PROTOCOL` to be able to allow any user besides the owner to access it. If the protocol is not trusted, you cannot give any other user permission to use it to read or write data. After a `TRUSTED` protocol is created, you can specify which other users can access it with the `GRANT` command.

-   To allow a user to create a readable external table with a trusted protocol

    ```
    GRANT SELECT ON PROTOCOL <protocolname> TO <username>
    ```

-   To allow a user to create a writable external table with a trusted protocol

    ```
    GRANT INSERT ON PROTOCOL <protocolname> TO <username>
    ```

-   To allow a user to create both readable and writable external tables with a trusted protocol

    ```
    GRANT ALL ON PROTOCOL <protocolname> TO <username>
    ```


You can also use this command to grant users permissions to create and use `hdfs`, `s3`, and `pxf` external tables. However, external tables of type `http`, `https`, `gpfdist`, and `gpfdists`, are implemented internally in Greenplum Database instead of as custom protocols. For these types, use the `CREATE ROLE` or `ALTER ROLE` command to set the `CREATEEXTTABLE` or `NOCREATEEXTTABLE` attribute for each user. See [CREATE ROLE](CREATE_ROLE.html) for syntax and examples.

## Parameters 

SELECT
:   Allows `SELECT` from any column of the specified table, view, or sequence. Also allows the use of `COPY TO`.

INSERT
:   Allows `INSERT` of a new row into the specified table. Also allows `COPY FROM`.

UPDATE
:   Allows `UPDATE` of any column of the specified table. `SELECT ... FOR UPDATE` and `SELECT ... FOR SHARE` also require this privilege \(as well as the `SELECT` privilege\). For sequences, this privilege allows the use of the `nextval()` and `setval()` functions.

DELETE
:   Allows `DELETE` of a row from the specified table.

REFERENCES
:   This keyword is accepted, although foreign key constraints are currently not supported in Greenplum Database. To create a foreign key constraint, it is necessary to have this privilege on both the referencing and referenced tables.

TRIGGER
:   Allows the creation of a trigger on the specified table.

    **Note:** Greenplum Database does not support triggers.

TRUNCATE
:   Allows `TRUNCATE` of all rows from the specified table.

CREATE
:   For databases, allows new schemas to be created within the database.

:   For schemas, allows new objects to be created within the schema. To rename an existing object, you must own the object and have this privilege for the containing schema.

:   For tablespaces, allows tables and indexes to be created within the tablespace, and allows databases to be created that have the tablespace as their default tablespace. \(Note that revoking this privilege will not alter the placement of existing objects.\)

CONNECT
:   Allows the user to connect to the specified database. This privilege is checked at connection startup \(in addition to checking any restrictions imposed by `pg_hba.conf`\).

TEMPORARY
TEMP
:   Allows temporary tables to be created while using the database.

EXECUTE
:   Allows the use of the specified function and the use of any operators that are implemented on top of the function. This is the only type of privilege that is applicable to functions. \(This syntax works for aggregate functions, as well.\)

USAGE
:   For procedural languages, allows the use of the specified language for the creation of functions in that language. This is the only type of privilege that is applicable to procedural languages.

:   For schemas, allows access to objects contained in the specified schema \(assuming that the objects' own privilege requirements are also met\). Essentially this allows the grantee to look up objects within the schema.

:   For sequences, this privilege allows the use of the `nextval()` function.

:   For foreign-data wrappers, this privilege enables the grantee to create new servers using that foreign-data wrapper.

:   For servers, this privilege enables the grantee to create, alter, and drop their own user's user mappings associated with that server. Also, it enables the grantee to query the options of the server and associated user mappings.

ALL PRIVILEGES
:   Grant all of the available privileges at once. The `PRIVILEGES` key word is optional in Greenplum Database, though it is required by strict SQL.

PUBLIC
:   A special group-level role that denotes that the privileges are to be granted to all roles, including those that may be created later.

WITH GRANT OPTION
:   The recipient of the privilege may in turn grant it to others.

WITH ADMIN OPTION
:   The member of a role may in turn grant membership in the role to others.

## Notes 

Database superusers can access all objects regardless of object privilege settings. One exception to this rule is view objects. Access to tables referenced in the view is determined by permissions of the view owner not the current user \(even if the current user is a superuser\).

If a superuser chooses to issue a `GRANT` or `REVOKE` command, the command is performed as though it were issued by the owner of the affected object. In particular, privileges granted via such a command will appear to have been granted by the object owner. For role membership, the membership appears to have been granted by the containing role itself.

`GRANT` and `REVOKE` can also be done by a role that is not the owner of the affected object, but is a member of the role that owns the object, or is a member of a role that holds privileges `WITH GRANT OPTION` on the object. In this case the privileges will be recorded as having been granted by the role that actually owns the object or holds the privileges `WITH GRANT OPTION`.

Granting permission on a table does not automatically extend permissions to any sequences used by the table, including sequences tied to `SERIAL` columns. Permissions on a sequence must be set separately.

Greenplum Database does not support granting or revoking privileges for individual columns of a table. One possible workaround is to create a view having just the desired columns and then grant privileges to that view.

The `GRANT` command cannot be used to set privileges for the protocols `file`, `gpfdist`, or `gpfdists`. These protocols are implemented internally in Greenplum Database. Instead, use the [CREATE ROLE](CREATE_ROLE.html) or [ALTER ROLE](ALTER_ROLE.html) command to set the `CREATEEXTTABLE` attribute for the role.

Use psql's `\z` meta-command to obtain information about existing privileges for an object.

## Examples 

Grant insert privilege to all roles on table `mytable`:

```
GRANT INSERT ON mytable TO PUBLIC;
```

Grant all available privileges to role `sally` on the view `topten`. Note that while the above will indeed grant all privileges if executed by a superuser or the owner of `topten`, when executed by someone else it will only grant those permissions for which the granting role has grant options.

```
GRANT ALL PRIVILEGES ON topten TO sally;
```

Grant membership in role `admins` to user `joe`:

```
GRANT admins TO joe;
```

## Compatibility 

The `PRIVILEGES` key word in is required in the SQL standard, but optional in Greenplum Database. The SQL standard does not support setting the privileges on more than one object per command.

Greenplum Database allows an object owner to revoke his own ordinary privileges: for example, a table owner can make the table read-only to himself by revoking his own `INSERT`, `UPDATE`, `DELETE`, and `TRUNCATE` privileges. This is not possible according to the SQL standard. Greenplum Database treats the owner's privileges as having been granted by the owner to himself; therefore he can revoke them too. In the SQL standard, the owner's privileges are granted by an assumed *system* entity.

The SQL standard allows setting privileges for individual columns within a table.

The SQL standard provides for a `USAGE` privilege on other kinds of objects: character sets, collations, translations, domains.

Privileges on databases, tablespaces, schemas, and languages are Greenplum Database extensions.

## See Also 

[REVOKE](REVOKE.html), [CREATE ROLE](CREATE_ROLE.html), [ALTER ROLE](ALTER_ROLE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

