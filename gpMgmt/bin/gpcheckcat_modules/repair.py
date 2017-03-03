#!/usr/bin/env python
"""
Purpose : Creates the repair dir and the corresponding sql/bash scripts for
          repairing some of the catalog issues(see the list below) reported by gpcheckcat.
          Not responsible for generating the repair contents.

Creates repair for the following gpcheckcat checks
       * missing_extraneous
       * owner
       * part_integrity
       * distribution_policy
"""

import os
import stat
from datetime import datetime
from gpcheckcat_modules.repair_missing_extraneous import RepairMissingExtraneous


class Repair:

    TIMESTAMP = datetime.now().strftime("%Y%m%d%H%M%S")

    def __init__(self, context=None, issue_type=None, desc=None):
        self._context = context
        self._issue_type = issue_type
        self._desc = desc
        self._repair_script = 'runsql_%s.sh' % self.TIMESTAMP

    def create_repair(self, sql_repair_contents):
        repair_dir = self.create_repair_dir()

        sql_file_name = self.__create_sql_file_in_repair_dir(repair_dir, sql_repair_contents)

        self.__create_bash_script_in_repair_dir(repair_dir, sql_file_name, segment_id=-1)

        return repair_dir

    def create_repair_for_extra_missing(self, catalog_table_obj, issues, pk_name):

        catalog_name = catalog_table_obj.getTableName()
        extra_missing_repair_obj = RepairMissingExtraneous(catalog_table_obj=catalog_table_obj,
                                                           issues=issues,
                                                           pk_name=pk_name)
        repair_dir = self.create_repair_dir()
        all_seg_ids = self._context.report_cfg.keys()
        segment_to_oid_map = extra_missing_repair_obj.get_segment_to_oid_mapping(all_seg_ids)

        for segment_id, oids in segment_to_oid_map.iteritems():
            sql_content = extra_missing_repair_obj.get_delete_sql(oids)
            self.__create_bash_script_in_repair_dir(repair_dir, sql_content,
                                                    segment_id=segment_id, catalog_name=catalog_name)

        return repair_dir

    def create_repair_dir(self):
        repair_dir = self._context.opt['-g']
        if not os.path.exists(repair_dir):
            os.mkdir(repair_dir)

        return repair_dir

    def append_content_to_bash_script(self, repair_dir_path, script, catalog_name=None):
        bash_file_path = self.__get_bash_filepath(repair_dir_path, catalog_name)
        if not os.path.isfile(bash_file_path):
            script = '#!/bin/bash\ncd $(dirname $0)\n' + script

        with open(bash_file_path, 'a') as bash_file:
            bash_file.write(script)

        os.chmod(bash_file_path, stat.S_IXUSR | stat.S_IRUSR | stat.S_IWUSR)

    def __create_sql_file_in_repair_dir(self, repair_dir, sql_repair_contents):
        sql_file_name = self.__get_sql_filename()
        sql_file_path = os.path.join(repair_dir, sql_file_name)

        with open(sql_file_path, 'w') as sql_file:
            for content in sql_repair_contents:
                sql_file.write(content + "\n")

        return sql_file_name

    def __create_bash_script_in_repair_dir(self, repair_dir, sql, segment_id, catalog_name=None):
        if not sql:
            return

        bash_script_content = self.__get_bash_script_content(sql, segment_id)
        self.append_content_to_bash_script(repair_dir, bash_script_content, catalog_name)

    def __get_sql_filename(self):
        return '%s_%s_%s.sql' % (self._context.dbname, self._issue_type, self.TIMESTAMP)

    def __get_bash_filepath(self, repair_dir, catalog_name):
        bash_file_name = self._repair_script

        if self._issue_type and catalog_name:
            bash_file_name = 'run_{0}_{1}_{2}_{3}.sh'.format(self._context.dbname, self._issue_type,
                                                             catalog_name, self.TIMESTAMP)

        return os.path.join(repair_dir, bash_file_name)

    def __get_bash_script_content(self, filename, segment_id):
        c = self._context.report_cfg[segment_id]
        out_filename = "{0}_{1}_{2}".format(self._context.dbname, self._issue_type, self.TIMESTAMP)
        bash_script_content = '\necho "{0}"\n'.format(self._desc)
        bash_script_content += self.__get_psql_command(segment_id).format(hostname=c['hostname'],
                                                                          port=c['port'], sql=filename,
                                                                          dbname=self._context.dbname,
                                                                          filename=out_filename)
        return bash_script_content

    def __get_psql_command(self, segment_id):
        """
        cases to consider
        self._issue_type : extra/missing vs everything else(policies, owner, constraint)
        master vs segment
        """
        psql_cmd = ""

        if segment_id > -1:  # segment node
            psql_cmd += 'PGOPTIONS=\'-c gp_session_role=utility\' '

        psql_cmd += 'psql -X -a -h {hostname} -p {port} '

        if self._issue_type == 'extra' or self._issue_type == 'missing':
            psql_cmd += '-c '
        else:
            psql_cmd += '-f '

        psql_cmd += '"{sql}" "{dbname}" >> {filename}.out 2>&1\n'

        return psql_cmd


