# pg_largeobject 

> **Note** Greenplum Database does not support the PostgreSQL [large object facility](https://www.postgresql.org/docs/12/largeobjects.html) for streaming user data that is stored in large-object structures.

The `pg_largeobject` system catalog table holds the data making up 'large objects'. A large object is identified by an OID assigned when it is created. Each large object is broken into segments or 'pages' small enough to be conveniently stored as rows in `pg_largeobject`. The amount of data per page is defined to be `LOBLKSIZE` \(which is currently `BLCKSZ`/4, or typically 8K\).

Each row of `pg_largeobject` holds data for one page of a large object, beginning at byte offset \(*pageno*`* LOBLKSIZE`\) within the object. The implementation allows sparse storage: pages may be missing, and may be shorter than `LOBLKSIZE` bytes even if they are not the last page of the object. Missing regions within a large object read as zeroes.

|column|type|references|description|
|------|----|----------|-----------|
|`loid`|oid| |Identifier of the large object that includes this page.|
|`pageno`|int4| |Page number of this page within its large object \(counting from zero\).|
|`data`|bytea| |Actual data stored in the large object. This will never be more than `LOBLKSIZE` bytes and may be less.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

