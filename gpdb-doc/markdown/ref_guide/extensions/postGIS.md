# Greenplum PostGIS Extension 

This chapter contains the following information:

-   [About PostGIS](#topic2)
-   [Greenplum PostGIS Extension](#topic3)
-   [Enabling and Removing PostGIS Support](#topic_b2l_hzw_q1b)
-   [Usage](#topic7)
-   [PostGIS Extension Support and Limitations](#postgis_support)
-   [PostGIS Support Scripts](#topic5)

**Parent topic:** [Greenplum Database Reference Guide](../ref_guide.html)

## About PostGIS 

PostGIS is a spatial database extension for PostgreSQL that allows GIS \(Geographic Information Systems\) objects to be stored in the database. The Greenplum Database PostGIS extension includes support for GiST-based R-Tree spatial indexes and functions for analysis and processing of GIS objects.

The Greenplum Database PostGIS extension supports the optional PostGIS `raster` data type and most PostGIS Raster functions. With the PostGIS Raster objects, PostGIS `geometry` data type offers a single set of overlay SQL functions \(such as `ST_Intersects`\) operating seamlessly on vector and raster geospatial data. PostGIS Raster uses the GDAL \(Geospatial Data Abstraction Library\) translator library for raster geospatial data formats that presents a [single raster abstract data model](http://www.gdal.org/gdal_datamodel.html) to a calling application.

For information about Greenplum Database PostGIS extension support, see [PostGIS Extension Support and Limitations](#postgis_support).

For information about PostGIS, see [http://postgis.refractions.net/](http://postgis.refractions.net/)

For information about GDAL, see [http://www.gdal.org](http://www.gdal.org).

### Greenplum PostGIS Extension 

The Greenplum Database PostGIS extension package is available from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb). After you download the package, you can follow the instructions in [Verifying the Greenplum Database Software Download](/vmware/install_guide/verify_sw.html) to verify the integrity of the **Greenplum Advanced Analytics PostGIS** software. You can install the package using the Greenplum Package Manager \(`gppkg`\). For details, see `gppkg` in the *Greenplum Database Utility Guide*.

Greenplum Database supports the PostGIS extension with these component versions.

-   PostGIS 2.1.5
-   Proj 4.8.0
-   Geos 3.4.2
-   GDAL 1.11.1
-   Json 0.12
-   Expat 2.1.0

For the information about supported extension packages and software versions see the *Greenplum Database Release Notes*.

Major enhancements and changes in PostGIS 2.1.5 from 2.0.3 include new PostGIS Raster functions. For a list of new and enhanced functions in PostGIS 2.1, see the PostGIS documentation [PostGIS Functions new or enhanced in 2.1](http://postgis.net/docs/manual-2.1/PostGIS_Special_Functions_Index.html#NewFunctions_2_1).

For a list of breaking changes in PostGIS 2.1, see [PostGIS functions breaking changes in 2.1](http://postgis.net/docs/manual-2.1/PostGIS_Special_Functions_Index.html#ChangedFunctions_2_1).

For a comprehensive list of PostGIS changes in PostGIS 2.1.5 and earlier, see PostGIS 2.1 Appendix A [Release 2.1.5](http://postgis.net/docs/manual-2.1/release_notes.html#idm34802).

#### Greenplum Database PostGIS Limitations 

The Greenplum Database PostGIS extension does not support the following features:

-   Topology
-   A small number of user defined functions and aggregates
-   PostGIS long transaction support
-   Geometry and geography type modifier

For information about Greenplum Database PostGIS support, see [PostGIS Extension Support and Limitations](#postgis_support).

## Enabling and Removing PostGIS Support 

The Greenplum Database PostGIS extension contains the `postgis_manager.sh` script that installs or removes both the PostGIS and PostGIS Raster features in a database. After the PostGIS extension package is installed, the script is in `$GPHOME/share/postgresql/contrib/postgis-2.1/`. The `postgis_manager.sh` script runs SQL scripts that install or remove PostGIS and PostGIS Raster from a database.

For information about the PostGIS and PostGIS Raster SQL scripts, and required PostGIS Raster environment variables, see [PostGIS Support Scripts](#topic5).

### Enabling PostGIS Support 

Run the `postgis_manager.sh` script specifying the database and with the `install` option to install PostGIS and PostGIS Raster. This example installs PostGIS and PostGIS Raster objects in the database `mydatabase`.

```
`postgis_manager.sh` mydatabase install
```

The script runs all the PostGIS SQL scripts that enable PostGIS in a database: `install/postgis.sql`, `install/rtpostgis.sql` `install/spatial_ref_sys.sql`, `install/postgis_comments.sql`, and `install/raster_comments.sql`.

The postGIS package installation adds these lines to the `greenplum_path.sh` file for PostGIS Raster support.

```
export GDAL_DATA=$GPHOME/share/gdal
export POSTGIS_ENABLE_OUTDB_RASTERS=0
export POSTGIS_GDAL_ENABLED_DRIVERS=DISABLE_ALL
```

### Enabling GDAL Raster Drivers 

PostGIS uses GDAL raster drivers when processing raster data with commands such as `ST_AsJPEG()`. As the default, PostGIS disables all raster drivers. You enable raster drivers by setting the value of the `POSTGIS_GDAL_ENABLED_DRIVERS` environment variable in the `greenplum_path.sh` file on all Greenplum Database hosts.

To see the list of supported GDAL raster drivers for a Greenplum Database system, run the `raster2pgsql` utility with the `-G` option on the Greenplum Database master.

```
raster2pgsql -G 
```

The command lists the driver long format name. The *GDAL Raster Formats* table at [http://www.gdal.org/formats\_list.html](http://www.gdal.org/formats_list.html) lists the long format names and the corresponding codes that you specify as the value of the environment variable. For example, the code for the long name Portable Network Graphics is `PNG`. This example `export` line enables four GDAL raster drivers.

```
export POSTGIS_GDAL_ENABLED_DRIVERS="GTiff PNG JPEG GIF"
```

The `gpstop -r` command restarts the Greenplum Database system to use the updated settings in the `greenplum_path.sh` file.

After you have updated the `greenplum_path.sh` file on all hosts, and have restarted the Greenplum Database system, you can display the enabled raster drivers with the `ST_GDALDrivers()` function. This `SELECT` command lists the enabled raster drivers.

```
SELECT short_name, long_name FROM ST_GDALDrivers();
```

### Removing PostGIS Support 

Run the `postgis_manager.sh` script specifying the database and with the `uninstall` option to remove PostGIS and PostGIS Raster. This example removes PostGIS and PostGIS Raster support from the database `mydatabase`.

```
`postgis_manager.sh` mydatabase uninstall
```

The script runs both the PostGIS SQL scripts that remove PostGIS and PostGIS Raster from a database: `uninstall_rtpostgis.sql` and `uninstall_postgis.sql`.

The `postgis_manager.sh` script does not remove these PostGIS Raster environment variables the `greenplum_path.sh` file: `GDAL_DATA`, `POSTGIS_ENABLE_OUTDB_RASTERS`, `POSTGIS_GDAL_ENABLED_DRIVERS`. The environment variables are removed when you uninstall the PostGIS extension package with the `gppkg` utility.

## Usage 

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
INSERT INTO geotest (id, name, geopoint)|
  VALUES (2, 'Renton', ST_GeometryFromText('POINT(-122.22 47.50)', 4326));
SELECT name,ST_AsText(geopoint) FROM geotest;
```

### Spatial Indexes 

PostgreSQL provides support for GiST spatial indexing. The GiST scheme offers indexing even on large objects. It uses a system of lossy indexing in which smaller objects act as proxies for larger ones in the index. In the PostGIS indexing system, all objects use their bounding boxes as proxies in the index.

#### Building a Spatial Index 

You can build a GiST index as follows:

```
CREATE INDEX indexname
  ON tablename
  USING GIST ( geometryfield );
```

## PostGIS Extension Support and Limitations 

This section describes Greenplum PostGIS extension feature support and limitations.

-   [Supported PostGIS Data Types](#topic_g2d_hkb_3p)
-   [Supported PostGIS Index](#topic_y5z_nkb_3p)
-   [Supported PostGIS Raster Data Types](#topic_bl3_4vy_d1b)
-   [PostGIS Extension Limitations](#topic_wy2_rkb_3p)

The Greenplum Database PostGIS extension does not support the following features:

-   Topology
-   Some Raster Functions

### Supported PostGIS Data Types 

Greenplum Database PostGIS extension supports these PostGIS data types:

-   box2d
-   box3d
-   geometry
-   geography

For a list of PostGIS data types, operators, and functions, see the [PostGIS reference documentation](http://postgis.net/docs/manual-2.1/reference.html).

### Supported PostGIS Raster Data Types 

Greenplum Database PostGIS supports these PostGIS Raster data types.

-   geomval
-   addbandarg
-   rastbandarg
-   raster
-   reclassarg
-   summarystats
-   unionarg

For information about PostGIS Raster data Management, queries, and applications [http://postgis.net/docs/manual-2.1/using\_raster\_dataman.html](http://postgis.net/docs/manual-2.1/using_raster_dataman.html)

For a list of PostGIS Raster data types, operators, and functions, see the [PostGIS Raster reference documentation](http://postgis.net/docs/manual-2.1/RT_reference.html).

### Supported PostGIS Index 

Greenplum Database PostGIS extension supports the GiST \(Generalized Search Tree\) index.

### PostGIS Extension Limitations 

This section lists the Greenplum Database PostGIS extension limitations for user-defined functions \(UDFs\), data types, and aggregates.

-   Data types and functions related to PostGIS topology functionality, such as TopoGeometry, are not supported by Greenplum Database.
-   Functions that perform `ANALYZE` operations for user-defined data types are not supported. For example, the ST\_Estimated\_Extent function is not supported. The function requires table column statistics for user defined data types that are not available with Greenplum Database.
-   These PostGIS aggregates are not supported by Greenplum Database:

    -   ST\_MemCollect
    -   ST\_MakeLine
    On a Greenplum Database with multiple segments, the aggregate might return different answers if it is called several times repeatedly.

-   Greenplum Database does not support PostGIS long transactions.

    PostGIS relies on triggers and the PostGIS table `public.authorization_table` for long transaction support. When PostGIS attempts to acquire locks for long transactions, Greenplum Database reports errors citing that the function cannot access the relation, `authorization_table`.

-   Greenplum Database does not support type modifiers for user defined types.

    The work around is to use the `AddGeometryColumn` function for PostGIS geometry. For example, a table with PostGIS geometry cannot be created with the following SQL command:

    ```
    CREATE TABLE geometries(id INTEGER, geom geometry(LINESTRING));
    ```

    Use the `AddGeometryColumn` function to add PostGIS geometry to a table. For example, these following SQL statements create a table and add PostGIS geometry to the table:

    ```
    CREATE TABLE geometries(id INTEGER);
    SELECT AddGeometryColumn('public', 'geometries', 'geom', 0, 'LINESTRING', 2);
    ```


## PostGIS Support Scripts 

After installing the PostGIS extension package, you enable PostGIS support for each database that requires its use. To enable or remove PostGIS support in your database, you can run SQL scripts that are supplied with the PostGIS package in `$GPHOME/share/postgresql/contrib/postgis-2.1/`.

-   [Scripts that Enable PostGIS and PostGIS Raster Support](#topic_ulm_4gl_r1b)
-   [Scripts that Remove PostGIS and PostGIS Raster Support](#topic_xp2_lgl_r1b)

Instead of running the scripts individually, you can use the `postgis_manager.sh` script to run SQL scripts that enable or remove PostGIS support. See [Enabling and Removing PostGIS Support](#topic_b2l_hzw_q1b).

You can run the PostGIS SQL scripts individually to enable or remove PostGIS support. For example, these commands run the SQL scripts `postgis.sql`, `rtpostgis.sql`, and `spatial_ref_sys.sql` in the database `mydatabase`.

```
psql -d mydatabase -f 
  $GPHOME/share/postgresql/contrib/postgis-2.1/install/postgis.sql
psql -d mydatabase -f 
  $GPHOME/share/postgresql/contrib/postgis-2.1/install/rtpostgis.sql
psql -d mydatabase -f 
  $GPHOME/share/postgresql/contrib/postgis-2.1/install/spatial_ref_sys.sql
```

After running the scripts, the database is enabled with both PostGIS and PostGIS Raster.

### Scripts that Enable PostGIS and PostGIS Raster Support 

These scripts enable PostGIS, and the optional PostGIS Raster in a database.

-   `install/postgis.sql` - Load the PostGIS objects and function definitions.
-   `install/rtpostgis.sql` - Load the PostGIS `raster` object and function definitions.

**Note:** If you are installing PostGIS Raster, PostGIS objects must be installed before PostGIS Raster. PostGIS Raster depends on PostGIS objects. Greenplum Database returns an error if `rtpostgis.sql` is run before `postgis.sql`.

These SQL scripts add data and comments to a PostGIS enabled database.

-   `install/spatial_ref_sys.sql` - Populate the `spatial_ref_sys` table with a complete set of EPSG coordinate system definition identifiers. With the definition identifiers you can perform `ST_Transform()` operations on geometries.

    **Note:** If you have overridden standard entries and want to use those overrides, do not load the `spatial_ref_sys.sql` file when creating the new database.

-   `install/postgis_comments.sql` - Add comments to the PostGIS functions.
-   `install/raster_comments.sql` - Add comments to the PostGIS Raster functions.

You can view comments with the `pslq` meta-command `\dd function\_name` or from any tool that can show Greenplum Database function comments.

#### PostGIS Raster Environment Variables 

The postGIS package installation adds these lines to the `greenplum_path.sh` file for PostGIS Raster support.

```
export GDAL_DATA=$GPHOME/share/gdal
export POSTGIS_ENABLE_OUTDB_RASTERS=0
export POSTGIS_GDAL_ENABLED_DRIVERS=DISABLE_ALL
```

`GDAL_DATA` specifies the location of GDAL utilities and support files used by the GDAL library. For example, the directory contains EPSG support files such as `gcs.csv​` and `pcs.csv` \(so called dictionaries, mostly in ​CSV format\). The GDAL library requires the support files to properly evaluate EPSG codes.

`POSTGIS_GDAL_ENABLED_DRIVERS` sets the enabled GDAL drivers in the PostGIS environment.

`POSTGIS_ENABLE_OUTDB_RASTERS` is a boolean configuration option to enable access to out of database raster bands.

### Scripts that Remove PostGIS and PostGIS Raster Support 

To remove PostGIS support from a database, run SQL scripts that are supplied with the PostGIS extension package in `$GPHOME/share/postgresql/contrib/postgis-2.1/`

**Note:** If you installed PostGIS Raster, you must uninstall PostGIS Raster before you uninstall the PostGIS objects. PostGIS Raster depends on PostGIS objects. Greenplum Database returns an error if PostGIS objects are removed before PostGIS Raster.

These scripts remove PostGIS and PostGIS Raster objects from a database.

-   `uninstall/uninstall_rtpostgis.sql` - Removes the PostGIS Raster object and function definitions.
-   `uninstall/uninstall_postgis.sql` - Removes the PostGIS objects and function definitions.

After PostGIS support has been removed from all databases in the Greenplum Database system, you can remove the PostGIS extension package. For example this `gppkg` command removes the PostGIS extension package.

```
gppkg -r postgis-2.1.5+pivotal.2
```

Restart Greenplum Database after removing the package.

```
gpstop -r
```

Ensure that these lines for PostGIS Raster support are removed from the `greenplum_path.sh` file.

```
export GDAL_DATA=$GPHOME/share/gdal
export POSTGIS_ENABLE_OUTDB_RASTERS=0
export POSTGIS_GDAL_ENABLED_DRIVERS=DISABLE_ALL
```

