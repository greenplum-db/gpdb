from gppylib.operations import Operation
import psycopg2

"""
These objects needed for gppylib.operations.test.test_utils are pulled out of said file for
pickle/import/visibility reasons. See gppylib.operations.utils.RemoteOperation, #4.
"""

class TestOperation(Operation):
    def execute(self):
        return 1

class MyException(Exception): pass
class RaiseOperation(Operation):
    def execute(self):
        raise MyException()

# Exceptions cannot be nested. 
# They cannot be pickled for reasons inherent to python. See utils.py
class RaiseOperation_Nested(Operation):
    def execute(self):
        raise RaiseOperation_Nested.MyException2()
    class MyException2(Exception): pass

class RaiseOperation_Safe(Operation):
    def execute(self):
        raise ExceptionWithArgs(1, 2)

# This is the proper idiom for a pickle-able exception with arguments: http://bugs.python.org/issue1692335
class ExceptionWithArgs(Exception):
    def __init__(self, x, y):
        self.x, self.y = x, y
        Exception.__init__(self, x, y)

class ExceptionWithArgsUnsafe(Exception):
    def __init__(self, x, y):
        self.x, self.y = x, y

class RaiseOperation_Unpicklable(Operation):
    def execute(self):
        raise psycopg2.DatabaseError()
