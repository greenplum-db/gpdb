# GRANT 

Defines access privileges.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
GRANT { {SELECT | INSERT | UPDATE | DELETE | REFERENCES | 
TRIGGER | TRUNCATE } [, ...] | ALL [PRIVILEGES] }
    ON { [TABLE] <table_name> [, ...]
         | ALL TABLES IN SCHEMA <schema_name> [, ...] }
    TO { [ GROUP ] <role_name> | PUBLIC} [, ...] [ WITH GRANT OPTION ] 

GRANT { { SELECT | INSERT | UPDATE | REFERENCES } ( <column_name> [, ...] )
    [, ...] | ALL [ PRIVILEGES ] ( <column_name> [, ...] ) }
    ON [ TABLE ] <table_name> [, ...]
    TO { <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { {USAGE | SELECT | UPDATE} [, ...] | ALL [PRIVILEGES] }
    ON { SEQUENCE <sequence_name> [, ...]
         | ALL SEQUENCES IN SCHEMA <schema_name> [, ...] }
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ] 

GRANT { {CREATE | CONNECT | TEMPORARY | TEMP} [, ...] | ALL 
[PRIVILEGES] }
    ON DATABASE <database_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON DOMAIN <domain_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON FOREIGN DATA WRAPPER <fdw_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON FOREIGN SERVER <server_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { EXECUTE | ALL [PRIVILEGES] }
    ON { FUNCTION <function_name> ( [ [ <argmode> ] [ <argname> ] <argtype> [, ...] 
] ) [, ...]
        | ALL FUNCTIONS IN SCHEMA <schema_name> [, ...] }
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [PRIVILEGES] }
    ON LANGUAGE <lang_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { { CREATE | USAGE } [, ...] | ALL [PRIVILEGES] }
    ON SCHEMA <schema_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC}  [, ...] [ WITH GRANT OPTION ]

GRANT { CREATE | ALL [PRIVILEGES] }
    ON TABLESPACE <tablespace_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON TYPE <type_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT <parent_role> [, ...] 
    TO <member_role> [, ...] [WITH ADMIN OPTION]

GRANT { SELECT | INSERT | ALL [PRIVILEGES] } 
    ON PROTOCOL <protocolname>
    TO <username>
