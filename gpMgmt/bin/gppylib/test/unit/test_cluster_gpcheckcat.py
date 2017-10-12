#!/usr/bin/env python

import imp
import os
import unittest
from gppylib.db import dbconn


class GpCheckCatColumnsTestCase(unittest.TestCase):
    def test_TableMainColumn_tablenames_exist(self):
        gpcheckcat_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpcheckcat")
        subject = imp.load_source('gpcheckcat', gpcheckcat_file)

        dburl = dbconn.DbURL(hostname=os.getenv('HOSTNAME', 'localhost'), port=os.getenv('PGPORT', 5432),
                             dbname=os.getenv('PGDATABASE', 'postgres'))
        conn = dbconn.connect(dburl)
        table_query = "select count(*) from pg_class where relname='{table_name}'"

        # 5.json has an incomplete list of catalog tables
        # src/backend/catalog has .h files for some catalog tables
        # gpdb-doc/dita/ref_guide/system_catalogs/ has .xml files for almost all catalog tables
        for key in subject.TableMainColumn.keys():
            cursor = dbconn.execSQL(conn, table_query.format(table_name=key))
            self.assertTrue(cursor.rowcount == 1, "%s not found in catalog dir" % key)


if __name__ == '__main__':
    unittest.main()
