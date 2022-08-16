---
title: Objects Deprecated in Greenplum 6 
---

Greenplum Database 6 has deprecated several database objects. These changes can effect the successful upgrade from one major version to another. Review these objects when using Geenplum Upgrade, or Greenplum Backup and Restore. This topic highlight these changes.

-   [Deprecated Relations](#dep_relations)
-   [Deprecated Columns](#dep_columns)
-   [Deprecated Functions and Procedures](#dep_functions_procedures)
-   [Deprecated Types, Domains, and Composite Types](#dep_types_domains_composite)
-   [Deprecated Operators](#dep_operators)

**Parent topic:** [Greenplum Database Reference Guide](ref_guide.html)

## <a id="dep_relations"></a>Deprecated Relations 

The following list includes the Greenplum Database 6 deprecated relations.

-   gp\_toolkit.\_\_gp\_localid
-   gp\_toolkit.\_\_gp\_masterid
-   gp\_toolkit.\_\_gp\_masterid
-   pg\_catalog.gp\_configuration
-   pg\_catalog.gp\_configuration
-   pg\_catalog.gp\_db\_interfaces
-   pg\_catalog.gp\_fault\_strategy
-   pg\_catalog.gp\_global\_sequence
-   pg\_catalog.gp\_interfaces
-   pg\_catalog.gp\_persistent\_database\_node
-   pg\_catalog.gp\_persistent\_filespace\_node
-   pg\_catalog.gp\_persistent\_relation\_node
-   pg\_catalog.gp\_persistent\_tablespace\_node
-   pg\_catalog.gp\_relation\_node
-   pg\_catalog.pg\_autovacuum
-   pg\_catalog.pg\_filespace
-   pg\_catalog.pg\_filespace\_entry
-   pg\_catalog.pg\_listener
-   pg\_catalog.pg\_window

## <a id="dep_columns"></a>Deprecated Columns 

The following list includes the Greenplum Database 6 deprecated columns.

-   gp\_toolkit.gp\_resgroup\_config.proposed\_concurrency
-   gp\_toolkit.gp\_resgroup\_config.proposed\_memory\_limit
-   gp\_toolkit.gp\_resgroup\_config.proposed\_memory\_shared\_quota
-   gp\_toolkit.gp\_resgroup\_config.proposed\_memory\_spill\_ratio
-   gp\_toolkit.gp\_workfile\_entries.current\_query
-   gp\_toolkit.gp\_workfile\_entries.directory
-   gp\_toolkit.gp\_workfile\_entries.procpid
-   gp\_toolkit.gp\_workfile\_entries.state
-   gp\_toolkit.gp\_workfile\_entries.workmem
-   gp\_toolkit.gp\_workfile\_usage\_per\_query.current\_query
-   gp\_toolkit.gp\_workfile\_usage\_per\_query.procpid
-   gp\_toolkit.gp\_workfile\_usage\_per\_query.state
-   information\_schema.triggers.condition\_reference\_new\_row
-   information\_schema.triggers.condition\_reference\_new\_table
-   information\_schema.triggers.condition\_reference\_old\_row
-   information\_schema.triggers.condition\_reference\_old\_table
-   information\_schema.triggers.condition\_timing
-   pg\_catalog.gp\_distribution\_policy.attrnums
-   pg\_catalog.gp\_segment\_configuration.replication\_port
-   pg\_catalog.pg\_aggregate.agginvprelimfn
-   pg\_catalog.pg\_aggregate.agginvtransfn
-   pg\_catalog.pg\_aggregate.aggordered
-   pg\_catalog.pg\_aggregate.aggprelimfn
-   pg\_catalog.pg\_am.amcanshrink
-   pg\_catalog.pg\_am.amgetmulti
-   pg\_catalog.pg\_am.amindexnulls
-   pg\_catalog.pg\_amop.amopreqcheck
-   pg\_catalog.pg\_authid.rolconfig
-   pg\_catalog.pg\_authid.rolcreaterexthdfs
-   pg\_catalog.pg\_authid.rolcreatewexthdfs
-   pg\_catalog.pg\_class.relfkeys
-   pg\_catalog.pg\_class.relrefs
-   pg\_catalog.pg\_class.reltoastidxid
-   pg\_catalog.pg\_class.reltriggers
-   pg\_catalog.pg\_class.relukeys
-   pg\_catalog.pg\_database.datconfig
-   pg\_catalog.pg\_exttable.fmterrtbl
-   pg\_catalog.pg\_proc.proiswin
-   pg\_catalog.pg\_resgroupcapability.proposed
-   pg\_catalog.pg\_rewrite.ev\_attr
-   pg\_catalog.pg\_roles.rolcreaterexthdfs
-   pg\_catalog.pg\_roles.rolcreatewexthdfs
-   pg\_catalog.pg\_stat\_activity.current\_query
-   pg\_catalog.pg\_stat\_activity.procpid
-   pg\_catalog.pg\_stat\_replication.procpid
-   pg\_catalog.pg\_tablespace.spcfsoid
-   pg\_catalog.pg\_tablespace.spclocation
-   pg\_catalog.pg\_tablespace.spcmirlocations
-   pg\_catalog.pg\_tablespace.spcprilocations
-   pg\_catalog.pg\_trigger.tgconstrname
-   pg\_catalog.pg\_trigger.tgisconstraint

## <a id="dep_functions_procedures"></a>Deprecated Functions and Procedures 

The following list includes the Greenplum Database 6 deprecated functions and procedures.

-   gp\_toolkit.\_\_gp\_aocsseg
-   gp\_toolkit.\_\_gp\_aocsseg\_history
-   gp\_toolkit.\_\_gp\_aocsseg\_name
-   gp\_toolkit.\_\_gp\_aoseg\_history
-   gp\_toolkit.\_\_gp\_aoseg\_name
-   gp\_toolkit.\_\_gp\_aovisimap
-   gp\_toolkit.\_\_gp\_aovisimap\_entry
-   gp\_toolkit.\_\_gp\_aovisimap\_entry\_name
-   gp\_toolkit.\_\_gp\_aovisimap\_hidden\_info
-   gp\_toolkit.\_\_gp\_aovisimap\_hidden\_info\_name
-   gp\_toolkit.\_\_gp\_aovisimap\_name
-   gp\_toolkit.\_\_gp\_param\_local\_setting
-   gp\_toolkit.\_\_gp\_workfile\_entries\_f
-   gp\_toolkit.\_\_gp\_workfile\_mgr\_used\_diskspace\_f
-   information\_schema.\_pg\_keyissubset
-   information\_schema.\_pg\_underlying\_index
-   pg\_catalog.areajoinsel
-   pg\_catalog.array\_agg\_finalfn
-   pg\_catalog.bmcostestimate
-   pg\_catalog.bmgetmulti
-   pg\_catalog.bpchar\_pattern\_eq
-   pg\_catalog.bpchar\_pattern\_ne
-   pg\_catalog.btcostestimate
-   pg\_catalog.btgetmulti
-   pg\_catalog.btgpxlogloccmp
-   pg\_catalog.btname\_pattern\_cmp
-   pg\_catalog.btrescan
-   pg\_catalog.contjoinsel
-   pg\_catalog.cume\_dist\_final
-   pg\_catalog.cume\_dist\_prelim
-   pg\_catalog.dense\_rank\_immed
-   pg\_catalog.eqjoinsel
-   pg\_catalog.first\_value
-   pg\_catalog.first\_value\_any
-   pg\_catalog.first\_value\_bit
-   pg\_catalog.first\_value\_bool
-   pg\_catalog.first\_value\_box
-   pg\_catalog.first\_value\_bytea
-   pg\_catalog.first\_value\_char
-   pg\_catalog.first\_value\_cidr
-   pg\_catalog.first\_value\_circle
-   pg\_catalog.first\_value\_float4
-   pg\_catalog.first\_value\_float8
-   pg\_catalog.first\_value\_inet
-   pg\_catalog.first\_value\_int4
-   pg\_catalog.first\_value\_int8
-   pg\_catalog.first\_value\_interval
-   pg\_catalog.first\_value\_line
-   pg\_catalog.first\_value\_lseg
-   pg\_catalog.first\_value\_macaddr
-   pg\_catalog.first\_value\_money
-   pg\_catalog.first\_value\_name
-   pg\_catalog.first\_value\_numeric
-   pg\_catalog.first\_value\_oid
-   pg\_catalog.first\_value\_path
-   pg\_catalog.first\_value\_point
-   pg\_catalog.first\_value\_polygon
-   pg\_catalog.first\_value\_reltime
-   pg\_catalog.first\_value\_smallint
-   pg\_catalog.first\_value\_text
-   pg\_catalog.first\_value\_tid
-   pg\_catalog.first\_value\_time
-   pg\_catalog.first\_value\_timestamp
-   pg\_catalog.first\_value\_timestamptz
-   pg\_catalog.first\_value\_timetz
-   pg\_catalog.first\_value\_varbit
-   pg\_catalog.first\_value\_varchar
-   pg\_catalog.first\_value\_xid
-   pg\_catalog.flatfile\_update\_trigger
-   pg\_catalog.float4\_avg\_accum
-   pg\_catalog.float4\_avg\_decum
-   pg\_catalog.float4\_decum
-   pg\_catalog.float8\_amalg
-   pg\_catalog.float8\_avg
-   pg\_catalog.float8\_avg\_accum
-   pg\_catalog.float8\_avg\_amalg
-   pg\_catalog.float8\_avg\_decum
-   pg\_catalog.float8\_avg\_demalg
-   pg\_catalog.float8\_decum
-   pg\_catalog.float8\_demalg
-   pg\_catalog.float8\_regr\_amalg
-   pg\_catalog.get\_ao\_compression\_ratio
-   pg\_catalog.get\_ao\_distribution
-   pg\_catalog.ginarrayconsistent
-   pg\_catalog.gincostestimate
-   pg\_catalog.gin\_extract\_tsquery
-   pg\_catalog.gingetmulti
-   pg\_catalog.gingettuple
-   pg\_catalog.ginqueryarrayextract
-   pg\_catalog.ginrescan
-   pg\_catalog.gin\_tsquery\_consistent
-   pg\_catalog.gist\_box\_consistent
-   pg\_catalog.gist\_circle\_consistent
-   pg\_catalog.gistcostestimate
-   pg\_catalog.gistgetmulti
-   pg\_catalog.gist\_poly\_consistent
-   pg\_catalog.gistrescan
-   pg\_catalog.gp\_activate\_standby
-   pg\_catalog.gp\_add\_global\_sequence\_entry
-   pg\_catalog.gp\_add\_master\_standby
-   pg\_catalog.gp\_add\_persistent\_database\_node\_entry
-   pg\_catalog.gp\_add\_persistent\_filespace\_node\_entry
-   pg\_catalog.gp\_add\_persistent\_relation\_node\_entry
-   pg\_catalog.gp\_add\_persistent\_tablespace\_node\_entry
-   pg\_catalog.gp\_add\_relation\_node\_entry
-   pg\_catalog.gp\_add\_segment
-   pg\_catalog.gp\_add\_segment\_mirror
-   pg\_catalog.gp\_add\_segment\_persistent\_entries
-   pg\_catalog.gpaotidin
-   pg\_catalog.gpaotidout
-   pg\_catalog.gpaotidrecv
-   pg\_catalog.gpaotidsend
-   pg\_catalog.gp\_backup\_launch
-   pg\_catalog.gp\_changetracking\_log
-   pg\_catalog.gp\_dbspecific\_ptcat\_verification
-   pg\_catalog.gp\_delete\_global\_sequence\_entry
-   pg\_catalog.gp\_delete\_persistent\_database\_node\_entry
-   pg\_catalog.gp\_delete\_persistent\_filespace\_node\_entry
-   pg\_catalog.gp\_delete\_persistent\_relation\_node\_entry
-   pg\_catalog.gp\_delete\_persistent\_tablespace\_node\_entry
-   pg\_catalog.gp\_delete\_relation\_node\_entry
-   pg\_catalog.gp\_nondbspecific\_ptcat\_verification
-   pg\_catalog.gp\_persistent\_build\_all
-   pg\_catalog.gp\_persistent\_build\_db
-   pg\_catalog.gp\_persistent\_relation\_node\_check
-   pg\_catalog.gp\_persistent\_repair\_delete
-   pg\_catalog.gp\_persistent\_reset\_all
-   pg\_catalog.gp\_prep\_new\_segment
-   pg\_catalog.gp\_quicklz\_compress
-   pg\_catalog.gp\_quicklz\_constructor
-   pg\_catalog.gp\_quicklz\_decompress
-   pg\_catalog.gp\_quicklz\_destructor
-   pg\_catalog.gp\_quicklz\_validator
-   pg\_catalog.gp\_read\_backup\_file
-   pg\_catalog.gp\_remove\_segment\_persistent\_entries
-   pg\_catalog.gp\_restore\_launch
-   pg\_catalog.gp\_statistics\_estimate\_reltuples\_relpages\_oid
-   pg\_catalog.gp\_update\_ao\_master\_stats
-   pg\_catalog.gp\_update\_global\_sequence\_entry
-   pg\_catalog.gp\_update\_persistent\_database\_node\_entry
-   pg\_catalog.gp\_update\_persistent\_filespace\_node\_entry
-   pg\_catalog.gp\_update\_persistent\_relation\_node\_entry
-   pg\_catalog.gp\_update\_persistent\_tablespace\_node\_entry
-   pg\_catalog.gp\_update\_relation\_node\_entry
-   pg\_catalog.gp\_write\_backup\_file
-   pg\_catalog.gpxlogloceq
-   pg\_catalog.gpxloglocge
-   pg\_catalog.gpxloglocgt
-   pg\_catalog.gpxloglocin
-   pg\_catalog.gpxlogloclarger
-   pg\_catalog.gpxloglocle
-   pg\_catalog.gpxlogloclt
-   pg\_catalog.gpxloglocne
-   pg\_catalog.gpxloglocout
-   pg\_catalog.gpxloglocrecv
-   pg\_catalog.gpxloglocsend
-   pg\_catalog.gpxloglocsmaller
-   pg\_catalog.gtsquery\_consistent
-   pg\_catalog.gtsvector\_consistent
-   pg\_catalog.hashcostestimate
-   pg\_catalog.hashgetmulti
-   pg\_catalog.hashrescan
-   pg\_catalog.iclikejoinsel
-   pg\_catalog.icnlikejoinsel
-   pg\_catalog.icregexeqjoinsel
-   pg\_catalog.icregexnejoinsel
-   pg\_catalog.int24mod
-   pg\_catalog.int2\_accum
-   pg\_catalog.int2\_avg\_accum
-   pg\_catalog.int2\_avg\_decum
-   pg\_catalog.int2\_decum
-   pg\_catalog.int2\_invsum
-   pg\_catalog.int42mod
-   pg\_catalog.int4\_accum
-   pg\_catalog.int4\_avg\_accum
-   pg\_catalog.int4\_avg\_decum
-   pg\_catalog.int4\_decum
-   pg\_catalog.int4\_invsum
-   pg\_catalog.int8
-   pg\_catalog.int8\_accum
-   pg\_catalog.int8\_avg
-   pg\_catalog.int8\_avg\_accum
-   pg\_catalog.int8\_avg\_amalg
-   pg\_catalog.int8\_avg\_decum
-   pg\_catalog.int8\_avg\_demalg
-   pg\_catalog.int8\_decum
-   pg\_catalog.int8\_invsum
-   pg\_catalog.interval\_amalg
-   pg\_catalog.interval\_decum
-   pg\_catalog.interval\_demalg
-   pg\_catalog.json\_each\_text
-   pg\_catalog.json\_extract\_path\_op
-   pg\_catalog.json\_extract\_path\_text\_op
-   pg\_catalog.lag\_any
-   pg\_catalog.lag\_bit
-   pg\_catalog.lag\_bool
-   pg\_catalog.lag\_box
-   pg\_catalog.lag\_bytea
-   pg\_catalog.lag\_char
-   pg\_catalog.lag\_cidr
-   pg\_catalog.lag\_circle
-   pg\_catalog.lag\_float4
-   pg\_catalog.lag\_float8
-   pg\_catalog.lag\_inet
-   pg\_catalog.lag\_int4
-   pg\_catalog.lag\_int8
-   pg\_catalog.lag\_interval
-   pg\_catalog.lag\_line
-   pg\_catalog.lag\_lseg
-   pg\_catalog.lag\_macaddr
-   pg\_catalog.lag\_money
-   pg\_catalog.lag\_name
-   pg\_catalog.lag\_numeric
-   pg\_catalog.lag\_oid
-   pg\_catalog.lag\_path
-   pg\_catalog.lag\_point
-   pg\_catalog.lag\_polygon
-   pg\_catalog.lag\_reltime
-   pg\_catalog.lag\_smallint
-   pg\_catalog.lag\_text
-   pg\_catalog.lag\_tid
-   pg\_catalog.lag\_time
-   pg\_catalog.lag\_timestamp
-   pg\_catalog.lag\_timestamptz
-   pg\_catalog.lag\_timetz
-   pg\_catalog.lag\_varbit
-   pg\_catalog.lag\_varchar
-   pg\_catalog.lag\_xid
-   pg\_catalog.last\_value
-   pg\_catalog.last\_value\_any
-   pg\_catalog.last\_value\_bigint
-   pg\_catalog.last\_value\_bit
-   pg\_catalog.last\_value\_bool
-   pg\_catalog.last\_value\_box
-   pg\_catalog.last\_value\_bytea
-   pg\_catalog.last\_value\_char
-   pg\_catalog.last\_value\_cidr
-   pg\_catalog.last\_value\_circle
-   pg\_catalog.last\_value\_float4
-   pg\_catalog.last\_value\_float8
-   pg\_catalog.last\_value\_inet
-   pg\_catalog.last\_value\_int
-   pg\_catalog.last\_value\_interval
-   pg\_catalog.last\_value\_line
-   pg\_catalog.last\_value\_lseg
-   pg\_catalog.last\_value\_macaddr
-   pg\_catalog.last\_value\_money
-   pg\_catalog.last\_value\_name
-   pg\_catalog.last\_value\_numeric
-   pg\_catalog.last\_value\_oid
-   pg\_catalog.last\_value\_path
-   pg\_catalog.last\_value\_point
-   pg\_catalog.last\_value\_polygon
-   pg\_catalog.last\_value\_reltime
-   pg\_catalog.last\_value\_smallint
-   pg\_catalog.last\_value\_text
-   pg\_catalog.last\_value\_tid
-   pg\_catalog.last\_value\_time
-   pg\_catalog.last\_value\_timestamp
-   pg\_catalog.last\_value\_timestamptz
-   pg\_catalog.last\_value\_timetz
-   pg\_catalog.last\_value\_varbit
-   pg\_catalog.last\_value\_varchar
-   pg\_catalog.last\_value\_xid
-   pg\_catalog.lead\_any
-   pg\_catalog.lead\_bit
-   pg\_catalog.lead\_bool
-   pg\_catalog.lead\_box
-   pg\_catalog.lead\_bytea
-   pg\_catalog.lead\_char
-   pg\_catalog.lead\_cidr
-   pg\_catalog.lead\_circle
-   pg\_catalog.lead\_float4
-   pg\_catalog.lead\_float8
-   pg\_catalog.lead\_inet
-   pg\_catalog.lead\_int
-   pg\_catalog.lead\_int8
-   pg\_catalog.lead\_interval
-   pg\_catalog.lead\_lag\_frame\_maker
-   pg\_catalog.lead\_line
-   pg\_catalog.lead\_lseg
-   pg\_catalog.lead\_macaddr
-   pg\_catalog.lead\_money
-   pg\_catalog.lead\_name
-   pg\_catalog.lead\_numeric
-   pg\_catalog.lead\_oid
-   pg\_catalog.lead\_path
-   pg\_catalog.lead\_point
-   pg\_catalog.lead\_polygon
-   pg\_catalog.lead\_reltime
-   pg\_catalog.lead\_smallint
-   pg\_catalog.lead\_text
-   pg\_catalog.lead\_tid
-   pg\_catalog.lead\_time
-   pg\_catalog.lead\_timestamp
-   pg\_catalog.lead\_timestamptz
-   pg\_catalog.lead\_timetz
-   pg\_catalog.lead\_varbit
-   pg\_catalog.lead\_varchar
-   pg\_catalog.lead\_xid
-   pg\_catalog.likejoinsel
-   pg\_catalog.max
-   pg\_catalog.min
-   pg\_catalog.mod
-   pg\_catalog.name\_pattern\_eq
-   pg\_catalog.name\_pattern\_ge
-   pg\_catalog.name\_pattern\_gt
-   pg\_catalog.name\_pattern\_le
-   pg\_catalog.name\_pattern\_lt
-   pg\_catalog.name\_pattern\_ne
-   pg\_catalog.neqjoinsel
-   pg\_catalog.nlikejoinsel
-   pg\_catalog.ntile
-   pg\_catalog.ntile\_final
-   pg\_catalog.ntile\_prelim\_bigint
-   pg\_catalog.ntile\_prelim\_int
-   pg\_catalog.ntile\_prelim\_numeric
-   pg\_catalog.numeric\_accum
-   pg\_catalog.numeric\_amalg
-   pg\_catalog.numeric\_avg
-   pg\_catalog.numeric\_avg\_accum
-   pg\_catalog.numeric\_avg\_amalg
-   pg\_catalog.numeric\_avg\_decum
-   pg\_catalog.numeric\_avg\_demalg
-   pg\_catalog.numeric\_decum
-   pg\_catalog.numeric\_demalg
-   pg\_catalog.numeric\_stddev\_pop
-   pg\_catalog.numeric\_stddev\_samp
-   pg\_catalog.numeric\_var\_pop
-   pg\_catalog.numeric\_var\_samp
-   pg\_catalog.percent\_rank\_final
-   pg\_catalog.pg\_current\_xlog\_insert\_location
-   pg\_catalog.pg\_current\_xlog\_location
-   pg\_catalog.pg\_cursor
-   pg\_catalog.pg\_get\_expr
-   pg\_catalog.pg\_lock\_status
-   pg\_catalog.pg\_objname\_to\_oid
-   pg\_catalog.pg\_prepared\_statement
-   pg\_catalog.pg\_prepared\_xact
-   pg\_catalog.pg\_relation\_size
-   pg\_catalog.pg\_show\_all\_settings
-   pg\_catalog.pg\_start\_backup
-   pg\_catalog.pg\_stat\_get\_activity
-   pg\_catalog.pg\_stat\_get\_backend\_txn\_start
-   pg\_catalog.pg\_stat\_get\_wal\_senders
-   pg\_catalog.pg\_stop\_backup
-   pg\_catalog.pg\_switch\_xlog
-   pg\_catalog.pg\_total\_relation\_size
-   pg\_catalog.pg\_xlogfile\_name
-   pg\_catalog.pg\_xlogfile\_name\_offset
-   pg\_catalog.positionjoinsel
-   pg\_catalog.rank\_immed
-   pg\_catalog.regexeqjoinsel
-   pg\_catalog.regexnejoinsel
-   pg\_catalog.row\_number\_immed
-   pg\_catalog.scalargtjoinsel
-   pg\_catalog.scalarltjoinsel
-   pg\_catalog.string\_agg
-   pg\_catalog.string\_agg\_delim\_transfn
-   pg\_catalog.string\_agg\_transfn
-   pg\_catalog.text\_pattern\_eq
-   pg\_catalog.text\_pattern\_ne

## <a id="dep_types_domains_composite"></a>Deprecated Types, Domains, and Composite Types 

The following list includes the Greenplum Database 6 deprecated types, domains, and composite types.

-   gp\_toolkit.\_\_gp\_localid
-   gp\_toolkit.\_\_gp\_masterid
-   pg\_catalog.\_gpaotid
-   pg\_catalog.\_gpxlogloc
-   pg\_catalog.gp\_configuration
-   pg\_catalog.gp\_db\_interfaces
-   pg\_catalog.gp\_fault\_strategy
-   pg\_catalog.gp\_global\_sequence
-   pg\_catalog.gp\_interfaces
-   pg\_catalog.gp\_persistent\_database\_node
-   pg\_catalog.gp\_persistent\_filespace\_node
-   pg\_catalog.gp\_persistent\_relation\_node
-   pg\_catalog.gp\_persistent\_tablespace\_node
-   pg\_catalog.gp\_relation\_node
-   pg\_catalog.gpaotid
-   pg\_catalog.gpxlogloc
-   pg\_catalog.nb\_classification
-   pg\_catalog.pg\_autovacuum
-   pg\_catalog.pg\_filespace
-   pg\_catalog.pg\_filespace\_entry
-   pg\_catalog.pg\_listener
-   pg\_catalog.pg\_window

## <a id="dep_operators"></a>Deprecated Operators 

The following list includes the Greenplum Database 6 deprecated operators.

|oprname|oprcode|
|-------|-------|
|pg\_catalog.\#\>|json\_extract\_path\_op|
|pg\_catalog.\#\>\>|json\_extract\_path\_text\_op|
|pg\_catalog.%|int42mod|
|pg\_catalog.%|int24mod|
|pg\_catalog.<|gpxlogloclt|
|pg\_catalog.<=|gpxloglocle|
|pg\_catalog.<\>|gpxloglocne|
|pg\_catalog.=|gpxlogloceq|
|pg\_catalog.\>|gpxloglocgt|
|pg\_catalog.\>=|gpxloglocge|
|pg\_catalog.~<=~|name\_pattern\_le|
|pg\_catalog.~<\>~|name\_pattern\_ne|
|pg\_catalog.~<\>~|bpchar\_pattern\_ne|
|pg\_catalog.~<\>~|textne|
|pg\_catalog.~<~|name\_pattern\_lt|
|pg\_catalog.~=~|name\_pattern\_eq|
|pg\_catalog.~=~|bpchar\_pattern\_eq|
|pg\_catalog.~=~|texteq|
|pg\_catalog.~\>=~|name\_pattern\_ge|
|pg\_catalog.~\>~|name\_pattern\_gt|