```

## <a id="section3"></a>Description 

Greenplum Database unifies the concepts of users and groups into a single kind of entity called a role. It is therefore not necessary to use the keyword `GROUP` to identify whether a grantee is a user or a group. `GROUP` is still allowed in the command, but it is a noise word.

The `GRANT` command has two basic variants: one that grants privileges on a database object \(table, column, view, foreign table, sequence, database, foreign-data wrapper, foreign server, function, procedural language, schema, or tablespace\), and one that grants membership in a role.

**GRANT on Database Objects**

This variant of the `GRANT` command gives specific privileges on a database object to one or more roles. These privileges are added to those already granted, if any.

There is also an option to grant privileges on all objects of the same type within one or more schemas. This functionality is currently supported only for tables, sequences, and functions \(but note that `ALL TABLES` is considered to include views and foreign tables\).

The keyword `PUBLIC` indicates that the privileges are to be granted to all roles, including those that may be created later. `PUBLIC` may be thought of as an implicitly defined group-level role that always includes all roles. Any particular role will have the sum of privileges granted directly to it, privileges granted to any role it is presently a member of, and privileges granted to `PUBLIC`.

If `WITH GRANT OPTION` is specified, the recipient of the privilege may in turn grant it to others. Without a grant option, the recipient cannot do that. Grant options cannot be granted to `PUBLIC`.

There is no need to grant privileges to the owner of an object \(usually the role that created it\), as the owner has all privileges by default. \(The owner could, however, choose to revoke some of their own privileges for safety.\)

The right to drop an object, or to alter its definition in any way is not treated as a grantable privilege; it is inherent in the owner, and cannot be granted or revoked. \(However, a similar effect can be obtained by granting or revoking membership in the role that owns the object; see below.\) The owner implicitly has all grant options for the object, too.

Greenplum Database grants default privileges on some types of objects to `PUBLIC`. No privileges are granted to `PUBLIC` by default on tables, table columns, sequences, foreign-data wrappers, foreign servers, large objects, schemas, or tablespaces. For other types of objects, the default privileges granted to `PUBLIC` are as follows:

-   `CONNECT` and `TEMPORARY` \(create temporary tables\) privileges for databases,
-   `EXECUTE` privilege for functions, and
-   `USAGE` privilege for languages and data types \(including domains\).

The object owner can, of course, `REVOKE` both default and expressly granted privileges. \(For maximum security, issue the `REVOKE` in the same transaction that creates the object; then there is no window in which another user can use the object.\)

\>

**GRANT on Roles**

This variant of the `GRANT` command grants membership in a role to one or more other roles. Membership in a role is significant because it conveys the privileges granted to a role to each of its members.

If `WITH ADMIN OPTION` is specified, the member may in turn grant membership in the role to others, and revoke membership in the role as well. Without the admin option, ordinary users cannot do that. A role is not considered to hold `WITH ADMIN OPTION` on itself, but it may grant or revoke membership in itself from a database session where the session user matches the role. Database superusers can grant or revoke membership in any role to anyone. Roles having `CREATEROLE` privilege can grant or revoke membership in any role that is not a superuser.

Unlike the case with privileges, membership in a role cannot be granted to `PUBLIC`.

**GRANT on Protocols**

You can also use the `GRANT` command to specify which users can access a trusted protocol. \(If the protocol is not trusted, you cannot give any other user permission to use it to read or write data.\)

-   To allow a user to create a readable external table with a trusted protocol:

    ```
    GRANT SELECT ON PROTOCOL <protocolname> TO <username>
    ```

-   To allow a user to create a writable external table with a trusted protocol:

    ```
    GRANT INSERT ON PROTOCOL <protocolname> TO <username>
    ```

-   To allow a user to create both readable and writable external table with a trusted protocol:

    ```
    GRANT ALL ON PROTOCOL <protocolname> TO <username>
    ```


You can also use this command to grant users permissions to create and use `s3` and `pxf` external tables. However, external tables of type `http`, `https`, `gpfdist`, and `gpfdists`, are implemented internally in Greenplum Database instead of as custom protocols. For these types, use the `CREATE ROLE` or `ALTER ROLE` command to set the `CREATEEXTTABLE` or `NOCREATEEXTTABLE` attribute for each user. See [CREATE ROLE](CREATE_ROLE.html) for syntax and examples.

## <a id="section7"></a>Parameters 

SELECT
:   Allows `SELECT` from any column, or the specific columns listed, of the specified table, view, or sequence. Also allows the use of `COPY TO`. This privilege is also needed to reference existing column values in `UPDATE` or `DELETE`.

INSERT
:   Allows `INSERT` of a new row into the specified table. If specific columns are listed, only those columns may be assigned to in the `INSERT` command \(other columns will receive default values\). Also allows `COPY FROM`.

UPDATE
:   Allows `UPDATE` of any column, or the specific columns listed, of the specified table. `SELECT ... FOR UPDATE` and `SELECT ... FOR SHARE` also require this privilege on at least one column, \(as well as the `SELECT` privilege\). For sequences, this privilege allows the use of the `nextval()` and `setval()` functions.

DELETE
:   Allows `DELETE` of a row from the specified table.

REFERENCES
:   This keyword is accepted, although foreign key constraints are currently not supported in Greenplum Database. To create a foreign key constraint, it is necessary to have this privilege on both the referencing and referenced columns. The privilege may be granted for all columns of a table, or just specific columns.

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

:   For sequences, this privilege allows the use of the `currval()` and `nextval()` function.

:   For types and domains, this privilege allows the use of the type or domain in the creation of tables, functions, and other schema objects. \(Note that it does not control general "usage" of the type, such as values of the type appearing in queries. It only prevents objects from being created that depend on the type. The main purpose of the privilege is controlling which users create dependencies on a type, which could prevent the owner from changing the type later.\)

:   For foreign-data wrappers, this privilege enables the grantee to create new servers using that foreign-data wrapper.

:   For servers, this privilege enables the grantee to create foreign tables using the server, and also to create, alter, or drop their own user's user mappings associated with that server.

ALL PRIVILEGES
:   Grant all of the available privileges at once. The `PRIVILEGES` key word is optional in Greenplum Database, though it is required by strict SQL.

PUBLIC
:   A special group-level role that denotes that the privileges are to be granted to all roles, including those that may be created later.

WITH GRANT OPTION
:   The recipient of the privilege may in turn grant it to others.

WITH ADMIN OPTION
:   The member of a role may in turn grant membership in the role to others.

## <a id="section8"></a>Notes 

A user may perform `SELECT`, `INSERT`, and so forth, on a column if they hold that privilege for either the specific column or the whole table. Granting the privilege at the table level and then revoking it for one column does not do what you might wish: the table-level grant is unaffected by a column-level operation.

Database superusers can access all objects regardless of object privilege settings. One exception to this rule is view objects. Access to tables referenced in the view is determined by permissions of the view owner not the current user \(even if the current user is a superuser\).

If a superuser chooses to issue a `GRANT` or `REVOKE` command, the command is performed as though it were issued by the owner of the affected object. In particular, privileges granted via such a command will appear to have been granted by the object owner. For role membership, the membership appears to have been granted by the containing role itself.

`GRANT` and `REVOKE` can also be done by a role that is not the owner of the affected object, but is a member of the role that owns the object, or is a member of a role that holds privileges `WITH GRANT OPTION` on the object. In this case the privileges will be recorded as having been granted by the role that actually owns the object or holds the privileges `WITH GRANT OPTION`.

Granting permission on a table does not automatically extend permissions to any sequences used by the table, including sequences tied to `SERIAL` columns. Permissions on a sequence must be set separately.

The `GRANT` command cannot be used to set privileges for the protocols `file`, `gpfdist`, or `gpfdists`. These protocols are implemented internally in Greenplum Database. Instead, use the [CREATE ROLE](CREATE_ROLE.html) or [ALTER ROLE](ALTER_ROLE.html) command to set the `CREATEEXTTABLE` attribute for the role.

Use `psql`'s `\dp` meta-command to obtain information about existing privileges for tables and columns. There are other `\d` meta-commands that you can use to display the privileges of non-table objects.

## <a id="section9"></a>Examples 

Grant insert privilege to all roles on table `mytable`:

```
GRANT INSERT ON mytable TO PUBLIC;
```

Grant all available privileges to role `sally` on the view `topten`. Note that while the above will indeed grant all privileges if run by a superuser or the owner of `topten`, when run by someone else it will only grant those permissions for which the granting role has grant options.

```
GRANT ALL PRIVILEGES ON topten TO sally;
```

Grant membership in role `admins` to user `joe`:

```
GRANT admins TO joe;
```

## <a id="section10"></a>Compatibility 

The `PRIVILEGES` key word is required in the SQL standard, but optional in Greenplum Database. The SQL standard does not support setting the privileges on more than one object per command.

Greenplum Database allows an object owner to revoke their own ordinary privileges: for example, a table owner can make the table read-only to theirself by revoking their own `INSERT`, `UPDATE`, `DELETE`, and `TRUNCATE` privileges. This is not possible according to the SQL standard. Greenplum Database treats the owner's privileges as having been granted by the owner to the owner; therefore they can revoke them too. In the SQL standard, the owner's privileges are granted by an assumed *system* entity.

The SQL standard provides for a `USAGE` privilege on other kinds of objects: character sets, collations, translations.

In the SQL standard, sequences only have a `USAGE` privilege, which controls the use of the `NEXT VALUE FOR` expression, which is equivalent to the function `nextval` in Greenplum Database. The sequence privileges `SELECT` and `UPDATE` are Greenplum Database extensions. The application of the sequence `USAGE` privilege to the `currval` function is also a Greenplum Database extension \(as is the function itself\).

Privileges on databases, tablespaces, schemas, and languages are Greenplum Database extensions.

## <a id="section11"></a>See Also 

[REVOKE](REVOKE.html), [CREATE ROLE](CREATE_ROLE.html), [ALTER ROLE](ALTER_ROLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

