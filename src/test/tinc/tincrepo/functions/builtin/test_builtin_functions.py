from functions.builtin import BuiltinFunctionTestCase

class BuiltinFunctionTests(BuiltinFunctionTestCase):
    '''
    this test is designed to compare stats between old planner vs ORCA
    @tags ORCA
    '''
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    out_dir = 'output/'
