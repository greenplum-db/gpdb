# DROP POLICY 

Removes a row-level security policy from a table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP POLICY [ IF EXISTS ] <name> ON <table_name> [ CASCADE | RESTRICT ]
```

## <a id="section3"></a>Description 

`DROP POLICY` removes the specified policy from the table. Note that if the last policy is removed for a table and the table still has row-level security enabled via `ALTER TABLE`, then the default-deny policy will be used. `ALTER TABLE ... DISABLE ROW LEVEL SECURITY` can be used to disable row-level security for a table, whether policies for the table exist or not.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the policy does not exist. A notice is issued in this case.

name
:   The name of the policy to drop.

table\_name
:   The name \(optionally schema-qualified\) of the table that the policy is on.

CASCADE
RESTRICT
:   These key words have no effect, since there are no dependencies on policies.

## <a id="section5"></a>Examples

To drop the policy called `p1` on the table named `my_table`:

```
DROP POLICY p1 ON my_table;
```

## <a id="section6"></a>Compatibility 

`DROP POLICY` is a Greenplum Database extension to the SQL standard.

## <a id="section7"></a>See Also 

[CREATE POLICY](CREATE_POLICY.html), [ALTER POLICY](ALTER_POLICY.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

