#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#

""" Unittesting for dbconn module
"""
import unittest

from gppylib.db.dbconn import *
 
class TestDbURL(unittest.TestCase):
    """UnitTest class for DbURL class"""
    
    def setUp(self):
        self._environ = dict(os.environ)

    def tearDown(self):
        os.environ = self._environ

    def testConstructorHost(self):
        old_pghost = None
        if 'PGHOST' in os.environ:
            old_pghost = os.environ['PGHOST']
            del os.environ['PGHOST']
            
        foo = DbURL()
        self.assertEqual(foo.pghost,"localhost")

        os.environ['PGHOST'] = "foo"
        foo = DbURL()
        self.assertEqual(foo.pghost,"foo")

        foo = DbURL(hostname='yoyoma')
        self.assertEqual(foo.pghost,'yoyoma')    

        del os.environ['PGHOST']
        if old_pghost is not None:	
            os.environ['PGHOST'] = old_pghost


    def testConstructorPort(self):
        old_pgport = None
        if 'PGPORT' in os.environ:
            old_pgport = os.environ['PGPORT']
            del os.environ['PGPORT']

        foo = DbURL()
        self.assertEqual(foo.pgport,5432)

        os.environ['PGPORT'] = "6000"
        foo = DbURL()        
        self.assertEqual(foo.pgport,6000)

        foo = DbURL(port=6000)        
        self.assertEqual(foo.pgport,6000)

        del os.environ['PGPORT']
        if old_pgport is not None:
            os.environ['PGPORT'] = old_pgport


    def testConstructorDbname(self):
        old_pgdatabase = None
        if 'PGDATABASE' in os.environ:
            old_pgdatabase = os.environ['PGDATABASE']
            del os.environ['PGDATABASE']

        foo = DbURL()
        self.assertEqual(foo.pgdb,'template1')
        
        os.environ['PGDATABASE'] = 'testdb'
        foo = DbURL()
        self.assertEqual(foo.pgdb,'testdb')    

        foo = DbURL(dbname='yoyodb')
        self.assertEqual(foo.pgdb, 'yoyodb')

        del os.environ['PGDATABASE']
        if old_pgdatabase is not None:
            os.environ['PGDATABASE'] = old_pgdatabase
       
        
    def testConstructorUsername(self):
        old_pguser = None
        if 'PGUSER' in os.environ:
            old_pguser = os.environ['PGUSER']
            del os.environ['PGUSER']

        foo = DbURL()
        self.assertEqual(foo.pguser,os.environ['USER'])

        os.environ['PGUSER'] = 'testuser'
        foo = DbURL()
        self.assertEqual(foo.pguser,'testuser')    

        foo = DbURL(username='yoyouser')
        self.assertEqual(foo.pguser, 'yoyouser')

        del os.environ['PGUSER']
        if old_pguser is not None:
            os.environ['PGUSER'] = old_pguser 

    def testConstructorPass(self):
        old_pass = None
        if 'PGPASSWORD' in os.environ:
            old_pass = os.environ['PGPASSWORD']
            del os.environ['PGPASSWORD']

        foo = DbURL()
        self.assertEqual(foo.pgpass,None)

        os.environ['PGPASSWORD'] = 'testpass'
        foo = DbURL()
        self.assertEqual(foo.pgpass,'testpass')    

        foo = DbURL(password='yoyopass')
        self.assertEqual(foo.pgpass, 'yoyopass')

        del os.environ['PGPASSWORD']
        if old_pass is not None:
            os.environ['PGPASSWORD'] = old_pass


#----------------------- Main ----------------------
if __name__ == '__main__':
    unittest.main()
