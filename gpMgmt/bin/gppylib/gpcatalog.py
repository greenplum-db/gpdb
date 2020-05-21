#!/usr/bin/env python
#
# Copyright (c) 2010-2011 EMC Corporation.  All Rights Reserved
#
"""
   gpcatalog.py

     Contains two classes representing catalog metadata:

       Catalog - a container class for CatalogTables
       CatalogTable - metadata about a single tables
"""
# ============================================================================
import os
import json
from gppylib import gplog
from gppylib.gpversion import GpVersion

logger = gplog.get_default_logger()

class GPCatalogException(Exception):
    pass

# Hard coded since "master only" is not defined in the catalog
MASTER_ONLY_TABLES = [
    'gp_configuration_history',
    'gp_segment_configuration',
    'pg_auth_time_constraint',
    'pg_description',
    'pg_partition',
    'pg_partition_encoding',
    'pg_partition_rule',
    'pg_shdescription',
    'pg_stat_last_operation',
    'pg_stat_last_shoperation',
    'pg_statistic',
    ]

# Hard coded tables that have different values on every segment
SEGMENT_LOCAL_TABLES = [
    'gp_fastsequence', # AO segment row id allocations
    'gp_id',
    'pg_shdepend', # (not if we fix oid inconsistencies)
    'pg_statistic',
    ]

# These catalog tables either do not use pg_depend or does not create an
# entry in pg_depend immediately when an entry is created in that
# catalog table
DEPENDENCY_EXCLUSION = [
    'pg_authid',
    'pg_compression',
    'pg_conversion',
    'pg_database',
    'pg_enum',
    'pg_namespace',
    'pg_partition',
    'pg_partition_rule',
    'pg_resgroup',
    'pg_resgroupcapability',
    'pg_resourcetype',
    'pg_resqueue',
    'pg_resqueuecapability',
    'pg_tablespace'
    ]

