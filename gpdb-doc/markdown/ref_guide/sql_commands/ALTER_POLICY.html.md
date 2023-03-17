# ALTER POLICY 

Changes the definition of a row-level security policy.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER POLICY <name> ON <table_name> RENAME TO <new_name>

ALTER POLICY <name ON table_name>
    [ TO { <role_name> | PUBLIC | CURRENT_USER | SESSION_USER } [, ...] ]
    [ USING ( <using_expression> ) ]
    [ WITH CHECK ( <check_expression> ) ]
```

## <a id="section3"></a>Description 

`ALTER POLICY` changes the definition of an existing row-level security policy. Note that `ALTER POLICY` only allows the set of roles to which the policy applies and the `USING` and `WITH CHECK` expressions to be modified. To change other properties of a policy, such as the command to which it applies or whether it is permissive or restrictive, the policy must be dropped and recreated.

To use `ALTER POLICY`, you must own the table to which the policy applies.

In the second form of `ALTER POLICY`, the role list, using\_expression, and check\_expression are replaced independently if specified. When one of those clauses is omitted, the corresponding part of the policy is unchanged.

## <a id="section4"></a>Parameters 

name
:   The name of an existing policy to alter.

table\_name
:   The name \(optionally schema-qualified\) of the table that the policy is on.

new\_name
:   The new name for the policy.

role\_name
:   The role\(s\) to which the policy applies. Multiple roles can be specified at one time. To apply the policy to all roles, use `PUBLIC`.

using\_expression
:   The `USING` expression for the policy. See [CREATE POLICY](CREATE_POLICY.html) for details.

check\_expression
:   The `WITH CHECK` expression for the policy. See [CREATE POLICY](CREATE_POLICY.html) for details.

## <a id="section6"></a>Compatibility 

`ALTER POLICY` is a Greenplum Database extension to the SQL standard.

## <a id="section7"></a>See Also 

[CREATE POLICY](CREATE_POLICY.html), [DROP POLICY](DROP_POLICY.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

