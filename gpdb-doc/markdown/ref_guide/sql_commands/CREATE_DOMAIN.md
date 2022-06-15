# CREATE DOMAIN 

Defines a new domain.

## Synopsis 

``` {#sql_command_synopsis}
CREATE DOMAIN <name> [AS] <data_type> [DEFAULT <expression>] 
       [CONSTRAINT <constraint_name>
       | NOT NULL | NULL 
       | CHECK (<expression>) [...]]
```

## Description 

`CREATE DOMAIN` creates a new domain. A domain is essentially a data type with optional constraints \(restrictions on the allowed set of values\). The user who defines a domain becomes its owner. The domain name must be unique among the data types and domains existing in its schema.

Domains are useful for abstracting common constraints on fields into a single location for maintenance. For example, several tables might contain email address columns, all requiring the same `CHECK` constraint to verify the address syntax. It is easier to define a domain rather than setting up a column constraint for each table that has an email column.

## Parameters 

name
:   The name \(optionally schema-qualified\) of a domain to be created.

data\_type
:   The underlying data type of the domain. This may include array specifiers.

DEFAULT expression
:   Specifies a default value for columns of the domain data type. The value is any variable-free expression \(but subqueries are not allowed\). The data type of the default expression must match the data type of the domain. If no default value is specified, then the default value is the null value. The default expression will be used in any insert operation that does not specify a value for the column. If a default value is defined for a particular column, it overrides any default associated with the domain. In turn, the domain default overrides any default value associated with the underlying data type.

CONSTRAINT constraint\_name
:   An optional name for a constraint. If not specified, the system generates a name.

NOT NULL
:   Values of this domain are not allowed to be null.

NULL
:   Values of this domain are allowed to be null. This is the default. This clause is only intended for compatibility with nonstandard SQL databases. Its use is discouraged in new applications.

CHECK \(expression\)
:   `CHECK` clauses specify integrity constraints or tests which values of the domain must satisfy. Each constraint must be an expression producing a Boolean result. It should use the key word `VALUE` to refer to the value being tested. Currently, `CHECK` expressions cannot contain subqueries nor refer to variables other than `VALUE`.

## Examples 

Create the `us_zip_code` data type. A regular expression test is used to verify that the value looks like a valid US zip code.

```
CREATE DOMAIN us_zip_code AS TEXT CHECK 
       ( VALUE ~ '^\\d{5}$' OR VALUE ~ '^\\d{5}-\\d{4}$' );
```

## Compatibility 

`CREATE DOMAIN` conforms to the SQL standard.

## See Also 

[ALTER DOMAIN](ALTER_DOMAIN.html), [DROP DOMAIN](DROP_DOMAIN.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

