#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#

""" Unittesting for pygres module
"""
import logging
import unittest


from pygresql import pg
from pygresql import pgdb

from gppylib import gplog
from gppylib.db.dbconn import *
from gppylib.db.test import skipIfDatabaseDown

logger=gplog.get_default_logger()
gplog.enable_verbose_logging()


#TODO:  incomplete list.
catalog_names = ['pg_aggregate','pg_amop','pg_attrdef','pg_auth_members',
                 'pg_autovacuum','pg_class','pg_conversion','pg_database','pg_description',
                 'pg_group','pg_indexes','pg_language',
                 'pg_max_external_files','pg_opclass','pg_partition']

datatypes = {'oid':True , 'int2':True, 'regproc':True, 'text':True, 'bool':True,
             'int4':True, 'float4':True, 'name':True, 'char':True, 'xid':True,
             '_aclitem':True, '_text':True, '_oid':True, 'int8':True,
             'int2vector':True,'oidvector':True}


@skipIfDatabaseDown()
class pygresqlTestCase(unittest.TestCase):

    
    def test_connect(self):
        dburl = DbURL()        
        logger.info("YO")
        db = pg.DB(dbname=dburl.pgdb)
        q = db.query("SELECT 1")
        logger.info(q.getresult())
    
    def test_DBI_connect(self):
        
        logger.info("test_dbi_connect")
        dburl = DbURL()        
        db = pgdb.connect(dburl.pguser,host=dburl.pghost,database=dburl.pgdb)
        curs = db.cursor()        
        curs.execute("COMMIT")
        curs.execute("BEGIN")
        curs.execute("DROP TABLE IF EXISTS jge ")
        curs.execute("CREATE TABLE jge ( a int )")
        curs.execute("DROP TABLE jge")
        db.commit()  

    def test_utilitymode(self):
        db = self.setupConnection("test_utilitymode")
        curs=db.cursor()  
        curs.execute("show gp_role")
        logger.info(curs.fetchall())
        curs.close()
        db.close()

    def test_pgcatalog_selects(self):
        db = self.setupConnection("test_pgcatalog_selects")
        curs=db.cursor()  
      
        
        for table in catalog_names:
            sql = "SELECT * FROM %s LIMIT 1" % table
            curs.execute(sql)
            rows=curs.fetchall()
            self.verifyResults(rows,curs.description)
                            
        curs.close()
        db.close()


    def test_nulls(self):
        db = self.setupConnection("test_nulls")        
        curs=db.cursor()
        
        curs.execute("BEGIN")
        curs.execute("CREATE TABLE test ( a int, b text )")
        curs.execute("INSERT INTO test VALUES (null,null)")
        curs.execute("SELECT * FROM test")
        row = curs.fetchone()
        self.assertTrue(row[0] is None)
        self.assertTrue(row[1] is None)
        
        
    def test_createdb(self):
        db = None
        try:
            db = self.setupConnection("test_createdb")
            curs=db.cursor()        
        
            curs.execute("COMMIT")
            curs.execute("CREATE DATABASE test")
            curs.execute("DROP DATABASE test")
        finally:
            if db is not None:
                db.close()
            

    def test_vacuum(self):
        db = self.setupConnection("test_vacuumdb")
        curs = db.cursor()
        
        curs.execute("COMMIT")
        curs.execute("VACUUM FULL pg_catalog.pg_class")
        db.close()
        
        
    #------------------------------- non-test helper --------------------------------
    def setupConnection(self,name):
        logger.info(name)
        dburl = DbURL()
        dsn=str(dburl) + "::"
        db = pgdb.connect(dsn=dsn)
        return db
        
        
    def verifyResults(self,rows,description):
        
        for col in description:
            colname = col[0]
            datatype = col[1]                        
            self.assertTrue(datatypes[datatype])
        
        for row in rows:
            for col in row:
                foo = "" + str(col)
            
            
        

#------------------------------- Mainline --------------------------------
if __name__ == '__main__':
    unittest.main()

