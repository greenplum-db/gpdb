---
title: ALTER TYPE 
---

Changes the definition of a data type.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}

ALTER TYPE <name> <action> [, ... ]
ALTER TYPE <name> OWNER TO <new_owner>
ALTER TYPE <name> RENAME ATTRIBUTE <attribute_name> TO <new_attribute_name> [ CASCADE | RESTRICT ]
ALTER TYPE <name> RENAME TO <new_name>
ALTER TYPE <name> SET SCHEMA <new_schema>
ALTER TYPE <name> ADD VALUE [ IF NOT EXISTS ] <new_enum_value> [ { BEFORE | AFTER } <existing_enum_value> ]
ALTER TYPE <name> SET DEFAULT ENCODING ( <storage_directive> )

where <action> is one of:
  
  ADD ATTRIBUTE <attribute_name> <data_type> [ COLLATE <collation> ] [ CASCADE | RESTRICT ]
  DROP ATTRIBUTE [ IF EXISTS ] <attribute_name> [ CASCADE | RESTRICT ]
  ALTER ATTRIBUTE <attribute_name> [ SET DATA ] TYPE <data_type> [ COLLATE <collation> ] [ CASCADE | RESTRICT ]

```

where storage\_directive is:

```
   COMPRESSTYPE={ZLIB | ZSTD | QUICKLZ | RLE_TYPE | NONE}
   COMPRESSLEVEL={0-19}
   BLOCKSIZE={8192-2097152}
```

>**Note**
>Support for the QuickLZ compression algorithm is deprecated and will be removed in the next major release of VMware Greenplum.


## <a id="section3"></a>Description 

`ALTER TYPE` changes the definition of an existing type. There are several subforms:

-   **`ADD ATTRIBUTE`** — Adds a new attribute to a composite type, using the same syntax as `CREATE TYPE`.
-   **`DROP ATTRIBUTE [ IF EXISTS ]`** — Drops an attribute from a composite type. If `IF EXISTS` is specified and the attribute does not exist, no error is thrown. In this case a notice is issued instead.
-   **`SET DATA TYPE`** — Changes the type of an attribute of a composite type.
-   **`OWNER`** — Changes the owner of the type.
-   **`RENAME`** — Changes the name of the type or the name of an individual attribute of a composite type.
-   **`SET SCHEMA`** — Moves the type into another schema.
-   **`ADD VALUE [ IF NOT EXISTS ] [ BEFORE | AFTER ]`** — Adds a new value to an enum type. The new value's place in the enum's ordering can be specified as being `BEFORE` or `AFTER` one of the existing values. Otherwise, the new item is added at the end of the list of values.

    If `IF NOT EXISTS` is specified, it is not an error if the type already contains the new value; a notice is issued but no other action is taken. Otherwise, an error will occur if the new value is already present.

-   **`CASCADE`** — Automatically propagate the operation to typed tables of the type being altered, and their descendants.
-   **`RESTRICT`** — Refuse the operation if the type being altered is the type of a typed table. This is the default.

The `ADD ATTRIBUTE`, `DROP ATTRIBUTE`, and `ALTER ATTRIBUTE` actions can be combined into a list of multiple alterations to apply in parallel. For example, it is possible to add several attributes and/or alter the type of several attributes in a single command.

You can change the name, the owner, and the schema of a type. You can also add or update storage options for a scalar type.

> **Note** Greenplum Database does not support adding storage options for row or composite types.

You must own the type to use `ALTER TYPE`. To change the schema of a type, you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the type's schema. \(These restrictions enforce that altering the owner does not do anything that could be done by dropping and recreating the type. However, a superuser can alter ownership of any type.\) To add an attribute or alter an attribute type, you must also have `USAGE` privilege on the data type.

`ALTER TYPE ... ADD VALUE` \(the form that adds a new value to an enum type\) cannot be run inside a transaction block.

Comparisons involving an added enum value will sometimes be slower than comparisons involving only original members of the enum type. This will usually only occur if `BEFORE` or `AFTER` is used to set the new value's sort position somewhere other than at the end of the list. However, sometimes it will happen even though the new value is added at the end \(this occurs if the OID counter "wrapped around" since the original creation of the enum type\). The slowdown is usually insignificant; but if it matters, optimal performance can be regained by dropping and recreating the enum type, or by dumping and reloading the database.

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing type to alter.

new\_name
:   The new name for the type.

new\_owner
:   The user name of the new owner of the type.

new\_schema
:   The new schema for the type.

attribute\_name
:   The name of the attribute to add, alter, or drop.

new\_attribute\_name
:   The new name of the attribute to be renamed.

data\_type
:   The data type of the attribute to add, or the new type of the attribute to alter.

new\_enum\_value
:   The new value to be added to an enum type's list of values. Like all enum literals, it needs to be quoted.

existing\_enum\_value
:   The existing enum value that the new value should be added immediately before or after in the enum type's sort ordering. Like all enum literals, it needs to be quoted.

storage\_directive
:   Identifies default storage options for the type when specified in a table column definition. Options include `COMPRESSTYPE`, `COMPRESSLEVEL`, and `BLOCKSIZE`.

:   **COMPRESSTYPE** — Set to `ZLIB` \(the default\), `ZSTD`, `RLE_TYPE`, or `QUICKLZ` to specify the type of compression used.

    > **Note** QuickLZ compression is available only in the commercial release of VMware Greenplum. Support for the QuickLZ compression algorithm is deprecated and will be removed in the next major release of VMware Greenplum.

:   **COMPRESSLEVEL** — For Zstd compression, set to an integer value from 1 \(fastest compression\) to 19 \(highest compression ratio\). For zlib compression, the valid range is from 1 to 9. The QuickLZ compression level can only be set to 1. For `RLE_TYPE`, the compression level can be set to an integer value from 1 \(fastest compression\) to 4 \(highest compression ratio\). The default compression level is 1.

:   **BLOCKSIZE** — Set to the size, in bytes, for each block in the column. The `BLOCKSIZE` must be between 8192 and 2097152 bytes, and be a multiple of 8192. The default block size is 32768.

    > **Note** storage\_directives defined at the table- or column-level override the default storage options defined for a type.

## <a id="section5"></a>Examples 

To rename the data type named `electronic_mail`:

```
ALTER TYPE electronic_mail RENAME TO email;
```

To change the owner of the user-defined type `email` to `joe`:

```
ALTER TYPE email OWNER TO joe;
```

To change the schema of the user-defined type `email` to `customers`:

```
ALTER TYPE email SET SCHEMA customers;
```

To set or alter the compression type and compression level of the user-defined type named `int33`:

```
ALTER TYPE int33 SET DEFAULT ENCODING (compresstype=zlib, compresslevel=7);
```

To add a new attribute to a type:

```
ALTER TYPE compfoo ADD ATTRIBUTE f3 int;
```

To add a new value to an enum type in a particular sort position:

```
ALTER TYPE colors ADD VALUE 'orange' AFTER 'red';
```

## <a id="section6"></a>Compatibility 

The variants to add and drop attributes are part of the SQL standard; the other variants are Greenplum Database extensions.

## <a id="section7"></a>See Also 

[CREATE TYPE](CREATE_TYPE.html), [DROP TYPE](DROP_TYPE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

