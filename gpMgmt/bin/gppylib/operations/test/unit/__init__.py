# Make sure Python loads the modules of this package via absolute paths.
from os.path import abspath as _abspath
__path__[0] = _abspath(__path__[0])

from gppylib.gparray import GpArray, Segment

def setup_fake_gparray():
    coordinator = Segment.initFromString("1|-1|p|p|s|u|cdw|cdw|5432|/data/coordinator")
    primary0 = Segment.initFromString("2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
    primary1 = Segment.initFromString("3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
    mirror0 = Segment.initFromString("4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
    mirror1 = Segment.initFromString("5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")
    return GpArray([coordinator,primary0,primary1,mirror0,mirror1])
