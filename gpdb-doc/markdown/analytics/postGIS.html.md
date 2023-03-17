---
title: Geospatial Analytics 
---

This chapter contains the following information:

-   [About PostGIS](#topic2)
-   [Greenplum PostGIS Extension](#topic3)
-   [Enabling and Removing PostGIS Support](#topic_b2l_hzw_q1b)
-   [Usage](#topic7)
-   [PostGIS Extension Support and Limitations](#postgis_support)

For information about upgrading PostGIS on Greenplum Database 6 systems, see [Upgrading PostGIS 2.1.5 or 2.5.4](postgis-upgrade.html)

## <a id="topic2"></a>About PostGIS 

PostGIS is a spatial database extension for PostgreSQL that allows GIS (Geographic Information Systems) objects to be stored in the database. The Greenplum PostGIS extension includes support for GiST-based R-Tree spatial indexes, and functions for analysis and processing of GIS objects.

The Greenplum PostGIS extension supports some PostGIS optional extensions and includes support for the PostGIS `raster` data type. With the PostGIS Raster objects, PostGIS `geometry` data type offers a single set of overlay SQL functions \(such as `ST_Intersects`\) operating seamlessly on vector and raster geospatial data. PostGIS Raster uses the GDAL \(Geospatial Data Abstraction Library\) translator library for raster geospatial data formats that presents a [single raster abstract data model](https://gdal.org/user/raster_data_model.html) to a calling application.

For information about Greenplum Database PostGIS extension support, see [PostGIS Extension Support and Limitations](#postgis_support).

For information about PostGIS, see [https://postgis.net/](https://postgis.net/)

For information about GDAL, see [https://gdal.org/](https://gdal.org/).

## <a id="topic3"></a>Greenplum PostGIS Extension 

The Greenplum PostGIS extension package is available from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb). After you download the package, you can follow the instructions in [Verifying the Greenplum Database Software Download](../install_guide/verify_sw.html) to verify the integrity of the download. You can install the package using the Greenplum Package Manager (`gppkg`). For details, see [`gppkg`](../utility_guide/ref/gppkg.html) in the _Greenplum Database Utility Guide_.

Greenplum Database supports the following PostGIS extension versions and components:

- PostGIS 2.5.4, and components Proj 4.8.0, Geos 3.10.2, GDAL 1.11.1, Json 0.12, Expat 2.4.4
- PostGIS 2.1.5, and components Proj 4.8.0, Geos 3.4.2, GDAL 1.11.1, Json 0.12, Expat 2.1.0

For information about the supported Greenplum extension packages and software versions, see [Extensions](../install_guide/platform-requirements-overview.html#topic_eyc_l2h_zz).

There are significant changes in PostGIS 2.5.4 compared with 2.1.5. For a list of new and enhanced functions in PostGIS 2.5, see the PostGIS documentation [PostGIS Functions new or enhanced in 2.5](https://postgis.net/docs/manual-2.5/PostGIS_Special_Functions_Index.html#NewFunctions_2_5) and [Release 2.5.4](https://postgis.net/docs/manual-2.5/release_notes.html).

<p class="note"><strong>Note:</strong> To upgrade PostGIS refer to  <a href="./postgis-upgrade.html">Upgrading PostGIS 2.1.5 or 2.5.4</a>.</p>

This table lists the PostGIS extensions support by Greenplum PostGIS.

<div class="tablenoborder"><table cellpadding="4" cellspacing="0" summary="" id="topic3__table_owt_4ml_xlb" class="table" frame="border" border="1" rules="all"><caption><span class="tablecap"><span class="table--title-label">Table 1. </span>Greenplum PostGIS Extensions</span></caption><colgroup><col style="width:32.786885245901644%" /><col style="width:67.21311475409836%" /></colgroup><thead class="thead" style="text-align:left;">
<tr class="row">
<th class="entry cellrowborder" style="vertical-align:top;" id="d47208e208">PostGIS Extension</th>
<th class="entry cellrowborder" style="vertical-align:top;" id="d47208e211">Greenplum PostGIS Notes</th>
</tr>
</thead>
<tbody class="tbody">
<tr class="row">
<td class="entry cellrowborder" style="vertical-align:top;" headers="d47208e208 "><code class="ph codeph">postgis</code><p dir="ltr" class="p">PostGIS and PostGIS Raster
                support</p>
</td>
<td class="entry cellrowborder" style="vertical-align:top;" headers="d47208e211 ">Supported. Both PostGIS and PostGIS Raster are enabled when the Greenplum
                  <code class="ph codeph">postgis</code> extension is enabled.</td>
</tr>
<tr class="row">
<td class="entry cellrowborder" style="vertical-align:top;" headers="d47208e208 "><code class="ph codeph">postgis_tiger_geocoder</code><p class="p">The US TIGER geocoder</p>
</td>
<td class="entry cellrowborder" style="vertical-align:top;" headers="d47208e211 ">Supported. Installed with Greenplum PostGIS. <p class="p">Requires the
                    <code class="ph codeph">postgis</code> and <code class="ph codeph">fuzzystrmatch</code>
                  extensions.</p>
<p class="p">The US TIGER geocoder converts addresses (like a street address)
                  to geographic coordinates.</p>
</td>
</tr>
<tr class="row">
<td class="entry cellrowborder" style="vertical-align:top;" headers="d47208e208 "><code class="ph codeph">address_standardizer</code><p class="p">Rule-based address
                standardizer</p>
</td>
<td class="entry cellrowborder" style="vertical-align:top;" headers="d47208e211 ">Supported. Installed but not enabled with Greenplum PostGIS. <p class="p">Can be used
                  with TIGER geocoder.</p>
<p class="p">A single line address parser that takes an input
                  address and normalizes it based on a set of rules stored in a table and helper
                    <code class="ph codeph">lex</code> and <code class="ph codeph">gaz</code> tables.</p>
</td>
</tr>
<tr class="row">
<td class="entry cellrowborder" style="vertical-align:top;" headers="d47208e208 "><code class="ph codeph">address_standardizer_data_us</code><p class="p">Sample rules tables for US
                  address data</p>
</td>
<td class="entry cellrowborder" style="vertical-align:top;" headers="d47208e211 ">Supported. Installed but not enabled with Greenplum PostGIS.<p class="p">Can be used with
                  the address standardizer.</p>
<p class="p">The extension contains <code class="ph codeph">gaz</code>,
                    <code class="ph codeph">lex</code>, and <code class="ph codeph">rules</code> tables for US address data. If
                  you are using other types of tables, see <a class="xref" href="#topic_wy2_rkb_3p">PostGIS Extension Limitations</a>.</p>
</td>
</tr>
<tr class="row">
<td class="entry cellrowborder" style="vertical-align:top;" headers="d47208e208 "><code class="ph codeph">fuzzystrmatch</code><p class="p">Fuzzy string matching</p>
</td>
<td class="entry cellrowborder" style="vertical-align:top;" headers="d47208e211 ">Supported. This extension is bundled but not enabled with Greenplum
                  Database.<p class="p">Required for the PostGIS TIGER geocoder.</p>
</td>
</tr>
</tbody>
</table>
</div>

> **Note** The PostGIS topology extension `postgis_topology` and the PostGIS 3D and geoprocessing extension `postgis_sfcgal` are not supported by Greenplum PostGIS and are not included in the Greenplum PostGIS extension package.

For information about the PostGIS extensions, see the [PostGIS 2.5 documentation](https://postgis.net/documentation/).

For information about Greenplum PostGIS feature support, see [PostGIS Extension Support and Limitations](#postgis_support).

## <a id="topic_b2l_hzw_q1b"></a>Enabling and Removing PostGIS Support 

This section describes how to enable and remove PostGIS and the supported PostGIS extensions, and how to configure PostGIS Raster.

-   [Enabling PostGIS Support](#topic_ln5_xcl_r1b)
-   [Enabling GDAL Raster Drivers](#topic_ydr_q5l_ybb)
-   [Enabling Out-of-Database Rasters](#topic_fx2_fpx_llb)
-   [Removing PostGIS Support](#topic_bgz_vcl_r1b)

For information about upgrading PostGIS on Greenplum Database 6 systems, see [Upgrading PostGIS 2.1.5 or 2.5.4](postgis-upgrade.html)

### <a id="topic_ln5_xcl_r1b"></a>Enabling PostGIS Support 

To enable PostGIS support, install the Greenplum PostGIS extension package into the Greenplum Database system, and then use the `CREATE EXTENSION` command to enable PostGIS support for an individual database.

#### <a id="section_dlv_xv1_rqb"></a>Installing the Greenplum PostGIS Extension Package 

Install Greenplum PostGIS extension package with the `gppkg` utility. For example, this command installs the package for RHEL 7.

```
gppkg -i postgis-2.5.4+pivotal.2.build.1-gp7-rhel8-x86_64.gppkg
```

After installing the package, source the `greenplum_path.sh` file and restart Greenplum Database. This command restarts Greenplum Database.

```
gpstop -ra
```

Installing the Greenplum PostGIS extension package updates the Greenplum Database system, including installing the supported PostGIS extensions to the system and updating `greenplum_path.sh` file with these lines for PostGIS Raster support.

```
export GDAL_DATA=$GPHOME/share/gdal
export POSTGIS_ENABLE_OUTDB_RASTERS=0
export POSTGIS_GDAL_ENABLED_DRIVERS=DISABLE_ALL
```

#### <a id="enable_postgis_cmd"></a>Using the CREATE EXTENSION Command 

These steps enable the PostGIS extension and the extensions that are used with PostGIS.

1.  To enable PostGIS and PostGIS Raster in a database, run this command after logging into the database.

    ```
    CREATE EXTENSION postgis ;
    ```

    To enable PostGIS and PostGIS Raster in a specific schema, create the schema, set the `search_path` to the PostGIS schema, and then enable the `postgis` extension with the `WITH SCHEMA` clause.

    ```
    SHOW search_path ; -- display the current search_path
    CREATE SCHEMA <schema_name> ;
    SET search_path TO <schema_name> ;
    CREATE EXTENSION postgis WITH SCHEMA <schema_name> ;
    ```

    After enabling the extension, reset the `search_path` and include the PostGIS schema in the `search_path` if needed.

2.  If needed, enable the PostGIS TIGER geocoder after enabling the `postgis` extension.

    To enable the PostGIS TIGER geocoder, you must enable the `fuzzystrmatch` extension before enabling `postgis_tiger_geocoder`. These two commands enable the extensions.

    ```
    CREATE EXTENSION fuzzystrmatch ;
    CREATE EXTENSION postgis_tiger_geocoder ;
    ```

3.  If needed, enable the rules-based address standardizer and add rules tables for the standardizer. These commands enable the extensions.

    ```
    CREATE EXTENSION address_standardizer ;
    CREATE EXTENSION address_standardizer_data_us ;
    ```


### <a id="topic_ydr_q5l_ybb"></a>Enabling GDAL Raster Drivers 

PostGIS uses GDAL raster drivers when processing raster data with commands such as `ST_AsJPEG()`. As the default, PostGIS deactivates all raster drivers. You enable raster drivers by setting the value of the `POSTGIS_GDAL_ENABLED_DRIVERS` environment variable in the `greenplum_path.sh` file on all Greenplum Database hosts.

Alternatively, you can do it at the session level by setting `postgis.gdal_enabled_drivers`. For a Greenplum Database session, this example `SET` command enables three GDAL raster drivers.

```
SET postgis.gdal_enabled_drivers TO 'GTiff PNG JPEG';
```

This `SET` command sets the enabled drivers to the default for a session.

```
SET postgis.gdal_enabled_drivers = default;
```

To see the list of supported GDAL raster drivers for a Greenplum Database system, run the `raster2pgsql` utility with the `-G` option on the Greenplum Database coordinator.

```
raster2pgsql -G 
```

The command lists the driver long format name. The *GDAL Raster* table at [https://gdal.org/drivers/raster/index.html](https://gdal.org/drivers/raster/index.html) lists the long format names and the corresponding codes that you specify as the value of the environment variable. For example, the code for the long name Portable Network Graphics is `PNG`. This example `export` line enables four GDAL raster drivers.

```
export POSTGIS_GDAL_ENABLED_DRIVERS="GTiff PNG JPEG GIF"
```

The `gpstop -r` command restarts the Greenplum Database system to use the updated settings in the `greenplum_path.sh` file.

After you have updated the `greenplum_path.sh` file on all hosts, and have restarted the Greenplum Database system, you can display the enabled raster drivers with the `ST_GDALDrivers()` function. This `SELECT` command lists the enabled raster drivers.

```
SELECT short_name, long_name FROM ST_GDALDrivers();
```

### <a id="topic_fx2_fpx_llb"></a>Enabling Out-of-Database Rasters 

After installing PostGIS, the default setting `POSTGIS_ENABLE_OUTDB_RASTERS=0` in the `greenplum_path.sh` file deactivates support for out-of-database rasters. To enable this feature, you can set the value to true \(a non-zero value\) on all hosts and restart the Greenplum Database system.

You can also activate or deactivate this feature for a Greenplum Database session. For example, this `SET` command enables the feature for the current session.

```
SET postgis.enable_outdb_rasters = true;				
```

> **Note** When the feature is enabled, the server configuration parameter `postgis.gdal_enabled_drivers` determines the accessible raster formats.

### <a id="topic_bgz_vcl_r1b"></a>Removing PostGIS Support 

You use the `DROP EXTENSION` command to remove support for the PostGIS extension and the extensions that are used with PostGIS.

Removing PostGIS support from a database does not remove these PostGIS Raster environment variables from the `greenplum_path.sh` file: `GDAL_DATA`, `POSTGIS_ENABLE_OUTDB_RASTERS`, `POSTGIS_GDAL_ENABLED_DRIVERS`. The environment variables are removed when you uninstall the PostGIS extension package.

> **Caution** Removing PostGIS support from a database drops PostGIS database objects from the database without warning. Users accessing PostGIS objects might interfere with the dropping of PostGIS objects. See [Notes](#postgis_note).

#### <a id="drop_postgis_cmd"></a>Using the DROP EXTENSION Command 

Depending on the extensions you enabled for PostGIS, drop support for the extensions in the database.

1.  If you enabled the address standardizer and sample rules tables, these commands drop support for those extensions from the current database.

    ```
    DROP EXTENSION IF EXISTS address_standardizer_data_us;
    DROP EXTENSION IF EXISTS address_standardizer;
    ```

2.  If you enabled the TIGER geocoder and the `fuzzystrmatch` extension to use the TIGER geocoder, these commands drop support for those extensions.

    ```
    DROP EXTENSION IF EXISTS postgis_tiger_geocoder;
    DROP EXTENSION IF EXISTS fuzzystrmatch;
    ```

3.  Drop support for PostGIS and PostGIS Raster. This command drops support for those extensions.

    ```
    DROP EXTENSION IF EXISTS postgis;
    ```

    If you enabled support for PostGIS and specified a specific schema with the `CREATE EXTENSION` command, you can update the `search_path` and drop the PostGIS schema if required.


#### <a id="section_wvr_wv1_rqb"></a>Uninstalling the Greenplum PostGIS Extension Package 

After PostGIS support has been removed from all databases in the Greenplum Database system, you can remove the PostGIS extension package. For example, this `gppkg` command removes the PostGIS extension package.

```
gppkg -r postgis-2.5.4+pivotal.2
```

After removing the package, ensure that these lines for PostGIS Raster support are removed from the `greenplum_path.sh` file.

```
export GDAL_DATA=$GPHOME/share/gdal
export POSTGIS_ENABLE_OUTDB_RASTERS=0
export POSTGIS_GDAL_ENABLED_DRIVERS=DISABLE_ALL
```

Source the `greenplum_path.sh` file and restart Greenplum Database. This command restarts Greenplum Database.

```
gpstop -ra
```

#### <a id="postgis_note"></a>Notes 

Removing PostGIS support from a database drops PostGIS objects from the database. Dropping the PostGIS objects cascades to objects that reference the PostGIS objects. Before removing PostGIS support, ensure that no users are accessing the database. Users accessing PostGIS objects might interfere with dropping PostGIS objects.

For example, this `CREATE TABLE` command creates a table with column `b` that is defined with the PostGIS `geometry` data type.

```
# CREATE TABLE test(a int, b geometry) DISTRIBUTED RANDOMLY;
```

This is the table definition in a database with PostGIS enabled.

```
# \d test
 Table "public.test"
 Column |   Type   | Modifiers
--------+----------+-----------
 a      | integer  |
 b      | geometry |
Distributed randomly
```

This is the table definition in a database after PostGIS support has been removed.

```
# \d test
  Table "public.test"
 Column |  Type   | Modifiers
--------+---------+-----------
 a      | integer |
Distributed randomly
```

## <a id="topic7"></a>Usage 

The following example SQL statements create non-OpenGIS tables and geometries.

```
CREATE TABLE geom_test ( gid int4, geom geometry, 
  name varchar(25) );

INSERT INTO geom_test ( gid, geom, name )
  VALUES ( 1, 'POLYGON((0 0 0,0 5 0,5 5 0,5 0 0,0 0 0))', '3D Square');
INSERT INTO geom_test ( gid, geom, name ) 
  VALUES ( 2, 'LINESTRING(1 1 1,5 5 5,7 7 5)', '3D Line' );
INSERT INTO geom_test ( gid, geom, name )
  VALUES ( 3, 'MULTIPOINT(3 4,8 9)', '2D Aggregate Point' );

SELECT * from geom_test WHERE geom &&
  Box3D(ST_GeomFromEWKT('LINESTRING(2 2 0, 3 3 0)'));
```

The following example SQL statements create a table and add a geometry column to the table with a SRID integer value that references an entry in the `SPATIAL_REF_SYS` table. The `INSERT` statements add two geopoints to the table.

```
CREATE TABLE geotest (id INT4, name VARCHAR(32) );
SELECT AddGeometryColumn('geotest','geopoint', 4326,'POINT',2);

INSERT INTO geotest (id, name, geopoint)
  VALUES (1, 'Olympia', ST_GeometryFromText('POINT(-122.90 46.97)', 4326));
INSERT INTO geotest (id, name, geopoint)
  VALUES (2, 'Renton', ST_GeometryFromText('POINT(-122.22 47.50)', 4326));

SELECT name,ST_AsText(geopoint) FROM geotest;
```

### <a id="topic8"></a>Spatial Indexes 

PostgreSQL provides support for GiST spatial indexing. The GiST scheme offers indexing even on large objects. It uses a system of lossy indexing in which smaller objects act as proxies for larger ones in the index. In the PostGIS indexing system, all objects use their bounding boxes as proxies in the index.

#### <a id="topic9"></a>Building a Spatial Index 

You can build a GiST index as follows:

```
CREATE INDEX indexname
  ON tablename
  USING GIST ( geometryfield );
```

## <a id="postgis_support"></a>PostGIS Extension Support and Limitations 

This section describes Greenplum PostGIS extension feature support and limitations.

-   [Supported PostGIS Data Types](#topic_g2d_hkb_3p)
-   [Supported PostGIS Raster Data Types](#topic_bl3_4vy_d1b)
-   [Supported PostGIS Index](#topic_y5z_nkb_3p)
-   [PostGIS Extension Limitations](#topic_wy2_rkb_3p)

In general, the Greenplum PostGIS extension does not support the following features:

-   The PostGIS topology extension `postgis_topology`
-   The PostGIS 3D and geoprocessing extension `postgis_sfcgal`
-   A small number of user defined functions and aggregates
-   PostGIS long transactions

For the PostGIS extensions supported by Greenplum PostGIS, see [Greenplum PostGIS Extension](#topic3).

### <a id="topic_g2d_hkb_3p"></a>Supported PostGIS Data Types 

Greenplum PostGIS extension supports these PostGIS data types:

-   box2d
-   box3d
-   geometry
-   geography

For a list of PostGIS data types, operators, and functions, see the [PostGIS reference documentation](https://postgis.net/docs/manual-2.5/reference.html).

### <a id="topic_bl3_4vy_d1b"></a>Supported PostGIS Raster Data Types 

Greenplum PostGIS supports these PostGIS Raster data types.

-   geomval
-   addbandarg
-   rastbandarg
-   raster
-   reclassarg
-   summarystats
-   unionarg

For information about PostGIS Raster data Management, queries, and applications, see [https://postgis.net/docs/manual-2.5/using\_raster\_dataman.html](https://postgis.net/docs/manual-2.5/using_raster_dataman.html).

For a list of PostGIS Raster data types, operators, and functions, see the [PostGIS Raster reference documentation](https://postgis.net/docs/manual-2.5/RT_reference.html).

### <a id="topic_y5z_nkb_3p"></a>Supported PostGIS Index 

Greenplum PostGIS extension supports the GiST \(Generalized Search Tree\) index.

### <a id="topic_wy2_rkb_3p"></a>PostGIS Extension Limitations 

This section lists the Greenplum PostGIS extension limitations for user-defined functions \(UDFs\), data types, and aggregates.

-   Data types and functions related to PostGIS topology functionality, such as TopoGeometry, are not supported by Greenplum Database.
-   These PostGIS aggregates are not supported by Greenplum Database:

    -   ST\_Collect
    -   ST\_MakeLine
    
    On a Greenplum Database with multiple segments, the aggregate might return different answers if it is called several times repeatedly.

-   Greenplum Database does not support PostGIS long transactions.

    PostGIS relies on triggers and the PostGIS table `public.authorization_table` for long transaction support. When PostGIS attempts to acquire locks for long transactions, Greenplum Database reports errors citing that the function cannot access the relation, `authorization_table`.

-   Greenplum Database does not support type modifiers for user defined types.

    The workaround is to use the `AddGeometryColumn` function for PostGIS geometry. For example, a table with PostGIS geometry cannot be created with the following SQL command:

    ```
    CREATE TABLE geometries(id INTEGER, geom geometry(LINESTRING));
    ```

    Use the `AddGeometryColumn` function to add PostGIS geometry to a table. For example, these following SQL statements create a table and add PostGIS geometry to the table:

    ```
    CREATE TABLE geometries(id INTEGER);
    SELECT AddGeometryColumn('public', 'geometries', 'geom', 0, 'LINESTRING', 2);
    ```

-   The `_postgis_index_extent` function is not supported on Greenplum Database 6 due to its dependence on spatial index operations.
-   The `<->` operator \(`geometry <-> geometry`\) returns the centroid/centroid distance for Greenplum Database 6.
-   The TIGER geocoder extension is supported. However, upgrading the TIGER geocoder extension is not supported.
-   The `standardize_address()` function uses `lex`, `gaz` or `rules` tables as parameters. If you are using tables apart from `us_lex`, `us_gaz` or `us_rules`, you should create them with the distribution policy `DISTRIBUTED REPLICATED` to work for Greenplum.

