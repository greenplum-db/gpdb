from mock import *
from .gp_unittest import *
from gppylib.utils import escape_string

class UtilsFunctionsTest(GpTestCase):
    def test_escape_string_can_handle_utf8(self):
        self.assertEqual('public."spiegelungssätze"', escape_string('public."spiegelungssätze"'))
