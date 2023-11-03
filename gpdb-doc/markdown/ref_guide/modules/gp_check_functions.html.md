# gp_check_functions

The `gp_check_functions` module implements views that identify missing and orphaned relation files. The module also exposes a user-defined function that you can use to move orphaned files.

The `gp_check_functions` module is a Greenplum Database extension.

## <a id="topic_reg"></a>Installing and Registering the Module

The `gp_check_functions` module is installed when you install Greenplum Database. Before you can use the views defined in the module, you must register the `gp_check_functions` extension in each database in which you want to use the views:
o

```
CREATE EXTENSION gp_check_functions;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.


## <a id="missingfiles"></a>Checking for Missing and Orphaned Data Files

Greenplum Database considers a relation data file that is present in the catalog, but not on disk, to be missing. Conversely, when Greenplum encounters an unexpected data file on disk that is not referenced in any relation, it considers that file to be orphaned.

Greenplum Database provides the following views to help identify if missing or orphaned files exist in the current database:

- [gp_check_orphaned_files](#orphaned)
- [gp_check_missing_files](#missing)
- [gp_check_missing_files_ext](#missing_ext)

Consider it a best practice to check for these conditions prior to expanding the cluster or before offline maintenance.

By default, the views in this module are available to `PUBLIC`.

### <a id="orphaned"></a>gp_check_orphaned_files

The `gp_check_orphaned_files` view scans the default and user-defined tablespaces for orphaned data files. Greenplum Database considers normal data files, files with an underscore (`_`) in the name, and extended numbered files (files that contain a `.<N>` in the name) in this check. `gp_check_orphaned_files` gathers results from the Greenplum Database master and all segments.

|Column|Description|
|------|-----------|
| gp_segment_id | The Greenplum Database segment identifier. |
| tablespace | The identifier of the tablespace in which the orphaned file resides. |
| filename | The file name of the orphaned data file. |
| filepath | The file system path of the orphaned data file, relative to the data directory of the master or segment. |

> **Caution** Use this view as one of many data points to identify orphaned data files. Do not delete files based solely on results from querying this view.


### <a id="missing"></a>gp_check_missing_files

The `gp_check_missing_files` view scans heap and append-optimized, column-oriented tables for missing data files. Greenplum considers only normal data files (files that do not contain a `.` or an `_` in the name) in this check. `gp_check_missing_files` gathers results from the Greenplum Database master and all segments.

|Column|Description|
|------|-----------|
| gp_segment_id | The Greenplum Database segment identifier. |
| tablespace | The identifier of the tablespace in which the table resides. |
| relname | The name of the table that has a missing data file(s). |
| filename | The file name of the missing data file. |


### <a id="missing_ext"></a>gp_check_missing_files_ext

The `gp_check_missing_files_ext` view scans only append-optimized, column-oriented tables for missing extended data files. Greenplum Database considers both normal data files and extended numbered files (files that contain a `.<N>` in the name) in this check. Files that contain an `_` in the name, and `.fsm`, `.vm`, and other supporting files, are not considered. `gp_check_missing_files_ext` gathers results from the Greenplum Database segments only.

|Column|Description|
|------|-----------|
| gp_segment_id | The Greenplum Database segment identifier. |
| tablespace | The identifier of the tablespace in which the table resides. |
| relname | The name of the table that has a missing extended data file(s). |
| filename | The file name of the missing extended data file. |


## <a id="moveorphanfiles"></a>Moving Orphaned Data Files

The `gp_move_orphaned_files()` user-defined function (UDF) moves orphaned files found by the [gp_check_orphaned_files](#orphaned) view into a file system location that you specify.

The function signature is: `gp_move_orphaned_files( <target_directory> TEXT )`.

`<target_directory>` must exist on all segment hosts before you move the files, and the specified directory must be accessible by the `gpadmin` user. If you specify a relative path for `<target_directory>`, it is considered relative to the data directory of the master or segment.

Greenplum Database renames each moved data file to one that reflects the original location of the file in the data directory. The file name format differs depending on the tablespace in which the orphaned file resides:

| Tablespace | Renamed File Format|
|------|-----------|
| default | `seg<num>_base_<database-oid>_<relfilenode>` |
| global | `seg<num>_global_<relfilenode>` |
| user-defined | `seg<num>_pg_tblspc_<tablespace-oid>_<gpdb-version>_<database-oid>_<relfilenode>` |

For example, if a file named `12345` in the default tablespace is orphaned on primary segment 2,

```
SELECT * FROM gp_move_orphaned_files('/home/gpadmin/orphaned');
```

moves and renames the file as follows:

| Original Location | New Location and File Name |
|------|-----------|
| `<data_directory>/base/13700/12345` | `/home/gpadmin/orphaned/seg2_base_13700_12345` |

`gp_move_orphaned_files()` returns both the original and the new file system locations for each file that it moves, and also provides an indication of the success or failure of the move operation.

Once you move the orphaned files, you may choose to remove them or to back them up.

## <a id="examples"></a>Examples

Check for missing and orphaned non-extended files:

``` sql
SELECT * FROM gp_check_missing_files;
SELECT * FROM gp_check_orphaned_files;
```

Check for missing extended data files for append-optimized, column-oriented tables:

``` sql
SELECT * FROM gp_check_missing_files_ext;
```

Move orphaned files to the `/home/gpadmin/orphaned` directory:

``` sql
SELECT * FROM gp_move_orphaned_files('/home/gpadmin/orphaned');
```