# ============================================================================
class GPCatalog():
    """
    Catalog is a container class that contains dictionary of CatalogTable 
    objects.

    It provides the CatalogTables with a context that they can use to
    refer to other CatalogTables (e.g. describe foreign keys) and it
    provides calling code with a simple wrapper for what a known catalog
    layout looks like.

    It supports multiple source versions of the database.  It issues a
    warning if there are catalog tables defined in the database that
    it is unaware of, usually indicating that it is operating against
    an unknown version.
    """

    # --------------------------------------------------------------------
    # Public API functions:
    #   - Catalog()              - Create a Catalog object
    #   - getCatalogTable()      - Returns a single CatalogTable
    #   - getCatalogTables()     - Returns a list of CatalogTable
    #   - getCatalogVersion()    - Returns a GpVersion
    # --------------------------------------------------------------------
    def getCatalogTable(self, tablename):
        """
        getCatalogTable(tablename) => Returns the specified CatalogTable
        
        Raises: CatalogException when the table does not exist
        """
        if tablename not in self._tables:
            raise GPCatalogException("No such catalog table: %s" % str(tablename))

        return self._tables[tablename]

    def getCatalogTables(self):
        """
        getCatalogTables() => Returns a list of CatalogTable
        """
        return self._tables.values()

    def getCatalogVersion(self):
        """
        getCatalogVersion() => Returns the GpVersion object
        """
        return self._version

    # --------------------------------------------------------------------
    # Private implementation functions:
    # --------------------------------------------------------------------
    def __init__(self, dbConnection):
        """
        Catalog() constructor

        1) Uses the supplied database connection to get a list of catalog tables
        2) iterate through the list building up CatalogTable objects
        3) Mark "master only" tables manually
        4) Mark a couple primary keys manually
        5) Mark foreign keys manually
        6) Mark known catalog differences manually
        7) Validate and return the Catalog object
        """
        self._dbConnection = dbConnection
        self._tables = {} 
        self._version = None
        self._tidycat = {} # tidycat definitions from JSON file

        version_query = """
           SELECT version()
        """
        catalog_query = """
           SELECT relname, relisshared FROM pg_class 
           WHERE relnamespace=11 and relkind = 'r' 
        """

        # Read the catalog version from the database
        try:
            curs = self._query(version_query)
        except Exception, e:
            raise GPCatalogException("Error reading database version: " + str(e))
        self._version = GpVersion(curs.getresult()[0][0])

        # Read the list of catalog tables from the database
        try:
            curs = self._query(catalog_query)
        except Exception, e:
            raise GPCatalogException("Error reading catalog: " + str(e))

        # Construct our internal representation of the catalog
        
        for [relname, relisshared] in curs.getresult():
            self._tables[relname] = GPCatalogTable(self, relname)
            # Note: stupid API returns t/f for boolean value
            self._tables[relname]._setShared(relisshared == 't')
        
        # The tidycat.pl utility has been used to generate a json file 
        # describing aspects of the catalog that we can not currently
        # interrogate from the catalog itself.  This includes things
        # like which tables are master only vs segment local and what 
        # the foreign key relationships are.
        self._getJson()

        # Which tables are "master only" is not derivable from the catalog
        # so we have to set this manually.
        self._markMasterOnlyTables()

        # We derived primary keys for most of the catalogs based on un
        # unique indexes, but we have to manually set a few stranglers
        self._setPrimaryKeys()

        # Foreign key relationships of the catalog tables are not actually
        # defined in the catalog, so must be obtained from tidycat
        self._setForeignKeys()

        # Most catalog tables are now ready to go, but some columns can
        # not be compared directly between segments, we need to indicate
        # these exceptions manually.
        self._setKnownDifferences()

        # Finally validate that everything looks right, this will issue
        # warnings if there are any regular catalog tables that do not
        # have primary keys set.
        self._validate()

    def _query(self, qry):
        """
        Simple wrapper around querying the database connection
        """
        return self._dbConnection.query(qry)

    def _markMasterOnlyTables(self):
        """
        We mark two types of catalog tables as "master only"
          - True "master only" tables
          - Tables we know to have different contents on master/segment

        While the later two are not technically "master only" they have
        the property that we cannot validate cross segment consistency,
        which makes them the same for our current purposes.

        We may want to eventually move these other types of tables into
        a different classification.
        """
        for name in MASTER_ONLY_TABLES:
            if name in self._tables:
                self._tables[name]._setMasterOnly()

        for name in SEGMENT_LOCAL_TABLES:
            if name in self._tables:
                self._tables[name]._setMasterOnly()

    def _setPrimaryKeys(self):
        """
        Most of the catalog primary keys are set automatically in
        CatalogTable by looking at unique indexes over the catalogs.

        However there are a couple of catalog tables that do not have
        unique indexes that we still want to perform cross segment
        consistency on, for them we have to manually set a primary key
        """
        self._tables['gp_version_at_initdb']._setPrimaryKey(
            "schemaversion productversion")
        self._tables['pg_constraint']._setPrimaryKey(
            "conname connamespace conrelid contypid")
        self._tables['pg_depend']._setPrimaryKey(
            "classid objid objsubid refclassid refobjid refobjsubid deptype")

        if self._version >= "4.0":
            self._tables['pg_resqueuecapability']._setPrimaryKey(
                "resqueueid restypid")

    def _getJson(self):
        """
        Read the json file generated by tidycat which contains, among other
        things, the primary key/foreign key relationships for the catalog
        tables.  Build the fkeys for each table and validate them against 
        the catalog.
        """
        indir = os.path.dirname(__file__)
        jname = str(self._version.getVersionRelease()) + ".json"
        try:
            # json doc in data subdirectory of pylib module
            infil = open(os.path.join(indir, "data", jname), "r")
            d = json.load(infil)
            # remove the tidycat comment
            if "__comment" in d:
                del d["__comment"]
            if "__info" in d:
                del d["__info"]
            infil.close()
            self._tidycat = d
        except Exception, e:
            # older versions of product will not have tidycat defs --
            # need to handle this case
            logger.warn("GPCatalogTable: "+ str(e))

    def _setForeignKeys(self):
        """
        Setup the foreign key relationships amongst the catalogs.  We 
        drive this based on the tidycat generate json file since this
        information is not derivable from the catalog.
        """
        try:
            for tname, tdef in self._tidycat.iteritems():
                if "foreign_keys" not in tdef:
                    continue
                for fkdef in tdef["foreign_keys"]:
                    fk2 = GPCatalogTableForeignKey(tname, 
                                                   fkdef[0], 
                                                   fkdef[1], 
                                                   fkdef[2])
                    self._tables[tname]._addForeignKey(fk2)
        except Exception, e:
            # older versions of product will not have tidycat defs --
            # need to handle this case
            logger.warn("GPCatalogTable: "+ str(e))


    def _setKnownDifferences(self):
        """
        Some catalogs have columns that, for one reason or another, we
        need to mark as being different between the segments and the master.
        
        These fall into two categories:
           - Bugs (marked with the appropriate jiras)
           - A small number of "special" columns
        """

        # -------------
        # Special cases
        # -------------
        
        # pg_class:
        #   - relfilenode should generally be consistent, but may not be (jira?)
        #   - relpages/reltuples/relfrozenxid/relminmxid are all vacumm/analyze related
        #   - relhasindex/relhaspkey are only cleared when vacuum completes
        #   - relowner has its own checks:
        #       => may want to separate out "owner" columns like acl and oid
        self._tables['pg_class']._setKnownDifferences(
            "relfilenode relpages reltuples relhasindex relhaspkey relowner relfrozenxid relminmxid relallvisible")

        # pg_type: typowner has its own checks:
        #       => may want to separate out "owner" columns like acl and oid
        self._tables['pg_type']._setKnownDifferences("typowner")

        # pg_database: datfrozenxid and datminmxid are vacuum related
        self._tables['pg_database']._setKnownDifferences("datfrozenxid datminmxid")

        # -------------
        # Issues still present in the product
        # -------------

        # MPP-11289 : inconsistent OIDS for table "default values"
        self._tables['pg_attrdef']._setKnownDifferences("oid")

        # MPP-11284 : inconsistent OIDS for constraints
        self._tables['pg_constraint']._setKnownDifferences("oid")

        # MPP-11282: Inconsistent oids for language callback functions
        # MPP-12015: Inconsistent oids for operator communtator/negator functions
        self._tables['pg_proc']._setKnownDifferences("oid prolang")

        # MPP-11282: pg_language oids and callback functions
        self._tables['pg_language']._setKnownDifferences("oid lanplcallfoid lanvalidator")

        # MPP-12015: Inconsistent oids for operator communtator/negator functions
        # MPP-12015: Inconsistent oids for operator sort/cmp operators
        self._tables['pg_operator']._setKnownDifferences(
            "oid oprcom oprnegate oprlsortop oprrsortop oprltcmpop oprgtcmpop")

        self._tables['pg_aggregate']._setKnownDifferences("aggsortop")

        # MPP-11281 : Inconsistent oids for views
        self._tables['pg_rewrite']._setKnownDifferences("oid ev_action")

        # MPP-11285 : Inconsistent oids for triggers
        self._tables['pg_trigger']._setKnownDifferences("oid")

        # MPP-11575 : Inconsistent handling of indpred for partial indexes
        # indcheckxmin column related to HOT feature in pg_index is calculated
        # independently for master and segment based on individual nodes
        # transaction state, hence it can be different so skip it from checks.
        self._tables['pg_index']._setKnownDifferences("indpred, indcheckxmin")

        # This section should have exceptions for tables for which OIDs are not
        # synchronized between master and segments, refer function
        # RelationNeedsSynchronizedOIDs() in catalog.c
        self._tables['pg_amop']._setKnownDifferences("oid, amopopr")
        self._tables['pg_amproc']._setKnownDifferences("oid");

    def _validate(self):
        """
        Check that all tables defined in the catalog have either been marked
        as "master only" or have a primary key
        """
        for relname in sorted(self._tables):
            if self._tables[relname].isMasterOnly():
                continue
            if self._tables[relname].getPrimaryKey() == []:
                logger.warn("GPCatalogTable: unable to derive primary key for %s"
                            % str(relname))


