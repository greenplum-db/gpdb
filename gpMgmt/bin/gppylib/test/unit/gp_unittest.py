import unittest2 as unittest


class GpTestCase(unittest.TestCase):
    def __init__(self, methodName='runTest'):
        super(GpTestCase, self).__init__(methodName)
        self.patches = []
        self.mock_objs = []

    def apply_patches(self, patches):
        if self.patches:
            raise Exception('Test class is already patched!')
        self.patches = patches
        self.mock_objs = [p.start() for p in self.patches]

    # if you have a tearDown() in your test class,
    # be sure to call this using super.tearDown()
    def tearDown(self):
        [p.stop() for p in self.patches]
        self.mock_objs = []

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
