# CREATE COLLATION 

Defines a new collation using the specified operating system locale settings, or by copying an existing collation.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE COLLATION <name> (    
    [ LOCALE = <locale>, ]    
    [ LC_COLLATE = <lc_collate>, ]    
    [ LC_CTYPE = <lc_ctype> ])

CREATE COLLATION <name> FROM <existing_collation>
```

## <a id="section3"></a>Description 

To be able to create a collation, you must have `CREATE` privilege on the destination schema.

## <a id="section4"></a>Parameters 

name
:   The name of the collation. The collation name can be schema-qualified. If it is not, the collation is defined in the current schema. The collation name must be unique within that schema. \(The system catalogs can contain collations with the same name for other encodings, but these are ignored if the database encoding does not match.\)

locale
:   This is a shortcut for setting `LC_COLLATE` and `LC_CTYPE` at once. If you specify this, you cannot specify either of those parameters.

lc\_collate
:   Use the specified operating system locale for the `LC_COLLATE` locale category. The locale must be applicable to the current database encoding. \(See [CREATE DATABASE](CREATE_DATABASE.html) for the precise rules.\)

lc\_ctype
:   Use the specified operating system locale for the `LC_CTYPE` locale category. The locale must be applicable to the current database encoding. \(See [CREATE DATABASE](CREATE_DATABASE.html) for the precise rules.\)

existing\_collation
:   The name of an existing collation to copy. The new collation will have the same properties as the existing one, but it will be an independent object.

## <a id="section5"></a>Notes 

To be able to create a collation, you must have `CREATE` privilege on the destination schema.

Use `DROP COLLATION` to remove user-defined collations.

See [Collation Support](https://www.postgresql.org/docs/9.4/collation.html) in the PostgreSQL documentation for more information about collation support in Greenplum Database.

## <a id="section6"></a>Examples 

To create a collation from the operating system locale `fr_FR.utf8` \(assuming the current database encoding is `UTF8`\):

```
CREATE COLLATION french (LOCALE = 'fr_FR.utf8');
```

To create a collation from an existing collation:

```
CREATE COLLATION german FROM "de_DE";
```

This can be convenient to be able to use operating-system-independent collation names in applications.

## <a id="section7"></a>Compatibility 

There is a `CREATE COLLATION` statement in the SQL standard, but it is limited to copying an existing collation. The syntax to create a new collation is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[ALTER COLLATION](ALTER_COLLATION.html), [DROP COLLATION](DROP_COLLATION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

