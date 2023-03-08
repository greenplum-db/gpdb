# Additional Supplied Programs 

Additional programs available in the Greenplum Database installation.

The following PostgreSQL `contrib` server utility programs are installed:

-   [pg\_upgrade](https://www.postgresql.org/docs/12/pgupgrade.html) - Server program to upgrade a Postgres Database server instance.

    > **Note** `pg_upgrade` is not intended for direct use with Greenplum 6, but will be used by Greenplum upgrade utilities in a future release.

-   `pg_upgrade_support` - supporting library for `pg_upgrade`.
-   [pg\_xlogdump](https://www.postgresql.org/docs/12/pgxlogdump.html) - Server utility program to display a human-readable rendering of the write-ahead log of a Greenplum Database cluster.

**Parent topic:** [Greenplum Database Utility Guide](utility_guide.html)

