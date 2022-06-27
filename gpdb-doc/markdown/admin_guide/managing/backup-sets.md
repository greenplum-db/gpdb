# Backing Up a Set of Tables 

You can create a backup that includes a subset of the schema or tables in a database by using the following `gpcrondump` options:

-   `-t schema.tablename` – specify a table to include in the backup. You can use the `-t` option multiple times.
-   `--table-file=filename` – specify a file containing a list of tables to include in the backup.
-   `-T schema.tablename` – specify a table to exclude from the backup. You can use the `-T` option multiple times.
-   `--exclude-table-file=filename` – specify a file containing a list of tables to exclude from the backup.
-   `-s schema\_name` – include all tables qualified by a specified schema name in the backup. You can use the `-s` option multiple times.
-   `--schema-file=filename` – specify a file containing a list of schemas to include in the backup.
-   `-S schema\_name` – exclude tables qualified by a specified schema name from the backup. You can use the `-S` option multiple times.
-   `--exclude-schema-file=filename` – specify a file containing schema names to exclude from the backup.

Only a set of tables or set of schemas can be specified. For example, the `-s` option cannot be specified with the `-t` option.

Refer to [Incremental Backup with Sets](backup-incremental.md#section_djm_lbb_tt) for additional information about using these `gpcrondump` options with incremental backups.

**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

