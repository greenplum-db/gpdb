# ALTER DOMAIN 

Changes the definition of a domain.

## Synopsis 

``` {#sql_command_synopsis}
ALTER DOMAIN <name> { SET DEFAULT <expression> | DROP DEFAULT }

ALTER DOMAIN <name> { SET | DROP } NOT NULL

ALTER DOMAIN <name> ADD <domain_constraint>

ALTER DOMAIN <name> DROP CONSTRAINT <constraint_name> [RESTRICT | CASCADE]

ALTER DOMAIN <name> OWNER TO <new_owner>

ALTER DOMAIN <name> SET SCHEMA <new_schema>
```

## Description 

`ALTER DOMAIN` changes the definition of an existing domain. There are several sub-forms:

-   **SET/DROP DEFAULT** — These forms set or remove the default value for a domain. Note that defaults only apply to subsequent `INSERT` commands. They do not affect rows already in a table using the domain.
-   **SET/DROP NOT NULL** — These forms change whether a domain is marked to allow `NULL` values or to reject `NULL` values. You may only `SET NOT NULL` when the columns using the domain contain no null values.
-   **ADD domain\_constraint** — This form adds a new constraint to a domain using the same syntax as `CREATE DOMAIN`. This will only succeed if all columns using the domain satisfy the new constraint.
-   **DROP CONSTRAINT** — This form drops constraints on a domain.
-   **OWNER** — This form changes the owner of the domain to the specified user.
-   **SET SCHEMA** — This form changes the schema of the domain. Any constraints associated with the domain are moved into the new schema as well.

You must own the domain to use `ALTER DOMAIN`. To change the schema of a domain, you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the domain's schema. \(These restrictions enforce that altering the owner does not do anything you could not do by dropping and recreating the domain. However, a superuser can alter ownership of any domain anyway.\)

## Parameters 

name
:   The name \(optionally schema-qualified\) of an existing domain to alter.

domain\_constraint
:   New domain constraint for the domain.

constraint\_name
:   Name of an existing constraint to drop.

CASCADE
:   Automatically drop objects that depend on the constraint.

RESTRICT
:   Refuse to drop the constraint if there are any dependent objects. This is the default behavior.

new\_owner
:   The user name of the new owner of the domain.

new\_schema
:   The new schema for the domain.

## Examples 

To add a `NOT NULL` constraint to a domain:

```
ALTER DOMAIN zipcode SET NOT NULL;
```

To remove a `NOT NULL` constraint from a domain:

```
ALTER DOMAIN zipcode DROP NOT NULL;
```

To add a check constraint to a domain:

```
ALTER DOMAIN zipcode ADD CONSTRAINT zipchk CHECK 
(char_length(VALUE) = 5);
```

To remove a check constraint from a domain:

```
ALTER DOMAIN zipcode DROP CONSTRAINT zipchk;
```

To move the domain into a different schema:

```
ALTER DOMAIN zipcode SET SCHEMA customers;
```

## Compatibility 

`ALTER DOMAIN` conforms to the SQL standard, except for the `OWNER` and `SET SCHEMA` variants, which are Greenplum Database extensions.

## See Also 

[CREATE DOMAIN](CREATE_DOMAIN.html), [DROP DOMAIN](DROP_DOMAIN.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

