#!/usr/bin/env python3

import itertools

from collections import namedtuple
from gpcheckcat_modules.orphan_toast_table_issues import OrphanToastTableIssue, DoubleOrphanToastTableIssue, ReferenceOrphanToastTableIssue, DependencyOrphanToastTableIssue, MismatchOrphanToastTableIssue


OrphanedTable = namedtuple('OrphanedTable', 'oid catname')


class OrphanedToastTablesCheck:
    def __init__(self):
        self._issues = []

        # Normally, there's a "loop" between a table and its TOAST table:
        # - The table's reltoastrelid field in pg_class points to its TOAST table
        # - The TOAST table has an entry in pg_depend pointing to its table
        # This can break and orphan a TOAST table in one of three ways:
        # - The reltoastrelid entry is set to 0
        # - The reltoastrelid entry is set to a different oid value
        # - The pg_depend entry is missing
        # - The reltoastrelid entry is wrong *and* the pg_depend entry is missing
        # The following query attempts to "follow" the loop from pg_class to
        # pg_depend back to pg_class, and if the table oids don't match and/or
        # one is missing, the TOAST table is considered to be an orphan.
        # Note: Handles toast tables <pg_toast_temp_*> which is created/used by InitTempTableNamespace().
        self.orphaned_toast_tables_query = """
SELECT
    gp_segment_id AS content_id,
    toast_table_oid,
    toast_table_name,
    expected_table_oid,
    expected_table_name,
    dependent_table_oid,
    dependent_table_name,
    double_orphan_parent_oid,
    double_orphan_parent_name,
    double_orphan_parent_reltoastrelid,
    double_orphan_parent_toast_oid,
    double_orphan_parent_toast_name
FROM (
    SELECT
        tst.gp_segment_id,
        tst.oid AS toast_table_oid,
        tst.relname AS toast_table_name,
        tbl.oid AS expected_table_oid,
        tbl.relname AS expected_table_name,
        dep.refobjid AS dependent_table_oid,
        dep.refobjid::regclass::text AS dependent_table_name,
        dbl.oid AS double_orphan_parent_oid,
        dbl.relname AS double_orphan_parent_name,
        dbl.reltoastrelid AS double_orphan_parent_reltoastrelid,
        dbl_tst.oid AS double_orphan_parent_toast_oid,
        dbl_tst.relname AS double_orphan_parent_toast_name
    FROM
        pg_class tst
        LEFT JOIN pg_depend dep
            ON dep.classid = 'pg_class'::regclass
            AND tst.oid = dep.objid
        LEFT JOIN pg_class tbl ON tst.oid = tbl.reltoastrelid
        LEFT JOIN pg_class dbl
            ON REGEXP_REPLACE(tst.oid::regclass::text, 'pg_toast(_temp_\d+)?.pg_toast_', '')::int::regclass::oid = dbl.oid
        LEFT JOIN pg_class dbl_tst ON dbl.reltoastrelid = dbl_tst.oid
    WHERE tst.relkind='t'
        AND	(
            tbl.oid IS NULL
            OR refobjid IS NULL
            OR tbl.oid != dep.refobjid
        )
        AND (
            tbl.relnamespace IS NULL
            OR tbl.relnamespace != (SELECT oid FROM pg_namespace WHERE nspname = 'pg_catalog')
        )
    UNION ALL
    SELECT
        tst.gp_segment_id,
        tst.oid AS toast_table_oid,
        tst.relname AS toast_table_name,
        tbl.oid AS expected_table_oid,
        tbl.relname AS expected_table_name,
        dep.refobjid AS dependent_table_oid,
        dep.refobjid::regclass::text AS dependent_table_name,
        dbl.oid AS double_orphan_parent_oid,
        dbl.relname AS double_orphan_parent_name,
        dbl.reltoastrelid AS double_orphan_parent_reltoastrelid,
        dbl.reltoastrelid AS double_orphan_parent_toast_oid,
        dbl_tst.relname AS double_orphan_parent_toast_name
    FROM gp_dist_random('pg_class') tst
        LEFT JOIN gp_dist_random('pg_depend') dep
            ON dep.classid = 'pg_class'::regclass
            AND tst.oid = dep.objid
            AND tst.gp_segment_id = dep.gp_segment_id
        LEFT JOIN gp_dist_random('pg_class') tbl ON tst.oid = tbl.reltoastrelid AND tst.gp_segment_id = tbl.gp_segment_id
        LEFT JOIN gp_dist_random('pg_class') dbl
            ON REGEXP_REPLACE(tst.oid::regclass::text, 'pg_toast(_temp_\d+)?.pg_toast_', '')::int::regclass::oid = dbl.oid
            AND tst.gp_segment_id = dbl.gp_segment_id
        LEFT JOIN pg_class dbl_tst ON dbl.reltoastrelid = dbl_tst.oid AND tst.gp_segment_id = dbl_tst.gp_segment_id
    WHERE tst.relkind='t'
        AND (
            tbl.oid IS NULL
            OR refobjid IS NULL
            OR tbl.oid != dep.refobjid
        )
        AND (
            tbl.relnamespace IS NULL
            OR tbl.relnamespace != (SELECT oid FROM pg_namespace WHERE nspname = 'pg_catalog')
        )
    ORDER BY toast_table_oid, expected_table_oid, dependent_table_oid, gp_segment_id
) AS subquery
GROUP BY gp_segment_id, toast_table_oid, toast_table_name, expected_table_oid, expected_table_name, dependent_table_oid, dependent_table_name,
    double_orphan_parent_oid,
    double_orphan_parent_name,
    double_orphan_parent_reltoastrelid,
    double_orphan_parent_toast_oid,
    double_orphan_parent_toast_name;
"""

    def runCheck(self, db_connection):
        orphaned_toast_tables = db_connection.query(self.orphaned_toast_tables_query).dictresult()
        if len(orphaned_toast_tables) == 0:
            return True

        for row in orphaned_toast_tables:
            if row['expected_table_oid'] is None and row['dependent_table_oid'] is None:
                table = OrphanedTable(row['toast_table_oid'], row['toast_table_name'])
                issue_type = DoubleOrphanToastTableIssue

            elif row['expected_table_oid'] is None:
                table = OrphanedTable(row['dependent_table_oid'], row['dependent_table_name'])
                issue_type = ReferenceOrphanToastTableIssue

            elif row['dependent_table_oid'] is None:
                table = OrphanedTable(row['expected_table_oid'], row['expected_table_name'])
                issue_type = DependencyOrphanToastTableIssue

            else:
                table = OrphanedTable(row['dependent_table_oid'], row['dependent_table_name'])
                issue_type = MismatchOrphanToastTableIssue

            issue = issue_type(row, table)
            self._issues.append(issue)

        return False

    def iterate_issues(self):
        """
        Yields an OrphanToastTableIssue instance, and the set of segment IDs
        that exhibit that issue, for every issue found by the checker. For
        instance, if table 'my_table' has a double-orphan issue on segments 1
        and 2, and a dependency-orphan issue on segment -1, iterate_issues()
        will generate two tuples:

            ( DoubleOrphanToastTableIssue('my_table', ...),     { 1, 2 } )
            ( DependencyOrphanToastTableIssue('my_table', ...), { -1 }   )

        The OrphanToastTableIssue instance internally corresponds to a segment
        that exhibits the issue, but in cases where there is more than one
        segment with the problem, *which* segment it corresponds to is not
        defined. (For instance, in the above example, the
        DoubleOrphanToastTableIssue's row['content_id'] could be either 1 or 2.)

        It's assumed that this is not a problem (and if it becomes a problem,
        this implementation will need a rework).
        """
        # The hard part here is that our "model" has one issue per segment,
        # whereas we want to present one issue per (table, issue type) pair as
        # our "view".
        #
        # We use itertools.groupby() as the core. This requires us to sort our
        # list by (table, issue type) before performing the grouping, for the
        # same reason that you need to perform `sort` in a `sort | uniq`
        # pipeline.
        def sort_key(issue):
            return issue.table.oid

        def group_key(issue):
            return (issue.table.oid, type(issue))

        sorted_issues = sorted(self._issues, key=sort_key)

        for _, group in itertools.groupby(sorted_issues, group_key):
            issues = list(group)
            yield issues[0], { i.row['content_id'] for i in issues }

    def rows_of_type(self, issue_cls):
        return [ issue.row for issue in self._issues if isinstance(issue, issue_cls) ]

    def issue_types(self):
        return { type(issue) for issue in self._issues }

    def get_fix_text(self):
        log_output = ['\nORPHAN TOAST TABLE FIXES:',
                      '===================================================================']
        for issue_type in self.issue_types():
            log_output += issue_type.fix_text
        return '\n'.join(log_output)

    def add_repair_statements(self, segments):
        content_id_to_segment_map = self._get_content_id_to_segment_map(segments)

        for issue in self._issues:
            if issue.repair_script:
                content_id_to_segment_map[issue.row['content_id']]['repair_statements'].append(issue.repair_script)

        segments_with_repair_statements = [segment for segment in list(content_id_to_segment_map.values()) if len(segment['repair_statements']) > 0]
        for segment in segments_with_repair_statements:
            segment['repair_statements'] = ["SET allow_system_table_mods=true;"] + segment['repair_statements']

        return segments_with_repair_statements

    @staticmethod
    def _get_content_id_to_segment_map(segments):
        content_id_to_segment = {}
        for segment in list(segments.values()):
            segment['repair_statements'] = []
            content_id_to_segment[segment['content']] = segment

        return content_id_to_segment
