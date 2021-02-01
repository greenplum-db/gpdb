import pytest
pytest.register_assert_rewrite('TEST')
from TEST_local_base import AnsFile
from TEST_local_base import read_diff
import os

def pytest_assertrepr_compare(config,op, left, right):

    #first: fname
    #second: output path
    if op == '==':
        diff = read_diff(os.path.splitext(left.path)[0], "")
        print(diff)
        output = ["query resulted in diff:"]
        return output