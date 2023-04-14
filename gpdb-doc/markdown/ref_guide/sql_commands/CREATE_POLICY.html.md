# CREATE POLICY 

Defines a new row-level security policy for a table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE POLICY <name> ON <table_name>
    [ AS { PERMISSIVE | RESTRICTIVE } ]
    [ FOR { ALL | SELECT | INSERT | UPDATE | DELETE } ]
    [ TO { <role_name> | PUBLIC | CURRENT_USER | SESSION_USER } [, ...] ]
    [ USING ( <using_expression> ) ]
    [ WITH CHECK ( <check_expression> ) ]
```

## <a id="section3"></a>Description 

The `CREATE POLICY` command defines a new row-level security policy for a table. Note that row-level security must be enabled on the table \(using `ALTER TABLE ... ENABLE ROW LEVEL SECURITY`\) in order for Greenplum Database to apply created policies.

A policy grants the permission to select, insert, update, or delete rows that match the relevant policy expression. Existing table rows are checked against the expression specified in `USING`, while new rows that would be created via `INSERT` or `UPDATE` are checked against the expression specified in `WITH CHECK`. When a `USING` expression returns true for a given row, then that row is visible to the user, while if false or null is returned then the row is not visible. When a `WITH CHECK` expression returns true for a row, then that row is inserted or updated, while if false or null is returned then an error occurs.

For `INSERT` and `UPDATE` statements, `WITH CHECK` expressions are enforced before any actual data modifications are made, and before any other constraints.

Policy names are per-table. Therefore, one policy name can be used for many different tables and have a definition for each table which is appropriate to that table.

Policies can be applied for specific commands or for specific roles. The default for newly-created policies is that they apply for all commands and roles, unless otherwise specified. Multiple policies may apply to a single command; see below for more details. The table in [Per-Command Policies](#section4a) summarizes how the different types of policies apply to specific commands.

For policies that can have both `USING` and `WITH CHECK` expressions \(`ALL` and `UPDATE`\), if no `WITH CHECK` expression is defined, then the `USING` expression will be used both to determine which rows are visible \(normal `USING` case\) and which new rows will be allowed to be added \(`WITH CHECK` case\).

If row-level security is enabled for a table, but no applicable policies exist, a "default deny" policy is assumed, so that no rows will be visible or updatable.

## <a id="section4"></a>Parameters 

name
:   The name of the policy to be created. This must be distinct from the name of any other policy defined for the table.

table\_name
:   The name \(optionally schema-qualified\) of the table to which the policy applies.

PERMISSIVE
:   Specify that the policy is to be created as a permissive policy. All permissive policies which are applicable to a given query will be combined together using the Boolean `OR` operator. By creating permissive policies, administrators can add to the set of records which can be accessed. Policies are permissive by default.

RESTRICTIVE
:   Specify that the policy is to be created as a restrictive policy. All restrictive policies which are applicable to a given query will be combined together using the Boolean `AND` operator. By creating restrictive policies, administrators can reduce the set of records which can be accessed as all restrictive policies must be passed for each record.

:   You must define at least one permissive policy to grant access to records before restrictive policies can be usefully used to reduce that access. If only restrictive policies exist, then no records will be accessible. When a mix of permissive and restrictive policies are present, a record is only accessible if at least one of the permissive policies passes, in addition to all the restrictive policies.

command
:   The command to which the policy applies. Valid options are `ALL`, `SELECT`, `INSERT`, `UPDATE`, and `DELETE`. `ALL` is the default. See below for specifics regarding how these are applied.

role\_name
:   The role\(s\) to which the policy is to be applied. The default is `PUBLIC`, which will apply the policy to all roles.

using\_expression
:   Any SQL conditional expression \(returning `boolean`\). The conditional expression cannot contain any aggregate or window functions. This expression will be added to queries that refer to the table if row-level security is enabled. Rows for which the expression returns true will be visible. Any rows for which the expression returns false or null will not be visible to the user \(in a `SELECT`\), and will not be available for modification \(in an `UPDATE` or `DELETE`\). Such rows are silently suppressed; no error is reported.

check\_expression
:   Any SQL conditional expression \(returning `boolean`\). The conditional expression cannot contain any aggregate or window functions. This expression will be used in `INSERT` and `UPDATE` queries against the table if row-level security is enabled. Only rows for which the expression evaluates to true will be allowed. An error will be thrown if the expression evaluates to false or null for any of the records inserted or any of the records that result from the update. Note that the check\_expression is evaluated against the proposed new contents of the row, not the original contents.

## <a id="section4a"></a>Per-Command Policies

ALL
:   Using `ALL` for a policy means that it will apply to all commands, regardless of the type of command. If an `ALL` policy exists and more specific policies exist, then both the `ALL` policy and the more specific policy \(or policies\) will be applied. Additionally, `ALL` policies will be applied to both the selection side of a query and the modification side, using the `USING` expression for both cases if only a `USING` expression has been defined.

:   As an example, if an `UPDATE` is issued, then the `ALL` policy will be applicable both to what the `UPDATE` will be able to select as rows to be updated \(applying the `USING` expression\), and to the resulting updated rows, to check if they are permitted to be added to the table \(applying the `WITH CHECK` expression, if defined, and the `USING` expression otherwise\). If an `INSERT` or `UPDATE` command attempts to add rows to the table that do not pass the `ALL` policy's `WITH CHECK` expression, the entire command will be aborted.

SELECT
:   Using `SELECT` for a policy means that it will apply to `SELECT` queries and whenever `SELECT` permissions are required on the relation that the policy is defined for. The result is that only those records from the relation that pass the `SELECT` policy will be returned during a `SELECT` query, and that queries that require `SELECT` permissions, such as `UPDATE`, will also only see those records that are allowed by the `SELECT` policy. A `SELECT` policy cannot have a `WITH CHECK` expression, as it only applies in cases where records are being retrieved from the relation.

INSERT
:   Using `INSERT` for a policy means that it will apply to `INSERT` commands. Rows being inserted that do not pass this policy will result in a policy violation error, and the entire `INSERT` command will be aborted. An `INSERT` policy cannot have a `USING` expression, as it only applies in cases where records are being added to the relation.

:   Note that `INSERT` with `ON CONFLICT DO UPDATE` checks `INSERT` policies' `WITH CHECK` expressions only for rows appended to the relation by the `INSERT` path.

UPDATE
:   Using `UPDATE` for a policy means that it will apply to `UPDATE`, `SELECT FOR UPDATE`, and `SELECT FOR SHARE` commands, as well as auxiliary `ON CONFLICT DO UPDATE` clauses of `INSERT` commands. Since `UPDATE` involves pulling an existing record and replacing it with a new modified record, `UPDATE` policies accept both a `USING` expression and a `WITH CHECK` expression. The `USING` expression determines which records the `UPDATE` command will see to operate against, while the `WITH CHECK` expression defines which modified rows are allowed to be stored back into the relation.

:   Any rows whose updated values do not pass the `WITH CHECK` expression will cause an error, and the entire command will be aborted. If only a `USING` clause is specified, then that clause will be used for both `USING` and `WITH CHECK` cases.

:   Typically an `UPDATE` command also needs to read data from columns in the relation being updated \(e.g., in a `WHERE` clause or a `RETURNING` clause, or in an expression on the right hand side of the `SET` clause\). In this case, `SELECT` rights are also required on the relation being updated, and the appropriate `SELECT` or `ALL` policies will be applied in addition to the `UPDATE` policies. Thus the user must have access to the row\(s\) being updated through a `SELECT` or `ALL` policy in addition to being granted permission to update the row\(s\) via an `UPDATE` or `ALL` policy.

:   When an `INSERT` command has an auxiliary `ON CONFLICT DO UPDATE` clause, if the `UPDATE` path is taken, the row to be updated is first checked against the `USING` expressions of any `UPDATE` policies, and then the new updated row is checked against the `WITH CHECK` expressions. Note, however, that unlike a standalone `UPDATE` command, if the existing row does not pass the `USING` expressions, an error will be thrown \(the `UPDATE` path will never be silently avoided\).

DELETE
:   Using `DELETE` for a policy means that it will apply to `DELETE` commands. Only rows that pass this policy will be seen by a `DELETE` command. There can be rows that are visible through a `SELECT` that are not available for deletion, if they do not pass the `USING` expression for the `DELETE` policy.

:   In most cases a `DELETE` command also needs to read data from columns in the relation that it is deleting from \(e.g., in a `WHERE` clause or a `RETURNING` clause\). In this case, `SELECT` rights are also required on the relation, and the appropriate `SELECT` or `ALL` policies will be applied in addition to the `DELETE` policies. Thus the user must have access to the row\(s\) being deleted through a `SELECT` or `ALL` policy in addition to being granted permission to delete the row\(s\) via a `DELETE` or `ALL` policy.

:   A `DELETE` policy cannot have a `WITH CHECK` expression, as it only applies in cases where records are being deleted from the relation, so that there is no new row to check.


| Command |SELECT/ALL policy</br>USING expresson | INSERT/ALL policy</br>WITH CHECK expression | UPDATE/ALL policy</br> USING expression | UPDATE/ALL policy</br>WITH CHECK expression | DELETE/ALL policy</br> USING expression|
|--------------|-----------|
| SELECT | Existing row | -- | -- | -- | -- |
| SELECT FOR UPDATE/SHARE | Existing row | -- | Existing row | -- | -- |
| INSERT | -- | New row | -- | -- | -- |
| INSERT ... RETURNING | New row<sup>[a]</sup> | New row | -- | -- | -- |
| UPDATE  | Existing and new rows<sup>[a]</sup> | -- | Existing row | New row | -- |
| DELETE  | Existing row<sup>[a]</sup> | -- | --  | -- | Existing row |
| ON CONFLICT DO UPDATE  | Existing and new rows | -- | Existing row | New row | -- |

<sup>[a]</sup>If read access is required to the existing or new row \(for example, a `WHERE` or `RETURNING` clause that refers to columns from the relation\).


## <a id="section4b"></a>Application of Multiple Policies

When multiple policies of different command types apply to the same command \(for example, `SELECT` and `UPDATE` policies applied to an `UPDATE` command\), then the user must have both types of permissions \(for example, permission to select rows from the relation as well as permission to update them\). Thus the expressions for one type of policy are combined with the expressions for the other type of policy using the `AND` operator.

When multiple policies of the same command type apply to the same command, then there must be at least one `PERMISSIVE` policy granting access to the relation, and all of the `RESTRICTIVE` policies must pass. Thus all the `PERMISSIVE` policy expressions are combined using `OR`, all the `RESTRICTIVE` policy expressions are combined using `AND`, and the results are combined using `AND`. If there are no `PERMISSIVE` policies, then access is denied.

Note that, for the purposes of combining multiple policies, `ALL` policies are treated as having the same type as whichever other type of policy is being applied.

For example, in an `UPDATE` command requiring both `SELECT` and `UPDATE` permissions, if there are multiple applicable policies of each type, they will be combined as follows:

```
expression from RESTRICTIVE SELECT/ALL policy 1
AND
expression from RESTRICTIVE SELECT/ALL policy 2
AND
...
AND
(
  expression from PERMISSIVE SELECT/ALL policy 1
  OR
  expression from PERMISSIVE SELECT/ALL policy 2
  OR
  ...
)
AND
expression from RESTRICTIVE UPDATE/ALL policy 1
AND
expression from RESTRICTIVE UPDATE/ALL policy 2
AND
...
AND
(
  expression from PERMISSIVE UPDATE/ALL policy 1
  OR
  expression from PERMISSIVE UPDATE/ALL policy 2
  OR
  ...
)
```

## <a id="section5"></a>Notes

You must be the owner of a table to create or change policies for it.

While policies will be applied for explicit queries against tables in the database, they are not applied when the system is performing internal referential integrity checks or validating constraints. This means there are indirect ways to determine that a given value exists. An example of this is attempting to insert a duplicate value into a column that is a primary key or has a unique constraint. If the insert fails then the user can infer that the value already exists. \(This example assumes that the user is permitted by policy to insert records which they are not allowed to see.\) Another example is where a user is allowed to insert into a table which references another, otherwise hidden table. Existence can be determined by the user inserting values into the referencing table, where success would indicate that the value exists in the referenced table. These issues can be addressed by carefully crafting policies to prevent users from being able to insert, delete, or update records at all which might possibly indicate a value they are not otherwise able to see, or by using generated values \(e.g., surrogate keys\) instead of keys with external meanings.

Generally, the system will enforce filter conditions imposed using security policies prior to qualifications that appear in user queries, in order to prevent inadvertent exposure of the protected data to user-defined functions which might not be trustworthy. However, functions and operators marked by the system \(or the system administrator\) as `LEAKPROOF` may be evaluated before policy expressions, as they are assumed to be trustworthy.

Since policy expressions are added to the user's query directly, they will be run with the rights of the user running the overall query. Therefore, users who are using a given policy must be able to access any tables or functions referenced in the expression or they will simply receive a permission denied error when attempting to query the table that has row-level security enabled. This does not change how views work, however. As with normal queries and views, permission checks and policies for the tables which are referenced by a view will use the view owner's rights and any policies which apply to the view owner.

Refer to [About Configuring Row-Level Security Policies](../../admin_guide/row_security.html) for more information and practical examples.

## <a id="section6"></a>Compatibility 

`CREATE POLICY` is a Greenplum Database extension to the SQL standard.

## <a id="section7"></a>See Also 

[ALTER POLICY](ALTER_POLICY.html), [DROP POLICY](DROP_POLICY.html), [ALTER TABLE](ALTER_TABLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

