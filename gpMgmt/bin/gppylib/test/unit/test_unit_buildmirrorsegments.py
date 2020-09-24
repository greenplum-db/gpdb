from mock import *
from .gp_unittest import *
from gppylib.gparray import GpArray, Segment
from gppylib.commands.base import WorkerPool

class GpMirrorListToBuildTestCase(GpTestCase):

    def setUp(self):
        self.pool = WorkerPool()

    def tearDown(self):
        # All background threads must be stopped, or else the test runner will
        # hang waiting. Join the stopped threads to make sure we're completely
        # clean for the next test.
        self.pool.haltWork()
        self.pool.joinWorkers()
        super(GpMirrorListToBuildTestCase, self).tearDown()

    def test_pg_rewind_parallel_execution(self):
        self.apply_patches([
            # Mock CHECKPOINT command in run_pg_rewind() as successful
            patch('gppylib.db.dbconn.connect', return_value=Mock()),
            patch('gppylib.db.dbconn.execSQL', return_value=Mock()),
            # Mock the command to remove postmaster.pid as successful
            patch('gppylib.commands.base.Command.run', return_value=Mock()),
            patch('gppylib.commands.base.Command.get_return_code', return_value=0),
            # Mock all pg_rewind commands to be not successful
            patch('gppylib.commands.base.Command.was_successful', return_value=False)
        ])
        from gppylib.operations.buildMirrorSegments import GpMirrorListToBuild
        # WorkerPool is the only valid parameter required in this test
        # case.  The test expects the workers to get a pg_rewind
        # command to run (and the command should fail to run).
        g = GpMirrorListToBuild(1, self.pool, 1,1)
        rewindInfo = {}
        p0 = Segment.initFromString("2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        p1 = Segment.initFromString("3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
        m0 = Segment.initFromString("4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        m1 = Segment.initFromString("5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")
        rewindInfo[p0.dbid] = GpMirrorListToBuild.RewindSegmentInfo(
            p0, p0.address, p0.port)
        rewindInfo[p1.dbid] = GpMirrorListToBuild.RewindSegmentInfo(
            p1, p1.address, p1.port)
        rewindInfo[m0.dbid] = GpMirrorListToBuild.RewindSegmentInfo(
            m0, m0.address, m0.port)
        rewindInfo[m1.dbid] = GpMirrorListToBuild.RewindSegmentInfo(
            m1, m1.address, m1.port)

        # Test1: all 4 pg_rewind commands should fail due the "was_successful" patch
        failedSegments = g.run_pg_rewind(rewindInfo)
        self.assertEqual(len(failedSegments), 4)
        # The returned list of failed segments should contain items of
        # type gparray.Segment
        failedSegments.remove(p0)
        self.assertTrue(failedSegments[0].getSegmentDbId() > 0)

        # Test2: patch it such that no failures this time
        patch('gppylib.commands.base.Command.was_successful', return_value=True).start()
        failedSegments = g.run_pg_rewind(rewindInfo)
        self.assertEqual(len(failedSegments), 0)

if __name__ == '__main__':
    run_tests()

