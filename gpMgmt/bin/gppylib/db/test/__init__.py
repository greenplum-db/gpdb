import unittest
from gppylib.db.dbconn import connect, DbURL

def skipIfDatabaseDown():
    try:
        dbconn = connect(DbURL())
    except:
        return unittest.skip("database must be up")
    return lambda o: o             
