# pg_database 

The `pg_database` system catalog table stores information about the available databases. Databases are created with the `CREATE DATABASE` SQL command. Unlike most system catalogs, `pg_database` is shared across all databases in the system. There is only one copy of `pg_database` per system, not one per database.

|column|type|references|description|
|------|----|----------|-----------|
|`datname`|name| |Database name.|
|`datdba`|oid|pg\_authid.oid|Owner of the database, usually the user who created it.|
|`encoding`|int4| |Character encoding for this database. `pg_encoding_to_char()` can translate this number to the encoding name.|
|`datcollate`|name| |`LC_COLLATE` for this database.|
|`datctype`|name| |`LC_CTYPE` for this database.|
|`datistemplate`|boolean| |If true then this database can be used in the `TEMPLATE` clause of `CREATE DATABASE` to create a new database as a clone of this one.|
|`datallowconn`|boolean| |If false then no one can connect to this database. This is used to protect the `template0` database from being altered.|
|`datconnlimit`|int4| |Sets the maximum number of concurrent connections that can be made to this database. `-1` means no limit.|
|`datlastsysoid`|oid| |Last system OID in the database.|
|`datfrozenxid`|xid| |All transaction IDs \(XIDs\) before this one have been replaced with a permanent \(frozen\) transaction ID in this database. This is used to track whether the database needs to be vacuumed in order to prevent transaction ID wraparound or to allow pg\_xact to be shrunk. It is the minimum of the per-table *pg\_class.relfrozenxid* values.|
|`datminmxid`|xid| |A *Multixact ID* is used to support row locking by multiple transactions. All multixact IDs before this one have been replaced with a transaction ID in this database. This is used to track whether the database needs to be vacuumed in order to prevent multixact ID wraparound or to allow `pg_multixact` to be shrunk. It is the minimum of the per-table *pg\_class.relminmxid* values.|
|`dattablespace`|oid|pg\_tablespace.oid|The default tablespace for the database. Within this database, all tables for which *pg\_class.reltablespace* is zero will be stored in this tablespace. All non-shared system catalogs will also be there.|
|`datacl`|aclitem\[\]| |Database access privileges as given by `GRANT` and `REVOKE`.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

