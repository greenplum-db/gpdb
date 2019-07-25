import unittest
import os
import pwd
from gppylib.db.dbconn import connect, DbURL

def skipIfRunningOnCI():
    if pwd.getpwuid( os.getuid() )[ 0 ] == 'travis':
        return unittest.skip("running in Travis CI")

def skipIfDatabaseDown():
    try:
        dbconn = connect(DbURL())
    except:
        return unittest.skip("database must be up")
    return lambda o: o             
