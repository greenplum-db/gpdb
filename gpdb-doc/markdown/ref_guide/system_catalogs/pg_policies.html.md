# pg_policies

The `pg_policies` view provides access to useful information about each row-level security policy in the database.

|Column Name|Type|References|Description|
|------|----|----------|-----------|
|`schemaname`|name|pg\_namespace.nspname |The name of schema that contas the table the policy is on|
|`tablename`|name|pg\_class.relname|The name of the table the policy is on|
|`policyname`|name|pg\_policy.polname |The name of policy|
|`polpermissive`|text| |Is the policy permissive or restrictive?|
|`roles`|name[]| |The roles to which this policy applies|
|`cmd`|text| |The command type to which the policy is applied|
|`qual`|text| |The expression added to the security barrier qualifications for queries to which this policy applies|
|`with_check`|text| |The expression added to the `WITH CHECK` qualifications for queries that attempt to add rows to this table|

> **Note**  Policies stored in `pg_policy` are applied only when `pg_class.relrowsecurity` is set for their table.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

