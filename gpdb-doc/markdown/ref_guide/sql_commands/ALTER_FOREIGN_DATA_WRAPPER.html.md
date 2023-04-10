# ALTER FOREIGN DATA WRAPPER 

Changes the definition of a foreign-data wrapper.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER FOREIGN DATA WRAPPER <name>
    [ HANDLER <handler_function> | NO HANDLER ]
    [ VALIDATOR <validator_function> | NO VALIDATOR ]
    [ OPTIONS ( [ ADD | SET | DROP ] <option> ['<value>'] [, ... ] ) ]

ALTER FOREIGN DATA WRAPPER <name> OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }
ALTER FOREIGN DATA WRAPPER <name> RENAME TO <new_name>
```

## <a id="section3"></a>Description 

`ALTER FOREIGN DATA WRAPPER` changes the definition of a foreign-data wrapper. The first form of the command changes the support functions or generic options of the foreign-data wrapper. Greenplum Database requires at least one clause. The second and third forms of the command change the owner or name of the foreign-data wrapper.

Only superusers can alter foreign-data wrappers. Additionally, only superusers can own foreign-data wrappers

## <a id="section4"></a>Parameters 

name
:   The name of an existing foreign-data wrapper.

HANDLER handler\_function
:   Specifies a new handler function for the foreign-data wrapper.

NO HANDLER
:   Specifies that the foreign-data wrapper should no longer have a handler function.

    > **Note** You cannot access a foreign table that uses a foreign-data wrapper with no handler.

VALIDATOR validator\_function
:   Specifies a new validator function for the foreign-data wrapper.

:   Note that it is possible that pre-existing options of the foreign-data wrapper, or of dependent servers, user mappings, or foreign tables, may become invalid when you change the validator function. Greenplum Database does not check for this. You must make sure that these options are correct before using the modified foreign-data wrapper. However, Greenplum Database will check any options specified in this `ALTER FOREIGN DATA WRAPPER` command using the new validator.

NO VALIDATOR
:   Specifies that the foreign-data wrapper should no longer have a validator function.

OPTIONS \( \[ ADD \| SET \| DROP \] option \['value'\] \[, ... \] \)
:   Change the foreign-data wrapper's options. `ADD`, `SET`, and `DROP` specify the action to perform. If no operation is explicitly specified, the default operation is `ADD`. Option names must be unique. Greenplum Database validates names and values using the foreign-data wrapper's validator function, if any.

OWNER TO new\_owner
:   Specifies the user name of the new owner of the foreign-data wrapper. Only superusers can own foreign-data wrappers.

RENAME TO new\_name
:   Specifies the new name of the foreign-data wrapper.

## <a id="section6"></a>Examples 

Change the definition of a foreign-data wrapper named `dbi` by adding a new option named `foo`, and removing the option named `bar`:

```
ALTER FOREIGN DATA WRAPPER dbi OPTIONS (ADD foo '1', DROP 'bar');
```

Change the validator function for a foreign-data wrapper named `dbi` to `bob.myvalidator`:

```
ALTER FOREIGN DATA WRAPPER dbi VALIDATOR bob.myvalidator;
```

## <a id="section7"></a>Compatibility 

`ALTER FOREIGN DATA WRAPPER` conforms to ISO/IEC 9075-9 \(SQL/MED\), with the exception that the `HANDLER`, `VALIDATOR`, `OWNER TO`, and `RENAME TO` clauses are Greenplum Database extensions.

## <a id="section8"></a>See Also 

[CREATE FOREIGN DATA WRAPPER](CREATE_FOREIGN_DATA_WRAPPER.html), [DROP FOREIGN DATA WRAPPER](DROP_FOREIGN_DATA_WRAPPER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

