import unittest2 as unittest


class GpTestCase(unittest.TestCase):
    def __init__(self, methodName='runTest'):
        super(GpTestCase, self).__init__(methodName)
        self.patches = []

    def apply_patches(self, patches):
        if self.patches:
            raise Exception('Test class is already patched!')
        self.patches = patches
        [p.start() for p in self.patches]

    # if you have a tearDown() in your test class,
    # be sure to call this using super.tearDown()
    def tearDown(self):
        [p.stop() for p in self.patches]

# hide unittest dependencies here
def run_tests():
    unittest.main(verbosity=2, buffer=True)

skip = unittest.skip
