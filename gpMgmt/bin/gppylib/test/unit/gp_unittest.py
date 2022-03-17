import unittest

from mock import MagicMock, Mock

class Contains(str):
    """
    This class is used as a way to assert for partial match in unittests
    """
    def __eq__(self, other):
        return self in other

class GpTestCase(unittest.TestCase):
    def __init__(self, methodName='runTest'):
        super(GpTestCase, self).__init__(methodName)
        self.patches = []
        self.mock_objs = []
        self.__class__.apply_patches_counter = 0
        self.__class__.tear_down_counter = 0

    def apply_patches(self, patches):
        if self.patches:
            raise Exception('Test class is already patched!')
        self.patches = patches
        self.mock_objs = [p.start() for p in self.patches]
        self.__class__.apply_patches_counter += 1

    def get_mock_from_apply_patch(self, mock_name):
        """ Return None if there is no existing object
            mock name prints out the last "namespace"
            for example "os.path.exists", mock_name will be "exists"
        """
        for mock_obj in self.mock_objs:
            if isinstance(mock_obj, Mock) or isinstance(mock_obj, MagicMock):
                if mock_name == mock_obj._mock_name:
                    return mock_obj
        return None

    # if you have a tearDown() in your test class,
    # be sure to call this using super(<child class name>, self).tearDown()
    def tearDown(self):
        [p.stop() for p in self.patches]
        self.mock_objs = []
        self.__class__.tear_down_counter += 1

    @classmethod
    def setUpClass(cls):
        cls.apply_patches_counter = 0
        cls.tear_down_counter = 0

    @classmethod
    def tearDownClass(cls):
        if cls.apply_patches_counter > 0 and cls.apply_patches_counter != cls.tear_down_counter:
            raise Exception("Unequal call for apply patches: %s, teardown: %s. "
                            "You probably need to add a super(<child class>, "
                            "self).tearDown() in your tearDown()" % (cls.apply_patches_counter,
                                                                     cls.tear_down_counter))


def add_setup(setup=None, teardown=None):
    """decorate test functions to add additional setup/teardown contexts"""
    def decorate_function(test):
        def wrapper(self):
            if setup:
                setup(self)
            test(self)
            if teardown:
                teardown(self)
        return wrapper
    return decorate_function


# hide unittest dependencies here
def run_tests():
    unittest.main(verbosity=2, buffer=True)

skip = unittest.skip


class FakeCursor:
    def __init__(self, my_list=None):
        self.list = []
        self.rowcount = 0
        if my_list:
            self.set_result_for_testing(my_list)

    def __iter__(self):
        return iter(self.list)

    def close(self):
        pass

    def fetchall(self):
        return self.list

    def set_result_for_testing(self, result_list):
        self.list = result_list
        self.rowcount = len(result_list)
