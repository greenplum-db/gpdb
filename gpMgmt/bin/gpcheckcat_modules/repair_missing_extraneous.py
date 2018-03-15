#!/usr/bin/env python
from gppylib.utils import escapeDoubleQuoteInSQLString

class RepairMissingExtraneous:

    def __init__(self, catalog_table_obj,  issues, pk_name):
        self.catalog_table_obj = catalog_table_obj
        catalog_name = self.catalog_table_obj.getTableName()
        self._escaped_catalog_name = escapeDoubleQuoteInSQLString(catalog_name)
        self._issues = issues
        self._pk_name = pk_name

    def _generate_delete_sql_for_oid(self, pk_name, oids):
        escaped_pk_name = escapeDoubleQuoteInSQLString(pk_name)
        delete_sql = 'BEGIN;set allow_system_table_mods="dml";delete from {0} where {1} in ({2});COMMIT;'
        return delete_sql.format(self._escaped_catalog_name, escaped_pk_name, ','.join(str(oid) for oid in oids))

    def _generate_delete_sql_for_pkeys(self, pk_names):
        delete_sql = 'BEGIN;set allow_system_table_mods="dml";'
        for issue in self._issues:
            delete_issue_sql = 'delete from {0} where '
            for pk, issue_col in zip(pk_names, issue):
                operator = " and " if pk != pk_names[-1] else ";"
                add_on = "{pk} = '{col}'{operator}".format(pk=pk,
                                                           col=str(issue_col),
                                                           operator=operator)
                delete_issue_sql += add_on
            delete_issue_sql = delete_issue_sql.format(self._escaped_catalog_name)
            delete_sql += delete_issue_sql
        delete_sql += 'COMMIT;'
        return delete_sql

    def get_delete_sql(self, oids):
        if self.catalog_table_obj.tableHasConsistentOids():
            pk_name = 'oid' if self._pk_name is None else self._pk_name
            return self._generate_delete_sql_for_oid(pk_name=pk_name, oids=oids)

        pk_names = tuple(self.catalog_table_obj.getPrimaryKey())
        return self._generate_delete_sql_for_pkeys(pk_names=pk_names)

    def get_segment_to_oid_mapping(self, all_seg_ids):
        if not self._issues:
            return

        #   issues look like this
        #   [(49401, "extra", '{1,2}'),
        #    (49401, "extra", '{1,2}')]
        #               OR
        #   [(49401, 'cmax', "extra", '{1,2}'),
        #    (49401, 'cmax', "extra", '{1,2}')]

        all_seg_ids = set([str(seg_id) for seg_id in all_seg_ids])
        oids_to_segment_mapping = {}
        for issue in self._issues:

            oid = issue[0]

            issue_type = issue[-2]
            seg_ids = issue[-1].strip('{}').split(',')

            # if an oid is missing from a segment(s) , then it is considered to be extra
            # on all the other segments/master
            if issue_type == "missing":
                seg_ids = all_seg_ids - set(seg_ids)

            for seg_id in seg_ids:
                seg_id = int(seg_id)

                if not oids_to_segment_mapping.has_key(seg_id):
                    oids_to_segment_mapping[seg_id] = set()

                oids_to_segment_mapping[seg_id].add(oid)

        return oids_to_segment_mapping
