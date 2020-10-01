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
        if 'PGHOST' in os.environ:
            del os.environ['PGHOST']
            
        foo = DbURL()
        self.assertEqual(foo.pghost,"localhost")
        
        os.environ['PGHOST'] = "foo"
        foo = DbURL()
        self.assertEqual(foo.pghost,"foo")

        
        foo = DbURL(hostname='yoyoma')
        self.assertEqual(foo.pghost,'yoyoma')    

    def testConstructorPort(self):
        if 'PGPORT' in os.environ:
            del os.environ['PGPORT']
        
        foo = DbURL()
        self.assertEqual(foo.pgport,5432)
        
        os.environ['PGPORT'] = "6000"
        foo = DbURL()        
        self.assertEqual(foo.pgport,6000)
                
        foo = DbURL(port=6000)        
        self.assertEqual(foo.pgport,6000)
    
    
    def testConstructorDbname(self):
        if 'PGDATABASE' in os.environ:
            del os.environ['PGDATABASE']
        
        foo = DbURL()
        self.assertEqual(foo.pgdb,'template1')
        
        os.environ['PGDATABASE'] = 'testdb'
        foo = DbURL()
        self.assertEqual(foo.pgdb,'testdb')    
        
        foo = DbURL(dbname='yoyodb')
        self.assertEqual(foo.pgdb, 'yoyodb')
        
        
    def testConstructorUsername(self):
        if 'PGUSER' in os.environ:
            del os.environ['PGUSER']
        
        foo = DbURL()
        self.assertEqual(foo.pguser,os.environ['USER'])
        
        os.environ['PGUSER'] = 'testuser'
        foo = DbURL()
        self.assertEqual(foo.pguser,'testuser')    
        
        foo = DbURL(username='yoyouser')
        self.assertEqual(foo.pguser, 'yoyouser')
            
        
    def testConstructorPass(self):
        if 'PGPASSWORD' in os.environ:
            del os.environ['PGPASSWORD']

        foo = DbURL()
        self.assertEqual(foo.pgpass,None)

        os.environ['PGPASSWORD'] = 'testpass'
        foo = DbURL()
        self.assertEqual(foo.pgpass,'testpass')    

        foo = DbURL(password='yoyopass')
        self.assertEqual(foo.pgpass, 'yoyopass')

    
#----------------------- Main ----------------------
if __name__ == '__main__':
    unittest.main()
