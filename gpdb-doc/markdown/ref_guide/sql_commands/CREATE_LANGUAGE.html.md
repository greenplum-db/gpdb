# CREATE LANGUAGE 

Defines a new procedural language.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE [ OR REPLACE ] [ PROCEDURAL ] LANGUAGE <name>

CREATE [ OR REPLACE ] [ TRUSTED ] [ PROCEDURAL ] LANGUAGE <name>
    HANDLER <call_handler>
   [ INLINE <inline_handler> ] 
   [ VALIDATOR <valfunction> ]
            
```

## <a id="section3"></a>Description 

`CREATE LANGUAGE` registers a new procedural language with a Greenplum database. Subsequently, functions and procedures can be defined in this new language.

> **Note** Procedural languages for Greenplum Database have been made into "extensions," and should therefore be installed with [CREATE EXTENSION](CREATE_EXTENSION.html), not `CREATE LANGUAGE`. Using `CREATE LANGUAGE` directly should be restricted to extension installation scripts. If you have a "bare" language in your database, perhaps as a result of an upgrade, you can convert it to an extension using `CREATE EXTENSION <langname> FROM unpackaged`.

`CREATE LANGUAGE` effectively associates the language name with handler function\(s\) that are responsible for executing functions written in the language.

There are two forms of the `CREATE LANGUAGE` command. In the first form, the user supplies just the name of the desired language, and the Greenplum Database server consults the `pg_pltemplate` system catalog to determine the correct parameters. In the second form, the user supplies the language parameters along with the language name. The second form can be used to create a language that is not defined in `pg_pltemplate`, but this approach is considered obsolete.

When the server finds an entry in the `pg_pltemplate` catalog for the given language name, it will use the catalog data even if the command includes language parameters. This behavior simplifies loading of old dump files, which are likely to contain out-of-date information about language support functions.

Ordinarily, the user must have the Greenplum Database superuser privilege to register a new language. However, the owner of a database can register a new language within that database if the language is listed in the `pg_pltemplate` catalog and is marked as allowed to be created by database owners \(`tmpldbacreate` is `true`\). The default is that trusted languages can be created by database owners, but this can be adjusted by superusers by modifying the contents of `pg_pltemplate`. The creator of a language becomes its owner and can later drop it, rename it, or assign it to a new owner.

`CREATE OR REPLACE LANGUAGE` will either create a new language, or replace an existing definition. If the language already exists, its parameters are updated according to the values specified or taken from `pg_pltemplate`, but the language's ownership and permissions settings do not change, and any existing functions written in the language are assumed to still be valid. In addition to the normal privilege requirements for creating a language, the user must be superuser or owner of the existing language. The `REPLACE` case is mainly meant to be used to ensure that the language exists. If the language has a `pg_pltemplate` entry then `REPLACE` will not actually change anything about an existing definition, except in the unusual case where the `pg_pltemplate` entry has been modified since the language was created.


## <a id="section4"></a>Parameters 

TRUSTED
:   `TRUSTED` specifies that the language does not grant access to data that the user would not otherwise have. If this key word is omitted when registering the language, only users with the Greenplum Database superuser privilege can use this language to create new functions.

PROCEDURAL
:   This is a noise word.

name
:   The name of the new procedural language. The name must be unique among the languages in the database.
:   For backward compatibility, the name can be enclosed by single quotes.

HANDLER call\_handler
:   The name of a previously registered function that will be called to run the procedural language's functions. The call handler for a procedural language must be written in a compiled language such as C with version 1 call convention and registered with Greenplum Database as a function taking no arguments and returning the `language_handler` type, a placeholder type that is simply used to identify the function as a call handler.

INLINE inline\_handler
:   The name of a previously registered function that is called to run an anonymous code block in this language that is created with the [DO](DO.html) command. If no inline\_handler function is specified, the language does not support anonymous code blocks. The handler function must take one argument of type `internal`, which is the [DO](DO.html) command internal representation. The function typically returns `void`. The return value of the handler is ignored.

VALIDATOR valfunction
:   The name of a previously registered function that will be called when a new function in the language is created, to validate the new function. If no validator function is specified, then Greenplum Database will not check a new function when it is created. The validator function must take one argument of type `oid`, which will be the OID of the to-be-created function, and will typically return `void`.
:   A validator function would typically inspect the function body for syntactical correctness, but it can also look at other properties of the function, for example if the language cannot handle certain argument types. To signal an error, the validator function should use the `ereport()` function. The return value of the function is ignored.

> **Note**  The `TRUSTED` option and the support function name\(s\) are ignored if the server has an entry for the specified language name in `pg_pltemplate`.

## <a id="section5"></a>Notes 

Use [DROP LANGUAGE](DROP_LANGUAGE.html) to drop procedural languages.

The system catalog [pg_language](../system_catalogs/pg_language.html) records information about the currently installed languages. Also, the [psql](../../utility_guide/ref/psql.html) command `\dL` lists the installed languages.

To create functions in a procedural language, a user must have the `USAGE` privilege for the language. By default, `USAGE` is granted to `PUBLIC` \(everyone\) for trusted languages. This may be revoked if desired.

Procedural languages are local to individual databases. However, a language can be installed into the `template1` database, which will cause it to be available automatically in all subsequently-created databases.

The call handler function, the inline handler function \(if any\), and the validator function \(if any\) must already exist if the server does not have an entry for the language in `pg_pltemplate`. But when there is an entry, the functions need not already exist; they will be automatically defined if not present in the database. \(This might result in `CREATE LANGUAGE` failing, if the shared library that implements the language is not available in the installation.\)

## <a id="section6"></a>Examples 

The preferred way of creating any of the standard procedural languages is to use `CREATE EXTENSION` instead of `CREATE LANGUAGE`. For example:

```
CREATE EXTENSION plperl;
```

For a language not known in the `pg_pltemplate` catalog, a sequence such as this is needed:

```
CREATE FUNCTION plsample_call_handler() RETURNS 
language_handler
    AS '$libdir/plsample'
    LANGUAGE C;
CREATE LANGUAGE plsample
    HANDLER plsample_call_handler;
```

## <a id="section7"></a>Compatibility 

`CREATE LANGUAGE` is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[ALTER LANGUAGE](ALTER_LANGUAGE.html), [CREATE EXTENSION](CREATE_EXTENSION.html), [CREATE FUNCTION](CREATE_FUNCTION.html), [DROP LANGUAGE](DROP_LANGUAGE.html), [GRANT](GRANT.html), [DO](DO.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

