CREATE TABLE pg_class (
    relname name NOT NULL,
    relnamespace oid NOT NULL,
    reltype oid NOT NULL,
    relowner oid NOT NULL,
    relam oid NOT NULL,
    relfilenode oid NOT NULL,
    reltablespace oid NOT NULL,
    relpages integer NOT NULL,
    reltuples real NOT NULL,
    reltoastrelid oid NOT NULL,
    reltoastidxid oid NOT NULL,
    relhasindex boolean NOT NULL,
    relisshared boolean NOT NULL,
    relkind "char" NOT NULL,
    relstorage "char" NOT NULL,
    relnatts smallint NOT NULL,
    relchecks smallint NOT NULL,
    reltriggers smallint NOT NULL,
    relukeys smallint NOT NULL,
    relfkeys smallint NOT NULL,
    relrefs smallint NOT NULL,
    relhasoids boolean NOT NULL,
    relhaspkey boolean NOT NULL,
    relhasrules boolean NOT NULL,
    relhassubclass boolean NOT NULL,
    relfrozenxid xid NOT NULL,
    relacl aclitem[],
    reloptions text[],
    oid INTEGER  -- manually added since oid is hidden column in postgreSQL
);

CREATE TABLE pg_namespace (
    nspname name NOT NULL,
    nspowner oid NOT NULL,
    nspacl aclitem[],
    oid INTEGER -- manually added since oid is hidden column in postgreSQL
);

CREATE TABLE pg_partition_rule (
    paroid oid NOT NULL,
    parchildrelid oid NOT NULL,
    parparentrule oid NOT NULL,
    parname name NOT NULL,
    parisdefault boolean NOT NULL,
    parruleord smallint NOT NULL,
    parrangestartincl boolean NOT NULL,
    parrangeendincl boolean NOT NULL,
    parrangestart text,
    parrangeend text,
    parrangeevery text,
    parlistvalues text,
    parreloptions text[],
    partemplatespace oid
);

CREATE TABLE pg_partition (
    parrelid oid NOT NULL,
    parkind "char" NOT NULL,
    parlevel smallint NOT NULL,
    paristemplate boolean NOT NULL,
    parnatts smallint NOT NULL,
    paratts int2vector NOT NULL,
    parclass oidvector NOT NULL
);


INSERT INTO pg_class (relname , relnamespace , reltype , relowner , relam , relfilenode , reltablespace , relpages , reltuples , reltoastrelid , reltoastidxid , relhasindex , relisshared , relkind , relstorage , relnatts , relchecks , reltriggers , relukeys , relfkeys , relrefs , relhasoids , relhaspkey , relhasrules , relhassubclass , relfrozenxid , relacl , reloptions , oid)
              VALUES ( 'foo'  , 2200         , 16387   , 10       , 0     , 16384       , 0             , 0        , 0         , 0             , 0             , 'f'         , 'f'         , 'r'     , 'a'        , 1        , 0         , 0           , 0        , 0        , 0       , 'f'        , 'f'        , 'f'         , 'f'            , 705          , null   , null       , 16385), -- AO
                     ( 'foo2' , 2200         , 16387   , 10       , 0     , 16384       , 0             , 0        , 0         , 0             , 0             , 'f'         , 'f'         , 'r'     , 'c'        , 1        , 0         , 0           , 0        , 0        , 0       , 'f'        , 'f'        , 'f'         , 'f'            , 705          , null   , null       , 16386), -- CO
                     ( 'heap' , 2200         , 16387   , 10       , 0     , 16384       , 0             , 0        , 0         , 0             , 0             , 'f'         , 'f'         , 'r'     , 'h'        , 1        , 0         , 0           , 0        , 0        , 0       , 'f'        , 'f'        , 'f'         , 'f'            , 705          , null   , null       , 16387); -- heap


INSERT INTO pg_namespace (nspname  , nspowner , nspacl                             , oid )
              VALUES     ('public' , 10       , '{pivotal=UC/pivotal,=UC/pivotal}' , 2200);
