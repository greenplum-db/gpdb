#!/usr/bin/env python

from gppylib.gplog import *
from gppylib.gpcatalog import *
import re

class ForeignKeyCheck:
    """
    PURPOSE: detect differences between foreign key and reference key values among catalogs
    """

    def __init__(self, db_connection, logger, shared_option, autoCast):
        self.db_connection = db_connection
        self.logger = logger
        self.shared_option = shared_option
        self.autoCast = autoCast
        self.query_filters = dict()
        self.query_filters['pg_appendonly.relid'] = "(relstorage='a' or relstorage='c')"
        self.query_filters['pg_attribute.attrelid'] = "true"
        self.query_filters["pg_index.indexrelid"] = "(relkind='i')"

    def runCheck(self, tables):
        foreign_key_issues = dict()
        for cat in sorted(tables):

            issues = self.checkTableForeignKey(cat)
            if issues:
                foreign_key_issues[cat.getTableName()] = issues

        return foreign_key_issues

    def checkTableForeignKey(self, cat):
        """
        return: list of issues in tuple (pkcatname, fields, results) format for the given catalog
        """
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
            castedFkey = [c + self.autoCast.get(coltypes[c], '') for c in fkeydef.getColumns()]
            fkeystr = ', '.join(castedFkey)
            pkeystr = ', '.join(fkeydef.getPKey())
            pkcatname = fkeydef.getPkeyTableName()
            catname_filter = '%s.%s' % (catname, fkeydef.getColumns()[0])

            #
            # The goal of this check is to validate foreign keys, which are associations between two tables.
            # We want to find a missing foreign key entry or a missing reference key entry when comparing
            # two tables that are supposed to have references to one another, either bidirectionally, where
            # both tables know about the other, or unidirectionally where one side of the comparison expects
            # the other to know about it.
            #
            # When both sides of a comparison demand a reference on the other side,
            # we can do a full join to look for missing entries. In cases where the association (foreign key) is
            # unidirectional, we validate only one side of the comparison,
            # using a left outer join to look for missing entries on only one side of the comparison.
            #
            # In the full-join case, we are explicitly talking about pg_class vs. catalog, and
            # we use a filter to select only the entries of interest in pg_class--the entries that
            # are foreign keys--using a very specific filtering condition, since the full join would otherwise contain
            # unwanted entries from pg_class.
            #
            can_use_full_join = self.query_filters.has_key(catname_filter) and pkcatname == 'pg_class'
            if can_use_full_join:
                qry = self.get_fk_query_full_join(catname, pkcatname, fkeystr, pkeystr,
                                                  pkey_aliases, cat1pkeys=cat1_pkeys_column_rename, filter=self.query_filters[catname_filter])
            else:
                qry = self.get_fk_query_left_join(catname, pkcatname, fkeystr, pkeystr, pkey_aliases, cat1_pkeys_column_rename)

            issue_list += self._validate_relation(catname, fkeystr, pkcatname, pkeystr, qry)

        return issue_list

    def _validate_relation(self, catname, fkeystr, pkcatname, pkeystr, qry):
        issue_list = []
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