# ============================================================================
class GPCatalogTable():

    # --------------------------------------------------------------------
    # Public API functions:
    #
    # Accessor functions
    #   - getTableName()     - Returns the table name (string)
    #   - tableHasOids()     - Returns if the table has oids (boolean)
    #   - isMasterOnly()     - Returns if the table is "master only" (boolean)
    #   - isShared()         - Returns if the table is shared (boolean)
    #   - getTableAcl()      - Returns name of the acl column (string|None)
    #   - getPrimaryKey()    - Returns the primary key (list)
    #   - getForeignKeys()   - Returns a list of foreign keys (list)
    #   - getTableColumns()  - Returns a list of table columns (list)
    #
    # --------------------------------------------------------------------
    def getTableName(self):
        return self._name

    def tableHasOids(self):
        return self._has_oid

    def tableHasConsistentOids(self):
        return (self._has_oid and 'oid' not in self._excluding)

    def isMasterOnly(self):
        return self._master

    def isShared(self):
        return self._isshared

    def getTableAcl(self):
        return self._acl

    def getPrimaryKey(self):
        return self._pkey

    def getForeignKeys(self):
        return self._fkey
    
    def getTableColtypes(self):
        return self._coltypes

    def getTableColumns(self, with_oid=True, with_acl=True, excluding=None):
        '''
        Returns the list of columns this catalog table contains.

        Optionally excluding:
           - oid columns
           - acl columns
           - user specified list of excluded columns

        By default excludes the "known differences" columns, to include them
        pass [] as the excluding list.
        '''
        if excluding is None:
            excluding = self._excluding
        else:
            excluding = set(excluding)

        # Return all columns that are not excluded
        return [
            x for x in self._columns 
            if ((with_oid or x != 'oid')     and
                (with_acl or x != self._acl) and
                (x not in excluding))
            ]


    # --------------------------------------------------------------------
    # Private Implementation functions
    # --------------------------------------------------------------------
    def __init__(self, parent, name, pkey=None):
        """
        Create a new GPCatalogTable object
        
        Uses the supplied database connection to identify:
          - What are the columns in the table?
          - Does the catalog table have an oid column?
          - Does the catalog table have an acl column?
        """
        assert(name != None)
     
        # Split string input
        if isinstance(pkey, str):    
            pkey = pkey.split()

        self._parent    = parent
        self._name      = name
        self._master    = False
        self._isshared  = False
        self._pkey      = list(pkey or [])
        self._fkey      = []      # foreign key
        self._excluding = set()
        self._columns   = []      # initial value
        self._coltypes  = {}
        self._acl       = None    # initial value
        self._has_oid   = False   # initial value

        # Query the database to lookup the catalog's definition
        qry = """
          select a.attname, a.atttypid, t.typname
          from pg_attribute a
               left outer join pg_type t on (a.atttypid = t.oid)
          where attrelid = 'pg_catalog.%s'::regclass and 
               (attnum > 0 or attname='oid')
          order by attnum
        """ % name
        try:
            cur = parent._query(qry)
        except:
            # The cast to regclass will fail if the catalog table doesn't
            # exist.
            raise GPCatalogException("Catalog table %s does not exist" % name)

        if cur.ntuples() == 0:
            raise GPCatalogException("Catalog table %s does not exist" % name)

        for row in cur.getresult():
            (attname, atttype, typname) = row

            # Mark if the catalog has an oid column
            if attname == 'oid':
                self._has_oid = True

            # Detect the presence of an ACL column
            if atttype == 1034:
                self._acl = attname

            # Add to the list of columns
            self._columns.append(attname)

            # Add to the coltypes dictionary
            self._coltypes[attname] = typname

        # If a primary key was not specified try to locate a unique index
        # If a table has multiple matching indexes, we'll pick the first index
        # order by indkey to avoid the issue of MPP-16663. 
        if self._pkey == []:
            qry = """
            SELECT attname FROM (
              SELECT unnest(indkey) as keynum FROM (
                SELECT indkey 
                FROM pg_index 
                WHERE indisunique and not (indkey @> '-2'::int2vector) and
                      indrelid = 'pg_catalog.{catname}'::regclass
                ORDER BY indkey LIMIT 1
              ) index_keys
            ) unnested_index_keys
            JOIN pg_attribute ON (attnum = keynum)
            WHERE attrelid = 'pg_catalog.{catname}'::regclass
            """.format(catname=name)
            cur = parent._query(qry)
            self._pkey = [row[0] for row in cur.getresult()]

        # Primary key must be in the column list
        for k in self._pkey:
            if k not in self._columns:
                raise GPCatalogException("%s.%s does not exist" % (name, k))

    def __str__(self):
        return self._name

    def __hash__(self):
        return hash(self.__str__())

    def __repr__(self):
        return "GPCatalogTable: %s; pkey: %s; oids: %s; acl: %s" % (
            str(self._name), str(self._pkey), str(self._has_oid), str(self._acl),
            )

    def __cmp__(self, other):
        return cmp(other, self._name)

    def _setMasterOnly(self, value=True):
        self._master = value

    def _setShared(self, value):
        self._isshared = value

    def _setPrimaryKey(self, pkey=None):
        # Split string input
        if isinstance(pkey, str):
            pkey = pkey.split()

        # Check that the specified keys are real columns
        pkey = list(pkey or [])
        for k in pkey:
            if k not in self._columns:
                raise Exception("%s.%s does not exist" % (self._name, k))

        self._pkey = pkey

    def _addForeignKey(self, fkey):
        # Check that the specified keys are real columns
        for k in fkey.getColumns():
            if k not in self._columns:
                raise Exception("%s.%s does not exist" % (self._name, k))

        self._fkey.append(fkey)

    def _setKnownDifferences(self, diffs):
        # Split string input
        if isinstance(diffs, str):    
            diffs = diffs.split()
        self._excluding = set(diffs or [])



