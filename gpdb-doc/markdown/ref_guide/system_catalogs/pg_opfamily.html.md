# pg_opfamily 

The catalog `pg_opfamily` defines operator families. Each operator family is a collection of operators and associated support routines that implement the semantics specified for a particular index access method. Furthermore, the operators in a family are all compatible in a way that is specified by the access method. The operator family concept allows cross-data-type operators to be used with indexes and to be reasoned about using knowledge of access method semantics.

The majority of the information defining an operator family is not in its `pg_opfamily` row, but in the associated rows in [pg\_amop](pg_amop.html), [pg\_amproc](pg_amproc.html), and [pg\_opclass](pg_opclass.html).

|Name|Type|References|Description|
|----|----|----------|-----------|
|`oid`|oid| |Row identifier \(hidden attribute; must be explicitly selected\)|
|`opfmethod`|oid|pg\_am.oid|Index access method operator for this family|
|`opfname`|name||Name of this operator family|
|`opfnamespace`|oid|pg\_namespace.oid|Namespace of this operator family|
|`opfowner`|oid|pg\_authid.oid|Owner of the operator family|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

