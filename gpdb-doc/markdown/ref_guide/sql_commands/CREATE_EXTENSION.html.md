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

Loading an extension essentially amounts to running the extension script file. The script typically creates new SQL objects such as functions, data types, operators and index support methods. The `CREATE EXTENSION` command also records the identities of all the created objects, so that they can be dropped again if `DROP EXTENSION` is issued.

Loading an extension requires the same privileges that would be required to create the component extension objects. For most extensions this means superuser or database owner privileges are required. The user who runs `CREATE EXTENSION` becomes the owner of the extension for purposes of later privilege checks, as well as the owner of any objects created by the extension script.

## <a id="section4"></a>Parameters 

IF NOT EXISTS
:   Do not throw an error if an extension with the same name already exists. A notice is issued in this case. There is no guarantee that the existing extension is similar to the extension that would have been installed.

extension\_name
:   The name of the extension to be installed. The name must be unique within the database. An extension is created from the details in the extension control file `SHAREDIR/extension/extension\_name.control`.

    SHAREDIR is the installation shared-data directory, for example `/usr/local/greenplum-db/share/postgresql`. The command `pg_config --sharedir` displays the directory.

SCHEMA schema\_name
:   The name of the schema in which to install the extension objects. This assumes that the extension allows its contents to be relocated. The named schema must already exist. If not specified, and the extension control file does not specify a schema, the current default object creation schema is used.

    If the extension specifies a schema parameter in its control file, then that schema cannot be overridden with a `SCHEMA` clause. Normally, an error is raised if a `SCHEMA` clause is given and it conflicts with the extension schema parameter. However, if the `CASCADE` clause is also given, then schema\_name is ignored when it conflicts. The given schema\_name is used for the installation of any needed extensions that do not a specify schema in their control files.

    The extension itself is not within any schema. Extensions have unqualified names that must be unique within the database. But objects belonging to the extension can be within a schema.

VERSION version
:   The version of the extension to install. This can be written as either an identifier or a string literal. The default version is value that is specified in the extension control file.

FROM old\_version
:   Specify `FROM old\_version` only if you are attempting to install an extension that replaces an *old-style* module that is a collection of objects that is not packaged into an extension. If specified, `CREATE EXTENSION` runs an alternative installation script that absorbs the existing objects into the extension, instead of creating new objects. Ensure that `SCHEMA` clause specifies the schema containing these pre-existing objects.

    The value to use for old\_version is determined by the extension author, and might vary if there is more than one version of the old-style module that can be upgraded into an extension. For the standard additional modules supplied with pre-9.1 PostgreSQL, specify `unpackaged` for the old\_version when updating a module to extension style.

CASCADE
:   Automatically install dependent extensions are not already installed. Dependent extensions are checked recursively and those dependencies are also installed automatically. If the SCHEMA clause is specified, the schema applies to the extension and all dependent extensions that are installed. Other options that are specified are not applied to the automatically-installed dependent extensions. In particular, default versions are always selected when installing dependent extensions.

## <a id="section5"></a>Notes 

The extensions currently available for loading can be identified from the *[pg\_available\_extensions](../system_catalogs/pg_available_extensions.html)* or *[pg\_available\_extension\_versions](../system_catalogs/pg_available_extension_versions.html)* system views.

Before you use `CREATE EXTENSION` to load an extension into a database, the supporting extension files must be installed including an extension control file and at least one least one SQL script file. The support files must be installed in the same location on all Greenplum Database hosts. For information about creating new extensions, see PostgreSQL information about [Packaging Related Objects into an Extension](https://www.postgresql.org/docs/12/extend-extensions.html).

## <a id="section7"></a>Compatibility 

`CREATE EXTENSION` is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[ALTER EXTENSION](ALTER_EXTENSION.html), [DROP EXTENSION](DROP_EXTENSION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

