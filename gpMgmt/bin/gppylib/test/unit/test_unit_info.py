from mock import *

from gp_unittest import *
from gppylib.system.info import *


class InfoTestCase(GpTestCase):

    def setUp(self):
        self.vmem = Mock()
        self.vmem.available = 123 * MB
        STACK_SIZE = 8 * MB
        self.apply_patches([
            patch("psutil.virtual_memory", return_value=self.vmem),
            patch("resource.getrlimit", return_value=[STACK_SIZE, 0])
        ])

    def test_automatic_thread_count(self):
        self.assertEquals(get_max_available_thread_count(), 3)

    def test_automatic_thread_minimum(self):
        self.vmem.available = 123
        self.assertEquals(get_max_available_thread_count(), 1)


if __name__ == '__main__':
    run_tests()
