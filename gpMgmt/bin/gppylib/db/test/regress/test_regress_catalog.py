#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
# Unit Testing of catalog module.
#

import unittest



from gppylib import gplog
from gppylib.db import dbconn
from gppylib.db import catalog
from gppylib.db.test import skipIfDatabaseDown


logger=gplog.get_default_logger()


@skipIfDatabaseDown()
class catalogTestCase(unittest.TestCase):

    def setUp(self):
        self.dburl=dbconn.DbURL()
        self.conn = dbconn.connect(self.dburl, unsetSearchPath=False)


    def tearDown(self):
        self.conn.close()
        pass

#------------------------------- Mainline --------------------------------
if __name__ == '__main__':
    unittest.main()
