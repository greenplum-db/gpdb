---
title: CREATE DOMAIN 
---

Defines a new domain.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE DOMAIN <name> [AS] <data_type> [DEFAULT <expression>]
       [ COLLATE <collation> ] 
       [ CONSTRAINT <constraint_name>
       | NOT NULL | NULL 
       | CHECK (<expression>) [...]]
```

## <a id="section3"></a>Description 

`CREATE DOMAIN` creates a new domain. A domain is essentially a data type with optional constraints \(restrictions on the allowed set of values\). The user who defines a domain becomes its owner. The domain name must be unique among the data types and domains existing in its schema.

If a schema name is given \(for example, `CREATE DOMAIN myschema.mydomain ...`\) then the domain is created in the specified schema. Otherwise it is created in the current schema.

Domains are useful for abstracting common constraints on fields into a single location for maintenance. For example, several tables might contain email address columns, all requiring the same `CHECK` constraint to verify the address syntax. It is easier to define a domain rather than setting up a column constraint for each table that has an email column.

To be able to create a domain, you must have `USAGE` privilege on the underlying type.

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of a domain to be created.

data\_type
:   The underlying data type of the domain. This may include array specifiers.

DEFAULT expression
:   Specifies a default value for columns of the domain data type. The value is any variable-free expression \(but subqueries are not allowed\). The data type of the default expression must match the data type of the domain. If no default value is specified, then the default value is the null value. The default expression will be used in any insert operation that does not specify a value for the column. If a default value is defined for a particular column, it overrides any default associated with the domain. In turn, the domain default overrides any default value associated with the underlying data type.

COLLATE collation
:   An optional collation for the domain. If no collation is specified, the underlying data type's default collation is used. The underlying type must be collatable if `COLLATE` is specified.

CONSTRAINT constraint\_name
:   An optional name for a constraint. If not specified, the system generates a name.

NOT NULL
:   Values of this domain are normally prevented from being null. However, it is still possible for a domain with this constraint to take a null value if it is assigned a matching domain type that has become null, e.g. via a left outer join, or a command such as `INSERT INTO tab (domcol) VALUES ((SELECT domcol FROM tab WHERE false))`.

NULL
:   Values of this domain are allowed to be null. This is the default. This clause is only intended for compatibility with nonstandard SQL databases. Its use is discouraged in new applications.

CHECK \(expression\)
:   `CHECK` clauses specify integrity constraints or tests which values of the domain must satisfy. Each constraint must be an expression producing a Boolean result. It should use the key word `VALUE` to refer to the value being tested. Currently, `CHECK` expressions cannot contain subqueries nor refer to variables other than `VALUE`.

## <a id="section5"></a>Examples 

Create the `us_zip_code` data type. A regular expression test is used to verify that the value looks like a valid US zip code.

```
CREATE DOMAIN us_zip_code AS TEXT CHECK 
       ( VALUE ~ '^\d{5}$' OR VALUE ~ '^\d{5}-\d{4}$' );
```

## <a id="section6"></a>Compatibility 

`CREATE DOMAIN` conforms to the SQL standard.

## <a id="section7"></a>See Also 

[ALTER DOMAIN](ALTER_DOMAIN.html), [DROP DOMAIN](DROP_DOMAIN.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

