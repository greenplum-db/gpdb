# Make sure Python loads the modules of this package via absolute paths.
import contextlib
from io import BytesIO as StringIO
from os.path import abspath as _abspath
import sys

__path__[0] = _abspath(__path__[0])

from gppylib.gparray import GpArray, Segment


def setup_fake_gparray():
    master = Segment.initFromString("1|-1|p|p|s|u|mdw|mdw|5432|/data/master")
    primary0 = Segment.initFromString("2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
    primary1 = Segment.initFromString("3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
    mirror0 = Segment.initFromString("4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
    mirror1 = Segment.initFromString("5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")
    return GpArray([master,primary0,primary1,mirror0,mirror1])


# python2 unittest does not have an in built redirect_stderr function
@contextlib.contextmanager
def redirect_stderr():
    original_stderr = sys.stderr
    sys.stderr = StringIO()
    yield sys.stderr
    sys.stderr = original_stderr
