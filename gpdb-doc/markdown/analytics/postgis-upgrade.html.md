---
title: Upgrading PostGIS 2.1.5 or 2.5.4 
---

For Greenplum Database 6, you can upgrade from PostGIS 2.1.5 to 2.5.4, or from a PostGIS 2.5.4 package to a newer PostGIS 2.5.4 package.

-   [Upgrading from PostGIS 2.1.5 to the PostGIS 2.5.4 pivotal.3 Package](#topic_zgw_v5x_3mb)
-   [Upgrade a PostGIS 2.5.4 Package from pivotal.1 or pivotal.2 to pivotal.3](#topic_k4x_dp3_kmb)
-   [Checking the PostGIS Version](#topic_yzz_l3h_kmb)

> **Note** For Greenplum Database 6, you can upgrade from PostGIS 2.1.5 to 2.5.4, or from a PostGIS 2.5.4 package to a newer PostGIS 2.5.4 package using the `postgis_manager.sh` script described in the upgrade instructions.

Upgrading PostGIS using the `postgis_manager.sh` script does not require you to remove PostGIS support and re-enable it.

Removing PostGIS support from a database drops PostGIS database objects from the database without warning. Users accessing PostGIS objects might interfere with the dropping of PostGIS objects. See the Notes section in [Removing PostGIS Support](postGIS.html).

## <a id="topic_zgw_v5x_3mb"></a>Upgrading from PostGIS 2.1.5 to the PostGIS 2.5.4 pivotal.3 Package 

A PostGIS 2.5.4 `pivotal.3` package contains PostGIS 2.5.4. Also, the PostGIS 2.5.4 `pivotal.3` package supports using the `CREATE EXTENSION` command and the `DROP EXTENSION` command to enable and remove PostGIS support in a database. See [Notes](#topic_hm5_3zk_jmb).

After upgrading the Greenplum PostGIS package, you can remove the PostGIS 2.1.5 package \(`gppkg`\) from the Greenplum system. See [Removing the PostGIS 2.1.5 package](#topic_unj_v5n_kmb).

1.  Confirm you have a PostGIS 2.1.5 package such as `postgis-2.1.5+pivotal.1` installed in a Greenplum Database system. See [Checking the PostGIS Version](#topic_yzz_l3h_kmb).
2.  Install the PostGIS 2.5.4 package into the Greenplum Database system with the `gppkg` utility.

    ```
    gppkg -i postgis-2.5.4+pivotal.3.build.1-gp7-rhel8-x86_64.gppkg
    ```

    Run the `gppkg -q --all` command to verify the updated package version is installed in the Greenplum Database system.

3.  For all databases with PostGIS enabled, run the PostGIS 2.5.4 `postgis_manager.sh` script in the directory `$GPHOME/share/postgresql/contrib/postgis-2.5` to upgrade PostGIS in that database. This command upgrades PostGIS that is enabled in the database `mytest` in the Greenplum Database system.

    ```
    $GPHOME/share/postgresql/contrib/postgis-2.5/postgis_manager.sh mytest upgrade
    ```

4.  After running the script, you can verify that PostGIS 2.5.4 is installed and enabled as an extension in a database with this query.

    ```
    # SELECT * FROM pg_available_extensions WHERE name = 'postgis' ;
    ```

5.  You can validate that PostGIS 2.5 is enabled in the database with the `postgis_version()` function.

After you have completed the upgrade to PostGIS 2.5.4 `pivotal.3` for the Greenplum system and all the databases with PostGIS enabled, you enable PostGIS in a new database with the `CREATE EXTENSION postgis` command. To remove PostGIS support, use the `DROP EXTENSION postgis CASCADE` command.

### <a id="topic_unj_v5n_kmb"></a>Removing the PostGIS 2.1.5 package 

After upgrading the databases in the Greenplum Database system, you can remove the PostGIS 2.1.5 package from the system. This command removes the `postgis-2.1.5+pivotal.2` package from a Greenplum Database system.

```
gppkg -r postgis-2.1.5+pivotal.2
```

Run the `gppkg -q --all` command to list the installed Greenplum packages.

## <a id="topic_k4x_dp3_kmb"></a>Upgrade a PostGIS 2.5.4 Package from pivotal.1 or pivotal.2 to pivotal.3 

You can upgrade the installed PostGIS 2.5.4 package from `pivotal.1` or `pivotal.2` to `pivotal.3` \(a minor release upgrade\). The upgrade updates the PostGIS 2.5.4 package to the minor release \(`pivotal.3`\) that uses the same PostGIS version \(2.5.4\).

The `pivotal.3` minor release supports using the `CREATE EXTENSION` command and the `DROP EXTENSION` command to enable and remove PostGIS support in a database. See [Notes](#topic_hm5_3zk_jmb).

1.  Confirm you have a PostGIS 2.5.4 package `postgis-2.5.4+**pivotal.1**` or `postgis-2.5.4+**pivotal.2**` installed in a Greenplum Database system. See [Checking the PostGIS Version](#topic_yzz_l3h_kmb).
2.  Upgrade the PostGIS package in the Greenplum Database system using the `gppkg` option `-u`. The command updates the package to the `postgis-2.5.4+pivotal.3.build.1` package.

    ```
    gppkg -u postgis-2.5.4+pivotal.3.build.1-gp7-rhel8-x86_64.gppkg
    ```

3.  Run the `gppkg -q --all` command to verify the updated package version is installed in the Greenplum Database system.
4.  For all databases with PostGIS enabled, upgrade PostGIS with the PostGIS 2.5.4 `postgis_manager.sh` script that is in the directory `$GPHOME/share/postgresql/contrib/postgis-2.5` to upgrade PostGIS in that database. This command upgrades PostGIS that is enabled in the database `mytest` in the Greenplum Database system.

    ```
    $GPHOME/share/postgresql/contrib/postgis-2.5/postgis_manager.sh mytest upgrade
    ```


After you have completed the upgrade to PostGIS 2.5.4 `pivotal.3` for the Greenplum system and all the databases with PostGIS enabled, you enable PostGIS in a new database with the `CREATE EXTENSION postgis` command. To remove PostGIS support, use the `DROP EXTENSION postgis CASCADE` command.

## <a id="topic_yzz_l3h_kmb"></a>Checking the PostGIS Version 

When upgrading PostGIS you must check the version of the Greenplum PostGIS package installed on the Greenplum Database system and the version of PostGIS enabled in the database.

-   Check the installed PostGIS package version with the `gppkg` utility. This command lists all installed Greenplum packages.

    ```
    gppkg -q --all
    ```

-   Check the enabled PostGIS version in a database with the `postgis_version()` function. This `psql` command displays the version PostGIS that is enabled for the database `testdb`.

    ```
    psql -d testdb -c 'select postgis_version();'
    ```

    If PostGIS is not enabled for the database, Greenplum returns a `function does not exist` error.

-   For the Geenplum PostGIS package `postgis-2.5.4+pivotal.2` and later, you can display the PostGIS extension version and state in a database with this query.

    ```
    # SELECT * FROM pg_available_extensions WHERE name = 'postgis' ;
    ```

    The query displays the version whether the extension is installed and enabled in a database. If the PostGIS package is not installed, no rows are returned.


## <a id="topic_hm5_3zk_jmb"></a>Notes 

Starting with the Greenplum `postgis-2.5.4+pivotal.2` package, you enable support for PostGIS in a database with the `CREATE EXTENSION` command. For previous PostGIS 2.5.4 packages and all PostGIS 2.1.5 packages, you use an SQL script.

