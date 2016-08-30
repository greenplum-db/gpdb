#!/usr/bin/env python

from gppylib.gplog import *
from gppylib.gpcatalog import *
import re

class ForeignKeyCheck:

    def __init__(self, db_connection, logger, shared_option):
        self.shared_option = shared_option
        self.db_connection = db_connection
        self.logger = logger
        self.query_filters = dict()
        self.query_filters['pg_appendonly.relid'] = "(relstorage='a' or relstorage='c')"
        self.query_filters['pg_attribute.attrelid'] = "true"
        self.query_filters["pg_index.indexrelid"] = "(relkind='i')"
        # NOTE: due to pg_class limitations where some flags (relhaspkey) are maintained lazily (updates are not enforced).
        # there is no good way to create a one to one mapping between pg_class
        # and pg_constraint. So, commenting it out for now
#        self.query_filters['pg_constraint.conrelid'] = "((relchecks>0 or relhaspkey='t') and relkind = 'r')"
#        self.query_filters['gp_distribution_policy.localoid'] = """(relnamespace not in(select oid from pg_namespace where nspname ~ 'pg_')
#                                                           and relnamespace not in(select oid from pg_namespace where nspname ~ 'gp_')
#                                                           and relnamespace!=(select oid from pg_namespace where nspname='information_schema')
#                                                           and relkind='r' and (relstorage='a' or relstorage='h' or relstorage='c'))"""

    def runCheck(self, tables, autoCast):
        foreign_key_issues = dict()
        for cat in sorted(tables):

            issues = self.checkTableForeignKey(cat, autoCast)
            if issues:
                foreign_key_issues[cat.getTableName()] = issues

        return foreign_key_issues

    def checkTableForeignKey(self, cat, autoCast):
        catname = cat.getTableName()
        fkeylist = cat.getForeignKeys()
        isShared = cat.isShared()
        pkeylist = cat.getPrimaryKey()
        coltypes = cat.getTableColtypes()

        # skip tables without fkey
        if len(fkeylist) <= 0:
            return
        if len(cat.getPrimaryKey()) <= 0:
            return

        # skip these master-only tables
        skipped_masteronly = ['gp_relation_node', 'pg_description',
                              'pg_shdescription', 'pg_stat_last_operation',
                              'pg_stat_last_shoperation', 'pg_statistic']

        if catname in skipped_masteronly:
            return

        # skip shared/non-shared tables
        if self.shared_option:
            if re.match("none", self.shared_option, re.I) and isShared:
                return
            if re.match("only", self.shared_option, re.I) and not isShared:
                return

        # primary key lists
        # cat1.objid as gp_fastsequence_objid
        cat1_pkeys_column_rename = []
        pkey_aliases = []

        # build array of catalog primary keys (with aliases) and
        # primary key alias list
        for pk in pkeylist:
            cat1_pkeys_column_rename.append('cat1.' + pk + ' as %s_%s' % (catname, pk))
            pkey_aliases.append('%s_%s' % (catname, pk))

        self.logger.info('Building %d queries to check FK constraint on table %s' % (len(fkeylist), catname))
        issue_list = list()
        for fkeydef in fkeylist:
            castedFkey = [c + autoCast.get(coltypes[c], '') for c in fkeydef.getColumns()]
            fkeystr = ', '.join(castedFkey)
            pkeystr = ', '.join(fkeydef.getPKey())
            pkcatname = fkeydef.getPkeyTableName()

            # This has to be a result of 1 because foreign key will be a one to
            # one mapping
            catname_filter = '%s.%s' % (catname, fkeydef.getColumns()[0])

            if self.query_filters.has_key(catname_filter) and pkcatname == 'pg_class':
                qry = self.get_fk_query_full_join(catname, pkcatname, fkeystr, pkeystr,
                                                  pkey_aliases, cat1pkeys=cat1_pkeys_column_rename, filter=self.query_filters[catname_filter])
            else:
                qry = self.get_fk_query_left_join(catname, pkcatname, fkeystr, pkeystr, pkey_aliases, cat1_pkeys_column_rename)

            # Execute the query
            try:
                curs = self.db_connection.query(qry)
                nrows = curs.ntuples()

                if nrows == 0:
                    self.logger.info('[OK] Foreign key check for %s(%s) referencing %s(%s)' %
                                (catname, fkeystr, pkcatname, pkeystr))
                else:
                    self.logger.info('[FAIL] Foreign key check for %s(%s) referencing %s(%s)' %
                                (catname, fkeystr, pkcatname, pkeystr))
                    self.logger.error('  %s has %d issue(s): entry has NULL reference of %s(%s)' %
                                 (catname, nrows, pkcatname, pkeystr))

                    fields = curs.listfields()
                    log_literal(self.logger, logging.ERROR, "    " + " | ".join(fields))
                    for row in curs.getresult():
                        log_literal(self.logger, logging.ERROR, "    " + " | ".join(map(str, row)))
                    results = curs.getresult()
                    issue_list.append((pkcatname, fields, results))

            except Exception as e:
                    err_msg = '[ERROR] executing: Foreign key check for catalog table {0}. Query : \n {1}\n'.format(catname, qry)
                    err_msg += str(e)
                    raise Exception(err_msg)

        return issue_list

    # -------------------------------------------------------------------------------
    def get_fk_query_left_join(self, catname, pkcatname, fkeystr, pkeystr, pkeys, cat1pkeys):
        qry = """
              SELECT {primary_key_alias}, missing_catalog, present_key, {cat2_dot_pk},
                     array_agg(gp_segment_id order by gp_segment_id) as segids
              FROM (
                    SELECT (case when cat1.{FK1} is not NULL then '{CATALOG2}' when cat2.{PK2} is not NULL then '{CATALOG1}' end) as missing_catalog,
                    (case when cat1.{FK1} is not NULL then '{FK1}' when cat2.{PK2} is not NULL then '{PK2}' end) as present_key,
                    cat1.gp_segment_id, {cat1_dot_pk}, cat1.{FK1} as {cat2_dot_pk}
                    FROM
                        gp_dist_random('{CATALOG1}') cat1 LEFT OUTER JOIN
                        gp_dist_random('{CATALOG2}') cat2
                        ON (cat1.gp_segment_id = cat2.gp_segment_id AND
                            cat1.{FK1} = cat2.{PK2} )
                    WHERE cat2.{PK2} is NULL
                      AND cat1.{FK1} != 0
                    UNION ALL
                    SELECT (case when cat1.{FK1} is not NULL then '{CATALOG2}' when cat2.{PK2} is not NULL then '{CATALOG1}' end) as missing_catalog,
                    (case when cat1.{FK1} is not NULL then '{FK1}' when cat2.{PK2} is not NULL then '{PK2}' end) as present_key,
                    -1 as gp_segment_id, {cat1_dot_pk}, cat1.{FK1} as {cat2_dot_pk}
                    FROM
                        {CATALOG1} cat1 LEFT OUTER JOIN
                        {CATALOG2} cat2
                        ON (cat1.gp_segment_id = cat2.gp_segment_id AND
                            cat1.{FK1} = cat2.{PK2} )
                    WHERE cat2.{PK2} is NULL
                      AND cat1.{FK1} != 0
                    ORDER BY {primary_key_alias}, gp_segment_id
              ) allresults
              GROUP BY {primary_key_alias}, {cat2_dot_pk}, missing_catalog, present_key
              """.format(FK1=fkeystr,
                         PK2=pkeystr,
                         CATALOG1=catname,
                         CATALOG2=pkcatname,
                         cat1_dot_pk=', '.join(cat1pkeys),
                         cat2_dot_pk='%s_%s' % (pkcatname, pkeystr),
                         primary_key_alias=', '.join(pkeys))
        return qry


    def get_fk_query_full_join(self, catname, pkcatname, fkeystr, pkeystr, pkeys, cat1pkeys, filter):
        qry = """
              SELECT {primary_key_alias}, missing_catalog, present_key, {cat2_dot_pk},
                     array_agg(gp_segment_id order by gp_segment_id) as segids
              FROM (
                    SELECT (case when cat1.{FK1} is not NULL then '{CATALOG2}' when cat2.{PK2} is not NULL then '{CATALOG1}' end) as missing_catalog,
                    (case when cat1.{FK1} is not NULL then '{FK1}' when cat2.{PK2} is not NULL then '{PK2}' end) as present_key,
                    COALESCE(cat1.gp_segment_id,cat2.gp_segment_id) as gp_segment_id , {cat1_dot_pk}, COALESCE(cat1.{FK1}, cat2.{PK2}) as {cat2_dot_pk}
                    FROM
                        gp_dist_random('{CATALOG1}') cat1 FULL OUTER JOIN
                        gp_dist_random('{CATALOG2}') cat2
                        ON (cat1.gp_segment_id = cat2.gp_segment_id AND
                            cat1.{FK1} = cat2.{PK2} )
                    WHERE (cat2.{PK2} is NULL or cat1.{FK1} is NULL)
                    AND {filter}
                    UNION ALL
                    SELECT (case when cat1.{FK1} is not NULL then '{CATALOG2}' when cat2.{PK2} is not NULL then '{CATALOG1}' end) as missing_catalog,
                    (case when cat1.{FK1} is not NULL then '{FK1}' when cat2.{PK2} is not NULL then '{PK2}' end) as present_key,
                    -1, {cat1_dot_pk}, COALESCE(cat1.{FK1}, cat2.{PK2}) as {cat2_dot_pk}
                    FROM
                        {CATALOG1} cat1 FULL OUTER JOIN
                        {CATALOG2} cat2
                        ON (cat1.gp_segment_id = cat2.gp_segment_id AND
                            cat1.{FK1} = cat2.{PK2} )
                    WHERE (cat2.{PK2} is NULL or cat1.{FK1} is NULL)
                    AND {filter}
                    ORDER BY {primary_key_alias}, gp_segment_id
              ) allresults
              GROUP BY {primary_key_alias}, {cat2_dot_pk}, missing_catalog, present_key
              """.format(FK1=fkeystr,
                         PK2=pkeystr,
                         CATALOG1=catname,
                         CATALOG2=pkcatname,
                         cat1_dot_pk=', '.join(cat1pkeys),
                         cat2_dot_pk='%s_%s' % (pkcatname, pkeystr),
                         primary_key_alias=', '.join(pkeys),
                         filter=filter)
        return qry
