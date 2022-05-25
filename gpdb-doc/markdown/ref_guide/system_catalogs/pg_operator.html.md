# pg_operator 

The `pg_operator` system catalog table stores information about operators, both built-in and those defined by `CREATE OPERATOR`. Unused column contain zeroes. For example, `oprleft` is zero for a prefix operator.

|column|type|references|description|
|------|----|----------|-----------|
|`oid`|oid| |Row identifier \(hidden attribute, must be explicityly selected\)|
|`oprname`|name| |Name of the operator|
|`oprnamespace`|oid|pg\_namespace.oid|The OID of the namespace that contains this operator|
|`oprowner`|oid|pg\_authid.oid|Owner of the operator|
|`oprkind`|char| |`b` = infix \(both\), `l` = prefix \(left\), `r` = postfix \(right\)|
|`oprcanmerge`|boolean| |This operator supports merge joins|
|`oprcanhash`|boolean| |This operator supports hash joins|
|`oprleft`|oid|pg\_type.oid|Type of the left operand|
|`oprright`|oid|pg\_type.oid|Type of the right operand|
|`oprresult`|oid|pg\_type.oid|Type of the result|
|`oprcom`|oid|pg\_operator.oid|Commutator of this operator, if any|
|`oprnegate`|oid|pg\_operator.oid|Negator of this operator, if any|
|`oprcode`|regproc|pg\_proc.oid|Function that implements this operator|
|`oprrest`|regproc|pg\_proc.oid|Restriction selectivity estimation function for this operator|
|`oprjoin`|regproc|pg\_proc.oid|Join selectivity estimation function for this operator|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

