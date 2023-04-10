# CREATE EXTENSION 

Registers an extension in a Greenplum database.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE EXTENSION [ IF NOT EXISTS ] <extension_name>
  [ WITH ] [ SCHEMA <schema_name> ]
           [ VERSION <version> ]
           [ FROM <old_version> ]
           [ CASCADE ]
```

## <a id="section3"></a>Description 

`CREATE EXTENSION` loads a new extension into the current database. There must not be an extension of the same name already loaded.

Loading an extension essentially amounts to running the extension script file. The script typically creates new SQL objects such as functions, data types, operators, and index support methods. `CREATE EXTENSION` additionally records the identities of all of the created objects, so that they can be dropped again if `DROP EXTENSION` is issued.

Loading an extension requires the same privileges that would be required to create its component objects. For most extensions this means superuser or database owner privileges are required. The user who runs `CREATE EXTENSION` becomes the owner of the extension for purposes of later privilege checks, as well as the owner of any objects created by the extension script.

## <a id="section4"></a>Parameters 

IF NOT EXISTS
:   Do not throw an error if an extension with the same name already exists. Greenplum Database issues a notice in this case. Note that there is no guarantee that the existing extension is anything like the one that would have been created from the currently-available script file.

extension\_name
:   The name of the extension to be installed. Greenplum Database will create the extension using details from the file `SHAREDIR/extension/<extension_name>.control`.
:   `SHAREDIR` is the installation shared-data directory, for example `/usr/local/greenplum-db/share/postgresql`. The command `pg_config --sharedir` displays the directory.

schema\_name
:   The name of the schema in which to install the extension objects, given that the extension allows its contents to be relocated. The named schema must already exist. If not specified, and the extension's control file does not specify a schema either, the current default object creation schema is used.
:   If the extension specifies a schema parameter in its control file, then that schema cannot be overridden with a `SCHEMA` clause. Normally, an error is raised if a `SCHEMA` clause is given and it conflicts with the extension's `schema` parameter. However, if the `CASCADE` clause is also given, then schema\_name is ignored when it conflicts. The given schema\_name is used for installation of any needed extensions that do not a specify schema in their control files.
:   Remember that the extension itself is not considered to be within any schema: extensions have unqualified names that must be unique database-wide. But objects belonging to the extension can be within schemas.

version
:   The version of the extension to install. This can be written as either an identifier or a string literal. The default version is whatever is specified in the extension's control file.

old\_version
:   Specify `FROM old_version` only if you are attempting to install an extension that replaces an *old-style* module that is a collection of objects that is not packaged into an extension. This option causes `CREATE EXTENSION` to run an alternative installation script that absorbs the existing objects into the extension, instead of creating new objects. Ensure that `SCHEMA` specifies the schema containing these pre-existing objects.
:   The value to use for old\_version is determined by the extension's author, and might vary if there is more than one version of the old-style module that can be upgraded into an extension.

CASCADE
:   Automatically install any extensions that this extension depends on that are not already installed. Dependent extensions are checked recursively and those dependencies are also installed automatically. If the `SCHEMA` clause is specified, the schema applies to the extension and all dependent extensions that are installed. Other options that are specified are not applied to the automatically-installed dependent extensions; in particular, their default versions are always selected.

## <a id="section5"></a>Notes 

Before you can use `CREATE EXTENSION` to load an extension into a database, the extension's supporting files must be installed. The supporting files must be installed in the same location on all Greenplum Database hosts. For information about creating new extensions, see the PostgreSQL [Packaging Related Objects into an Extension](https://www.postgresql.org/docs/12/extend-extensions.html) documentation.

The extensions currently available for loading can be identified from the [pg\_available\_extensions](../system_catalogs/pg_available_extensions.html) or [pg\_available\_extension\_versions](../system_catalogs/pg_available_extension_versions.html) system views.

<div class="note">Installing an extension as superuser requires trusting that the extension's author wrote the extension installation script in a secure fashion. It is not terribly difficult for a malicious user to create trojan-horse objects that will compromise later execution of a carelessly-written extension script, allowing that user to acquire superuser privileges. However, trojan-horse objects are only hazardous if they are in the <code>search_path</code> during script execution, meaning that they are in the extension's installation target schema or in the schema of some extension it depends on. Therefore, a good rule of thumb when dealing with extensions whose scripts have not been carefully vetted is to install them only into schemas for which <code>CREATE</code> privilege has not been and will not be granted to any untrusted users. Likewise for any extensions they depend on.<p>
The extensions supplied with Greenplum Database are believed to be secure against installation-time attacks of this sort, except for a few that depend on other extensions. As stated in the documentation for those extensions, they should be installed into secure schemas, or installed into the same schemas as the extensions they depend on, or both.</p></div>

## <a id="section6"></a>Examples 

Install the `hstore` extension into the current database, placing its objects in schema `addons`:

```
CREATE EXTENSION hstore SCHEMA addons;
```

Another way to accomplish the same thing:

```
SET search_path = addons;
CREATE EXTENSION hstore;
```

## <a id="section7"></a>Compatibility 

`CREATE EXTENSION` is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[ALTER EXTENSION](ALTER_EXTENSION.html), [DROP EXTENSION](DROP_EXTENSION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

