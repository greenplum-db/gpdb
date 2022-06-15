# pg\_opclass 

The `pg_opclass` system catalog table defines index access method operator classes. Each operator class defines semantics for index columns of a particular data type and a particular index access method. Note that there can be multiple operator classes for a given data type/access method combination, thus supporting multiple behaviors. The majority of the information defining an operator class is actually not in its `pg_opclass` row, but in the associated rows in [pg\_amop](pg_amop.html) and [pg\_amproc](pg_amproc.html). Those rows are considered to be part of the operator class definition — this is not unlike the way that a relation is defined by a single [pg\_class](pg_class.html) row plus associated rows in [pg\_attribute](pg_attribute.html) and other tables.

|column|type|references|description|
|------|----|----------|-----------|
|`opcamid`|oid|pg\_am.oid|Index access method operator class is for.|
|`opcname`|name| |Name of this operator class|
|`opcnamespace`|oid|pg\_namespace.oid|Namespace of this operator class|
|`opcowner`|oid|pg\_authid.oid|Owner of the operator class|
|`opcintype`|oid|pg\_type.oid|Data type that the operator class indexes.|
|`opcdefault`|boolean| |True if this operator class is the default for the data type opcintype.|
|`opckeytype`|oid|pg\_type.oid|Type of data stored in index, or zero if same as opcintype.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