# ============================================================================
class GPCatalogTableForeignKey():
    """
    GPCatalogTableForeignKey is a container for a single instance of a
    postgres catalog primary key/foreign key relationship.  The
    foreign key is a set of columns for with a table, associated with
    a set of primary key columns on a primary key table.

    Note that tables can self-join, so it is possible to have the
    primary and foreign key tables be one and the same.

    This class constructs the key, but does not validate it against
    the catalog.

    """
    # --------------------------------------------------------------------
    # Public API functions:
    #
    # Accessor functions
    #   - getTableName()      - Returns name of table with fkeys
    #   - getPkeyTableName()  - Returns name of the pkey table for the fkeys
    #   - getColumns()        - Returns a list of [foreign] key columns (list)
    #   - getPKey()           - Returns a list of primary key columns (list)
    #
    # --------------------------------------------------------------------
    def getTableName(self):
        return self._tname

    def getPkeyTableName(self):
        return self._pktablename

    def getColumns(self):
        return self._columns 

    def getPKey(self):
        return self._pkey 

    # --------------------------------------------------------------------
    # Private Implementation functions
    # --------------------------------------------------------------------
    def __init__(self, tname, cols, pktablename, pkey):
        """
        Create a new GPCatalogTableForeignKey object
        

        """
        assert(tname != None)
        assert(pktablename != None)
     
        # Split string input
        if isinstance(pkey, str):    
            pkey = pkey.split()

        self._tname       = tname
        self._pktablename = pktablename
        self._pkey        = list(pkey or [])
        self._columns     = cols

    def __str__(self):
        return "%s: %s" % (self._tname, str(self._columns))

    def __repr__(self):
        return "GPCatalogTableForeignKey: %s; col: %s; " % (
            str(self._tname), str(self._columns)
            )
