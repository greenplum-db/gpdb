# ALTER SERVER 

Changes the definition of a foreign server.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER SERVER <name> [ VERSION '<new_version>' ]
    [ OPTIONS ( [ ADD | SET | DROP ] <option> ['<value>'] [, ... ] ) ]

ALTER SERVER <name> OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }
                
ALTER SERVER <name> RENAME TO <new_name>
```

## <a id="section3"></a>Description 

`ALTER SERVER` changes the definition of a foreign server. The first form of the command changes the version string or the generic options of the server. Greenplum Database requires at least one clause. The second and third forms of the command change the owner or the name of the server.

To alter the server, you must be the owner of the server. To alter the owner you must:

-   Own the server.
-   Be a direct or indirect member of the new owning role.
-   Have `USAGE` privilege on the server's foreign-data wrapper.

Superusers automatically satisfy all of these criteria.

## <a id="section4"></a>Parameters 

name
:   The name of an existing server.

new\_version
:   The new server version.

OPTIONS \( \[ ADD \| SET \| DROP \] option \['value'\] \[, ... \] \)
:   Change the server's options. `ADD`, `SET`, and `DROP` specify the action to perform. If no operation is explicitly specified, the default operation is `ADD`. Option names must be unique. Greenplum Database validates names and values using the server's foreign-data wrapper library.

new\_owner
:   Specifies the new owner of the foreign server.

new\_name
:   Specifies the new name of the foreign server.

## <a id="section6"></a>Examples 

Change the definition of a server named `foo` by adding connection options:

```
ALTER SERVER foo OPTIONS (host 'foo', dbname 'foodb');
```

Change the option named `host` for a server named `foo`, and set the server version:

```
ALTER SERVER foo VERSION '9.1' OPTIONS (SET host 'baz');
```

## <a id="section7"></a>Compatibility 

`ALTER SERVER` conforms to ISO/IEC 9075-9 \(SQL/MED\). The `OWNER TO` and `RENAME` forms are Greenplum Database extensions.

## <a id="section8"></a>See Also 

[CREATE SERVER](CREATE_SERVER.html), [DROP SERVER](DROP_SERVER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

